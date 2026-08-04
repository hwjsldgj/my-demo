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

using namespace std;
using namespace chrono;

// ---------- 全局配置 ----------
int SIZE;           // 9 或 19，运行时设置
float KOMI;         // 9路 0.0，19路 3.75
const int MAX_SIZE = 19;
enum Color { EMPTY, BLACK, WHITE };
Color board[MAX_SIZE][MAX_SIZE];

// ---------- 基础棋盘操作 ----------
bool inBounds(int r, int c) {
    return r >= 0 && r < SIZE && c >= 0 && c < SIZE;
}
vector<pair<int,int>> getNeighbors(int r, int c) {
    vector<pair<int,int>> neigh;
    int dr[] = {-1,1,0,0}, dc[] = {0,0,-1,1};
    for(int i=0;i<4;i++){ int nr=r+dr[i], nc=c+dc[i]; if(inBounds(nr,nc)) neigh.push_back({nr,nc}); }
    return neigh;
}
vector<pair<int,int>> getGroup(int r, int c, Color color, const Color board[MAX_SIZE][MAX_SIZE]) {
    vector<pair<int,int>> group;
    bool visited[MAX_SIZE][MAX_SIZE] = {false};
    vector<pair<int,int>> stack = {{r,c}};
    visited[r][c] = true;
    while(!stack.empty()){
        auto p = stack.back(); stack.pop_back();
        group.push_back(p);
        for(auto neigh : getNeighbors(p.first, p.second)){
            int nr=neigh.first, nc=neigh.second;
            if(!visited[nr][nc] && board[nr][nc]==color){
                visited[nr][nc]=true;
                stack.push_back({nr,nc});
            }
        }
    }
    return group;
}
int countLiberties(const vector<pair<int,int>>& group, const Color board[MAX_SIZE][MAX_SIZE]) {
    int lib=0;
    bool counted[MAX_SIZE][MAX_SIZE] = {false};
    for(auto p : group){
        for(auto neigh : getNeighbors(p.first, p.second)){
            int nr=neigh.first, nc=neigh.second;
            if(!counted[nr][nc] && board[nr][nc]==EMPTY){ counted[nr][nc]=true; lib++; }
        }
    }
    return lib;
}
void removeDeadStones(Color board[MAX_SIZE][MAX_SIZE], Color color) {
    bool removed[MAX_SIZE][MAX_SIZE]={false};
    for(int r=0;r<SIZE;r++) for(int c=0;c<SIZE;c++){
        if(board[r][c]==color && !removed[r][c]){
            auto group = getGroup(r,c,color,board);
            if(countLiberties(group,board)==0){
                for(auto p : group){ board[p.first][p.second]=EMPTY; removed[p.first][p.second]=true; }
            } else {
                for(auto p : group) removed[p.first][p.second]=true;
            }
        }
    }
}
bool tryMove(int r, int c, Color color, Color board[MAX_SIZE][MAX_SIZE]) {
    if(!inBounds(r,c) || board[r][c]!=EMPTY) return false;
    Color temp[MAX_SIZE][MAX_SIZE];
    for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) temp[i][j]=board[i][j];
    temp[r][c]=color;
    Color opp = (color==BLACK)?WHITE:BLACK;
    removeDeadStones(temp, opp);
    auto group = getGroup(r,c,color,temp);
    if(countLiberties(group,temp)==0) return false;
    for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) board[i][j]=temp[i][j];
    return true;
}

// ---------- 星位（按棋盘大小） ----------
bool isStarPoint(int r, int c) {
    if(SIZE == 9) return (r==4 && c==4);
    else { // 19路
        int stars[9][2] = {{3,3},{3,15},{15,3},{15,15},{3,9},{15,9},{9,3},{9,15},{9,9}};
        for(int i=0;i<9;i++) if(r==stars[i][0] && c==stars[i][1]) return true;
        return false;
    }
}

