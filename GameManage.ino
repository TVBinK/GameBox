// Bao gồm các thư viện cần thiết cho giao tiếp SPI, màn hình TFT, và hệ điều hành FreeRTOS
#include <SPI.h>
#include <TFT_eSPI.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "game_config.h"
#include "game_state.h"
#include "game_racing.h"
#include "game_snake.h"

// Khởi tạo đối tượng màn hình TFT
TFT_eSPI display = TFT_eSPI();

// Khai báo các handle cho các task (luồng xử lý) của FreeRTOS
TaskHandle_t inputTaskHandle = NULL;         // Task xử lý input từ nút bấm
TaskHandle_t gameTaskHandle = NULL;         // Task xử lý logic game chính
TaskHandle_t snakeUpdateTaskHandle = NULL;  // Task cập nhật logic game Snake
TaskHandle_t snakeRenderTaskHandle = NULL;  // Task vẽ giao diện game Snake
TaskHandle_t racingUpdateTaskHandle = NULL; // Task cập nhật logic game Racing
TaskHandle_t racingRenderTaskHandle = NULL; // Task vẽ giao diện game Racing

// Khai báo các hàng đợi (queue) để giao tiếp giữa các task
QueueHandle_t buttonQueue;        // Hàng đợi lưu sự kiện nhấn nút
QueueHandle_t snakeUpdateQueue;   // Hàng đợi cập nhật dữ liệu game Snake
QueueHandle_t racingUpdateQueue;  // Hàng đợi cập nhật dữ liệu game Racing

// Khai báo các semaphore (cơ chế khóa) để quản lý truy cập đồng thời
SemaphoreHandle_t tftMutex;       // Khóa bảo vệ truy cập màn hình TFT
SemaphoreHandle_t stateMutex;     // Khóa bảo vệ trạng thái game
SemaphoreHandle_t buttonMutex;    // Khóa bảo vệ trạng thái nút bấm

// Mảng lưu trạng thái hiện tại và trạng thái trước đó của các nút bấm
bool buttonStates[NUM_BUTTONS] = {false};    // Trạng thái hiện tại
bool lastButtonStates[NUM_BUTTONS] = {false}; // Trạng thái trước đó

// Các biến toàn cục được khai báo ở file khác
extern GameState currentState;    // Trạng thái hiện tại của game (MENU, GAME1, GAME2, DIFFICULTY)
extern int selectedGame;          // Game được chọn (0: Snake, 1: Racing)
extern bool menuNeedsRedraw;      // Cờ báo cần vẽ lại menu
extern bool firstRun;             // Cờ báo lần chạy đầu tiên
extern bool difficultyNeedsRedraw;// Cờ báo cần vẽ lại menu chọn độ khó

// Biến quản lý trạng thái menu và thời gian chuyển trạng thái
bool menuActive = true;                       // Menu đang hoạt động
unsigned long lastStateChange = 0;           // Thời gian thay đổi trạng thái cuối
const unsigned long STATE_CHANGE_DELAY = 1000;// Độ trễ tối thiểu giữa các thay đổi trạng thái (ms)

// Hàm kiểm tra trạng thái nút bấm với khóa bảo vệ
bool checkButton(Button btn) {
    // Lấy khóa buttonMutex với thời gian chờ tối đa 50ms
    if (xSemaphoreTake(buttonMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        bool state = buttonStates[btn]; // Lấy trạng thái nút
        xSemaphoreGive(buttonMutex);    // Giải phóng khóa
        return state;
    }
    return false; // Trả về false nếu không lấy được khóa
}

// Hàm xóa màn hình nhanh, đảm bảo không để lại hiện tượng nhấp nháy
void fastClearScreen() {
    // Thử lấy khóa tftMutex với thời gian chờ 200ms
    if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        display.fillScreen(TFT_BLACK); // Xóa màn hình bằng màu đen
        display.fillScreen(TFT_BLACK); // Xóa lần thứ hai để tránh hiện tượng nhấp nháy
        xSemaphoreGive(tftMutex);      // Giải phóng khóa
        Serial.println("Screen cleared rapidly for game transition");
    } else {
        // Nếu không lấy được khóa, thử khôi phục
        Serial.println("WARNING: Failed to acquire tftMutex for screen clear, attempting recovery");
        vTaskDelay(50 / portTICK_PERIOD_MS); // Chờ 50ms
        if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            display.fillScreen(TFT_BLACK); // Xóa màn hình
            display.fillScreen(TFT_BLACK);
            xSemaphoreGive(tftMutex);
            Serial.println("Emergency screen clear successful");
        } else {
            // Nếu vẫn thất bại, xóa và tạo lại khóa
            vSemaphoreDelete(tftMutex); // Xóa khóa cũ
            tftMutex = xSemaphoreCreateMutex(); // Tạo khóa mới
            if (tftMutex != NULL && xSemaphoreTake(tftMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                display.fillScreen(TFT_BLACK);
                xSemaphoreGive(tftMutex);
                Serial.println("Screen cleared after mutex reset");
            } else {
                Serial.println("CRITICAL: Failed to clear screen");
            }
        }
    }
}

