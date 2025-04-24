#include "game_racing.h"
#include "game_state.h"

// Các biến trạng thái của trò chơi
int car_lane = 2; // Làn đường hiện tại của xe người chơi
int car_x = SCREEN_WIDTH / 2; // Tọa độ x của xe người chơi
int car_y = SCREEN_HEIGHT - 40; // Tọa độ y của xe người chơi
int old_car_x = car_x; // Tọa độ x trước đó của xe
int old_car_y = car_y; // Tọa độ y trước đó của xe
int enemy_x[4]; // Tọa độ x của các xe địch
int enemy_y[4]; // Tọa độ y của các xe địch
static int racingScore = 0; // Điểm số hiện tại của trò chơi
const int lane_width = SCREEN_WIDTH / LANE_COUNT; // Chiều rộng mỗi làn đường
const int lane_centers[LANE_COUNT] = {30, 90, 150, 210}; // Tọa độ trung tâm của các làn đường
bool hasDrawBG = false; // Cờ kiểm tra xem nền đã được vẽ chưa
static bool carJustMoved = false; // Cờ kiểm tra xem xe vừa di chuyển chưa
bool gameOverFlag = false; // Cờ báo hiệu trò chơi kết thúc
bool exitGameFlag = false; // Cờ báo hiệu thoát trò chơi
unsigned long racingGameStartTime = 0; // Thời gian bắt đầu trò chơi
int barrier_x = 0; // Tọa độ x của chướng ngại vật
int barrier_y = 0; // Tọa độ y của chướng ngại vật
static bool lastLeftState = false; // Trạng thái nút trái trước đó
static bool lastRightState = false; // Trạng thái nút phải trước đó
static unsigned long lastMoveYTime = 0; // Thời gian di chuyển y gần nhất
static bool isPushingBack = false; // Cờ kiểm tra xem xe đang bị đẩy lùi
static unsigned long lastEnemyLaneChangeTime = 0; // Thời gian thay đổi làn của xe địch gần nhất
static unsigned long lastScoreUpdate = 0; // Thời gian cập nhật điểm số gần nhất
bool racingGameOverScreenActive = false; // Cờ kiểm tra màn hình game over đang hiển thị

// Bộ đếm lỗi mutex để theo dõi tranh chấp
static int mutexFailCount = 0;
static TaskHandle_t lastMutexHolder = NULL;

// Khoảng cách tối thiểu giữa các xe địch (tính bằng pixel)
#define MIN_ENEMY_Y_DISTANCE 150 // Khoảng cách dọc tối thiểu giữa các xe địch

// Cấu hình độ khó cho từng cấp độ
DifficultySettings difficultySettings[] = {
    {3, 3, false, false}, // Dễ: 3 xe địch, tốc độ 3, không đổi làn, không có chướng ngại
    {3, 3, false, true},  // Trung bình: 3 xe địch, tốc độ 3, không đổi làn, có chướng ngại
    {4, 4, true, true}    // Khó: 4 xe địch, tốc độ 4, có đổi làn, có chướng ngại
};

// Hàm lấy mutex với cơ chế thử lại và phát hiện deadlock
bool acquireTftMutex(const char* caller, TickType_t timeout = pdMS_TO_TICKS(1000)) {
    int retries = 0;
    const int MAX_RETRIES = 5; // Số lần thử lại tối đa
    
    while (retries < MAX_RETRIES) {
        TaskHandle_t currentHolder = (TaskHandle_t)xSemaphoreGetMutexHolder(tftMutex);
        
        // Kiểm tra nếu task hiện tại đã giữ mutex
        if (currentHolder == xTaskGetCurrentTaskHandle()) {
            Serial.printf("CẢNH BÁO: %s đã giữ tftMutex (gọi đệ quy)\n", caller);
            return true;
        }
        
        // Phát hiện deadlock nếu mutex bị giữ quá lâu
        if (currentHolder != NULL && currentHolder == lastMutexHolder) {
            mutexFailCount++;
            if (mutexFailCount > 10) {
                Serial.printf("NGUY HIỂM: Phát hiện khả năng deadlock trong %s, mutex được giữ bởi task %p\n", 
                            caller, currentHolder);
                if (currentHolder != NULL) {
                    Serial.printf("Thử khôi phục mutex khẩn cấp...\n");
                    vTaskPrioritySet(currentHolder, 1);
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    vTaskPrioritySet(currentHolder, 2);
                }
            }
        }
        
        // Thử lấy mutex
        if (xSemaphoreTake(tftMutex, timeout) == pdTRUE) {
            lastMutexHolder = xTaskGetCurrentTaskHandle();
            mutexFailCount = 0;
            return true;
        }
        
        vTaskDelay(50 / portTICK_PERIOD_MS);
        retries++;
        timeout += pdMS_TO_TICKS(500);
    }
    
    mutexFailCount++;
    Serial.printf("LỖI: %s không thể lấy tftMutex sau %d lần thử (tổng số lỗi: %d)\n", 
                  caller, MAX_RETRIES, mutexFailCount);
    return false;
}

