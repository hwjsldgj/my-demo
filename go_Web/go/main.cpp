#include "go_common.h"
#include "httplib.h"
#include "json.hpp"
#include <fstream>
#include <sstream>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace httplib;
using json = nlohmann::json;

// ---------- 全局状态 ----------
Color board[MAX_SIZE][MAX_SIZE];
Color currentTurn;
int passCount;
int moveCount;
bool gameOver;
bool aiResigned;
Color humanColor;
Color aiColor;
int selectedDifficulty;
int selectedBoardSize;
float komi;

std::unique_ptr<RuleBasedAI> ruleAI;
std::unique_ptr<GreedyAI> greedyAI;
std::unique_ptr<Search2AI> search2;
std::unique_ptr<Search4AI> search4;
std::unique_ptr<MCTS_AI> mctsAI;

struct State {
    Color board[MAX_SIZE][MAX_SIZE];
    Color turn;
};
std::vector<State> history;

// ---------- 辅助函数 ----------
json boardToJson() {
    json j = json::array();
    for (int r = 0; r < BOARD_SIZE; r++) {
        json row = json::array();
        for (int c = 0; c < BOARD_SIZE; c++) {
            row.push_back(static_cast<int>(board[r][c]));
        }
        j.push_back(row);
    }
    return j;
}

// ---------- 游戏初始化 ----------
void initGame(int boardSize, int difficulty) {
    // 如果模型是 19 路，且选择了 9 路，但用户没有提供 9 路模型，则自动切换到 19 路
    if (boardSize == 9) {
        std::ifstream model_9("go_model_9.onnx");
        if (boardSize == 9 && difficulty == 4) {
            std::cout << "⚠️ 9路棋盘不支持MCTS，自动切换为困难（6层搜索）" << std::endl;
            difficulty = 3;
        }
    }
    selectedBoardSize = boardSize;
    selectedDifficulty = difficulty;
    BOARD_SIZE = boardSize;
    komi = (boardSize == 9) ? 0.0f : 3.75f;
    KOMI = komi;

    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            board[i][j] = EMPTY;
    humanColor = BLACK;
    aiColor = WHITE;
    currentTurn = BLACK;
    passCount = 0;
    moveCount = 0;
    gameOver = false;
    aiResigned = false;
    history.clear();
    State init;
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            init.board[i][j] = EMPTY;
    init.turn = BLACK;
    history.push_back(init);

    ruleAI.reset();
    greedyAI.reset();
    search2.reset();
    search4.reset();
    mctsAI.reset();

    if (difficulty == 0) ruleAI = std::make_unique<RuleBasedAI>();
    else if (difficulty == 1) greedyAI = std::make_unique<GreedyAI>();
    else if (difficulty == 2) search2 = std::make_unique<Search2AI>();
    else if (difficulty == 3) {
        int depth = (boardSize == 9) ? 8 : 6;
        search4 = std::make_unique<Search4AI>(depth, 12);
    } else if (difficulty == 4) {
        mctsAI = std::make_unique<MCTS_AI>();
        std::string model_name = (boardSize == 9) ? "go_model_9.onnx" : "go_model.onnx";
        std::ifstream model_file(model_name);
        if (model_file.good()) {
            MCTS_AI::init_onnx_session(model_name);
        } else {
            std::cout << "未找到 " << model_name << "，MCTS 将使用启发式评估" << std::endl;
        }
    }
}