void printBoard(const Color board[MAX_SIZE][MAX_SIZE]) {
    cout << "   ";
    for(int c=1;c<=SIZE;c++){
        int idx = c-1;
        if(SIZE==19 && idx>=8) idx++; // 跳过 I
        char letter = 'A' + idx;
        cout << letter << " ";
    }
    cout << endl;
    for(int r=0;r<SIZE;r++){
        if(r+1<10) cout << " " << r+1 << " ";
        else cout << r+1 << " ";
        for(int c=0;c<SIZE;c++){
            if(board[r][c]==EMPTY) cout << (isStarPoint(r,c) ? "+ " : ". ");
            else if(board[r][c]==BLACK) cout << "X ";
            else cout << "O ";
        }
        cout << endl;
    }
}

struct ScoreDetail { int blackStones, blackTerritory, whiteStones, whiteTerritory; };
ScoreDetail calculateScore(const Color board[MAX_SIZE][MAX_SIZE]) {
    bool visited[MAX_SIZE][MAX_SIZE]={false};
    int blackStones=0, blackTerritory=0, whiteStones=0, whiteTerritory=0;
    for(int r=0;r<SIZE;r++) for(int c=0;c<SIZE;c++){ if(board[r][c]==BLACK) blackStones++; else if(board[r][c]==WHITE) whiteStones++; }
    for(int r=0;r<SIZE;r++) for(int c=0;c<SIZE;c++){
        if(board[r][c]==EMPTY && !visited[r][c]){
            vector<pair<int,int>> region, stack={{r,c}};
            visited[r][c]=true;
            bool hasBlack=false, hasWhite=false;
            while(!stack.empty()){
                auto p=stack.back(); stack.pop_back();
                region.push_back(p);
                for(auto neigh : getNeighbors(p.first,p.second)){
                    int nr=neigh.first, nc=neigh.second;
                    if(board[nr][nc]==EMPTY && !visited[nr][nc]){ visited[nr][nc]=true; stack.push_back({nr,nc}); }
                    else if(board[nr][nc]==BLACK) hasBlack=true;
                    else if(board[nr][nc]==WHITE) hasWhite=true;
                }
            }
            if(hasBlack && !hasWhite) blackTerritory += region.size();
            else if(!hasBlack && hasWhite) whiteTerritory += region.size();
        }
    }
    return {blackStones, blackTerritory, whiteStones, whiteTerritory};
}

bool parseCoordinate(const string& input, int& row, int& col) {
    string s=input;
    replace(s.begin(),s.end(),'.',' ');
    replace(s.begin(),s.end(),',',' ');
    stringstream ss(s);
    int r,c;
    if(ss>>r>>c){ string rem; ss>>rem; if(rem.empty()){ row=r-1; col=c-1; if(inBounds(row,col)) return true; } }
    string letters, digits;
    for(char ch:input){ if(isalpha(ch)) letters+=tolower(ch); else if(isdigit(ch)) digits+=ch; }
    if(letters.length()!=1 || digits.empty()) return false;
    char letter=letters[0];
    int colIndex;
    if(SIZE==9){
        if(letter<'a' || letter>'i') return false;
        colIndex = letter - 'a';
    } else { // 19路，跳过 'i'
        if(letter == 'i') return false;
        if(letter < 'i') colIndex = letter - 'a';
        else colIndex = letter - 'a' - 1;
    }
    int rowIndex = stoi(digits) - 1;
    if(rowIndex<0 || rowIndex>=SIZE || colIndex<0 || colIndex>=SIZE) return false;
    row=rowIndex; col=colIndex;
    return true;
}