// Hàm giải phóng mutex với kiểm tra quyền sở hữu
void releaseTftMutex(const char* caller) {
    TaskHandle_t currentHolder = (TaskHandle_t)xSemaphoreGetMutexHolder(tftMutex);
    
    // Kiểm tra xem task hiện tại có phải là chủ sở hữu mutex
    if (currentHolder == xTaskGetCurrentTaskHandle()) {
        xSemaphoreGive(tftMutex);
        Serial.printf("tftMutex được giải phóng bởi %s\n", caller);
    } else if (currentHolder == NULL) {
        Serial.printf("CẢNH BÁO: %s cố gắng giải phóng tftMutex nhưng không có ai giữ\n", caller);
    } else {
        Serial.printf("LỖI: %s cố gắng giải phóng tftMutex nhưng nó đang được giữ bởi task khác\n", caller);
    }
}

// Hàm vẽ nền đường đua
void drawBackgroundRacing() {
    if (acquireTftMutex("drawBackgroundRacing")) {
        display.fillScreen(ROAD_GRAY); // Tô màu xám cho đường
        // Vẽ các vạch phân làn
        for (int i = 1; i < LANE_COUNT; i++) {
            int line_x = i * lane_width;
            for (int y = 0; y < SCREEN_HEIGHT; y += 20) {
                display.drawFastVLine(line_x, y, 10, TFT_WHITE);
            }
        }
        Serial.println("Nền được vẽ thành công");
        releaseTftMutex("drawBackgroundRacing");
    }
}

// Hàm vẽ xe với màu sắc tùy chọn
void drawCar(int x, int y, uint16_t color) {
    if (acquireTftMutex("drawCar")) {
        int body_width = 16; // Chiều rộng thân xe
        int body_height = 40; // Chiều cao thân xe
        // Vẽ thân xe
        display.fillRoundRect(x - body_width / 2, y - body_height / 2, body_width, body_height, 3, color);
        // Vẽ cửa sổ
        display.fillEllipse(x, y - 5, 5, 8, TFT_BLACK);
        // Vẽ cánh gió phía trước
        display.fillTriangle(x - 4, y - body_height / 2, x + 4, y - body_height / 2, x, y - body_height / 2 - 6, color);
        // Vẽ các chi tiết khác
        display.fillRect(x - 12, y - body_height / 2 - 2, 24, 2, TFT_DARKGREY);
        display.fillRect(x - 14, y + body_height / 2, 28, 3, TFT_DARKGREY);
        int wheel_radius = 4;
        // Vẽ bánh xe
        display.fillCircle(x - 10, y - body_height / 2 + 6, wheel_radius, TFT_BLACK);
        display.fillCircle(x + 10, y - body_height / 2 + 6, wheel_radius, TFT_BLACK);
        display.fillCircle(x - 10, y + body_height / 2 - 6, wheel_radius, TFT_BLACK);
        display.fillCircle(x + 10, y + body_height / 2 - 6, wheel_radius, TFT_BLACK);
        
        releaseTftMutex("drawCar");
    }
}

// Hàm xóa xe khỏi màn hình
void delCar(int x, int y) {
    if (acquireTftMutex("delCar")) {
        int clear_width = CAR_CLEAR_WIDTH; // Chiều rộng vùng xóa
        int clear_height = CAR_CLEAR_HEIGHT; // Chiều cao vùng xóa
        
        // Xóa vùng chứa xe
        display.fillRect(x - clear_width / 2, y - clear_height / 2, 
                        clear_width, clear_height, ROAD_GRAY);
        
        // Vẽ lại các vạch phân làn nếu bị xóa
        for (int i = 1; i < LANE_COUNT; i++) {
            int line_x = i * lane_width;
            if (abs(x - line_x) < clear_width / 2) {
                int y_start = max(0, y - clear_height / 2);
                int y_end = min(SCREEN_HEIGHT, y + clear_height / 2);
                y_start = (y_start / 20) * 20;
                for (int y_pos = y_start; y_pos < y_end; y_pos += 20) {
                    if (y_pos + 10 <= y_end) {
                        display.drawFastVLine(line_x, y_pos, 10, TFT_WHITE);
                    }
                }
            }
        }
        
        releaseTftMutex("delCar");
    }
}

// Hàm vẽ chướng ngại vật
void drawBarrier(int x, int y) {
    if (acquireTftMutex("drawBarrier")) {
        // Vẽ hình chữ nhật chướng ngại vật
        display.fillRect(x - BARRIER_WIDTH / 2, y - BARRIER_HEIGHT / 2, 
                        BARRIER_WIDTH, BARRIER_HEIGHT, BARRIER_BLUE);
        // Vẽ viền trắng
        display.drawRect(x - BARRIER_WIDTH / 2, y - BARRIER_HEIGHT / 2, 
                        BARRIER_WIDTH, BARRIER_HEIGHT, TFT_WHITE);
        // Vẽ các đường chéo màu vàng
        display.drawLine(x - BARRIER_WIDTH / 2, y - BARRIER_HEIGHT / 2, 
                        x + BARRIER_WIDTH / 2, y + BARRIER_HEIGHT / 2, TFT_YELLOW);
        display.drawLine(x + BARRIER_WIDTH / 2, y - BARRIER_HEIGHT / 2, 
                        x - BARRIER_WIDTH / 2, y + BARRIER_HEIGHT / 2, TFT_YELLOW);
        
        releaseTftMutex("drawBarrier");
    }
}

