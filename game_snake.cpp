#include "game_snake.h"
#include "game_state.h"
#include "game_config.h"

extern TFT_eSPI display;
extern SemaphoreHandle_t tftMutex;

Direction currentDir = RIGHT;
Point snake[SNAKE_MAX_LENGTH];
int snakeLength = 3;
Point food;
FoodType foodType = NORMAL;
unsigned long foodSpawnTime = 0;
unsigned long foodTimeout = 0;
int score = 0;
bool hasDrawnBackground = false;
unsigned long lastMoveTime = 0;
unsigned long lastBoostTime = 0;
int moveDelay = 200;
Difficulty currentDifficulty = EASY;
Obstacle obstacles[MAX_OBSTACLES];
int obstacleCount = 0;
unsigned long gameStartTime = 0;
int currentRank = -1;
bool gameOverDrawn = false;

void drawSnake(int x, int y, bool isHead) {
    int pixel_x = x * CELL_SIZE;
    int pixel_y = y * CELL_SIZE;
    if (isHead) {
        display.fillCircle(pixel_x, pixel_y, 5, TFT_GREEN);
        display.fillCircle(pixel_x + 2, pixel_y - 2, 1, TFT_WHITE);
        display.fillCircle(pixel_x - 2, pixel_y - 2, 1, TFT_WHITE);
    } else {
        display.fillCircle(pixel_x, pixel_y, 4, TFT_GREEN);
    }
    Serial.printf("Drew snake %s at (%d, %d)\n", isHead ? "head" : "body", pixel_x, pixel_y);
}

void drawFood(int x, int y) {
    int pixel_x = x * CELL_SIZE;
    int pixel_y = y * CELL_SIZE;
    uint16_t color = (foodType == SPECIAL) ? TFT_YELLOW : (esp_random() & 0xFFFF);
    display.fillCircle(pixel_x, pixel_y, 3, color);
    display.fillCircle(pixel_x + 2, pixel_y - 2, 2, color);
    display.fillCircle(pixel_x - 2, pixel_y + 2, 2, color);
    Serial.printf("Drew food at (%d, %d) with color %d\n", pixel_x, pixel_y, color);
}

void eraseCell(int x, int y) {
    int pixel_x = x * CELL_SIZE;
    int pixel_y = y * CELL_SIZE;
    display.fillRect(pixel_x - 5, pixel_y - 5, 11, 11, TFT_BLACK);
    Serial.printf("Erased cell at (%d, %d)\n", pixel_x, pixel_y);
}

void drawBackground() {
    if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        display.fillScreen(TFT_BLACK);
        display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, TFT_WHITE);
        xSemaphoreGive(tftMutex);
    }
}

void drawInitialState() {
    if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < snakeLength; i++) {
            drawSnake(snake[i].x, snake[i].y, i == 0);
        }
        drawFood(food.x, food.y);
        for (int i = 0; i < obstacleCount; i++) {
            for (int j = 0; j < obstacles[i].length; j++) {
                int pixel_x = obstacles[i].points[j].x * CELL_SIZE;
                int pixel_y = obstacles[i].points[j].y * CELL_SIZE;
                display.fillRect(pixel_x - 5, pixel_y - 5, 10, 10, TFT_RED);
            }
        }
        xSemaphoreGive(tftMutex);
    }
}