// ---------- 执行一步走棋（含错误类型） ----------
bool executeMove(int row, int col, Color color, std::string& errorMsg) {
    if (gameOver) {
        errorMsg = "game over";
        return false;
    }
    if (!inBounds(row, col) || board[row][col] != EMPTY) {
        errorMsg = "occupied";
        return false;
    }
    Color temp[MAX_SIZE][MAX_SIZE];
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            temp[i][j] = board[i][j];
    if (!tryMove(row, col, color, temp)) {
        errorMsg = "suicide";
        return false;
    }
    Color nextTurn = (color == BLACK) ? WHITE : BLACK;
    // 劫争检测
    for (const auto& st : history) {
        if (st.turn != nextTurn) continue;
        bool same = true;
        for (int i = 0; i < BOARD_SIZE && same; i++)
            for (int j = 0; j < BOARD_SIZE && same; j++)
                if (st.board[i][j] != temp[i][j]) { same = false; break; }
        if (same) {
            errorMsg = "ko";
            return false;
        }
    }
    // 更新棋盘
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            board[i][j] = temp[i][j];
    State newState;
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            newState.board[i][j] = board[i][j];
    newState.turn = nextTurn;
    history.push_back(newState);
    passCount = 0;
    moveCount++;
    currentTurn = nextTurn;
    errorMsg = "";
    return true;
}

// ---------- 领地是否完全确定 ----------
bool isTerritoryDetermined() {
    int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board[r][c] == EMPTY) {
                bool hasBlack = false, hasWhite = false, hasEmpty = false;
                for (int k = 0; k < 4; k++) {
                    int nr = r + dr[k], nc = c + dc[k];
                    if (!inBounds(nr, nc)) continue;
                    if (board[nr][nc] == EMPTY) hasEmpty = true;
                    else if (board[nr][nc] == BLACK) hasBlack = true;
                    else if (board[nr][nc] == WHITE) hasWhite = true;
                }
                if (hasEmpty || (hasBlack && hasWhite)) return false;
            }
        }
    }
    return true;
}

// ---------- AI 走棋（含认输判断） ----------
std::pair<int,int> aiMove() {
    if (gameOver) return {-1, -1};

    ScoreDetail score = calculateScore(board);
    int blackTotal = score.blackStones + score.blackTerritory;
    int whiteTotal = score.whiteStones + score.whiteTerritory;
    int totalPoints = BOARD_SIZE * BOARD_SIZE;

    // 条件1：原有阈值认输
    if (moveCount > 20) {
        double threshold;
        if (selectedDifficulty == 0) threshold = 0.10;
        else if (selectedDifficulty == 1) threshold = 0.20;
        else if (selectedDifficulty == 2) threshold = 0.25;
        else if (selectedDifficulty == 3) threshold = 0.30;
        else threshold = 0.35;
        if (whiteTotal < blackTotal * threshold) {
            aiResigned = true;
            gameOver = true;
            return {-1, -1};
        }
    }

    // 条件2：领地已确定且白方落后
    if (isTerritoryDetermined() && whiteTotal < blackTotal) {
        aiResigned = true;
        gameOver = true;
        return {-1, -1};
    }

    // 条件3：黑方占据超过一半棋盘
    if (blackTotal >= (totalPoints + 1) / 2) {
        aiResigned = true;
        gameOver = true;
        return {-1, -1};
    }

    AIBoard aiBoard;
    aiBoard.fromGlobal(board);
    std::pair<int,int> move;
    if (selectedDifficulty == 0) move = ruleAI->getMove(aiBoard, aiColor);
    else if (selectedDifficulty == 1) move = greedyAI->getMove(aiBoard, aiColor);
    else if (selectedDifficulty == 2) move = search2->getMove(aiBoard, aiColor);
    else if (selectedDifficulty == 3) move = search4->getMove(aiBoard, aiColor);
    else if (selectedDifficulty == 4) move = mctsAI->getMove(aiBoard, aiColor);
    else return {-1, -1};

    if (move.first == -1) {
        passCount++;
        if (passCount >= 2) gameOver = true;
        else currentTurn = (currentTurn == BLACK) ? WHITE : BLACK;
        return {-1, -1};
    }

    std::string err;
    if (!executeMove(move.first, move.second, aiColor, err)) {
        passCount++;
        if (passCount >= 2) gameOver = true;
        else currentTurn = (currentTurn == BLACK) ? WHITE : BLACK;
        return {-1, -1};
    }
    return move;
}

