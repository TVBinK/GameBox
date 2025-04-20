#include "game_racing.h"
#include "game_state.h"

// Định nghĩa các biến toàn cục cho game đua xe
int car_lane = 2;
int car_x = SCREEN_WIDTH / 2;
int car_y = SCREEN_HEIGHT - 40;
int enemy_x[ENEMY_COUNT];
int enemy_y[ENEMY_COUNT];
static int racingScore = 0;
const int lane_width = SCREEN_WIDTH / LANE_COUNT;
const int lane_centers[LANE_COUNT] = {30, 90, 150, 210};
bool hasDrawBG = false;
bool needDelCar = false;
bool gameOverFlag = false;
bool exitGameFlag = false;

// Hàm vẽ nền đường đua
void drawBackgroundRacing() {
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        display.fillScreen(ROAD_GRAY);
        for (int i = 1; i < LANE_COUNT; i++) {
            int line_x = i * lane_width;
            for (int y = 0; y < SCREEN_HEIGHT; y += 20) {
                display.drawFastVLine(line_x, y, 10, TFT_WHITE);
            }
        }
        Serial.println("Drawing lanes at 60, 120, 180");
        xSemaphoreGive(tftMutex);
    }
}

// Hàm vẽ xe
void drawCar(int x, int y, uint16_t color) {
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        int body_width = 16;
        int body_height = 40;
        display.fillRoundRect(x - body_width / 2, y - body_height / 2, body_width, body_height, 3, color);
        display.fillEllipse(x, y - 5, 5, 8, TFT_BLACK);
        display.fillTriangle(x - 4, y - body_height / 2, x + 4, y - body_height / 2, x, y - body_height / 2 - 6, color);
        display.fillRect(x - 12, y - body_height / 2 - 2, 24, 2, TFT_DARKGREY);
        display.fillRect(x - 14, y + body_height / 2, 28, 3, TFT_DARKGREY);
        int wheel_radius = 4;
        display.fillCircle(x - 10, y - body_height / 2 + 6, wheel_radius, TFT_BLACK);
        display.fillCircle(x + 10, y - body_height / 2 + 6, wheel_radius, TFT_BLACK);
        display.fillCircle(x - 10, y + body_height / 2 - 6, wheel_radius, TFT_BLACK);
        display.fillCircle(x + 10, y + body_height / 2 - 6, wheel_radius, TFT_BLACK);
        xSemaphoreGive(tftMutex);
    }
}

// Hàm xóa xe
void delCar(int x, int y) {
    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        int clear_width = 36;
        int clear_height = 52;
        display.fillRect(x - clear_width / 2, y - clear_height / 2, clear_width, clear_height, ROAD_GRAY);
        xSemaphoreGive(tftMutex);
    }
}

// Hàm khởi tạo game Racing
void initGameRacing() {
    car_lane = 2;
    car_x = lane_centers[car_lane];
    car_y = SCREEN_HEIGHT - 40;
    racingScore = 0;
    for (int i = 0; i < ENEMY_COUNT; i++) {
        enemy_x[i] = lane_centers[random(0, LANE_COUNT)];
        enemy_y[i] = -CAR_HEIGHT - i * (SCREEN_HEIGHT / ENEMY_COUNT);
    }
    hasDrawBG = false;
    needDelCar = false;
    gameOverFlag = false;
    exitGameFlag = false;
}

// Hàm chạy logic chính của game Racing
bool runGameRacing() {
    if (firstRun) {
        initGameRacing();
        firstRun = false;
    }

    if (gameOverFlag) {
        if (checkButton(BTN_RETURN)) {
            exitGameFlag = true;
        }
        return !exitGameFlag;
    }

    if (!hasDrawBG) {
        drawBackgroundRacing();
        hasDrawBG = true;
    }

    if (checkButton(BTN_LEFT) && car_lane > 0) {
        delCar(car_x, car_y);
        car_lane--;
        car_x = lane_centers[car_lane];
        needDelCar = false;
        Serial.print("Car moved to lane: "); Serial.println(car_lane);
    }
    if (checkButton(BTN_RIGHT) && car_lane < LANE_COUNT - 1) {
        delCar(car_x, car_y);
        car_lane++;
        car_x = lane_centers[car_lane];
        needDelCar = false;
        Serial.print("Car moved to lane: "); Serial.println(car_lane);
    }

    for (int i = 0; i < ENEMY_COUNT; i++) {
        delCar(enemy_x[i], enemy_y[i]);
        enemy_y[i] += ENEMY_SPEED;
        if (enemy_y[i] > SCREEN_HEIGHT + CAR_HEIGHT) {
            enemy_x[i] = lane_centers[random(0, LANE_COUNT)];
            enemy_y[i] = -CAR_HEIGHT;
            racingScore++;
            if (racingScore > maxRacing) maxRacing = racingScore;
        }
        drawCar(enemy_x[i], enemy_y[i], ENEMY_YELLOW);
    }

    if (needDelCar) {
        delCar(car_x, car_y);
    }
    drawCar(car_x, car_y, CAR_RED);
    needDelCar = true;

    for (int i = 0; i < ENEMY_COUNT; i++) {
        if (car_x == enemy_x[i] && abs(car_y - enemy_y[i]) <= CAR_HEIGHT) {
            gameOverRacing();
            return true;
        }
    }

    if (xSemaphoreTake(tftMutex, portMAX_DELAY) == pdTRUE) {
        display.setTextColor(TFT_WHITE, ROAD_GRAY);
        display.setTextSize(2);
        display.setCursor(10, 10);
        display.print("Score: ");
        display.print(racingScore);
        xSemaphoreGive(tftMutex);
    }

    vTaskDelay(30 / portTICK_PERIOD_MS);
    return true;
}

// Hàm xử lý khi game over
void gameOverRacing() {
    delCar(car_x, car_y);
    for (int i = 0; i < ENEMY_COUNT; i++) {
        delCar(enemy_x[i], enemy_y[i]);
    }

    if (xSemaphoreTake(tftMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        display.fillScreen(TFT_BLACK);
        display.setTextColor(TFT_RED);
        display.setTextSize(2);
        const char* gameOverText = "Game Over!";
        int textWidth = strlen(gameOverText) * 12;
        int x = (SCREEN_WIDTH - textWidth) / 2;
        int y = 100;
        display.setCursor(x, y);
        display.print(gameOverText);

        char scoreText[20];
        sprintf(scoreText, "Score: %d", racingScore);
        textWidth = strlen(scoreText) * 12;
        x = (SCREEN_WIDTH - textWidth) / 2;
        y = 140;
        display.setCursor(x, y);
        display.print(scoreText);

        if (racingScore > maxRacing) maxRacing = racingScore;
        char highScoreText[20];
        sprintf(highScoreText, "High Score: %d", maxRacing);
        textWidth = strlen(highScoreText) * 12;
        x = (SCREEN_WIDTH - textWidth) / 2;
        y = 180;
        display.setCursor(x, y);
        display.print(highScoreText);

        xSemaphoreGive(tftMutex);
        Serial.println("Game Over displayed");
    } else {
        Serial.println("ERROR: Failed to take tftMutex in gameOverRacing!");
    }

    gameOverFlag = true;
}