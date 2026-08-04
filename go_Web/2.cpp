#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <thread>
#include <chrono>
#include <random>
#include <iomanip>

using namespace std;

// ---------- 全局配置 ----------
int SIZE;
float KOMI;
enum Color { EMPTY, BLACK, WHITE };
Color board[19][19];

int lastRow = -1, lastCol = -1;

// 计时相关
chrono::steady_clock::time_point gameStart;
chrono::steady_clock::time_point turnStart;
chrono::milliseconds blackTime(0), whiteTime(0);

// AI 随机引擎
random_device rd;
mt19937 gen(rd());

// ---------- 清屏 ----------
void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// ---------- 基础工具 ----------
bool inBounds(int r, int c) {
    return r >= 0 && r < SIZE && c >= 0 && c < SIZE;
}

vector<pair<int, int>> getNeighbors(int r, int c) {
    vector<pair<int, int>> neigh;
    int dr[] = {-1, 1, 0, 0};
    int dc[] = {0, 0, -1, 1};
    for (int i = 0; i < 4; i++) {
        int nr = r + dr[i], nc = c + dc[i];
        if (inBounds(nr, nc))
            neigh.push_back({nr, nc});
    }
    return neigh;
}

vector<pair<int, int>> getGroup(int r, int c, Color color, const Color board[19][19]) {
    vector<pair<int, int>> group;
    bool visited[19][19] = {false};
    vector<pair<int, int>> stack;
    stack.push_back({r, c});
    visited[r][c] = true;

    while (!stack.empty()) {
        auto p = stack.back();
        stack.pop_back();
        group.push_back(p);
        for (auto neigh : getNeighbors(p.first, p.second)) {
            int nr = neigh.first, nc = neigh.second;
            if (!visited[nr][nc] && board[nr][nc] == color) {
                visited[nr][nc] = true;
                stack.push_back({nr, nc});
            }
        }
    }
    return group;
}

int countLiberties(const vector<pair<int, int>>& group, const Color board[19][19]) {
    int lib = 0;
    bool counted[19][19] = {false};
    for (auto p : group) {
        for (auto neigh : getNeighbors(p.first, p.second)) {
            int nr = neigh.first, nc = neigh.second;
            if (!counted[nr][nc] && board[nr][nc] == EMPTY) {
                counted[nr][nc] = true;
                lib++;
            }
        }
    }
    return lib;
}

vector<pair<int, int>> removeDeadStones(Color board[19][19], Color color) {
    vector<pair<int, int>> captured;
    bool removed[19][19] = {false};
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] == color && !removed[r][c]) {
                auto group = getGroup(r, c, color, board);
                if (countLiberties(group, board) == 0) {
                    for (auto p : group) {
                        board[p.first][p.second] = EMPTY;
                        removed[p.first][p.second] = true;
                        captured.push_back(p);
                    }
                } else {
                    for (auto p : group)
                        removed[p.first][p.second] = true;
                }
            }
        }
    }
    return captured;
}

pair<bool, vector<pair<int, int>>> tryMove(int r, int c, Color color, Color board[19][19]) {
    if (!inBounds(r, c) || board[r][c] != EMPTY)
        return {false, {}};

    Color temp[19][19];
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            temp[i][j] = board[i][j];

    temp[r][c] = color;
    Color opp = (color == BLACK) ? WHITE : BLACK;

    vector<pair<int, int>> captured = removeDeadStones(temp, opp);

    auto group = getGroup(r, c, color, temp);
    if (countLiberties(group, temp) == 0)
        return {false, {}};

    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            board[i][j] = temp[i][j];

    return {true, captured};
}

bool isStarPoint(int r, int c) {
    if (SIZE == 9) {
        return (r == 4 && c == 4);
    } else {
        int stars[9][2] = {
            {3,3},{3,15},{15,3},{15,15},
            {3,9},{15,9},{9,3},{9,15},{9,9}
        };
        for (int i = 0; i < 9; i++) {
            if (r == stars[i][0] && c == stars[i][1])
                return true;
        }
        return false;
    }
}

string coordToString(int r, int c) {
    if (r < 0 || c < 0) return "无";
    int idx = c;
    if (SIZE == 19 && idx >= 8) idx++;
    char letter = 'A' + idx;
    return string(1, letter) + to_string(r + 1);
}

