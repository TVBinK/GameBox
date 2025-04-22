#include "game_racing.h"
#include "game_state.h"

// Biến toàn cục
int car_lane = 2;
int car_x = SCREEN_WIDTH / 2;
int car_y = SCREEN_HEIGHT - 40;
int enemy_x[4];  // Tối đa 4 xe địch (cho HARD)
int enemy_y[4];
static int racingScore = 0;
const int lane_width = SCREEN_WIDTH / LANE_COUNT;
const int lane_centers[LANE_COUNT] = {30, 90, 150, 210};
bool hasDrawBG = false;
bool needDelCar = false;
bool gameOverFlag = false;
bool exitGameFlag = false;
unsigned long racingGameStartTime = 0;

// Biến cho rào chắn
int barrier_x = 0;
int barrier_y = 0;

// Biến để theo dõi trạng thái
static bool lastLeftState = false;
static bool lastRightState = false;
static unsigned long lastMoveYTime = 0; // Thời gian di chuyển dọc cuối cùng
static bool isPushingBack = false; // Trạng thái lùi dần về vạch xuất phát
static unsigned long lastEnemyLaneChangeTime = 0; // Thời gian nhảy làn cuối cùng

// Cấu hình cho các mức độ khó
DifficultySettings difficultySettings[] = {
    {3, 3, false, false}, // EASY: Tốc độ 2, 2 xe địch, không đổi làn, không rào chắn
    {3, 3, false, true},  // MEDIUM: Tốc độ 3, 3 xe địch, không đổi làn, có rào chắn
    {4, 4, true, true}    // HARD: Tốc độ 4, 4 xe địch, đổi làn, có rào chắn
};

// Hàm vẽ nền đường đua
void drawBackgroundRacing() {
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        display.fillScreen(ROAD_GRAY);
        for (int i = 1; i < LANE_COUNT; i++) {
            int line_x = i * lane_width;
            for (int y = 0; y < SCREEN_HEIGHT; y += 20) {
                display.drawFastVLine(line_x, y, 10, TFT_WHITE);
            }
        }
        Serial.println("Drawing lanes at 60, 120, 180");
        xSemaphoreGive(tftMutex);
    }
}

// Hàm vẽ xe
void drawCar(int x, int y, uint16_t color) {
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        int body_width = 16;
        int body_height = 40;
        display.fillRoundRect(x - body_width / 2, y - body_height / 2, body_width, body_height, 3, color);
        display.fillEllipse(x, y - 5, 5, 8, TFT_BLACK);
        display.fillTriangle(x - 4, y - body_height / 2, x + 4, y - body_height / 2, x, y - body_height / 2 - 6, color);
        display.fillRect(x - 12, y - body_height / 2 - 2, 24, 2, TFT_DARKGREY);
        display.fillRect(x - 14, y + body_height / 2, 28, 3, TFT_DARKGREY);
        int wheel_radius = 4;
        display.fillCircle(x - 10, y - body_height / 2 + 6, wheel_radius, TFT_BLACK);
        display.fillCircle(x + 10, y - body_height / 2 + 6, wheel_radius, TFT_BLACK);
        display.fillCircle(x - 10, y + body_height / 2 - 6, wheel_radius, TFT_BLACK);
        display.fillCircle(x + 10, y + body_height / 2 - 6, wheel_radius, TFT_BLACK);
        xSemaphoreGive(tftMutex);
    }
}

// Hàm xóa xe
void delCar(int x, int y) {
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        display.fillRect(x - CAR_CLEAR_WIDTH / 2, y - CAR_CLEAR_HEIGHT / 2, CAR_CLEAR_WIDTH, CAR_CLEAR_HEIGHT, ROAD_GRAY);
        xSemaphoreGive(tftMutex);
    }
}

// Hàm vẽ rào chắn
void drawBarrier(int x, int y) {
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        // Vẽ hình chữ nhật chính
        display.fillRect(x - BARRIER_WIDTH / 2, y - BARRIER_HEIGHT / 2, BARRIER_WIDTH, BARRIER_HEIGHT, BARRIER_BLUE);
        // Vẽ viền trắng
        display.drawRect(x - BARRIER_WIDTH / 2, y - BARRIER_HEIGHT / 2, BARRIER_WIDTH, BARRIER_HEIGHT, TFT_WHITE);
        // Vẽ hai đường chéo vàng
        display.drawLine(x - BARRIER_WIDTH / 2, y - BARRIER_HEIGHT / 2, x + BARRIER_WIDTH / 2, y + BARRIER_HEIGHT / 2, TFT_YELLOW);
        display.drawLine(x + BARRIER_WIDTH / 2, y - BARRIER_HEIGHT / 2, x - BARRIER_WIDTH / 2, y + BARRIER_HEIGHT / 2, TFT_YELLOW);
        Serial.println("Drawing barrier at x: " + String(x) + ", y: " + String(y));
        xSemaphoreGive(tftMutex);
    }
}

