#include "game_snake.h"   // Bao gồm tệp tiêu đề của game Snake
#include "game_state.h"   // Bao gồm tệp tiêu đề chứa trạng thái game
#include "game_config.h"  // Bao gồm tệp cấu hình game

extern TFT_eSPI display;          // Đối tượng màn hình TFT từ file chính
extern SemaphoreHandle_t tftMutex; // Semaphore bảo vệ truy cập màn hình

// Định nghĩa các biến toàn cục
Direction currentDir = RIGHT;          // Hướng ban đầu của rắn: sang phải
Point snake[SNAKE_MAX_LENGTH];         // Mảng lưu tọa độ các đoạn thân rắn
int snakeLength = 3;                   // Độ dài ban đầu của rắn: 3 ô
Point food;                            // Tọa độ của thức ăn
int score = 0;                         // Điểm số ban đầu
bool hasDrawnBackground = false;       // Cờ kiểm tra xem nền đã vẽ chưa
unsigned long lastMoveTime = 0;        // Thời gian di chuyển cuối cùng
int moveDelay = 200;                   // Giá trị mặc định, sẽ được cập nhật theo độ khó
int maxSnake = 0;                      // Điểm cao nhất của game Snake
bool gameOverDrawn = false;            // Cờ kiểm soát việc vẽ màn hình Game Over
Difficulty currentDifficulty = EASY; // Độ khó mặc định


// Hàm vẽ một đoạn của rắn: đầu tròn có mắt, thân tròn nhỏ hơn
void drawSnake(int x, int y, bool isHead) {
    int pixel_x = x * CELL_SIZE; // Chuyển đổi tọa độ ô sang pixel (x * 10)
    int pixel_y = y * CELL_SIZE; // Chuyển đổi tọa độ ô sang pixel (y * 10)
    if (isHead) {                // Nếu là đầu rắn
        display.fillCircle(pixel_x, pixel_y, 5, TFT_GREEN); // Vẽ đầu tròn, bán kính 5px, màu xanh
        display.fillCircle(pixel_x + 2, pixel_y - 2, 1, TFT_WHITE); // Mắt phải
        display.fillCircle(pixel_x - 2, pixel_y - 2, 1, TFT_WHITE); // Mắt trái
    } else {                     // Nếu là thân rắn
        display.fillCircle(pixel_x, pixel_y, 4, TFT_GREEN); // Vẽ thân tròn, bán kính 4px, màu xanh
    }
    Serial.printf("Drew snake %s at (%d, %d)\n", isHead ? "head" : "body", pixel_x, pixel_y); // Log vị trí
}

// Hàm vẽ thức ăn: ba đốm tròn với màu ngẫu nhiên
void drawFood(int x, int y) {
    int pixel_x = x * CELL_SIZE; // Chuyển đổi tọa độ ô sang pixel
    int pixel_y = y * CELL_SIZE;
    uint16_t random_color = esp_random() & 0xFFFF; // Tạo màu ngẫu nhiên (16-bit)
    display.fillCircle(pixel_x, pixel_y, 3, random_color);        // Đốm chính, bán kính 3px
    display.fillCircle(pixel_x + 2, pixel_y - 2, 2, random_color); // Đốm phụ trên phải
    display.fillCircle(pixel_x - 2, pixel_y + 2, 2, random_color); // Đốm phụ dưới trái
    Serial.printf("Drew food at (%d, %d) with color %d\n", pixel_x, pixel_y, random_color); // Log
}

// Hàm xóa ô cũ trên màn hình
void eraseCell(int x, int y) {
    int pixel_x = x * CELL_SIZE; // Chuyển đổi tọa độ ô sang pixel
    int pixel_y = y * CELL_SIZE;
    display.fillRect(pixel_x - 5, pixel_y - 5, 11, 11, TFT_BLACK); // Xóa hình vuông 11x11px, màu đen
    Serial.printf("Erased cell at (%d, %d)\n", pixel_x, pixel_y);  // Log vị trí xóa
}

// Hàm vẽ nền game
void drawBackground() {
    if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(500)) == pdTRUE) { // Đợi semaphore, tối đa 500ms
        display.fillScreen(TFT_BLACK);         // Đổ nền đen toàn màn hình
        display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, TFT_WHITE); // Vẽ viền trắng bao quanh
        xSemaphoreGive(tftMutex);              // Giải phóng semaphore
    } else {
        Serial.println("ERROR: Failed to take tftMutex in drawBackground!"); // Báo lỗi nếu không lấy được semaphore
    }
}