// Hàm xóa chướng ngại vật
void delBarrier(int x, int y) {
    if (acquireTftMutex("delBarrier")) {
        int clear_width = BARRIER_CLEAR_WIDTH; // Chiều rộng vùng xóa
        int clear_height = BARRIER_CLEAR_HEIGHT; // Chiều cao vùng xóa
        
        // Xóa vùng chứa chướng ngại vật
        display.fillRect(x - clear_width / 2, y - clear_height / 2, 
                        clear_width, clear_height, ROAD_GRAY);
        
        // Vẽ lại các vạch phân làn nếu bị xóa
        for (int i = 1; i < LANE_COUNT; i++) {
            int line_x = i * lane_width;
            if (abs(x - line_x) < clear_width / 2) {
                int y_start = max(0, y - clear_height / 2);
                int y_end = min(SCREEN_HEIGHT, y + clear_height / 2);
                y_start = (y_start / 20) * 20;
                for (int y_pos = y_start; y_pos < y_end; y_pos += 20) {
                    if (y_pos + 10 <= y_end) {
                        display.drawFastVLine(line_x, y_pos, 10, TFT_WHITE);
                    }
                }
            }
        }
        
        releaseTftMutex("delBarrier");
    }
}

// Hàm chọn làn không chồng lấn với các xe địch
int selectNonOverlappingLane(int* enemy_x, int enemy_count) {
    bool lanes_available[LANE_COUNT] = {true, true, true, true}; // Trạng thái các làn
    int available_count = LANE_COUNT; // Số làn khả dụng

    // Kiểm tra và đánh dấu các làn đang bị chiếm
    for (int i = 0; i < enemy_count; i++) {
        for (int j = 0; j < LANE_COUNT; j++) {
            if (enemy_x[i] == lane_centers[j] && lanes_available[j]) {
                lanes_available[j] = false;
                available_count--;
                break;
            }
        }
    }

    // Nếu có làn khả dụng, chọn ngẫu nhiên một làn
    if (available_count > 0) {
        int rand_index = random(0, LANE_COUNT);
        int attempts = 0;
        while (!lanes_available[rand_index] && attempts < LANE_COUNT) {
            rand_index = (rand_index + 1) % LANE_COUNT;
            attempts++;
        }
        return lane_centers[rand_index];
    }

    // Nếu không còn làn khả dụng, chọn ngẫu nhiên
    return lane_centers[random(0, LANE_COUNT)];
}

// Hàm kiểm tra vị trí mới của xe địch có hợp lệ (không quá gần xe khác)
bool isEnemyPositionValid(int new_x, int new_y, int exclude_index, int enemy_count) {
    for (int i = 0; i < enemy_count; i++) {
        if (i == exclude_index) continue;
        int dx = abs(new_x - enemy_x[i]);
        int dy = abs(new_y - enemy_y[i]);
        if (dx < lane_width && dy < MIN_ENEMY_Y_DISTANCE) {
            return false;
        }
    }
    return true;
}

// Hàm hiển thị điểm số
void drawRacingScoreArea() {
    if (racingGameOverScreenActive) {
        return; // Không vẽ nếu màn hình game over đang hiển thị
    }
    
    if (acquireTftMutex("drawRacingScoreArea", pdMS_TO_TICKS(500))) {
        // Xóa vùng hiển thị điểm số
        display.fillRect(8, 8, 120, 24, ROAD_GRAY);
        // Thiết lập kiểu chữ và màu
        display.setTextColor(TFT_WHITE, ROAD_GRAY);
        display.setTextSize(2);
        display.setCursor(8, 8);
        // Hiển thị điểm số
        char scoreText[20];
        sprintf(scoreText, "Score: %d", racingScore);
        display.print(scoreText);
        
        releaseTftMutex("drawRacingScoreArea");
        lastScoreUpdate = millis();
    }
}

