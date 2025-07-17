#include "game.h"
#include "raylib.h"

#include "util/vec_ops.h"
#include "raymath.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <map>
#include <string>

extern "C" const unsigned char res_tiles[];
extern "C" const size_t        res_tiles_len;
extern "C" const unsigned char res_font[];
extern "C" const size_t        res_font_len;

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
        float startCoeff = easeOutQuad(std::clamp((GetTime() - gs.gameStartTime)/GAME_START_TIME, 0.0, 1.0));
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
        gs.particles.acquire(Particle{true, thing, pos, vel});
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
        gs.rearmTime = GetTime();
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
        gs.swapTime = GetTime();
    }

    void loadAssets(GameAssets& ga) {
        ga.tiles = LoadTextureFromImage(LoadImageFromMemory(".png", res_tiles, res_tiles_len));
        char8_t _allChars[228] = u8" !\"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ";
        int c; auto cdpts = LoadCodepoints((const char*)_allChars, &c);
        ga.font = LoadFontFromMemory(".ttf", res_font, res_font_len, 39, cdpts, c);
    }

    void resetGame(GameState& gs) {
        gs = GameState{0};
        
        gs.seed = rand() % std::numeric_limits<int>::max();

        generateRows(gs, BOARD_HEIGHT - BOARD_EMP_BOT_ROW_GAP);

        rearm(gs);

        float bHeight = ROW_HEIGHT * BOARD_HEIGHT;   
        
        gs.gameStartTime = GetTime();
    }

    DLL_EXPORT void init(GameAssets& ga, GameState& gs)
    {
        loadAssets(ga);
        resetGame(gs);
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
        auto extraRows = countBotEmpRows(gs) - BOARD_EMP_BOT_ROW_GAP;
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
        for (int k = 0; k < gs.n_params; ++k) {
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
        gs.bullet.rebTime = GetTime();
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
            float prog = (float)(GetTime() - gs.bullet.rebTime)/BULLET_REBOUNCE_TIME;
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
        for (int i = 0; i < gs.particles.count(); ++i) {
            auto& prt = gs.particles.at(i);
            if (prt.exists) {
                prt.pos += prt.vel * delta;
                prt.vel += GRAVITY * Vector2{0.0f, 1.0f} * delta;
                if (prt.pos.y < GetScreenHeight())
                    someInFrame = true;
            }
        }
        if (!someInFrame)
            gs.particles.clear();
    }

    void gameOver(GameState& gs) {
        gs.gameOver = true;
        gs.gameOverTime = GetTime();
        gs.bullet.exists = false;
        Vector2 gunPos = {(float)GetScreenWidth() * 0.5f, (float)GetScreenHeight() - TILE_RADIUS};
        addParticle(gs, gs.gun.armed, gunPos, Vector2{50.0f * RAND_FLOAT_SIGNED, -400.0f - 100.0f * RAND_FLOAT});
        addParticle(gs, gs.gun.next, {GetScreenWidth() - TILE_RADIUS, GetScreenHeight() - TILE_RADIUS}, Vector2{50.0f * RAND_FLOAT_SIGNED, -400.0f - 100.0f * RAND_FLOAT});
        if (gs.gun.extraArmed)
            addParticle(gs, gs.gun.extra, {TILE_RADIUS, GetScreenHeight() - TILE_RADIUS}, Vector2{50.0f * RAND_FLOAT_SIGNED, -400.0f - 100.0f * RAND_FLOAT});
    }

    void update(GameState& gs) 
    {
        if (gs.gameOver) {
            if (GetTime() > gs.gameOverTime + GAME_OVER_TIMEOUT) {
#ifdef PLATFORM_ANDROID
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
#else
                if ((IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))
#endif            
                resetGame(gs);
            }
        } else if (gs.gameStartTime + GAME_START_TIME < GetTime()) {
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

    void updateOnce(GameState& gs) 
    {   
        if (gs.gameStartTime + GAME_START_TIME < GetTime()) {
            if (gs.gameOver) {
                for (int i = 0; i < BOARD_HEIGHT; ++i) {
                    for (int j = 0; j < BOARD_WIDTH - ((i + gs.board.even) % 2); ++j) {
                        const Tile& tile = gs.board.things[i][j];
                        if (tile.ref.exists) {
                            if ((GetTime() - gs.gameOverTime) > (GAME_OVER_TIME_PER_ROW * (BOARD_HEIGHT - 1 - i))) {
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

#ifdef PLATFORM_ANDROID
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && (GetMouseY() > GetScreenHeight() - TILE_RADIUS * 2.0f))
#else
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
#endif
                swapExtra(gs);
        
                if (gs.board.moveTime > 0 && gs.board.pos < 0) {
                    gs.board.pos = gs.board.pos * (1.0f - easeOutQuad(1.0f - gs.board.moveTime/gs.board.totalMoveTime));
                    gs.board.moveTime -= GetFrameTime();
                }

                if (IsKeyPressed(KEY_Q))
                    gs.n_params = (gs.n_params % 3) + 1;

            }

            flyParticles(gs, GetFrameTime());

            if (gs.firstShotFired) {
                gs.board.pos += TILE_RADIUS / 16.0f * gs.board.speed * GetFrameTime();
                gs.board.speed += BOARD_ACC * GetFrameTime();
            }
        }
    }

    void drawThing(const GameAssets& ga, const GameState& gs, Vector2 pos, const Thing& thing) {
        //DrawCircle(pos.x, pos.y, TILE_RADIUS, COLORS[thing.clr]);
        pos = {(float)int(pos.x), (float)int(pos.y)};
        if (gs.n_params == 1)
            DrawTexturePro(ga.tiles, {0.0f, 0.0f, 16.0f, 17.0f}, {pos.x - TILE_RADIUS, pos.y - TILE_RADIUS, TILE_RADIUS * 2, TILE_RADIUS * 2 + TILE_RADIUS * 2.0f/17.0f}, {0, 0}, 0, COLORS[thing.clr]);
        else if (gs.n_params >= 2)
            DrawTexturePro(ga.tiles, {16.0f * thing.shp, 0.0f, 16.0f, 17.0f}, {pos.x - TILE_RADIUS, pos.y - TILE_RADIUS, TILE_RADIUS * 2, TILE_RADIUS * 2 + TILE_RADIUS * 2.0f/16.0f}, {0, 0}, 0, COLORS[thing.clr]);
        if (gs.n_params >= 3)
            DrawTexturePro(ga.tiles, {16.0f * thing.sym, 16.0f, 16.0f, 16.0f}, {pos.x - TILE_RADIUS, pos.y - TILE_RADIUS, TILE_RADIUS * 2, TILE_RADIUS * 2}, {0, 0}, 0, COLORS[thing.clr]);
        //for (auto& n : thing.neighs)
        //    if (n.exists) DrawLineV(pos, (getPixByPos(gs, n.pos) + pos) * 0.5, WHITE);
    }

    void drawParticles(const GameAssets& ga, const GameState& gs) {
        for (int i = gs.particles.count() - 1; i >= 0; --i) {
            auto& prt = gs.particles.get(i);
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
                        SHAKE_STR * RAND_FLOAT_SIGNED_2D * std::clamp((GetTime() - gs.gameOverTime)/std::max((GAME_OVER_TIME_PER_ROW * (BOARD_HEIGHT - 1 - i)), 0.001f), 0.0, 1.0) :
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
        float coeff = easeOutBounce(1.0f - std::clamp((gs.gameOverTime + GAME_OVER_TIMEOUT - GetTime())/GAME_OVER_TIMEOUT_BEF, 0.0, 1.0));
        DrawTexturePro(ga.tiles, {(int(floor(GetTime() * 10)) % 2 == 0) ? 80.0f : 96.0f, 32.0f, 16.0f, 16.0f}, {GetScreenWidth() * 0.5f - TILE_RADIUS, GetScreenHeight() * -0.25f - TILE_RADIUS + coeff * GetScreenHeight() * 0.5f, TILE_RADIUS * 2, TILE_RADIUS * 2}, {0, 0}, 0, WHITE);
        auto scorestr = std::to_string(gs.score);
        auto sz = ga.font.baseSize * floor(TILE_RADIUS * 2 / ga.font.baseSize);
        auto meas = MeasureTextEx(ga.font, scorestr.c_str(), sz, 1.0);
        auto txtPos1prv = Vector2{TILE_RADIUS * 2.0f + (GetScreenWidth() - TILE_RADIUS * 6.0f) * 0.25f - meas.x * 0.5f, GetScreenHeight() - TILE_RADIUS - meas.y * 0.5f};
        auto txtPos2prv = Vector2{GetScreenWidth() - TILE_RADIUS * 2.0f - (GetScreenWidth() - TILE_RADIUS * 6.0f) * 0.25f - meas.x * 0.5f, GetScreenHeight() - TILE_RADIUS - meas.y * 0.5f};
        auto txtPosnew = Vector2{GetScreenWidth() * 0.5f - meas.x * 0.5f, GetScreenHeight() * 0.5f - meas.y * 0.5f};

        DrawTextEx(ga.font, scorestr.c_str(), txtPos1prv + (txtPosnew - txtPos1prv) * coeff, sz, 1.0, PINK);
        DrawTextEx(ga.font, scorestr.c_str(), txtPos2prv + (txtPosnew - txtPos2prv) * coeff, sz, 1.0, PINK);
        DrawTexturePro(ga.tiles, {64.0f, 32.0f, 16.0f, 16.0f}, {GetScreenWidth() * 0.5f - TILE_RADIUS, GetScreenHeight() * 1.25f - TILE_RADIUS - coeff * GetScreenHeight() * 0.5f, TILE_RADIUS * 2, TILE_RADIUS * 2}, {0, 0}, 0, WHITE);
    }

    void drawBottom(const GameAssets& ga, const GameState& gs) 
    {
        float startCoeff = easeOutQuad(std::clamp((GetTime() - gs.gameStartTime)/GAME_START_TIME, 0.0, 1.0));

        Vector2 nextNextPos = {GetScreenWidth() - TILE_RADIUS + TILE_RADIUS  * 2.0f, GetScreenHeight() - TILE_RADIUS};
        Vector2 nextPos = {GetScreenWidth() + TILE_RADIUS - startCoeff * 2 * TILE_RADIUS, GetScreenHeight() - TILE_RADIUS};
        Vector2 gunPos = {(float)GetScreenWidth() * 0.5f, (float)GetScreenHeight() + TILE_RADIUS - startCoeff * 2 * TILE_RADIUS};
        Vector2 extraPos = {-2.0f * TILE_RADIUS + startCoeff * 3.0f * TILE_RADIUS, GetScreenHeight() - TILE_RADIUS};
        float rearmCoeff = easeOutQuad(std::clamp((GetTime() - gs.rearmTime)/REARM_TIMEOUT, 0.0, 1.0));
        float swapCoeff = easeOutQuad(std::clamp((GetTime() - gs.swapTime)/REARM_TIMEOUT, 0.0, 1.0));
        if (startCoeff < 1.0f) rearmCoeff = 1.0f;
        
        float gameOverCoeff = gs.gameOver ? easeOutQuad(std::clamp((GetTime() - gs.gameOverTime)/GAME_OVER_TIMEOUT, 0.0, 1.0)) : 0.0f;
        DrawCircleV(gunPos + gameOverCoeff * Vector2{0, TILE_RADIUS * 3.0f}, TILE_RADIUS + TILE_RADIUS * 0.2f, WHITE);
        DrawCircleV(extraPos + gameOverCoeff * Vector2{-TILE_RADIUS * 3.0f, 0}, TILE_RADIUS + TILE_RADIUS * 0.2f, DARKGRAY);

        if (!gs.gameOver) {

            if (gs.gameStartTime + GAME_START_TIME < GetTime()) {
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
            auto scorestr = std::to_string(gs.score);
            auto sz = ga.font.baseSize * floor(TILE_RADIUS * 2 / ga.font.baseSize);
            auto meas = MeasureTextEx(ga.font, scorestr.c_str(), sz, 1.0);
            DrawTextEx(ga.font, scorestr.c_str(), {TILE_RADIUS * 2.0f + (GetScreenWidth() - TILE_RADIUS * 6.0f) * 0.25f - meas.x * 0.5f - (1.0f - startCoeff) * TILE_RADIUS * 2.0f, GetScreenHeight() - TILE_RADIUS - meas.y * 0.5f + (1.0f - startCoeff) * TILE_RADIUS * 2.0f}, sz, 1.0, GRAY);
            DrawTextEx(ga.font, scorestr.c_str(), {GetScreenWidth() - TILE_RADIUS * 2.0f - (GetScreenWidth() - TILE_RADIUS * 6.0f) * 0.25f - meas.x * 0.5f + (1.0f - startCoeff) * TILE_RADIUS * 2.0f, GetScreenHeight() - TILE_RADIUS - meas.y * 0.5f + (1.0f - startCoeff) * TILE_RADIUS * 2.0f}, sz, 1.0, GRAY);

            for (int i = 0; i < BOARD_HEIGHT; ++i) {
                for (int j = 0; j < BOARD_WIDTH - ((i + gs.board.even) % 2); ++j) {
                    const Tile& tile = gs.board.things[i][j];
                    if (tile.ref.exists) {
                        Vector2 tpos = getPixByPos(gs, {i, j});
                        float h = (GetScreenHeight() - 2 * TILE_RADIUS) - (tpos.y + TILE_RADIUS);
                        if (h < ROW_HEIGHT * 2) {
                            DrawTexturePro(ga.tiles, {0.0f, 32.0f, 48.0f, 16.0f}, {tpos.x - TILE_RADIUS * 3, GetScreenHeight() - 2 * TILE_RADIUS - 5.0f * TILE_RADIUS / 16.0f, TILE_RADIUS * 6, TILE_RADIUS * 2}, {0, 0}, 0, WHITE);
                            if (h < ROW_HEIGHT * 1) {
                                DrawTexturePro(ga.tiles, {48.0f, 32.0f, 16.0f, 16.0f}, {tpos.x - TILE_RADIUS, GetScreenHeight() - 2 * TILE_RADIUS, TILE_RADIUS * 2, TILE_RADIUS * 2}, {0, 0}, 0, (int(floor(GetTime() * 10)) % 2 == 0) ? WHITE : BLANK);
                            }
                        }
                    }
                }
            }
        }
    }

    void draw(const GameAssets& ga, const GameState& gs) {        
        BeginDrawing();
        ClearBackground(BLACK);
        if (IsWindowFocused()) {
            drawBoard(ga, gs);
            if (gs.gameOver)
            drawGameOver(ga, gs);
            drawBottom(ga, gs);
            drawParticles(ga, gs);
            if (gs.bullet.exists)
                drawBullet(ga, gs);
        }
        EndDrawing();
    }

    DLL_EXPORT void updateAndDraw(const GameAssets& ga, GameState& gs) 
    {
        if (gs.gameStartTime > GetTime()) {
            gs.gameStartTime = GetTime() - GAME_START_TIME;
            gs.focusTime = GetTime() - UNFOCUS_TIMEOUT;
            gs.rearmTime = GetTime() - REARM_TIMEOUT;
            gs.swapTime = GetTime() - REARM_TIMEOUT;
        }
        if (IsWindowFocused()) {
            if (gs.focusTime == 0)
                gs.focusTime = GetTime();
            if (GetTime() - gs.focusTime > UNFOCUS_TIMEOUT) {
                for (int i = 0; i < UPDATE_ITS; ++i)
                    update(gs);
                updateOnce(gs);
            }
        } else  {
            gs.focusTime = 0;
        }

        draw(ga, gs);
    }

} // extern "C"