string formatTime(chrono::milliseconds ms) {
    auto totalSec = chrono::duration_cast<chrono::seconds>(ms).count();
    auto minutes = totalSec / 60;
    auto seconds = totalSec % 60;
    auto millis = ms.count() % 1000;
    ostringstream oss;
    oss << setw(2) << setfill('0') << minutes << ":"
        << setw(2) << setfill('0') << seconds << "."
        << setw(3) << setfill('0') << millis;
    return oss.str();
}

void printBoardWithFlash(const Color board[19][19], const vector<pair<int, int>>& flashCoords, bool showFlash,
                         const string& statusLine = "") {
    clearScreen();

    cout << "上一手: " << coordToString(lastRow, lastCol) << "\n";
    cout << "黑方用时: " << formatTime(blackTime) << "  白方用时: " << formatTime(whiteTime) << "\n";
    if (!statusLine.empty())
        cout << statusLine << "\n";
    cout << "\n";

    cout << "   ";
    for (int c = 1; c <= SIZE; c++) {
        int idx = c - 1;
        if (SIZE == 19 && idx >= 8) idx++;
        char letter = 'A' + idx;
        cout << letter << " ";
    }
    cout << endl;

    for (int r = 0; r < SIZE; r++) {
        if (r + 1 < 10) cout << " " << r + 1 << " ";
        else cout << r + 1 << " ";
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] != EMPTY) {
                cout << (board[r][c] == BLACK ? "X " : "O ");
            } else {
                bool isFlash = false;
                if (showFlash) {
                    for (auto p : flashCoords) {
                        if (p.first == r && p.second == c) {
                            isFlash = true;
                            break;
                        }
                    }
                }
                if (isFlash)
                    cout << "▉ ";
                else if (isStarPoint(r, c))
                    cout << "+ ";
                else
                    cout << ". ";
            }
        }
        cout << endl;
    }
}

void flashCaptured(const Color board[19][19], const vector<pair<int, int>>& captured, const string& status = "") {
    if (captured.empty()) return;
    for (int i = 0; i < 3; i++) {
        printBoardWithFlash(board, captured, true, status);
        this_thread::sleep_for(chrono::milliseconds(150));
        printBoardWithFlash(board, captured, false, status);
        this_thread::sleep_for(chrono::milliseconds(150));
    }
}

void printBoard(const Color board[19][19], const string& status = "") {
    vector<pair<int, int>> empty;
    printBoardWithFlash(board, empty, false, status);
}

// ---------- 计分 ----------
struct ScoreDetail {
    int blackStones;
    int blackTerritory;
    int whiteStones;
    int whiteTerritory;
};

ScoreDetail calculateScore(const Color board[19][19]) {
    bool visited[19][19] = {false};
    int blackStones = 0, blackTerritory = 0;
    int whiteStones = 0, whiteTerritory = 0;

    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] == BLACK) blackStones++;
            else if (board[r][c] == WHITE) whiteStones++;
        }
    }

    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] == EMPTY && !visited[r][c]) {
                vector<pair<int, int>> region;
                vector<pair<int, int>> stack;
                stack.push_back({r, c});
                visited[r][c] = true;
                bool hasBlack = false, hasWhite = false;

                while (!stack.empty()) {
                    auto p = stack.back();
                    stack.pop_back();
                    region.push_back(p);

                    for (auto neigh : getNeighbors(p.first, p.second)) {
                        int nr = neigh.first, nc = neigh.second;
                        if (board[nr][nc] == EMPTY && !visited[nr][nc]) {
                            visited[nr][nc] = true;
                            stack.push_back({nr, nc});
                        } else if (board[nr][nc] == BLACK) {
                            hasBlack = true;
                        } else if (board[nr][nc] == WHITE) {
                            hasWhite = true;
                        }
                    }
                }

                if (hasBlack && !hasWhite)
                    blackTerritory += region.size();
                else if (!hasBlack && hasWhite)
                    whiteTerritory += region.size();
            }
        }
    }

    return {blackStones, blackTerritory, whiteStones, whiteTerritory};
}