// Hàm khởi tạo trò chơi
void initGameRacing() {
    racingGameOverScreenActive = false; // Tắt màn hình game over
    car_lane = 2; // Đặt xe ở làn giữa
    car_x = lane_centers[car_lane]; // Tọa độ x ban đầu
    car_y = START_Y; // Tọa độ y ban đầu
    old_car_x = car_x;
    old_car_y = car_y;
    racingScore = 0; // Đặt lại điểm số
    hasDrawBG = false; // Đặt lại cờ nền
    carJustMoved = false;
    gameOverFlag = false;
    exitGameFlag = false;
    lastLeftState = false;
    lastRightState = false;
    lastMoveYTime = 0;
    isPushingBack = false;
    lastEnemyLaneChangeTime = 0;
    lastScoreUpdate = 0;
    mutexFailCount = 0;
    lastMutexHolder = NULL;
    racingGameStartTime = millis(); // Ghi lại thời gian bắt đầu
    racingPlayCount++; // Tăng số lần chơi

    // Xóa màn hình và vẽ nền
    if (acquireTftMutex("initGameRacing", pdMS_TO_TICKS(1000))) {
        display.fillScreen(TFT_BLACK);
        drawBackgroundRacing();
        hasDrawBG = true;
        releaseTftMutex("initGameRacing");
        Serial.println("Màn hình được xóa và nền được vẽ cho trò chơi đua xe");
    }

    // Thiết lập vị trí ban đầu cho xe địch
    DifficultySettings settings = difficultySettings[selectedDifficulty];
    for (int i = 0; i < settings.enemyCount; i++) {
        enemy_x[i] = selectNonOverlappingLane(enemy_x, i);
        enemy_y[i] = -CAR_HEIGHT - i * MIN_ENEMY_Y_DISTANCE;
        int attempts = 0;
        while (!isEnemyPositionValid(enemy_x[i], enemy_y[i], i, settings.enemyCount) && attempts < 10) {
            enemy_x[i] = selectNonOverlappingLane(enemy_x, i);
            enemy_y[i] = -CAR_HEIGHT - i * MIN_ENEMY_Y_DISTANCE - random(0, 50);
            attempts++;
        }
    }

    // Thiết lập chướng ngại vật
    if (settings.hasBarrier) {
        barrier_x = selectNonOverlappingLane(enemy_x, settings.enemyCount);
        barrier_y = -BARRIER_HEIGHT * 2;
    } else {
        barrier_x = -999;
        barrier_y = -999;
    }

    // Đặt lại hàng đợi cập nhật
    xQueueReset(racingUpdateQueue);
    RacingUpdateData initData = {
        .type = UPDATE_RACING,
        .car_x = car_x,
        .car_y = car_y,
        .car_lane = car_lane,
        .enemy_x = {enemy_x[0], enemy_x[1], enemy_x[2], enemy_x[3]},
        .enemy_y = {enemy_y[0], enemy_y[1], enemy_y[2], enemy_y[3]},
        .score = racingScore,
        .barrier_x = barrier_x,
        .barrier_y = barrier_y,
        .gameOver = false
    };
    
    // Gửi dữ liệu khởi tạo vào hàng đợi
    if (xQueueSendToFront(racingUpdateQueue, &initData, pdMS_TO_TICKS(100)) == pdPASS) {
        Serial.println("Gửi UPDATE_RACING ban đầu vào racingUpdateQueue");
    } else {
        Serial.println("LỖI: Không thể gửi UPDATE_RACING ban đầu vào racingUpdateQueue!");
    }

    // Gửi dữ liệu đặt lại vào hàng đợi
    RacingUpdateData resetData = {
        .type = UPDATE_RACING_RESET,
        .car_x = 0,
        .car_y = 0,
        .car_lane = 0,
        .enemy_x = {0, 0, 0, 0},
        .enemy_y = {0, 0, 0, 0},
        .score = 0,
        .barrier_x = 0,
        .barrier_y = 0,
        .gameOver = false
    };
    if (xQueueSend(racingUpdateQueue, &resetData, pdMS_TO_TICKS(100)) == pdPASS) {
        Serial.println("Gửi UPDATE_RACING_RESET vào racingUpdateQueue");
    }
}