// Hàm vẽ trạng thái ban đầu của game
void drawInitialState() {
    Serial.println("Drawing initial state...");
    if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(500)) == pdTRUE) { // Đợi semaphore
        for (int i = 0; i < snakeLength; i++) { // Vẽ từng đoạn của rắn
            drawSnake(snake[i].x, snake[i].y, i == 0); // Đầu rắn khi i = 0
        }
        drawFood(food.x, food.y); // Vẽ thức ăn
        xSemaphoreGive(tftMutex); // Giải phóng semaphore
        Serial.println("Initial state drawn");
    } else {
        Serial.println("ERROR: Failed to take tftMutex in drawInitialState!"); // Báo lỗi
    }
}

// Hàm tạo thức ăn ngẫu nhiên
void spawnFood() {
    bool valid = false;
    int attempts = 0;
    while (!valid && attempts < 100) { // Thử tối đa 100 lần
        food.x = (esp_random() % (NUM_COLS - 2)) + 1; // Ngẫu nhiên trong lưới, trừ viền
        food.y = (esp_random() % (NUM_ROWS - 2)) + 1;
        valid = true;
        for (int i = 0; i < snakeLength; i++) { // Kiểm tra trùng với rắn
            if (snake[i].x == food.x && snake[i].y == food.y) {
                valid = false;
                break;
            }
        }
        attempts++;
    }
    if (!valid) { // Nếu không tìm được vị trí hợp lệ
        food.x = NUM_COLS - 2; // Đặt ở góc dưới phải
        food.y = NUM_ROWS - 2;
    }
    Serial.printf("Food spawned at (%d, %d)\n", food.x, food.y); // Log
    drawFood(food.x, food.y); // Vẽ thức ăn
}

// Hàm hiển thị màn hình Game Over
void displayGameOver() {
    if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(500)) == pdTRUE) { // Đợi semaphore
        display.fillScreen(TFT_BLACK); // Đổ nền đen

        // Vẽ "Game Over!" căn giữa
        display.setTextColor(TFT_RED); // Màu đỏ
        display.setTextSize(2);        // Kích thước chữ lớn
        const char* gameOverText = "Game Over!";
        int textWidth = strlen(gameOverText) * 12; // Ước tính chiều rộng (12px/ký tự)
        int x = (240 - textWidth) / 2; // Căn giữa ngang
        int y = 100;                   // Vị trí y
        display.setCursor(x, y);
        display.print(gameOverText);

        // Vẽ "Score: <score>" căn giữa
        char scoreText[20];
        sprintf(scoreText, "Score: %d", score);
        textWidth = strlen(scoreText) * 12;
        x = (240 - textWidth) / 2;
        y = 140; // Cách "Game Over!" 40px
        display.setCursor(x, y);
        display.print(scoreText);

        // Cập nhật và vẽ "High Score: <maxSnake>" căn giữa
        if (score > maxSnake) maxSnake = score; // Cập nhật điểm cao nhất
        char highScoreText[20];
        sprintf(highScoreText, "High Score: %d", maxSnake);
        textWidth = strlen(highScoreText) * 12;
        x = (240 - textWidth) / 2;
        y = 180; // Cách "Score" 40px
        display.setCursor(x, y);
        display.print(highScoreText);

        xSemaphoreGive(tftMutex); // Giải phóng semaphore
        Serial.println("Game Over displayed");
    } else {
        Serial.println("ERROR: Failed to take tftMutex in displayGameOver!"); // Báo lỗi
    }
}

// Hàm kiểm tra va chạm
bool checkCollision(Point p) {
    for (int i = 1; i < snakeLength; i++) { // Kiểm tra va chạm với thân và đầu rắn (p.x, p.y)
        if (snake[i].x == p.x && snake[i].y == p.y) {
            Serial.println("Collision with body!");
            return true;
        }
    }
    // Kiểm tra va chạm với tường
    if (p.x <= 0 || p.x >= NUM_COLS - 1 || p.y <= 0 || p.y >= NUM_ROWS - 1) {
        Serial.println("Collision with wall!");
        return true;
    }
    return false;
}

// Hàm reset game
void resetGame() {
    snakeLength = 3;
    snake[0] = {NUM_COLS / 2, NUM_ROWS / 2};
    snake[1] = {NUM_COLS / 2 - 1, NUM_ROWS / 2};
    snake[2] = {NUM_COLS / 2 - 2, NUM_ROWS / 2};
    currentDir = RIGHT;
    score = 0;
    spawnFood();
    gameOver = false;
    gameOverDrawn = false;
    hasDrawnBackground = false;
    // Đặt moveDelay dựa trên độ khó
    switch (selectedDifficulty) {
        case EASY: moveDelay = 300; break; // Chậm
        case MEDIUM: moveDelay = 200; break; // Trung bình
        case HARD: moveDelay = 100; break; // Nhanh
    }
    currentDifficulty = selectedDifficulty; // Lưu độ khó
    Serial.println("Game reset with difficulty: " + String(currentDifficulty));
}

