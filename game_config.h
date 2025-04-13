#ifndef GAME_CONFIG_H // Kiểm tra nếu chưa định nghĩa GAME_CONFIG_H
#define GAME_CONFIG_H   // Định nghĩa GAME_CONFIG_H để tránh bao gồm nhiều lần

#include <Arduino.h>         // Thư viện Arduino cơ bản
#include <TFT_eSPI.h>        // Thư viện điều khiển màn hình TFT_eSPI
#include "freertos/FreeRTOS.h" // Thư viện FreeRTOS cho hệ thống đa nhiệm
#include "freertos/task.h"    // Thư viện quản lý task trong FreeRTOS
#include "freertos/queue.h"   // Thư viện hàng đợi trong FreeRTOS
#include "freertos/semphr.h"  // Thư viện semaphore trong FreeRTOS

// Định nghĩa các trạng thái game trong enum GameState
enum GameState {
    MENU = 0,  // Trạng thái menu chính (0)
    GAME1,     // Trạng thái chơi game Snake (1)
    GAME2,     // Trạng thái chơi game Racing (2)
    DIFFICULTY
};

// Định nghĩa các nút bấm trong enum Button
enum Button {
    BTN_UP = 0,     // Nút UP (0)
    BTN_DOWN = 1,   // Nút DOWN (1)
    BTN_LEFT = 2,   // Nút LEFT (2)
    BTN_RIGHT = 3,  // Nút RIGHT (3)
    BTN_SELECT = 4, // Nút SELECT (4)
    BTN_RETURN = 5  // Nút RETURN (5)
};

// Định nghĩa số lượng game và nút bấm
#define NUM_GAMES 2    // Tổng số trò chơi (Snake và Racing)
#define NUM_BUTTONS 6  // Tổng số nút bấm (UP, DOWN, LEFT, RIGHT, SELECT, RETURN)

// Định nghĩa các chân GPIO cho nút bấm
#define BTN_UP_PIN     26  // Chân GPIO cho nút UP
#define BTN_DOWN_PIN   25  // Chân GPIO cho nút DOWN
#define BTN_LEFT_PIN   32  // Chân GPIO cho nút LEFT
#define BTN_RIGHT_PIN  33  // Chân GPIO cho nút RIGHT
#define BTN_SELECT_PIN 27  // Chân GPIO cho nút SELECT
#define BTN_RETURN_PIN 14  // Chân GPIO cho nút RETURN

// Định nghĩa chân điều khiển đèn nền
#define PIN_BACKLIGHT  4   // Chân GPIO cho đèn nền màn hình

// Khai báo các biến toàn cục (được định nghĩa ở file .cpp khác)
extern TFT_eSPI display;                // Đối tượng màn hình TFT
extern QueueHandle_t buttonQueue;       // Hàng đợi lưu trữ sự kiện nút bấm
extern SemaphoreHandle_t tftMutex;      // Semaphore bảo vệ truy cập màn hình TFT
extern SemaphoreHandle_t stateMutex;    // Semaphore bảo vệ trạng thái game
extern SemaphoreHandle_t buttonMutex;   // Semaphore bảo vệ trạng thái nút bấm

extern bool buttonStates[NUM_BUTTONS];    // Mảng lưu trạng thái hiện tại của các nút
extern bool lastButtonStates[NUM_BUTTONS]; // Mảng lưu trạng thái trước đó của các nút

// Khai báo các hàm chung
bool checkButton(Button button);         // Hàm kiểm tra trạng thái của một nút
void processButtonPress(Button button);  // Hàm xử lý sự kiện khi nút được nhấn
void handleMenu();                       // Hàm xử lý logic của menu

#endif // GAME_CONFIG_H // Kết thúc kiểm tra định nghĩa