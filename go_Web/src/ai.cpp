#ifdef _WIN32
#include <windows.h>
// 禁用 ONNX Runtime 的 SAL 宏以避免与 Windows SDK 冲突
#define ONNXRUNTIME_API_USE_SAL 0
#endif
#include "go_common.h"
#include <chrono>
#include <random>
#include <fstream>
#include <onnxruntime_cxx_api.h>

// ---------- 静态成员 ----------
Ort::Session* MCTS_AI::session = nullptr;
bool MCTS_AI::session_initialized = false;
std::string MCTS_AI::model_path = "";
static Ort::Env* g_env = nullptr;  

// ---------- AIBoard 实现 ----------
AIBoard::AIBoard() { clear(); }

void AIBoard::clear() {
    for(int i=0;i<BOARD_SIZE;i++) for(int j=0;j<BOARD_SIZE;j++) grid[i][j]=EMPTY;
}

void AIBoard::fromGlobal(const Color gb[MAX_SIZE][MAX_SIZE]) {
    for(int i=0;i<BOARD_SIZE;i++) for(int j=0;j<BOARD_SIZE;j++) grid[i][j]=(int)gb[i][j];
}

bool AIBoard::inBoard(int r,int c) const {
    return r>=0 && r<BOARD_SIZE && c>=0 && c<BOARD_SIZE;
}

int AIBoard::getLiberties(int r,int c) const {
    int libs=0;
    int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
    for(int k=0;k<4;k++){ int nr=r+dr[k], nc=c+dc[k]; if(inBoard(nr,nc) && grid[nr][nc]==EMPTY) libs++; }
    return libs;
}

int AIBoard::groupLiberties(int r,int c, bool visited[MAX_SIZE][MAX_SIZE]) const {
    if(!inBoard(r,c) || visited[r][c] || grid[r][c]==EMPTY) return 0;
    int color=grid[r][c];
    visited[r][c]=true;
    int libs=getLiberties(r,c);
    int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
    for(int k=0;k<4;k++){ int nr=r+dr[k], nc=c+dc[k]; if(inBoard(nr,nc) && !visited[nr][nc] && grid[nr][nc]==color) libs += groupLiberties(nr,nc,visited); }
    return libs;
}

bool AIBoard::isValidMove(int r,int c,int color) const {
    if(!inBoard(r,c) || grid[r][c]!=EMPTY) return false;
    AIBoard temp=*this;
    temp.grid[r][c]=color;
    int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
    for(int k=0;k<4;k++){
        int nr=r+dr[k], nc=c+dc[k];
        if(temp.inBoard(nr,nc) && temp.grid[nr][nc]!=EMPTY && temp.grid[nr][nc]!=color){
            bool vis[MAX_SIZE][MAX_SIZE]={false};
            if(temp.groupLiberties(nr,nc,vis)==0) temp.removeGroup(nr,nc);
        }
    }
    bool visSelf[MAX_SIZE][MAX_SIZE]={false};
    if(temp.groupLiberties(r,c,visSelf)==0) return false;
    return true;
}

void AIBoard::removeGroup(int r,int c) {
    if(!inBoard(r,c) || grid[r][c]==EMPTY) return;
    int color=grid[r][c];
    grid[r][c]=EMPTY;
    int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
    for(int k=0;k<4;k++){ int nr=r+dr[k], nc=c+dc[k]; if(inBoard(nr,nc) && grid[nr][nc]==color) removeGroup(nr,nc); }
}

bool AIBoard::placeStone(int r,int c,int color) {
    if(!isValidMove(r,c,color)) return false;
    AIBoard temp=*this;
    temp.grid[r][c]=color;
    int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
    for(int k=0;k<4;k++){
        int nr=r+dr[k], nc=c+dc[k];
        if(temp.inBoard(nr,nc) && temp.grid[nr][nc]!=EMPTY && temp.grid[nr][nc]!=color){
            bool vis[MAX_SIZE][MAX_SIZE]={false};
            if(temp.groupLiberties(nr,nc,vis)==0) temp.removeGroup(nr,nc);
        }
    }
    bool visSelf[MAX_SIZE][MAX_SIZE]={false};
    if(temp.groupLiberties(r,c,visSelf)==0) return false;
    *this=temp;
    return true;
}

std::vector<std::pair<int,int>> AIBoard::getLegalMoves(int color) const {
    std::vector<std::pair<int,int>> moves;
    for(int i=0;i<BOARD_SIZE;i++) for(int j=0;j<BOARD_SIZE;j++) if(grid[i][j]==EMPTY && isValidMove(i,j,color)) moves.push_back({i,j});
    return moves;
}