// ---------- 玩家 Pass ----------
bool playerPass() {
    if (gameOver || currentTurn != humanColor) return false;
    passCount++;
    if (passCount >= 2) {
        gameOver = true;
        return true;
    }
    currentTurn = (currentTurn == BLACK) ? WHITE : BLACK;
    return true;
}

// ---------- 玩家认输 ----------
void playerResign() {
    if (gameOver) return;
    gameOver = true;
    aiResigned = false;
}

// ---------- 终局得分 ----------
json getFinalScore() {
    ScoreDetail sd = calculateScore(board);
    int blackTotal = sd.blackStones + sd.blackTerritory;
    int whiteTotal = sd.whiteStones + sd.whiteTerritory;
    float blackFinal = blackTotal - komi;
    json result = {
        {"blackStones", sd.blackStones},
        {"blackTerritory", sd.blackTerritory},
        {"blackTotal", blackTotal},
        {"whiteStones", sd.whiteStones},
        {"whiteTerritory", sd.whiteTerritory},
        {"whiteTotal", whiteTotal},
        {"komi", komi},
        {"blackFinal", blackFinal},
        {"winner", aiResigned ? "black" : (blackFinal > whiteTotal ? "black" : (blackFinal < whiteTotal ? "white" : "draw"))}
    };
    return result;
}

