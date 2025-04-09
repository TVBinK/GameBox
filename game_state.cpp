#include "game_state.h"  // Bao gồm tệp tiêu đề chứa định nghĩa trạng thái game (GameState)
#include "game_config.h" // Bao gồm tệp tiêu đề chứa các cấu hình game (ví dụ: NUM_GAMES)

extern TFT_eSPI display;         // Khai báo biến toàn cục display kiểu TFT_eSPI để vẽ lên màn hình
extern SemaphoreHandle_t tftMutex; // Khai báo semaphore để đồng bộ hóa truy cập màn hình giữa các tác vụ

// Khởi tạo biến toàn cục
GameState currentState = MENU; // Trạng thái hiện tại của game, bắt đầu từ MENU
int selectedGame = 0;          // Chỉ số của trò chơi được chọn trong menu (0 là Snake, 1 là Racing)
bool menuNeedsRedraw = true;   // Cờ để kiểm tra xem menu có cần vẽ lại hay không
bool firstRun = true;          // Cờ để kiểm tra lần chạy đầu tiên
bool gameOver = false;         // Cờ để kiểm tra trạng thái game over

// Hàm vẽ giao diện menu lên màn hình
void drawMenu() {
    // Đợi semaphore để đảm bảo chỉ một tác vụ truy cập màn hình tại một thời điểm
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        // Vẽ nền với màu gradient từ xanh dương nhạt đến đen
        for (int i = 0; i < display.height(); i++) {
            // Tạo màu gradient: đỏ = 0, xanh tăng từ 0->127, lam giảm từ 255->0
            int color = display.color565(0, i / 2, 255 - i / 2);
            // Vẽ đường ngang toàn màn hình với màu gradient
            display.drawFastHLine(0, i, display.width(), color);
        }

        // Vẽ tiêu đề "GAME" căn giữa màn hình
        const char* title = "GAME"; // Chuỗi tiêu đề
        // Tính chiều rộng tiêu đề: mỗi ký tự 6px * kích thước chữ 3 = 18px
        int titleWidth = strlen(title) * 18;
        // Tính vị trí X để căn giữa tiêu đề
        int titleX = (display.width() - titleWidth) / 2;
        display.setTextColor(TFT_WHITE); // Đặt màu chữ trắng
        display.setTextSize(3);          // Đặt kích thước chữ là 3 (to hơn bình thường)
        display.setCursor(titleX, 20);   // Đặt con trỏ tại vị trí căn giữa, cách đỉnh 20px
        display.println(title);          // In tiêu đề

        // Danh sách các trò chơi trong menu
        const char* games[] = {
            "Snake",  // Trò chơi 1
            "Racing", // Trò chơi 2
        };

        // Vẽ từng mục trong menu
        for (int i = 0; i < NUM_GAMES; ++i) {
            // Đặt vị trí con trỏ: cách lề trái 30px, cách đỉnh 100px + 50px mỗi mục
            display.setCursor(30, 100 + i * 50);

            // Kiểm tra xem mục này có phải là mục được chọn không
            if (selectedGame == i) {
                display.setTextSize(2);         // Đặt kích thước chữ là 2
                display.print("> ");            // In ký hiệu ">" để đánh dấu mục được chọn
                display.setTextColor(TFT_WHITE); // Đặt lại màu chữ trắng
                display.print(games[i]);        // In tên trò chơi
            } else {
                display.setTextColor(TFT_WHITE); // Đặt màu chữ trắng
                display.setTextSize(2);         // Đặt kích thước chữ là 2
                display.print("  ");            // In khoảng trắng để căn chỉnh với mục chọn
                display.println(games[i]);      // In tên trò chơi và xuống dòng
            }
        }

        // Giải phóng semaphore sau khi vẽ xong
        xSemaphoreGive(tftMutex);
    }
}

// Hàm xử lý logic menu
void handleMenu() {
    static unsigned long lastPressTime = 0; // Thời điểm nhấn nút cuối cùng (dùng để chống dội)
    const unsigned long debounceDelay = 200; // Độ trễ chống dội (200ms)

    // Nếu menu cần vẽ lại (khi khởi động hoặc thay đổi trạng thái)
    if (menuNeedsRedraw) {
        drawMenu();          // Gọi hàm vẽ menu
        menuNeedsRedraw = false; // Đặt lại cờ để không vẽ lại liên tục
    }

    // Tạm dừng 20ms để giảm tải CPU trong hệ thống nhúng
    vTaskDelay(20 / portTICK_PERIOD_MS);
}