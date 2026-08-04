#include <iostream>
#include <vector>
#include <string>
#include <bitset>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <random>
#include <memory>
#include <cctype>

// ------------------------------------------------------------
// 常量与辅助函数
// ------------------------------------------------------------
const int BOARD_SIZE = 9;
const int MAX_MOVES = BOARD_SIZE * BOARD_SIZE;

int pos_to_index(int row, int col) { return row * BOARD_SIZE + col; }
void index_to_pos(int idx, int& row, int& col) { row = idx / BOARD_SIZE; col = idx % BOARD_SIZE; }

std::vector<int> neighbors(int idx) {
    static std::vector<int> neigh[MAX_MOVES];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < BOARD_SIZE; ++i) {
            for (int j = 0; j < BOARD_SIZE; ++j) {
                int idx_ = pos_to_index(i, j);
                if (i > 0) neigh[idx_].push_back(pos_to_index(i-1, j));
                if (i < BOARD_SIZE-1) neigh[idx_].push_back(pos_to_index(i+1, j));
                if (j > 0) neigh[idx_].push_back(pos_to_index(i, j-1));
                if (j < BOARD_SIZE-1) neigh[idx_].push_back(pos_to_index(i, j+1));
            }
        }
        init = true;
    }
    return neigh[idx];
}

// ------------------------------------------------------------
// 1. 位棋盘 & 规则引擎
// ------------------------------------------------------------
struct BitBoard {
    std::bitset<MAX_MOVES> black;
    std::bitset<MAX_MOVES> white;
    int ko_idx = -1;

    bool empty(int idx) const { return !black[idx] && !white[idx]; }
    bool empty(int row, int col) const { return empty(pos_to_index(row, col)); }
};

std::vector<int> get_group(const BitBoard& board, int idx, bool black_turn) {
    const auto& stones = black_turn ? board.black : board.white;
    std::vector<int> group;
    std::vector<int> stack = {idx};
    std::bitset<MAX_MOVES> visited;
    visited[idx] = true;
    while (!stack.empty()) {
        int cur = stack.back(); stack.pop_back();
        group.push_back(cur);
        for (int nb : neighbors(cur)) {
            if (!visited[nb] && stones[nb]) {
                visited[nb] = true;
                stack.push_back(nb);
            }
        }
    }
    return group;
}

int count_liberties(const BitBoard& board, const std::vector<int>& group) {
    std::bitset<MAX_MOVES> lib;
    for (int idx : group) {
        for (int nb : neighbors(idx)) {
            if (board.empty(nb)) lib[nb] = true;
        }
    }
    return lib.count();
}

BitBoard apply_move(const BitBoard& board, int idx, bool black_turn) {
    if (!board.empty(idx)) return board;
    BitBoard nb = board;
    if (black_turn) nb.black[idx] = true;
    else nb.white[idx] = true;

    bool opp = !black_turn;
    const auto& opp_stones = opp ? nb.white : nb.black;
    for (int nb_idx : neighbors(idx)) {
        if (opp_stones[nb_idx]) {
            auto group = get_group(nb, nb_idx, opp);
            if (count_liberties(nb, group) == 0) {
                for (int g : group) {
                    if (opp) nb.white[g] = false;
                    else nb.black[g] = false;
                }
            }
        }
    }

    auto self_group = get_group(nb, idx, black_turn);
    if (count_liberties(nb, self_group) == 0) return board;

    // 打劫检测：简单版
    int captured = 0, captured_idx = -1;
    const auto& old_opp = opp ? board.white : board.black;
    const auto& new_opp = opp ? nb.white : nb.black;
    for (int i = 0; i < MAX_MOVES; ++i) {
        if (old_opp[i] && !new_opp[i]) { captured++; captured_idx = i; }
    }
    if (captured == 1 && captured_idx != -1) {
        int empty_neighbors = 0;
        for (int nb_idx : neighbors(captured_idx)) if (nb.empty(nb_idx)) empty_neighbors++;
        if (empty_neighbors == 1) {
            for (int nb_idx : neighbors(captured_idx)) {
                if (nb.empty(nb_idx)) { nb.ko_idx = nb_idx; break; }
            }
        }
    } else {
        nb.ko_idx = -1;
    }
    return nb;
}

std::vector<int> get_legal_moves(const BitBoard& board, bool black_turn) {
    std::vector<int> moves;
    for (int i = 0; i < MAX_MOVES; ++i) {
        if (board.empty(i) && i != board.ko_idx) {
            BitBoard nb = apply_move(board, i, black_turn);
            if (nb.black != board.black || nb.white != board.white)
                moves.push_back(i);
        }
    }
    return moves;
}

// ------------------------------------------------------------
// 2. 模拟神经网络（启发式）
// ------------------------------------------------------------
struct NetworkOutput {
    std::vector<double> policy;
    double value;
};

