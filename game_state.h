// Bảo vệ chống trùng lặp include
#ifndef GAME_STATE_H
#define GAME_STATE_H
#include "game_config.h"

// Định nghĩa enum cho các mức độ khó
enum Difficulty {
    EASY = 0,   // Dễ
    MEDIUM,     // Trung bình
    HARD        // Khó
};

// Khai báo các biến toàn cục cho trạng thái game
extern GameState currentState;      // Trạng thái hiện tại (MENU, GAME1, GAME2, DIFFICULTY)
extern int selectedGame;            // Game được chọn (0: Snake, 1: Racing)
extern bool menuNeedsRedraw;        // Cờ báo cần vẽ lại menu
extern bool firstRun;               // Cờ báo lần chạy đầu tiên
extern bool gameOver;               // Cờ báo trạng thái Game Over
extern int maxSnake;                // Điểm cao nhất game Snake
extern int maxRacing;               // Điểm cao nhất game Racing
extern Difficulty selectedDifficulty;// Độ khó được chọn
extern bool difficultyNeedsRedraw;   // Cờ báo cần vẽ lại menu độ khó
extern int playCount;               // Tổng số lần chơi (Snake + Racing)
extern unsigned long longestSurvivalTime; // Thời gian sống lâu nhất (ms)

// Biến lưu điểm cao cho game Snake
extern int highScoresEasy[5];       // Điểm cao mức dễ
extern int highScoresMedium[5];     // Điểm cao mức trung bình
extern int highScoresHard[5];       // Điểm cao mức khó

// Biến lưu điểm cao và thống kê cho game Racing
extern int racingHighScoresEasy[5]; // Điểm cao mức dễ
extern int racingHighScoresMedium[5];// Điểm cao mức trung bình
extern int racingHighScoresHard[5]; // Điểm cao mức khó
extern int racingPlayCount;         // Số lần chơi Racing
extern unsigned long racingLongestSurvivalTime; // Thời gian sống lâu nhất Racing
extern int racingCurrentRank;       // Xếp hạng hiện tại trong Racing

// Khai báo các hàm xử lý menu
void drawMenu();                    // Vẽ menu chính
void handleMenu();                  // Xử lý logic menu
void drawDifficultyMenu();          // Vẽ menu chọn độ khó
void handleDifficultyMenu();        // Xử lý logic menu độ khó

#endif