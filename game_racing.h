#ifndef GAME_RACING_H
#define GAME_RACING_H

#include <TFT_eSPI.h>
#include "game_config.h"
#include "game_state.h"

extern TFT_eSPI display;

#define CAR_RED 0xF800       // Màu đỏ cho xe người chơi 
#define ENEMY_YELLOW 0xFFE0  // Màu vàng cho xe đối thủ 
#define BARRIER_BLUE 0x001F  // Màu xanh lam cho rào chắn
#define ROAD_GRAY 0x4208     // Màu xám cho đường
#define ROAD_DARK 0x2104     // Màu xám đậm cho nền
#define LANE_COUNT 4         // 4 làn đường
#define CAR_WIDTH 36         // Kích thước xe khớp với bitmap
#define CAR_HEIGHT 52        // Kích thước xe khớp với bitmap
#define CAR_CLEAR_WIDTH 42   // Kích thước vùng xóa xe
#define CAR_CLEAR_HEIGHT 60  // Kích thước vùng xóa xe
#define BARRIER_WIDTH 30     // Chiều rộng rào chắn
#define BARRIER_HEIGHT 15    // Chiều cao rào chắn
#define BARRIER_CLEAR_WIDTH 36   // Kích thước vùng xóa rào chắn
#define BARRIER_CLEAR_HEIGHT 21  // Kích thước vùng xóa rào chắn
#define SCREEN_WIDTH 240     // Chiều rộng 240px
#define SCREEN_HEIGHT 320    // Chiều cao 320px
#define CAR_SPEED_Y 10       // Tốc độ di chuyển dọc của xe (10px mỗi lần di chuyển)
#define CAR_Y_MIN 40         // Giới hạn trên của xe (tránh khu vực điểm số)
#define CAR_Y_MAX 280        // Giới hạn dưới của xe (gần bottom màn hình)
#define MOVE_Y_INTERVAL 100  // Khoảng thời gian giữa các lần di chuyển dọc khi giữ nút (ms)
#define BARRIER_SCORE_PENALTY 5 // Số điểm bị trừ khi chạm rào chắn
#define PUSHBACK_SPEED 5     // Tốc độ lùi dần khi chạm rào chắn (px mỗi 50ms)
#define START_Y (SCREEN_HEIGHT - 40) // Vạch xuất phát (y ban đầu của xe)
#define ENEMY_LANE_CHANGE_INTERVAL 3000 // Khoảng thời gian giữa các lần nhảy làn (ms, tăng từ 500ms lên 1000ms)

// Định nghĩa tham số cho các mức độ khó
struct DifficultySettings {
    int enemySpeed;      // Tốc độ xe địch và rào chắn
    int enemyCount;      // Số lượng xe địch
    bool allowLaneChange; // Cho phép xe địch đổi làn
    bool hasBarrier;     // Có rào chắn hay không
};

extern unsigned long racingGameStartTime; // Thời gian bắt đầu game Racing

// Định nghĩa màu sắc cho nền đua xe
#define TFT_DARKRED   0x8000  // Màu đỏ đậm
#define TFT_DARKGREY  0x7BEF  // Màu xám đậm

void drawBackgroundRacing();
void drawCar(int x, int y, uint16_t color);
void delCar(int x, int y);
void drawBarrier(int x, int y);
void delBarrier(int x, int y);
void initGameRacing();
bool runGameRacing();
void gameOverRacing();
void updateRacingHighScores();

#endif