int AIBoard::countStones(int color) const {
    int cnt=0;
    for(int i=0;i<BOARD_SIZE;i++) for(int j=0;j<BOARD_SIZE;j++) if(grid[i][j]==color) cnt++;
    return cnt;
}

uint64_t AIBoard::hash() const {
    uint64_t h=0;
    for(int i=0;i<BOARD_SIZE;i++) for(int j=0;j<BOARD_SIZE;j++) h = h*3 + grid[i][j];
    return h;
}

// ---------- RuleBasedAI ----------
std::pair<int,int> RuleBasedAI::getMove(const AIBoard& board, int color) {
    auto moves = board.getLegalMoves(color);
    if(moves.empty()) return {-1,-1};
    for(auto [r,c] : moves){
        AIBoard temp=board;
        temp.placeStone(r,c,color);
        int opp=(color==BLACK)?WHITE:BLACK;
        bool captured=false;
        int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
        for(int k=0;k<4;k++){ int nr=r+dr[k], nc=c+dc[k]; if(temp.inBoard(nr,nc) && temp.grid[nr][nc]==opp){ bool vis[MAX_SIZE][MAX_SIZE]={false}; if(temp.groupLiberties(nr,nc,vis)==0){ captured=true; break; } } }
        if(captured) return {r,c};
    }
    for(int i=0;i<BOARD_SIZE;i++) for(int j=0;j<BOARD_SIZE;j++){
        if(board.grid[i][j]==color){
            int libs=board.getLiberties(i,j);
            if(libs==1){
                int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
                for(int k=0;k<4;k++){ int nr=i+dr[k], nc=j+dc[k]; if(board.inBoard(nr,nc) && board.grid[nr][nc]==EMPTY && board.isValidMove(nr,nc,color)) return {nr,nc}; }
            }
        }
    }
    int idx=rand()%moves.size();
    return moves[idx];
}

// ---------- GreedyAI ----------
double GreedyAI::evaluateMove(const AIBoard& board, int r, int c, int color) {
    double score=0.0;
    AIBoard temp=board;
    temp.placeStone(r,c,color);
    int opp=(color==BLACK)?WHITE:BLACK;
    int captured=0;
    int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
    for(int k=0;k<4;k++){ int nr=r+dr[k], nc=c+dc[k]; if(temp.inBoard(nr,nc) && temp.grid[nr][nc]==opp){ bool vis[MAX_SIZE][MAX_SIZE]={false}; if(temp.groupLiberties(nr,nc,vis)==0) captured++; } }
    score += captured*15.0;
    int myLibs=temp.getLiberties(r,c);
    int oldLibs=board.getLiberties(r,c);
    score += (myLibs-oldLibs)*3.0;
    for(int k=0;k<4;k++){ int nr=r+dr[k], nc=c+dc[k]; if(temp.inBoard(nr,nc) && temp.grid[nr][nc]==color) score += 2.0; }
    for(int k=0;k<4;k++){ int nr=r+dr[k], nc=c+dc[k]; if(temp.inBoard(nr,nc) && temp.grid[nr][nc]==opp){ bool vis[MAX_SIZE][MAX_SIZE]={false}; if(temp.groupLiberties(nr,nc,vis)==1) score += 8.0; } }
    int center = (BOARD_SIZE-1)/2;
    double centerDist = abs(r-center)+abs(c-center);
    score += (BOARD_SIZE - 1 - centerDist) * 0.8;
    if(r==0 || r==BOARD_SIZE-1 || c==0 || c==BOARD_SIZE-1) score -= 1.0;
    return score;
}

std::pair<int,int> GreedyAI::getMove(const AIBoard& board, int color) {
    auto moves = board.getLegalMoves(color);
    if(moves.empty()) return {-1,-1};
    if(board.countStones(BLACK)==0 && board.countStones(WHITE)==0) return {BOARD_SIZE/2, BOARD_SIZE/2};
    double bestScore=-1e9;
    std::vector<std::pair<int,int>> bestMoves;
    for(auto [r,c] : moves){
        double sc = evaluateMove(board,r,c,color);
        if(sc > bestScore){ bestScore=sc; bestMoves.clear(); bestMoves.push_back({r,c}); }
        else if(fabs(sc-bestScore)<1e-6) bestMoves.push_back({r,c});
    }
    int idx=rand()%bestMoves.size();
    return bestMoves[idx];
}