// Hàm xử lý sự kiện nhấn nút
void processButtonPress(Button btn) {
    // Lấy khóa trạng thái game
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        switch (currentState) {
            case MENU: // Xử lý trong menu chính
                if (btn == BTN_UP) {
                    // Chuyển lên game trước đó
                    selectedGame = (selectedGame > 0) ? selectedGame - 1 : NUM_GAMES - 1;
                    menuNeedsRedraw = true; // Đánh dấu cần vẽ lại menu
                } else if (btn == BTN_DOWN) {
                    // Chuyển xuống game tiếp theo
                    selectedGame = (selectedGame + 1) % NUM_GAMES;
                    menuNeedsRedraw = true;
                } else if (btn == BTN_SELECT) {
                    // Chọn game và chuyển sang màn hình chọn độ khó
                    switch (selectedGame) {
                        case 0: // Snake
                            currentState = DIFFICULTY;
                            difficultyNeedsRedraw = true;
                            break;
                        case 1: // Racing
                            currentState = DIFFICULTY;
                            difficultyNeedsRedraw = true;
                            break;
                    }
                    vTaskDelay(50 / portTICK_PERIOD_MS); // Chờ 50ms để ổn định
                }
                break;
            case DIFFICULTY: // Xử lý trong menu chọn độ khó
                if (btn == BTN_UP) {
                    // Chuyển lên độ khó trước
                    selectedDifficulty = (selectedDifficulty > EASY) ? static_cast<Difficulty>(selectedDifficulty - 1) : HARD;
                    difficultyNeedsRedraw = true;
                } else if (btn == BTN_DOWN) {
                    // Chuyển xuống độ khó sau
                    selectedDifficulty = (selectedDifficulty < HARD) ? static_cast<Difficulty>(selectedDifficulty + 1) : EASY;
                    difficultyNeedsRedraw = true;
                } else if (btn == BTN_SELECT) {
                    // Tạm dừng tất cả task game để tránh xung đột khi chuyển game
                    if (snakeUpdateTaskHandle != NULL) vTaskSuspend(snakeUpdateTaskHandle);
                    if (snakeRenderTaskHandle != NULL) vTaskSuspend(snakeRenderTaskHandle);
                    if (racingUpdateTaskHandle != NULL) vTaskSuspend(racingUpdateTaskHandle);
                    if (racingRenderTaskHandle != NULL) vTaskSuspend(racingRenderTaskHandle);

                    // Xóa màn hình trước khi bắt đầu game mới
                    fastClearScreen();

                    if (selectedGame == 0) { // Khởi động game Snake
                        currentState = GAME1;
                        menuActive = false;
                        firstRun = true;
                        difficultyNeedsRedraw = false;

                        // Xóa task Racing nếu tồn tại
                        if (racingUpdateTaskHandle != NULL) {
                            vTaskDelete(racingUpdateTaskHandle);
                            racingUpdateTaskHandle = NULL;
                        }
                        if (racingRenderTaskHandle != NULL) {
                            vTaskDelete(racingRenderTaskHandle);
                            racingRenderTaskHandle = NULL;
                        }

                        // Đặt lại hàng đợi
                        if (snakeUpdateQueue != NULL) xQueueReset(snakeUpdateQueue);
                        if (racingUpdateQueue != NULL) xQueueReset(racingUpdateQueue);

                        // Tạo hoặc khôi phục task Snake
                        if (snakeUpdateTaskHandle == NULL) {
                            xTaskCreatePinnedToCore(snakeUpdateTask, "Snake Update Task", 8192, NULL, 2, &snakeUpdateTaskHandle, 1);
                        }
                        if (snakeRenderTaskHandle == NULL) {
                            xTaskCreatePinnedToCore(snakeRenderTask, "Snake Render Task", 12288, NULL, 4, &snakeRenderTaskHandle, 1);
                        }
                        vTaskResume(snakeUpdateTaskHandle);
                        vTaskResume(snakeRenderTaskHandle);
                        Serial.println("Snake game initialized with clean screen");
                    } else if (selectedGame == 1) { // Khởi động game Racing
                        currentState = GAME2;
                        menuActive = false;
                        firstRun = true;
                        difficultyNeedsRedraw = false;

                        // Xóa task Snake nếu tồn tại
                        if (snakeUpdateTaskHandle != NULL) {
                            vTaskDelete(snakeUpdateTaskHandle);
                            snakeUpdateTaskHandle = NULL;
                        }
                        if (snakeRenderTaskHandle != NULL) {
                            vTaskDelete(snakeRenderTaskHandle);
                            snakeRenderTaskHandle = NULL;
                        }

                        // Đặt lại hàng đợi
                        if (snakeUpdateQueue != NULL) xQueueReset(snakeUpdateQueue);
                        if (racingUpdateQueue != NULL) xQueueReset(racingUpdateQueue);

                        // Tạo hoặc khôi phục task Racing
                        if (racingUpdateTaskHandle == NULL) {
                            xTaskCreatePinnedToCore(racingUpdateTask, "Racing Update Task", 8192, NULL, 2, &racingUpdateTaskHandle, 1);
                        }
                        if (racingRenderTaskHandle == NULL) {
                            xTaskCreatePinnedToCore(racingRenderTask, "Racing Render Task", 12288, NULL, 4, &racingRenderTaskHandle, 1);
                        }
                        vTaskResume(racingUpdateTaskHandle);
                        vTaskResume(racingRenderTaskHandle);
                        Serial.println("Racing game initialized with clean screen");
                    }
                } else if (btn == BTN_RETURN) {
                    // Quay lại menu chính
                    currentState = MENU;
                    menuNeedsRedraw = true;
                    difficultyNeedsRedraw = false;
                    fastClearScreen();
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                }
                break;
            case GAME1: // Trong game Snake
            case GAME2: // Trong game Racing
                if (btn == BTN_RETURN) {
                    // Tạm dừng tất cả task game
                    if (snakeUpdateTaskHandle != NULL) vTaskSuspend(snakeUpdateTaskHandle);
                    if (snakeRenderTaskHandle != NULL) vTaskSuspend(snakeRenderTaskHandle);
                    if (racingUpdateTaskHandle != NULL) vTaskSuspend(racingUpdateTaskHandle);
                    if (racingRenderTaskHandle != NULL) vTaskSuspend(racingRenderTaskHandle);

                    // Xóa màn hình
                    fastClearScreen();

                    // Chuyển về menu
                    currentState = MENU;
                    menuNeedsRedraw = true;
                    menuActive = true;
                    difficultyNeedsRedraw = false;

                    // Xóa task tương ứng
                    if (currentState == GAME1) {
                        if (snakeUpdateTaskHandle != NULL) {
                            vTaskDelete(snakeUpdateTaskHandle);
                            snakeUpdateTaskHandle = NULL;
                        }
                        if (snakeRenderTaskHandle != NULL) {
                            vTaskDelete(snakeRenderTaskHandle);
                            snakeRenderTaskHandle = NULL;
                        }
                    } else if (currentState == GAME2) {
                        if (racingUpdateTaskHandle != NULL) {
                            vTaskDelete(racingUpdateTaskHandle);
                            racingUpdateTaskHandle = NULL;
                        }
                        if (racingRenderTaskHandle != NULL) {
                            vTaskDelete(racingRenderTaskHandle);
                            racingRenderTaskHandle = NULL;
                        }
                    }

                    // Đặt lại hàng đợi
                    if (snakeUpdateQueue != NULL) xQueueReset(snakeUpdateQueue);
                    if (racingUpdateQueue != NULL) xQueueReset(racingUpdateQueue);

                    Serial.println("Returned to menu with clean screen");
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                }
                break;
        }
        xSemaphoreGive(stateMutex); // Giải phóng khóa trạng thái
    }
}

