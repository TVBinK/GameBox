#include <SPI.h>              // Thư viện giao tiếp SPI cho màn hình TFT
#include <TFT_eSPI.h>         // Thư viện điều khiển màn hình TFT_eSPI
#include "freertos/FreeRTOS.h" // Thư viện FreeRTOS cho hệ thống đa nhiệm
#include "freertos/task.h"    // Thư viện quản lý task trong FreeRTOS
#include "freertos/queue.h"   // Thư viện hàng đợi trong FreeRTOS
#include "freertos/semphr.h"  // Thư viện semaphore trong FreeRTOS
#include "game_config.h"      // Tệp cấu hình game (định nghĩa các hằng số như NUM_BUTTONS)
#include "game_state.h"       // Tệp định nghĩa trạng thái game (GameState)
#include "game_racing.h"      // Tệp chứa logic game Racing
#include "game_snake.h"       // Tệp chứa logic game Snake

// Khởi tạo đối tượng màn hình TFT
TFT_eSPI display = TFT_eSPI();

// Khai báo các handle cho task
TaskHandle_t inputTaskHandle = NULL; // Handle cho task xử lý nút bấm
TaskHandle_t gameTaskHandle = NULL;  // Handle cho task xử lý logic game

// Khai báo hàng đợi và semaphore
QueueHandle_t buttonQueue;           // Hàng đợi lưu trữ sự kiện nút bấm
SemaphoreHandle_t tftMutex;          // Semaphore bảo vệ truy cập màn hình TFT
SemaphoreHandle_t stateMutex;        // Semaphore bảo vệ biến trạng thái game
SemaphoreHandle_t buttonMutex;       // Semaphore bảo vệ trạng thái nút bấm

// Mảng lưu trạng thái nút bấm
bool buttonStates[NUM_BUTTONS] = {false};    // Trạng thái hiện tại của các nút
bool lastButtonStates[NUM_BUTTONS] = {false}; // Trạng thái trước đó của các nút

unsigned long lastActivityTime = 0; // Thời gian hoạt động cuối cùng (chưa dùng trong mã này)

// Khai báo biến toàn cục từ file khác
extern GameState currentState; // Trạng thái game hiện tại (MENU, GAME1, GAME2)
extern int selectedGame;       // Trò chơi được chọn trong menu
extern bool menuNeedsRedraw;   // Cờ để vẽ lại menu
extern bool firstRun;          // Cờ cho lần chạy đầu tiên

// Biến bổ sung
bool ignoreStuckButtons = false;      // Cờ để bỏ qua nút bị kẹt (chưa dùng)
bool menuActive = true;               // Cờ kiểm tra menu có đang hoạt động không
unsigned long lastStateChange = 0;    // Thời gian thay đổi trạng thái cuối cùng
const unsigned long STATE_CHANGE_DELAY = 1000; // Độ trễ tối thiểu giữa các thay đổi trạng thái (ms)

// Kiểm tra trạng thái nút bấm
bool checkButton(Button btn) {
    if (xSemaphoreTake(buttonMutex, portMAX_DELAY) == pdTRUE) { // Đợi semaphore để truy cập an toàn
        bool state = buttonStates[btn]; // Lấy trạng thái nút
        xSemaphoreGive(buttonMutex);    // Giải phóng semaphore
        return state;                   // Trả về trạng thái
    }
    return false; // Trả về false nếu không lấy được semaphore
}

// Xử lý sự kiện nhấn nút
void processButtonPress(Button btn) {
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) { // Đợi semaphore để thay đổi trạng thái
        switch (currentState) {
            case MENU: // Khi đang ở menu
                if (btn == BTN_UP) { // Nút UP: Chuyển lên mục trước
                    selectedGame = (selectedGame > 0) ? selectedGame - 1 : NUM_GAMES - 1;
                    menuNeedsRedraw = true; // Đánh dấu cần vẽ lại menu
                } else if (btn == BTN_DOWN) { // Nút DOWN: Chuyển xuống mục sau
                    selectedGame = (selectedGame + 1) % NUM_GAMES;
                    menuNeedsRedraw = true; // Đánh dấu cần vẽ lại menu
                } else if (btn == BTN_SELECT) { // Nút SELECT: Chọn trò chơi
                    switch (selectedGame) {
                        case 0: currentState = GAME1; break; // Snake
                        case 1: currentState = GAME2; break; // Racing
                    }
                    menuActive = false; // Tắt menu
                    firstRun = true;    // Đánh dấu lần chạy đầu tiên của game
                    vTaskDelay(200 / portTICK_PERIOD_MS); // Trễ để tránh nhấn liên tục
                }
                break;
            case GAME1: // Khi đang chơi Snake
            case GAME2: // Khi đang chơi Racing
                if (btn == BTN_RETURN) { // Nút RETURN: Quay lại menu
                    currentState = MENU;
                    menuNeedsRedraw = true;
                    menuActive = true;
                    vTaskDelay(200 / portTICK_PERIOD_MS); // Trễ để tránh nhấn liên tục
                }
                break;
        }
        xSemaphoreGive(stateMutex); // Giải phóng semaphore
    }
}