// Hàm cập nhật bảng điểm cao
void updateRacingHighScores() {
    racingCurrentRank = -1;
    int* highScores;
    
    // Chọn bảng điểm cao theo độ khó
    switch (selectedDifficulty) {
        case EASY: highScores = racingHighScoresEasy; break;
        case MEDIUM: highScores = racingHighScoresMedium; break;
        case HARD: highScores = racingHighScoresHard; break;
        default: return;
    }
    
    // Kiểm tra nếu điểm số hiện tại đã có trong bảng xếp hạng
    for (int i = 0; i < 5; i++) {
        if (racingScore == highScores[i] && racingScore != 0) {
            racingCurrentRank = i + 1;
            return;
        }
    }
    
    // Thêm điểm số mới vào bảng xếp hạng nếu cao hơn
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

// Hàm hiển thị màn hình game over
void gameOverRacing() {
    racingGameOverScreenActive = true;
    updateRacingHighScores(); // Cập nhật điểm cao
    
    if (acquireTftMutex("gameOverRacing", pdMS_TO_TICKS(1000))) {
        display.fillScreen(TFT_BLACK); // Xóa màn hình
        int y = 20;
        display.setTextColor(TFT_RED);
        display.setTextSize(2);
        // Hiển thị thông báo "Game Over!"
        const char* gameOverText = "Game Over!";
        int textWidth = strlen(gameOverText) * 12;
        display.setCursor((SCREEN_WIDTH - textWidth) / 2, y);
        display.print(gameOverText);
        
        // Hiển thị điểm số
        y += 30;
        char scoreText[20];
        sprintf(scoreText, "Score: %d", racingScore);
        textWidth = strlen(scoreText) * 12;
        display.setCursor((SCREEN_WIDTH - textWidth) / 2, y);
        display.print(scoreText);
        
        // Hiển thị bảng xếp hạng
        y += 30;
        display.setTextColor(TFT_WHITE);
        display.setTextSize(1);
        const int colWidth = SCREEN_WIDTH / 3;
        const int xEasy = colWidth / 2 - 12;
        const int xMedium = colWidth + colWidth / 2 - 18;
        const int xHard = 2 * colWidth + colWidth / 2 - 12;
        
        // Tiêu đề các mức độ khó
        display.setCursor(xEasy, y);
        display.print("Easy");
        display.setCursor(xMedium, y);
        display.print("Medium");
        display.setCursor(xHard, y);
        display.print("Hard");
        
        y += 15;
        
        // Hiển thị điểm cao cho từng mức độ khó
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
        
        // Hiển thị thứ hạng hiện tại
        y += 5;
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
        
        // Hiển thị thống kê
        y += 20;
        char statsText[50];
        sprintf(statsText, "Plays: %d, Longest: %lus", racingPlayCount, racingLongestSurvivalTime / 1000);
        textWidth = strlen(statsText) * 6;
        display.setCursor((SCREEN_WIDTH - textWidth) / 2, y);
        display.print(statsText);
        
        // Hiển thị hướng dẫn quay lại menu
        y += 20;
        const char* returnText = "Press RETURN for menu";
        textWidth = strlen(returnText) * 6;
        display.setCursor((SCREEN_WIDTH - textWidth) / 2, y);
        display.print(returnText);
        
        releaseTftMutex("gameOverRacing");
        Serial.println("Màn hình Game Over hiển thị thành công");
    }
}

// Hàm khởi động trò chơi
void runGameRacing() {
    if (firstRun) {
        initGameRacing(); // Khởi tạo trò chơi nếu là lần chạy đầu tiên
        firstRun = false;
    }
}

// Hàm cập nhật logic trò chơi
void racingUpdateTask(void *parameter) {
    unsigned long lastFrameTime = 0;
    const unsigned long FRAME_TIME = 50; // Mục tiêu 20 FPS
    
    while (1) {
        unsigned long now = millis();
        
        // Kiểm tra trạng thái trò chơi
        if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (currentState != GAME2) {
                racingGameOverScreenActive = false;
                xSemaphoreGive(stateMutex);
                vTaskDelay(50 / portTICK_PERIOD_MS);
                continue;
            }
            xSemaphoreGive(stateMutex);
        } else {
            vTaskDelay(50 / portTICK_PERIOD_MS);
            continue;
        }

        // Cập nhật điểm số định kỳ
        if (!racingGameOverScreenActive && !gameOverFlag && now - lastScoreUpdate > 500) {
            RacingUpdateData scoreData = {
                .type = UPDATE_RACING_SCORE,
                .score = racingScore,
                .gameOver = false
            };
            if (xQueueSend(racingUpdateQueue, &scoreData, pdMS_TO_TICKS(10)) == pdPASS) {
                Serial.println("Gửi cập nhật điểm số vào hàng đợi");
            }
        }

        // Xử lý trạng thái game over
        if (gameOverFlag) {
            if (!racingGameOverScreenActive) {
                unsigned long survivalTime = now - racingGameStartTime;
                if (survivalTime > racingLongestSurvivalTime) {
                    racingLongestSurvivalTime = survivalTime;
                }
                gameOverRacing();
            }
            
            // Quay lại menu nếu nhấn nút RETURN
            if (checkButton(BTN_RETURN)) {
                if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    racingGameOverScreenActive = false;
                    currentState = MENU;
                    menuNeedsRedraw = true;
                    firstRun = true;
                    gameOverFlag = false;
                    xQueueReset(racingUpdateQueue);
                    xSemaphoreGive(stateMutex);
                    Serial.println("Quay lại menu từ màn hình game over");
                }
            }
            
            vTaskDelay(200 / portTICK_PERIOD_MS);
            continue;
        }

        if (racingGameOverScreenActive) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        // Kiểm tra thời gian khung hình
        if (now - lastFrameTime < FRAME_TIME) {
            vTaskDelay(5 / portTICK_PERIOD_MS);
            continue;
        }
        lastFrameTime = now;

        DifficultySettings settings = difficultySettings[selectedDifficulty];

        // Lấy trạng thái các nút điều khiển
        bool currentLeftState = checkButton(BTN_LEFT);
        bool currentRightState = checkButton(BTN_RIGHT);
        bool currentUpState = checkButton(BTN_UP);
        bool currentDownState = checkButton(BTN_DOWN);
        
        old_car_x = car_x;
        old_car_y = car_y;
        bool positionChanged = false;

        // Xử lý di chuyển sang trái
        if (currentLeftState && !lastLeftState && car_lane > 0) {
            car_lane--;
            car_x = lane_centers[car_lane];
            positionChanged = true;
        }
        
        // Xử lý di chuyển sang phải
        if (currentRightState && !lastRightState && car_lane < LANE_COUNT - 1) {
            car_lane++;
            car_x = lane_centers[car_lane];
            positionChanged = true;
        }
        
        // Xử lý di chuyển lên/xuống
        if (!isPushingBack && (currentUpState || currentDownState) && now - lastMoveYTime >= MOVE_Y_INTERVAL) {
            if (currentUpState && car_y > CAR_Y_MIN) {
                car_y -= CAR_SPEED_Y;
                positionChanged = true;
            }
            if (currentDownState && car_y < CAR_Y_MAX) {
                car_y += CAR_SPEED_Y;
                positionChanged = true;
            }
            lastMoveYTime = now;
        }
        
        // Xử lý đẩy lùi khi va chạm chướng ngại
        if (isPushingBack) {
            if (currentUpState) {
                isPushingBack = false;
            } else {
                old_car_y = car_y;
                if (car_y < START_Y) {
                    car_y += PUSHBACK_SPEED;
                    positionChanged = true;
                } else {
                    car_y = START_Y;
                    isPushingBack = false;
                }
            }
        }
        
        carJustMoved = positionChanged;
        lastLeftState = currentLeftState;
        lastRightState = currentRightState;

        // Xử lý thay đổi làn của xe địch
        if (settings.allowLaneChange && now - lastEnemyLaneChangeTime >= ENEMY_LANE_CHANGE_INTERVAL) {
            for (int i = 0; i < settings.enemyCount; i++) {
                if (abs(car_y - enemy_y[i]) < CAR_HEIGHT) {
                    continue;
                }
                
                int current_lane = -1;
                for (int j = 0; j < LANE_COUNT; j++) {
                    if (enemy_x[i] == lane_centers[j]) {
                        current_lane = j;
                        break;
                    }
                }
                
                if (current_lane == -1) continue;
                
                int possible_lanes[2];
                int lane_count = 0;
                
                if (current_lane > 0) possible_lanes[lane_count++] = current_lane - 1;
                if (current_lane < LANE_COUNT - 1) possible_lanes[lane_count++] = current_lane + 1;
                
                if (lane_count > 0) {
                    int new_lane = possible_lanes[random(0, lane_count)];
                    int new_x = lane_centers[new_lane];
                    if (isEnemyPositionValid(new_x, enemy_y[i], i, settings.enemyCount)) {
                        enemy_x[i] = new_x;
                    }
                }
            }
            lastEnemyLaneChangeTime = now;
        }

        // Di chuyển xe địch
        for (int i = 0; i < settings.enemyCount; i++) {
            enemy_y[i] += settings.enemySpeed;
            
            // Đặt lại vị trí xe địch khi ra khỏi màn hình
            if (enemy_y[i] > SCREEN_HEIGHT + CAR_HEIGHT) {
                enemy_x[i] = selectNonOverlappingLane(enemy_x, i);
                enemy_y[i] = -CAR_HEIGHT;
                int attempts = 0;
                while (!isEnemyPositionValid(enemy_x[i], enemy_y[i], i, settings.enemyCount) && attempts < 10) {
                    enemy_x[i] = selectNonOverlappingLane(enemy_x, i);
                    enemy_y[i] = -CAR_HEIGHT - random(0, 50);
                    attempts++;
                }
                racingScore += 10; // Tăng điểm khi vượt qua xe địch
                if (racingScore > maxRacing) maxRacing = racingScore;
            }
        }

        // Di chuyển chướng ngại vật
        if (settings.hasBarrier) {
            barrier_y += settings.enemySpeed;
            
            if (barrier_y > SCREEN_HEIGHT + BARRIER_HEIGHT) {
                barrier_x = selectNonOverlappingLane(enemy_x, settings.enemyCount);
                barrier_y = -BARRIER_HEIGHT;
            }
        }

        // Kiểm tra va chạm với xe địch
        for (int i = 0; i < settings.enemyCount; i++) {
            int dx = abs(car_x - enemy_x[i]);
            int dy = abs(car_y - enemy_y[i]);
            
            if (dx < (CAR_WIDTH + CAR_WIDTH) / 2 && dy < (CAR_HEIGHT + CAR_HEIGHT) / 2) {
                gameOverFlag = true;
                break;
            }
        }

        // Kiểm tra va chạm với chướng ngại vật
        if (settings.hasBarrier &&
            abs(car_x - barrier_x) < (CAR_WIDTH + BARRIER_WIDTH) / 2 &&
            abs(car_y - barrier_y) < (CAR_HEIGHT + BARRIER_HEIGHT) / 2) {
            isPushingBack = true;
            if (racingScore > 0) {
                racingScore = max(0, racingScore - BARRIER_SCORE_PENALTY); // Phạt điểm
                RacingUpdateData scoreData = {
                    .type = UPDATE_RACING_SCORE,
                    .score = racingScore,
                    .gameOver = false
                };
                xQueueSend(racingUpdateQueue, &scoreData, pdMS_TO_TICKS(10));
            }
        }

        // Xử lý quay lại menu
        if (checkButton(BTN_RETURN)) {
            unsigned long survivalTime = now - racingGameStartTime;
            if (survivalTime > racingLongestSurvivalTime) {
                racingLongestSurvivalTime = survivalTime;
            }
            
            if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                racingGameOverScreenActive = false;
                currentState = MENU;
                menuNeedsRedraw = true;
                firstRun = true;
                xQueueReset(racingUpdateQueue);
                
                RacingUpdateData resetData = {
                    .type = UPDATE_RACING_RESET,
                    .car_x = 0,
                    .car_y = 0,
                    .car_lane = 0,
                    .enemy_x = {0, 0, 0, 0},
                    .enemy_y = {0, 0, 0, 0},
                    .score = 0,
                    .barrier_x = 0,
                    .barrier_y = 0,
                    .gameOver = false
                };
                if (xQueueSend(racingUpdateQueue, &resetData, pdMS_TO_TICKS(100)) == pdPASS) {
                    Serial.println("Gửi UPDATE_RACING_RESET vào racingUpdateQueue khi nhấn RETURN");
                }
                
                initGameRacing();
                
                xSemaphoreGive(stateMutex);
                Serial.println("Quay lại menu và đặt lại trạng thái trò chơi");
            }
            continue;
        }

        // Gửi dữ liệu cập nhật vào hàng đợi
        RacingUpdateData updateData = {
            .type = UPDATE_RACING,
            .car_x = car_x,
            .car_y = car_y,
            .car_lane = car_lane,
            .enemy_x = {enemy_x[0], enemy_x[1], enemy_x[2], enemy_x[3]},
            .enemy_y = {enemy_y[0], enemy_y[1], enemy_y[2], enemy_y[3]},
            .score = racingScore,
            .barrier_x = barrier_x,
            .barrier_y = barrier_y,
            .gameOver = gameOverFlag
        };
        
        if (xQueueSend(racingUpdateQueue, &updateData, pdMS_TO_TICKS(10)) == pdPASS) {
            Serial.println("Gửi cập nhật trạng thái trò chơi vào hàng đợi");
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// Hàm vẽ trò chơi
void racingRenderTask(void *parameter) {
    static unsigned long lastFullRefresh = 0; // Thời gian làm mới toàn màn hình gần nhất
    static int queueEmptyCount = 0; // Đếm số lần hàng đợi rỗng
    static bool firstFrame = true; // Cờ kiểm tra khung hình đầu tiên
    
    static int prev_car_x = -999; // Tọa độ x trước đó của xe người chơi
    static int prev_car_y = -999; // Tọa độ y trước đó của xe người chơi
    static int prev_enemy_x[4] = {-999, -999, -999, -999}; // Tọa độ x trước đó của xe địch
    static int prev_enemy_y[4] = {-999, -999, -999, -999}; // Tọa độ y trước đó của xe địch
    static int prev_barrier_x = -999; // Tọa độ x trước đó của chướng ngại
    static int prev_barrier_y = -999; // Tọa độ y trước đó của chướng ngại
    
    while (1) {
        RacingUpdateData updateData;
        
        // Nhận dữ liệu từ hàng đợi
        if (xQueueReceive(racingUpdateQueue, &updateData, pdMS_TO_TICKS(100)) == pdPASS) {
            queueEmptyCount = 0;
            
            if (acquireTftMutex("racingRenderTask", pdMS_TO_TICKS(500))) {
                unsigned long now = millis();
                
                switch (updateData.type) {
                    case UPDATE_RACING: {
                        if (racingGameOverScreenActive) {
                            releaseTftMutex("racingRenderTask");
                            continue;
                        }
                        
                        DifficultySettings settings = difficultySettings[selectedDifficulty];
                        
                        // Vẽ khung hình đầu tiên
                        if (firstFrame) {
                            drawBackgroundRacing();
                            firstFrame = false;
                            lastFullRefresh = now;
                            prev_car_x = updateData.car_x;
                            prev_car_y = updateData.car_y;
                            for (int i = 0; i < 4; i++) {
                                prev_enemy_x[i] = updateData.enemy_x[i];
                                prev_enemy_y[i] = updateData.enemy_y[i];
                            }
                            prev_barrier_x = updateData.barrier_x;
                            prev_barrier_y = updateData.barrier_y;
                        }
                        
                        // Làm mới toàn màn hình định kỳ
                        if (now - lastFullRefresh >= 10000) {
                            drawBackgroundRacing();
                            lastFullRefresh = now;
                            Serial.println("Làm mới toàn màn hình định kỳ");
                        }
                        
                        // Xóa xe người chơi nếu di chuyển
                        if (carJustMoved || prev_car_x != updateData.car_x || prev_car_y != updateData.car_y) {
                            delCar(prev_car_x, prev_car_y);
                        }
                        
                        // Xóa xe địch nếu di chuyển
                        for (int i = 0; i < settings.enemyCount; i++) {
                            if (prev_enemy_x[i] != updateData.enemy_x[i] || prev_enemy_y[i] != updateData.enemy_y[i]) {
                                if (prev_enemy_y[i] > -CAR_HEIGHT && prev_enemy_y[i] < SCREEN_HEIGHT + CAR_HEIGHT) {
                                    delCar(prev_enemy_x[i], prev_enemy_y[i]);
                                }
                            }
                        }
                        
                        // Xóa chướng ngại nếu di chuyển
                        if (settings.hasBarrier && 
                            (prev_barrier_x != updateData.barrier_x || prev_barrier_y != updateData.barrier_y)) {
                            if (prev_barrier_y > -BARRIER_HEIGHT && prev_barrier_y < SCREEN_HEIGHT + BARRIER_HEIGHT) {
                                delBarrier(prev_barrier_x, prev_barrier_y);
                            }
                        }
                        
                        // Vẽ xe người chơi
                        drawCar(updateData.car_x, updateData.car_y, CAR_RED);
                        
                        // Vẽ xe địch
                        for (int i = 0; i < settings.enemyCount; i++) {
                            if (updateData.enemy_y[i] > -CAR_HEIGHT && updateData.enemy_y[i] < SCREEN_HEIGHT + CAR_HEIGHT) {
                                drawCar(updateData.enemy_x[i], updateData.enemy_y[i], ENEMY_YELLOW);
                            }
                        }
                        
                        // Vẽ chướng ngại vật
                        if (settings.hasBarrier && 
                            updateData.barrier_y > -BARRIER_HEIGHT && 
                            updateData.barrier_y < SCREEN_HEIGHT + BARRIER_HEIGHT) {
                            drawBarrier(updateData.barrier_x, updateData.barrier_y);
                        }
                        
                        // Vẽ vùng điểm số
                        display.fillRect(8, 8, 120, 24, ROAD_GRAY);
                        display.setTextColor(TFT_WHITE, ROAD_GRAY);
                        display.setTextSize(2);
                        display.setCursor(8, 8);
                        char scoreText[20];
                        sprintf(scoreText, "Score: %d", updateData.score);
                        display.print(scoreText);
                        
                        // Cập nhật vị trí trước đó
                        prev_car_x = updateData.car_x;
                        prev_car_y = updateData.car_y;
                        for (int i = 0; i < 4; i++) {
                            prev_enemy_x[i] = updateData.enemy_x[i];
                            prev_enemy_y[i] = updateData.enemy_y[i];
                        }
                        prev_barrier_x = updateData.barrier_x;
                        prev_barrier_y = updateData.barrier_y;
                        
                        carJustMoved = false;
                        lastScoreUpdate = now;
                        break;
                    }
                    
                    case UPDATE_RACING_SCORE: {
                        if (racingGameOverScreenActive) {
                            releaseTftMutex("racingRenderTask_score");
                            continue;
                        }
                        
                        racingScore = updateData.score;
                        drawRacingScoreArea();
                        break;
                    }
                    
                    case UPDATE_RACING_GAME_OVER: {
                        gameOverRacing();
                        break;
                    }
                    
                    case UPDATE_RACING_RESET: {
                        firstFrame = true;
                        lastFullRefresh = 0;
                        prev_car_x = -999;
                        prev_car_y = -999;
                        for (int i = 0; i < 4; i++) {
                            prev_enemy_x[i] = -999;
                            prev_enemy_y[i] = -999;
                        }
                        prev_barrier_x = -999;
                        prev_barrier_y = -999;
                        Serial.println("Đặt lại trạng thái nhiệm vụ vẽ");
                        break;
                    }
                }
                
                releaseTftMutex("racingRenderTask");
            } else {
                // Cập nhật trạng thái nếu không thể lấy mutex
                if (updateData.type == UPDATE_RACING) {
                    car_x = updateData.car_x;
                    car_y = updateData.car_y;
                    car_lane = updateData.car_lane;
                    for (int i = 0; i < 4; i++) {
                        enemy_x[i] = updateData.enemy_x[i];
                        enemy_y[i] = updateData.enemy_y[i];
                    }
                    barrier_x = updateData.barrier_x;
                    barrier_y = updateData.barrier_y;
                    racingScore = updateData.score;
                }
            }
        } else {
            queueEmptyCount++;
            // Làm mới nền nếu hàng đợi rỗng quá lâu
            if (queueEmptyCount > 20 && !racingGameOverScreenActive) {
                queueEmptyCount = 0;
                if (acquireTftMutex("racingRenderTask_bgRefresh", pdMS_TO_TICKS(200))) {
                    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        if (currentState == GAME2 && !hasDrawBG) {
                            drawBackgroundRacing();
                            hasDrawBG = true;
                            lastFullRefresh = millis();
                        }
                        xSemaphoreGive(stateMutex);
                    }
                    releaseTftMutex("racingRenderTask_bgRefresh");
                }
            }
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
    }
}