// ========== AI 模块（通用） ==========
class AIBoard {
public:
    int grid[MAX_SIZE][MAX_SIZE];
    AIBoard(){ clear(); }
    void clear(){ for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) grid[i][j]=EMPTY; }
    void fromGlobal(const Color gb[MAX_SIZE][MAX_SIZE]){ for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) grid[i][j]=(int)gb[i][j]; }
    bool inBoard(int r,int c) const { return r>=0 && r<SIZE && c>=0 && c<SIZE; }
    int getLiberties(int r,int c) const {
        int libs=0; int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
        for(int k=0;k<4;k++){ int nr=r+dr[k], nc=c+dc[k]; if(inBoard(nr,nc) && grid[nr][nc]==EMPTY) libs++; }
        return libs;
    }
    int groupLiberties(int r,int c,bool visited[MAX_SIZE][MAX_SIZE]) const {
        if(!inBoard(r,c) || visited[r][c] || grid[r][c]==EMPTY) return 0;
        int color=grid[r][c]; visited[r][c]=true;
        int libs=getLiberties(r,c);
        int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
        for(int k=0;k<4;k++){ int nr=r+dr[k], nc=c+dc[k]; if(inBoard(nr,nc) && !visited[nr][nc] && grid[nr][nc]==color) libs += groupLiberties(nr,nc,visited); }
        return libs;
    }
    bool isValidMove(int r,int c,int color) const {
        if(!inBoard(r,c) || grid[r][c]!=EMPTY) return false;
        AIBoard temp=*this; temp.grid[r][c]=color;
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
    void removeGroup(int r,int c){
        if(!inBoard(r,c) || grid[r][c]==EMPTY) return;
        int color=grid[r][c]; grid[r][c]=EMPTY;
        int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
        for(int k=0;k<4;k++){ int nr=r+dr[k], nc=c+dc[k]; if(inBoard(nr,nc) && grid[nr][nc]==color) removeGroup(nr,nc); }
    }
    bool placeStone(int r,int c,int color){
        if(!isValidMove(r,c,color)) return false;
        AIBoard temp=*this; temp.grid[r][c]=color;
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
        *this=temp; return true;
    }
    vector<pair<int,int>> getLegalMoves(int color) const {
        vector<pair<int,int>> moves;
        for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) if(grid[i][j]==EMPTY && isValidMove(i,j,color)) moves.push_back({i,j});
        return moves;
    }
    int countStones(int color) const {
        int cnt=0; for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) if(grid[i][j]==color) cnt++;
        return cnt;
    }
    uint64_t hash() const {
        uint64_t h=0; for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) h = h*3 + grid[i][j];
        return h;
    }
};

// ---------- 难度0：极简（规则） ----------
class RuleBasedAI {
public:
    pair<int,int> getMove(const AIBoard& board, int color) {
        auto moves = board.getLegalMoves(color);
        if(moves.empty()) return {-1,-1};
        // 提子
        for(auto [r,c] : moves){
            AIBoard temp=board; temp.placeStone(r,c,color);
            int opp=(color==BLACK)?WHITE:BLACK;
            bool captured=false;
            int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
            for(int k=0;k<4;k++){ int nr=r+dr[k], nc=c+dc[k]; if(temp.inBoard(nr,nc) && temp.grid[nr][nc]==opp){ bool vis[MAX_SIZE][MAX_SIZE]={false}; if(temp.groupLiberties(nr,nc,vis)==0){ captured=true; break; } } }
            if(captured) return {r,c};
        }
        // 逃命（单子一口气）
        for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++){
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
};

// ---------- 难度1：简单（贪心） ----------
class GreedyAI {
public:
    double evaluateMove(const AIBoard& board, int r, int c, int color) {
        double score=0.0;
        AIBoard temp=board; temp.placeStone(r,c,color);
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
        // 中心偏好（动态中心）
        int center = (SIZE-1)/2;
        double centerDist = abs(r-center)+abs(c-center);
        score += (SIZE - 1 - centerDist) * 0.8;
        if(r==0 || r==SIZE-1 || c==0 || c==SIZE-1) score -= 1.0;
        return score;
    }

    pair<int,int> getMove(const AIBoard& board, int color) {
        auto moves = board.getLegalMoves(color);
        if(moves.empty()) return {-1,-1};
        if(board.countStones(BLACK)==0 && board.countStones(WHITE)==0) return {SIZE/2, SIZE/2};
        double bestScore=-1e9;
        vector<pair<int,int>> bestMoves;
        for(auto [r,c] : moves){
            double sc = evaluateMove(board,r,c,color);
            if(sc > bestScore){ bestScore=sc; bestMoves.clear(); bestMoves.push_back({r,c}); }
            else if(fabs(sc-bestScore)<1e-6) bestMoves.push_back({r,c});
        }
        int idx=rand()%bestMoves.size();
        return bestMoves[idx];
    }
};

// ---------- 难度2：中等（2层搜索） ----------
class Search2AI {
public:
    double evaluate(const AIBoard& board, int color) {
        Color temp[MAX_SIZE][MAX_SIZE];
        for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) temp[i][j]=(Color)board.grid[i][j];
        ScoreDetail sd = calculateScore(temp);
        int blackTotal = sd.blackStones + sd.blackTerritory;
        int whiteTotal = sd.whiteStones + sd.whiteTerritory;
        if(color==BLACK) return blackTotal - whiteTotal;
        else return whiteTotal - blackTotal;
    }

