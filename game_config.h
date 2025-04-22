#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Định nghĩa các trạng thái game trong enum GameState
enum GameState {
    MENU = 0,
    GAME1,
    GAME2,
    DIFFICULTY
};

// Định nghĩa các nút bấm trong enum Button
enum Button {
    BTN_UP = 0,
    BTN_DOWN = 1,
    BTN_LEFT = 2,
    BTN_RIGHT = 3,
    BTN_SELECT = 4,
    BTN_RETURN = 5
};

// Định nghĩa cấu trúc Point (dùng chung cho Snake và Racing)
struct Point {
    int x;
    int y;
};

// Định nghĩa cấu trúc cho vật cản (dùng chung cho Snake và Racing)
struct Obstacle {
    int x;          // Tọa độ x của vật cản (dùng cho Racing)
    int y;          // Tọa độ y của vật cản (dùng cho Racing)
    bool isMoving;  // Vật cản có di chuyển không (dùng cho Racing)
    int direction;  // Hướng di chuyển (1: phải, -1: trái) (dùng cho Racing)
    Point points[8]; // Mảng các điểm của vật cản (dùng cho Snake)
    int length;     // Độ dài vật cản (dùng cho Snake)
};

// Định nghĩa số lượng game và nút bấm
#define NUM_GAMES 2
#define NUM_BUTTONS 6

// Định nghĩa các chân GPIO cho nút bấm
#define BTN_UP_PIN     26
#define BTN_DOWN_PIN   25
#define BTN_LEFT_PIN   32
#define BTN_RIGHT_PIN  33
#define BTN_SELECT_PIN 27
#define BTN_RETURN_PIN 14

// Định nghĩa chân điều khiển đèn nền
#define PIN_BACKLIGHT  4

// Khai báo các biến toàn cục
extern TFT_eSPI display;
extern QueueHandle_t buttonQueue;
extern SemaphoreHandle_t tftMutex;
extern SemaphoreHandle_t stateMutex;
extern SemaphoreHandle_t buttonMutex;

extern bool buttonStates[NUM_BUTTONS];
extern bool lastButtonStates[NUM_BUTTONS];

// Khai báo các hàm chung
bool checkButton(Button button);
void processButtonPress(Button button);
void handleMenu();

#endif