// Hàm xóa rào chắn
void delBarrier(int x, int y) {
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        display.fillRect(x - BARRIER_CLEAR_WIDTH / 2, y - BARRIER_CLEAR_HEIGHT / 2, BARRIER_CLEAR_WIDTH, BARRIER_CLEAR_HEIGHT, ROAD_GRAY);
        Serial.println("Clearing barrier at x: " + String(x) + ", y: " + String(y));
        xSemaphoreGive(tftMutex);
    }
}

// Hàm chọn làn không trùng với xe địch
int selectNonOverlappingLane(int* enemy_x, int enemy_count) {
    bool lane_available[LANE_COUNT] = {true, true, true, true};
    int available_count = LANE_COUNT;

    // Đánh dấu các làn đang có xe địch
    for (int i = 0; i < enemy_count; i++) {
        for (int j = 0; j < LANE_COUNT; j++) {
            if (enemy_x[i] == lane_centers[j] && lane_available[j]) {
                lane_available[j] = false;
                available_count--;
                break;
            }
        }
    }

    // Nếu có làn trống, chọn ngẫu nhiên
    if (available_count > 0) {
        int rand_index = random(0, LANE_COUNT);
        int attempts = 0;
        while (!lane_available[rand_index] && attempts < LANE_COUNT) {
            rand_index = (rand_index + 1) % LANE_COUNT;
            attempts++;
        }
        return lane_centers[rand_index];
    }

    // Nếu không có làn trống, chọn ngẫu nhiên (ít xảy ra với 4 làn và tối đa 4 xe)
    return lane_centers[random(0, LANE_COUNT)];
}

// Hàm khởi tạo game Racing
void initGameRacing() {
    car_lane = 2;
    car_x = lane_centers[car_lane];
    car_y = START_Y; // Vạch xuất phát
    racingScore = 0;
    hasDrawBG = false;
    needDelCar = false;
    gameOverFlag = false;
    exitGameFlag = false;
    lastLeftState = false;
    lastRightState = false;
    lastMoveYTime = 0;
    isPushingBack = false;
    lastEnemyLaneChangeTime = 0;
    racingGameStartTime = millis();
    racingPlayCount++;

    // Lấy cấu hình theo độ khó
    DifficultySettings settings = difficultySettings[selectedDifficulty];

    // Khởi tạo xe địch
    for (int i = 0; i < settings.enemyCount; i++) {
        enemy_x[i] = lane_centers[random(0, LANE_COUNT)];
        enemy_y[i] = -CAR_HEIGHT - i * 120; // Khoảng cách y cố định 120px
        Serial.print("Enemy "); Serial.print(i); 
        Serial.print(" initialized at x: "); Serial.print(enemy_x[i]);
        Serial.print(", y: "); Serial.println(enemy_y[i]);
    }

    // Khởi tạo rào chắn (chỉ ở MEDIUM và HARD)
    if (settings.hasBarrier) {
        barrier_x = selectNonOverlappingLane(enemy_x, settings.enemyCount);
        barrier_y = -BARRIER_HEIGHT;
        Serial.println("Barrier initialized at x: " + String(barrier_x) + ", y: " + String(barrier_y));
    } else {
        barrier_x = 0;
        barrier_y = -BARRIER_HEIGHT; // Đặt ngoài màn hình để không vẽ
    }
}

// Hàm cập nhật bảng điểm cao cho Racing
void updateRacingHighScores() {
    racingCurrentRank = -1;
    int* highScores;
    switch (selectedDifficulty) {
        case EASY: highScores = racingHighScoresEasy; break;
        case MEDIUM: highScores = racingHighScoresMedium; break;
        case HARD: highScores = racingHighScoresHard; break;
        default: return;
    }
    for (int i = 0; i < 5; i++) {
        if (racingScore == highScores[i] && racingScore != 0) {
            racingCurrentRank = i + 1;
            return;
        }
    }
    for (int i = 0; i < 5; i++) {
        if (racingScore > highScores[i] || highScores[i] == 0) {
            racingCurrentRank = i + 1;
            for (int j = 4; j > i; j--) {
                highScores[j] = highScores[j - 1];
            }
            highScores[i] = racingScore;
            break;
        }
    }
}