// ---------- Search2AI ----------
double Search2AI::evaluate(const AIBoard& board, int color) {
    Color temp[MAX_SIZE][MAX_SIZE];
    for(int i=0;i<BOARD_SIZE;i++) for(int j=0;j<BOARD_SIZE;j++) temp[i][j]=(Color)board.grid[i][j];
    ScoreDetail sd = calculateScore(temp);
    int blackTotal = sd.blackStones + sd.blackTerritory;
    int whiteTotal = sd.whiteStones + sd.whiteTerritory;
    if(color==BLACK) return blackTotal - whiteTotal;
    else return whiteTotal - blackTotal;
}

std::pair<int,int> Search2AI::getMove(const AIBoard& board, int color) {
    auto moves = board.getLegalMoves(color);
    if(moves.empty()) return {-1,-1};
    if(board.countStones(BLACK)==0 && board.countStones(WHITE)==0) return {BOARD_SIZE/2, BOARD_SIZE/2};
    int opp = (color==BLACK)?WHITE:BLACK;
    double bestScore = -1e9;
    std::vector<std::pair<int,int>> bestMoves;
    for(auto [r,c] : moves){
        AIBoard board1 = board;
        board1.placeStone(r,c,color);
        auto oppMoves = board1.getLegalMoves(opp);
        if(oppMoves.empty()){
            double score = evaluate(board1, color);
            if(score > bestScore){ bestScore=score; bestMoves.clear(); bestMoves.push_back({r,c}); }
            else if(fabs(score-bestScore)<1e-6) bestMoves.push_back({r,c});
            continue;
        }
        double worstForMe = 1e9;
        for(auto [or2, oc2] : oppMoves){
            AIBoard board2 = board1;
            board2.placeStone(or2, oc2, opp);
            double oppScore = evaluate(board2, opp);
            if(oppScore < worstForMe) worstForMe = oppScore;
        }
        double myScore = -worstForMe;
        if(myScore > bestScore){ bestScore=myScore; bestMoves.clear(); bestMoves.push_back({r,c}); }
        else if(fabs(myScore-bestScore)<1e-6) bestMoves.push_back({r,c});
    }
    if(bestMoves.empty()) return moves[0];
    int idx=rand()%bestMoves.size();
    return bestMoves[idx];
}

// ---------- Search4AI ----------
Search4AI::Search4AI(int depth, int k) : searchDepth(depth), topK(k) {}

double Search4AI::heuristicScore(const AIBoard& board, int r, int c, int color) {
    double score=0.0;
    AIBoard temp=board;
    temp.placeStone(r,c,color);
    int opp=(color==BLACK)?WHITE:BLACK;
    int captured=0;
    int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
    for(int k=0;k<4;k++){ int nr=r+dr[k], nc=c+dc[k]; if(temp.inBoard(nr,nc) && temp.grid[nr][nc]==opp){ bool vis[MAX_SIZE][MAX_SIZE]={false}; if(temp.groupLiberties(nr,nc,vis)==0) captured++; } }
    score += captured*20.0;
    int myLibs=temp.getLiberties(r,c);
    int oldLibs=board.getLiberties(r,c);
    score += (myLibs-oldLibs)*4.0;
    for(int k=0;k<4;k++){ int nr=r+dr[k], nc=c+dc[k]; if(temp.inBoard(nr,nc) && temp.grid[nr][nc]==color) score+=3.0; }
    for(int k=0;k<4;k++){ int nr=r+dr[k], nc=c+dc[k]; if(temp.inBoard(nr,nc) && temp.grid[nr][nc]==opp){ bool vis[MAX_SIZE][MAX_SIZE]={false}; if(temp.groupLiberties(nr,nc,vis)==1) score+=12.0; } }
    int center=(BOARD_SIZE-1)/2;
    double centerDist=abs(r-center)+abs(c-center);
    score += (BOARD_SIZE-1-centerDist)*1.0;
    if(r==0 || r==BOARD_SIZE-1 || c==0 || c==BOARD_SIZE-1) score-=2.0;
    return score;
}

double Search4AI::evaluate(const AIBoard& board, int color) {
    Color temp[MAX_SIZE][MAX_SIZE];
    for(int i=0;i<BOARD_SIZE;i++) for(int j=0;j<BOARD_SIZE;j++) temp[i][j]=(Color)board.grid[i][j];
    ScoreDetail sd = calculateScore(temp);
    int blackTotal = sd.blackStones + sd.blackTerritory;
    int whiteTotal = sd.whiteStones + sd.whiteTerritory;
    if(color==BLACK) return blackTotal - whiteTotal;
    else return whiteTotal - blackTotal;
}