void initObstacles() {
    obstacleCount = 0;
    if (currentDifficulty == MEDIUM) {
        // Luôn tạo 3 chướng ngại vật cố định, phân bố đều
        int numObstacles = 3;
        // Chia lưới thành 3 vùng dọc: x từ 3–9, 10–16, 17–21
        int xRanges[3][2] = {{3, 9}, {10, 16}, {17, 21}};
        for (int i = 0; i < numObstacles && obstacleCount < MAX_OBSTACLES; i++) {
            obstacles[obstacleCount].length = 5 + (esp_random() % 3); // Độ dài 5–7
            obstacles[obstacleCount].isMoving = false;
            // Chọn vị trí ngẫu nhiên trong vùng, không sát tường
            int startX, startY;
            bool valid = false;
            int shape = -1;
            int attempts = 0;
            while (!valid && attempts < 50) {
                startX = xRanges[i][0] + (esp_random() % (xRanges[i][1] - xRanges[i][0] + 1));
                startY = 3 + (esp_random() % (NUM_ROWS - 6)); // y từ 3 đến NUM_ROWS-4
                shape = esp_random() % 4; // Chọn hình dạng ngẫu nhiên
                // Tạo thử chướng ngại vật
                Point tempPoints[8];
                tempPoints[0] = {startX, startY};
                bool shapeValid = true;
                for (int j = 1; j < obstacles[obstacleCount].length; j++) {
                    switch (shape) {
                        case 0: // Chữ L
                            tempPoints[j] = (j < obstacles[obstacleCount].length - 1) ? 
                                Point{startX, startY + j} : Point{startX + 1, startY + j - 1};
                            break;
                        case 1: // Chữ T
                            tempPoints[j] = (j < obstacles[obstacleCount].length - 2) ? 
                                Point{startX, startY + j} : (j == obstacles[obstacleCount].length - 2) ? 
                                Point{startX - 1, startY + j - 1} : Point{startX + 1, startY + j - 2};
                            break;
                        case 2: // Đường thẳng ngang
                            tempPoints[j] = Point{startX + j, startY};
                            break;
                        case 3: // Zigzag
                            tempPoints[j] = (j % 2 == 0) ? 
                                Point{startX + j / 2, startY + 1} : Point{startX + j / 2, startY};
                            break;
                    }
                    // Kiểm tra biên
                    if (tempPoints[j].x < 3 || tempPoints[j].x >= NUM_COLS - 4 ||
                        tempPoints[j].y < 3 || tempPoints[j].y >= NUM_ROWS - 4) {
                        shapeValid = false;
                        break;
                    }
                }
                if (!shapeValid) {
                    attempts++;
                    continue;
                }
                // Kiểm tra khoảng cách đến rắn (>5 ô)
                valid = abs(startX - snake[0].x) > 5 || abs(startY - snake[0].y) > 5;
                // Kiểm tra khoảng cách đến chướng ngại vật khác (>5 ô)
                for (int k = 0; k < obstacleCount; k++) {
                    if (abs(startX - obstacles[k].points[0].x) <= 5 && abs(startY - obstacles[k].points[0].y) <= 5) {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    // Gán các điểm hợp lệ
                    for (int j = 0; j < obstacles[obstacleCount].length; j++) {
                        obstacles[obstacleCount].points[j] = tempPoints[j];
                    }
                }
                attempts++;
            }
            if (!valid) {
                // Vị trí mặc định nếu không tìm được
                startX = xRanges[i][0] + 3;
                startY = NUM_ROWS / 2;
                shape = 2; // Đường thẳng ngang làm mặc định
                obstacles[obstacleCount].points[0] = {startX, startY};
                for (int j = 1; j < obstacles[obstacleCount].length; j++) {
                    obstacles[obstacleCount].points[j] = {startX + j, startY};
                }
            }
            obstacleCount++;
        }
    } else if (currentDifficulty == HARD) {
        // Tạo 2 bức tường ngang di động
        obstacleCount = 2;
        // Tường trên (y = 5–10, x ở nửa trái)
        obstacles[0].length = 5 + (esp_random() % 3); // Độ dài 5–7
        obstacles[0].isMoving = true;
        obstacles[0].direction = 1;
        int startX = 3 + (esp_random() % (NUM_COLS / 2 - obstacles[0].length - 3)); // x từ 3 đến NUM_COLS/2-length
        int startY = 5 + (esp_random() % 6); // y từ 5 đến 10
        for (int j = 0; j < obstacles[0].length; j++) {
            obstacles[0].points[j] = {startX + j, startY};
        }
        // Tường dưới (y = 20–25, x ở nửa phải)
        obstacles[1].length = 5 + (esp_random() % 3); // Độ dài 5–7
        obstacles[1].isMoving = true;
        obstacles[1].direction = -1; // Di chuyển ngược hướng tường trên
        startX = NUM_COLS / 2 + (esp_random() % (NUM_COLS / 2 - obstacles[1].length - 3)); // x từ NUM_COLS/2 đến NUM_COLS-4-length
        startY = 20 + (esp_random() % 6); // y từ 20 đến 25
        for (int j = 0; j < obstacles[1].length; j++) {
            obstacles[1].points[j] = {startX + j, startY};
        }
    }
}

void updateObstacles() {
    if (currentDifficulty == HARD) {
        if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            for (int i = 0; i < obstacleCount; i++) {
                // Kiểm tra va chạm với thân rắn trước khi di chuyển
                bool willCollide = false;
                for (int j = 0; j < obstacles[i].length; j++) {
                    int newX = obstacles[i].points[j].x + obstacles[i].direction;
                    int newY = obstacles[i].points[j].y;
                    // Kiểm tra với thân rắn (bỏ qua đầu rắn)
                    for (int k = 1; k < snakeLength; k++) {
                        if (newX == snake[k].x && newY == snake[k].y) {
                            willCollide = true;
                            break;
                        }
                    }
                    if (willCollide) break;
                }
                // Xóa vị trí cũ
                for (int j = 0; j < obstacles[i].length; j++) {
                    eraseCell(obstacles[i].points[j].x, obstacles[i].points[j].y);
                }
                // Đảo chiều nếu sắp va chạm
                if (willCollide) {
                    obstacles[i].direction = -obstacles[i].direction;
                    Serial.printf("Obstacle %d reversed direction to %d to avoid snake\n", i, obstacles[i].direction);
                } else {
                    // Cập nhật vị trí nếu không va chạm
                    for (int j = 0; j < obstacles[i].length; j++) {
                        obstacles[i].points[j].x += obstacles[i].direction;
                    }
                }
                // Kiểm tra biên và đổi hướng
                int min_x = obstacles[i].points[0].x;
                int max_x = obstacles[i].points[0].x;
                for (int j = 1; j < obstacles[i].length; j++) {
                    if (obstacles[i].points[j].x < min_x) min_x = obstacles[i].points[j].x;
                    if (obstacles[i].points[j].x > max_x) max_x = obstacles[i].points[j].x;
                }
                if (max_x >= NUM_COLS - 2) obstacles[i].direction = -1;
                if (min_x <= 1) obstacles[i].direction = 1;
                // Vẽ lại
                for (int j = 0; j < obstacles[i].length; j++) {
                    int pixel_x = obstacles[i].points[j].x * CELL_SIZE;
                    int pixel_y = obstacles[i].points[j].y * CELL_SIZE;
                    display.fillRect(pixel_x - 5, pixel_y - 5, 10, 10, TFT_RED);
                }
            }
            xSemaphoreGive(tftMutex);
        }
    }
}

