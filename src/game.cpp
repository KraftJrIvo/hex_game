#include "game.h"
#include "raylib.h"
#include "rlgl.h"

#include "util/vec_ops.h"
#include "raymath.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

extern "C" const unsigned char res_tiles[];
extern "C" const size_t        res_tiles_len;
extern "C" const unsigned char res_explosion[];
extern "C" const size_t        res_explosion_len;
extern "C" const unsigned char res_splash[];
extern "C" const size_t        res_splash_len;
extern "C" const unsigned char res_font[];
extern "C" const size_t        res_font_len;
extern "C" const unsigned char res_music[];
extern "C" const size_t        res_music_len;
extern "C" const unsigned char res_clang0[];
extern "C" const size_t        res_clang0_len;
extern "C" const unsigned char res_clang1[];
extern "C" const size_t        res_clang1_len;
extern "C" const unsigned char res_clang2[];
extern "C" const size_t        res_clang2_len;
extern "C" const unsigned char res_sndexp[];
extern "C" const size_t        res_sndexp_len;
extern "C" const unsigned char res_shatter[];
extern "C" const size_t        res_shatter_len;
extern "C" const unsigned char res_whoosh0[];
extern "C" const size_t        res_whoosh0_len;
extern "C" const unsigned char res_whoosh1[];
extern "C" const size_t        res_whoosh1_len;
extern "C" const unsigned char res_sizzle[];
extern "C" const size_t        res_sizzle_len;
extern "C" const unsigned char res_ppfs[];
extern "C" const unsigned char res_mskfs[];

#if (defined(_WIN32) || defined(_WIN64)) && defined(GAME_BASE_DLL)
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

bool checkBounds(const GameState& gs, const ThingPos& pos) {
    return (pos.row >= 0 && pos.row < BOARD_HEIGHT && pos.col >= 0 && pos.col < (((pos.row + gs.board.even) % 2) ? (BOARD_WIDTH - 1) : (BOARD_WIDTH)));
}

Tile& getTile(GameState& gs, const ThingPos& pos) {
    return gs.board.things[pos.row][pos.col];
}

std::vector<ThingPos> getNeighs(GameState& gs, const ThingPos& pos) {
    std::vector<ThingPos> res;
    for (int i = 0; i < 6; ++i)
        if (checkBounds(gs, TOGO[i]))
            res.push_back(TOGO[i]);
    return res;
}

