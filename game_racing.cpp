#include "game_racing.h" // Bao gồm tệp tiêu đề của game Racing
#include "game_state.h"  // Bao gồm tệp tiêu đề chứa trạng thái game

// Định nghĩa các biến toàn cục cho game đua xe
int car_lane = 2;                     // Lane ban đầu của xe người chơi (0-3, bắt đầu ở lane 2)
int car_x = SCREEN_WIDTH / 2;         // Tọa độ x ban đầu của xe (sẽ tính lại dựa trên lane)
int car_y = SCREEN_HEIGHT - 40;       // Tọa độ y của xe, cách đáy màn hình 40px
int enemy_x[ENEMY_COUNT];             // Mảng tọa độ x của các xe đối thủ
int enemy_y[ENEMY_COUNT];             // Mảng tọa độ y của các xe đối thủ
static int racingScore = 0;           // Điểm số hiện tại của game Racing
int maxRacing = 0;                    // Điểm cao nhất của game Racing
const int lane_width = SCREEN_WIDTH / LANE_COUNT; // Chiều rộng mỗi lane (240 / 4 = 60px)
const int lane_centers[LANE_COUNT] = {30, 90, 150, 210}; // Tọa độ trung tâm của 4 lane

bool hasDrawBG = false;               // Cờ kiểm tra xem nền đã được vẽ chưa
bool needDelCar = false;              // Cờ kiểm tra xem xe cần xóa trước khi vẽ lại không
bool gameOverFlag = false;            // Cờ báo hiệu game over
bool exitGameFlag = false;            // Cờ báo hiệu thoát game

// Hàm vẽ nền đường đua
void drawBackgroundRacing() {
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) { // Đợi semaphore để truy cập màn hình
        display.fillScreen(ROAD_GRAY); // Đổ màu xám cho toàn màn hình (đường)
        // Vẽ 3 vạch kẻ đường đứt nét cho 4 làn
        for (int i = 1; i < LANE_COUNT; i++) { // i = 1, 2, 3 -> vạch tại 60, 120, 180
            int line_x = i * lane_width; // Tọa độ x của vạch
            for (int y = 0; y < SCREEN_HEIGHT; y += 20) { // Vẽ các đoạn đứt nét
                display.drawFastVLine(line_x, y, 10, TFT_WHITE); // Vạch cao 10px, màu trắng
            }
        }
        Serial.println("Drawing lanes at 60, 120, 180"); // Log thông báo
        xSemaphoreGive(tftMutex); // Giải phóng semaphore
    }
}

void drawCar(int x, int y, uint16_t color) {
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        // === Thân xe(Vẽ một hình chữ nhật có bo góc và đổ màu)  ===
        //fillRoundRect(x, y, width, height, radius, color);

        int body_width = 16;
        int body_height = 40;
        display.fillRoundRect(x - body_width / 2, y - body_height / 2, body_width, body_height, 3, color);

        // === Cabin (khoang lái) ===
        display.fillEllipse(x, y - 5, 5, 8, TFT_BLACK); // hình elip đen ở gần đầu xe

        // === Mũi xe nhọn (tam giác nhỏ phía trước) === fillTriangle(x1, y1, x2, y2, x3, y3, color);

        display.fillTriangle(x - 4, y - body_height / 2,x + 4, y - body_height / 2,x,y - body_height / 2 - 6,color);

        // === Cánh gió trước (ngang đầu xe) === fillRect(x, y, width, height, color);
        display.fillRect(x - 12, y - body_height / 2 - 2, 24, 2, TFT_DARKGREY);

        // === Cánh gió sau (ngang đuôi xe) ===
        display.fillRect(x - 14, y + body_height / 2, 28, 3, TFT_DARKGREY);

        // === Bánh xe ===
        int wheel_radius = 4; //bán kính bánh xe

        // Bánh trước trái
        display.fillCircle(x - 10, y - body_height / 2 + 6, wheel_radius, TFT_BLACK);
        // Bánh trước phải
        display.fillCircle(x + 10, y - body_height / 2 + 6, wheel_radius, TFT_BLACK);
        // Bánh sau trái
        display.fillCircle(x - 10, y + body_height / 2 - 6, wheel_radius, TFT_BLACK);
        // Bánh sau phải
        display.fillCircle(x + 10, y + body_height / 2 - 6, wheel_radius, TFT_BLACK);

        xSemaphoreGive(tftMutex);
    }
}