void spawnFood() {
    bool valid = false;
    int attempts = 0;
    const int maxAttempts = 50;
    while (!valid && attempts < maxAttempts) {
        food.x = (esp_random() % (NUM_COLS - 2)) + 1;
        food.y = (esp_random() % (NUM_ROWS - 2)) + 1;
        valid = true;
        for (int i = 0; i < snakeLength; i++) {
            if (snake[i].x == food.x && snake[i].y == food.y) {
                valid = false;
                break;
            }
        }
        for (int i = 0; i < obstacleCount; i++) {
            for (int j = 0; j < obstacles[i].length; j++) {
                if (obstacles[i].points[j].x == food.x && obstacles[i].points[j].y == food.y) {
                    valid = false;
                    break;
                }
            }
        }
        attempts++;
    }
    if (!valid) {
        food.x = NUM_COLS - 2;
        food.y = NUM_ROWS - 2;
    }
    foodType = (esp_random() % SPECIAL_FOOD_CHANCE == 0) ? SPECIAL : NORMAL;
    foodTimeout = (foodType == SPECIAL) ? 2000 : 
                  (currentDifficulty == EASY) ? 0 : 
                  (currentDifficulty == MEDIUM) ? (5000 + (esp_random() % 3000)) : 
                  (3000 + (esp_random() % 2000));
    foodSpawnTime = millis();
    if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        drawFood(food.x, food.y);
        xSemaphoreGive(tftMutex);
    }
    Serial.printf("Food spawned at (%d, %d), type: %s, timeout: %lu\n", food.x, food.y, foodType == SPECIAL ? "Special" : "Normal", foodTimeout);
}