// Hàm chạy logic chính của game Racing
bool runGameRacing() {
    if (firstRun) {
        initGameRacing();
        firstRun = false;
    }

    if (gameOverFlag) {
        if (checkButton(BTN_RETURN)) {
            exitGameFlag = true;
        }
        return !exitGameFlag;
    }

    if (!hasDrawBG) {
        drawBackgroundRacing();
        hasDrawBG = true;
    }

    // Lấy cấu hình theo độ khó
    DifficultySettings settings = difficultySettings[selectedDifficulty];

    // Di chuyển xe người chơi
    bool currentLeftState = checkButton(BTN_LEFT);
    bool currentRightState = checkButton(BTN_RIGHT);
    bool currentUpState = checkButton(BTN_UP);
    bool currentDownState = checkButton(BTN_DOWN);
    unsigned long now = millis();

    if (currentLeftState && !lastLeftState && car_lane > 0) {
        delCar(car_x, car_y);
        car_lane--;
        car_x = lane_centers[car_lane];
        needDelCar = false;
        Serial.print("Car moved to lane: "); Serial.println(car_lane);
    }
    if (currentRightState && !lastRightState && car_lane < LANE_COUNT - 1) {
        delCar(car_x, car_y);
        car_lane++;
        car_x = lane_centers[car_lane];
        needDelCar = false;
        Serial.print("Car moved to lane: "); Serial.println(car_lane);
    }
    if (!isPushingBack && (currentUpState || currentDownState) && now - lastMoveYTime >= MOVE_Y_INTERVAL) {
        delCar(car_x, car_y);
        if (currentUpState && car_y > CAR_Y_MIN) {
            car_y -= CAR_SPEED_Y;
            Serial.print("Car moved up to y: "); Serial.println(car_y);
        }
        if (currentDownState && car_y < CAR_Y_MAX) {
            car_y += CAR_SPEED_Y;
            Serial.print("Car moved down to y: "); Serial.println(car_y);
        }
        needDelCar = false;
        lastMoveYTime = now;
    }
    if (isPushingBack) {
        if (currentUpState) {
            isPushingBack = false; // Ngưng đẩy lùi khi nhấn BTN_UP
            Serial.println("Pushback stopped by BTN_UP");
        } else {
            delCar(car_x, car_y);
            if (car_y < START_Y) {
                car_y += PUSHBACK_SPEED; // Lùi dần về vạch xuất phát
                Serial.print("Pushing back to y: "); Serial.println(car_y);
            } else {
                car_y = START_Y; // Đạt vạch xuất phát
                isPushingBack = false; // Tắt trạng thái lùi
                Serial.println("Reached start line");
            }
            needDelCar = false;
        }
    }

    // Cập nhật trạng thái nút
    lastLeftState = currentLeftState;
    lastRightState = currentRightState;

    // Cập nhật xe địch
    if (settings.allowLaneChange && now - lastEnemyLaneChangeTime >= ENEMY_LANE_CHANGE_INTERVAL) {
        for (int i = 0; i < settings.enemyCount; i++) {
            // Kiểm tra ngang hàng với xe người chơi
            if (abs(car_y - enemy_y[i]) < CAR_HEIGHT) {
                Serial.print("Enemy "); Serial.print(i); 
                Serial.println(" blocked from lane change (aligned with player)");
                continue; // Không nhảy làn nếu ngang hàng
            }

            // Tìm làn hiện tại
            int current_lane = -1;
            for (int j = 0; j < LANE_COUNT; j++) {
                if (enemy_x[i] == lane_centers[j]) {
                    current_lane = j;
                    break;
                }
            }
            if (current_lane == -1) continue; // Bỏ qua nếu không ở làn hợp lệ

            // Chọn làn bên cạnh ngẫu nhiên
            int possible_lanes[2];
            int lane_count = 0;
            if (current_lane > 0) possible_lanes[lane_count++] = current_lane - 1; // Làn trái
            if (current_lane < LANE_COUNT - 1) possible_lanes[lane_count++] = current_lane + 1; // Làn phải

            if (lane_count > 0) {
                int new_lane = possible_lanes[random(0, lane_count)];
                delCar(enemy_x[i], enemy_y[i]);
                enemy_x[i] = lane_centers[new_lane];
                Serial.print("Enemy "); Serial.print(i); 
                Serial.print(" changed to lane "); Serial.print(new_lane);
                Serial.print(" at x: "); Serial.println(enemy_x[i]);
            }
        }
        lastEnemyLaneChangeTime = now;
        Serial.println("Enemy lane change attempt at time: " + String(now));
    }

    for (int i = 0; i < settings.enemyCount; i++) {
        delCar(enemy_x[i], enemy_y[i]);
        enemy_y[i] += settings.enemySpeed;

        if (enemy_y[i] > SCREEN_HEIGHT + CAR_HEIGHT) {
            enemy_x[i] = lane_centers[random(0, LANE_COUNT)];
            enemy_y[i] = -CAR_HEIGHT;
            racingScore += 10; // Tăng 10 điểm
            if (racingScore > maxRacing) maxRacing = racingScore;
            Serial.print("Score increased to: "); Serial.println(racingScore);
        }
        drawCar(enemy_x[i], enemy_y[i], ENEMY_YELLOW);
    }

    // Cập nhật rào chắn (chỉ ở MEDIUM và HARD)
    if (settings.hasBarrier) {
        delBarrier(barrier_x, barrier_y);
        barrier_y += settings.enemySpeed;
        if (barrier_y > SCREEN_HEIGHT + BARRIER_HEIGHT) {
            barrier_x = selectNonOverlappingLane(enemy_x, settings.enemyCount);
            barrier_y = -BARRIER_HEIGHT;
            Serial.println("Barrier respawned at x: " + String(barrier_x) + ", y: " + String(barrier_y));
        }
        drawBarrier(barrier_x, barrier_y);
    }

    // Kiểm tra va chạm với xe địch (chồng lấn hình chữ nhật)
    for (int i = 0; i < settings.enemyCount; i++) {
        int dx = abs(car_x - enemy_x[i]);
        int dy = abs(car_y - enemy_y[i]);
        if (dx < (CAR_WIDTH + CAR_WIDTH) / 2 && dy < (CAR_HEIGHT + CAR_HEIGHT) / 2) {
            Serial.print("Collision with enemy "); Serial.print(i);
            Serial.print(" at dx: "); Serial.print(dx);
            Serial.print(", dy: "); Serial.println(dy);
            gameOverRacing();
            return true;
        }
    }

    // Kiểm tra va chạm với rào chắn (chỉ ở MEDIUM và HARD)
    if (settings.hasBarrier && 
        abs(car_x - barrier_x) < (CAR_WIDTH + BARRIER_WIDTH) / 2 &&
        abs(car_y - barrier_y) < (CAR_HEIGHT + BARRIER_HEIGHT) / 2) {
        // Kích hoạt trạng thái lùi dần
        isPushingBack = true;
        Serial.println("Hit barrier, starting pushback");

        // Trừ điểm nếu điểm > 0
        if (racingScore > 0) {
            racingScore -= BARRIER_SCORE_PENALTY;
            Serial.print("Score reduced to: "); Serial.println(racingScore);
        } else {
            Serial.println("Score is 0, no penalty applied");
        }

        // Cập nhật hiển thị điểm số ngay lập tức
        if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
            // Xóa vùng điểm số trước
            display.fillRect(8, 8, 120, 24, ROAD_GRAY);
            // Vẽ điểm số mới
            display.setTextColor(TFT_WHITE, ROAD_GRAY);
            display.setTextSize(2);
            display.setCursor(8, 8);
            char scoreText[20];
            sprintf(scoreText, "Score: %d", racingScore);
            display.print(scoreText);
            Serial.println("Drawing score: " + String(scoreText));
            xSemaphoreGive(tftMutex);
        } else {
            Serial.println("ERROR: Failed to take tftMutex for score update!");
        }
    }

    // Vẽ xe người chơi
    if (needDelCar) {
        delCar(car_x, car_y);
    }
    drawCar(car_x, car_y, CAR_RED);
    needDelCar = true;

    // Hiển thị điểm số định kỳ
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        // Xóa vùng điểm số trước
        display.fillRect(8, 8, 120, 24, ROAD_GRAY);
        // Vẽ điểm số mới
        display.setTextColor(TFT_WHITE, ROAD_GRAY);
        display.setTextSize(2);
        display.setCursor(8, 8);
        char scoreText[20];
        sprintf(scoreText, "Score: %d", racingScore);
        display.print(scoreText);
        Serial.println("Drawing score: " + String(scoreText));
        xSemaphoreGive(tftMutex);
    } else {
        Serial.println("ERROR: Failed to take tftMutex for score update!");
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
    return true;
}

