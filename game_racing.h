// Bảo vệ chống trùng lặp include
#ifndef GAME_RACING_H
#define GAME_RACING_H

// Bao gồm các thư viện cần thiết
#include <TFT_eSPI.h>
#include "game_config.h"
#include "game_state.h"

// Khai báo các đối tượng và biến toàn cục từ file khác
extern TFT_eSPI display;                    // Đối tượng màn hình TFT
extern TaskHandle_t gameTaskHandle;         // Task xử lý logic game chính
extern TaskHandle_t inputTaskHandle;        // Task xử lý input
extern TaskHandle_t snakeUpdateTaskHandle;  // Task cập nhật logic Snake
extern TaskHandle_t snakeRenderTaskHandle;  // Task vẽ giao diện Snake
extern QueueHandle_t snakeUpdateQueue;     // Hàng đợi dữ liệu Snake

// Định nghĩa các màu sắc cho game Racing
#define CAR_RED 0xF800        // Màu đỏ cho xe chính
#define ENEMY_YELLOW 0xFFE0   // Màu vàng cho xe địch
#define BARRIER_BLUE 0x001F   // Màu xanh cho chướng ngại vật
#define ROAD_GRAY 0x4208      // Màu xám cho đường
#define ROAD_DARK 0x2104      // Màu xám đậm cho đường
#define TFT_DARKRED   0x8000  // Màu đỏ đậm
#define TFT_DARKGREY  0x7BEF  // Màu xám đậm

// Định nghĩa các hằng số cho giao diện và logic game
#define LANE_COUNT 4          // Số làn đường
#define CAR_WIDTH 36          // Chiều rộng xe
#define CAR_HEIGHT 52         // Chiều cao xe
#define CAR_CLEAR_WIDTH 42    // Chiều rộng vùng xóa xe
#define CAR_CLEAR_HEIGHT 60   // Chiều cao vùng xóa xe
#define BARRIER_WIDTH 30      // Chiều rộng chướng ngại vật
#define BARRIER_HEIGHT 15     // Chiều cao chướng ngại vật
#define BARRIER_CLEAR_WIDTH 36 // Chiều rộng vùng xóa chướng ngại
#define BARRIER_CLEAR_HEIGHT 21 // Chiều cao vùng xóa chướng ngại
#define SCREEN_WIDTH 240      // Chiều rộng màn hình
#define SCREEN_HEIGHT 320     // Chiều cao màn hình
#define CAR_SPEED_Y 10        // Tốc độ di chuyển theo trục Y
#define CAR_Y_MIN 40          // Vị trí Y tối thiểu của xe
#define CAR_Y_MAX 280         // Vị trí Y tối đa của xe
#define MOVE_Y_INTERVAL 100   // Khoảng thời gian di chuyển Y (ms)
#define BARRIER_SCORE_PENALTY 5 // Phạt điểm khi va chạm chướng ngại
#define PUSHBACK_SPEED 5      // Tốc độ đẩy lùi khi va chạm
#define START_Y (SCREEN_HEIGHT - 40) // Vị trí Y ban đầu của xe
#define ENEMY_LANE_CHANGE_INTERVAL 3000 // Thời gian đổi làn của xe địch (ms)

// Cấu trúc lưu cài đặt độ khó
struct DifficultySettings {
    int enemySpeed;      // Tốc độ xe địch
    int enemyCount;      // Số lượng xe địch
    bool allowLaneChange;// Cho phép xe địch đổi làn
    bool hasBarrier;     // Có chướng ngại vật hay không
};

// Định nghĩa các loại cập nhật cho game Racing
enum RacingUpdateType {
    UPDATE_RACING,         // Cập nhật trạng thái game
    UPDATE_RACING_SCORE,   // Cập nhật điểm số
    UPDATE_RACING_GAME_OVER,// Cập nhật trạng thái Game Over
    UPDATE_RACING_RESET    // Đặt lại trạng thái game
};

// Cấu trúc dữ liệu cập nhật cho game Racing
struct RacingUpdateData {
    RacingUpdateType type; // Loại cập nhật
    int car_x;             // Tọa độ X của xe chính
    int car_y;             // Tọa độ Y của xe chính
    int car_lane;          // Làn đường hiện tại của xe
    int enemy_x[4];        // Tọa độ X của xe địch
    int enemy_y[4];        // Tọa độ Y của xe địch
    int score;             // Điểm số
    int barrier_x;         // Tọa độ X của chướng ngại vật
    int barrier_y;         // Tọa độ Y của chướng ngại vật
    bool gameOver;         // Trạng thái Game Over
};

// Khai báo các biến toàn cục
extern unsigned long racingGameStartTime;   // Thời gian bắt đầu game
extern QueueHandle_t racingUpdateQueue;     // Hàng đợi cập nhật Racing
extern TaskHandle_t racingUpdateTaskHandle; // Task cập nhật logic Racing
extern TaskHandle_t racingRenderTaskHandle; // Task vẽ giao diện Racing

// Khai báo các hàm
void drawBackgroundRacing();    // Vẽ nền đường đua
void drawCar(int x, int y, uint16_t color); // Vẽ xe
void delCar(int x, int y);      // Xóa xe
void drawBarrier(int x, int y); // Vẽ chướng ngại vật
void delBarrier(int x, int y);  // Xóa chướng ngại vật
void drawRacingScoreArea();     // Vẽ vùng hiển thị điểm số
void initGameRacing();          // Khởi tạo game Racing
void runGameRacing();           // Chạy game Racing
void racingUpdateTask(void *parameter); // Task cập nhật logic
void racingRenderTask(void *parameter); // Task vẽ giao diện
void gameOverRacing();          // Hiển thị màn hình Game Over
void updateRacingHighScores();  // Cập nhật bảng điểm cao

#endif