std::vector<std::pair<int,int>> Search4AI::getTopMoves(const AIBoard& board, int color, int topK) {
    auto moves = board.getLegalMoves(color);
    if(moves.empty()) return {};
    std::vector<std::pair<double, std::pair<int,int>>> scored;
    for(auto m : moves){
        double sc = heuristicScore(board, m.first, m.second, color);
        scored.push_back({sc, m});
    }
    std::sort(scored.begin(), scored.end(), [](auto& a, auto& b){ return a.first > b.first; });
    std::vector<std::pair<int,int>> result;
    for(int i=0; i<std::min(topK, (int)scored.size()); ++i) result.push_back(scored[i].second);
    return result;
}

double Search4AI::dfs(AIBoard board, int color, int depth, int maxDepth, double alpha, double beta) {
    if(depth == maxDepth) return evaluate(board, color);
    int opp = (color==BLACK)?WHITE:BLACK;
    auto moves = getTopMoves(board, color, topK);
    if(moves.empty()){
        return dfs(board, opp, depth+1, maxDepth, alpha, beta);
    }
    if(color == BLACK){
        double best = -1e9;
        for(auto m : moves){
            AIBoard nb = board;
            nb.placeStone(m.first, m.second, color);
            double val = dfs(nb, opp, depth+1, maxDepth, alpha, beta);
            best = std::max(best, val);
            alpha = std::max(alpha, val);
            if(beta <= alpha) break;
        }
        return best;
    } else {
        double best = 1e9;
        for(auto m : moves){
            AIBoard nb = board;
            nb.placeStone(m.first, m.second, color);
            double val = dfs(nb, opp, depth+1, maxDepth, alpha, beta);
            best = std::min(best, val);
            beta = std::min(beta, val);
            if(beta <= alpha) break;
        }
        return best;
    }
}

std::pair<int,int> Search4AI::getMove(const AIBoard& board, int color) {
    if(board.countStones(BLACK)==0 && board.countStones(WHITE)==0) return {BOARD_SIZE/2, BOARD_SIZE/2};
    auto topMoves = getTopMoves(board, color, topK);
    if(topMoves.empty()) return {-1,-1};
    int opp = (color==BLACK)?WHITE:BLACK;
    double bestScore = -1e9;
    std::vector<std::pair<int,int>> bestMoves;
    for(auto m : topMoves){
        AIBoard nb = board;
        nb.placeStone(m.first, m.second, color);
        double score = dfs(nb, opp, 1, searchDepth, -1e9, 1e9);
        if(score > bestScore){ bestScore=score; bestMoves.clear(); bestMoves.push_back(m); }
        else if(fabs(score-bestScore)<1e-6) bestMoves.push_back(m);
    }
    if(bestMoves.empty()) return topMoves[0];
    int idx=rand()%bestMoves.size();
    return bestMoves[idx];
}

// ============================================================
// ---------- 特征编码器（关键修改：支持 9 路填充到 19 路） ----------
// ============================================================
std::vector<float> FeatureEncoder::encode(const AIBoard& board, Color to_play) {
    int target_size = 19;  // 模型固定输入 19x19
    int input_size = BOARD_SIZE;
    int total = target_size * target_size;
    std::vector<float> tensor(3 * target_size * target_size, 0.0f);

    // 计算偏移量（将输入棋盘居中放置）
    int offset = (target_size - input_size) / 2; // 9 路时 offset=5，19 路时 offset=0

    bool swap = (to_play == WHITE);

    for (int r = 0; r < input_size; ++r) {
        for (int c = 0; c < input_size; ++c) {
            int tr = r + offset;
            int tc = c + offset;
            // 如果 BOARD_SIZE == 19，offset=0，直接对应
            int dst_idx = tr * target_size + tc;
            Color cell = static_cast<Color>(board.grid[r][c]);

            if (swap) {
                if (cell == WHITE) tensor[0 * total + dst_idx] = 1.0f;
                else if (cell == BLACK) tensor[1 * total + dst_idx] = 1.0f;
            } else {
                if (cell == BLACK) tensor[0 * total + dst_idx] = 1.0f;
                else if (cell == WHITE) tensor[1 * total + dst_idx] = 1.0f;
            }
            // 通道 2：全 1（根据 TinyGo 模型要求）
            tensor[2 * total + dst_idx] = 1.0f;
        }
    }
    return tensor;
}

// ============================================================
// ---------- MCTS_AI 实现 ----------
// ============================================================

MCTS_AI::MCTS_AI() : is_initialized(false) {}

MCTS_AI::~MCTS_AI() {}

