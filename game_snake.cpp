#include "game_snake.h"
#include "game_state.h"
#include "game_config.h"

// Khai báo đối tượng màn hình TFT và semaphore từ tệp khác
extern TFT_eSPI display; // Đối tượng điều khiển màn hình TFT
extern SemaphoreHandle_t tftMutex; // Mutex bảo vệ truy cập màn hình TFT

// Khởi tạo các biến toàn cục cho game Snake
Direction currentDir = RIGHT; // Hướng di chuyển ban đầu của rắn
Point snake[SNAKE_MAX_LENGTH]; // Mảng lưu vị trí các đoạn thân rắn
int snakeLength = 3; // Độ dài ban đầu của rắn
Point food; // Vị trí thức ăn hiện tại
Point oldFood; // Vị trí thức ăn cũ (dùng để xóa)
bool needClearOldFood = false; // Cờ đánh dấu cần xóa thức ăn cũ
FoodType foodType = NORMAL; // Loại thức ăn (bình thường hoặc đặc biệt)
unsigned long foodSpawnTime = 0; // Thời gian thức ăn xuất hiện
unsigned long foodTimeout = 0; // Thời gian tồn tại của thức ăn
int score = 0; // Điểm số hiện tại
bool hasDrawnBackground = false; // Cờ đánh dấu nền đã được vẽ
unsigned long lastMoveTime = 0; // Thời gian di chuyển cuối cùng của rắn
unsigned long lastBoostTime = 0; // Thời gian tăng tốc cuối cùng
int moveDelay = 200; // Độ trễ giữa các lần di chuyển (ms)
Difficulty currentDifficulty = EASY; // Độ khó hiện tại
Obstacle obstacles[MAX_OBSTACLES]; // Mảng lưu các chướng ngại vật
int obstacleCount = 0; // Số lượng chướng ngại vật
unsigned long gameStartTime = 0; // Thời gian bắt đầu trò chơi
int currentRank = -1; // Xếp hạng điểm số hiện tại
bool gameOverDrawn = false; // Cờ đánh dấu màn hình Game Over đã được vẽ
bool isInitialMove = true; // Cờ đánh dấu di chuyển đầu tiên (bỏ qua kiểm tra va chạm)
unsigned long lastFullRefresh = 0; // Thời gian làm mới màn hình cuối cùng
bool foodNeedsRedraw = false; // Cờ đánh dấu cần vẽ lại thức ăn
unsigned long lastScoreUpdate = 0; // Thời gian cập nhật điểm số cuối
bool gameOverScreenActive = false; // Cờ kiểm soát trạng thái màn hình Game Over

// Hàm vẽ một đoạn thân rắn
void drawSnake(int x, int y, bool isHead) {
    // Chuyển tọa độ lưới sang tọa độ pixel
    int pixel_x = x * CELL_SIZE;
    int pixel_y = y * CELL_SIZE;
    
    // Kiểm tra nếu đoạn thân nằm trong vùng hiển thị điểm số
    if (pixel_y < SCORE_AREA_HEIGHT) {
        return; // Không vẽ để tránh ghi đè lên vùng điểm số
    }
    
    // Vẽ đầu rắn (lớn hơn, có mắt) hoặc thân rắn
    if (isHead) {
        display.fillCircle(pixel_x, pixel_y, 5, TFT_GREEN); // Vẽ đầu rắn màu xanh
        display.fillCircle(pixel_x + 2, pixel_y - 2, 1, TFT_WHITE); // Mắt phải
        display.fillCircle(pixel_x - 2, pixel_y - 2, 1, TFT_WHITE); // Mắt trái
    } else {
        display.fillCircle(pixel_x, pixel_y, 4, TFT_GREEN); // Vẽ thân rắn nhỏ hơn
    }
    // Ghi log vị trí và loại đoạn thân
    Serial.printf("Drew snake %s at (%d, %d)\n", isHead ? "head" : "body", pixel_x, pixel_y);
}

// Hàm vẽ thức ăn
void drawFood(int x, int y) {
    // Chuyển tọa độ lưới sang tọa độ pixel
    int pixel_x = x * CELL_SIZE;
    int pixel_y = y * CELL_SIZE;
    
    // Kiểm tra nếu thức ăn nằm trong vùng điểm số
    if (pixel_y < SCORE_AREA_HEIGHT) {
        food.y = SCORE_AREA_HEIGHT / CELL_SIZE + 1; // Đặt lại vị trí thức ăn
        pixel_y = food.y * CELL_SIZE;
    }
    
    // Chọn màu: vàng cho thức ăn đặc biệt, ngẫu nhiên cho thức ăn thường
    uint16_t color = (foodType == SPECIAL) ? TFT_YELLOW : (esp_random() & 0xFFFF);
    display.fillCircle(pixel_x, pixel_y, 3, color); // Vẽ hình tròn chính
    display.fillCircle(pixel_x + 2, pixel_y - 2, 2, color); // Điểm trang trí
    display.fillCircle(pixel_x - 2, pixel_y + 2, 2, color); // Điểm trang trí
    // Ghi log vị trí và màu thức ăn
    Serial.printf("Drew food at (%d, %d) with color %d\n", pixel_x, pixel_y, color);
}

// Hàm vẽ viền màn hình
void drawBorder() {
    // Vẽ viền trái (từ dưới vùng điểm số đến cuối màn hình)
    display.drawFastVLine(0, SCORE_AREA_HEIGHT, SCREEN_HEIGHT - SCORE_AREA_HEIGHT, TFT_WHITE);
    display.drawFastVLine(1, SCORE_AREA_HEIGHT, SCREEN_HEIGHT - SCORE_AREA_HEIGHT, TFT_WHITE);
    
    // Vẽ viền phải
    display.drawFastVLine(SCREEN_WIDTH-1, SCORE_AREA_HEIGHT, SCREEN_HEIGHT - SCORE_AREA_HEIGHT, TFT_WHITE);
    display.drawFastVLine(SCREEN_WIDTH-2, SCORE_AREA_HEIGHT, SCREEN_HEIGHT - SCORE_AREA_HEIGHT, TFT_WHITE);
    
    // Vẽ viền dưới
    display.drawFastHLine(0, SCREEN_HEIGHT-1, SCREEN_WIDTH, TFT_WHITE);
    display.drawFastHLine(0, SCREEN_HEIGHT-2, SCREEN_WIDTH, TFT_WHITE);
    
    Serial.println("Border drawn"); // Ghi log khi viền được vẽ
}

// Hàm vẽ khu vực hiển thị điểm số
void drawScoreArea() {
    // Không vẽ nếu màn hình Game Over đang hiển thị
    if (gameOverScreenActive) {
        return;
    }
    
    // Xóa vùng điểm số bằng màu đen
    display.fillRect(0, 0, SCREEN_WIDTH, SCORE_AREA_HEIGHT, TFT_BLACK);
    
    // Vẽ đường phân cách dưới vùng điểm số
    display.drawFastHLine(0, SCORE_AREA_HEIGHT, SCREEN_WIDTH, TFT_WHITE);
    display.drawFastHLine(0, SCORE_AREA_HEIGHT+1, SCREEN_WIDTH, TFT_WHITE);
    
    // Hiển thị điểm số
    display.setTextColor(TFT_WHITE); // Màu chữ trắng
    display.setTextSize(2); // Kích thước chữ
    display.setCursor(10, 10); // Vị trí bắt đầu
    display.print("Score: "); // In nhãn
    display.print(score); // In điểm số
    
    lastScoreUpdate = millis(); // Cập nhật thời gian vẽ điểm số
    Serial.println("Score area drawn"); // Ghi log
}