void updateHighScores() {
    currentRank = -1;
    int* highScores;
    switch (currentDifficulty) {
        case EASY: highScores = highScoresEasy; break;
        case MEDIUM: highScores = highScoresMedium; break;
        case HARD: highScores = highScoresHard; break;
        default: return;
    }
    // Kiểm tra nếu điểm đã tồn tại trong top 5
    for (int i = 0; i < 5; i++) {
        if (score == highScores[i] && score != 0) {
            currentRank = i + 1;
            return; // Không chèn mới, chỉ đặt rank và thoát
        }
    }
    // Chèn điểm mới nếu lớn hơn hoặc danh sách chưa đầy
    for (int i = 0; i < 5; i++) {
        if (score > highScores[i] || highScores[i] == 0) {
            currentRank = i + 1;
            for (int j = 4; j > i; j--) {
                highScores[j] = highScores[j - 1];
            }
            highScores[i] = score;
            break;
        }
    }
}

void displayGameOver() {
    if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        display.fillScreen(TFT_BLACK);
        int y = 20;
        display.setTextColor(TFT_RED);
        display.setTextSize(2);
        const char* gameOverText = "Game Over!";
        int textWidth = strlen(gameOverText) * 12;
        display.setCursor((SCREEN_WIDTH - textWidth) / 2, y);
        display.print(gameOverText);
        y += 30;
        char scoreText[20];
        sprintf(scoreText, "Score: %d", score);
        textWidth = strlen(scoreText) * 12;
        display.setCursor((SCREEN_WIDTH - textWidth) / 2, y);
        display.print(scoreText);
        y += 30;

        // Căn giữa 3 cột Easy, Medium, Hard
        display.setTextColor(TFT_WHITE);
        display.setTextSize(1);
        const int colWidth = SCREEN_WIDTH / 3; // 80px mỗi cột
        const int xEasy = colWidth / 2 - 12;   // ~28px (Easy ~24px)
        const int xMedium = colWidth + colWidth / 2 - 18; // ~102px (Medium ~36px)
        const int xHard = 2 * colWidth + colWidth / 2 - 12; // ~188px (Hard ~24px)
        display.setCursor(xEasy, y);
        display.print("Easy");
        display.setCursor(xMedium, y);
        display.print("Medium");
        display.setCursor(xHard, y);
        display.print("Hard");
        y += 15;

        updateHighScores();
        for (int i = 0; i < 5; i++) {
            char easyText[10];
            sprintf(easyText, "%d. %d", i + 1, highScoresEasy[i]);
            textWidth = strlen(easyText) * 6;
            display.setCursor(xEasy, y); // Sử dụng xEasy trực tiếp để căn giữa
            if (currentDifficulty == EASY && score == highScoresEasy[i] && score != 0) {
                display.setTextColor(TFT_RED);
                display.drawRect(xEasy - 5, y - 2, textWidth + 10, 12, TFT_RED);
            } else {
                display.setTextColor(TFT_WHITE);
            }
            display.print(easyText);

            char mediumText[10];
            sprintf(mediumText, "%d. %d", i + 1, highScoresMedium[i]);
            textWidth = strlen(mediumText) * 6;
            display.setCursor(xMedium, y); // Sử dụng xMedium trực tiếp để căn giữa
            if (currentDifficulty == MEDIUM && score == highScoresMedium[i] && score != 0) {
                display.setTextColor(TFT_RED);
                display.drawRect(xMedium - 5, y - 2, textWidth + 10, 12, TFT_RED);
            } else {
                display.setTextColor(TFT_WHITE);
            }
            display.print(mediumText);

            char hardText[10];
            sprintf(hardText, "%d. %d", i + 1, highScoresHard[i]);
            textWidth = strlen(hardText) * 6;
            display.setCursor(xHard, y); // Sử dụng xHard trực tiếp để căn giữa
            if (currentDifficulty == HARD && score == highScoresHard[i] && score != 0) {
                display.setTextColor(TFT_RED);
                display.drawRect(xHard - 5, y - 2, textWidth + 10, 12, TFT_RED);
            } else {
                display.setTextColor(TFT_WHITE);
            }
            display.print(hardText);

            y += 12;
        }

        y += 5;
        char rankText[30];
        if (currentRank > 0) {
            sprintf(rankText, "Rank (%s): %d", 
                    currentDifficulty == EASY ? "Easy" : 
                    currentDifficulty == MEDIUM ? "Medium" : "Hard", 
                    currentRank);
        } else {
            sprintf(rankText, "Rank (%s): Not in top 5", 
                    currentDifficulty == EASY ? "Easy" : 
                    currentDifficulty == MEDIUM ? "Medium" : "Hard");
        }
        textWidth = strlen(rankText) * 6;
        display.setCursor((SCREEN_WIDTH - textWidth) / 2, y);
        display.print(rankText);
        y += 20;

        char statsText[50];
        sprintf(statsText, "Plays: %d, Longest: %lus", playCount, longestSurvivalTime / 1000);
        textWidth = strlen(statsText) * 6;
        display.setCursor((SCREEN_WIDTH - textWidth) / 2, y);
        display.print(statsText);

        xSemaphoreGive(tftMutex);
        Serial.println("Game Over displayed");
    }
}

