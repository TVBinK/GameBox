#ifndef GAME_STATE_H // Kiểm tra nếu chưa định nghĩa GAME_STATE_H
#define GAME_STATE_H   // Định nghĩa GAME_STATE_H để tránh bao gồm nhiều lần
#include "game_config.h" // Bao gồm tệp cấu hình game (chứa các hằng số như NUM_GAMES)
// Định nghĩa mức độ khó
enum Difficulty {
    EASY = 0,
    MEDIUM,
    HARD
};

// Khai báo các biến toàn cục (được định nghĩa ở file .cpp khác)
extern GameState currentState;  // Trạng thái hiện tại của game (MENU, GAME1, GAME2)
extern int selectedGame;        // Chỉ số trò chơi được chọn trong menu (0: Snake, 1: Racing)
extern bool menuNeedsRedraw;    // Cờ để kiểm tra xem menu có cần vẽ lại không
extern bool needRank;           // Cờ để kiểm tra xem có cần hiển thị bảng xếp hạng không
extern bool firstRun;           // Cờ để kiểm tra lần chạy đầu tiên của game
extern bool gameOver;           // Cờ để kiểm tra trạng thái game over
extern int maxSnake;            // Điểm cao nhất của game Snake
extern int maxRacing;           // Điểm cao nhất của game Racing
extern Difficulty selectedDifficulty; // Biến lưu độ khó được chọn
extern bool difficultyNeedsRedraw;   // Cờ để vẽ lại màn hình độ khó

// Khai báo các hàm xử lý menu và trạng thái game
void drawMenu();               // Hàm vẽ giao diện menu lên màn hình
void handleMenu();             // Hàm xử lý logic của menu (điều hướng, chọn game)
void initGameVariables();      // Hàm khởi tạo các biến game về giá trị ban đầu
void drawDifficultyMenu();  // Hàm vẽ màn hình chọn độ khó
void handleDifficultyMenu(); // Hàm xử lý logic chọn độ khó

#endif // GAME_STATE_H // Kết thúc kiểm tra định nghĩa