// Hàm xóa một ô trên màn hình
void eraseCell(int x, int y) {
    // Không xóa nếu màn hình Game Over đang hiển thị
    if (gameOverScreenActive) {
        return;
    }
    
    // Chuyển tọa độ lưới sang tọa độ pixel
    int pixel_x = x * CELL_SIZE;
    int pixel_y = y * CELL_SIZE;
    
    // Không xóa nếu vị trí nằm trong vùng điểm số
    if (pixel_y < SCORE_AREA_HEIGHT) {
        return;
    }
    
    // Kiểm tra nếu vị trí xóa gần viền màn hình
    if (pixel_x < 15 || pixel_x > SCREEN_WIDTH - 15 || 
        pixel_y < SCORE_AREA_HEIGHT + 15 || pixel_y > SCREEN_HEIGHT - 15) {
        // Xóa vùng nhỏ hơn để tránh làm hỏng viền
        display.fillRect(pixel_x - 5, pixel_y - 5, 10, 10, TFT_BLACK);
        // Vẽ lại viền nếu cần
        if (pixel_x < 15 || pixel_x > SCREEN_WIDTH - 15 || 
            pixel_y > SCREEN_HEIGHT - 15) {
            drawBorder();
        }
        // Vẽ lại đường phân cách nếu cần
        if (pixel_y >= SCORE_AREA_HEIGHT && pixel_y < SCORE_AREA_HEIGHT + 15) {
            display.drawFastHLine(0, SCORE_AREA_HEIGHT, SCREEN_WIDTH, TFT_WHITE);
            display.drawFastHLine(0, SCORE_AREA_HEIGHT+1, SCREEN_WIDTH, TFT_WHITE);
        }
    } else {
        // Kiểm tra xem việc xóa có ảnh hưởng đến thức ăn không
        bool willAffectFood = false;
        int food_pixel_x = food.x * CELL_SIZE;
        int food_pixel_y = food.y * CELL_SIZE;
        
        // Tính khoảng cách từ vị trí xóa đến thức ăn
        int dx = abs(pixel_x - food_pixel_x);
        int dy = abs(pixel_y - food_pixel_y);
        
        // Nếu khoảng cách nhỏ, có nguy cơ xóa thức ăn
        if (dx < 15 && dy < 15) {
            willAffectFood = true;
        }
        
        // Xóa vùng lớn hơn
        display.fillRect(pixel_x - 15, pixel_y - 15, 30, 30, TFT_BLACK);
        
        // Vẽ lại thức ăn nếu bị ảnh hưởng
        if (willAffectFood) {
            drawFood(food.x, food.y);
        }
    }
    
    // Ghi log vị trí xóa
    Serial.printf("Erased cell at (%d, %d)\n", pixel_x, pixel_y);
}

// Hàm vẽ nền màn hình
void drawBackground() {
    // Thử lấy mutex để truy cập màn hình
    if (xSemaphoreTake(tftMutex, 0) == pdTRUE) {
        display.fillScreen(TFT_BLACK); // Xóa màn hình bằng màu đen
        drawBorder(); // Vẽ viền
        drawScoreArea(); // Vẽ vùng điểm số
        xSemaphoreGive(tftMutex); // Giải phóng mutex
        Serial.println("Background drawn, tftMutex released");
    } else if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Thử lại lần 2 nếu lần đầu thất bại
        display.fillScreen(TFT_BLACK);
        drawBorder();
        drawScoreArea();
        xSemaphoreGive(tftMutex);
        Serial.println("Background drawn, tftMutex released");
    } else if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Thử lại lần 3
        display.fillScreen(TFT_BLACK);
        drawBorder();
        drawScoreArea();
        xSemaphoreGive(tftMutex);
        Serial.println("Background drawn, tftMutex released");
    } else {
        // Ghi log lỗi nếu không lấy được mutex
        Serial.println("ERROR: Failed to take tftMutex in drawBackground!");
    }
}

// Hàm vẽ trạng thái ban đầu của trò chơi
void drawInitialState() {
    // Thử lấy mutex để vẽ
    if (xSemaphoreTake(tftMutex, 0) == pdTRUE) {
        display.fillScreen(TFT_BLACK); // Xóa màn hình
        drawBorder(); // Vẽ viền
        drawScoreArea(); // Vẽ vùng điểm số
        
        // Vẽ rắn
        for (int i = 0; i < snakeLength; i++) {
            drawSnake(snake[i].x, snake[i].y, i == 0); // Đầu rắn nếu i == 0
        }
        drawFood(food.x, food.y); // Vẽ thức ăn
        
        // Vẽ chướng ngại vật nếu có
        if (obstacleCount > 0) {
            for (int i = 0; i < obstacleCount; i++) {
                for (int j = 0; j < obstacles[i].length; j++) {
                    int pixel_x = obstacles[i].points[j].x * CELL_SIZE;
                    int pixel_y = obstacles[i].points[j].y * CELL_SIZE;
                    // Chỉ vẽ nếu không nằm trong vùng điểm số
                    if (pixel_y >= SCORE_AREA_HEIGHT) {
                        display.fillRect(pixel_x - 5, pixel_y - 5, 10, 10, TFT_RED);
                    }
                }
            }
        }
        
        xSemaphoreGive(tftMutex); // Giải phóng mutex
        Serial.println("Initial state drawn, tftMutex released");
    } else if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Thử lại lần 2
        display.fillScreen(TFT_BLACK);
        drawBorder();
        drawScoreArea();
        for (int i = 0; i < snakeLength; i++) {
            drawSnake(snake[i].x, snake[i].y, i == 0);
        }
        drawFood(food.x, food.y);
        if (obstacleCount > 0) {
            for (int i = 0; i < obstacleCount; i++) {
                for (int j = 0; j < obstacles[i].length; j++) {
                    int pixel_x = obstacles[i].points[j].x * CELL_SIZE;
                    int pixel_y = obstacles[i].points[j].y * CELL_SIZE;
                    if (pixel_y >= SCORE_AREA_HEIGHT) {
                        display.fillRect(pixel_x - 5, pixel_y - 5, 10, 10, TFT_RED);
                    }
                }
            }
        }
        xSemaphoreGive(tftMutex);
        Serial.println("Initial state drawn, tftMutex released");
    } else if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Thử lại lần 3
        display.fillScreen(TFT_BLACK);
        drawBorder();
        drawScoreArea();
        for (int i = 0; i < snakeLength; i++) {
            drawSnake(snake[i].x, snake[i].y, i == 0);
        }
        drawFood(food.x, food.y);
        if (obstacleCount > 0) {
            for (int i = 0; i < obstacleCount; i++) {
                for (int j = 0; j < obstacles[i].length; j++) {
                    int pixel_x = obstacles[i].points[j].x * CELL_SIZE;
                    int pixel_y = obstacles[i].points[j].y * CELL_SIZE;
                    if (pixel_y >= SCORE_AREA_HEIGHT) {
                        display.fillRect(pixel_x - 5, pixel_y - 5, 10, 10, TFT_RED);
                    }
                }
            }
        }
        xSemaphoreGive(tftMutex);
        Serial.println("Initial state drawn, tftMutex released");
    } else {
        // Ghi log lỗi
        Serial.println("ERROR: Failed to take tftMutex in drawInitialState!");
    }
}