std::vector<int> MCTS_AI::neighbors(int idx) {
    static std::vector<int> neigh[361];
    static bool init = false;
    if (!init) {
        int max = 19;
        for (int i = 0; i < max; ++i) {
            for (int j = 0; j < max; ++j) {
                int idx_ = i * max + j;
                if (i > 0) neigh[idx_].push_back((i-1)*max + j);
                if (i < max-1) neigh[idx_].push_back((i+1)*max + j);
                if (j > 0) neigh[idx_].push_back(i*max + (j-1));
                if (j < max-1) neigh[idx_].push_back(i*max + (j+1));
            }
        }
        init = true;
    }
    return neigh[idx];
}

MCTS_AI::BitBoard MCTS_AI::apply_move(const BitBoard& board, int idx, bool black_turn) {
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

    int captured = 0, captured_idx = -1;
    const auto& old_opp = opp ? board.white : board.black;
    const auto& new_opp = opp ? nb.white : nb.black;
    int total = BOARD_SIZE * BOARD_SIZE;
    for (int i = 0; i < total; ++i) {
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

std::vector<int> MCTS_AI::get_group(const BitBoard& board, int idx, bool black_turn) {
    const auto& stones = black_turn ? board.black : board.white;
    std::vector<int> group;
    std::vector<int> stack = {idx};
    std::bitset<361> visited;
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

int MCTS_AI::count_liberties(const BitBoard& board, const std::vector<int>& group) {
    std::bitset<361> lib;
    for (int idx : group) {
        for (int nb : neighbors(idx)) {
            if (board.empty(nb)) lib[nb] = true;
        }
    }
    return lib.count();
}

std::vector<int> MCTS_AI::get_legal_moves(const BitBoard& board, bool black_turn) {
    std::vector<int> moves;
    int total = BOARD_SIZE * BOARD_SIZE;
    for (int i = 0; i < total; ++i) {
        if (board.empty(i) && i != board.ko_idx) {
            BitBoard nb = apply_move(board, i, black_turn);
            if (nb.black != board.black || nb.white != board.white)
                moves.push_back(i);
        }
    }
    return moves;
}

void MCTS_AI::init_onnx_session(const std::string& path) {
    if (session_initialized) return;
    try {
        static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "GoAI");
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(4);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        std::wstring wpath(path.begin(), path.end());
        session = new Ort::Session(env, wpath.c_str(), session_options);
        session_initialized = true;
        model_path = path;
        std::cout << "✅ ONNX 会话初始化成功: " << path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ ONNX 会话初始化失败: " << e.what() << std::endl;
        session_initialized = false;
    }
}

// ---------- ONNX 推理（仅策略，无价值） ----------
bool MCTS_AI::run_onnx_inference(const std::vector<float>& input_tensor,
                                 std::vector<double>& policy,
                                 double& value) {
    if (!session_initialized || session == nullptr) return false;

    try {
        int total = BOARD_SIZE * BOARD_SIZE;
        // 注意：模型实际输入是 (1, 3, 19, 19)，特征编码器已经处理好
        std::vector<int64_t> input_shape = {1, 3, 19, 19};
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value input_ort = Ort::Value::CreateTensor<float>(
            memory_info, const_cast<float*>(input_tensor.data()), input_tensor.size(),
            input_shape.data(), input_shape.size());

        const char* input_names[] = {"input"};
        const char* output_names[] = {"output"};  // 模型只有一个输出（策略 logits）
        std::vector<Ort::Value> outputs = session->Run(Ort::RunOptions{nullptr},
                                                       input_names, &input_ort, 1,
                                                       output_names, 1);

        float* logits = outputs[0].GetTensorMutableData<float>();
        double sum_exp = 0.0;
        std::vector<double> exp_vals(total);
        for (int i = 0; i < total; ++i) {
            exp_vals[i] = std::exp(logits[i]);
            sum_exp += exp_vals[i];
        }
        policy.resize(total);
        for (int i = 0; i < total; ++i) {
            policy[i] = exp_vals[i] / sum_exp;
        }
        value = 0.0; // 模型无价值头，由启发式提供
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ ONNX 推理失败: " << e.what() << std::endl;
        return false;
    }
}

// ---------- 主推理接口（策略来自 NN，价值来自启发式） ----------
MCTS_AI::NetworkOutput MCTS_AI::infer(const BitBoard& board, bool black_turn) {
    NetworkOutput out;
    int total = BOARD_SIZE * BOARD_SIZE;
    out.policy.assign(total, 0.0);
    out.value = 0.0;

    // 先尝试 ONNX 策略
    AIBoard ab = from_bitboard(board);
    Color to_play = black_turn ? BLACK : WHITE;
    std::vector<float> input_tensor = FeatureEncoder::encode(ab, to_play);
    std::vector<double> policy_nn;
    double value_nn = 0.0;
    bool nn_ok = run_onnx_inference(input_tensor, policy_nn, value_nn);

    // 获取启发式结果（价值和备用策略）
    NetworkOutput heur = fallback_heuristic(board, black_turn);

    if (nn_ok) {
        // 使用神经网络策略
        out.policy = policy_nn;
    } else {
        // 降级：使用启发式策略
        out.policy = heur.policy;
    }

    // 关键修复：价值始终使用启发式评估（因为模型没有价值头）
    out.value = heur.value;
    return out;
}

// ---------- 启发式后备（价值评估较强） ----------
MCTS_AI::NetworkOutput MCTS_AI::fallback_heuristic(const BitBoard& board, bool black_turn) {
    NetworkOutput out;
    int total = BOARD_SIZE * BOARD_SIZE;
    out.policy.assign(total, 0.0);
    auto legal = get_legal_moves(board, black_turn);
    if (legal.empty()) {
        out.value = black_turn ? -0.9 : 0.9;
        return out;
    }

    int black_stones = 0, white_stones = 0;
    int black_liberties = 0, white_liberties = 0;
    for (int i = 0; i < total; ++i) {
        if (board.black[i]) {
            black_stones++;
            black_liberties += count_liberties(board, get_group(board, i, true));
        } else if (board.white[i]) {
            white_stones++;
            white_liberties += count_liberties(board, get_group(board, i, false));
        }
    }

    int black_territory = 0, white_territory = 0;
    bool visited_empty[MAX_SIZE][MAX_SIZE] = {false};
    int dr[4] = {-1,1,0,0}, dc[4] = {0,0,-1,1};
    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIZE; ++j) {
            int idx = i * BOARD_SIZE + j;
            if (board.empty(idx) && !visited_empty[i][j]) {
                std::vector<int> region;
                std::vector<int> stack = {idx};
                visited_empty[i][j] = true;
                bool touches_border = false;
                bool has_black = false, has_white = false;
                while (!stack.empty()) {
                    int cur = stack.back(); stack.pop_back();
                    region.push_back(cur);
                    int r = cur / BOARD_SIZE, c = cur % BOARD_SIZE;
                    for (int nb : neighbors(cur)) {
                        if (board.empty(nb) && !visited_empty[nb / BOARD_SIZE][nb % BOARD_SIZE]) {
                            visited_empty[nb / BOARD_SIZE][nb % BOARD_SIZE] = true;
                            stack.push_back(nb);
                        } else if (board.black[nb]) {
                            has_black = true;
                        } else if (board.white[nb]) {
                            has_white = true;
                        }
                    }
                    if (r == 0 || r == BOARD_SIZE-1 || c == 0 || c == BOARD_SIZE-1)
                        touches_border = true;
                }
                if (!touches_border) {
                    if (has_black && !has_white) black_territory += region.size();
                    else if (has_white && !has_black) white_territory += region.size();
                }
            }
        }
    }

    double black_total = black_stones + black_territory + black_liberties * 0.3;
    double white_total = white_stones + white_territory + white_liberties * 0.3;
    double base_value = std::tanh((black_total - white_total) / (BOARD_SIZE * BOARD_SIZE * 0.5));

    std::vector<double> scores;
    for (int move : legal) {
        double score = 0.0;
        BitBoard nb = apply_move(board, move, black_turn);

        int captured = 0;
        const auto& opp = black_turn ? board.white : board.black;
        const auto& new_opp = black_turn ? nb.white : nb.black;
        for (int i = 0; i < total; ++i) if (opp[i] && !new_opp[i]) captured++;
        score += captured * 30.0;

        auto group = get_group(nb, move, black_turn);
        int my_libs = count_liberties(nb, group);
        int old_libs = board.empty(move) ? 0 : count_liberties(board, get_group(board, move, black_turn));
        score += (my_libs - old_libs) * 5.0;
        score += group.size() * 0.8;

        for (int nb_idx : neighbors(move)) {
            if (!nb.empty(nb_idx)) {
                bool is_opponent = (black_turn ? nb.white[nb_idx] : nb.black[nb_idx]);
                if (is_opponent) {
                    auto opp_group = get_group(nb, nb_idx, !black_turn);
                    int opp_libs = count_liberties(nb, opp_group);
                    if (opp_libs == 1) score += 20.0;
                    else if (opp_libs == 2) score += 8.0;
                }
            }
        }

        int empty_neighbors = 0, friendly_neighbors = 0;
        for (int nb_idx : neighbors(move)) {
            if (nb.empty(nb_idx)) empty_neighbors++;
            else {
                bool is_friendly = (black_turn ? nb.black[nb_idx] : nb.white[nb_idx]);
                if (is_friendly) friendly_neighbors++;
            }
        }
        if (empty_neighbors >= 2 && friendly_neighbors >= 2) score += 10.0;
        if (empty_neighbors >= 3 && friendly_neighbors >= 3) score += 15.0;

        int row = move / BOARD_SIZE, col = move % BOARD_SIZE;
        int center = (BOARD_SIZE - 1) / 2;
        double dist = std::abs(row - center) + std::abs(col - center);
        int total_stones = black_stones + white_stones;
        double center_weight = 1.0 - (double)total_stones / (BOARD_SIZE * BOARD_SIZE);
        score += (BOARD_SIZE - 1 - dist) * center_weight * 1.2;
        if (row == 0 || row == BOARD_SIZE-1 || col == 0 || col == BOARD_SIZE-1) score -= 2.0;
        if ((row == 0 || row == BOARD_SIZE-1) && (col == 0 || col == BOARD_SIZE-1)) score -= 3.0;

        int nearby_friends = 0;
        for (int nb_idx : neighbors(move)) {
            if (!nb.empty(nb_idx)) {
                bool is_friendly = (black_turn ? nb.black[nb_idx] : nb.white[nb_idx]);
                if (is_friendly) nearby_friends++;
            }
        }
        score += nearby_friends * 2.0;
        scores.push_back(score);
    }

    double max_s = *std::max_element(scores.begin(), scores.end());
    double sum_exp = 0.0;
    for (double s : scores) sum_exp += std::exp(s - max_s);
    for (size_t i = 0; i < legal.size(); ++i)
        out.policy[legal[i]] = std::exp(scores[i] - max_s) / sum_exp;

    double avg_score = 0.0;
    for (double s : scores) avg_score += s;
    avg_score /= scores.size();
    double scaled_score = std::tanh(avg_score / 50.0);
    double final_value = base_value * 0.6 + scaled_score * 0.4;
    if (!black_turn) final_value = -final_value;
    out.value = final_value;
    return out;
}