bool checkCollision(Point p) {
    for (int i = 1; i < snakeLength; i++) {
        if (snake[i].x == p.x && snake[i].y == p.y) {
            Serial.println("Collision with body!");
            return true;
        }
    }
    if (p.x <= 0 || p.x >= NUM_COLS - 1 || p.y <= 0 || p.y >= NUM_ROWS - 1) {
        Serial.println("Collision with wall!");
        return true;
    }
    for (int i = 0; i < obstacleCount; i++) {
        for (int j = 0; j < obstacles[i].length; j++) {
            if (obstacles[i].points[j].x == p.x && obstacles[i].points[j].y == p.y) {
                Serial.println("Collision with obstacle!");
                return true;
            }
        }
    }
    return false;
}

void resetGame() {
    snakeLength = 3;
    snake[0] = {NUM_COLS / 2, NUM_ROWS / 2};
    snake[1] = {NUM_COLS / 2 - 1, NUM_ROWS / 2};
    snake[2] = {NUM_COLS / 2 - 2, NUM_ROWS / 2};
    currentDir = RIGHT;
    score = 0;
    foodType = NORMAL;
    foodSpawnTime = 0;
    foodTimeout = 0;
    gameOver = false;
    gameOverDrawn = false;
    hasDrawnBackground = false;
    lastMoveTime = 0;
    lastBoostTime = 0;
    switch (selectedDifficulty) {
        case EASY: moveDelay = 300; break;
        case MEDIUM: moveDelay = 200; break;
        case HARD: moveDelay = 100; break;
    }
    currentDifficulty = selectedDifficulty;
    initObstacles();
    spawnFood();
    gameStartTime = millis();
    playCount++;
    Serial.println("Game reset with difficulty: " + String(currentDifficulty));
}

