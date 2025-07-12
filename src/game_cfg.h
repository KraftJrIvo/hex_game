#include <array>
#include <vector>

#include "raylib.h"

#define BOARD_EMP_BOT_ROW_GAP 5
#define RAND_FLOAT static_cast <float> (rand()) / (static_cast <float> (RAND_MAX) + 1.0f)
#define UPDATE_ITS  5
#define ROW_HEIGHT (float)(TILE_RADIUS * sqrt(3))
#define BOARD_MOVE_TIME_PER_LINE 5.0f
#ifdef PLATFORM_ANDROID
    #define BULLET_SPEED 4500.0f
#else
    #define BULLET_SPEED 1500.0f
#endif
#define BULLET_RADIUS_V TILE_RADIUS
#define BULLET_RADIUS_H TILE_RADIUS * 0.6f
#define BULLET_REBOUNCE TILE_RADIUS
#define BULLET_REBOUNCE_TIME 0.25f
#define GUN_START_SPEED 0.5f
#define GUN_FULL_SPEED 2.0f
#define GUN_ACC 10.0f
#define SHAKE_DEPTH 5
#define SHAKE_STR TILE_RADIUS * 0.5f
#define SHAKE_TIME BULLET_REBOUNCE_TIME
#define MAX_SHAKE 0.25f
#define BOARD_ACC 0.0f
#define GRAVITY 2500.0f
#define COLORS std::array<Color, 5>{ RED, GREEN, BLUE, ORANGE, PINK }
#define TOGOI std::vector<int>{1, 0, 3, 2, 5, 4}
#define TOGO std::vector<ThingPos>{{pos.row, pos.col + 1}, {pos.row, pos.col - 1}, {pos.row - 1, pos.col}, {pos.row + 1, pos.col}, {pos.row - 1, ((pos.row + gs.board.even) % 2) ? (pos.col + 1) : (pos.col - 1)}, {pos.row + 1, ((pos.row + gs.board.even) % 2) ? (pos.col + 1) : (pos.col - 1)}}
#define N_TO_DROP 4