// ---------- 解析坐标 ----------
bool parseCoordinate(const string& input, int& row, int& col) {
    string s = input;
    replace(s.begin(), s.end(), '.', ' ');
    replace(s.begin(), s.end(), ',', ' ');
    stringstream ss(s);
    int r, c;
    if (ss >> r >> c) {
        string remaining;
        ss >> remaining;
        if (remaining.empty()) {
            row = r - 1; col = c - 1;
            if (inBounds(row, col)) return true;
        }
    }
    string letters, digits;
    for (char ch : input) {
        if (isalpha(ch)) {
            letters += tolower(ch);
        } else if (isdigit(ch)) {
            digits += ch;
        }
    }
    if (letters.length() != 1 || digits.empty()) return false;
    char letter = letters[0];
    int colIndex;
    if (SIZE == 9) {
        if (letter < 'a' || letter > 'i') return false;
        colIndex = letter - 'a';
    } else {
        if (letter == 'i') return false;
        if (letter < 'i') {
            colIndex = letter - 'a';
        } else if (letter > 'i') {
            colIndex = letter - 'a' - 1;
        } else {
            return false;
        }
    }
    int rowIndex = stoi(digits) - 1;
    if (rowIndex < 0 || rowIndex >= SIZE || colIndex < 0 || colIndex >= SIZE) return false;
    row = rowIndex;
    col = colIndex;
    return true;
}

// ---------- AI 简单随机落子 ----------
pair<int, int> aiMove(Color color, const Color board[19][19]) {
    vector<pair<int, int>> candidates;
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] == EMPTY) {
                Color temp[19][19];
                for (int i = 0; i < SIZE; i++)
                    for (int j = 0; j < SIZE; j++)
                        temp[i][j] = board[i][j];
                auto result = tryMove(r, c, color, temp);
                if (result.first) {
                    candidates.push_back({r, c});
                }
            }
        }
    }
    if (candidates.empty()) return {-1, -1};
    uniform_int_distribution<> dist(0, candidates.size() - 1);
    return candidates[dist(gen)];
}