    pair<int,int> getMove(const AIBoard& board, int color) {
        auto moves = board.getLegalMoves(color);
        if(moves.empty()) return {-1,-1};
        if(board.countStones(BLACK)==0 && board.countStones(WHITE)==0) return {SIZE/2, SIZE/2};

        int opp = (color==BLACK)?WHITE:BLACK;
        double bestScore = -1e9;
        vector<pair<int,int>> bestMoves;

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
};

// ---------- 难度3：困难（深度自适应：9路4层，19路3层） ----------
class Search4AI {
public:
    int searchDepth; // 由构造函数设置

    Search4AI(int depth) : searchDepth(depth) {}

    double heuristicScore(const AIBoard& board, int r, int c, int color) {
        double score=0.0;
        AIBoard temp=board; temp.placeStone(r,c,color);
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
        int center=(SIZE-1)/2;
        double centerDist=abs(r-center)+abs(c-center);
        score += (SIZE-1-centerDist)*1.0;
        if(r==0 || r==SIZE-1 || c==0 || c==SIZE-1) score-=2.0;
        return score;
    }

    double evaluate(const AIBoard& board, int color) {
        Color temp[MAX_SIZE][MAX_SIZE];
        for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) temp[i][j]=(Color)board.grid[i][j];
        ScoreDetail sd = calculateScore(temp);
        int blackTotal = sd.blackStones + sd.blackTerritory;
        int whiteTotal = sd.whiteStones + sd.whiteTerritory;
        if(color==BLACK) return blackTotal - whiteTotal;
        else return whiteTotal - blackTotal;
    }

    vector<pair<int,int>> getTopMoves(const AIBoard& board, int color, int topK) {
        auto moves = board.getLegalMoves(color);
        if(moves.empty()) return {};
        vector<pair<double, pair<int,int>>> scored;
        for(auto m : moves){
            double sc = heuristicScore(board, m.first, m.second, color);
            scored.push_back({sc, m});
        }
        sort(scored.begin(), scored.end(), [](auto& a, auto& b){ return a.first > b.first; });
        vector<pair<int,int>> result;
        for(int i=0; i<min(topK, (int)scored.size()); ++i) result.push_back(scored[i].second);
        return result;
    }

    double dfs(AIBoard board, int color, int depth, int maxDepth, double alpha, double beta) {
        if(depth == maxDepth) return evaluate(board, color);
        int opp = (color==BLACK)?WHITE:BLACK;
        auto moves = getTopMoves(board, color, 8);
        if(moves.empty()){
            return dfs(board, opp, depth+1, maxDepth, alpha, beta);
        }
        if(color == BLACK){
            double best = -1e9;
            for(auto m : moves){
                AIBoard nb = board;
                nb.placeStone(m.first, m.second, color);
                double val = dfs(nb, opp, depth+1, maxDepth, alpha, beta);
                best = max(best, val);
                alpha = max(alpha, val);
                if(beta <= alpha) break;
            }
            return best;
        } else {
            double best = 1e9;
            for(auto m : moves){
                AIBoard nb = board;
                nb.placeStone(m.first, m.second, color);
                double val = dfs(nb, opp, depth+1, maxDepth, alpha, beta);
                best = min(best, val);
                beta = min(beta, val);
                if(beta <= alpha) break;
            }
            return best;
        }
    }

