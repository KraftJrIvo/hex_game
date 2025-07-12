#include <array>

#include "raylib.h"

#include "util/arena.h"
#include "raymath.h"

#define WINDOW_WIDTH   800
#define WINDOW_HEIGHT  800
#define BOARD_WIDTH    9
#define BOARD_HEIGHT   36
#define MAX_PARTICLES  1024
#define MAX_TODROP     1024

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
    int nFulRowsTop = 0;
    std::array<std::array<Tile, BOARD_WIDTH>, BOARD_HEIGHT> things;
    bool even = false;
    double moveTime, totalMoveTime;
};

struct Gun {
    float speed;
    float dir = 0;
    Thing armed;
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
};

struct GameState {
    unsigned int seed;
    Board board;
    Gun gun;
    Arena<MAX_PARTICLES, Particle> particles;
    Bullet bullet;
    int n_params = 2;
    Texture2D tiles;
};

#ifndef GAME_BASE_DLL
extern "C" {
    void init(GameState& gs);
    void updateAndDraw(GameState& gs);
}
#endif
