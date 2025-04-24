// Bảo vệ chống trùng lặp include
#ifndef GAME_SNAKE_H
#define GAME_SNAKE_H

// Bao gồm các thư viện và file tiêu đề cần thiết
#include <cstdint>
#include "game_config.h"
#include "game_state.h"

// Định nghĩa các hằng số cho game Snake
#define SNAKE_MAX_LENGTH 100      // Độ dài tối đa của rắn
#define CELL_SIZE 10              // Kích thước mỗi ô trên lưới (pixel)
#define SCREEN_WIDTH 240          // Chiều rộng màn hình
#define SCREEN_HEIGHT 320         // Chiều cao màn hình
#define NUM_COLS (SCREEN_WIDTH / CELL_SIZE) // Số cột trên lưới
#define NUM_ROWS (SCREEN_HEIGHT / CELL_SIZE) // Số hàng trên lưới
#define MAX_OBSTACLES 10          // Số chướng ngại vật tối đa
#define SPECIAL_FOOD_CHANCE 20    // Xác suất xuất hiện thức ăn đặc biệt (1/20)
#define SCORE_AREA_HEIGHT 30      // Chiều cao vùng hiển thị điểm số

// Định nghĩa các hướng di chuyển của rắn
enum Direction {
    UP,    // Lên
    DOWN,  // Xuống
    LEFT,  // Trái
    RIGHT  // Phải
};

// Định nghĩa các loại thức ăn
enum FoodType {
    NORMAL,  // Thức ăn thường
    SPECIAL  // Thức ăn đặc biệt (cho nhiều điểm hơn)
};

// Định nghĩa các loại cập nhật cho game Snake
enum UpdateType {
    UPDATE_SNAKE,      // Cập nhật trạng thái rắn
    UPDATE_FOOD,       // Cập nhật thức ăn
    UPDATE_OBSTACLES,  // Cập nhật chướng ngại vật
    UPDATE_SCORE,      // Cập nhật điểm số
    UPDATE_GAME_OVER   // Cập nhật trạng thái Game Over
};

// Cấu trúc dữ liệu cập nhật cho game Snake
struct UpdateData {
    UpdateType type;      // Loại cập nhật
    int snakeLength;      // Độ dài rắn
    Point food;           // Vị trí thức ăn
    FoodType foodType;    // Loại thức ăn
    int obstacleCount;    // Số chướng ngại vật
    int score;            // Điểm số
    bool gameOver;        // Trạng thái Game Over
};

// Khai báo các biến toàn cục cho game Snake
extern Direction currentDir;                   // Hướng di chuyển hiện tại của rắn
extern Point snake[SNAKE_MAX_LENGTH];         // Mảng lưu các đoạn thân rắn
extern int snakeLength;                       // Độ dài hiện tại của rắn
extern Point food;                            // Vị trí thức ăn hiện tại
extern Point oldFood;                         // Vị trí thức ăn cũ
extern bool needClearOldFood;                 // Cờ báo cần xóa thức ăn cũ
extern FoodType foodType;                     // Loại thức ăn hiện tại
extern unsigned long foodSpawnTime;           // Thời gian thức ăn xuất hiện
extern unsigned long foodTimeout;             // Thời gian tồn tại của thức ăn
extern int score;                             // Điểm số hiện tại
extern bool hasDrawnBackground;               // Cờ báo nền đã được vẽ
extern unsigned long lastMoveTime;            // Thời gian di chuyển cuối của rắn
extern unsigned long lastBoostTime;           // Thời gian tăng tốc cuối
extern int moveDelay;                         // Độ trễ giữa các lần di chuyển (ms)
extern Difficulty currentDifficulty;          // Độ khó hiện tại
extern Obstacle obstacles[MAX_OBSTACLES];     // Mảng lưu chướng ngại vật
extern int obstacleCount;                     // Số lượng chướng ngại vật
extern unsigned long gameStartTime;           // Thời gian bắt đầu game
extern int currentRank;                       // Xếp hạng hiện tại
extern bool gameOverDrawn;                    // Cờ báo màn hình Game Over đã vẽ
extern QueueHandle_t snakeUpdateQueue;        // Hàng đợi cập nhật dữ liệu Snake
extern TaskHandle_t snakeUpdateTaskHandle;    // Task cập nhật logic Snake
extern TaskHandle_t snakeRenderTaskHandle;    // Task vẽ giao diện Snake
extern int highScoresEasy[5];                 // Bảng điểm cao mức EASY
extern int highScoresMedium[5];               // Bảng điểm cao mức MEDIUM
extern int highScoresHard[5];                 // Bảng điểm cao mức HARD
extern TaskHandle_t gameTaskHandle;           // Task xử lý logic game chính
extern TaskHandle_t inputTaskHandle;          // Task xử lý input
extern unsigned long lastScoreUpdate;         // Thời gian cập nhật điểm số cuối
extern bool gameOverScreenActive;             // Cờ báo màn hình Game Over đang hiển thị

// Khai báo các hàm của game Snake
void resetGame();                             // Đặt lại trạng thái game
void runGameSnake();                          // Chạy game Snake
void snakeUpdateTask(void *parameter);        // Task cập nhật logic game
void snakeRenderTask(void *parameter);        // Task vẽ giao diện game
void drawBackground();                        // Vẽ nền game
void drawBorder();                            // Vẽ viền màn hình
void drawScoreArea();                         // Vẽ vùng hiển thị điểm số
void drawInitialState();                      // Vẽ trạng thái ban đầu
void displayGameOver();                       // Hiển thị màn hình Game Over
void spawnFood();                             // Tạo thức ăn mới
bool checkCollision(Point p);                 // Kiểm tra va chạm
void drawSnake(int x, int y, bool isHead);    // Vẽ một đoạn thân rắn
void drawFood(int x, int y);                  // Vẽ thức ăn
void clearFoodArea(int x, int y);             // Xóa vùng thức ăn
void drawObstacles();                         // Vẽ tất cả chướng ngại vật
void updateObstacles();                       // Cập nhật vị trí chướng ngại vật
void initObstacles();                         // Khởi tạo chướng ngại vật
void updateHighScores();                      // Cập nhật bảng điểm cao

#endif