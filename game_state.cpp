#include "game_state.h"
#include "game_config.h"

extern TFT_eSPI display;
extern SemaphoreHandle_t tftMutex;

// Khởi tạo biến toàn cục
GameState currentState = MENU;
int selectedGame = 0;
bool menuNeedsRedraw = true;
bool firstRun = true;
bool gameOver = false;
Difficulty selectedDifficulty = EASY;
bool difficultyNeedsRedraw = true;
int maxSnake = 0;
int maxRacing = 0;
int playCount = 0;
unsigned long longestSurvivalTime = 0;
int highScoresEasy[5] = {0, 0, 0, 0, 0};
int highScoresMedium[5] = {0, 0, 0, 0, 0};
int highScoresHard[5] = {0, 0, 0, 0, 0};
int racingHighScoresEasy[5] = {0, 0, 0, 0, 0};
int racingHighScoresMedium[5] = {0, 0, 0, 0, 0};
int racingHighScoresHard[5] = {0, 0, 0, 0, 0};
int racingPlayCount = 0;
unsigned long racingLongestSurvivalTime = 0;
int racingCurrentRank = -1;

// Hàm vẽ màn hình chọn độ khó
void drawDifficultyMenu() {
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < display.height(); i++) {
            int color = display.color565(0, i / 2, 255 - i / 2);
            display.drawFastHLine(0, i, display.width(), color);
        }
        const char* title = "DIFFICULTY";
        int titleWidth = strlen(title) * 18;
        int titleX = (display.width() - titleWidth) / 2;
        display.setTextColor(TFT_WHITE);
        display.setTextSize(3);
        display.setCursor(titleX, 20);
        display.println(title);
        const char* difficulties[] = {"Easy", "Medium", "Hard"};
        for (int i = 0; i < 3; ++i) {
            display.setCursor(30, 100 + i * 50);
            if (selectedDifficulty == i) {
                display.setTextSize(2);
                display.print(">> ");
                display.setTextColor(TFT_WHITE);
                display.print(difficulties[i]);
            } else {
                display.setTextColor(TFT_WHITE);
                display.setTextSize(2);
                display.print("  ");
                display.println(difficulties[i]);
            }
        }
        xSemaphoreGive(tftMutex);
    }
}

// Hàm vẽ menu
void drawMenu() {
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < display.height(); i++) {
            int color = display.color565(0, i / 2, 255 - i / 2);
            display.drawFastHLine(0, i, display.width(), color);
        }
        const char* title = "GAME";
        int titleWidth = strlen(title) * 18;
        int titleX = (display.width() - titleWidth) / 2;
        display.setTextColor(TFT_WHITE);
        display.setTextSize(3);
        display.setCursor(titleX, 20);
        display.println(title);
        const char* games[] = {"Snake", "Racing"};
        for (int i = 0; i < NUM_GAMES; ++i) {
            display.setCursor(30, 100 + i * 50);
            if (selectedGame == i) {
                display.setTextSize(2);
                display.print(">> ");
                display.setTextColor(TFT_WHITE);
                display.print(games[i]);
            } else {
                display.setTextColor(TFT_WHITE);
                display.setTextSize(2);
                display.print("  ");
                display.println(games[i]);
            }
        }
        xSemaphoreGive(tftMutex);
    }
}

// Hàm xử lý menu
void handleMenu() {
    static unsigned long lastPressTime = 0;
    const unsigned long debounceDelay = 200;
    if (menuNeedsRedraw) {
        drawMenu();
        menuNeedsRedraw = false;
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
}

// Hàm xử lý màn hình độ khó
void handleDifficultyMenu() {
    if (difficultyNeedsRedraw) {
        drawDifficultyMenu();
        difficultyNeedsRedraw = false;
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
}