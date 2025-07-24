#include <array>
#include <cstdint>

#include "raylib.h"

#include "util/arena.h"
#include "raymath.h"
#include "game_cfg.h"

#ifdef GAME_BASE_DLL    
#define DO_NOT_SERIALIZE friend zpp::bits::access; using serialize = zpp::bits::members<0>;        
#else
#define DO_NOT_SERIALIZE ;
#endif

struct ThingPos {
    int row, col;
};

struct Thing {
    unsigned char clr, shp, sym;
    bool bomb = false;
    double triggerTime;
    bool triggered = false;
};

struct Tile {
    bool exists;
    ThingPos pos;
    Thing thing;
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
    Arena<MAX_TODROP, ThingPos> todrop;
    Arena<MAX_TODROP, ThingPos> uncon;
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
    bool masked = false;
    ThingPos maskTilesStartPos;
    uint8_t maskId1;
    uint8_t maskId2;
};

struct Animation {
    const Texture2D* tex;
    double startTime;
    double interval;
    Vector2 pos;
    bool done = false;
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
};

struct GameAssets {
    Texture2D tiles;
    Texture2D explosion;
    Texture2D splash;
    Font font;
    Music music;
    Sound clang[3];
    Sound sndexp;
    Sound shatter;
    Sound whoosh[2];
    Sound sizzle;
    Shader postProcFragShader;
    Shader maskFragShader;
};

struct GameState {
    unsigned int seed;
    Board board;
    Gun gun;
    Bullet bullet;
    int score = 0;
    int combo = 1;
    bool firstShotFired = false;
    bool gameOver = false;
    double time;
    double gameStartTime;
    double gameOverTime;
    double inputTimeoutTime;
    double rearmTime;
    double swapTime;
    bool musicLoopDone = false;
    bool settingsOpened = false;
    bool alteredDifficulty = false;
    struct UserData {
        DO_NOT_SERIALIZE
        int bestScore = 0;
        int n_params = 2;
        bool musEnabled = true;
        bool sndEnabled = true;
        bool accEnabled = true;
        bool velEnabled = true;
        bool operator==(const UserData&) const = default;
    } usr;
    struct Temp {
        DO_NOT_SERIALIZE
        Arena<MAX_PARTICLES, Particle> particles;
        Arena<MAX_PARTICLES, Animation> animations;
        bool timeOffsetSet = false;
        double timeOffset;
        RenderTexture2D renderTex;
        uint32_t shNDrops = 0;
        float shTime;
        float shFadeTime = WAVE_FADE_TIME;
        Vector2 shScreenSize;
        std::array<float, 128> shDropTimes;
        std::array<Vector2, 128> shDropCenters;
        Vector2 shMaskTilePos;
        uint32_t shMaskId;
    } tmp;
};

#ifndef GAME_BASE_DLL
extern "C" {
    void init(GameAssets& ga, GameState& gs);
    void setState(GameState& gs, const GameState& ngs);
    void updateAndDraw(const GameAssets& ga, GameState& gs);
}
#endif