// Hàm xóa vùng thức ăn
void clearFoodArea(int x, int y) {
    // Không xóa nếu màn hình Game Over đang hiển thị
    if (gameOverScreenActive) {
        return;
    }
    
    // Lấy mutex để truy cập màn hình
    if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        int pixel_x = x * CELL_SIZE;
        int pixel_y = y * CELL_SIZE;
        
        // Không xóa nếu nằm trong vùng điểm số
        if (pixel_y < SCORE_AREA_HEIGHT) {
            xSemaphoreGive(tftMutex);
            return;
        }
        
        // Kiểm tra nếu vị trí xóa gần viền
        if (pixel_x < 20 || pixel_x > SCREEN_WIDTH - 20 || 
            pixel_y < SCORE_AREA_HEIGHT + 20 || pixel_y > SCREEN_HEIGHT - 20) {
            // Xóa vùng nhỏ hơn để tránh làm hỏng viền
            display.fillRect(pixel_x - 10, pixel_y - 10, 20, 20, TFT_BLACK);
            drawBorder(); // Vẽ lại viền
            // Vẽ lại đường phân cách nếu cần
            if (pixel_y < SCORE_AREA_HEIGHT + 20) {
                display.drawFastHLine(0, SCORE_AREA_HEIGHT, SCREEN_WIDTH, TFT_WHITE);
                display.drawFastHLine(0, SCORE_AREA_HEIGHT+1, SCREEN_WIDTH, TFT_WHITE);
            }
        } else {
            // Xóa vùng lớn hơn để đảm bảo xóa hết thức ăn
            display.fillRect(pixel_x - 20, pixel_y - 20, 40, 40, TFT_BLACK);
        }
        
        xSemaphoreGive(tftMutex); // Giải phóng mutex
        Serial.printf("Cleared food area at (%d, %d)\n", x, y); // Ghi log
    } else {
        Serial.println("ERROR: Failed to take tftMutex for clearing food area");
    }
}

// Hàm vẽ tất cả chướng ngại vật
void drawObstacles() {
    // Không vẽ nếu màn hình Game Over đang hiển thị
    if (gameOverScreenActive) {
        return;
    }
    
    // Lấy mutex để truy cập màn hình
    if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        Serial.println("tftMutex taken by drawObstacles");
        // Vẽ từng chướng ngại vật
        for (int i = 0; i < obstacleCount; i++) {
            for (int j = 0; j < obstacles[i].length; j++) {
                int pixel_x = obstacles[i].points[j].x * CELL_SIZE;
                int pixel_y = obstacles[i].points[j].y * CELL_SIZE;
                // Chỉ vẽ nếu không nằm trong vùng điểm số
                if (pixel_y >= SCORE_AREA_HEIGHT) {
                    display.fillRect(pixel_x - 5, pixel_y - 5, 10, 10, TFT_RED); // Vẽ hình vuông đỏ
                }
            }
        }
        drawBorder(); // Vẽ lại viền để đảm bảo không bị xóa
        xSemaphoreGive(tftMutex); // Giải phóng mutex
        Serial.println("Obstacles drawn, tftMutex released");
    } else {
        Serial.println("ERROR: Failed to take tftMutex in drawObstacles!");
    }
}

