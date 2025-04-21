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

TFT_eSPI display = TFT_eSPI();

TaskHandle_t inputTaskHandle = NULL;
TaskHandle_t gameTaskHandle = NULL;
TaskHandle_t snakeUpdateTaskHandle = NULL;
TaskHandle_t snakeRenderTaskHandle = NULL;

QueueHandle_t buttonQueue;
QueueHandle_t snakeUpdateQueue;

SemaphoreHandle_t tftMutex;
SemaphoreHandle_t stateMutex;
SemaphoreHandle_t buttonMutex;

bool buttonStates[NUM_BUTTONS] = {false};
bool lastButtonStates[NUM_BUTTONS] = {false};

extern GameState currentState;
extern int selectedGame;
extern bool menuNeedsRedraw;
extern bool firstRun;
extern bool difficultyNeedsRedraw;

bool menuActive = true;
unsigned long lastStateChange = 0;
const unsigned long STATE_CHANGE_DELAY = 1000;

bool checkButton(Button btn) {
    if (xSemaphoreTake(buttonMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        bool state = buttonStates[btn];
        xSemaphoreGive(buttonMutex);
        return state;
    }
    return false;
}

void processButtonPress(Button btn) {
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        switch (currentState) {
            case MENU:
                if (btn == BTN_UP) {
                    selectedGame = (selectedGame > 0) ? selectedGame - 1 : NUM_GAMES - 1;
                    menuNeedsRedraw = true;
                } else if (btn == BTN_DOWN) {
                    selectedGame = (selectedGame + 1) % NUM_GAMES;
                    menuNeedsRedraw = true;
                } else if (btn == BTN_SELECT) {
                    switch (selectedGame) {
                        case 0: // Snake
                            currentState = DIFFICULTY;
                            difficultyNeedsRedraw = true;
                            break;
                        case 1: // Racing
                            currentState = GAME2;
                            menuActive = false;
                            firstRun = true;
                            if (snakeUpdateTaskHandle != NULL) {
                                vTaskSuspend(snakeUpdateTaskHandle);
                            }
                            if (snakeRenderTaskHandle != NULL) {
                                vTaskSuspend(snakeRenderTaskHandle);
                            }
                            for (int i = 0; i < 3; i++) {
                                if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                                    display.fillScreen(TFT_BLACK);
                                    xSemaphoreGive(tftMutex);
                                    Serial.println("Screen cleared for GAME2, attempt " + String(i + 1));
                                    break;
                                }
                                vTaskDelay(10 / portTICK_PERIOD_MS);
                            }
                            if (snakeUpdateTaskHandle != NULL) {
                                vTaskResume(snakeUpdateTaskHandle);
                            }
                            if (snakeRenderTaskHandle != NULL) {
                                vTaskResume(snakeRenderTaskHandle);
                            }
                            break;
                    }
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                }
                break;
            case DIFFICULTY:
                if (btn == BTN_UP) {
                    selectedDifficulty = (selectedDifficulty > EASY) ? static_cast<Difficulty>(selectedDifficulty - 1) : HARD;
                    difficultyNeedsRedraw = true;
                } else if (btn == BTN_DOWN) {
                    selectedDifficulty = (selectedDifficulty < HARD) ? static_cast<Difficulty>(selectedDifficulty + 1) : EASY;
                    difficultyNeedsRedraw = true;
                } else if (btn == BTN_SELECT) {
                    currentState = GAME1;
                    menuActive = false;
                    firstRun = true;
                    difficultyNeedsRedraw = false;
                    if (snakeUpdateTaskHandle != NULL) {
                        vTaskSuspend(snakeUpdateTaskHandle);
                    }
                    if (snakeRenderTaskHandle != NULL) {
                        vTaskSuspend(snakeRenderTaskHandle);
                    }
                    for (int i = 0; i < 3; i++) {
                        if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                            display.fillScreen(TFT_BLACK);
                            display.fillScreen(TFT_BLACK); // Gọi lần thứ hai để đảm bảo
                            xSemaphoreGive(tftMutex);
                            Serial.println("Screen cleared for GAME1, attempt " + String(i + 1));
                            break;
                        }
                        vTaskDelay(10 / portTICK_PERIOD_MS);
                    }
                    xTaskCreatePinnedToCore(snakeUpdateTask, "Snake Update Task", 8192, NULL, 2, &snakeUpdateTaskHandle, 1);
                    xTaskCreatePinnedToCore(snakeRenderTask, "Snake Render Task", 12288, NULL, 4, &snakeRenderTaskHandle, 1);
                    if (snakeUpdateTaskHandle != NULL) {
                        vTaskResume(snakeUpdateTaskHandle);
                    }
                    if (snakeRenderTaskHandle != NULL) {
                        vTaskResume(snakeRenderTaskHandle);
                    }
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                } else if (btn == BTN_RETURN) {
                    currentState = MENU;
                    menuNeedsRedraw = true;
                    difficultyNeedsRedraw = false;
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                }
                break;
            case GAME1:
            case GAME2:
                if (btn == BTN_RETURN) {
                    currentState = MENU;
                    menuNeedsRedraw = true;
                    menuActive = true;
                    difficultyNeedsRedraw = false;
                    if (currentState == GAME1) {
                        if (snakeUpdateTaskHandle != NULL) {
                            vTaskDelete(snakeUpdateTaskHandle);
                            snakeUpdateTaskHandle = NULL;
                        }
                        if (snakeRenderTaskHandle != NULL) {
                            vTaskDelete(snakeRenderTaskHandle);
                            snakeRenderTaskHandle = NULL;
                        }
                    }
                    for (int i = 0; i < 3; i++) {
                        if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                            display.fillScreen(TFT_BLACK);
                            display.fillScreen(TFT_BLACK);
                            xSemaphoreGive(tftMutex);
                            Serial.println("Screen cleared for MENU, attempt " + String(i + 1));
                            break;
                        }
                        vTaskDelay(10 / portTICK_PERIOD_MS);
                    }
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                }
                break;
        }
        xSemaphoreGive(stateMutex);
    }
}

