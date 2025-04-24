// Bao gồm các file tiêu đề cần thiết
#include "game_state.h"
#include "game_config.h"

// Khai báo đối tượng màn hình TFT và semaphore từ file khác
extern TFT_eSPI display;          // Đối tượng điều khiển màn hình TFT
extern SemaphoreHandle_t tftMutex;// Mutex bảo vệ truy cập màn hình TFT

// Khởi tạo các biến toàn cục cho trạng thái game
GameState currentState = MENU;    // Trạng thái ban đầu là MENU
int selectedGame = 0;             // Game được chọn (0: Snake, 1: Racing)
bool menuNeedsRedraw = true;      // Cờ báo cần vẽ lại menu chính
bool firstRun = true;             // Cờ báo lần chạy đầu tiên
bool gameOver = false;            // Cờ báo trạng thái Game Over
Difficulty selectedDifficulty = EASY; // Độ khó mặc định là EASY
bool difficultyNeedsRedraw = true;// Cờ báo cần vẽ lại menu độ khó
int maxSnake = 0;                 // Điểm cao nhất của game Snake
int maxRacing = 0;                // Điểm cao nhất của game Racing
int playCount = 0;                // Tổng số lần chơi (Snake + Racing)
unsigned long longestSurvivalTime = 0; // Thời gian sống lâu nhất (ms)
int highScoresEasy[5] = {0, 0, 0, 0, 0};   // Bảng điểm cao mức EASY (Snake)
int highScoresMedium[5] = {0, 0, 0, 0, 0}; // Bảng điểm cao mức MEDIUM (Snake)
int highScoresHard[5] = {0, 0, 0, 0, 0};   // Bảng điểm cao mức HARD (Snake)
int racingHighScoresEasy[5] = {0, 0, 0, 0, 0};   // Bảng điểm cao mức EASY (Racing)
int racingHighScoresMedium[5] = {0, 0, 0, 0, 0}; // Bảng điểm cao mức MEDIUM (Racing)
int racingHighScoresHard[5] = {0, 0, 0, 0, 0};   // Bảng điểm cao mức HARD (Racing)
int racingPlayCount = 0;          // Số lần chơi game Racing
unsigned long racingLongestSurvivalTime = 0; // Thời gian sống lâu nhất trong Racing
int racingCurrentRank = -1;       // Xếp hạng hiện tại trong Racing

// Hàm vẽ menu chọn độ khó
void drawDifficultyMenu() {
    // Lấy mutex để truy cập màn hình TFT
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        // Tạo hiệu ứng gradient cho nền
        for (int i = 0; i < display.height(); i++) {
            int color = display.color565(0, i / 2, 255 - i / 2); // Tạo màu gradient
            display.drawFastHLine(0, i, display.width(), color); // Vẽ dòng ngang
        }
        // Vẽ tiêu đề "DIFFICULTY"
        const char* title = "DIFFICULTY";
        int titleWidth = strlen(title) * 18; // Tính chiều rộng tiêu đề
        int titleX = (display.width() - titleWidth) / 2; // Căn giữa
        display.setTextColor(TFT_WHITE); // Màu chữ trắng
        display.setTextSize(3); // Kích thước chữ lớn
        display.setCursor(titleX, 20); // Đặt vị trí vẽ
        display.println(title); // In tiêu đề
        // Danh sách các mức độ khó
        const char* difficulties[] = {"Easy", "Medium", "Hard"};
        // Vẽ từng mức độ khó
        for (int i = 0; i < 3; ++i) {
            display.setCursor(30, 100 + i * 50); // Đặt vị trí cho mỗi dòng
            if (selectedDifficulty == i) { // Nếu là mức độ khó đang được chọn
                display.setTextSize(2);
                display.print(">> "); // Vẽ dấu mũi tên để đánh dấu
                display.setTextColor(TFT_WHITE);
                display.print(difficulties[i]); // In tên mức độ khó
            } else {
                display.setTextColor(TFT_WHITE);
                display.setTextSize(2);
                display.print("  "); // Để trống nếu không được chọn
                display.println(difficulties[i]); // In tên mức độ khó
            }
        }
        xSemaphoreGive(tftMutex); // Giải phóng mutex
    }
}

// Hàm vẽ menu chính
void drawMenu() {
    // Lấy mutex để truy cập màn hình TFT
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        // Tạo hiệu ứng gradient cho nền
        for (int i = 0; i < display.height(); i++) {
            int color = display.color565(0, i / 2, 255 - i / 2); // Tạo màu gradient
            display.drawFastHLine(0, i, display.width(), color); // Vẽ dòng ngang
        }
        // Vẽ tiêu đề "GAME"
        const char* title = "GAME";
        int titleWidth = strlen(title) * 18; // Tính chiều rộng tiêu đề
        int titleX = (display.width() - titleWidth) / 2; // Căn giữa
        display.setTextColor(TFT_WHITE); // Màu chữ trắng
        display.setTextSize(3); // Kích thước chữ lớn
        display.setCursor(titleX, 20); // Đặt vị trí vẽ
        display.println(title); // In tiêu đề
        // Danh sách các game
        const char* games[] = {"Snake", "Racing"};
        // Vẽ từng game
        for (int i = 0; i < NUM_GAMES; ++i) {
            display.setCursor(30, 100 + i * 50); // Đặt vị trí cho mỗi dòng
            if (selectedGame == i) { // Nếu là game đang được chọn
                display.setTextSize(2);
                display.print(">> "); // Vẽ dấu mũi tên để đánh dấu
                display.setTextColor(TFT_WHITE);
                display.print(games[i]); // In tên game
            } else {
                display.setTextColor(TFT_WHITE);
                display.setTextSize(2);
                display.print("  "); // Để trống nếu không được chọn
                display.println(games[i]); // In tên game
            }
        }
        xSemaphoreGive(tftMutex); // Giải phóng mutex
    }
}

// Hàm xử lý logic menu chính
void handleMenu() {
    static unsigned long lastPressTime = 0; // Thời gian nhấn nút cuối
    const unsigned long debounceDelay = 200;// Độ trễ chống nhiễu (ms)
    // Nếu cần vẽ lại menu
    if (menuNeedsRedraw) {
        drawMenu(); // Vẽ menu
        menuNeedsRedraw = false; // Đặt lại cờ
    }
    vTaskDelay(20 / portTICK_PERIOD_MS); // Chờ 20ms để giảm tải CPU
}

// Hàm xử lý logic menu chọn độ khó
void handleDifficultyMenu() {
    // Nếu cần vẽ lại menu độ khó
    if (difficultyNeedsRedraw) {
        drawDifficultyMenu(); // Vẽ menu độ khó
        difficultyNeedsRedraw = false; // Đặt lại cờ
    }
    vTaskDelay(20 / portTICK_PERIOD_MS); // Chờ 20ms để giảm tải CPU
}