// Hàm khởi tạo chướng ngại vật
void initObstacles() {
    obstacleCount = 0; // Đặt lại số lượng chướng ngại vật
    if (currentDifficulty == MEDIUM) {
        int numObstacles = 3; // Số chướng ngại vật ở mức trung bình
        // Phạm vi tọa độ x cho từng chướng ngại vật
        int xRanges[3][2] = {{3, 9}, {10, 16}, {17, 21}};
        for (int i = 0; i < numObstacles && obstacleCount < MAX_OBSTACLES; i++) {
            obstacles[obstacleCount].length = 5 + (esp_random() % 3); // Độ dài ngẫu nhiên (5-7)
            obstacles[obstacleCount].isMoving = false; // Không di chuyển
            int startX, startY;
            bool valid = false;
            int shape = -1;
            int attempts = 0;
            // Thử tối đa 50 lần để tìm vị trí hợp lệ
            while (!valid && attempts < 50) {
                startX = xRanges[i][0] + (esp_random() % (xRanges[i][1] - xRanges[i][0] + 1));
                // Đặt y để tránh vùng điểm số
                startY = (SCORE_AREA_HEIGHT / CELL_SIZE) + 1 + (esp_random() % (NUM_ROWS - (SCORE_AREA_HEIGHT / CELL_SIZE) - 6));
                
                shape = esp_random() % 4; // Chọn hình dạng ngẫu nhiên (L, T, thẳng, zigzag)
                Point tempPoints[8]; // Mảng tạm để lưu điểm
                tempPoints[0] = {startX, startY};
                bool shapeValid = true;
                // Tạo các điểm theo hình dạng
                for (int j = 1; j < obstacles[obstacleCount].length; j++) {
                    switch (shape) {
                        case 0: // Hình chữ L
                            tempPoints[j] = (j < obstacles[obstacleCount].length - 1) ? 
                                Point{startX, startY + j} : Point{startX + 1, startY + j - 1};
                            break;
                        case 1: // Hình chữ T
                            tempPoints[j] = (j < obstacles[obstacleCount].length - 2) ? 
                                Point{startX, startY + j} : (j == obstacles[obstacleCount].length - 2) ? 
                                Point{startX - 1, startY + j - 1} : Point{startX + 1, startY + j - 2};
                            break;
                        case 2: // Đường thẳng ngang
                            tempPoints[j] = Point{startX + j, startY};
                            break;
                        case 3: // Hình zigzag
                            tempPoints[j] = (j % 2 == 0) ? 
                                Point{startX + j / 2, startY + 1} : Point{startX + j / 2, startY};
                            break;
                    }
                    // Kiểm tra điểm có hợp lệ (không vượt biên hoặc vào vùng điểm số)
                    if (tempPoints[j].x < 3 || tempPoints[j].x >= NUM_COLS - 4 ||
                        tempPoints[j].y < (SCORE_AREA_HEIGHT / CELL_SIZE) + 1 || tempPoints[j].y >= NUM_ROWS - 4) {
                        shapeValid = false;
                        break;
                    }
                }
                if (!shapeValid) {
                    attempts++;
                    continue;
                }
                // Kiểm tra khoảng cách với rắn và các chướng ngại vật khác
                valid = abs(startX - snake[0].x) > 5 || abs(startY - snake[0].y) > 5;
                for (int k = 0; k < obstacleCount; k++) {
                    if (abs(startX - obstacles[k].points[0].x) <= 5 && abs(startY - obstacles[k].points[0].y) <= 5) {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    for (int j = 0; j < obstacles[obstacleCount].length; j++) {
                        obstacles[obstacleCount].points[j] = tempPoints[j];
                    }
                }
                attempts++;
            }
            // Nếu không tìm được vị trí hợp lệ, dùng vị trí mặc định
            if (!valid) {
                startX = xRanges[i][0] + 3;
                startY = (SCORE_AREA_HEIGHT / CELL_SIZE) + 5;
                shape = 2; // Đường thẳng ngang
                obstacles[obstacleCount].points[0] = {startX, startY};
                for (int j = 1; j < obstacles[obstacleCount].length; j++) {
                    obstacles[obstacleCount].points[j] = {startX + j, startY};
                }
            }
            obstacleCount++; // Tăng số lượng chướng ngại vật
        }
    } else if (currentDifficulty == HARD) {
        obstacleCount = 2; // 2 chướng ngại vật ở mức khó
        
        // Chướng ngại vật 1 (phần trên màn hình)
        obstacles[0].length = 5 + (esp_random() % 3); // Độ dài ngẫu nhiên
        obstacles[0].isMoving = true; // Di chuyển
        obstacles[0].direction = 1; // Hướng phải
        
        // Đặt vị trí xa rắn
        int startX1 = 3 + (esp_random() % (NUM_COLS / 2 - obstacles[0].length - 3));
        int startY1 = (SCORE_AREA_HEIGHT / CELL_SIZE) + 2 + (esp_random() % 4);
        
        for (int j = 0; j < obstacles[0].length; j++) {
            obstacles[0].points[j] = {startX1 + j, startY1}; // Đường thẳng ngang
        }
        
        // Chướng ngại vật 2 (phần dưới màn hình)
        obstacles[1].length = 5 + (esp_random() % 3);
        obstacles[1].isMoving = true;
        obstacles[1].direction = -1; // Hướng trái
        
        // Đặt vị trí xa rắn
        int startX2 = NUM_COLS / 2 + (esp_random() % (NUM_COLS / 2 - obstacles[1].length - 3));
        int startY2 = NUM_ROWS - 10 - (esp_random() % 4);
        
        for (int j = 0; j < obstacles[1].length; j++) {
            obstacles[1].points[j] = {startX2 + j, startY2};
        }
        
        // Đảm bảo chướng ngại vật không quá gần rắn
        if (abs(startY1 - NUM_ROWS/2) < 5) {
            startY1 = (SCORE_AREA_HEIGHT / CELL_SIZE) + 4;
            for (int j = 0; j < obstacles[0].length; j++) {
                obstacles[0].points[j].y = startY1;
            }
        }
        
        if (abs(startY2 - NUM_ROWS/2) < 5) {
            startY2 = NUM_ROWS - 8;
            for (int j = 0; j < obstacles[1].length; j++) {
                obstacles[1].points[j].y = startY2;
            }
        }
    }
}

// Hàm cập nhật vị trí chướng ngại vật (chỉ ở mức khó)
void updateObstacles() {
    // Không cập nhật nếu Game Over hoặc màn hình Game Over đang hiển thị
    if (gameOverScreenActive || gameOver) {
        return;
    }
    
    static unsigned long lastObstacleUpdate = 0; // Thời gian cập nhật cuối
    unsigned long now = millis();
    
    // Chỉ cập nhật mỗi 150ms để giảm nháy màn hình
    if (now - lastObstacleUpdate < 150) {
        return;
    }
    
    lastObstacleUpdate = now;
    
    // Chỉ cập nhật ở mức khó
    if (currentDifficulty == HARD && !gameOver) {
        // Lấy mutex để truy cập màn hình
        if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            for (int i = 0; i < obstacleCount; i++) {
                bool willCollide = false;
                // Kiểm tra va chạm với thân rắn
                for (int j = 0; j < obstacles[i].length; j++) {
                    int newX = obstacles[i].points[j].x + obstacles[i].direction;
                    int newY = obstacles[i].points[j].y;
                    for (int k = 1; k < snakeLength; k++) {
                        if (newX == snake[k].x && newY == snake[k].y) {
                            willCollide = true;
                            break;
                        }
                    }
                    if (willCollide) break;
                }
                
                // Xóa chướng ngại vật cũ
                for (int j = 0; j < obstacles[i].length; j++) {
                    int pixel_x = obstacles[i].points[j].x * CELL_SIZE;
                    int pixel_y = obstacles[i].points[j].y * CELL_SIZE;
                    display.fillRect(pixel_x - 5, pixel_y - 5, 10, 10, TFT_BLACK);
                }
                
                // Cập nhật vị trí
                if (willCollide) {
                    obstacles[i].direction = -obstacles[i].direction; // Đảo hướng nếu va chạm
                    Serial.printf("Obstacle %d reversed direction to %d\n", i, obstacles[i].direction);
                } else {
                    for (int j = 0; j < obstacles[i].length; j++) {
                        obstacles[i].points[j].x += obstacles[i].direction; // Di chuyển
                    }
                }
                
                // Kiểm tra giới hạn biên
                int min_x = obstacles[i].points[0].x;
                int max_x = obstacles[i].points[0].x;
                for (int j = 1; j < obstacles[i].length; j++) {
                    if (obstacles[i].points[j].x < min_x) min_x = obstacles[i].points[j].x;
                    if (obstacles[i].points[j].x > max_x) max_x = obstacles[i].points[j].x;
                }
                if (max_x >= NUM_COLS - 2) obstacles[i].direction = -1; // Đảo hướng nếu chạm biên phải
                if (min_x <= 1) obstacles[i].direction = 1; // Đảo hướng nếu chạm biên trái
                
                // Vẽ lại chướng ngại vật mới
                for (int j = 0; j < obstacles[i].length; j++) {
                    int pixel_x = obstacles[i].points[j].x * CELL_SIZE;
                    int pixel_y = obstacles[i].points[j].y * CELL_SIZE;
                    if (pixel_y >= SCORE_AREA_HEIGHT) {
                        display.fillRect(pixel_x - 5, pixel_y - 5, 10, 10, TFT_RED);
                    }
                }
            }
            
            drawBorder(); // Vẽ lại viền
            xSemaphoreGive(tftMutex); // Giải phóng mutex
        }
    }
}