// Task xử lý đầu vào từ nút bấm
void inputTask(void *parameter) {
    // Khởi tạo trạng thái ban đầu của các nút
    for (int i = 0; i < NUM_BUTTONS; i++) {
        lastButtonStates[i] = digitalRead(getPinForButton(static_cast<Button>(i))) == LOW;
        buttonStates[i] = lastButtonStates[i];
    }

    while (1) { // Vòng lặp vô hạn
        if (xSemaphoreTake(buttonMutex, portMAX_DELAY) == pdTRUE) { // Đợi semaphore
            for (int i = 0; i < NUM_BUTTONS; i++) {
                bool current = digitalRead(getPinForButton(static_cast<Button>(i))) == LOW;

                // Phát hiện cạnh lên (từ không nhấn sang nhấn)
                if (current && !lastButtonStates[i]) {
                    vTaskDelay(30 / portTICK_PERIOD_MS); // Trễ 30ms để chống dội
                    if (digitalRead(getPinForButton(static_cast<Button>(i))) == LOW) { // Xác nhận lại
                        Button btn = static_cast<Button>(i);
                        xQueueSend(buttonQueue, &btn, 0); // Gửi sự kiện nút vào hàng đợi
                    }
                }

                lastButtonStates[i] = current; // Cập nhật trạng thái trước đó
                buttonStates[i] = current;     // Cập nhật trạng thái hiện tại
            }
            xSemaphoreGive(buttonMutex); // Giải phóng semaphore
        }
        vTaskDelay(20 / portTICK_PERIOD_MS); // Trễ 20ms để giảm tải CPU
    }
}

// Lấy chân GPIO tương ứng với nút
int getPinForButton(Button btn) {
    switch(btn) {
        case BTN_UP: return BTN_UP_PIN;       // Chân cho nút UP
        case BTN_DOWN: return BTN_DOWN_PIN;   // Chân cho nút DOWN
        case BTN_LEFT: return BTN_LEFT_PIN;   // Chân cho nút LEFT
        case BTN_RIGHT: return BTN_RIGHT_PIN; // Chân cho nút RIGHT
        case BTN_SELECT: return BTN_SELECT_PIN; // Chân cho nút SELECT
        case BTN_RETURN: return BTN_RETURN_PIN; // Chân cho nút RETURN
        default: return -1;                   // Trả về -1 nếu nút không hợp lệ
    }
}

// Task xử lý logic game
void gameLogicTask(void *parameter) {
    Button receivedButton; // Biến lưu nút nhận được từ hàng đợi

    while (1) { // Vòng lặp vô hạn
        if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) { // Đợi semaphore
            GameState currentGameState = currentState; // Lấy trạng thái hiện tại
            xSemaphoreGive(stateMutex); // Giải phóng semaphore

            if (currentGameState == MENU) { // Nếu đang ở menu
                handleMenu(); // Xử lý menu
            } else { // Nếu đang chơi game
                switch (currentGameState) {
                    case GAME1: runGameSnake(); break; // Chạy game Snake
                    case GAME2: runGameRacing(); break; // Chạy game Racing
                    default: break;
                }
            }
        }

        // Nhận sự kiện nút từ hàng đợi
        if (xQueueReceive(buttonQueue, &receivedButton, 0) == pdPASS) {
            processButtonPress(receivedButton); // Xử lý nút nhấn
        }

        vTaskDelay(50 / portTICK_PERIOD_MS); // Trễ 50ms để giảm tải CPU
    }
}

// Hàm khởi tạo hệ thống
void setup() {
    Serial.begin(115200); // Khởi tạo Serial để debug
    delay(1000);          // Trễ 1 giây để ổn định

    // Cấu hình các chân nút bấm với pull-up
    pinMode(BTN_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_LEFT_PIN, INPUT_PULLUP);
    pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);
    pinMode(BTN_SELECT_PIN, INPUT_PULLUP);
    pinMode(BTN_RETURN_PIN, INPUT_PULLUP);

    // Khởi tạo màn hình TFT
    display.init();           // Khởi động màn hình
    display.setRotation(0);   // Đặt hướng màn hình (0: dọc)
    display.fillScreen(TFT_BLACK); // Xóa màn hình thành màu đen

    // Khởi tạo đèn nền (nếu có)
    pinMode(PIN_BACKLIGHT, OUTPUT);
    digitalWrite(PIN_BACKLIGHT, HIGH); // Bật đèn nền

    // Tạo hàng đợi và semaphore
    buttonQueue = xQueueCreate(10, sizeof(Button)); // Hàng đợi cho 10 sự kiện nút
    tftMutex = xSemaphoreCreateMutex();    // Semaphore cho màn hình
    stateMutex = xSemaphoreCreateMutex();  // Semaphore cho trạng thái
    buttonMutex = xSemaphoreCreateMutex(); // Semaphore cho nút bấm

    // Kiểm tra lỗi khi tạo semaphore
    if (tftMutex == NULL) {
        Serial.println("ERROR: Failed to create tftMutex!");
        while (1); // Dừng chương trình nếu lỗi
    }

    // Thiết lập trạng thái ban đầu
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        currentState = MENU;    // Bắt đầu từ menu
        selectedGame = 0;       // Chọn trò chơi đầu tiên
        menuNeedsRedraw = true; // Đánh dấu cần vẽ menu
        firstRun = true;        // Đánh dấu lần chạy đầu tiên
        xSemaphoreGive(stateMutex);
    }

    // Tạo các task
    xTaskCreatePinnedToCore(inputTask, "Input Task", 4096, NULL, 3, &inputTaskHandle, 0); // Task xử lý nút, chạy trên lõi 0
    vTaskDelay(100 / portTICK_PERIOD_MS); // Trễ để task khởi động
    xTaskCreatePinnedToCore(gameLogicTask, "Game Logic Task", 8192, NULL, 1, &gameTaskHandle, 1); // Task logic game, chạy trên lõi 1
}

// Hàm vòng lặp chính (chỉ để debug)
void loop() {
    vTaskDelay(1000 / portTICK_PERIOD_MS); // Trễ 1 giây
    Serial.println(currentState); // In trạng thái hiện tại để debug
}