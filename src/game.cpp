#include "game.h"
#include "raylib.h"

#include "util/vec_ops.h"
#include "raymath.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

extern "C" const unsigned char res_tiles[];
extern "C" const size_t        res_tiles_len;
extern "C" const unsigned char res_font[];
extern "C" const size_t        res_font_len;
extern "C" const unsigned char res_music[];
extern "C" const size_t        res_music_len;

#if (defined(_WIN32) || defined(_WIN64)) && defined(GAME_BASE_DLL)
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

extern "C" {

    int getRandVal(GameState& gs, int min, int max) {
        SetRandomSeed(gs.seed++);
        return GetRandomValue(min, max);
    }

    double getTime(const GameState& gs) {
        return GetTime() + gs.tmp.timeOffset;
    }

    bool checkBounds(const GameState& gs, const ThingPos& pos) {
        return (pos.row >= 0 && pos.row < BOARD_HEIGHT && pos.col >= 0 && pos.col < (((pos.row + gs.board.even) % 2) ? (BOARD_WIDTH - 1) : (BOARD_WIDTH)));
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
                if (gs.board.things[row][col].ref.exists) {
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
            if (!gs.board.things[row][i].ref.exists) {
                fullrow = false;
                break;
            }
        }
        return fullrow;
    }

    void updateNeighs(GameState& gs, const ThingPos& pos, bool exists) {
        for (int i = 0; i < 6; ++i) {
            auto& dis = gs.board.things[pos.row][pos.col];
            if (checkBounds(gs, TOGO[i])) {
                auto& nei = gs.board.things[TOGO[i].row][TOGO[i].col];
                nei.neighs[TOGOI[i]].exists = exists;
                if (exists) nei.neighs[TOGOI[i]].pos = pos;
                if (nei.ref.exists)
                    dis.neighs[i] = nei.ref;
                else
                    dis.neighs[i].exists = false;
            } else {
                dis.neighs[i].exists = false;
            }
        }
    }

    void addTile(GameState& gs, const ThingPos& pos, const Tile& tile, bool updateFullRows = true, bool makeExist = false) {
        auto& th = gs.board.things[pos.row][pos.col];
        th = tile;
        if (makeExist) th.ref.exists = true;
        th.ref.pos = pos;
        updateNeighs(gs, pos, true);

        if (updateFullRows) {
            int i = 0;
            while ((pos.row - i > 0) && checkFullRow(gs, pos.row - i)) i++;
            if (i > 0 && pos.row - i <= gs.board.nFulRowsTop)
                gs.board.nFulRowsTop = pos.row + 1;
        }
    }

    void addParticle(GameState& gs, const Thing& thing, Vector2 pos, Vector2 vel) {
        gs.tmp.particles.acquire(Particle{true, thing, pos, vel});
    }

    void generateRows(GameState& gs, int n) {
        for (int row = 0; row < n; ++row)
            for (int col = 0; col < BOARD_WIDTH - ((row + gs.board.even) % 2); ++col)
                addTile(gs, {row, col}, Tile{{(unsigned char)getRandVal(gs, 0, COLORS.size() - 1), (unsigned char)getRandVal(gs, 0, COLORS.size() - 1), (unsigned char)getRandVal(gs, 0, COLORS.size() - 1)}, 
                    {(col != (BOARD_WIDTH - 1)) || ((row + gs.board.even) % 2 == 0), {row, col}}, {0,0,0,0,0,0}});
    }

    void removeTile(GameState& gs, const ThingPos& pos) {
        gs.board.things[pos.row][pos.col].ref.exists = false;
        updateNeighs(gs, pos, false);        
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

    void swapExtra(GameState& gs) {
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
        gs.swapTime = getTime(gs);
    }

    void loadAssets(GameAssets& ga) {
        ga.tiles = LoadTextureFromImage(LoadImageFromMemory(".png", res_tiles, res_tiles_len));
        char8_t _allChars[228] = u8" !\"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ";
        int c; auto cdpts = LoadCodepoints((const char*)_allChars, &c);
        ga.font = LoadFontFromMemory(".ttf", res_font, res_font_len, 39, cdpts, c);
        InitAudioDevice();
        ga.music = LoadMusicStreamFromMemory(".ogg", res_music, res_music_len);
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

    void resetGame(GameState& gs) {
        gs = GameState{0};
        loadUserData(gs);
        
        gs.seed = rand() % std::numeric_limits<int>::max();

        generateRows(gs, BOARD_HEIGHT - gs.board.nRowsGap);

        rearm(gs);

        float bHeight = ROW_HEIGHT * BOARD_HEIGHT;   
        
        gs.gameStartTime = getTime(gs);
    }

    DLL_EXPORT void init(GameAssets& ga, GameState& gs)
    {
        loadAssets(ga);
        resetGame(gs);
        loadUserData(gs);
        PlayMusicStream(ga.music);
    }

    void shootAndRearm(GameState& gs) {
        gs.firstShotFired = true;
        gs.bullet.exists = true;
        gs.bullet.rebouncing = false;
        float dir = gs.gun.dir + PI * 0.5f;
        gs.bullet.thing = gs.gun.armed;
        gs.bullet.vel = BULLET_SPEED * Vector2{cos(dir), -sin(dir)};
        gs.bullet.pos = {(float)GetScreenWidth() * 0.5f, (float)GetScreenHeight() - TILE_RADIUS};

        rearm(gs);
    }

    bool checkMatch(const Thing& th1, const Thing& th2, int param) {
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
            const auto& tile = gs.board.things[pos.row][pos.col];
            bool match = checkMatch(tile.thing, thing, param);
            if ((tile.ref.exists && match) || first) {
                if (tile.ref.exists && match) todrop.acquire(pos);
                for (auto& n : tile.neighs)
                    if (n.exists) checkDropRecur(gs, n.pos, thing, param, todrop, visited, false);
            }
        }
    }

    bool isConnectedToTopRecur(const GameState& gs, const ThingPos& pos, std::map<int, std::map<int, bool>>& visited)
    {
        if (visited.count(pos.row) && visited[pos.row].count(pos.col))
            return false;
        if (pos.row == 21 && pos.col == 5)
            std::cout << "\n";
        visited[pos.row][pos.col] = true;
        if (checkBounds(gs, pos)) {
            auto& tile = gs.board.things[pos.row][pos.col];
            if (tile.ref.exists) {
                bool connected = (pos.row == gs.board.nFulRowsTop - 1);
                for (auto& n : tile.neighs)
                    if (n.exists && !connected) connected |= isConnectedToTopRecur(gs, n.pos, visited);
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
            auto& tile = gs.board.things[pos.row][pos.col];
            if (tile.ref.exists) {
                uncon.acquire(pos);
                for (auto& n : tile.neighs)
                    if (n.exists) checkUnconnectedRecur(gs, n.pos, visited, uncon, false);
            }
        }
    }

    void addShakeRecur(GameState& gs, const ThingPos& pos, std::map<int, std::map<int, bool>>& visited, const Thing& thing, int param, float shake, int depth, int curdepth = 0, bool mtchstreak = true)
    {
        if (visited.count(pos.row) && visited[pos.row].count(pos.col) || curdepth >= depth)
            return;
        visited[pos.row][pos.col] = true;
        auto& tile = gs.board.things[pos.row][pos.col];
        if (tile.ref.exists && curdepth == 0) tile.shake = std::max(tile.shake, shake / (curdepth + 1));
        if (tile.ref.exists || curdepth == 0) {
            for (int i = 0; i < 6; ++i) {
                if (checkBounds(gs, TOGO[i])) {
                    auto& n = gs.board.things[TOGO[i].row][TOGO[i].col];
                    bool match = checkMatch(n.thing, thing, param);
                    bool samecolor = (mtchstreak && match);
                    if (n.ref.exists) 
                        n.shake = std::max(n.shake, samecolor ? shake : (shake / (curdepth + 2)));
                }
            }
            if (mtchstreak) {
                for (int i = 0; i < 6; ++i) {
                    if (checkBounds(gs, TOGO[i])) {
                        auto& n = gs.board.things[TOGO[i].row][TOGO[i].col];
                        bool match = checkMatch(n.thing, thing, param);
                        if (n.ref.exists && match) 
                            addShakeRecur(gs, n.ref.pos, visited, thing, param, shake, depth, curdepth, true);
                    }
                }
            }
            if (!mtchstreak || curdepth == 0) {
                for (int i = 0; i < 6; ++i) {
                    if (checkBounds(gs, TOGO[i])) {
                        auto& n = gs.board.things[TOGO[i].row][TOGO[i].col];
                        bool match = checkMatch(n.thing, thing, param);
                        if (n.ref.exists && !match) 
                            addShakeRecur(gs, n.ref.pos, visited, thing, param, shake, depth, curdepth + 1, false);
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

    void checkDrop(GameState& gs) {
        int bestK = 0, bestScore = 0;
        Arena<MAX_TODROP, ThingPos> todrops[3];
        Arena<MAX_TODROP, ThingPos> uncons[3];
        for (int k = 0; k < gs.usr.n_params; ++k) {
            std::map<int, std::map<int, bool>> vis;
            checkDropRecur(gs, gs.bullet.lstEmp, gs.bullet.thing, k, todrops[k], vis);
            if (todrops[k].count() >= N_TO_DROP - 1) {
                for (int i = 0; i < todrops[k].count(); ++i)
                    removeTile(gs, todrops[k].at(i));
                std::map<int, std::map<int, bool>> vis2;
                for (int i = 0; i < todrops[k].count(); ++i) {
                    auto& td = todrops[k].at(i);
                    auto& thing = gs.board.things[td.row][td.col];
                    for (auto& n : thing.neighs) {
                        if (n.exists)
                            checkUnconnectedRecur(gs, n.pos, vis2, uncons[k]);
                    }
                }
                for (int i = 0; i < todrops[k].count(); ++i)
                    addTile(gs, todrops[k].at(i), gs.board.things[todrops[k].at(i).row][todrops[k].at(i).col], true, true);
                todrops[k].acquire(gs.bullet.lstEmp);
            }
            int score = todrops[k].count() + uncons[k].count();
            if (bestScore < score) {
                bestScore = score;
                bestK = k;
            }
        }
        std::map<int, std::map<int, bool>> vis2;
        addShakeRecur(gs, gs.bullet.lstEmp, vis2, gs.bullet.thing, bestK, SHAKE_TIME, SHAKE_DEPTH);
        gs.bullet.todrop = todrops[bestK];
        gs.bullet.uncon = uncons[bestK];
        gs.bullet.rebouncing = true;
        gs.bullet.rebounce = 0.0f;
        gs.bullet.rebCp = (gs.bullet.pos - Vector2Normalize(gs.bullet.vel) * BULLET_REBOUNCE)- Vector2{0, gs.board.pos};
        gs.bullet.rebEnd = (getPixByPos(gs, gs.bullet.lstEmp)) - Vector2{0, gs.board.pos};
        gs.bullet.rebTime = getTime(gs);
    }

    void doDrop(GameState& gs, const ThingPos& pos) {
        if (gs.bullet.todrop.count() >= N_TO_DROP) {
            gs.score += gs.bullet.todrop.count();
            gs.score += gs.bullet.uncon.count();
            for (int i = 0; i < gs.bullet.todrop.count(); ++i) {
                auto& td = gs.bullet.todrop.at(i);
                removeTile(gs, td);
                addParticle(gs, gs.board.things[td.row][td.col].thing, getPixByPos(gs, td), {gs.bullet.vel.x * 0.1f, -500.5f});
            }
            for (int i = 0; i < gs.bullet.uncon.count(); ++i) {                
                auto& un = gs.bullet.uncon.at(i);
                removeTile(gs, un);
                addParticle(gs, gs.board.things[un.row][un.col].thing, getPixByPos(gs, un), Vector2Zero());
            }
        }
        checkLines(gs);
    }

    void flyBullet(GameState& gs, float delta)
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
                addTile(gs, gs.bullet.lstEmp, Tile{gs.bullet.thing, {true}});
                doDrop(gs, gs.bullet.lstEmp);
                gs.bullet.rebouncing = false;
            } else {
                gs.bullet.rebounce = easeOutBounce(prog);
            }
        } else if (gs.bullet.exists) {
            if (gs.bullet.pos.x - BULLET_RADIUS_H < brect.x || gs.bullet.pos.x + BULLET_RADIUS_H > brect.x + brect.width)
                gs.bullet.vel.x *= -1.0f;

            if (!gs.board.things[bulpos.row][bulpos.col].ref.exists)
                gs.bullet.lstEmp = {bulpos.row, bulpos.col};
            
            for (int i = 0; i < BOARD_HEIGHT; ++i) {
                for (int j = 0; j < BOARD_WIDTH - ((i + gs.board.even) % 2); ++j) {
                    const auto& tile = gs.board.things[i][j];
                    if (tile.ref.exists) {
                        Vector2 tpos = getPixByPos(gs, {i, j});
                        if (Vector2DistanceSqr(tpos, gs.bullet.pos) < BULLET_HIT_DIST_SQR ||
                            Vector2DistanceSqr(tpos, gs.bullet.pos + Vector2Normalize(gs.bullet.vel) * BULLET_RADIUS_H) < BULLET_HIT_DIST_SQR) {
                            checkDrop(gs);
                            break;
                        }
                    }
                }
                if (gs.bullet.rebouncing)
                    break;
            }
        }
    }

    void flyParticles(GameState& gs, float delta) {
        bool someInFrame = false;
        for (int i = 0; i < gs.tmp.particles.count(); ++i) {
            auto& prt = gs.tmp.particles.at(i);
            if (prt.exists) {
                prt.pos += prt.vel * delta;
                prt.vel += GRAVITY * Vector2{0.0f, 1.0f} * delta;
                if (prt.pos.y < GetScreenHeight())
                    someInFrame = true;
            }
        }
        if (!someInFrame)
            gs.tmp.particles.clear();
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
        if (gs.score > gs.usr.bestScore) {
            gs.usr.bestScore = gs.score;
            saveUserData(gs);
        }
    }

    void update(GameState& gs) 
    {
        if (gs.gameOver) {
            if (getTime(gs) > gs.gameOverTime + GAME_OVER_TIMEOUT) {
#ifdef PLATFORM_ANDROID
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
#else
                if ((IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))
#endif            
                resetGame(gs);
            }
        } else if (gs.gameStartTime + GAME_START_TIME < getTime(gs)) {
            auto delta = GetFrameTime() / UPDATE_ITS;

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

            flyBullet(gs, delta);
        }
    }

    void updateOnce(const GameAssets& ga, GameState& gs) 
    {   
        if (gs.gameStartTime + GAME_START_TIME < getTime(gs)) {
            if (gs.gameOver) {
                for (int i = 0; i < BOARD_HEIGHT; ++i) {
                    for (int j = 0; j < BOARD_WIDTH - ((i + gs.board.even) % 2); ++j) {
                        const Tile& tile = gs.board.things[i][j];
                        if (tile.ref.exists) {
                            if ((getTime(gs) - gs.gameOverTime) > (GAME_OVER_TIME_PER_ROW * (BOARD_HEIGHT - 1 - i))) {
                                gs.board.things[i][j].ref.exists = false;
                                Vector2 tpos = getPixByPos(gs, {i, j});
                                if (tpos.y > 0)
                                    addParticle(gs, gs.board.things[i][j].thing, getPixByPos(gs, {i, j}), Vector2{50.0f * RAND_FLOAT_SIGNED, -400.0f - 100.0f * RAND_FLOAT});
                            }
                        }
                    }
                }
            } else {
                for (int i = 0; i < BOARD_HEIGHT; ++i) {
                    for (int j = 0; j < BOARD_WIDTH - ((i + gs.board.even) % 2); ++j) {
                        Tile& tile = gs.board.things[i][j];
                        if (tile.ref.exists) {
                            if (tile.shake < SHAKE_TIME || gs.bullet.todrop.count() < N_TO_DROP - 1)
                                tile.shake = std::max(tile.shake - GetFrameTime(), 0.0f);
                            else
                                tile.shake = std::min(tile.shake + GetFrameTime() * 2, MAX_SHAKE);
                            Vector2 tpos = getPixByPos(gs, {i, j});
                            if ((GetScreenHeight() - 2 * TILE_RADIUS) - (tpos.y + TILE_RADIUS) < 0)
                                gameOver(gs);
                        }
                    }
                }

                if (IsKeyDown(KEY_LEFT_CONTROL)) {
                    auto mpos = getPosByPix(gs, {(float)GetMouseX(), (float)GetMouseY()});
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        addTile(gs, mpos, Tile{{(unsigned char)getRandVal(gs, 0, COLORS.size() - 1), (unsigned char)getRandVal(gs, 0, COLORS.size() - 1), (unsigned char)getRandVal(gs, 0, COLORS.size() - 1)}, 
                    {(mpos.col != (BOARD_WIDTH - 1)) || ((mpos.row + gs.board.even) % 2 == 0), {mpos.row, mpos.col}}, {0,0,0,0,0,0}});
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
                        shootAndRearm(gs);
                }
                
                static int touchCount = 0;                
#ifdef PLATFORM_ANDROID
                if ((GetTouchPointCount() == 2 && touchCount == 1) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && GetMouseY() > GetScreenHeight() - TILE_RADIUS * 2.0f))
#else
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && GetMouseY() > GetScreenHeight() - TILE_RADIUS * 2.0f))
#endif
                    swapExtra(gs);
                touchCount = GetTouchPointCount();
        
                if (gs.board.moveTime > 0 && gs.board.pos < 0) {
                    gs.board.pos = gs.board.pos * (1.0f - easeOutQuad(1.0f - gs.board.moveTime/gs.board.totalMoveTime));
                    gs.board.moveTime -= GetFrameTime();
                }

                if (IsKeyPressed(KEY_Q))
                    gs.usr.n_params = (gs.usr.n_params % 3) + 1;

            }

            flyParticles(gs, GetFrameTime());

            if (gs.firstShotFired) {
                if (gs.usr.velEnabled) 
                    gs.board.pos += TILE_PIXEL * (gs.usr.accEnabled ? gs.board.speed : BOARD_CONST_SPEED) * GetFrameTime();
                if (gs.usr.accEnabled) 
                    gs.board.speed += BOARD_ACC * GetFrameTime();
            }

            if (IsKeyPressed(KEY_Z)) {
                if (gs.usr.accEnabled)
                    gs.usr.accEnabled = false;
                else
                    gs.usr.velEnabled = false;
            }
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
        DrawTexturePro(ga.tiles, {tpos.col * TILE_SIZE, tpos.row * TILE_SIZE, sz.x, sz.y}, {pos.x, pos.y, sz.x * TILE_PIXEL, sz.y * TILE_PIXEL}, {0, 0}, 0, col);
    }

    void drawThing(const GameAssets& ga, const GameState& gs, Vector2 pos, const Thing& thing) {
        if (gs.usr.n_params == 1)
            drawTile(ga, {0, 0}, pos, COLORS[thing.clr], {TILE_SIZE, TILE_SIZE + 1.0f});
        else if (gs.usr.n_params >= 2)
            drawTile(ga, {0, thing.shp}, pos, COLORS[thing.clr], {TILE_SIZE, TILE_SIZE + 1.0f});
        if (gs.usr.n_params >= 3)
            drawTile(ga, {0, thing.sym}, pos, COLORS[thing.clr]);
        //for (auto& n : thing.neighs)
        //    if (n.exists) DrawLineV(pos, (getPixByPos(gs, n.pos) + pos) * 0.5, WHITE);
    }

    void drawParticles(const GameAssets& ga, const GameState& gs) {
        for (int i = gs.tmp.particles.count() - 1; i >= 0; --i) {
            auto& prt = gs.tmp.particles.get(i);
            if (prt.exists)
                drawThing(ga, gs, prt.pos, prt.thing);
        }
    }

    void drawBoard(const GameAssets& ga, const GameState& gs) {
        for (int i = 0; i < BOARD_HEIGHT; ++i) {
            for (int j = 0; j < BOARD_WIDTH - ((i + gs.board.even) % 2); ++j) {
                const Tile& tile = gs.board.things[i][j];
                if (tile.ref.exists) {
                    Vector2 tpos = getPixByPos(gs, {i, j});
                    Vector2 shake = gs.gameOver ?
                        SHAKE_STR * RAND_FLOAT_SIGNED_2D * std::clamp((getTime(gs) - gs.gameOverTime)/std::max((GAME_OVER_TIME_PER_ROW * (BOARD_HEIGHT - 1 - i)), 0.001f), 0.0, 1.0) :
                        RAND_FLOAT_SIGNED_2D * tile.shake * SHAKE_STR;
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
        drawTile(ga, {2, ((int(floor(getTime(gs) * 10)) % 2 == 0) ? 5 : ((gs.score == 0) ? 8 : ((gs.usr.bestScore == gs.score) ? 7 : 6)))}, skulpos);
        auto verdictstr = (gs.usr.bestScore == gs.score) ? ((gs.usr.bestScore > 0) ? std::string("NEW RECORD!") : std::string("Really now???")) : ("Best: " + std::to_string(gs.usr.bestScore));
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
                    if (tile.ref.exists) {
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
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && Vector2DistanceSqr({(float)GetScreenWidth(), 0.0f}, GetMousePosition()) < TILE_RADIUS * TILE_RADIUS * 4) {
            gs.settingsOpened = !gs.settingsOpened;
            gs.inputTimeoutTime = getTime(gs);
        }
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

        auto movPos = Vector2{(float)int(GetScreenWidth() * 0.333f) - TILE_SIZE * 2.0f, (float)int(GetScreenHeight() * 0.25f + TILE_RADIUS * 4.0f)};
        drawTile(ga, {3, (gs.usr.velEnabled ? 1 : 0)}, movPos);
        drawText(ga, "board movement", movPos + Vector2{TILE_SIZE * 2.0f, -TILE_RADIUS + TILE_PIXEL * 4.0f}, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && abs(movPos.y - GetMousePosition().y) < TILE_RADIUS) {
            gs.usr.velEnabled = !gs.usr.velEnabled;
            if (!gs.usr.velEnabled) gs.usr.accEnabled = false;
        }
        auto accPos = movPos + Vector2{0, TILE_RADIUS * 3.0f};
        drawTile(ga, {3, (gs.usr.accEnabled ? 1 : 0)}, accPos);
        drawText(ga, "acceleration", accPos + Vector2{TILE_SIZE * 2.0f, -TILE_RADIUS + TILE_PIXEL * 4.0f}, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && abs(accPos.y - GetMousePosition().y) < TILE_RADIUS)
            gs.usr.accEnabled = !gs.usr.accEnabled;
        auto colPos = accPos + Vector2{0, TILE_RADIUS * 3.0f};
        drawTile(ga, {3, ((gs.usr.n_params == 1) ? 1 : 0)}, colPos);
        drawText(ga, "color only", colPos + Vector2{TILE_SIZE * 2.0f, -TILE_RADIUS + TILE_PIXEL * 4.0f}, WHITE);
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
        
        BeginDrawing();
        ClearBackground(BLACK);        
        if (gs.settingsOpened) {
            updateAndDrawSettings(ga, gs);
        } else {
            updateSettingsButton(ga, gs);
            if (IsWindowFocused()) {                            
                if (gs.inputTimeoutTime == 0)
                    gs.inputTimeoutTime = getTime(gs);
                if (getTime(gs) - gs.inputTimeoutTime > INPUT_TIMEOUT && GetFrameTime() < 1.0) {
                    for (int i = 0; i < UPDATE_ITS; ++i)
                        update(gs);
                    updateOnce(ga, gs);
                    updateMusic(ga, gs);
                }
            } else  {
                gs.inputTimeoutTime = 0;
            }            
            draw(ga, gs);
            drawSettingsButton(ga, gs);
        }
        EndDrawing();

        gs.time = GetTime();
    }

} // extern "C"