void delCar(int x, int y) {
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        // Kích thước xe F1 mở rộng hơn thân xe đơn giản
        int clear_width = 36;  // Bao phủ cả thân, cánh gió và bánh
        int clear_height = 52; // Bao phủ cả đầu xe nhọn và cánh sau
        //Fill toàn bộ xe mầu nền để xóa xe
        display.fillRect(
            x - clear_width / 2,
            y - clear_height / 2,
            clear_width,
            clear_height,
            ROAD_GRAY // Màu nền đường
        );
        xSemaphoreGive(tftMutex);
    }
}

// Hàm khởi tạo game Racing
void initGameRacing() {
    car_lane = 2;                // Xe người chơi bắt đầu ở lane 2 (giữa)
    car_x = lane_centers[car_lane]; // Tọa độ x ban đầu (150)
    car_y = SCREEN_HEIGHT - 40;  // Tọa độ y cố định, cách đáy 40px
    racingScore = 0;             // Đặt lại điểm số
    for (int i = 0; i < ENEMY_COUNT; i++) { // Khởi tạo xe đối thủ
        enemy_x[i] = lane_centers[random(0, LANE_COUNT)]; // Ngẫu nhiên một lane
        enemy_y[i] = -CAR_HEIGHT - i * (SCREEN_HEIGHT / ENEMY_COUNT); // Xuất hiện từ trên xuống
    }
    hasDrawBG = false;           // Đánh dấu cần vẽ lại nền
    needDelCar = false;          // Không cần xóa xe ngay
    gameOverFlag = false;        // Reset trạng thái game over
    exitGameFlag = false;        // Reset trạng thái thoát game
}

// Hàm chạy logic chính của game Racing
bool runGameRacing() {
    if (firstRun) {              // Nếu là lần chạy đầu tiên
        initGameRacing();        // Khởi tạo game
        firstRun = false;        // Đánh dấu đã chạy lần đầu
    }

    if (gameOverFlag) {          // Nếu game over
        if (checkButton(BTN_RETURN)) { // Kiểm tra nút RETURN
            exitGameFlag = true; // Đánh dấu thoát game
        }
        return !exitGameFlag;    // Trả về false khi thoát
    }

    if (!hasDrawBG) {            // Nếu chưa vẽ nền
        drawBackgroundRacing();  // Vẽ nền
        hasDrawBG = true;        // Đánh dấu đã vẽ
    }

    // Điều khiển xe người chơi
    if (checkButton(BTN_LEFT) && car_lane > 0) { // Nút LEFT: sang trái
        delCar(car_x, car_y);    // Xóa vị trí cũ
        car_lane--;              // Giảm lane
        car_x = lane_centers[car_lane]; // Cập nhật tọa độ x
        needDelCar = false;      // Không cần xóa lại
        Serial.print("Car moved to lane: "); Serial.println(car_lane); // Log
    }
    if (checkButton(BTN_RIGHT) && car_lane < LANE_COUNT - 1) { // Nút RIGHT: sang phải
        delCar(car_x, car_y);    // Xóa vị trí cũ
        car_lane++;              // Tăng lane
        car_x = lane_centers[car_lane]; // Cập nhật tọa độ x
        needDelCar = false;      // Không cần xóa lại
        Serial.print("Car moved to lane: "); Serial.println(car_lane); // Log
    }

    // Cập nhật xe đối thủ
    for (int i = 0; i < ENEMY_COUNT; i++) {
        delCar(enemy_x[i], enemy_y[i]); // Xóa vị trí cũ
        enemy_y[i] += ENEMY_SPEED;      // Di chuyển xuống dưới
        if (enemy_y[i] > SCREEN_HEIGHT + CAR_HEIGHT) { // Nếu ra khỏi màn hình
            enemy_x[i] = lane_centers[random(0, LANE_COUNT)]; // Ngẫu nhiên lane mới
            enemy_y[i] = -CAR_HEIGHT;     // Đặt lại phía trên màn hình
            racingScore++;                // Tăng điểm
            if (racingScore > maxRacing) maxRacing = racingScore; // Cập nhật điểm cao nhất
        }
        drawCar(enemy_x[i], enemy_y[i], ENEMY_YELLOW); // Vẽ xe đối thủ màu vàng
    }

    // Vẽ xe người chơi
    if (needDelCar) {                // Nếu cần xóa vị trí cũ
        delCar(car_x, car_y);        // Xóa xe
    }
    drawCar(car_x, car_y, CAR_RED);  // Vẽ xe màu đỏ
    needDelCar = true;               // Đánh dấu cần xóa lần sau

    // Kiểm tra va chạm
    for (int i = 0; i < ENEMY_COUNT; i++) {
        if (car_x == enemy_x[i] && abs(car_y - enemy_y[i]) <= CAR_HEIGHT) { // Nếu cùng lane và gần nhau
            gameOverRacing();        // Gọi hàm Game Over
            return true;             // Trả về true để tiếp tục vòng lặp
        }
    }

    // Hiển thị điểm số
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) { // Đợi semaphore
        display.setTextColor(TFT_WHITE, ROAD_GRAY); // Màu chữ trắng, nền xám
        display.setTextSize(2);            // Kích thước chữ lớn
        display.setCursor(10, 10);         // Đặt con trỏ góc trên trái
        display.print("Score: ");
        display.print(racingScore);
        xSemaphoreGive(tftMutex);          // Giải phóng semaphore
    }

    vTaskDelay(30 / portTICK_PERIOD_MS); // Trễ 30ms để giảm tải CPU
    return true;                   // Trả về true để tiếp tục game
}

