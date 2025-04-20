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

struct Point {
    int x;
    int y;
};

struct Obstacle {
    Point points[8];
    int length;
    bool isMoving;
    int direction;
};

extern Direction currentDir;
extern Point snake[SNAKE_MAX_LENGTH];
extern int snakeLength;
extern Point food;
extern FoodType foodType;
extern unsigned long foodSpawnTime;
extern unsigned long foodTimeout;
extern int score;
extern bool hasDrawnBackground;
extern unsigned long lastMoveTime;
extern unsigned long lastBoostTime; // Thêm để theo dõi thời gian tăng tốc
extern int moveDelay;
extern Difficulty currentDifficulty;
extern Obstacle obstacles[MAX_OBSTACLES];
extern int obstacleCount;
extern unsigned long gameStartTime;
extern int currentRank;
extern bool gameOverDrawn;
extern int highScoresEasy[5];
extern int highScoresMedium[5];
extern int highScoresHard[5];

void resetGame();
void runGameSnake();
void drawBackground();
void drawInitialState();
void displayGameOver();
void spawnFood();
bool checkCollision(Point p);
void drawSnake(int x, int y, bool isHead);
void drawFood(int x, int y);
void drawObstacles();
void updateObstacles();
void initObstacles();
void updateHighScores();

#endif