extern "C" {

    int getRandVal(GameState& gs, int min, int max) {
        SetRandomSeed(gs.seed++);
        return GetRandomValue(min, max);
    }

    double getTime(const GameState& gs) {
        return (GetTime() + gs.tmp.timeOffset);// * 0.5f;
    }

    float getFrameTime(const GameState& gs) {
        return GetFrameTime();// * 0.5f;
    }

    void playSound(const GameAssets& ga, const GameState& gs, const Sound& snd) {
        if (gs.usr.sndEnabled)
            PlaySound(snd);
    }

    float easeOutBounce(float x) 
    {
        float n1 = 7.5625f;
        float d1 = 2.75f;

        if (x < 1 / d1) {
            return n1 * x * x;
        } else if (x < 2 / d1) {
            return n1 * (x - 1.5 / d1) * (x - 1.5 / d1) + 0.75;
        } else if (x < 2.5 / d1) {
            return n1 * (x - 2.25 / d1) * (x - 2.25 / d1) + 0.9375;
        } else {
            return n1 * (x - 2.625 / d1) * (x - 2.625 / d1) + 0.984375;
        }
    }

    float easeOutQuad(float t) {
        return 1 - (1 - t) * (1 - t);
    }

    Rectangle getBoardRect(const GameState& gs) {
        float bWidth = TILE_RADIUS * 2 * BOARD_WIDTH;
        float bHeight = ROW_HEIGHT * BOARD_HEIGHT;
        float startCoeff = easeOutQuad(std::clamp((getTime(gs) - gs.gameStartTime)/GAME_START_TIME, 0.0, 1.0));
        Vector2 bPos = {(GetScreenWidth() - bWidth) * 0.5f, (float)GetScreenHeight() - 2 * bHeight + bHeight * startCoeff + gs.board.pos};
        return {float(int(bPos.x)), float(int(bPos.y)), bWidth, bHeight};
    }

    ThingPos getPosByPix(const GameState& gs, const Vector2& pix) {
        auto brec = getBoardRect(gs);
        int row = std::clamp((int)floor((pix.y - brec.y) / ROW_HEIGHT), 0, BOARD_HEIGHT - 1);
        bool shortRow = ((row + gs.board.even) % 2);
        int col = std::clamp((int)floor((pix.x - brec.x - float(shortRow) * TILE_RADIUS) / (TILE_RADIUS * 2)), 0, shortRow ? (BOARD_WIDTH - 2) : (BOARD_WIDTH - 1));
        return {row, col};
    }

    Vector2 getPixByPos(const GameState& gs, const ThingPos& pos) {
        auto brec = getBoardRect(gs);
        float offset = float((pos.row + gs.board.even) % 2) * TILE_RADIUS;
        return {float(int(offset + brec.x + TILE_RADIUS + pos.col * TILE_RADIUS * 2)), (float)int(brec.y + (pos.row + 0.5f) * ROW_HEIGHT)};
    }

    int countBotEmpRows(const GameState& gs) {
        int n = 0;
        bool keep = true;
        for (int row = BOARD_HEIGHT - 1; row >= 0; --row) {
            for (int col = 0; col < BOARD_WIDTH - ((row + gs.board.even) % 2); ++col) {
                if (gs.board.things[row][col].exists) {
                    keep = false;
                    break;
                }
            }
            if (!keep) break;
            n++;
        }
        return n;
    }

    bool checkFullRow(const GameState& gs, int row) {
        bool fullrow = true;
        for (int i = 0; i < BOARD_WIDTH - ((row + gs.board.even) % 2); ++i) {
            if (!gs.board.things[row][i].exists) {
                fullrow = false;
                break;
            }
        }
        return fullrow;
    }

    void addTile(GameState& gs, const ThingPos& pos, const Tile& tile, bool updateFullRows = true, bool makeExist = false) {
        auto& th = gs.board.things[pos.row][pos.col];
        th = tile;
        if (makeExist) th.exists = true;
        th.pos = pos;

        if (updateFullRows) {
            int i = 0;
            while ((pos.row - i > 0) && checkFullRow(gs, pos.row - i)) i++;
            if (i > 0 && pos.row - i <= gs.board.nFulRowsTop)
                gs.board.nFulRowsTop = pos.row + 1;
        }
    }

    void addShatteredParticles(GameState& gs, const Thing& thing, Vector2 pos) {
        uint8_t mskId1 = getRandVal(gs, 0, 2);
        for (uint8_t mskId2 = 0; mskId2 < 5; ++mskId2) {
            Vector2 vel;
            if (mskId2 == 0) vel = {0, -1};
            else if (mskId2 == 1) vel = {-cos(PI*0.25f), -cos(PI*0.25f)};
            else if (mskId2 == 2) vel = {1, 0};
            else if (mskId2 == 3) vel = {0, 1};
            else if (mskId2 == 4) vel = {-cos(PI*0.25f), cos(PI*0.25f)};
            gs.tmp.particles.acquire(Particle{true, thing, pos, 200.0f * vel + Vector2{200 * RAND_FLOAT_SIGNED, -200 - 200 * RAND_FLOAT}, false, true, {6, 0}, mskId1, mskId2});
        }
    }

    void addParticle(GameState& gs, const Thing& thing, Vector2 pos, Vector2 vel) {
        gs.tmp.particles.acquire(Particle{true, thing, pos, vel});
    }

    void generateRows(GameState& gs, int n) {
        for (int row = 0; row < n; ++row) {
            for (int col = 0; col < BOARD_WIDTH - ((row + gs.board.even) % 2); ++col) {
                addTile(gs, {row, col}, Tile{(col != (BOARD_WIDTH - 1)) || ((row + gs.board.even) % 2 == 0), {row, col}, {(unsigned char)getRandVal(gs, 0, COLORS.size() - 1), (unsigned char)getRandVal(gs, 0, COLORS.size() - 1), (unsigned char)getRandVal(gs, 0, COLORS.size() - 1)}});
                auto& thing = gs.board.things[row][col].thing;
                thing.bomb = (getRandVal(gs, 0, 100000) < 100000 * BOMB_PROB);
                thing.triggered = false;
            }
        }
    }

    void removeTile(GameState& gs, const ThingPos& pos) {
        gs.board.things[pos.row][pos.col].exists = false;
        gs.board.things[pos.row][pos.col].pos = pos;        
        if (pos.row < gs.board.nFulRowsTop)
            gs.board.nFulRowsTop = pos.row + 1;
    }

    void shiftBoard(GameState& gs, int off) {
        if (off % 2 != 0)
            gs.board.even = !gs.board.even;
        if (off < 0) {
            for (int row = 0; row < BOARD_HEIGHT - 1; ++row) {
                for (int col = 0; col < BOARD_WIDTH - ((row + gs.board.even) % 2); ++col) {
                    if (row > BOARD_HEIGHT + off - 1)
                        removeTile(gs, {row, col});
                    else
                        addTile(gs, {row, col}, gs.board.things[row - off][col]);

                }
            }
        } else {
            for (int row = BOARD_HEIGHT - 1; row >= 0; --row) {
                for (int col = 0; col < BOARD_WIDTH - ((row + gs.board.even) % 2); ++col) {
                    if (row < off)
                        removeTile(gs, {row, col});
                    else
                        addTile(gs, {row, col}, gs.board.things[row - off][col]);
                }
            }
        }
    }

    void setNext(GameState& gs) {
        gs.gun.next.shp = (unsigned char)getRandVal(gs, 0, COLORS.size() - 1);
        gs.gun.next.clr = (unsigned char)getRandVal(gs, 0, COLORS.size() - 1);
        gs.gun.next.sym = (unsigned char)getRandVal(gs, 0, COLORS.size() - 1);
        gs.gun.nextArmed = true;
    }

    void rearm(GameState& gs) {
        if (!gs.gun.nextArmed)
            setNext(gs);
        gs.gun.armed = gs.gun.next;
        setNext(gs);
        gs.rearmTime = getTime(gs);
    }

    void swapExtra(const GameAssets& ga, GameState& gs) {
        if (gs.gun.extraArmed) {
            auto e = gs.gun.extra;
            gs.gun.extra = gs.gun.armed;
            gs.gun.armed = e;
            gs.gun.firstSwap = false;
        } else {
            gs.gun.extra = gs.gun.armed;
            gs.gun.extraArmed = true;
            rearm(gs);
        }
        playSound(ga, gs, ga.whoosh[1]);
        gs.swapTime = getTime(gs);
    }

    void loadAssets(GameAssets& ga) {
        ga.tiles = LoadTextureFromImage(LoadImageFromMemory(".png", res_tiles, res_tiles_len));
        ga.explosion = LoadTextureFromImage(LoadImageFromMemory(".png", res_explosion, res_explosion_len));
        ga.splash = LoadTextureFromImage(LoadImageFromMemory(".png", res_splash, res_splash_len));

        ga.music = LoadMusicStreamFromMemory(".ogg", res_music, res_music_len);

        ga.clang[0] = LoadSoundFromWave(LoadWaveFromMemory(".ogg", res_clang0, res_clang0_len));
        ga.clang[1] = LoadSoundFromWave(LoadWaveFromMemory(".ogg", res_clang1, res_clang1_len));
        ga.clang[2] = LoadSoundFromWave(LoadWaveFromMemory(".ogg", res_clang2, res_clang2_len));
        ga.sndexp = LoadSoundFromWave(LoadWaveFromMemory(".ogg", res_sndexp, res_sndexp_len));
        ga.shatter = LoadSoundFromWave(LoadWaveFromMemory(".ogg", res_shatter, res_shatter_len));
        ga.whoosh[0] = LoadSoundFromWave(LoadWaveFromMemory(".ogg", res_whoosh0, res_whoosh0_len));
        ga.whoosh[1] = LoadSoundFromWave(LoadWaveFromMemory(".ogg", res_whoosh1, res_whoosh1_len));
        ga.sizzle = LoadSoundFromWave(LoadWaveFromMemory(".ogg", res_sizzle, res_sizzle_len));

        ga.postProcFragShader = LoadShaderFromMemory(NULL, (const char*)res_ppfs);
        ga.maskFragShader = LoadShaderFromMemory(NULL, (const char*)res_mskfs);

        char8_t _allChars[228] = u8" !\"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ";
        int c; auto cdpts = LoadCodepoints((const char*)_allChars, &c);
        ga.font = LoadFontFromMemory(".ttf", res_font, res_font_len, 39, cdpts, c);
    }

    DLL_EXPORT void saveUserData(const GameState& gs) {
        SaveFileData("userdata", (void*)&gs.usr, sizeof(GameState::UserData));
    }

    DLL_EXPORT void loadUserData(GameState& gs) {
        int sz = sizeof(GameState::UserData);
        std::vector<unsigned char> buffer(sz);
        int datasz;
        unsigned char* ptr = LoadFileData("userdata", &datasz);
        if (ptr && datasz == sz)
            gs.usr = *((GameState::UserData*)ptr);
    }

    void setStuff(GameState& gs) {
        loadUserData(gs);
        gs.tmp.renderTex = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        SetTextureWrap(gs.tmp.renderTex.texture, TEXTURE_WRAP_CLAMP);
    }

    DLL_EXPORT void setState(GameState& gs, const GameState& ngs)
    {
        gs = ngs;
        setStuff(gs);
    }

    void reset(const GameAssets& ga, GameState& gs) {
        gs = GameState{0};
        gs.seed = rand() % std::numeric_limits<int>::max();
        for (int i = 0; i < gs.board.things.size(); ++i)
            std::fill(gs.board.things[i].begin(), gs.board.things[i].end(), Tile());
        generateRows(gs, BOARD_HEIGHT - gs.board.nRowsGap);
        rearm(gs);        
        gs.gameStartTime = getTime(gs);
        setState(gs, gs);
    }

    DLL_EXPORT void init(GameAssets& ga, GameState& gs)
    {
        if (!IsAudioDeviceReady()) {
            InitAudioDevice();
        } else if (IsMusicStreamPlaying(ga.music)) {
            StopMusicStream(ga.music);
        }

        loadAssets(ga);
        PlayMusicStream(ga.music);
        
        reset(ga, gs);
    }

    void shootAndRearm(const GameAssets& ga, GameState& gs) {
        gs.firstShotFired = true;
        gs.bullet.exists = true;
        gs.bullet.rebouncing = false;
        float dir = gs.gun.dir + PI * 0.5f;
        gs.bullet.thing = gs.gun.armed;
        gs.bullet.vel = BULLET_SPEED * Vector2{cos(dir), -sin(dir)};
        gs.bullet.pos = {(float)GetScreenWidth() * 0.5f, (float)GetScreenHeight() - TILE_RADIUS};
        playSound(ga, gs, ga.whoosh[0]);
        rearm(gs);
    }

    bool checkMatch(const Thing& th1, const Thing& th2, int param) {
        if (th1.bomb || th2.bomb)
            return false;
        switch (param) {
            case 0: return th1.clr == th2.clr;
            case 1: return th1.shp == th2.shp;
            case 2: return th1.sym == th2.sym;
        }
        return false;
    }

    void checkDropRecur(GameState& gs, const ThingPos& pos, const Thing& thing, int param, Arena<MAX_TODROP, ThingPos>& todrop, std::map<int, std::map<int, bool>>& visited, bool first = true) 
    {
        if (visited.count(pos.row) && visited[pos.row].count(pos.col))
            return;
        visited[pos.row][pos.col] = true;
        if (checkBounds(gs, pos)) {
            const auto& tile = getTile(gs, pos);
            bool match = checkMatch(tile.thing, thing, param);
            if ((tile.exists && match) || (first && !tile.exists)) {
                if (tile.exists && match) todrop.acquire(pos);
                for (auto& n : getNeighs(gs, pos))
                    if (getTile(gs, n).exists) checkDropRecur(gs, n, thing, param, todrop, visited, false);
            }
        }
    }

    bool isConnectedToTopRecur(GameState& gs, const ThingPos& pos, std::map<int, std::map<int, bool>>& visited)
    {
        if (visited.count(pos.row) && visited[pos.row].count(pos.col))
            return false;
        if (pos.row == 21 && pos.col == 5)
            std::cout << "\n";
        visited[pos.row][pos.col] = true;
        if (checkBounds(gs, pos)) {
            auto& tile = getTile(gs, pos);
            if (tile.exists) {
                bool connected = (pos.row == gs.board.nFulRowsTop - 1);
                for (auto& n : getNeighs(gs, pos))
                    if (getTile(gs, n).exists && !connected) connected |= isConnectedToTopRecur(gs, n, visited);
                visited[pos.row][pos.col] = connected;
                return connected;
            }
        }
        return false;
    }

    void checkUnconnectedRecur(GameState& gs, const ThingPos& pos, std::map<int, std::map<int, bool>>& visited, Arena<MAX_TODROP, ThingPos>& uncon, bool check = true)
    {
        if (visited.count(pos.row) && visited[pos.row].count(pos.col))
            return;
        if (pos.row == 20 && pos.col == 6)
            std::cout << "\n";
        visited[pos.row][pos.col] = true;
        std::map<int, std::map<int, bool>> visCon;
        if (checkBounds(gs, pos) && (!check || !isConnectedToTopRecur(gs, pos, visCon))) {
            auto& tile = getTile(gs, pos);
            if (tile.exists) {
                uncon.acquire(pos);
                for (auto& n : getNeighs(gs, pos))
                    if (getTile(gs, n).exists) checkUnconnectedRecur(gs, n, visited, uncon, false);
            }
        }
    }

    void addShakeRecur(GameState& gs, const ThingPos& pos, std::map<int, std::map<int, bool>>& visited, const Thing& thing, int param, float shake, int depth, int curdepth = 0, bool mtchstreak = true)
    {
        if (visited.count(pos.row) && visited[pos.row].count(pos.col) || curdepth >= depth)
            return;
        visited[pos.row][pos.col] = true;
        auto& tile = getTile(gs, pos);
        if (tile.exists && curdepth == 0) tile.shake = std::max(tile.shake, shake / (curdepth + 1));
        if (tile.exists || curdepth == 0) {
            for (int i = 0; i < 6; ++i) {
                if (checkBounds(gs, TOGO[i])) {
                    auto& n = getTile(gs, TOGO[i]);
                    bool match = checkMatch(n.thing, thing, param);
                    bool samecolor = (mtchstreak && match);
                    if (n.exists) 
                        n.shake = std::max(n.shake, samecolor ? shake : (shake / (curdepth + 2)));
                }
            }
            if (mtchstreak) {
                for (int i = 0; i < 6; ++i) {
                    if (checkBounds(gs, TOGO[i])) {
                        auto& n = getTile(gs, TOGO[i]);
                        bool match = checkMatch(n.thing, thing, param);
                        if (n.exists && match) 
                            addShakeRecur(gs, n.pos, visited, thing, param, shake, depth, curdepth, true);
                    }
                }
            }
            if (!mtchstreak || curdepth == 0) {
                for (int i = 0; i < 6; ++i) {
                    if (checkBounds(gs, TOGO[i])) {
                        auto& n = getTile(gs, TOGO[i]);
                        bool match = checkMatch(n.thing, thing, param);
                        if (n.exists && !match) 
                            addShakeRecur(gs, n.pos, visited, thing, param, shake, depth, curdepth + 1, false);
                    }
                }
            }
        }
    }

    void checkLines(GameState& gs) {
        auto extraRows = countBotEmpRows(gs) - gs.board.nRowsGap;
        if (extraRows > 0) {
            shiftBoard(gs, extraRows);
            generateRows(gs, extraRows);
            gs.board.pos -= ROW_HEIGHT * extraRows;
            gs.board.moveTime = gs.board.totalMoveTime = BOARD_MOVE_TIME_PER_LINE * extraRows;
        }
    }

    void scorePoints(GameState& gs, int amount) {
        gs.score += amount * gs.combo;
    }

    void addDrop(GameState& gs, Vector2 pos) {
        gs.tmp.shDropCenters[gs.tmp.shNDrops] = pos;
        gs.tmp.shDropTimes[gs.tmp.shNDrops] = getTime(gs);
        gs.tmp.shNDrops++;
    }

    void addAnimation(const GameAssets& ga, GameState& gs, const Texture2D* tex, float interval, Vector2 pos) {
        gs.tmp.animations.acquire(Animation{tex, getTime(gs), interval, pos});
    }

    void triggerBomb(const GameAssets& ga, GameState& gs, const ThingPos& pos) {
        auto& thing = getTile(gs, pos).thing;
        thing.triggered = true;
        thing.triggerTime = getTime(gs);
        gs.bullet.exists = false;
        playSound(ga, gs, ga.sizzle);
        addParticle(gs, gs.bullet.thing, gs.bullet.pos, {-gs.bullet.vel.x, -400.0f - 100.0f * RAND_FLOAT});
        scorePoints(gs, 1);
    }

    void checkDrop(GameState& gs, const ThingPos& pos, const Thing& thing, int minToDrop = 0) {
        int bestK = 0, bestScore = 0;
        Arena<MAX_TODROP, ThingPos> todrops[3];
        Arena<MAX_TODROP, ThingPos> uncons[3];
        auto exists = getTile(gs, pos).exists;
        int lim = (exists ? minToDrop : (minToDrop - 1));
        for (int k = 0; k < gs.usr.n_params; ++k) {
            std::map<int, std::map<int, bool>> vis;
            checkDropRecur(gs, pos, thing, k, todrops[k], vis);
            int count = todrops[k].count();
            if (count >= lim) {
                for (int i = 0; i < todrops[k].count(); ++i)
                    removeTile(gs, todrops[k].at(i));
                std::map<int, std::map<int, bool>> vis2;
                for (int i = 0; i < todrops[k].count(); ++i) {
                    auto& td = todrops[k].at(i);
                    auto& tile = getTile(gs, td);
                    for (auto& n : getNeighs(gs, td)) {
                        if (getTile(gs, n).exists)
                            checkUnconnectedRecur(gs, n, vis2, uncons[k]);
                    }
                }
                for (int i = 0; i < todrops[k].count(); ++i)
                    addTile(gs, todrops[k].at(i), getTile(gs, todrops[k].at(i)), true, true);
                if (!exists) todrops[k].acquire(pos);
            }
            int score = todrops[k].count() + uncons[k].count();
            if (bestScore < score) {
                bestScore = score;
                bestK = k;
            }
        }
        std::map<int, std::map<int, bool>> vis2;
        addShakeRecur(gs, pos, vis2, thing, bestK, SHAKE_TIME, SHAKE_DEPTH);
        gs.board.todrop = todrops[bestK];
        gs.board.uncon = uncons[bestK];
    }

    void explodeBomb(const GameAssets& ga, GameState& gs, const ThingPos& pos_);

    void doDrop(const GameAssets& ga, GameState& gs, int minToDrop = 0, bool shatter = true, Vector2 vel = Vector2Zero()) {
        if (gs.board.todrop.count() >= minToDrop) {
            scorePoints(gs, gs.board.todrop.count() + gs.board.uncon.count());
            for (int i = 0; i < gs.board.todrop.count(); ++i) {
                auto& td = gs.board.todrop.at(i);
                removeTile(gs, td);
                auto pixpos = getPixByPos(gs, td);
                if (shatter) {
                    addAnimation(ga, gs, &ga.splash, SPLASH_TIME, pixpos);
                    playSound(ga, gs, ga.shatter);
                    addShatteredParticles(gs, getTile(gs, td).thing, pixpos);
                } else {
                    addParticle(gs, getTile(gs, td).thing, getPixByPos(gs, td), vel);
                }
            }
            for (int i = 0; i < gs.board.uncon.count(); ++i) {                
                auto& un = gs.board.uncon.at(i);
                removeTile(gs, un);
                addParticle(gs, getTile(gs, un).thing, getPixByPos(gs, un), Vector2Zero());
            }
        }
        gs.board.todrop.clear();
        gs.board.uncon.clear();
    }

    void explodeBomb(const GameAssets& ga, GameState& gs, const ThingPos& pos) {
        auto& thing = getTile(gs, pos).thing;
        auto pixpos = getPixByPos(gs, pos);
        addDrop(gs, pixpos);
        playSound(ga, gs, ga.sndexp);
        addAnimation(ga, gs, &ga.explosion, EXPLOSION_TIME, pixpos);
        auto& tile = getTile(gs, pos);
        removeTile(gs, pos);
        for (auto& n : getNeighs(gs, pos)) {
            auto ntile = getTile(gs, n);
            if (ntile.exists) {
                if (ntile.thing.bomb) {
                    triggerBomb(ga, gs, n);
                } else {
                    checkDrop(gs, n, getTile(gs, pos).thing);
                    doDrop(ga, gs);
                }
            }            
            for (auto& nn : getNeighs(gs, n)) {
                auto nntile = getTile(gs, nn);
                if (nntile.exists) {
                    if (nntile.thing.bomb) {
                        //triggerBomb(ga, gs, nntile.pos);
                        explodeBomb(ga, gs, nntile.pos);
                    } else {
                        checkDrop(gs, nntile.pos, getTile(gs, nntile.pos).thing);
                        doDrop(ga, gs, 0, false, 300.0f * Vector2Normalize(getPixByPos(gs, nntile.pos) - pixpos));
                    }
                }
            }
        }
    }

    void checkBomb(const GameAssets& ga, GameState& gs, const ThingPos& pos) {
        auto& tile = getTile(gs, pos);
        if (tile.thing.triggered) {
            tile.shake = std::clamp((getTime(gs) - tile.thing.triggerTime)/BOMB_TRIGGER_TIME, 0.0, 1.0);
            if (getTime(gs) - tile.thing.triggerTime > BOMB_TRIGGER_TIME)
                explodeBomb(ga, gs, pos);
        }
    }

    void flyBullet(const GameAssets& ga, GameState& gs, float delta)
    {
        if (gs.bullet.exists)
            gs.bullet.pos += gs.bullet.vel * delta;
        if (gs.bullet.pos.y + TILE_RADIUS < 0)
            gs.bullet.exists = false;
        
        auto brect = getBoardRect(gs);
        auto bulpos = getPosByPix(gs, gs.bullet.pos);

        if (gs.bullet.rebouncing) {
            gs.bullet.pos = GetSplinePointBezierQuad(gs.bullet.pos - Vector2{0, gs.board.pos}, gs.bullet.rebCp, gs.bullet.rebEnd, gs.bullet.rebounce) + Vector2{0, gs.board.pos};
            float prog = (float)(getTime(gs) - gs.bullet.rebTime)/BULLET_REBOUNCE_TIME;
            if (prog > 1.0f) {
                gs.bullet.exists = false;
                addTile(gs, gs.bullet.lstEmp, Tile{true, gs.bullet.lstEmp, gs.bullet.thing});
                doDrop(ga, gs, N_TO_DROP); 
                gs.bullet.rebouncing = false;
            } else {
                gs.bullet.rebounce = easeOutBounce(prog);
            }
        } else if (gs.bullet.exists) {
            if (gs.bullet.pos.x - BULLET_RADIUS_H < brect.x || gs.bullet.pos.x + BULLET_RADIUS_H > brect.x + brect.width) {
                playSound(ga, gs, ga.clang[GetRandomValue(0, 2)]);
                addAnimation(ga, gs, &ga.splash, SPLASH_TIME, gs.bullet.pos + Vector2{gs.bullet.vel.x/abs(gs.bullet.vel.x), 0});
                gs.bullet.vel.x *= -1.0f;
            }

            if (!getTile(gs, bulpos).exists)
                gs.bullet.lstEmp = {bulpos.row, bulpos.col};
            
            for (int i = 0; i < BOARD_HEIGHT; ++i) {
                for (int j = 0; j < BOARD_WIDTH - ((i + gs.board.even) % 2); ++j) {
                    const auto& tile = gs.board.things[i][j];
                    if (tile.exists) {
                        Vector2 tpos = getPixByPos(gs, {i, j});
                        if (Vector2DistanceSqr(tpos, gs.bullet.pos) < BULLET_HIT_DIST_SQR ||
                            Vector2DistanceSqr(tpos, gs.bullet.pos + Vector2Normalize(gs.bullet.vel) * BULLET_RADIUS_H) < BULLET_HIT_DIST_SQR) {
                            playSound(ga, gs, ga.clang[GetRandomValue(0, 2)]);
                            addAnimation(ga, gs, &ga.splash, SPLASH_TIME, 0.5f * (tpos + getPixByPos(gs, gs.bullet.lstEmp)));
                            if (tile.thing.bomb) {
                                triggerBomb(ga, gs, {i, j});
                            } else {
                                checkDrop(gs, gs.bullet.lstEmp, gs.bullet.thing, N_TO_DROP);                                
                                gs.bullet.rebouncing = true;
                                gs.bullet.rebounce = 0.0f;
                                gs.bullet.rebCp = (gs.bullet.pos - Vector2Normalize(gs.bullet.vel) * BULLET_REBOUNCE)- Vector2{0, gs.board.pos};
                                gs.bullet.rebEnd = (getPixByPos(gs, gs.bullet.lstEmp)) - Vector2{0, gs.board.pos};
                                gs.bullet.rebTime = getTime(gs);
                            }
                            break;
                        }
                    }
                }
                if (gs.bullet.rebouncing)
                    break;
            }
        }
    }

    void flyParticles(GameState& gs) {
        bool someInFrame = false;
        for (int i = 0; i < gs.tmp.particles.count(); ++i) {
            auto& prt = gs.tmp.particles.at(i);
            if (prt.exists) {
                prt.pos += prt.vel * getFrameTime(gs);
                prt.vel += GRAVITY * Vector2{0.0f, 1.0f} * getFrameTime(gs);
                if (prt.pos.y < GetScreenHeight())
                    someInFrame = true;
            }
        }
        if (!someInFrame)
            gs.tmp.particles.clear();
    }

    void checkDrops(GameState& gs) {
        bool someStillGoing = false;
        for (int i = 0; i < gs.tmp.shNDrops; ++i) {
            if (getTime(gs) - gs.tmp.shDropTimes[i] < WAVE_FADE_TIME) {
                someStillGoing = true;
                break;
            }
        }
        if (!someStillGoing)
            gs.tmp.shNDrops = 0;
    }

    void checkAnimations(GameState& gs) {
        bool someStillGoing = false;
        for (int i = 0; i < gs.tmp.animations.count(); ++i) {
            auto& anim = gs.tmp.animations.at(i);
            if (!anim.done) {
                anim.done = ((getTime(gs) - anim.startTime) / anim.interval > 1.0f);
                someStillGoing = true;
            }
        }
        if (!someStillGoing)
            gs.tmp.animations.clear();
    }

    void gameOver(GameState& gs) {
        gs.gameOver = true;
        gs.gameOverTime = getTime(gs);
        gs.bullet.exists = false;
        Vector2 gunPos = {(float)GetScreenWidth() * 0.5f, (float)GetScreenHeight() - TILE_RADIUS};
        addParticle(gs, gs.gun.armed, gunPos, Vector2{50.0f * RAND_FLOAT_SIGNED, -400.0f - 100.0f * RAND_FLOAT});
        addParticle(gs, gs.gun.next, {GetScreenWidth() - TILE_RADIUS, GetScreenHeight() - TILE_RADIUS}, Vector2{50.0f * RAND_FLOAT_SIGNED, -400.0f - 100.0f * RAND_FLOAT});
        if (gs.gun.extraArmed)
            addParticle(gs, gs.gun.extra, {TILE_RADIUS, GetScreenHeight() - TILE_RADIUS}, Vector2{50.0f * RAND_FLOAT_SIGNED, -400.0f - 100.0f * RAND_FLOAT});
        if (!gs.alteredDifficulty && gs.score > gs.usr.bestScore) {
            gs.usr.bestScore = gs.score;
            saveUserData(gs);
        }
    }

    void update(const GameAssets& ga, GameState& gs) 
    {
        if (gs.gameStartTime + GAME_START_TIME < getTime(gs)) {
            auto delta = getFrameTime(gs) / UPDATE_ITS;

#ifdef PLATFORM_ANDROID
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
#else
            if (fabs(GetMouseDelta().x) > 0) {
#endif
                Vector2 gunPos = {(float)GetScreenWidth() * 0.5f, (float)GetScreenHeight() - TILE_RADIUS};
                gs.gun.dir = atan2(gunPos.y - GetMouseY(), GetMouseX() - gunPos.x) - PI * 0.5f;      
            } else if (IsKeyDown(KEY_LEFT)) {
                gs.gun.dir += gs.gun.speed * delta;
                gs.gun.speed += GUN_ACC * delta;
            } else if (IsKeyDown(KEY_RIGHT)) {
                gs.gun.dir -= gs.gun.speed * delta;
                gs.gun.speed += GUN_ACC * delta;
            } else {
                gs.gun.speed = GUN_START_SPEED;
            }
            gs.gun.speed = std::clamp(gs.gun.speed, GUN_START_SPEED, GUN_FULL_SPEED);
            gs.gun.dir = std::clamp(gs.gun.dir, -PI * 0.45f, PI * 0.45f);

            flyBullet(ga, gs, delta);
        }
    }

    void updateOnce(const GameAssets& ga, GameState& gs) 
    {   
        if (gs.gameOver) {
            for (int i = 0; i < BOARD_HEIGHT; ++i) {
                for (int j = 0; j < BOARD_WIDTH - ((i + gs.board.even) % 2); ++j) {
                    const Tile& tile = gs.board.things[i][j];
                    if (tile.exists) {
                        if ((getTime(gs) - gs.gameOverTime) > (GAME_OVER_TIME_PER_ROW * (BOARD_HEIGHT - 1 - i))) {
                            gs.board.things[i][j].exists = false;
                            Vector2 tpos = getPixByPos(gs, {i, j});
                            if (tpos.y > 0) {
                                playSound(ga, gs, ga.clang[GetRandomValue(0, 2)]);
                                addParticle(gs, gs.board.things[i][j].thing, getPixByPos(gs, {i, j}), Vector2{50.0f * RAND_FLOAT_SIGNED, -400.0f - 100.0f * RAND_FLOAT});
                            }
                        }
                    }
                }
            }
            if (getTime(gs) > gs.gameOverTime + GAME_OVER_TIMEOUT) {                
#ifdef PLATFORM_ANDROID
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
#else
                if ((IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))
#endif            
                    reset(ga, gs);
                
            }                
        } else if (gs.gameStartTime + GAME_START_TIME < getTime(gs)) {
            for (int i = 0; i < BOARD_HEIGHT; ++i) {
                for (int j = 0; j < BOARD_WIDTH - ((i + gs.board.even) % 2); ++j) {
                    Tile& tile = gs.board.things[i][j];
                    if (tile.exists) {
                        if (tile.shake < SHAKE_TIME || gs.board.todrop.count() < N_TO_DROP - 1)
                            tile.shake = std::max(tile.shake - getFrameTime(gs), 0.0f);
                        else
                            tile.shake = std::min(tile.shake + getFrameTime(gs) * 2, MAX_SHAKE);
                        Vector2 tpos = getPixByPos(gs, {i, j});
                        if ((GetScreenHeight() - 2 * TILE_RADIUS) - (tpos.y + TILE_RADIUS) < 0)
                            gameOver(gs);
                        if (tile.thing.bomb)
                            checkBomb(ga, gs, {i, j});
                    }
                }
            }

            if (IsKeyDown(KEY_LEFT_CONTROL)) {
                auto mpos = getPosByPix(gs, {(float)GetMouseX(), (float)GetMouseY()});
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    addTile(gs, mpos, Tile{(mpos.col != (BOARD_WIDTH - 1)) || ((mpos.row + gs.board.even) % 2 == 0), {mpos.row, mpos.col}, 
                        {(unsigned char)getRandVal(gs, 0, COLORS.size() - 1), (unsigned char)getRandVal(gs, 0, COLORS.size() - 1), (unsigned char)getRandVal(gs, 0, COLORS.size() - 1)}});
                } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    removeTile(gs, mpos);
                }
            }

            if (!IsKeyDown(KEY_LEFT_CONTROL) && GetMouseY() < GetScreenHeight() - TILE_RADIUS * 2.0f) {
        #ifdef PLATFORM_ANDROID
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !gs.bullet.exists)
        #else
                if ((IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) && !gs.bullet.exists)
        #endif
                    shootAndRearm(ga, gs);
            }
            
            static int touchCount = 0;                