void runGameSnake() {
    if (firstRun) {
        resetGame();
        drawBackground();
        drawInitialState();
        firstRun = false;
        hasDrawnBackground = true;
    }

    if (gameOver) {
        if (!gameOverDrawn) {
            unsigned long survivalTime = millis() - gameStartTime;
            if (survivalTime > longestSurvivalTime) {
                longestSurvivalTime = survivalTime;
            }
            displayGameOver();
            gameOverDrawn = true;
        }
        if (checkButton(BTN_RETURN)) {
            currentState = MENU;
            menuNeedsRedraw = true;
            firstRun = true;
            gameOver = false;
            Serial.println("Returning to menu");
        }
        vTaskDelay(20 / portTICK_PERIOD_MS);
        return;
    }

    // Xử lý điều khiển và tăng tốc
    bool boost = false;
    const unsigned long boostDelay = 100; // Thời gian chờ giữa các lần tăng tốc (ms)
    if (millis() - lastBoostTime >= boostDelay) {
        if (checkButton(BTN_RIGHT) && currentDir == RIGHT) {
            boost = true;
            lastBoostTime = millis();
        } else if (checkButton(BTN_LEFT) && currentDir == LEFT) {
            boost = true;
            lastBoostTime = millis();
        } else if (checkButton(BTN_UP) && currentDir == UP) {
            boost = true;
            lastBoostTime = millis();
        } else if (checkButton(BTN_DOWN) && currentDir == DOWN) {
            boost = true;
            lastBoostTime = millis();
        }
    }

    // Xử lý đổi hướng
    if (checkButton(BTN_UP) && currentDir != DOWN) currentDir = UP;
    else if (checkButton(BTN_DOWN) && currentDir != UP) currentDir = DOWN;
    else if (checkButton(BTN_LEFT) && currentDir != RIGHT) currentDir = LEFT;
    else if (checkButton(BTN_RIGHT) && currentDir != LEFT) currentDir = RIGHT;
    if (checkButton(BTN_RETURN)) {
        unsigned long survivalTime = millis() - gameStartTime;
        if (survivalTime > longestSurvivalTime) {
            longestSurvivalTime = survivalTime;
        }
        currentState = MENU;
        menuNeedsRedraw = true;
        firstRun = true;
        Serial.println("Returning to menu");
        return;
    }

    // Kiểm tra thời gian tồn tại của thức ăn
    if (currentDifficulty != EASY && foodSpawnTime > 0 && millis() - foodSpawnTime >= foodTimeout) {
        if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            eraseCell(food.x, food.y);
            xSemaphoreGive(tftMutex);
        }
        spawnFood();
    }

    // Cập nhật rắn (di chuyển định kỳ hoặc tăng tốc)
    bool moved = false;
    if (boost || (millis() - lastMoveTime >= moveDelay)) {
        lastMoveTime = boost ? lastMoveTime : millis(); // Chỉ cập nhật nếu không tăng tốc
        moved = true;
        Point tail = snake[snakeLength - 1];
        for (int i = snakeLength - 1; i > 0; i--) {
            snake[i] = snake[i - 1];
        }
        switch (currentDir) {
            case UP:    snake[0].y--; break;
            case DOWN:  snake[0].y++; break;
            case LEFT:  snake[0].x--; break;
            case RIGHT: snake[0].x++; break;
        }

        if (checkCollision(snake[0])) {
            gameOver = true;
            return;
        }

        bool foodEaten = false;
        if (snake[0].x == food.x && snake[0].y == food.y) {
            if (snakeLength < SNAKE_MAX_LENGTH) {
                snake[snakeLength] = tail;
                snakeLength++;
            }
            score += (foodType == SPECIAL) ? 20 : 10;
            if (currentDifficulty == MEDIUM && score % 10 == 0) {
                moveDelay = max(moveDelay - 10, 100);
            }
            spawnFood();
            foodEaten = true;
            Serial.println("Food eaten");
        }

        if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            eraseCell(tail.x, tail.y);
            drawSnake(snake[0].x, snake[0].y, true);
            for (int i = 1; i < snakeLength; i++) {
                drawSnake(snake[i].x, snake[i].y, false);
            }
            if (!foodEaten) {
                drawFood(food.x, food.y);
            }
            display.setTextColor(TFT_WHITE, TFT_BLACK);
            display.setTextSize(2);
            display.setCursor(10, 10);
            display.print("Score: ");
            display.println(score);
            xSemaphoreGive(tftMutex);
        }
    }

    if (moved) {
        updateObstacles();
    }

    vTaskDelay(20 / portTICK_PERIOD_MS);
}