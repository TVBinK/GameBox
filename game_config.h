// Bảo vệ chống trùng lặp include
#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

// Bao gồm các thư viện cần thiết
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Định nghĩa các trạng thái game
enum GameState {
    MENU = 0,   // Menu chính
    GAME1,      // Game Snake
    GAME2,      // Game Racing
    DIFFICULTY  // Menu chọn độ khó
};

// Định nghĩa các nút bấm
enum Button {
    BTN_UP = 0,     // Nút lên
    BTN_DOWN = 1,   // Nút xuống
    BTN_LEFT = 2,   // Nút trái
    BTN_RIGHT = 3,  // Nút phải
    BTN_SELECT = 4, // Nút chọn
    BTN_RETURN = 5  // Nút quay lại
};

// Cấu trúc lưu tọa độ điểm
struct Point {
    int x; // Tọa độ X
    int y; // Tọa độ Y
};

// Cấu trúc lưu thông tin chướng ngại vật
struct Obstacle {
    int x;          // Tọa độ X (cho Racing)
    int y;          // Tọa độ Y (cho Racing)
    bool isMoving;  // Chướng ngại có di chuyển không (Racing)
    int direction;  // Hướng di chuyển (1: phải, -1: trái) (Racing)
    Point points[8]; // Mảng các điểm của chướng ngại (Snake)
    int length;     // Độ dài chướng ngại (Snake)
};

// Định nghĩa số lượng game và nút bấm
#define NUM_GAMES 2     // Số game (Snake, Racing)
#define NUM_BUTTONS 6   // Số nút bấm

// Định nghĩa các chân GPIO cho nút bấm
#define BTN_UP_PIN     25
#define BTN_DOWN_PIN   13
#define BTN_LEFT_PIN   33
#define BTN_RIGHT_PIN  12
#define BTN_SELECT_PIN 32
#define BTN_RETURN_PIN 14

// Định nghĩa chân điều khiển đèn nền
#define PIN_BACKLIGHT  4

// Khai báo các biến toàn cục
extern TFT_eSPI display;               // Đối tượng màn hình TFT
extern QueueHandle_t buttonQueue;      // Hàng đợi sự kiện nút bấm
extern SemaphoreHandle_t tftMutex;     // Khóa bảo vệ màn hình
extern SemaphoreHandle_t stateMutex;   // Khóa bảo vệ trạng thái
extern SemaphoreHandle_t buttonMutex;  // Khóa bảo vệ nút bấm

extern bool buttonStates[NUM_BUTTONS];    // Trạng thái hiện tại của nút
extern bool lastButtonStates[NUM_BUTTONS];// Trạng thái trước đó của nút

// Khai báo các hàm chung
bool checkButton(Button button);           // Kiểm tra trạng thái nút
void processButtonPress(Button button);    // Xử lý sự kiện nhấn nút
void handleMenu();                         // Xử lý menu chính

#endif