// Hàm tạo thức ăn mới
void spawnFood() {
    // Không tạo thức ăn nếu Game Over hoặc màn hình Game Over đang hiển thị
    if (gameOverScreenActive || gameOver) {
        return;
    }
    
    oldFood = food; // Lưu vị trí thức ăn cũ
    needClearOldFood = true; // Đánh dấu cần xóa thức ăn cũ
    
    bool valid = false;
    int attempts = 0;
    const int maxAttempts = 50;
    // Tìm vị trí hợp lệ cho thức ăn
    while (!valid && attempts < maxAttempts) {
        food.x = (esp_random() % (NUM_COLS - 2)) + 1; // Ngẫu nhiên tọa độ x
        // Đảm bảo không nằm trong vùng điểm số
        food.y = (SCORE_AREA_HEIGHT / CELL_SIZE) + 1 + (esp_random() % (NUM_ROWS - (SCORE_AREA_HEIGHT / CELL_SIZE) - 2));
        
        valid = true;
        // Kiểm tra trùng với rắn
        for (int i = 0; i < snakeLength; i++) {
            if (snake[i].x == food.x && snake[i].y == food.y) {
                valid = false;
                break;
            }
        }
        // Kiểm tra trùng với chướng ngại vật
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
    // Nếu không tìm được vị trí hợp lệ, dùng vị trí mặc định
    if (!valid) {
        food.x = NUM_COLS - 2;
        food.y = max((SCORE_AREA_HEIGHT / CELL_SIZE) + 1, NUM_ROWS - 2);
    }
    // Xác định loại thức ăn (đặc biệt với tỷ lệ 1/SPECIAL_FOOD_CHANCE)
    foodType = (esp_random() % SPECIAL_FOOD_CHANCE == 0) ? SPECIAL : NORMAL;
    // Đặt thời gian tồn tại của thức ăn
    foodTimeout = (foodType == SPECIAL) ? 2000 : 
                  (currentDifficulty == EASY) ? 0 : 
                  (currentDifficulty == MEDIUM) ? (5000 + (esp_random() % 3000)) : 
                  (3000 + (esp_random() % 2000));
    foodSpawnTime = millis(); // Lưu thời gian tạo
    foodNeedsRedraw = true; // Đánh dấu cần vẽ lại
    // Ghi log vị trí thức ăn
    Serial.println("Food spawned at (" + String(food.x) + ", " + String(food.y) + ")");
}

// Hàm cập nhật bảng điểm cao
void updateHighScores() {
    currentRank = -1; // Đặt lại xếp hạng
    int* highScores;
    // Chọn bảng điểm cao theo độ khó
    switch (currentDifficulty) {
        case EASY: highScores = highScoresEasy; break;
        case MEDIUM: highScores = highScoresMedium; break;
        case HARD: highScores = highScoresHard; break;
        default: return;
    }
    // Kiểm tra nếu điểm số đã có trong bảng
    for (int i = 0; i < 5; i++) {
        if (score == highScores[i] && score != 0) {
            currentRank = i + 1;
            return;
        }
    }
    // Thêm điểm số mới nếu cao hơn hoặc bảng còn chỗ trống
    for (int i = 0; i < 5; i++) {
        if (score > highScores[i] || highScores[i] == 0) {
            currentRank = i + 1;
            for (int j = 4; j > i; j--) {
                highScores[j] = highScores[j - 1]; // Dịch các điểm số xuống
            }
            highScores[i] = score; // Thêm điểm số mới
            break;
        }
    }
}

// Hàm hiển thị màn hình Game Over
void displayGameOver() {
    gameOverScreenActive = true; // Đánh dấu màn hình Game Over đang hiển thị
    
    // Lấy mutex để truy cập màn hình
    if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        Serial.println("tftMutex taken by displayGameOver");
        
        display.fillScreen(TFT_BLACK); // Xóa màn hình
        
        int y = 20;
        display.setTextColor(TFT_RED); // Màu chữ đỏ
        display.setTextSize(2); // Kích thước chữ
        const char* gameOverText = "Game Over!";
        int textWidth = strlen(gameOverText) * 12;
        display.setCursor((SCREEN_WIDTH - textWidth) / 2, y); // Căn giữa
        display.print(gameOverText); // In "Game Over!"
        y += 30;
        // In điểm số
        char scoreText[20];
        sprintf(scoreText, "Score: %d", score);
        textWidth = strlen(scoreText) * 12;
        display.setCursor((SCREEN_WIDTH - textWidth) / 2, y);
        display.print(scoreText);
        y += 30;
        display.setTextColor(TFT_WHITE); // Màu chữ trắng
        display.setTextSize(1); // Kích thước chữ nhỏ
        // Chia màn hình thành 3 cột cho Easy, Medium, Hard
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
        updateHighScores(); // Cập nhật bảng điểm cao
        // Hiển thị bảng điểm cao
        for (int i = 0; i < 5; i++) {
            char easyText[10];
            sprintf(easyText, "%d. %d", i + 1, highScoresEasy[i]);
            textWidth = strlen(easyText) * 6;
            display.setCursor(xEasy, y);
            if (currentDifficulty == EASY && score == highScoresEasy[i] && score != 0) {
                display.setTextColor(TFT_RED); // Tô đỏ nếu là điểm số hiện tại
                display.drawRect(xEasy - 5, y - 2, textWidth + 10, 12, TFT_RED); // Vẽ khung
            } else {
                display.setTextColor(TFT_WHITE);
            }
            display.print(easyText);
            char mediumText[10];
            sprintf(mediumText, "%d. %d", i + 1, highScoresMedium[i]);
            textWidth = strlen(mediumText) * 6;
            display.setCursor(xMedium, y);
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
            display.setCursor(xHard, y);
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
        // Hiển thị xếp hạng
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
        // Hiển thị thống kê
        char statsText[50];
        sprintf(statsText, "Plays: %d, Longest: %lus", playCount, longestSurvivalTime / 1000);
        textWidth = strlen(statsText) * 6;
        display.setCursor((SCREEN_WIDTH - textWidth) / 2, y);
        display.print(statsText);
        
        xSemaphoreGive(tftMutex); // Giải phóng mutex
        Serial.println("Game Over displayed, tftMutex released");
    } else {
        Serial.println("ERROR: Failed to take tftMutex in displayGameOver!");
    }
}

// Hàm kiểm tra va chạm
bool checkCollision(Point p) {
    // Không kiểm tra nếu màn hình Game Over đang hiển thị
    if (gameOverScreenActive) {
        return false;
    }
    
    Serial.printf("Checking collision at (%d, %d)\n", p.x, p.y);
    // Bỏ qua kiểm tra va chạm cho di chuyển đầu tiên
    if (isInitialMove) {
        Serial.println("Skipping initial collision check");
        isInitialMove = false;
        return false;
    }
    // Kiểm tra va chạm với thân rắn
    for (int i = 1; i < snakeLength; i++) {
        if (snake[i].x == p.x && snake[i].y == p.y) {
            Serial.println("Collision with body!");
            displayGameOver(); // Hiển thị màn hình Game Over
            return true;
        }
    }
    // Kiểm tra va chạm với tường
    if (p.x <= 0 || p.x >= NUM_COLS - 1 || 
        p.y <= (SCORE_AREA_HEIGHT / CELL_SIZE) || p.y >= NUM_ROWS - 1) {
        Serial.println("Collision with wall!");
        displayGameOver();
        return true;
    }
    // Kiểm tra va chạm với chướng ngại vật
    for (int i = 0; i < obstacleCount; i++) {
        for (int j = 0; j < obstacles[i].length; j++) {
            if (obstacles[i].points[j].x == p.x && obstacles[i].points[j].y == p.y) {
                Serial.println("Collision with obstacle!");
                displayGameOver();
                return true;
            }
        }
    }
    return false; // Không có va chạm
}

// Hàm đặt lại trạng thái trò chơi
void resetGame() {
    gameOverScreenActive = false; // Đặt lại trạng thái màn hình Game Over
    
    snakeLength = 3; // Độ dài rắn ban đầu
    // Khởi tạo vị trí rắn
    snake[0] = {NUM_COLS / 2, NUM_ROWS / 2};
    snake[1] = {NUM_COLS / 2 - 1, NUM_ROWS / 2};
    snake[2] = {NUM_COLS / 2 - 2, NUM_ROWS / 2};
    Serial.printf("Initial snake: head at (%d, %d), length: %d\n", snake[0].x, snake[0].y, snakeLength);
    currentDir = RIGHT; // Hướng ban đầu
    score = 0; // Đặt lại điểm số
    foodType = NORMAL; // Loại thức ăn mặc định
    foodSpawnTime = 0;
    foodTimeout = 0;
    gameOver = false; // Đặt lại trạng thái Game Over
    gameOverDrawn = false;
    hasDrawnBackground = false; // Đánh dấu chưa vẽ nền
    isInitialMove = true; // Đánh dấu di chuyển đầu tiên
    lastMoveTime = millis() + 500; // Trì hoãn di chuyển đầu tiên 500ms
    lastBoostTime = 0;
    lastFullRefresh = 0;
    foodNeedsRedraw = false;
    needClearOldFood = false;
    // Đặt độ trễ di chuyển theo độ khó
    switch (selectedDifficulty) {
        case EASY: moveDelay = 300; break;
        case MEDIUM: moveDelay = 200; break;
        case HARD: moveDelay = 100; break;
    }
    currentDifficulty = selectedDifficulty; // Lưu độ khó hiện tại
    // Xóa màn hình
    for (int i = 0; i < 3; i++) {
        if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            display.fillScreen(TFT_BLACK);
            xSemaphoreGive(tftMutex);
            Serial.println("Screen cleared in resetGame, attempt " + String(i + 1));
            break;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); // Chờ nếu không lấy được mutex
    }
    drawBackground(); // Vẽ nền
    initObstacles(); // Khởi tạo chướng ngại vật
    spawnFood(); // Tạo thức ăn mới
    gameStartTime = millis(); // Lưu thời gian bắt đầu
    playCount++; // Tăng số lần chơi
    // Gửi trạng thái ban đầu đến hàng đợi
    UpdateData initData = {
        .type = UPDATE_SNAKE,
        .snakeLength = snakeLength,
        .food = food,
        .foodType = foodType,
        .obstacleCount = obstacleCount,
        .score = score,
        .gameOver = false
    };
    if (xQueueSendToFront(snakeUpdateQueue, &initData, pdMS_TO_TICKS(10)) == pdPASS) {
        Serial.println("Sent initial UPDATE_SNAKE to snakeUpdateQueue");
    } else {
        Serial.println("ERROR: Failed to send initial UPDATE_SNAKE to snakeUpdateQueue!");
    }
}

