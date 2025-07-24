#include <array>
#include <vector>

#include "raylib.h"

#define WINDOW_WIDTH   432
#define WINDOW_HEIGHT  864
#define BOARD_WIDTH    9
#define BOARD_HEIGHT   36
#define TILE_SIZE      16.0f
#define TILE_RADIUS    std::min(GetScreenWidth(), GetScreenHeight()) / (BOARD_WIDTH * 2.0f)
#define TILE_PIXEL     (TILE_RADIUS * 2.0f) / TILE_SIZE
#define MAX_PARTICLES  1024
#define MAX_TODROP     1024

#define BOARD_EMP_BOT_ROW_GAP 10
#define BOARD_WARNING_GAP 3
#define BOARD_SPEED 1.0f
#define BOARD_CONST_SPEED 3.0f
#define BOARD_ACC 0.01f
#define RAND_FLOAT static_cast <float> (rand()) / (static_cast <float> (RAND_MAX) + 1.0f)
#define RAND_FLOAT_SIGNED (2.0f * RAND_FLOAT - 1.0f)
#define RAND_FLOAT_SIGNED_2D Vector2{RAND_FLOAT_SIGNED, RAND_FLOAT_SIGNED}
#define UPDATE_ITS  5
#define ROW_HEIGHT (float)(TILE_RADIUS * sqrt(3))
#define BOARD_MOVE_TIME_PER_LINE 5.0f
#define GAME_START_TIME 1.0f
#define GAME_OVER_TIME_PER_ROW 0.1f
#define GAME_OVER_TIMEOUT 3.0f
#define GAME_OVER_TIMEOUT_BEF 1.0f
#define BULLET_SPEED 75.0f * TILE_RADIUS
#define BULLET_RADIUS_V TILE_RADIUS
#define BULLET_RADIUS_H TILE_RADIUS * 0.5f
#define BULLET_REBOUNCE TILE_RADIUS
#define BULLET_HIT_DIST_SQR (TILE_RADIUS + BULLET_RADIUS_H) * (TILE_RADIUS + BULLET_RADIUS_H)
#define BULLET_REBOUNCE_TIME 0.25f
#define GUN_START_SPEED 0.5f
#define GUN_FULL_SPEED 2.0f
#define GUN_ACC 10.0f
#define SHAKE_DEPTH 5
#define SHAKE_STR TILE_RADIUS * 0.5f
#define SHAKE_TIME BULLET_REBOUNCE_TIME
#define MAX_SHAKE 0.25f
#define GRAVITY 2500.0f
#define COLORS std::array<Color, 5>{ RED, GREEN, BLUE, ORANGE, PINK }
#define TOGOI std::vector<int>{1, 0, 3, 2, 5, 4}
#define TOGO std::vector<ThingPos>{{pos.row, pos.col + 1}, {pos.row, pos.col - 1}, {pos.row - 1, pos.col}, {pos.row + 1, pos.col}, {pos.row - 1, ((pos.row + gs.board.even) % 2) ? (pos.col + 1) : (pos.col - 1)}, {pos.row + 1, ((pos.row + gs.board.even) % 2) ? (pos.col + 1) : (pos.col - 1)}}
#ifdef PLATFORM_ANDROID
    #define INPUT_TIMEOUT 1.0f
#else
    #define INPUT_TIMEOUT 0.1f
#endif
#define REARM_TIMEOUT 0.25f
#define N_TO_DROP 4
#define WAVE_FADE_TIME 1.0f
#define BOMB_PROB 0.1f
#define BOMB_TRIGGER_TIME 0.5f
#define EXPLOSION_TIME 0.4f
#define SPLASH_TIME 0.2f