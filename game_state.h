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
extern int playCount; // Tổng số lần chơi (cả Snake và Racing)
extern unsigned long longestSurvivalTime; // Thời gian sống lâu nhất (cả Snake và Racing)

// Biến cho game Snake
extern int highScoresEasy[5];
extern int highScoresMedium[5];
extern int highScoresHard[5];

// Biến cho game Racing
extern int racingHighScoresEasy[5];
extern int racingHighScoresMedium[5];
extern int racingHighScoresHard[5];
extern int racingPlayCount; // Số lần chơi Racing
extern unsigned long racingLongestSurvivalTime; // Thời gian sống lâu nhất trong Racing
extern int racingCurrentRank; // Xếp hạng hiện tại trong Racing

void drawMenu();
void handleMenu();
void drawDifficultyMenu();
void handleDifficultyMenu();

#endif