NetworkOutput infer(const BitBoard& board, bool black_turn) {
    NetworkOutput out;
    out.policy.assign(MAX_MOVES, 0.0);
    auto legal = get_legal_moves(board, black_turn);
    if (legal.empty()) {
        out.value = black_turn ? -0.9 : 0.9;
        return out;
    }

    std::vector<double> scores;
    for (int move : legal) {
        double score = 0.0;
        BitBoard nb = apply_move(board, move, black_turn);
        int captured = 0;
        const auto& opp = black_turn ? board.white : board.black;
        const auto& new_opp = black_turn ? nb.white : nb.black;
        for (int i = 0; i < MAX_MOVES; ++i) if (opp[i] && !new_opp[i]) captured++;
        score += captured * 10.0;
        auto group = get_group(nb, move, black_turn);
        score += count_liberties(nb, group) * 0.5;
        int row, col; index_to_pos(move, row, col);
        double dist = std::abs(row - 4) + std::abs(col - 4);
        score += (4 - dist) * 0.3;
        if (row == 0 || row == 8 || col == 0 || col == 8) score -= 1.0;
        scores.push_back(score);
    }
    double max_s = *std::max_element(scores.begin(), scores.end());
    double sum_exp = 0.0;
    for (double s : scores) sum_exp += std::exp(s - max_s);
    for (size_t i = 0; i < legal.size(); ++i)
        out.policy[legal[i]] = std::exp(scores[i] - max_s) / sum_exp;

    // value
    int black_stones = board.black.count();
    int white_stones = board.white.count();
    int black_libs = 0, white_libs = 0;
    for (int i = 0; i < MAX_MOVES; ++i) {
        if (board.black[i]) {
            for (int nb : neighbors(i)) if (board.empty(nb)) black_libs++;
        } else if (board.white[i]) {
            for (int nb : neighbors(i)) if (board.empty(nb)) white_libs++;
        }
    }
    double raw = (black_stones - white_stones) + (black_libs - white_libs) * 0.3;
    double value = std::tanh(raw * 0.2);
    if (!black_turn) value = -value;
    out.value = value;
    return out;
}

// ------------------------------------------------------------
// 3. MCTS 节点
// ------------------------------------------------------------
struct Node {
    Node* parent = nullptr;
    int move = -1;
    std::unordered_map<int, Node*> children;
    int N = 0;
    double W = 0.0;
    double P = 0.0;
    double Q() const { return N > 0 ? W / N : 0.0; }
    double U(double cpuct, double parent_N) const {
        return cpuct * P * std::sqrt(parent_N) / (1.0 + N);
    }
    ~Node() { for (auto& kv : children) delete kv.second; }
};

// ------------------------------------------------------------
// 4. MCTS 搜索器
// ------------------------------------------------------------
class MCTS {
public:
    MCTS() : root(nullptr), root_board(BitBoard()), black_turn(true) {
        root = new Node();
    }
    ~MCTS() { delete root; }

    void reset(const BitBoard& board, bool turn) {
        delete root;
        root = new Node();
        root_board = board;
        black_turn = turn;
    }

    // 重用子树：在人类落子后调用
    bool reuse_subtree(int human_move) {
        if (!root) return false;
        auto it = root->children.find(human_move);
        if (it == root->children.end()) return false;
        Node* child = it->second;
        child->parent = nullptr;
        root->children.erase(human_move);
        delete root;
        root = child;
        root_board = apply_move(root_board, human_move, black_turn);
        black_turn = !black_turn;
        return true;
    }

    int search(int time_budget_ms = 800) {
        if (!root) return -1;
        auto start = std::chrono::steady_clock::now();
        while (true) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() >= time_budget_ms)
                break;
            Node* leaf = select(root);
            double value = expand_and_evaluate(leaf);
            backup(leaf, value);
        }
        if (root->children.empty()) return -1;
        int best = -1, best_N = -1;
        for (auto& kv : root->children) {
            if (kv.second->N > best_N) {
                best_N = kv.second->N;
                best = kv.first;
            }
        }
        return best;
    }

private:
    Node* root;
    BitBoard root_board;
    bool black_turn;
    const double CPUCT = 1.2;

    Node* select(Node* node) {
        if (node->children.empty()) return node;
        Node* best = nullptr;
        double best_score = -1e9;
        double parent_N = (double)node->N + 1e-6;
        for (auto& kv : node->children) {
            Node* child = kv.second;
            double score = child->Q() + child->U(CPUCT, parent_N);
            if (score > best_score) { best_score = score; best = child; }
        }
        return best ? select(best) : node;
    }

    double expand_and_evaluate(Node* leaf) {
        BitBoard board = get_board(leaf);
        bool turn = get_turn(leaf);
        auto legal = get_legal_moves(board, turn);
        if (legal.empty()) return turn ? -0.9 : 0.9;
        NetworkOutput out = infer(board, turn);
        for (int move : legal) {
            Node* child = new Node();
            child->parent = leaf;
            child->move = move;
            child->P = out.policy[move];
            leaf->children[move] = child;
        }
        return out.value;
    }

    void backup(Node* leaf, double value) {
        Node* cur = leaf;
        double v = value;
        while (cur) {
            cur->N += 1;
            cur->W += v;
            v = -v;
            cur = cur->parent;
        }
    }

    BitBoard get_board(Node* node) {
        std::vector<int> moves;
        Node* cur = node;
        while (cur->parent) { moves.push_back(cur->move); cur = cur->parent; }
        std::reverse(moves.begin(), moves.end());
        BitBoard board = root_board;
        bool turn = black_turn;
        for (int m : moves) {
            board = apply_move(board, m, turn);
            turn = !turn;
        }
        return board;
    }

    bool get_turn(Node* node) {
        int depth = 0; Node* cur = node;
        while (cur->parent) { depth++; cur = cur->parent; }
        return (depth % 2 == 0) ? black_turn : !black_turn;
    }
};

