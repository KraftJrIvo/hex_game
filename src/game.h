#include <array>

#include "raylib.h"

#include "util/arena.h"
#include "raymath.h"
#include "game_cfg.h"

struct ThingPos {
    int row, col;
};

struct ThingRef {
    bool exists;
    ThingPos pos;
};

struct Thing {
    unsigned char clr, shp, sym;
};

struct Tile {
    Thing thing;
    ThingRef ref;
    std::array<ThingRef, 6> neighs;
    float shake = 0.0f;
};

struct Board {
    float pos = 0;
    float speed = BOARD_SPEED;
    int nFulRowsTop = 0;
    int nRowsGap = BOARD_EMP_BOT_ROW_GAP;
    std::array<std::array<Tile, BOARD_WIDTH>, BOARD_HEIGHT> things;
    bool even = false;
    double moveTime, totalMoveTime;
    bool accEnabled = true;
    bool velEnabled = true;
};

struct Gun {
    float speed;
    float dir = 0;
    Thing armed;
    bool extraArmed = false;
    bool firstSwap = true;
    Thing extra;
    bool nextArmed = false;
    Thing next;
};

struct Particle {
    bool exists = true;
    Thing thing;
    Vector2 pos = Vector2Zero();
    Vector2 vel = Vector2Zero();
    bool bounced = false;
};

struct Bullet {
    bool exists = false;
    Thing thing;
    Vector2 pos = Vector2Zero();
    Vector2 vel = Vector2Zero();
    ThingPos lstEmp;
    bool rebouncing = false;
    float rebounce;
    Vector2 rebCp, rebEnd;
    double rebTime;
    Arena<MAX_TODROP, ThingPos> todrop;
    Arena<MAX_TODROP, ThingPos> uncon;
};

struct GameAssets {
    Texture2D tiles;
    Font font;
};

struct GameState {
    unsigned int seed;
    Board board;
    Gun gun;
    Arena<MAX_PARTICLES, Particle> particles;
    Bullet bullet;
    int n_params = 2;
    int score = 0;
    bool firstShotFired = false;
    bool gameOver = false;
    double time;
    double gameStartTime;
    double gameOverTime;
    double focusTime;
    double rearmTime;
    double swapTime;
    struct Temp {
        bool timeOffsetSet = false;
        double timeOffset;
    } tmp;
};

#ifndef GAME_BASE_DLL
extern "C" {
    void init(GameAssets& ga, GameState& gs);
    void updateAndDraw(const GameAssets& ga, GameState& gs);
}
#endif