#ifdef PLATFORM_ANDROID
            if ((GetTouchPointCount() == 2 && touchCount == 1) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && GetMouseY() > GetScreenHeight() - TILE_RADIUS * 2.0f))
#else
            if (IsKeyPressed(KEY_LEFT_CONTROL) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && GetMouseY() > GetScreenHeight() - TILE_RADIUS * 2.0f))
#endif
                swapExtra(ga, gs);
            touchCount = GetTouchPointCount();
    
            if (gs.board.moveTime > 0 && gs.board.pos < 0) {
                gs.board.pos = gs.board.pos * (1.0f - easeOutQuad(1.0f - gs.board.moveTime/gs.board.totalMoveTime));
                gs.board.moveTime -= getFrameTime(gs);
            }

            if (IsKeyPressed(KEY_Q))
                gs.usr.n_params = (gs.usr.n_params % 3) + 1;

            if (gs.firstShotFired) {
                if (gs.usr.velEnabled) 
                    gs.board.pos += TILE_PIXEL * (gs.usr.accEnabled ? gs.board.speed : BOARD_CONST_SPEED) * getFrameTime(gs);
                if (gs.usr.accEnabled) 
                    gs.board.speed += BOARD_ACC * getFrameTime(gs);
            }

            if (IsKeyPressed(KEY_Z)) {
                if (gs.usr.accEnabled)
                    gs.usr.accEnabled = false;
                else
                    gs.usr.velEnabled = false;
            }
            
            checkLines(gs);
        }
    }

    void updateMusic(const GameAssets& ga, GameState& gs) {
        if (gs.usr.musEnabled) {
            UpdateMusicStream(ga.music);
            auto musicLen = GetMusicTimeLength(ga.music);
            auto musicTimePlayed = GetMusicTimePlayed(ga.music);
            //if (gs.musicLoopDone && musicTimePlayed < musicLen * 0.5f)
            //    SeekMusicStream(ga.music, musicLen * 0.5f + musicTimePlayed);
            //if (!gs.musicLoopDone && musicTimePlayed > musicLen * 0.5f)
            //    gs.musicLoopDone = true;
        }
    }

    float getTextSize(const GameAssets& ga) {
        auto sz = ga.font.baseSize * floor(TILE_RADIUS * 2 / ga.font.baseSize);
        return sz;
    }

    void drawText(const GameAssets& ga, const std::string txt, Vector2 pos, Color col = WHITE) {
        pos = {(float)int(pos.x), (float)int(pos.y)};
        auto pos2 = Vector2{pos.x, (float)int(pos.y + ceil(TILE_PIXEL))};
        auto sz = getTextSize(ga);
        Color darkol = Color{uint8_t(col.r * 0.6f), uint8_t(col.g * 0.6f), uint8_t(col.b * 0.6f), 255};
        DrawTextEx(ga.font, txt.c_str(), pos2, sz, 1.0, darkol);
        DrawTextEx(ga.font, txt.c_str(), pos, sz, 1.0, col);
    }

    void drawTile(const GameAssets& ga, const ThingPos& tpos, Vector2 pos, Color col = WHITE, Vector2 sz = {TILE_SIZE, TILE_SIZE}) {
        pos = {(float)int(pos.x - sz.x * TILE_PIXEL * 0.5f), (float)int(pos.y - sz.y * TILE_PIXEL * 0.5f)};
        DrawTexturePro(ga.tiles, {tpos.col * TILE_SIZE, tpos.row * TILE_SIZE, sz.x, sz.y}, {pos.x, pos.y, (float)int(sz.x * TILE_PIXEL), (float)int(sz.y * TILE_PIXEL)}, {0, 0}, 0, col);
    }

    void drawThing(const GameAssets& ga, const GameState& gs, Vector2 pos, const Thing& thing, bool masked = false, ThingPos maskTilePos = {}, uint8_t maskId1 = 0, uint8_t maskId2 = 0) {

        if (masked) {
            (*((GameState*)(&gs))).tmp.shMaskTilePos = Vector2{float(maskTilePos.col + maskId1), float(maskTilePos.row)};
            (*((GameState*)(&gs))).tmp.shMaskId = maskId2;
            SetShaderValue(ga.maskFragShader, GetShaderLocation(ga.maskFragShader, "maskTilePos"), &gs.tmp.shMaskTilePos, SHADER_UNIFORM_VEC2);
            SetShaderValue(ga.maskFragShader, GetShaderLocation(ga.maskFragShader, "maskId"), &gs.tmp.shMaskId, SHADER_UNIFORM_UINT);
            BeginDrawing();
            BeginShaderMode(ga.maskFragShader);
            rlEnableShader(ga.maskFragShader.id);
            rlSetUniformSampler(GetShaderLocation(ga.maskFragShader, "tiles"), ga.tiles.id);
        }
        
        if (thing.bomb)
            drawTile(ga, {4, thing.triggered ? ((int(floor(getTime(gs) * 20)) % 2 == 0) ? 4 : 5) : 3}, pos);
        else
            drawTile(ga, {0, (gs.usr.n_params == 1) ? 0 : thing.shp}, pos, COLORS[thing.clr], {TILE_SIZE, TILE_SIZE + 1.0f});

        if (masked) {
            EndShaderMode();
        }

        //if (gs.usr.n_params >= 3)
        //    drawTile(ga, {0, thing.sym}, pos, COLORS[thing.clr]);
        //for (auto& n : thing.neighs)
        //    if (n.exists) DrawLineV(pos, (getPixByPos(gs, n.pos) + pos) * 0.5, WHITE);
    }

    void drawParticles(const GameAssets& ga, const GameState& gs) {
        for (int i = 0; i < gs.tmp.animations.count(); ++i) {
            auto& anim = gs.tmp.animations.get(i);
            if (!anim.done) {
                auto nframes = int(anim.tex->width / anim.tex->height);
                auto frame = std::clamp(int(std::clamp(float((getTime(gs) - anim.startTime)/anim.interval), 0.0f, 1.0f) * nframes), 0, nframes - 1);
                DrawTexturePro(*anim.tex, {float(anim.tex->height * frame), 0.0f, float(anim.tex->height), float(anim.tex->height)}, {anim.pos.x - anim.tex->height * 0.5f * TILE_PIXEL, anim.pos.y - anim.tex->height * 0.5f * TILE_PIXEL, anim.tex->height * TILE_PIXEL, anim.tex->height * TILE_PIXEL}, {0, 0}, 0, WHITE);
            }
        }
        for (int i = gs.tmp.particles.count() - 1; i >= 0; --i) {
            auto& prt = gs.tmp.particles.get(i);
            if (prt.exists) {
                drawThing(ga, gs, prt.pos, prt.thing, prt.masked, prt.maskTilesStartPos, prt.maskId1, prt.maskId2);
            }
        }
    }

    void drawBoard(const GameAssets& ga, const GameState& gs) {
        for (int i = 0; i < BOARD_HEIGHT; ++i) {
            for (int j = 0; j < BOARD_WIDTH - ((i + gs.board.even) % 2); ++j) {
                const Tile& tile = gs.board.things[i][j];
                if (tile.exists) {
                    Vector2 tpos = getPixByPos(gs, {i, j});
                    Vector2 shake = SHAKE_STR * RAND_FLOAT_SIGNED_2D * (
                        gs.gameOver ?
                            std::clamp((getTime(gs) - gs.gameOverTime)/std::max((GAME_OVER_TIME_PER_ROW * (BOARD_HEIGHT - 1 - i)), 0.001f), 0.0, 1.0) :
                            tile.shake
                        );
                    drawThing(ga, gs, tpos + shake, tile.thing);
                }
            }
        }
        auto brect = getBoardRect(gs);
        DrawRectangleRec({brect.x - 3.0f, 0.0f, 3.0f, (float)GetScreenHeight()}, WHITE);
        DrawRectangleRec({brect.x + brect.width, 0.0f, 3.0f, (float)GetScreenHeight()}, WHITE);

        //auto mpos = getPosByPix(gs, {(float)GetMouseX(), (float)GetMouseY()});
        //std::map<int, std::map<int, bool>> visited;
        //if (isConnectedToTop(gs, mpos, visited))
        //    DrawCircleV(getPixByPos(gs, mpos), 5, WHITE);
    }

    void drawBullet(const GameAssets& ga, const GameState& gs) {
        drawThing(ga, gs, gs.bullet.pos, gs.bullet.thing);
    }

    void drawGameOver(const GameAssets& ga, const GameState& gs) {
        float coeff = easeOutBounce(1.0f - std::clamp((gs.gameOverTime + GAME_OVER_TIMEOUT - getTime(gs))/GAME_OVER_TIMEOUT_BEF, 0.0, 1.0));
        Vector2 skulpos = {GetScreenWidth() * 0.5f, GetScreenHeight() * -0.25f + coeff * GetScreenHeight() * 0.5f};
        drawTile(ga, {2, ((int(floor(getTime(gs) * 10)) % 2 == 0) ? 5 : (gs.alteredDifficulty ? 9 : ((gs.score == 0) ? 8 : ((gs.usr.bestScore == gs.score) ? 7 : 6))))}, skulpos);
        auto verdictstr = (gs.usr.bestScore == gs.score && !gs.alteredDifficulty) ? ((gs.usr.bestScore > 0) ? std::string("NEW RECORD!") : std::string("Really now???")) : ("Best: " + std::to_string(gs.usr.bestScore));
        auto sz = getTextSize(ga);
        auto vmeas = MeasureTextEx(ga.font, verdictstr.c_str(), sz, 1.0);
        drawText(ga, verdictstr, skulpos + Vector2{-vmeas.x * 0.5f, TILE_RADIUS * 3.0f - vmeas.y * 0.5f}, WHITE);

        auto scorestr = gs.alteredDifficulty ? ("\"" + std::to_string(gs.score) + "\"") : std::to_string(gs.score);
        auto meas = MeasureTextEx(ga.font, scorestr.c_str(), sz, 1.0);
        auto txtPos1prv = Vector2{TILE_RADIUS * 2.0f + (GetScreenWidth() - TILE_RADIUS * 6.0f) * 0.25f - meas.x * 0.5f, GetScreenHeight() - TILE_RADIUS - meas.y * 0.5f};
        auto txtPos2prv = Vector2{GetScreenWidth() - TILE_RADIUS * 2.0f - (GetScreenWidth() - TILE_RADIUS * 6.0f) * 0.25f - meas.x * 0.5f, GetScreenHeight() - TILE_RADIUS - meas.y * 0.5f};
        auto txtPosnew = Vector2{GetScreenWidth() * 0.5f - meas.x * 0.5f, GetScreenHeight() * 0.5f - meas.y * 0.5f};

        drawText(ga, scorestr, txtPos1prv + (txtPosnew - txtPos1prv) * coeff, PINK);
        drawText(ga, scorestr, txtPos2prv + (txtPosnew - txtPos2prv) * coeff, PINK);
        drawTile(ga, {2, 4}, {GetScreenWidth() * 0.5f, GetScreenHeight() * 1.25f - coeff * GetScreenHeight() * 0.5f}, WHITE, {TILE_SIZE, TILE_SIZE + 1});
    }

    void drawBottom(const GameAssets& ga, const GameState& gs) 
    {
        float startCoeff = easeOutQuad(std::clamp((getTime(gs) - gs.gameStartTime)/GAME_START_TIME, 0.0, 1.0));

        Vector2 nextNextPos = {GetScreenWidth() - TILE_RADIUS + TILE_RADIUS  * 2.0f, GetScreenHeight() - TILE_RADIUS};
        Vector2 nextPos = {GetScreenWidth() + TILE_RADIUS - startCoeff * 2 * TILE_RADIUS, GetScreenHeight() - TILE_RADIUS};
        Vector2 gunPos = {(float)GetScreenWidth() * 0.5f, (float)GetScreenHeight() + TILE_RADIUS - startCoeff * 2 * TILE_RADIUS};
        Vector2 extraPos = {-2.0f * TILE_RADIUS + startCoeff * 3.0f * TILE_RADIUS, GetScreenHeight() - TILE_RADIUS};
        float rearmCoeff = easeOutQuad(std::clamp((getTime(gs) - gs.rearmTime)/REARM_TIMEOUT, 0.0, 1.0));
        float swapCoeff = easeOutQuad(std::clamp((getTime(gs) - gs.swapTime)/REARM_TIMEOUT, 0.0, 1.0));
        if (startCoeff < 1.0f) rearmCoeff = 1.0f;
        
        float gameOverCoeff = gs.gameOver ? easeOutQuad(std::clamp((getTime(gs) - gs.gameOverTime)/GAME_OVER_TIMEOUT, 0.0, 1.0)) : 0.0f;
        DrawCircleV(gunPos + gameOverCoeff * Vector2{0, TILE_RADIUS * 3.0f}, TILE_RADIUS + TILE_RADIUS * 0.2f, WHITE);
        DrawCircleV(extraPos + gameOverCoeff * Vector2{-TILE_RADIUS * 3.0f, 0}, TILE_RADIUS + TILE_RADIUS * 0.2f, DARKGRAY);

        if (!gs.gameOver) {

            if (gs.gameStartTime + GAME_START_TIME < getTime(gs)) {
                const int NTICKS = 50;
                const float TICKSTEP = TILE_RADIUS * 2;
                Vector2 pos = gunPos;
                float dir = gs.gun.dir + PI * 0.5f;
                for (int i = 0; i < NTICKS; ++i) {
                    pos += TICKSTEP * Vector2{cos(dir), -sin(dir)};
                    DrawCircleV(pos, 3, WHITE);
                }
            }
            auto pt = GetSplinePointBezierQuad(nextPos, (nextPos + gunPos) * 0.5f - Vector2{0, 2.0f * TILE_RADIUS}, gunPos, rearmCoeff);
            auto pt2 = GetSplinePointBezierQuad(extraPos, (extraPos + gunPos) * 0.5f + Vector2{0, -2.0f * TILE_RADIUS}, gunPos, swapCoeff);
            drawThing(ga, gs, (swapCoeff == 1.0f || gs.gun.firstSwap) ? pt : pt2, gs.gun.armed);

            drawThing(ga, gs, nextNextPos + (nextPos - nextNextPos) * rearmCoeff, gs.gun.next);

            if (gs.gun.extraArmed)
                drawThing(ga, gs, gunPos + (extraPos - gunPos) * swapCoeff, gs.gun.extra);
            auto scorestr = gs.alteredDifficulty ? ("\"" + std::to_string(gs.score) + "\"") : std::to_string(gs.score);
            auto meas = MeasureTextEx(ga.font, scorestr.c_str(), getTextSize(ga), 1.0);
            drawText(ga, scorestr, {TILE_RADIUS * 2.0f + (GetScreenWidth() - TILE_RADIUS * 6.0f) * 0.25f - meas.x * 0.5f - (1.0f - startCoeff) * TILE_RADIUS * 2.0f, GetScreenHeight() - TILE_RADIUS - meas.y * 0.5f + (1.0f - startCoeff) * TILE_RADIUS * 2.0f}, GRAY);
            drawText(ga, scorestr, {GetScreenWidth() - TILE_RADIUS * 2.0f - (GetScreenWidth() - TILE_RADIUS * 6.0f) * 0.25f - meas.x * 0.5f + (1.0f - startCoeff) * TILE_RADIUS * 2.0f, GetScreenHeight() - TILE_RADIUS - meas.y * 0.5f + (1.0f - startCoeff) * TILE_RADIUS * 2.0f}, GRAY);

            for (int i = 0; i < BOARD_HEIGHT; ++i) {
                for (int j = 0; j < BOARD_WIDTH - ((i + gs.board.even) % 2); ++j) {
                    const Tile& tile = gs.board.things[i][j];
                    if (tile.exists) {
                        Vector2 tpos = getPixByPos(gs, {i, j});
                        float h = (GetScreenHeight() - 2 * TILE_RADIUS) - (tpos.y + TILE_RADIUS);
                        if (h < ROW_HEIGHT * 2) {
                            drawTile(ga, {2, 0}, {tpos.x, GetScreenHeight() - TILE_RADIUS - 3.0f * TILE_PIXEL}, WHITE, {3 * TILE_SIZE, TILE_SIZE});
                            if (h < ROW_HEIGHT * 1)
                                drawTile(ga, {2, 3}, {tpos.x, GetScreenHeight() - TILE_RADIUS}, (int(floor(getTime(gs) * 10)) % 2 == 0) ? WHITE : BLANK);
                        }
                    }
                }
            }
        }
    }

    void updateSettingsButton(const GameAssets& ga, GameState& gs) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && Vector2DistanceSqr({(float)GetScreenWidth(), 0.0f}, GetMousePosition()) < TILE_RADIUS * TILE_RADIUS * 4 * 2.0f) {
            gs.settingsOpened = !gs.settingsOpened;
            gs.inputTimeoutTime = getTime(gs);
        }
        if (IsKeyPressed(KEY_ESCAPE))
            gs.settingsOpened = !gs.settingsOpened;

    }

    void drawSettingsButton(const GameAssets& ga, const GameState& gs) {
        drawTile(ga, {3, (gs.settingsOpened ? 7 : 6)}, {GetScreenWidth() - TILE_RADIUS, TILE_RADIUS});
    }

    void draw(const GameAssets& ga, const GameState& gs) {        
        if (IsWindowFocused()) {
            drawBoard(ga, gs);
            if (gs.gameOver)
                drawGameOver(ga, gs);
            drawBottom(ga, gs);
            drawParticles(ga, gs);
            if (gs.bullet.exists)
                drawBullet(ga, gs);
        }
    }

    void updateAndDrawSettings(const GameAssets& ga, GameState& gs) 
    {
        auto prvusr = gs.usr;
        
        updateMusic(ga, gs);
        
        auto sndPos = Vector2{(float)int(GetScreenWidth() * 0.333f), (float)int(GetScreenHeight() * 0.25f)};
        auto musPos = Vector2{(float)int(GetScreenWidth() * 0.666f), (float)int(GetScreenHeight() * 0.25f)};
        drawTile(ga, {3, 2}, sndPos);
        if (!gs.usr.sndEnabled) 
            drawTile(ga, {3, 3}, sndPos);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && Vector2DistanceSqr(sndPos, GetMousePosition()) < TILE_RADIUS * TILE_RADIUS)
            gs.usr.sndEnabled = !gs.usr.sndEnabled;
        drawTile(ga, {3, 5}, musPos);
        if (!gs.usr.musEnabled)
            drawTile(ga, {3, 3}, musPos);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && Vector2DistanceSqr(musPos, GetMousePosition()) < TILE_RADIUS * TILE_RADIUS)
            gs.usr.musEnabled = !gs.usr.musEnabled;

        auto movPos = Vector2{(float)int(GetScreenWidth() * 0.333f) - TILE_RADIUS * 2.0f, (float)int(GetScreenHeight() * 0.25f + TILE_RADIUS * 4.0f)};
        drawTile(ga, {3, (gs.usr.velEnabled ? 1 : 0)}, movPos);
        drawText(ga, "board movement", movPos + Vector2{TILE_RADIUS * 1.5f, -TILE_RADIUS + TILE_PIXEL * 2.0f}, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && abs(movPos.y - GetMousePosition().y) < TILE_RADIUS) {
            gs.usr.velEnabled = !gs.usr.velEnabled;
            if (!gs.usr.velEnabled) gs.usr.accEnabled = false;
        }
        auto accPos = movPos + Vector2{0, TILE_RADIUS * 3.0f};
        drawTile(ga, {3, (gs.usr.accEnabled ? 1 : 0)}, accPos);
        drawText(ga, "acceleration", accPos + Vector2{TILE_RADIUS * 1.5f, -TILE_RADIUS + TILE_PIXEL * 2.0f}, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && abs(accPos.y - GetMousePosition().y) < TILE_RADIUS)
            gs.usr.accEnabled = !gs.usr.accEnabled;
        auto colPos = accPos + Vector2{0, TILE_RADIUS * 3.0f};
        drawTile(ga, {3, ((gs.usr.n_params == 1) ? 1 : 0)}, colPos);
        drawText(ga, "color only", colPos + Vector2{TILE_RADIUS * 1.5f, -TILE_RADIUS + TILE_PIXEL * 2.0f}, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && abs(colPos.y - GetMousePosition().y) < TILE_RADIUS)
            gs.usr.n_params = (gs.usr.n_params == 1) ? 2 : 1;


        if (prvusr != gs.usr)
            saveUserData(gs);

            updateSettingsButton(ga, gs);
        drawSettingsButton(ga, gs);
    }

    DLL_EXPORT void updateAndDraw(const GameAssets& ga, GameState& gs) 
    {
        if (!gs.tmp.timeOffsetSet) {
            if (gs.time == 0) gs.time = GetTime();
            gs.tmp.timeOffset = gs.time - GetTime();
            gs.tmp.timeOffsetSet = true;
        }

        if (!gs.usr.velEnabled || !gs.usr.accEnabled || (gs.usr.n_params == 1))
            gs.alteredDifficulty = true;
        
        BeginTextureMode(gs.tmp.renderTex);
        ClearBackground(BLACK);        
        if (gs.settingsOpened) {
            updateAndDrawSettings(ga, gs);
        } else {
            updateSettingsButton(ga, gs);
            if (IsWindowFocused()) {                            
                if (gs.inputTimeoutTime == 0)
                    gs.inputTimeoutTime = getTime(gs);
                if (getTime(gs) - gs.inputTimeoutTime > INPUT_TIMEOUT && getFrameTime(gs) < 1.0) {
                    for (int i = 0; i < UPDATE_ITS; ++i)
                        update(ga, gs);
                    updateOnce(ga, gs);
                }
                flyParticles(gs);
                checkDrops(gs);
                checkAnimations(gs);
                updateMusic(ga, gs);
            } else  {
                gs.inputTimeoutTime = 0;
            }            
            draw(ga, gs);
            drawSettingsButton(ga, gs);
        }
        EndTextureMode();

        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
            addDrop(gs, GetMousePosition());
        
        gs.tmp.shTime = getTime(gs);
        gs.tmp.shScreenSize = {(float)GetScreenWidth(), (float)GetScreenHeight()};
        SetShaderValue(ga.postProcFragShader, GetShaderLocation(ga.postProcFragShader, "time"), &gs.tmp.shTime, SHADER_UNIFORM_FLOAT);
        SetShaderValue(ga.postProcFragShader, GetShaderLocation(ga.postProcFragShader, "screenSize"), &gs.tmp.shScreenSize, SHADER_UNIFORM_VEC2);
        SetShaderValue(ga.postProcFragShader, GetShaderLocation(ga.postProcFragShader, "nDrops"), &gs.tmp.shNDrops, SHADER_UNIFORM_UINT);
        if (gs.tmp.shNDrops) {
            SetShaderValueV(ga.postProcFragShader, GetShaderLocation(ga.postProcFragShader, "dropTimes"), gs.tmp.shDropTimes.data(), SHADER_UNIFORM_FLOAT, gs.tmp.shNDrops);
            SetShaderValueV(ga.postProcFragShader, GetShaderLocation(ga.postProcFragShader, "dropCenters"), gs.tmp.shDropCenters.data(), SHADER_UNIFORM_VEC2, gs.tmp.shNDrops);
        }

        BeginDrawing();
        BeginShaderMode(ga.postProcFragShader);
        DrawTextureRec(gs.tmp.renderTex.texture, Rectangle{0, 0, (float)gs.tmp.renderTex.texture.width, (float)-gs.tmp.renderTex.texture.height}, Vector2Zero(), WHITE);
        EndShaderMode();
        EndDrawing();
        
        //rlEnableShader(_shader.id);
        //rlSetUniformSampler(GetShaderLocation(_shader, "texture1"), _frontTex.texture.id);

        gs.time = GetTime();
    }

} // extern "C"