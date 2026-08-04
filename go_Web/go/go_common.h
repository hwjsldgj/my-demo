#ifndef GO_COMMON_H
#define GO_COMMON_H

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <random>
#include <cmath>
#include <chrono>
#include <unordered_set>
#include <memory>
#include <bitset>
#include <unordered_map>

// 前向声明 ONNX Runtime 类型
namespace Ort {
    class Session;
    struct MemoryInfo;
    class Value;
}

extern int BOARD_SIZE;
extern float KOMI;
const int MAX_SIZE = 19;
enum Color { EMPTY, BLACK, WHITE };

bool inBounds(int r, int c);
std::vector<std::pair<int,int>> getNeighbors(int r, int c);
std::vector<std::pair<int,int>> getGroup(int r, int c, Color color, const Color board[MAX_SIZE][MAX_SIZE]);
int countLiberties(const std::vector<std::pair<int,int>>& group, const Color board[MAX_SIZE][MAX_SIZE]);
void removeDeadStones(Color board[MAX_SIZE][MAX_SIZE], Color color);
bool tryMove(int r, int c, Color color, Color board[MAX_SIZE][MAX_SIZE]);
bool isStarPoint(int r, int c);
void printBoard(const Color board[MAX_SIZE][MAX_SIZE]);

struct ScoreDetail { int blackStones, blackTerritory, whiteStones, whiteTerritory; };
ScoreDetail calculateScore(const Color board[MAX_SIZE][MAX_SIZE]);
bool parseCoordinate(const std::string& input, int& row, int& col);

class AIBoard {
public:
    int grid[MAX_SIZE][MAX_SIZE];
    AIBoard();
    void clear();
    void fromGlobal(const Color gb[MAX_SIZE][MAX_SIZE]);
    bool inBoard(int r, int c) const;
    int getLiberties(int r, int c) const;
    int groupLiberties(int r, int c, bool visited[MAX_SIZE][MAX_SIZE]) const;
    bool isValidMove(int r, int c, int color) const;
    void removeGroup(int r, int c);
    bool placeStone(int r, int c, int color);
    std::vector<std::pair<int,int>> getLegalMoves(int color) const;
    int countStones(int color) const;
    uint64_t hash() const;
};

class RuleBasedAI {
public:
    std::pair<int,int> getMove(const AIBoard& board, int color);
};

class GreedyAI {
public:
    double evaluateMove(const AIBoard& board, int r, int c, int color);
    std::pair<int,int> getMove(const AIBoard& board, int color);
};

class Search2AI {
public:
    double evaluate(const AIBoard& board, int color);
    std::pair<int,int> getMove(const AIBoard& board, int color);
};

class Search4AI {
public:
    int searchDepth;
    int topK;
    Search4AI(int depth, int k);
    double heuristicScore(const AIBoard& board, int r, int c, int color);
    double evaluate(const AIBoard& board, int color);
    std::vector<std::pair<int,int>> getTopMoves(const AIBoard& board, int color, int topK);
    double dfs(AIBoard board, int color, int depth, int maxDepth, double alpha, double beta);
    std::pair<int,int> getMove(const AIBoard& board, int color);
};

// ---------- 特征编码器 ----------
class FeatureEncoder {
public:
    static std::vector<float> encode(const AIBoard& board, Color to_play);
};

// ---------- MCTS AI ----------
class MCTS_AI {
public:
    MCTS_AI();
    ~MCTS_AI();

    void apply_human_move(int row, int col, int color);
    std::pair<int,int> getMove(const AIBoard& board, int color);
    void set_time_limit(int ms) { time_limit_ms = ms; }

    // 初始化 ONNX 会话（静态）
    static void init_onnx_session(const std::string& model_path);

private:
    int time_limit_ms = 0;

    struct BitBoard {
        std::bitset<361> black;
        std::bitset<361> white;
        int ko_idx = -1;
        bool empty(int idx) const { return !black[idx] && !white[idx]; }
    };

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

    struct NetworkOutput {
        std::vector<double> policy;
        double value;
    };

    class Searcher {
    public:
        Searcher() : root(nullptr), root_board(BitBoard()), black_turn(true) { root = new Node(); }
        ~Searcher() { delete root; }
        void reset(const BitBoard& board, bool turn);
        bool reuse_subtree(int human_move);
        int search(int time_budget_ms, double cpuct = 1.5, bool add_noise = true);
        const BitBoard& get_board() const { return root_board; }
        bool get_turn() const { return black_turn; }
    private:
        Node* root;
        BitBoard root_board;
        bool black_turn;
        Node* select(Node* node, double cpuct);
        double expand_and_evaluate(Node* leaf);
        void backup(Node* leaf, double value);
        BitBoard get_board(Node* node);
        bool get_turn(Node* node);
        void add_dirichlet_noise(Node* node, double alpha);

        void set_root(Node* new_root) { 
            if (root && root != new_root) delete root; 
            root = new_root; 
            if (root) root->parent = nullptr; 
        }
        Node* get_root() const { return root; }
    };

    // 静态辅助函数
    static std::vector<int> neighbors(int idx);
    static BitBoard apply_move(const BitBoard& board, int idx, bool black_turn);
    static std::vector<int> get_legal_moves(const BitBoard& board, bool black_turn);
    static std::vector<int> get_group(const BitBoard& board, int idx, bool black_turn);
    static int count_liberties(const BitBoard& board, const std::vector<int>& group);
    static NetworkOutput infer(const BitBoard& board, bool black_turn);
    static NetworkOutput fallback_heuristic(const BitBoard& board, bool black_turn);

    static BitBoard to_bitboard(const AIBoard& board);
    static AIBoard from_bitboard(const BitBoard& board);

    bool is_initialized = false;
    BitBoard current_board;
    bool current_black_turn;
    Searcher searcher;

    static Ort::Session* session;
    static bool session_initialized;
    static std::string model_path;

    static bool run_onnx_inference(const std::vector<float>& input_tensor, std::vector<double>& policy, double& value);
};

#endif