// ---------- Searcher ----------
void MCTS_AI::Searcher::reset(const BitBoard& board, bool turn) {
    delete root;
    root = new Node();
    root_board = board;
    black_turn = turn;
}

bool MCTS_AI::Searcher::reuse_subtree(int human_move) {
    if (!root) return false;
    auto it = root->children.find(human_move);
    if (it == root->children.end()) return false;
    Node* child = it->second;
    child->parent = nullptr;
    root->children.erase(human_move);
    delete root;
    root = child;
    root_board = MCTS_AI::apply_move(root_board, human_move, black_turn);
    black_turn = !black_turn;
    return true;
}

void MCTS_AI::Searcher::add_dirichlet_noise(Node* node, double alpha) {
    if (node->children.empty()) return;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::gamma_distribution<> gamma(alpha, 1.0);
    std::vector<double> noise;
    double sum = 0.0;
    for (size_t i = 0; i < node->children.size(); ++i) {
        double n = gamma(gen);
        noise.push_back(n);
        sum += n;
    }
    size_t idx = 0;
    for (auto& kv : node->children) {
        kv.second->P = 0.75 * kv.second->P + 0.25 * (noise[idx] / sum);
        idx++;
    }
}

MCTS_AI::Node* MCTS_AI::Searcher::select(Node* node, double cpuct) {
    if (node->children.empty()) return node;
    Node* best = nullptr;
    double best_score = -1e9;
    double parent_N = (double)node->N + 1e-6;
    for (auto& kv : node->children) {
        Node* child = kv.second;
        double score = child->Q() + child->U(cpuct, parent_N);
        if (score > best_score) { best_score = score; best = child; }
    }
    return best ? select(best, cpuct) : node;
}