    pair<int,int> getMove(const AIBoard& board, int color) {
        if(board.countStones(BLACK)==0 && board.countStones(WHITE)==0) return {SIZE/2, SIZE/2};
        auto topMoves = getTopMoves(board, color, 8);
        if(topMoves.empty()) return {-1,-1};
        int opp = (color==BLACK)?WHITE:BLACK;
        double bestScore = -1e9;
        vector<pair<int,int>> bestMoves;
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
};

// ========== 主程序 ==========
int main() {
    srand(time(nullptr));

    cout << "请选择棋盘大小：\n";
    cout << "1 - 9x9（无贴子）\n";
    cout << "2 - 19x19（黑贴 3.75 子）\n";
    cout << "请输入 1 或 2：";
    int boardOpt; cin >> boardOpt; cin.ignore();
    if(boardOpt == 1){ SIZE=9; KOMI=0.0f; }
    else { SIZE=19; KOMI=3.75f; }

    cout << "\n欢迎来到 " << SIZE << "x" << SIZE << " 围棋 AI 对弈！\n";
    cout << "请选择 AI 难度：\n";
    cout << "0 - 极简（规则）\n";
    cout << "1 - 简单（贪心）\n";
    cout << "2 - 中等（2层搜索）\n";
    cout << "3 - 困难（" << (SIZE==9?"4":"3") << "层搜索+剪枝）\n";
    cout << "请输入 0、1、2 或 3：";
    int difficulty; cin >> difficulty; cin.ignore();

    for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) board[i][j]=EMPTY;

    Color humanColor=BLACK, aiColor=WHITE, current=BLACK;
    int passCount=0, moveCount=0;
    bool gameOver=false, aiResigned=false;

