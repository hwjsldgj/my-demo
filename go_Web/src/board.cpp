#include "go_common.h"

int BOARD_SIZE;
float KOMI;

bool inBounds(int r, int c) {
    return r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE;
}

std::vector<std::pair<int,int>> getNeighbors(int r, int c) {
    std::vector<std::pair<int,int>> neigh;
    int dr[] = {-1,1,0,0}, dc[] = {0,0,-1,1};
    for(int i=0;i<4;i++){ int nr=r+dr[i], nc=c+dc[i]; if(inBounds(nr,nc)) neigh.push_back({nr,nc}); }
    return neigh;
}

std::vector<std::pair<int,int>> getGroup(int r, int c, Color color, const Color board[MAX_SIZE][MAX_SIZE]) {
    std::vector<std::pair<int,int>> group;
    bool visited[MAX_SIZE][MAX_SIZE] = {false};
    std::vector<std::pair<int,int>> stack = {{r,c}};
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

int countLiberties(const std::vector<std::pair<int,int>>& group, const Color board[MAX_SIZE][MAX_SIZE]) {
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
    for(int r=0;r<BOARD_SIZE;r++) for(int c=0;c<BOARD_SIZE;c++){
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
    for(int i=0;i<BOARD_SIZE;i++) for(int j=0;j<BOARD_SIZE;j++) temp[i][j]=board[i][j];
    temp[r][c]=color;
    Color opp = (color==BLACK)?WHITE:BLACK;
    removeDeadStones(temp, opp);
    auto group = getGroup(r,c,color,temp);
    if(countLiberties(group,temp)==0) return false;
    for(int i=0;i<BOARD_SIZE;i++) for(int j=0;j<BOARD_SIZE;j++) board[i][j]=temp[i][j];
    return true;
}

bool isStarPoint(int r, int c) {
    if(BOARD_SIZE == 9) return (r==4 && c==4);
    else {
        int stars[9][2] = {{3,3},{3,15},{15,3},{15,15},{3,9},{15,9},{9,3},{9,15},{9,9}};
        for(int i=0;i<9;i++) if(r==stars[i][0] && c==stars[i][1]) return true;
        return false;
    }
}

void printBoard(const Color board[MAX_SIZE][MAX_SIZE]) {
    std::cout << "   ";
    for(int c=1;c<=BOARD_SIZE;c++){
        int idx = c-1;
        if(BOARD_SIZE==19 && idx>=8) idx++;
        char letter = 'A' + idx;
        std::cout << letter << " ";
    }
    std::cout << std::endl;
    for(int r=0;r<BOARD_SIZE;r++){
        if(r+1<10) std::cout << " " << r+1 << " ";
        else std::cout << r+1 << " ";
        for(int c=0;c<BOARD_SIZE;c++){
            if(board[r][c]==EMPTY) std::cout << (isStarPoint(r,c) ? "+ " : ". ");
            else if(board[r][c]==BLACK) std::cout << "X ";
            else std::cout << "O ";
        }
        std::cout << std::endl;
    }
}

ScoreDetail calculateScore(const Color board[MAX_SIZE][MAX_SIZE]) {
    bool visited[MAX_SIZE][MAX_SIZE] = {false};
    int blackStones = 0, whiteStones = 0;
    int blackTerritory = 0, whiteTerritory = 0;

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board[r][c] == BLACK) blackStones++;
            else if (board[r][c] == WHITE) whiteStones++;
        }
    }

    int dr[4] = {-1, 1, 0, 0};
    int dc[4] = {0, 0, -1, 1};

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board[r][c] == EMPTY && !visited[r][c]) {
                std::vector<std::pair<int,int>> region;
                std::vector<std::pair<int,int>> stack = {{r, c}};
                visited[r][c] = true;
                bool touchesBorder = false;
                bool hasBlack = false, hasWhite = false;

                while (!stack.empty()) {
                    auto p = stack.back();
                    stack.pop_back();
                    region.push_back(p);

                    for (int k = 0; k < 4; k++) {
                        int nr = p.first + dr[k];
                        int nc = p.second + dc[k];
                        if (!inBounds(nr, nc)) {
                            touchesBorder = true;
                            continue;
                        }
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

                if (!touchesBorder && hasBlack && !hasWhite) {
                    blackTerritory += region.size();
                } else if (!touchesBorder && hasWhite && !hasBlack) {
                    whiteTerritory += region.size();
                }
            }
        }
    }

    return {blackStones, blackTerritory, whiteStones, whiteTerritory};
}

bool parseCoordinate(const std::string& input, int& row, int& col) {
    std::string s=input;
    std::replace(s.begin(),s.end(),'.',' ');
    std::replace(s.begin(),s.end(),',',' ');
    std::stringstream ss(s);
    int r,c;
    if(ss>>r>>c){ std::string rem; ss>>rem; if(rem.empty()){ row=r-1; col=c-1; if(inBounds(row,col)) return true; } }
    std::string letters, digits;
    for(char ch:input){ if(isalpha(ch)) letters+=tolower(ch); else if(isdigit(ch)) digits+=ch; }
    if(letters.length()!=1 || digits.empty()) return false;
    char letter=letters[0];
    int colIndex;
    if(BOARD_SIZE==9){
        if(letter<'a' || letter>'i') return false;
        colIndex = letter - 'a';
    } else {
        if(letter == 'i') return false;
        if(letter < 'i') colIndex = letter - 'a';
        else colIndex = letter - 'a' - 1;
    }
    int rowIndex = std::stoi(digits) - 1;
    if(rowIndex<0 || rowIndex>=BOARD_SIZE || colIndex<0 || colIndex>=BOARD_SIZE) return false;
    row=rowIndex; col=colIndex;
    return true;
}