// Hàm chạy trò chơi Snake
void runGameSnake() {
    if (firstRun) { // Nếu là lần chạy đầu tiên
        resetGame(); // Đặt lại trạng thái
        firstRun = false; // Đánh dấu đã chạy
    }
}

// Task cập nhật logic trò chơi
void snakeUpdateTask(void *parameter) {
    static unsigned long lastFoodCheck = 0; // Thời gian kiểm tra thức ăn cuối
    while (1) {
        unsigned long now = millis(); // Lấy thời gian hiện tại
        // Kiểm tra trạng thái trò chơi
        if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (currentState != GAME1) { // Nếu không ở trạng thái Snake
                gameOverScreenActive = false; // Đặt lại trạng thái Game Over
                xSemaphoreGive(stateMutex);
                vTaskDelay(50 / portTICK_PERIOD_MS); // Chờ 50ms
                continue;
            }
            xSemaphoreGive(stateMutex);
        } else {
            vTaskDelay(50 / portTICK_PERIOD_MS);
            continue;
        }
        
        // Cập nhật điểm số mỗi 500ms nếu không ở trạng thái Game Over
        if (!gameOverScreenActive && !gameOver && now - lastScoreUpdate > 500) {
            if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                drawScoreArea();
                xSemaphoreGive(tftMutex);
            }
        }
        
        // Xử lý trạng thái Game Over
        if (gameOver) {
            if (!gameOverDrawn) {
                unsigned long survivalTime = now - gameStartTime; // Tính thời gian chơi
                if (survivalTime > longestSurvivalTime) {
                    longestSurvivalTime = survivalTime; // Cập nhật thời gian sống lâu nhất
                }
                
                // Tạm dừng các task khác để hiển thị Game Over
                vTaskSuspend(gameTaskHandle);
                vTaskSuspend(inputTaskHandle);
                
                xQueueReset(snakeUpdateQueue); // Xóa hàng đợi
                
                // Gửi thông báo Game Over
                UpdateData updateData = {
                    .type = UPDATE_GAME_OVER,
                    .score = score,
                    .gameOver = true
                };
                
                if (xQueueSend(snakeUpdateQueue, &updateData, pdMS_TO_TICKS(10)) == pdPASS) {
                    Serial.println("Sent UPDATE_GAME_OVER to snakeUpdateQueue");
                    gameOverDrawn = true;
                } else {
                    displayGameOver(); // Hiển thị trực tiếp nếu không gửi được
                    gameOverDrawn = true;
                }
                
                // Khôi phục task
                vTaskResume(inputTaskHandle);
                vTaskResume(gameTaskHandle);
            }
            
            // Kiểm tra nút RETURN để quay lại menu
            if (checkButton(BTN_RETURN)) {
                if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    gameOverScreenActive = false;
                    currentState = MENU; // Chuyển về menu
                    menuNeedsRedraw = true;
                    firstRun = true;
                    gameOver = false;
                    gameOverDrawn = false;
                    xQueueReset(snakeUpdateQueue);
                    xSemaphoreGive(stateMutex);
                    Serial.println("Returning to menu, queue reset");
                }
            }
            vTaskDelay(200 / portTICK_PERIOD_MS); // Chờ 200ms
            continue;
        }
        
        // Bỏ qua logic nếu màn hình Game Over đang hiển thị
        if (gameOverScreenActive) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }
        
        // Kiểm tra thời gian tồn tại của thức ăn
        if (now - lastFoodCheck >= 10) {
            lastFoodCheck = now;
            
            // Nếu thức ăn hết thời gian tồn tại (không áp dụng cho EASY)
            if (currentDifficulty != EASY && foodSpawnTime > 0 && now - foodSpawnTime >= foodTimeout) {
                if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    oldFood = food; // Lưu vị trí cũ
                    
                    // Xóa thức ăn cũ
                    int pixel_x = food.x * CELL_SIZE;
                    int pixel_y = food.y * CELL_SIZE;
                    if (pixel_y >= SCORE_AREA_HEIGHT) {
                        display.fillRect(pixel_x - 10, pixel_y - 10, 20, 20, TFT_BLACK);
                    }
                    
                    // Tạo thức ăn mới
                    bool valid = false;
                    int attempts = 0;
                    const int maxAttempts = 50;
                    while (!valid && attempts < maxAttempts) {
                        food.x = (esp_random() % (NUM_COLS - 2)) + 1;
                        food.y = (SCORE_AREA_HEIGHT / CELL_SIZE) + 1 + (esp_random() % (NUM_ROWS - (SCORE_AREA_HEIGHT / CELL_SIZE) - 2));
                        
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
                        food.y = max((SCORE_AREA_HEIGHT / CELL_SIZE) + 1, NUM_ROWS - 2);
                    }
                    
                    foodType = (esp_random() % SPECIAL_FOOD_CHANCE == 0) ? SPECIAL : NORMAL;
                    foodTimeout = (foodType == SPECIAL) ? 2000 : 
                                  (currentDifficulty == MEDIUM) ? (5000 + (esp_random() % 3000)) : 
                                  (3000 + (esp_random() % 2000));
                    foodSpawnTime = now;
                    
                    // Vẽ thức ăn mới
                    pixel_x = food.x * CELL_SIZE;
                    pixel_y = food.y * CELL_SIZE;
                    uint16_t color = (foodType == SPECIAL) ? TFT_YELLOW : (esp_random() & 0xFFFF);
                    display.fillCircle(pixel_x, pixel_y, 3, color);
                    display.fillCircle(pixel_x + 2, pixel_y - 2, 2, color);
                    display.fillCircle(pixel_x - 2, pixel_y + 2, 2, color);
                    
                    drawBorder(); // Vẽ lại viền
                    drawScoreArea(); // Vẽ lại điểm số
                    
                    xSemaphoreGive(tftMutex);
                    Serial.println("Food spawned at (" + String(food.x) + ", " + String(food.y) + ")");
                    
                    needClearOldFood = false; // Không cần xóa thêm
                    foodNeedsRedraw = false; // Không cần vẽ lại
                }
            }
        }
        
        // Xử lý tăng tốc (boost)
        bool boost = false;
        const unsigned long boostDelay = 100; // Độ trễ tối thiểu giữa các lần boost
        if (now - lastBoostTime >= boostDelay) {
            if (checkButton(BTN_RIGHT) && currentDir == RIGHT) {
                boost = true;
                lastBoostTime = now;
                Serial.println("Boost RIGHT");
            } else if (checkButton(BTN_LEFT) && currentDir == LEFT) {
                boost = true;
                lastBoostTime = now;
                Serial.println("Boost LEFT");
            } else if (checkButton(BTN_UP) && currentDir == UP) {
                boost = true;
                lastBoostTime = now;
                Serial.println("Boost UP");
            } else if (checkButton(BTN_DOWN) && currentDir == DOWN) {
                boost = true;
                lastBoostTime = now;
                Serial.println("Boost DOWN");
            }
        }
        // Xử lý thay đổi hướng
        if (checkButton(BTN_UP) && currentDir != DOWN) {
            currentDir = UP;
            Serial.println("Direction changed to UP");
        } else if (checkButton(BTN_DOWN) && currentDir != UP) {
            currentDir = DOWN;
            Serial.println("Direction changed to DOWN");
        } else if (checkButton(BTN_LEFT) && currentDir != RIGHT) {
            currentDir = LEFT;
            Serial.println("Direction changed to LEFT");
        } else if (checkButton(BTN_RIGHT) && currentDir != LEFT) {
            currentDir = RIGHT;
            Serial.println("Direction changed to RIGHT");
        }
        // Xử lý nút RETURN để quay lại menu
        if (checkButton(BTN_RETURN)) {
            unsigned long survivalTime = now - gameStartTime;
            if (survivalTime > longestSurvivalTime) {
                longestSurvivalTime = survivalTime;
            }
            if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                gameOverScreenActive = false;
                currentState = MENU;
                menuNeedsRedraw = true;
                firstRun = true;
                xQueueReset(snakeUpdateQueue);
                xSemaphoreGive(stateMutex);
                Serial.println("Returning to menu, queue reset");
            }
            continue;
        }
        bool moved = false;
        // Di chuyển rắn nếu boost hoặc đủ thời gian
        if (boost || (now >= lastMoveTime + moveDelay)) {
            lastMoveTime = now;
            moved = true;
            Point tail = snake[snakeLength - 1]; // Lưu đuôi rắn
            // Dịch chuyển các đoạn thân
            for (int i = snakeLength - 1; i > 0; i--) {
                snake[i] = snake[i - 1];
            }
            // Cập nhật vị trí đầu rắn
            switch (currentDir) {
                case UP:    snake[0].y--; break;
                case DOWN:  snake[0].y++; break;
                case LEFT:  snake[0].x--; break;
                case RIGHT: snake[0].x++; break;
            }
            Serial.printf("Snake moved to (%d, %d)\n", snake[0].x, snake[0].y);
            // Kiểm tra va chạm
            if (checkCollision(snake[0])) {
                gameOver = true;
                Serial.println("Game Over triggered due to collision");
                continue;
            }
            bool foodEaten = false;
            // Xử lý ăn thức ăn
            if (snake[0].x == food.x && snake[0].y == food.y) {
                // Thực hiện tất cả trong một lần lấy mutex để tránh trễ
                if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    // Tăng độ dài rắn
                    if (snakeLength < SNAKE_MAX_LENGTH) {
                        snake[snakeLength] = tail;
                        snakeLength++;
                        Serial.printf("Snake length increased to %d\n", snakeLength);
                    } else {
                        Serial.println("Warning: Snake length reached SNAKE_MAX_LENGTH!");
                    }

                    // Cập nhật điểm số
                    score += (foodType == SPECIAL) ? 20 : 10;
                    // Tăng tốc độ ở mức trung bình
                    if (currentDifficulty == MEDIUM && score % 10 == 0) {
                        moveDelay = max(moveDelay - 10, 100);
                    }
                    
                    // Xóa thức ăn đã ăn
                    int pixel_x = food.x * CELL_SIZE;
                    int pixel_y = food.y * CELL_SIZE;
                    display.fillRect(pixel_x - 10, pixel_y - 10, 20, 20, TFT_BLACK);
                    
                    // Tạo thức ăn mới
                    oldFood = food;
                    bool valid = false;
                    int attempts = 0;
                    const int maxAttempts = 50;
                    while (!valid && attempts < maxAttempts) {
                        food.x = (esp_random() % (NUM_COLS - 2)) + 1;
                        food.y = (SCORE_AREA_HEIGHT / CELL_SIZE) + 1 + (esp_random() % (NUM_ROWS - (SCORE_AREA_HEIGHT / CELL_SIZE) - 2));
                        
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
                        food.y = max((SCORE_AREA_HEIGHT / CELL_SIZE) + 1, NUM_ROWS - 2);
                    }
                    
                    foodType = (esp_random() % SPECIAL_FOOD_CHANCE == 0) ? SPECIAL : NORMAL;
                    foodTimeout = (foodType == SPECIAL) ? 2000 : 
                                (currentDifficulty == MEDIUM) ? (5000 + (esp_random() % 3000)) : 
                                (3000 + (esp_random() % 2000));
                    foodSpawnTime = now;
                    
                    // Vẽ thức ăn mới
                    pixel_x = food.x * CELL_SIZE;
                    pixel_y = food.y * CELL_SIZE;
                    uint16_t color = (foodType == SPECIAL) ? TFT_YELLOW : (esp_random() & 0xFFFF);
                    display.fillCircle(pixel_x, pixel_y, 3, color);
                    display.fillCircle(pixel_x + 2, pixel_y - 2, 2, color);
                    display.fillCircle(pixel_x - 2, pixel_y + 2, 2, color);
                    
                    drawScoreArea(); // Vẽ lại điểm số
                    drawBorder(); // Vẽ lại viền
                    
                    xSemaphoreGive(tftMutex);
                    Serial.println("Food eaten and new food spawned");
                    
                    needClearOldFood = false;
                    foodNeedsRedraw = false;
                    foodEaten = true;
                } else {
                    // Phương pháp dự phòng nếu không lấy được mutex
                    if (snakeLength < SNAKE_MAX_LENGTH) {
                        snake[snakeLength] = tail;
                        snakeLength++;
                    }
                    score += (foodType == SPECIAL) ? 20 : 10;
                    clearFoodArea(food.x, food.y);
                    spawnFood();
                    foodEaten = true;
                }
            }

            updateObstacles(); // Cập nhật chướng ngại vật
            // Gửi cập nhật trạng thái đến snakeRenderTask
            UpdateData updateData = {
                .type = UPDATE_SNAKE,
                .snakeLength = snakeLength,
                .food = food,
                .foodType = foodType,
                .obstacleCount = obstacleCount,
                .score = score,
                .gameOver = false
            };
            if (xQueueSend(snakeUpdateQueue, &updateData, pdMS_TO_TICKS(10)) == pdPASS) {
                Serial.println("Sent UPDATE_SNAKE to snakeUpdateQueue");
            }
        }
        vTaskDelay(20 / portTICK_PERIOD_MS); // Chờ 20ms
    }
}