    struct State { Color board[MAX_SIZE][MAX_SIZE]; Color turn; };
    vector<State> history;
    State init; for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) init.board[i][j]=EMPTY;
    init.turn=BLACK; history.push_back(init);

    cout << "\n您执黑 (X)，AI 执白 (O)。\n";
    cout << "列号字母：";
    if(SIZE==9) cout << "A-I";
    else cout << "A-T (跳过 I)";
    cout << "，行号为数字。\n";
    cout << "输入坐标如 'A1'、'1A'、'3 4' 等。\n";
    cout << "输入 'pass' 弃权，'q' 退出。\n\n";

    unique_ptr<RuleBasedAI> ruleAI;
    unique_ptr<GreedyAI> greedyAI;
    unique_ptr<Search2AI> search2;
    unique_ptr<Search4AI> search4;

    if(difficulty==0){ ruleAI=make_unique<RuleBasedAI>(); cout<<"难度：极简\n"; }
    else if(difficulty==1){ greedyAI=make_unique<GreedyAI>(); cout<<"难度：简单\n"; }
    else if(difficulty==2){ search2=make_unique<Search2AI>(); cout<<"难度：中等\n"; }
    else { int depth = (SIZE==9)?4:3; search4=make_unique<Search4AI>(depth); cout<<"难度：困难（"<<depth<<"层）\n"; }

    while(!gameOver){
        cout << "\n";
        printBoard(board);
        cout << (current==BLACK ? "黑方 (您)" : "白方 (AI)") << " 落子：";

        if(current==humanColor){
            string input; getline(cin,input);
            input.erase(0, input.find_first_not_of(" \t\n\r"));
            input.erase(input.find_last_not_of(" \t\n\r")+1);
            if(input=="q" || input=="Q" || input=="quit" || input=="exit"){ cout<<"游戏已终止。\n"; break; }
            if(input=="pass"){ passCount++; if(passCount>=2){ gameOver=true; break; } current=(current==BLACK)?WHITE:BLACK; continue; }
            else passCount=0;

            int row,col;
            if(!parseCoordinate(input,row,col)){ cout<<"输入格式错误。\n"; continue; }
            Color tempBoard[MAX_SIZE][MAX_SIZE];
            for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) tempBoard[i][j]=board[i][j];
            if(!tryMove(row,col,current,tempBoard)){ cout<<"非法落子。\n"; continue; }
            Color nextTurn=(current==BLACK)?WHITE:BLACK;
            bool duplicate=false;
            for(const auto& st : history){
                if(st.turn!=nextTurn) continue;
                bool same=true;
                for(int i=0;i<SIZE && same;i++) for(int j=0;j<SIZE && same;j++) if(st.board[i][j]!=tempBoard[i][j]){ same=false; break; }
                if(same){ duplicate=true; break; }
            }
            if(duplicate){ cout<<"劫争，禁止。\n"; continue; }
            for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) board[i][j]=tempBoard[i][j];
            State newState; for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) newState.board[i][j]=board[i][j];
            newState.turn=nextTurn; history.push_back(newState);
            moveCount++; current=nextTurn;
        } else {
            // 认输判断（所有难度，阈值不同）
            if(moveCount > 20){
                ScoreDetail score=calculateScore(board);
                int blackTotal=score.blackStones+score.blackTerritory;
                int whiteTotal=score.whiteStones+score.whiteTerritory;
                double threshold;
                if(difficulty==0) threshold = 0.10;
                else if(difficulty==1) threshold = 0.20;
                else if(difficulty==2) threshold = 0.25;
                else threshold = 0.35;
                if(whiteTotal < blackTotal * threshold){
                    cout << "AI 认输。\n";
                    aiResigned = true;
                    gameOver = true;
                    break;
                }
            }

            cout << "思考中...\n";
            AIBoard aiBoard; aiBoard.fromGlobal(board);
            pair<int,int> move;

            if(difficulty==0){
                move = ruleAI->getMove(aiBoard, aiColor);
            } else if(difficulty==1){
                move = greedyAI->getMove(aiBoard, aiColor);
            } else if(difficulty==2){
                move = search2->getMove(aiBoard, aiColor);
            } else {
                move = search4->getMove(aiBoard, aiColor);
            }

            if(move.first==-1){ cout<<"AI 无合法走法，自动弃权。\n"; passCount++; if(passCount>=2){ gameOver=true; break; } current=(current==BLACK)?WHITE:BLACK; continue; }

            Color tempBoard[MAX_SIZE][MAX_SIZE];
            for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) tempBoard[i][j]=board[i][j];
            if(!tryMove(move.first,move.second,aiColor,tempBoard)){ cout<<"AI 走法意外非法，跳过。\n"; passCount++; if(passCount>=2){ gameOver=true; break; } current=(current==BLACK)?WHITE:BLACK; continue; }
            Color nextTurn=(current==BLACK)?WHITE:BLACK;
            bool duplicate=false;
            for(const auto& st : history){
                if(st.turn!=nextTurn) continue;
                bool same=true;
                for(int i=0;i<SIZE && same;i++) for(int j=0;j<SIZE && same;j++) if(st.board[i][j]!=tempBoard[i][j]){ same=false; break; }
                if(same){ duplicate=true; break; }
            }
            if(duplicate){ cout<<"AI 走法导致劫争，自动弃权。\n"; passCount++; if(passCount>=2){ gameOver=true; break; } current=(current==BLACK)?WHITE:BLACK; continue; }
            for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) board[i][j]=tempBoard[i][j];
            // 列字母输出（考虑19路跳过I）
            int colLetter = move.second;
            if(SIZE==19 && colLetter>=8) colLetter++; // 跳过I
            cout << "AI 落子 " << (char)('A'+colLetter) << (move.first+1) << endl;
            State newState; for(int i=0;i<SIZE;i++) for(int j=0;j<SIZE;j++) newState.board[i][j]=board[i][j];
            newState.turn=nextTurn; history.push_back(newState);
            passCount=0; moveCount++; current=nextTurn;
        }
    }

    cout << "\n\n\n";
    printBoard(board);
    ScoreDetail detail = calculateScore(board);
    cout << "\n========== 终局数子 ==========\n";
    cout << "黑方棋子数 : " << detail.blackStones << " 子\n";
    cout << "黑方领地数 : " << detail.blackTerritory << " 目（空点）\n";
    cout << "黑方总子数 : " << (detail.blackStones+detail.blackTerritory) << " 子";
    if(KOMI>0) cout << "，贴 " << KOMI << " 子";
    cout << "\n";
    float blackFinal = detail.blackStones + detail.blackTerritory - KOMI;
    cout << "黑方贴子后 : " << blackFinal << " 子\n\n";

    cout << "白方棋子数 : " << detail.whiteStones << " 子\n";
    cout << "白方领地数 : " << detail.whiteTerritory << " 目（空点）\n";
    cout << "白方总子数 : " << (detail.whiteStones+detail.whiteTerritory) << " 子\n\n";

    if(aiResigned) cout << "AI 认输，黑方（您）获胜！\n";
    else if(blackFinal > detail.whiteStones+detail.whiteTerritory) cout << "结果：黑方胜！\n";
    else if(blackFinal < detail.whiteStones+detail.whiteTerritory) cout << "结果：白方胜！\n";
    else cout << "结果：平局！\n";
    cout << "==============================\n";
    return 0;
}