// Task xử lý input từ nút bấm
void inputTask(void *parameter) {
    static unsigned long lastPressTime[NUM_BUTTONS] = {0}; // Thời gian nhấn nút cuối
    const unsigned long debounceDelay = 50;                // Độ trễ chống nhiễu (ms)

    // Khởi tạo trạng thái ban đầu của các nút
    for (int i = 0; i < NUM_BUTTONS; i++) {
        lastButtonStates[i] = digitalRead(getPinForButton(static_cast<Button>(i))) == LOW;
        buttonStates[i] = lastButtonStates[i];
    }
    while (1) {
        // Lấy khóa buttonMutex
        if (xSemaphoreTake(buttonMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            unsigned long currentTime = millis();
            for (int i = 0; i < NUM_BUTTONS; i++) {
                bool current = digitalRead(getPinForButton(static_cast<Button>(i))) == LOW;
                // Kiểm tra sự kiện nhấn nút (chuyển từ HIGH sang LOW)
                if (current && !lastButtonStates[i] && (currentTime - lastPressTime[i] >= debounceDelay)) {
                    vTaskDelay(debounceDelay / portTICK_PERIOD_MS); // Chờ để chống nhiễu
                    if (digitalRead(getPinForButton(static_cast<Button>(i))) == LOW) {
                        Button btn = static_cast<Button>(i);
                        xQueueSend(buttonQueue, &btn, 0); // Gửi sự kiện vào hàng đợi
                        lastPressTime[i] = currentTime;
                        Serial.print("Button pressed: "); Serial.println(i);
                    }
                }
                lastButtonStates[i] = current;
                buttonStates[i] = current;
            }
            xSemaphoreGive(buttonMutex); // Giải phóng khóa
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); // Chờ 10ms
    }
}

// Hàm lấy chân GPIO tương ứng với nút bấm
int getPinForButton(Button btn) {
    switch(btn) {
        case BTN_UP: return BTN_UP_PIN;
        case BTN_DOWN: return BTN_DOWN_PIN;
        case BTN_LEFT: return BTN_LEFT_PIN;
        case BTN_RIGHT: return BTN_RIGHT_PIN;
        case BTN_SELECT: return BTN_SELECT_PIN;
        case BTN_RETURN: return BTN_RETURN_PIN;
        default: return -1; // Trả về -1 nếu nút không hợp lệ
    }
}

// Task xử lý logic chính của game
void gameLogicTask(void *parameter) {
    Button receivedButton; // Biến lưu nút được nhận từ hàng đợi
    while (1) {
        // Kiểm tra trạng thái game
        if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            GameState currentGameState = currentState;
            xSemaphoreGive(stateMutex);
            // Xử lý theo trạng thái
            if (currentGameState == MENU) {
                handleMenu(); // Vẽ và xử lý menu
            } else if (currentGameState == DIFFICULTY) {
                handleDifficultyMenu(); // Vẽ và xử lý menu độ khó
            } else {
                switch (currentGameState) {
                    case GAME1: runGameSnake(); break; // Chạy game Snake
                    case GAME2: runGameRacing(); break; // Chạy game Racing
                    default: break;
                }
            }
        }
        // Nhận sự kiện nút bấm từ hàng đợi
        if (xQueueReceive(buttonQueue, &receivedButton, 0) == pdPASS) {
            processButtonPress(receivedButton); // Xử lý nút được nhấn
        }
        vTaskDelay(20 / portTICK_PERIOD_MS); // Chờ 20ms
    }
}