// Task hiển thị đồ họa
void snakeRenderTask(void *parameter) {
    while (1) {
        UpdateData updateData;
        // Nhận cập nhật từ hàng đợi
        if (xQueueReceive(snakeUpdateQueue, &updateData, pdMS_TO_TICKS(10)) == pdPASS) {
            Serial.printf("Received update type: %d, snakeLength: %d\n", updateData.type, updateData.snakeLength);
            // Lấy mutex để truy cập màn hình
            if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                Serial.println("tftMutex taken by snakeRenderTask");
                unsigned long now = millis();
                switch (updateData.type) {
                    case UPDATE_SNAKE:
                        // Không cập nhật nếu màn hình Game Over đang hiển thị
                        if (gameOverScreenActive) {
                            xSemaphoreGive(tftMutex);
                            continue;
                        }
                        
                        if (!hasDrawnBackground) {
                            drawBackground(); // Vẽ nền lần đầu
                            hasDrawnBackground = true;
                        }
                        
                        // Làm mới định kỳ mỗi 1000ms
                        if (now - lastFullRefresh >= 1000) {
                            lastFullRefresh = now;
                            // Xóa phần giữa màn hình, giữ viền và vùng điểm số
                            display.fillRect(2, SCORE_AREA_HEIGHT + 1, SCREEN_WIDTH-4, SCREEN_HEIGHT-SCORE_AREA_HEIGHT-3, TFT_BLACK);
                            
                            drawFood(food.x, food.y); // Vẽ lại thức ăn
                            
                            // Vẽ lại chướng ngại vật
                            for (int i = 0; i < obstacleCount; i++) {
                                for (int j = 0; j < obstacles[i].length; j++) {
                                    int pixel_x = obstacles[i].points[j].x * CELL_SIZE;
                                    int pixel_y = obstacles[i].points[j].y * CELL_SIZE;
                                    if (pixel_y >= SCORE_AREA_HEIGHT) {
                                        display.fillRect(pixel_x - 5, pixel_y - 5, 10, 10, TFT_RED);
                                    }
                                }
                            }
                            
                            drawBorder(); // Vẽ lại viền
                            drawScoreArea(); // Vẽ lại điểm số
                            
                            Serial.println("Full refresh performed");
                        }
                        // Kiểm tra độ dài rắn
                        if (updateData.snakeLength > SNAKE_MAX_LENGTH) {
                            Serial.println("ERROR: snakeLength exceeds SNAKE_MAX_LENGTH!");
                            updateData.snakeLength = SNAKE_MAX_LENGTH;
                        }
                        // Xóa rắn cũ
                        for (int i = 0; i < updateData.snakeLength; i++) {
                            eraseCell(snake[i].x, snake[i].y);
                        }
                        // Vẽ rắn mới
                        for (int i = 0; i < updateData.snakeLength; i++) {
                            drawSnake(snake[i].x, snake[i].y, i == 0);
                        }
                        // Vẽ lại thức ăn nếu cần
                        if (foodNeedsRedraw) {
                            clearFoodArea(updateData.food.x, updateData.food.y);
                            drawFood(updateData.food.x, updateData.food.y);
                            foodNeedsRedraw = false;
                        }
                        // Vẽ lại chướng ngại vật
                        if (updateData.obstacleCount > 0) {
                            for (int i = 0; i < updateData.obstacleCount; i++) {
                                for (int j = 0; j < obstacles[i].length; j++) {
                                    int pixel_x = obstacles[i].points[j].x * CELL_SIZE;
                                    int pixel_y = obstacles[i].points[j].y * CELL_SIZE;
                                    if (pixel_y >= SCORE_AREA_HEIGHT) {
                                        display.fillRect(pixel_x - 5, pixel_y - 5, 10, 10, TFT_RED);
                                    }
                                }
                            }
                        }
                        
                        drawBorder(); // Vẽ lại viền
                        drawScoreArea(); // Vẽ lại điểm số
                        break;
                    case UPDATE_FOOD:
                        if (gameOverScreenActive) {
                            xSemaphoreGive(tftMutex);
                            continue;
                        }
                        
                        // Xóa thức ăn cũ nếu cần
                        if (needClearOldFood) {
                            clearFoodArea(oldFood.x, oldFood.y);
                            needClearOldFood = false;
                        }
                        
                        // Xóa và vẽ thức ăn mới
                        clearFoodArea(updateData.food.x, updateData.food.y);
                        drawFood(updateData.food.x, updateData.food.y);
                        foodNeedsRedraw = false;
                        
                        drawBorder();
                        drawScoreArea();
                        break;
                    case UPDATE_OBSTACLES:
                        if (gameOverScreenActive) {
                            xSemaphoreGive(tftMutex);
                            continue;
                        }
                        
                        // Xóa chướng ngại vật cũ
                        for (int i = 0; i < updateData.obstacleCount; i++) {
                            for (int j = 0; j < obstacles[i].length; j++) {
                                eraseCell(obstacles[i].points[j].x, obstacles[i].points[j].y);
                            }
                        }
                        drawObstacles(); // Vẽ lại chướng ngại vật
                        
                        drawBorder();
                        drawScoreArea();
                        break;
                    case UPDATE_SCORE:
                        if (gameOverScreenActive) {
                            xSemaphoreGive(tftMutex);
                            continue;
                        }
                        
                        drawScoreArea(); // Cập nhật điểm số
                        break;
                    case UPDATE_GAME_OVER:
                        Serial.println("Processing UPDATE_GAME_OVER in snakeRenderTask");
                        displayGameOver(); // Hiển thị màn hình Game Over
                        break;
                }
                xSemaphoreGive(tftMutex); // Giải phóng mutex
                Serial.println("tftMutex released by snakeRenderTask");
            } else {
                Serial.println("ERROR: Failed to take tftMutex in snakeRenderTask!");
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); // Chờ 10ms
    }
}