// Hàm xử lý khi game over
void gameOverRacing() {
    delCar(car_x, car_y);          // Xóa xe người chơi
    // Xóa tất cả xe đối thủ
    for (int i = 0; i < ENEMY_COUNT; i++) {
        delCar(enemy_x[i], enemy_y[i]);
    }

    if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(500)) == pdTRUE) { // Đợi semaphore, tối đa 500ms
        display.fillScreen(TFT_BLACK); // Đổ nền đen

        // Vẽ "Game Over!" căn giữa
        display.setTextColor(TFT_RED); // Màu đỏ
        display.setTextSize(2);        // Kích thước chữ lớn
        const char* gameOverText = "Game Over!";
        int textWidth = strlen(gameOverText) * 12; // Ước tính chiều rộng
        int x = (SCREEN_WIDTH - textWidth) / 2; // Căn giữa ngang
        int y = 100;                   // Vị trí y
        display.setCursor(x, y);
        display.print(gameOverText);

        // Vẽ "Score: <score>" căn giữa
        char scoreText[20];
        sprintf(scoreText, "Score: %d", racingScore);
        textWidth = strlen(scoreText) * 12;
        x = (SCREEN_WIDTH - textWidth) / 2;
        y = 140; // Cách "Game Over!" 40px
        display.setCursor(x, y);
        display.print(scoreText);

        // Cập nhật và vẽ "High Score: <maxRacing>" căn giữa
        if (racingScore > maxRacing) maxRacing = racingScore; // Cập nhật điểm cao nhất
        char highScoreText[20];
        sprintf(highScoreText, "High Score: %d", maxRacing);
        textWidth = strlen(highScoreText) * 12;
        x = (SCREEN_WIDTH - textWidth) / 2;
        y = 180; // Cách "Score" 40px
        display.setCursor(x, y);
        display.print(highScoreText);

        xSemaphoreGive(tftMutex); // Giải phóng semaphore
        Serial.println("Game Over displayed");
    } else {
        Serial.println("ERROR: Failed to take tftMutex in gameOverRacing!"); // Báo lỗi
    }

    gameOverFlag = true; // Đánh dấu game over
}