// Hàm khởi tạo chương trình
void setup() {
    Serial.begin(115200); // Khởi tạo giao tiếp Serial
    delay(500); // Chờ 500ms để ổn định
    // Cấu hình các chân nút bấm với chế độ INPUT_PULLUP
    pinMode(BTN_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_LEFT_PIN, INPUT_PULLUP);
    pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);
    pinMode(BTN_SELECT_PIN, INPUT_PULLUP);
    pinMode(BTN_RETURN_PIN, INPUT_PULLUP);
    display.init(); // Khởi tạo màn hình TFT
    display.setRotation(0); // Cài đặt hướng màn hình
    display.fillScreen(TFT_BLACK); // Xóa màn hình bằng màu đen
    pinMode(PIN_BACKLIGHT, OUTPUT); // Cấu hình chân đèn nền
    digitalWrite(PIN_BACKLIGHT, HIGH); // Bật đèn nền
    // Tạo các hàng đợi
    buttonQueue = xQueueCreate(10, sizeof(Button));
    snakeUpdateQueue = xQueueCreate(20, sizeof(UpdateData));
    racingUpdateQueue = xQueueCreate(20, sizeof(RacingUpdateData));
    Serial.println("snakeUpdateQueue created with size 20");
    Serial.println("racingUpdateQueue created with size 20");
    // Tạo các semaphore
    tftMutex = xSemaphoreCreateMutex();
    stateMutex = xSemaphoreCreateMutex();
    buttonMutex = xSemaphoreCreateMutex();
    // Kiểm tra lỗi khi tạo tftMutex
    if (tftMutex == NULL) {
        Serial.println("ERROR: Failed to create tftMutex!");
        while (1);
    }
    // Khởi tạo trạng thái ban đầu
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        currentState = MENU;
        selectedGame = 0;
        menuNeedsRedraw = true;
        firstRun = true;
        xSemaphoreGive(stateMutex);
    }
    // Tạo task xử lý input và logic game
    xTaskCreatePinnedToCore(inputTask, "Input Task", 4096, NULL, 2, &inputTaskHandle, 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    xTaskCreatePinnedToCore(gameLogicTask, "Game Logic Task", 8192, NULL, 1, &gameTaskHandle, 1);
}

// Vòng lặp chính (chỉ in trạng thái để debug)
void loop() {
    vTaskDelay(500 / portTICK_PERIOD_MS); // Chờ 500ms
    Serial.println(currentState); // In trạng thái hiện tại
}