// ---------- 主程序 ----------
int main() {
    cout << "请选择模式：\n";
    cout << "1. 双人对弈\n";
    cout << "2. 人机对战（您执黑，AI执白）\n";
    cout << "3. AI 对 AI\n";
    cout << "请输入 1、2 或 3：";
    int mode;
    cin >> mode;
    cin.ignore();

    cout << "请选择棋盘：\n";
    cout << "1. 简单模式 (9x9 不贴子)\n";
    cout << "2. 标准模式 (19x19 贴 3.75 子)\n";
    cout << "请输入 1 或 2：";
    int boardMode;
    cin >> boardMode;
    cin.ignore();

    if (boardMode == 1) {
        SIZE = 9;
        KOMI = 0.0f;
    } else {
        SIZE = 19;
        KOMI = 3.75f;
    }

    // 初始化棋盘
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            board[i][j] = EMPTY;

    Color current = BLACK;
    int passCount = 0;
    bool gameOver = false;
    bool resignFlag = false;

    // 历史记录（全局同形）
    struct State {
        Color board[19][19];
        Color turn;
    };
    vector<State> history;
    State init;
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            init.board[i][j] = EMPTY;
    init.turn = BLACK;
    history.push_back(init);

    // 计时初始化
    gameStart = chrono::steady_clock::now();
    blackTime = chrono::milliseconds(0);
    whiteTime = chrono::milliseconds(0);
    turnStart = chrono::steady_clock::now();

    cout << "围棋对弈 — " << (SIZE == 9 ? "简单" : "标准") << "模式\n";
    cout << "黑子: X, 白子: O, 空点: ., 星位: +\n";
    if (KOMI > 0) cout << "贴子: 黑贴 " << KOMI << " 子 (数子法)\n";
    else cout << "无贴子\n";
    cout << "列标题为字母 (";
    if (SIZE == 9) cout << "A-I";
    else cout << "A-T (跳过I)";
    cout << ")，行号为数字。\n";
    cout << "输入坐标如 'A1'、'1A'、'3.4'、'3 4' 等。\n";
    cout << "指令：'p'/'pass' 弃权，'r'/'resign' 认输，'q' 退出。\n";
    if (mode == 2) cout << "您执黑，AI 执白。\n";
    else if (mode == 3) cout << "AI 对 AI，自动进行。\n";
    cout << "\n按 Enter 开始...";
    cin.get();

    printBoard(board);

    while (!gameOver) {
        // 更新当前玩家计时
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - turnStart);
        if (current == BLACK)
            blackTime += elapsed;
        else
            whiteTime += elapsed;
        turnStart = now;

        // 判断当前回合是否由人类操作
        bool isHumanTurn = false;
        if (mode == 1) {
            isHumanTurn = true;               // 双人都是人
        } else if (mode == 2) {
            isHumanTurn = (current == BLACK); // 玩家执黑
        } else { // mode == 3
            isHumanTurn = false;              // 全部AI
        }

        // ---------- 人类回合 ----------
        if (isHumanTurn) {
            printBoard(board);
            cout << (current == BLACK ? "黑方" : "白方") << "落子: ";
            string input;
            getline(cin, input);

            input.erase(0, input.find_first_not_of(" \t\n\r"));
            input.erase(input.find_last_not_of(" \t\n\r") + 1);

            if (input == "q" || input == "Q" || input == "quit" || input == "exit") {
                cout << "游戏结束。\n";
                break;
            }

            if (input == "r" || input == "R" || input == "resign" || input == "Resign") {
                resignFlag = true;
                gameOver = true;
                break;
            }

            if (input == "p" || input == "P" || input == "pass" || input == "Pass") {
                passCount++;
                if (passCount >= 2) {
                    gameOver = true;
                    break;
                }
                current = (current == BLACK) ? WHITE : BLACK;
                // 重置计时起点（pass 不耗时应扣除刚才加的，但我们已加了，需要回退）
                // 简单处理：将刚加的减去
                auto now2 = chrono::steady_clock::now();
                auto elapsed2 = chrono::duration_cast<chrono::milliseconds>(now2 - turnStart);
                if (current == BLACK) // 注意此时 current 已切换，减的是原玩家的时间
                    blackTime -= elapsed2;
                else
                    whiteTime -= elapsed2;
                turnStart = now2;
                printBoard(board);
                continue;
            } else {
                passCount = 0;
            }

            int row, col;
            if (!parseCoordinate(input, row, col)) {
                cout << "输入格式错误，请重新输入。\n";
                // 扣除无效输入的时间
                auto now2 = chrono::steady_clock::now();
                auto elapsed2 = chrono::duration_cast<chrono::milliseconds>(now2 - turnStart);
                if (current == BLACK)
                    blackTime -= elapsed2;
                else
                    whiteTime -= elapsed2;
                turnStart = now2;
                continue;
            }

            Color tempBoard[19][19];
            for (int i = 0; i < SIZE; i++)
                for (int j = 0; j < SIZE; j++)
                    tempBoard[i][j] = board[i][j];

            auto result = tryMove(row, col, current, tempBoard);
            if (!result.first) {
                cout << "非法落子（无气或已有子），请重新输入。\n";
                auto now2 = chrono::steady_clock::now();
                auto elapsed2 = chrono::duration_cast<chrono::milliseconds>(now2 - turnStart);
                if (current == BLACK)
                    blackTime -= elapsed2;
                else
                    whiteTime -= elapsed2;
                turnStart = now2;
                continue;
            }
            vector<pair<int, int>> captured = result.second;

            // 全局同形检测
            Color nextTurn = (current == BLACK) ? WHITE : BLACK;
            bool duplicate = false;
            for (const auto& st : history) {
                if (st.turn != nextTurn) continue;
                bool same = true;
                for (int i = 0; i < SIZE && same; i++) {
                    for (int j = 0; j < SIZE && same; j++) {
                        if (st.board[i][j] != tempBoard[i][j])
                            same = false;
                    }
                }
                if (same) {
                    duplicate = true;
                    break;
                }
            }

            if (duplicate) {
                cout << "违反全局同形规则（劫争/循环劫），禁止落子。\n";
                auto now2 = chrono::steady_clock::now();
                auto elapsed2 = chrono::duration_cast<chrono::milliseconds>(now2 - turnStart);
                if (current == BLACK)
                    blackTime -= elapsed2;
                else
                    whiteTime -= elapsed2;
                turnStart = now2;
                continue;
            }

            // 执行落子
            for (int i = 0; i < SIZE; i++)
                for (int j = 0; j < SIZE; j++)
                    board[i][j] = tempBoard[i][j];
            lastRow = row;
            lastCol = col;

            State newState;
            for (int i = 0; i < SIZE; i++)
                for (int j = 0; j < SIZE; j++)
                    newState.board[i][j] = board[i][j];
            newState.turn = nextTurn;
            history.push_back(newState);

            if (!captured.empty()) {
                flashCaptured(board, captured);
            } else {
                printBoard(board);
            }

            current = nextTurn;
            turnStart = chrono::steady_clock::now();
            continue;
        }

        // ---------- AI 回合 ----------
        string status = (current == BLACK ? "黑方" : "白方");
        status += " AI 思考中";
        int thinkTime = 500 + (gen() % 1000);
        auto startAnim = chrono::steady_clock::now();
        int dots = 0;
        while (chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - startAnim).count() < thinkTime) {
            string statusLine = status + string(dots % 4, '.');
            printBoard(board, statusLine);
            this_thread::sleep_for(chrono::milliseconds(300));
            dots++;
        }
        printBoard(board, "");

        auto move = aiMove(current, board);
        if (move.first == -1) {
            passCount++;
            if (passCount >= 2) {
                gameOver = true;
                break;
            }
            current = (current == BLACK) ? WHITE : BLACK;
            turnStart = chrono::steady_clock::now();
            continue;
        }
        int row = move.first, col = move.second;

        Color tempBoard[19][19];
        for (int i = 0; i < SIZE; i++)
            for (int j = 0; j < SIZE; j++)
                tempBoard[i][j] = board[i][j];

        auto result = tryMove(row, col, current, tempBoard);
        if (!result.first) {
            passCount++;
            if (passCount >= 2) {
                gameOver = true;
                break;
            }
            current = (current == BLACK) ? WHITE : BLACK;
            turnStart = chrono::steady_clock::now();
            continue;
        }
        vector<pair<int, int>> captured = result.second;

        Color nextTurn = (current == BLACK) ? WHITE : BLACK;
        bool duplicate = false;
        for (const auto& st : history) {
            if (st.turn != nextTurn) continue;
            bool same = true;
            for (int i = 0; i < SIZE && same; i++) {
                for (int j = 0; j < SIZE && same; j++) {
                    if (st.board[i][j] != tempBoard[i][j])
                        same = false;
                }
            }
            if (same) {
                duplicate = true;
                break;
            }
        }

        if (duplicate) {
            passCount++;
            if (passCount >= 2) {
                gameOver = true;
                break;
            }
            current = nextTurn;
            turnStart = chrono::steady_clock::now();
            continue;
        }

        for (int i = 0; i < SIZE; i++)
            for (int j = 0; j < SIZE; j++)
                board[i][j] = tempBoard[i][j];
        lastRow = row;
        lastCol = col;

        State newState;
        for (int i = 0; i < SIZE; i++)
            for (int j = 0; j < SIZE; j++)
                newState.board[i][j] = board[i][j];
        newState.turn = nextTurn;
        history.push_back(newState);

        if (!captured.empty()) {
            flashCaptured(board, captured);
        } else {
            printBoard(board);
        }

        current = nextTurn;
        turnStart = chrono::steady_clock::now();

        if (mode == 3) {
            this_thread::sleep_for(chrono::milliseconds(500));
        }
    }

    // 终局处理
    clearScreen();
    printBoard(board);

    if (resignFlag) {
        string loser = (current == BLACK) ? "黑方" : "白方";
        string winner = (current == BLACK) ? "白方" : "黑方";
        cout << loser << "认输，" << winner << "胜！\n";
    } else {
        ScoreDetail detail = calculateScore(board);
        cout << "\n========== 终局数子 ==========\n";
        cout << "黑方棋子数 : " << detail.blackStones << " 子\n";
        cout << "黑方领地数 : " << detail.blackTerritory << " 目（空点）\n";
        cout << "黑方总子数 : " << (detail.blackStones + detail.blackTerritory) << " 子（未贴子）\n";
        float blackFinal = detail.blackStones + detail.blackTerritory - KOMI;
        cout << "黑方贴子后 : " << blackFinal << " 子\n\n";

        cout << "白方棋子数 : " << detail.whiteStones << " 子\n";
        cout << "白方领地数 : " << detail.whiteTerritory << " 目（空点）\n";
        cout << "白方总子数 : " << (detail.whiteStones + detail.whiteTerritory) << " 子\n";
        cout << "白方贴子后 : " << (float)(detail.whiteStones + detail.whiteTerritory) << " 子\n\n";

        if (blackFinal > detail.whiteStones + detail.whiteTerritory)
            cout << "结果：黑方胜！\n";
        else if (blackFinal < detail.whiteStones + detail.whiteTerritory)
            cout << "结果：白方胜！\n";
        else
            cout << "结果：平局！\n";

        cout << "==============================\n";
    }

    cout << "黑方总用时: " << formatTime(blackTime) << "\n";
    cout << "白方总用时: " << formatTime(whiteTime) << "\n";

    return 0;
}