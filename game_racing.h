#ifndef GAME_RACING_H
#define GAME_RACING_H

#include <TFT_eSPI.h>
#include "game_config.h"  // Nếu bạn đã tạo tệp này trước đó

extern TFT_eSPI display;

#define CAR_RED 0xF800       // Màu đỏ cho xe người chơi (dùng khi không cần bitmap đa màu)
#define ENEMY_YELLOW 0xFFE0  // Màu vàng cho xe đối thủ (dùng khi không cần bitmap đa màu)
#define ROAD_GRAY 0x4208     // Màu xám cho đường
#define ROAD_DARK 0x2104     // Màu xám đậm cho nền
#define LANE_COUNT 4         // 4 làn đường
#define CAR_WIDTH 20         // Kích thước xe khớp với bitmap (32px chiều rộng)
#define CAR_HEIGHT 30        // Kích thước xe khớp với bitmap (32px chiều cao)
#define ENEMY_SPEED 4
#define SCREEN_WIDTH 240     // Chiều rộng 240px
#define SCREEN_HEIGHT 320    // Chiều cao 320px
#define ENEMY_COUNT 6        // 6 xe đối thủ

void drawBackgroundRacing();
void drawCar(int x, int y, uint16_t color);
void delCar(int x, int y);
void initGameRacing();
bool runGameRacing();
void gameOverRacing();

#endif