// ---------- HTTP 服务 ----------
int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    srand(static_cast<unsigned>(time(nullptr)));
    // 默认尝试 19 路（因为模型是 19 路）
    initGame(19, 0);

    Server svr;

    svr.Get("/", [](const Request& req, Response& res) {
        std::ifstream file("index.html");
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            res.set_content(content, "text/html");
        } else {
            res.set_content("index.html not found", "text/plain");
        }
    });

    svr.Post("/config", [](const Request& req, Response& res) {
        try {
            json data = json::parse(req.body);
            int size = data["boardSize"].get<int>();
            int diff = data["difficulty"].get<int>();
            if (size != 9 && size != 19) size = 9;
            if (diff < 0 || diff > 4) diff = 0;
            initGame(size, diff);
            json resp = {{"status", "ok"}, {"boardSize", BOARD_SIZE}, {"difficulty", diff}};
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            json err = {{"error", e.what()}};
            res.set_content(err.dump(), "application/json");
            res.status = 400;
        }
    });

    svr.Post("/reset", [](const Request& req, Response& res) {
        initGame(selectedBoardSize, selectedDifficulty);
        if (selectedDifficulty == 4) {
            mctsAI.reset(new MCTS_AI());
            std::string model_name = (selectedBoardSize == 9) ? "go_model_9.onnx" : "go_model.onnx";
            std::ifstream model_file(model_name);
            if (model_file.good()) {
                MCTS_AI::init_onnx_session(model_name);
            }
        }
        json resp = {{"status", "ok"}};
        res.set_content(resp.dump(), "application/json");
    });

    svr.Post("/resign", [](const Request& req, Response& res) {
        if (gameOver) {
            json resp = {{"error", "game already over"}};
            res.set_content(resp.dump(), "application/json");
            return;
        }
        playerResign();
        json resp = {{"gameOver", true}, {"finalScore", getFinalScore()}};
        res.set_content(resp.dump(), "application/json");
    });

    svr.Post("/endgame", [](const Request& req, Response& res) {
        if (gameOver) {
            json resp = {{"error", "game already over"}};
            res.set_content(resp.dump(), "application/json");
            return;
        }
        gameOver = true;
        json resp = {{"gameOver", true}, {"finalScore", getFinalScore()}};
        res.set_content(resp.dump(), "application/json");
    });

    svr.Get("/status", [](const Request& req, Response& res) {
        json resp = {
            {"board", boardToJson()},
            {"currentTurn", currentTurn},
            {"gameOver", gameOver},
            {"aiResigned", aiResigned},
            {"passCount", passCount},
            {"moveCount", moveCount}
        };
        if (gameOver) {
            resp["finalScore"] = getFinalScore();
        }
        res.set_content(resp.dump(), "application/json");
    });

    svr.Post("/move", [](const Request& req, Response& res) {
        try {
            if (gameOver) {
                json resp = {{"error", "game already over"}};
                res.set_content(resp.dump(), "application/json");
                return;
            }
            if (currentTurn != humanColor) {
                json resp = {{"error", "not your turn"}};
                res.set_content(resp.dump(), "application/json");
                return;
            }
            json data = json::parse(req.body);
            int row = data["row"].get<int>();
            int col = data["col"].get<int>();
            if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE || board[row][col] != EMPTY) {
                json resp = {{"error", "invalid move"}};
                res.set_content(resp.dump(), "application/json");
                return;
            }

            std::string err;
            if (!executeMove(row, col, humanColor, err)) {
                std::string errorType = "invalid";
                if (err == "ko") errorType = "ko";
                else if (err == "suicide") errorType = "suicide";
                json resp = {{"error", errorType}};
                res.set_content(resp.dump(), "application/json");
                return;
            }

            // 更新 MCTS 树（子树重用）
            if (selectedDifficulty == 4 && mctsAI) {
                mctsAI->apply_human_move(row, col, humanColor);
            }

            if (gameOver) {
                json resp = {{"gameOver", true}, {"finalScore", getFinalScore()}};
                res.set_content(resp.dump(), "application/json");
                return;
            }

            auto aiMoveResult = aiMove();
            if (gameOver) {
                json resp = {
                    {"gameOver", true},
                    {"finalScore", getFinalScore()},
                    {"aiMove", json{{"row", -1}, {"col", -1}}},
                    {"pass", true}
                };
                res.set_content(resp.dump(), "application/json");
                return;
            }

            json resp = {
                {"board", boardToJson()},
                {"currentTurn", currentTurn},
                {"pass", (aiMoveResult.first == -1)},
                {"aiRow", aiMoveResult.first},
                {"aiCol", aiMoveResult.second},
                {"gameOver", false}
            };
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            json err = {{"error", e.what()}};
            res.set_content(err.dump(), "application/json");
            res.status = 400;
        }
    });

    svr.Post("/pass", [](const Request& req, Response& res) {
        try {
            if (gameOver) {
                json resp = {{"error", "game already over"}};
                res.set_content(resp.dump(), "application/json");
                return;
            }
            if (currentTurn != humanColor) {
                json resp = {{"error", "not your turn"}};
                res.set_content(resp.dump(), "application/json");
                return;
            }
            if (!playerPass()) {
                json resp = {{"error", "cannot pass"}};
                res.set_content(resp.dump(), "application/json");
                return;
            }

            // Pass 后 MCTS 树无法重用，重置
            if (selectedDifficulty == 4 && mctsAI) {
                mctsAI.reset(new MCTS_AI());
                std::string model_name = (selectedBoardSize == 9) ? "go_model_9.onnx" : "go_model.onnx";
                std::ifstream model_file(model_name);
                if (model_file.good()) {
                    MCTS_AI::init_onnx_session(model_name);
                }
            }

            if (gameOver) {
                json resp = {{"gameOver", true}, {"finalScore", getFinalScore()}};
                res.set_content(resp.dump(), "application/json");
                return;
            }
            auto aiMoveResult = aiMove();
            if (gameOver) {
                json resp = {
                    {"gameOver", true},
                    {"finalScore", getFinalScore()},
                    {"aiMove", json{{"row", -1}, {"col", -1}}},
                    {"pass", true}
                };
                res.set_content(resp.dump(), "application/json");
                return;
            }
            json resp = {
                {"board", boardToJson()},
                {"currentTurn", currentTurn},
                {"pass", (aiMoveResult.first == -1)},
                {"aiRow", aiMoveResult.first},
                {"aiCol", aiMoveResult.second},
                {"gameOver", false}
            };
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            json err = {{"error", e.what()}};
            res.set_content(err.dump(), "application/json");
            res.status = 400;
        }
    });

    std::cout << "服务器启动在 http://localhost:8080\n";
    svr.listen("0.0.0.0", 8080);
    return 0;
}