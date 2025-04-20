#ifndef GAME_STATE_H
#define GAME_STATE_H
#include "game_config.h"

enum Difficulty {
    EASY = 0,
    MEDIUM,
    HARD
};

extern GameState currentState;
extern int selectedGame;
extern bool menuNeedsRedraw;
extern bool firstRun;
extern bool gameOver;
extern int maxSnake;
extern int maxRacing;
extern Difficulty selectedDifficulty;
extern bool difficultyNeedsRedraw;
extern int playCount;
extern unsigned long longestSurvivalTime;
extern int highScores[5];

void drawMenu();
void handleMenu();
void drawDifficultyMenu();
void handleDifficultyMenu();

#endif