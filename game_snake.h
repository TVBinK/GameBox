#ifndef GAME_SNAKE_H
#define GAME_SNAKE_H

#include <cstdint>
#include "game_config.h"
#include "game_state.h"

// Định nghĩa các hằng số cho game Snake
#define SNAKE_MAX_LENGTH 100
#define CELL_SIZE 10
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define NUM_COLS (SCREEN_WIDTH / CELL_SIZE)
#define NUM_ROWS (SCREEN_HEIGHT / CELL_SIZE)
#define MAX_OBSTACLES 10
#define SPECIAL_FOOD_CHANCE 20
#define SCORE_AREA_HEIGHT 30  // Hằng số cho khu vực bảo vệ điểm số

enum Direction {
    UP,
    DOWN,
    LEFT,
    RIGHT
};

enum FoodType {
    NORMAL,
    SPECIAL
};

enum UpdateType {
    UPDATE_SNAKE,
    UPDATE_FOOD,
    UPDATE_OBSTACLES,
    UPDATE_SCORE,
    UPDATE_GAME_OVER
};

struct UpdateData {
    UpdateType type;
    int snakeLength;
    Point food;
    FoodType foodType;
    int obstacleCount;
    int score;
    bool gameOver;
};

extern Direction currentDir;
extern Point snake[SNAKE_MAX_LENGTH];
extern int snakeLength;
extern Point food;
extern Point oldFood;
extern bool needClearOldFood;
extern FoodType foodType;
extern unsigned long foodSpawnTime;
extern unsigned long foodTimeout;
extern int score;
extern bool hasDrawnBackground;
extern unsigned long lastMoveTime;
extern unsigned long lastBoostTime;
extern int moveDelay;
extern Difficulty currentDifficulty;
extern Obstacle obstacles[MAX_OBSTACLES];
extern int obstacleCount;
extern unsigned long gameStartTime;
extern int currentRank;
extern bool gameOverDrawn;
extern QueueHandle_t snakeUpdateQueue;
extern TaskHandle_t snakeUpdateTaskHandle;
extern TaskHandle_t snakeRenderTaskHandle;
extern int highScoresEasy[5];
extern int highScoresMedium[5];
extern int highScoresHard[5];
extern TaskHandle_t gameTaskHandle;
extern TaskHandle_t inputTaskHandle;
extern unsigned long lastScoreUpdate;
extern bool gameOverScreenActive;

void resetGame();
void runGameSnake();
void snakeUpdateTask(void *parameter);
void snakeRenderTask(void *parameter);
void drawBackground();
void drawBorder();
void drawScoreArea();
void drawInitialState();
void displayGameOver();
void spawnFood();
bool checkCollision(Point p);
void drawSnake(int x, int y, bool isHead);
void drawFood(int x, int y);
void clearFoodArea(int x, int y);
void drawObstacles();
void updateObstacles();
void initObstacles();
void updateHighScores();

#endif