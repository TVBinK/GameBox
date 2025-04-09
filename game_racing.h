#ifndef GAME_RACING_H
#define GAME_RACING_H

#include <TFT_eSPI.h>
#include "game_config.h"  // Nếu bạn đã tạo tệp này trước đó

extern TFT_eSPI display;

#define CAR_RED 0xF800       // Màu đỏ cho xe người chơi 
#define ENEMY_YELLOW 0xFFE0  // Màu vàng cho xe đối thủ 
#define ROAD_GRAY 0x4208     // Màu xám cho đường
#define ROAD_DARK 0x2104     // Màu xám đậm cho nền
#define LANE_COUNT 4         // 4 làn đường
#define CAR_WIDTH 36         // Kích thước xe khớp với bitmap (32px chiều rộng)
#define CAR_HEIGHT 52        // Kích thước xe khớp với bitmap (32px chiều cao)
#define ENEMY_SPEED 4        // tốc dộ xe di chuyển là 4
#define SCREEN_WIDTH 240     // Chiều rộng 240px
#define SCREEN_HEIGHT 320    // Chiều cao 320px
#define ENEMY_COUNT 6        // 6 xe đối thủ
// Định nghĩa màu sắc cho nền đua xe
#define TFT_DARKRED   0x8000  // Màu đỏ đậm (R=128, G=0, B=0)
#define TFT_DARKGREY  0x7BEF  // Màu xám đậm (R=128, G=128, B=128)

void drawBackgroundRacing();
void drawCar(int x, int y, uint16_t color);
void delCar(int x, int y);
void initGameRacing();
bool runGameRacing();
void gameOverRacing();

#endif