double MCTS_AI::Searcher::expand_and_evaluate(Node* leaf) {
    BitBoard board = get_board(leaf);
    bool turn = get_turn(leaf);
    auto legal = MCTS_AI::get_legal_moves(board, turn);
    if (legal.empty()) return turn ? -0.9 : 0.9;
    NetworkOutput out = MCTS_AI::infer(board, turn);
    // 仅扩展策略概率较高的 top-K 动作（提高效率）
    int top_k = (BOARD_SIZE == 9) ? 15 : 20;
    std::vector<std::pair<int, double>> scored;
    for (int move : legal) {
        scored.push_back({move, out.policy[move]});
    }
    std::sort(scored.begin(), scored.end(), [](auto& a, auto& b){ return a.second > b.second; });
    int expanded = 0;
    for (auto& p : scored) {
        if (expanded >= top_k) break;
        int move = p.first;
        Node* child = new Node();
        child->parent = leaf;
        child->move = move;
        child->P = p.second;
        leaf->children[move] = child;
        expanded++;
    }
    return out.value;
}

void MCTS_AI::Searcher::backup(Node* leaf, double value) {
    Node* cur = leaf;
    double v = value;
    while (cur) {
        cur->N += 1;
        cur->W += v;
        v = -v;
        cur = cur->parent;
    }
}