// ------------------------------------------------------------
// 5. UI（控制台）
// ------------------------------------------------------------
void print_board(const BitBoard& board) {
    std::cout << "  A B C D E F G H I\n";
    for (int r = 0; r < BOARD_SIZE; ++r) {
        std::cout << (r + 1) << " ";
        for (int c = 0; c < BOARD_SIZE; ++c) {
            int idx = pos_to_index(r, c);
            if (board.black[idx]) std::cout << "X ";
            else if (board.white[idx]) std::cout << "O ";
            else std::cout << ". ";
        }
        std::cout << "\n";
    }
}

bool parse_input(const std::string& s, int& move) {
    if (s == "pass") { move = -1; return true; }
    if (s == "q" || s == "quit") { move = -2; return true; }
    std::string input = s;
    std::transform(input.begin(), input.end(), input.begin(), ::toupper);
    if (input.size() == 2 && input[0] >= 'A' && input[0] <= 'I' && input[1] >= '1' && input[1] <= '9') {
        int col = input[0] - 'A';
        int row = input[1] - '1';
        move = pos_to_index(row, col);
        return true;
    }
    if (input.size() == 2 && input[0] >= '1' && input[0] <= '9' && input[1] >= 'A' && input[1] <= 'I') {
        int row = input[0] - '1';
        int col = input[1] - 'A';
        move = pos_to_index(row, col);
        return true;
    }
    return false;
}

int main() {
    std::cout << "9x9 围棋 AI (MCTS + 启发式网络模拟)\n";
    std::cout << "你执黑(X)，AI执白(O)\n";
    std::cout << "输入坐标如 A1, pass 弃权, q 退出\n\n";

    MCTS mcts;
    BitBoard board;
    bool black_turn = true;
    int pass_count = 0;

    while (true) {
        print_board(board);
        if (black_turn) {
            std::cout << "黑方(你)落子: ";
            std::string input;
            std::getline(std::cin, input);
            int move;
            if (!parse_input(input, move)) {
                std::cout << "无效输入，重新输入\n";
                continue;
            }
            if (move == -2) { std::cout << "退出\n"; break; }
            if (move == -1) { // pass
                pass_count++;
                if (pass_count >= 2) {
                    std::cout << "双方连续弃权，对局结束\n";
                    break;
                }
                black_turn = false;
                continue;
            }
            pass_count = 0;
            if (!board.empty(move)) {
                std::cout << "该位置已有棋子\n";
                continue;
            }
            BitBoard new_board = apply_move(board, move, black_turn);
            if (new_board.black == board.black && new_board.white == board.white) {
                std::cout << "非法落子\n";
                continue;
            }
            board = new_board;
            black_turn = false;
            // 尝试重用之前的搜索树（如果人类落子在树中）
            if (!mcts.reuse_subtree(move)) {
                // 不在树中，重置根
                mcts.reset(board, black_turn);
            }
        } else {
            std::cout << "AI 思考中...\n";
            // AI 走棋（使用搜索）
            int move = mcts.search(800); // 800ms
            if (move == -1) {
                // AI 无合法走法，自动 pass
                std::cout << "AI 无棋可下，自动弃权\n";
                pass_count++;
                if (pass_count >= 2) {
                    std::cout << "双方连续弃权，对局结束\n";
                    break;
                }
                black_turn = true;
                // 重置根，因为AI pass导致根需要更新
                mcts.reset(board, black_turn);
                continue;
            }
            pass_count = 0;
            BitBoard new_board = apply_move(board, move, false);
            if (new_board.black == board.black && new_board.white == board.white) {
                std::cout << "AI 内部错误，落子非法\n";
                break;
            }
            board = new_board;
            black_turn = true;
            // AI 落子后，重置根（丢弃旧树，这样简单，但失去重用）
            // 为了演示重用，我们可以保留根，但为了简单，我们重置。
            // 实际使用应实现更精细的重用。
            mcts.reset(board, black_turn);
        }
    }

    // 终局显示
    print_board(board);
    std::cout << "对局结束！\n";
    return 0;
}