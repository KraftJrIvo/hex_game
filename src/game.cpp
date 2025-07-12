#include "game.h"
#include "game_cfg.h"
#include "raylib.h"

#include "util/vec_ops.h"
#include "raymath.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <map>

extern "C" const unsigned char res_tiles[];
extern "C" const size_t        res_tiles_len;

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

    Rectangle getBoardRect(const GameState& gs) {
        float bWidth = TILE_RADIUS * 2 * BOARD_WIDTH;
        float bHeight = ROW_HEIGHT * BOARD_HEIGHT;
        Vector2 bPos = {(GetScreenWidth() - bWidth) * 0.5f, (float)GetScreenHeight() - bHeight + gs.board.pos};
        return {float(int(bPos.x)), float(int(bPos.y)), bWidth, bHeight};
    }

    ThingPos getPosByPix(const GameState& gs, const Vector2& pix) {
        auto brec = getBoardRect(gs);
        int row = floor((pix.y - brec.y) / ROW_HEIGHT);
        int col = floor((pix.x - brec.x - float((row + gs.board.even) % 2) * TILE_RADIUS) / (TILE_RADIUS * 2));
        return {row, col};
    }

    Vector2 getPixByPos(const GameState& gs, const ThingPos& pos) {
        auto brec = getBoardRect(gs);
        float offset = float((pos.row + gs.board.even) % 2) * TILE_RADIUS;
        return {offset + brec.x + TILE_RADIUS + pos.col * TILE_RADIUS * 2, (float)(brec.y + (pos.row + 0.5f) * ROW_HEIGHT)};
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

    void addTile(GameState& gs, const ThingPos& pos, const Tile& tile) {
        auto& th = gs.board.things[pos.row][pos.col];
        th = tile;
        th.ref.pos = pos;
        updateNeighs(gs, pos, true);

        int i = 0;
        while ((pos.row - i > 0) && checkFullRow(gs, pos.row - i)) i++;
        if (i > 0 && pos.row - i <= gs.board.nFulRowsTop)
            gs.board.nFulRowsTop = pos.row + 1;
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

    void rearm(GameState& gs) {
        gs.gun.armed.shp = (unsigned char)getRandVal(gs, 0, COLORS.size() - 1);
        gs.gun.armed.clr = (unsigned char)getRandVal(gs, 0, COLORS.size() - 1);
        gs.gun.armed.sym = (unsigned char)getRandVal(gs, 0, COLORS.size() - 1);
    }

    DLL_EXPORT void init(GameState& gs)
    {
        gs = GameState{0};
        
        gs.seed = rand() % std::numeric_limits<int>::max();

        generateRows(gs, BOARD_HEIGHT - BOARD_EMP_BOT_ROW_GAP);

        rearm(gs);

        float bHeight = ROW_HEIGHT * BOARD_HEIGHT;

        gs.tiles = LoadTextureFromImage(LoadImageFromMemory(".png", res_tiles, res_tiles_len));
    }

    void shootAndRearm(GameState& gs) {
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

    bool isConnectedToTop(const GameState& gs, const ThingPos& pos, std::map<int, std::map<int, bool>>& visited)
    {
        if (visited.count(pos.row) && visited[pos.row].count(pos.col))
            return false;
        visited[pos.row][pos.col] = true;
        if (checkBounds(gs, pos)) {
            auto& tile = gs.board.things[pos.row][pos.col];
            if (tile.ref.exists) {
                bool connected = (pos.row == gs.board.nFulRowsTop - 1);
                for (auto& n : tile.neighs)
                    if (n.exists && !connected) connected |= isConnectedToTop(gs, n.pos, visited);
                visited[pos.row][pos.col] = connected;
                return connected;
            }
        }
        return false;
    }

    void massRemoveUnconnected(GameState& gs, const ThingPos& pos, std::map<int, std::map<int, bool>>& visited, bool check = true)
    {
        if (visited.count(pos.row) && visited[pos.row].count(pos.col))
            return;
        visited[pos.row][pos.col] = true;
        std::map<int, std::map<int, bool>> visCon;
        if (checkBounds(gs, pos) && (!check || !isConnectedToTop(gs, pos, visCon))) {
            auto& tile = gs.board.things[pos.row][pos.col];
            if (tile.ref.exists) {
                removeTile(gs, pos);
                addParticle(gs, gs.board.things[pos.row][pos.col].thing, getPixByPos(gs, pos), Vector2Zero());
                for (auto& n : tile.neighs)
                    if (n.exists) massRemoveUnconnected(gs, n.pos, visited, false);
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

    void checkDrop(GameState& gs, const ThingPos& pos) {
        if (gs.bullet.todrop.count() >= N_TO_DROP) {
            for (int i = 0; i < gs.bullet.todrop.count(); ++i) {
                auto& td = gs.bullet.todrop.at(i);
                removeTile(gs, td);
                addParticle(gs, gs.board.things[td.row][td.col].thing, getPixByPos(gs, td), {gs.bullet.vel.x * 0.1f, -500.5f});
            }
            for (int i = 0; i < gs.bullet.todrop.count(); ++i) {
                auto& td = gs.bullet.todrop.at(i);
                auto& thing = gs.board.things[td.row][td.col];
                for (auto& n : thing.neighs) {
                    std::map<int, std::map<int, bool>> visited;
                    massRemoveUnconnected(gs, n.pos, visited);
                }
            }
        }
        checkLines(gs);
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
                gs.bullet.todrop.acquire(gs.bullet.lstEmp);
                checkDrop(gs, gs.bullet.lstEmp);
                gs.bullet.rebouncing = false;
            } else {
                gs.bullet.rebounce = easeOutBounce(prog);
            }
        } else if (gs.bullet.exists) {
            if (gs.bullet.pos.x - BULLET_RADIUS_H < brect.x || gs.bullet.pos.x + BULLET_RADIUS_H > brect.x + brect.width)
                gs.bullet.vel.x *= -1.0f;

            if (bulpos.row >= 0 && bulpos.row < BOARD_HEIGHT && bulpos.col >= -1 && bulpos.col < BOARD_WIDTH && (bulpos.col == -1 || !gs.board.things[bulpos.row][bulpos.col].ref.exists)) {
                gs.bullet.lstEmp = {bulpos.row, bulpos.col};
                if ((bulpos.row + gs.board.even) % 2) {
                    if (bulpos.col == BOARD_WIDTH - 1) {
                        if (gs.board.things[bulpos.row][bulpos.col - 1].ref.exists) {
                            gs.bullet.lstEmp.row++;
                        } else {
                            gs.bullet.lstEmp.col--;
                        }
                    } else if (bulpos.col == -1) {
                        if (gs.board.things[bulpos.row][bulpos.col + 1].ref.exists) {
                            gs.bullet.lstEmp.row++;
                            gs.bullet.lstEmp.col++;
                        } else {
                            gs.bullet.lstEmp.col++;
                        }
                    }
                }
            }
            
            for (int i = 0; i < BOARD_HEIGHT; ++i) {
                for (int j = 0; j < BOARD_WIDTH - ((i + gs.board.even) % 2); ++j) {
                    const auto& tile = gs.board.things[i][j];
                    if (tile.ref.exists) {
                        Vector2 tpos = getPixByPos(gs, {i, j});
                        if (Vector2DistanceSqr(tpos, gs.bullet.pos) < (TILE_RADIUS + BULLET_RADIUS_H) * (TILE_RADIUS + BULLET_RADIUS_H)) {
                            int bestparam = 0, bestparamN = 0;
                            Arena<MAX_TODROP, ThingPos> todrops[3];
                            for (int k = 0; k < gs.n_params; ++k) {
                                std::map<int, std::map<int, bool>> vis;
                                checkDropRecur(gs, gs.bullet.lstEmp, gs.bullet.thing, k, todrops[k], vis);
                                if (bestparamN < todrops[k].count()) {
                                    bestparamN = todrops[k].count();
                                    bestparam = k;
                                }
                            }
                            std::map<int, std::map<int, bool>> vis2;
                            addShakeRecur(gs, gs.bullet.lstEmp, vis2, gs.bullet.thing, bestparam, SHAKE_TIME, SHAKE_DEPTH);
                            gs.bullet.todrop = todrops[bestparam];
                            gs.bullet.rebouncing = true;
                            gs.bullet.rebounce = 0.0f;
                            gs.bullet.rebCp = (gs.bullet.pos - Vector2Normalize(gs.bullet.vel) * BULLET_REBOUNCE)- Vector2{0, gs.board.pos};
                            gs.bullet.rebEnd = (getPixByPos(gs, gs.bullet.lstEmp)) - Vector2{0, gs.board.pos};
                            gs.bullet.rebTime = GetTime();
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

    void update(GameState& gs) 
    {
        auto delta = GetFrameTime() / UPDATE_ITS;

        if (fabs(GetMouseDelta().x) > 0) {
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

    void updateOnce(GameState& gs) 
    {
        for (int i = 0; i < BOARD_HEIGHT; ++i) {
            for (int j = 0; j < BOARD_WIDTH - ((i + gs.board.even) % 2); ++j) {
                Tile& tile = gs.board.things[i][j];
                if (tile.ref.exists) {
                    if (tile.shake < SHAKE_TIME || gs.bullet.todrop.count() < N_TO_DROP - 1)
                        tile.shake = std::max(tile.shake - GetFrameTime(), 0.0f);
                    else
                        tile.shake = std::min(tile.shake + GetFrameTime() * 2, MAX_SHAKE);
                }
            }
        }

        auto mpos = getPosByPix(gs, {(float)GetMouseX(), (float)GetMouseY()});
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && mpos.row >= 0 && mpos.row < BOARD_HEIGHT && mpos.col >= 0 && mpos.col < BOARD_WIDTH) {
            addTile(gs, mpos, Tile{});
            gs.board.things[mpos.row][mpos.col].thing.clr = GetRandomValue(0, COLORS.size() - 1);
        }

        flyParticles(gs, GetFrameTime());

        
        if (gs.board.moveTime > 0) {
            gs.board.pos = gs.board.pos * (1.0f - easeOutQuad(1.0f - gs.board.moveTime/gs.board.totalMoveTime));
            gs.board.moveTime -= GetFrameTime();
        }

        if (IsKeyPressed(KEY_Q))
            gs.n_params = (gs.n_params % 3) + 1;

        if ((IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) && !gs.bullet.exists)
            shootAndRearm(gs);
    }

    void drawThing(const GameState& gs, Vector2 pos, const Thing& thing) {
        //DrawCircle(pos.x, pos.y, TILE_RADIUS, COLORS[thing.clr]);
        pos = {(float)int(pos.x), (float)int(pos.y)};
        if (gs.n_params == 1)
            DrawTexturePro(gs.tiles, {0.0f, 0.0f, 16.0f, 17.0f}, {pos.x - TILE_RADIUS, pos.y - TILE_RADIUS, TILE_RADIUS * 2, TILE_RADIUS * 2 + TILE_RADIUS * 2.0f/17.0f}, {0, 0}, 0, COLORS[thing.clr]);
        else if (gs.n_params >= 2)
            DrawTexturePro(gs.tiles, {16.0f * thing.shp, 0.0f, 16.0f, 17.0f}, {pos.x - TILE_RADIUS, pos.y - TILE_RADIUS, TILE_RADIUS * 2, TILE_RADIUS * 2 + TILE_RADIUS * 2.0f/16.0f}, {0, 0}, 0, COLORS[thing.clr]);
        if (gs.n_params >= 3)
            DrawTexturePro(gs.tiles, {16.0f * thing.sym, 16.0f, 16.0f, 16.0f}, {pos.x - TILE_RADIUS, pos.y - TILE_RADIUS, TILE_RADIUS * 2, TILE_RADIUS * 2}, {0, 0}, 0, COLORS[thing.clr]);
        //for (auto& n : thing.neighs)
        //    if (n.exists) DrawLineV(pos, (getPixByPos(gs, n.pos) + pos) * 0.5, WHITE);
    }

    void drawParticles(GameState& gs) {
        for (int i = 0; i < gs.particles.count(); ++i) {
            auto& prt = gs.particles.at(i);
            if (prt.exists)
                drawThing(gs, prt.pos, prt.thing);
        }
    }

    void drawBoard(const GameState& gs) {
        for (int i = 0; i < BOARD_HEIGHT; ++i) {
            for (int j = 0; j < BOARD_WIDTH - ((i + gs.board.even) % 2); ++j) {
                const Tile& tile = gs.board.things[i][j];
                if (tile.ref.exists) {
                    Vector2 tpos = getPixByPos(gs, {i, j});
                    Vector2 shake = (2.0f * Vector2{RAND_FLOAT - 0.5f, RAND_FLOAT - 0.5f}) * tile.shake * SHAKE_STR;
                    drawThing(gs, tpos + shake, tile.thing);
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

    void drawBullet(const GameState& gs) {
        drawThing(gs, gs.bullet.pos, gs.bullet.thing);
    }

    void drawGun(const GameState& gs) {
        Vector2 gunPos = {(float)GetScreenWidth() * 0.5f, (float)GetScreenHeight() - TILE_RADIUS};
        DrawCircleV(gunPos, TILE_RADIUS + 3, WHITE);
        drawThing(gs, gunPos, gs.gun.armed);

        const int NTICKS = 50;
        const float TICKSTEP = TILE_RADIUS * 2;
        Vector2 pos = gunPos;
        float dir = gs.gun.dir + PI * 0.5f;
        for (int i = 0; i < NTICKS; ++i) {
            pos += TICKSTEP * Vector2{cos(dir), -sin(dir)};
            DrawCircleV(pos, 3, WHITE);
        }
    }    

    DLL_EXPORT void updateAndDraw(GameState& gs) 
    {
        for (int i = 0; i < UPDATE_ITS; ++i)
            update(gs);
        updateOnce(gs);

        BeginDrawing();
        ClearBackground(BLACK);
        drawBoard(gs);
        drawParticles(gs);
        drawGun(gs);
        if (gs.bullet.exists)
            drawBullet(gs);
        EndDrawing();
    }

} // extern "C"