MCTS_AI::BitBoard MCTS_AI::Searcher::get_board(Node* node) {
    std::vector<int> moves;
    Node* cur = node;
    while (cur->parent) { moves.push_back(cur->move); cur = cur->parent; }
    std::reverse(moves.begin(), moves.end());
    BitBoard board = root_board;
    bool turn = black_turn;
    for (int m : moves) {
        board = MCTS_AI::apply_move(board, m, turn);
        turn = !turn;
    }
    return board;
}

bool MCTS_AI::Searcher::get_turn(Node* node) {
    int depth = 0;
    Node* cur = node;
    while (cur->parent) { depth++; cur = cur->parent; }
    return (depth % 2 == 0) ? black_turn : !black_turn;
}

int MCTS_AI::Searcher::search(int time_budget_ms, double cpuct, bool add_noise) {
    if (!root) return -1;
    if (add_noise && root->N == 0) {
        add_dirichlet_noise(root, 0.3);
    }
    auto start = std::chrono::steady_clock::now();
    int iterations = 0;
    int max_iterations = (BOARD_SIZE == 9) ? 800 : 400; // 防止无限循环
    while (iterations < max_iterations) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() >= time_budget_ms)
            break;
        Node* leaf = select(root, cpuct);
        double value = expand_and_evaluate(leaf);
        backup(leaf, value);
        iterations++;
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

// ---------- MCTS_AI 主接口 ----------
MCTS_AI::BitBoard MCTS_AI::to_bitboard(const AIBoard& board) {
    BitBoard bb;
    int total = BOARD_SIZE * BOARD_SIZE;
    for (int i = 0; i < total; ++i) {
        int r = i / BOARD_SIZE, c = i % BOARD_SIZE;
        if (board.grid[r][c] == BLACK) bb.black[i] = true;
        else if (board.grid[r][c] == WHITE) bb.white[i] = true;
    }
    return bb;
}

AIBoard MCTS_AI::from_bitboard(const BitBoard& board) {
    AIBoard ab;
    ab.clear();
    int total = BOARD_SIZE * BOARD_SIZE;
    for (int i = 0; i < total; ++i) {
        int r = i / BOARD_SIZE, c = i % BOARD_SIZE;
        if (board.black[i]) ab.grid[r][c] = BLACK;
        else if (board.white[i]) ab.grid[r][c] = WHITE;
    }
    return ab;
}

void MCTS_AI::apply_human_move(int row, int col, int color) {
    int idx = row * BOARD_SIZE + col;
    bool black_turn = (color == BLACK);
    if (!is_initialized) {
        AIBoard empty;
        empty.clear();
        current_board = to_bitboard(empty);
        current_black_turn = true;
        is_initialized = true;
        searcher.reset(current_board, current_black_turn);
    }
    if (!searcher.reuse_subtree(idx)) {
        current_board = apply_move(current_board, idx, current_black_turn);
        current_black_turn = !current_black_turn;
        searcher.reset(current_board, current_black_turn);
    } else {
        current_board = searcher.get_board();
        current_black_turn = searcher.get_turn();
    }
}

std::pair<int,int> MCTS_AI::getMove(const AIBoard& board, int color) {
    if (!is_initialized) {
        current_board = to_bitboard(board);
        current_black_turn = (color == BLACK);
        searcher.reset(current_board, current_black_turn);
        is_initialized = true;
    } else {
        BitBoard in = to_bitboard(board);
        if (in.black != current_board.black || in.white != current_board.white) {
            current_board = in;
            current_black_turn = (color == BLACK);
            searcher.reset(current_board, current_black_turn);
        }
    }

    int budget = (BOARD_SIZE == 9) ? 1000 : 2000; // 19 路 2 秒，9 路 1 秒
    if (time_limit_ms > 0) budget = time_limit_ms;
    int move_idx = searcher.search(budget, 1.5, true);

    if (move_idx == -1) return {-1, -1};

    // 关键修复：保留搜索树，将根节点移动到子节点（子树重用）
    searcher.reuse_subtree(move_idx);

    // 更新外部状态
    current_board = searcher.get_board();
    current_black_turn = searcher.get_turn();

    int row = move_idx / BOARD_SIZE, col = move_idx % BOARD_SIZE;
    return {row, col};
}