// Hàm chạy logic chính của game Snake
void runGameSnake() {
    Serial.println("Starting Snake game...");

    if (firstRun) { // Nếu là lần chạy đầu tiên
        Serial.println("First run, initializing...");
        resetGame();         // Reset game
        drawBackground();    // Vẽ nền
        drawInitialState();  // Vẽ trạng thái ban đầu
        firstRun = false;    // Đánh dấu đã chạy lần đầu
        hasDrawnBackground = true; // Đánh dấu nền đã vẽ
    } else {
        Serial.println("Continuing game...");
    }

    if (gameOver) { // Nếu game over
        if (!gameOverDrawn) { // Nếu chưa vẽ màn hình Game Over
            displayGameOver();
            gameOverDrawn = true; // Đánh dấu đã vẽ
        }
        if (checkButton(BTN_RETURN)) { // Nếu nhấn RETURN
            if (score > maxSnake) maxSnake = score; // Cập nhật điểm cao nhất
            currentState = MENU; // Quay lại menu
            menuNeedsRedraw = true; // Đánh dấu cần vẽ lại menu
            firstRun = true; // Đánh dấu lần chạy đầu cho lần sau
            gameOver = false; // Reset trạng thái game over
            Serial.println("Returning to menu");
        }
        vTaskDelay(20 / portTICK_PERIOD_MS); // Trễ 20ms
        return;
    }

    // Xử lý điều khiển hướng
    if (checkButton(BTN_UP) && currentDir != DOWN) currentDir = UP;
    else if (checkButton(BTN_DOWN) && currentDir != UP) currentDir = DOWN;
    else if (checkButton(BTN_LEFT) && currentDir != RIGHT) currentDir = LEFT;
    else if (checkButton(BTN_RIGHT) && currentDir != LEFT) currentDir = RIGHT;
    if (checkButton(BTN_RETURN)) { // Nhấn RETURN để thoát
        if (score > maxSnake) maxSnake = score; // Cập nhật điểm cao nhất
        currentState = MENU;
        menuNeedsRedraw = true;
        firstRun = true;
        Serial.println("Returning to menu");
        return;
    }

    // Cập nhật vị trí rắn theo thời gian
    if (millis() - lastMoveTime >= moveDelay) {
        lastMoveTime = millis(); // Cập nhật thời gian di chuyển
        Point tail = snake[snakeLength - 1]; // Lưu vị trí đuôi cũ
        for (int i = snakeLength - 1; i > 0; i--) { // Di chuyển thân
            snake[i] = snake[i - 1];
        }
        // Di chuyển đầu rắn theo hướng
        switch (currentDir) {
            case UP:    snake[0].y--; break;
            case DOWN:  snake[0].y++; break;
            case LEFT:  snake[0].x--; break;
            case RIGHT: snake[0].x++; break;
        }

        if (checkCollision(snake[0])) { // Kiểm tra va chạm
            gameOver = true; // Kết thúc game
            return;
        }

        bool foodEaten = false;
        if (snake[0].x == food.x && snake[0].y == food.y) { // Nếu ăn thức ăn
            if (snakeLength < SNAKE_MAX_LENGTH) { // Nếu chưa đạt độ dài tối đa
                snake[snakeLength] = tail; // Thêm đoạn mới bằng đuôi cũ
                snakeLength++;
            }
            score += 10; // Tăng điểm
            spawnFood(); // Tạo thức ăn mới
            foodEaten = true;
            Serial.println("Food eaten");
        }

        // Cập nhật màn hình
        eraseCell(tail.x, tail.y); // Xóa ô đuôi cũ
        drawSnake(snake[0].x, snake[0].y, true); // Vẽ đầu mới
        for (int i = 1; i < snakeLength; i++) { // Vẽ lại thân
            drawSnake(snake[i].x, snake[i].y, false);
        }
        if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) { // Đợi semaphore
            display.setTextColor(TFT_WHITE,TFT_BLACK);           // Màu chữ trắng
            display.setTextSize(2);                    // Kích thước chữ lớn hơn
            display.setCursor(10, 10);                 // Đặt con trỏ ở góc trên trái (cách lề trái 10px)
            display.print("Score: ");
            display.println(score);                    // In điểm số
            xSemaphoreGive(tftMutex);                  // Giải phóng semaphore
            Serial.println("Screen updated");
        }
    }

    vTaskDelay(20 / portTICK_PERIOD_MS); // Trễ 20ms để giảm tải CPU
}