// Hàm xử lý khi game over
void gameOverRacing() {
    // Xóa các đối tượng trên màn hình
    delCar(car_x, car_y);
    for (int i = 0; i < difficultySettings[selectedDifficulty].enemyCount; i++) {
        delCar(enemy_x[i], enemy_y[i]);
    }
    if (difficultySettings[selectedDifficulty].hasBarrier) {
        delBarrier(barrier_x, barrier_y);
    }

    // Tính thời gian sống
    unsigned long survivalTime = millis() - racingGameStartTime;
    if (survivalTime > racingLongestSurvivalTime) {
        racingLongestSurvivalTime = survivalTime;
    }

    // Cập nhật điểm cao
    updateRacingHighScores();

    // Hiển thị màn hình Game Over
    if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        display.fillScreen(TFT_BLACK);
        
        int y = 20;
        display.setTextColor(TFT_RED);
        display.setTextSize(2);
        const char* gameOverText = "Game Over!";
        int textWidth = strlen(gameOverText) * 12;
        display.setCursor((SCREEN_WIDTH - textWidth) / 2, y);
        display.print(gameOverText);
        y += 30;

        // Hiển thị điểm số
        char scoreText[20];
        sprintf(scoreText, "Score: %d", racingScore);
        textWidth = strlen(scoreText) * 12;
        display.setCursor((SCREEN_WIDTH - textWidth) / 2, y);
        display.print(scoreText);
        y += 30;

        // Hiển thị bảng xếp hạng
        display.setTextColor(TFT_WHITE);
        display.setTextSize(1);
        const int colWidth = SCREEN_WIDTH / 3;
        const int xEasy = colWidth / 2 - 12;
        const int xMedium = colWidth + colWidth / 2 - 18;
        const int xHard = 2 * colWidth + colWidth / 2 - 12;
        display.setCursor(xEasy, y);
        display.print("Easy");
        display.setCursor(xMedium, y);
        display.print("Medium");
        display.setCursor(xHard, y);
        display.print("Hard");
        y += 15;

        // Hiển thị điểm cao
        for (int i = 0; i < 5; i++) {
            char easyText[10];
            sprintf(easyText, "%d. %d", i + 1, racingHighScoresEasy[i]);
            textWidth = strlen(easyText) * 6;
            display.setCursor(xEasy, y);
            if (selectedDifficulty == EASY && racingScore == racingHighScoresEasy[i] && racingScore != 0) {
                display.setTextColor(TFT_RED);
                display.drawRect(xEasy - 5, y - 2, textWidth + 10, 12, TFT_RED);
            } else {
                display.setTextColor(TFT_WHITE);
            }
            display.print(easyText);

            char mediumText[10];
            sprintf(mediumText, "%d. %d", i + 1, racingHighScoresMedium[i]);
            textWidth = strlen(mediumText) * 6;
            display.setCursor(xMedium, y);
            if (selectedDifficulty == MEDIUM && racingScore == racingHighScoresMedium[i] && racingScore != 0) {
                display.setTextColor(TFT_RED);
                display.drawRect(xMedium - 5, y - 2, textWidth + 10, 12, TFT_RED);
            } else {
                display.setTextColor(TFT_WHITE);
            }
            display.print(mediumText);

            char hardText[10];
            sprintf(hardText, "%d. %d", i + 1, racingHighScoresHard[i]);
            textWidth = strlen(hardText) * 6;
            display.setCursor(xHard, y);
            if (selectedDifficulty == HARD && racingScore == racingHighScoresHard[i] && racingScore != 0) {
                display.setTextColor(TFT_RED);
                display.drawRect(xHard - 5, y - 2, textWidth + 10, 12, TFT_RED);
            } else {
                display.setTextColor(TFT_WHITE);
            }
            display.print(hardText);
            y += 12;
        }
        y += 5;

        // Hiển thị xếp hạng
        char rankText[30];
        if (racingCurrentRank > 0) {
            sprintf(rankText, "Rank (%s): %d", 
                    selectedDifficulty == EASY ? "Easy" : 
                    selectedDifficulty == MEDIUM ? "Medium" : "Hard", 
                    racingCurrentRank);
        } else {
            sprintf(rankText, "Rank (%s): Not in top 5", 
                    selectedDifficulty == EASY ? "Easy" : 
                    selectedDifficulty == MEDIUM ? "Medium" : "Hard");
        }
        textWidth = strlen(rankText) * 6;
        display.setCursor((SCREEN_WIDTH - textWidth) / 2, y);
        display.print(rankText);
        y += 20;

        // Hiển thị thống kê
        char statsText[50];
        sprintf(statsText, "Plays: %d, Longest: %lus", racingPlayCount, racingLongestSurvivalTime / 1000);
        textWidth = strlen(statsText) * 6;
        display.setCursor((SCREEN_WIDTH - textWidth) / 2, y);
        display.print(statsText);

        xSemaphoreGive(tftMutex);
        Serial.println("Game Over displayed with ranking");
    } else {
        Serial.println("ERROR: Failed to take tftMutex for game over!");
    }

    gameOverFlag = true;
}