void inputTask(void *parameter) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        lastButtonStates[i] = digitalRead(getPinForButton(static_cast<Button>(i))) == LOW;
        buttonStates[i] = lastButtonStates[i];
    }
    while (1) {
        if (xSemaphoreTake(buttonMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            for (int i = 0; i < NUM_BUTTONS; i++) {
                bool current = digitalRead(getPinForButton(static_cast<Button>(i))) == LOW;
                if (current && !lastButtonStates[i]) {
                    vTaskDelay(20 / portTICK_PERIOD_MS);
                    if (digitalRead(getPinForButton(static_cast<Button>(i))) == LOW) {
                        Button btn = static_cast<Button>(i);
                        xQueueSend(buttonQueue, &btn, 0);
                    }
                }
                lastButtonStates[i] = current;
                buttonStates[i] = current;
            }
            xSemaphoreGive(buttonMutex);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

int getPinForButton(Button btn) {
    switch(btn) {
        case BTN_UP: return BTN_UP_PIN;
        case BTN_DOWN: return BTN_DOWN_PIN;
        case BTN_LEFT: return BTN_LEFT_PIN;
        case BTN_RIGHT: return BTN_RIGHT_PIN;
        case BTN_SELECT: return BTN_SELECT_PIN;
        case BTN_RETURN: return BTN_RETURN_PIN;
        default: return -1;
    }
}

void gameLogicTask(void *parameter) {
    Button receivedButton;
    while (1) {
        if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            GameState currentGameState = currentState;
            xSemaphoreGive(stateMutex);
            if (currentGameState == MENU) {
                handleMenu();
            } else if (currentGameState == DIFFICULTY) {
                handleDifficultyMenu();
            } else {
                switch (currentGameState) {
                    case GAME1: runGameSnake(); break;
                    case GAME2: runGameRacing(); break;
                    default: break;
                }
            }
        }
        if (xQueueReceive(buttonQueue, &receivedButton, 0) == pdPASS) {
            processButtonPress(receivedButton);
        }
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    pinMode(BTN_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_LEFT_PIN, INPUT_PULLUP);
    pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);
    pinMode(BTN_SELECT_PIN, INPUT_PULLUP);
    pinMode(BTN_RETURN_PIN, INPUT_PULLUP);
    display.init();
    display.setRotation(0);
    display.fillScreen(TFT_BLACK);
    pinMode(PIN_BACKLIGHT, OUTPUT);
    digitalWrite(PIN_BACKLIGHT, HIGH);
    buttonQueue = xQueueCreate(10, sizeof(Button));
    snakeUpdateQueue = xQueueCreate(20, sizeof(UpdateData));
    Serial.println("snakeUpdateQueue created with size 20");
    tftMutex = xSemaphoreCreateMutex();
    stateMutex = xSemaphoreCreateMutex();
    buttonMutex = xSemaphoreCreateMutex();
    if (tftMutex == NULL) {
        Serial.println("ERROR: Failed to create tftMutex!");
        while (1);
    }
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        currentState = MENU;
        selectedGame = 0;
        menuNeedsRedraw = true;
        firstRun = true;
        xSemaphoreGive(stateMutex);
    }
    xTaskCreatePinnedToCore(inputTask, "Input Task", 4096, NULL, 2, &inputTaskHandle, 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    xTaskCreatePinnedToCore(gameLogicTask, "Game Logic Task", 8192, NULL, 1, &gameTaskHandle, 1);
}

void loop() {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    Serial.println(currentState);
}




