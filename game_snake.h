#ifndef GAME_SNAKE_H // Kiểm tra nếu chưa định nghĩa GAME_SNAKE_H
#define GAME_SNAKE_H   // Định nghĩa GAME_SNAKE_H để tránh bao gồm nhiều lần

#include <cstdint>      // Thư viện cung cấp các kiểu dữ liệu nguyên chuẩn (int, uint32_t, ...)
#include "game_config.h" // Bao gồm tệp cấu hình game (chứa Button, TFT_eSPI, ...)
#include "game_state.h"

// Định nghĩa các hằng số cho game Snake
#define SNAKE_MAX_LENGTH 100  // Độ dài tối đa của con rắn (100 ô)
#define CELL_SIZE 10          // Kích thước mỗi ô trên lưới (10px x 10px)
#define SCREEN_WIDTH 240      // Chiều rộng màn hình (240px)
#define SCREEN_HEIGHT 320     // Chiều cao màn hình (320px)
#define NUM_COLS (SCREEN_WIDTH / CELL_SIZE)   // Số cột trên lưới (240 / 10 = 24)
#define NUM_ROWS (SCREEN_HEIGHT / CELL_SIZE)  // Số hàng trên lưới (320 / 10 = 32)

// Định nghĩa các hướng di chuyển của rắn
enum Direction { 
    UP,    // Hướng lên
    DOWN,  // Hướng xuống
    LEFT,  // Hướng trái
    RIGHT  // Hướng phải
};

// Cấu trúc Point để lưu tọa độ của rắn và thức ăn
struct Point {
    int x; // Tọa độ x (cột)
    int y; // Tọa độ y (hàng)
};

// Khai báo các biến toàn cục (được định nghĩa ở file .cpp)
extern Direction currentDir;            // Hướng hiện tại của rắn
extern Point snake[SNAKE_MAX_LENGTH];   // Mảng lưu các đoạn thân rắn
extern int snakeLength;                 // Độ dài hiện tại của rắn
extern Point food;                      // Tọa độ của thức ăn
extern int score;                       // Điểm số hiện tại
extern bool hasDrawnBackground;         // Cờ kiểm tra xem nền đã được vẽ chưa
extern unsigned long lastMoveTime;      // Thời gian di chuyển cuối cùng của rắn
extern int moveDelay;                   // Độ trễ giữa các lần di chuyển (ms)
extern int maxSnake;                    // Điểm cao nhất của game Snake
extern Difficulty currentDifficulty;    // Biến lưu độ khó hiện tại của game Snake

// Khai báo các hàm cho game Snake
void resetGame();          // Hàm đặt lại game về trạng thái ban đầu
void runGameSnake();       // Hàm chạy logic chính của game Snake
void drawBackground();     // Hàm vẽ nền lưới cho game
void drawInitialState();   // Hàm vẽ trạng thái ban đầu (rắn và thức ăn)
void displayGameOver();    // Hàm hiển thị màn hình "Game Over"
void spawnFood();          // Hàm tạo vị trí ngẫu nhiên cho thức ăn
bool checkCollision(Point p); // Hàm kiểm tra va chạm của rắn với chính nó hoặc tường

void drawSnake(int x, int y, bool isHead); // Hàm vẽ một đoạn rắn, isHead xác định đầu rắn
void drawFood(int x, int y);               // Hàm vẽ thức ăn lên màn hình

#endif // GAME_SNAKE_H // Kết thúc kiểm tra định nghĩa