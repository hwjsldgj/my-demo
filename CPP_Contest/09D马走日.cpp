/*09D马走日*/

#include <cstring>
#include <iostream>
using namespace std;

int n, m;
int X, Y;
int s = 0;
bool v[5][5];

int dx[8] = {1, 2, 2, 1, -1, -2, -2, -1};
int dy[8] = {2, 1, -1, -2, -2, -1, 1, 2};

void dfs(int x, int y, int step) {
    if (step == n * m) {
        s++;
    return;
    }

    for (int i = 0; i < 8; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (nx >= 0 && nx < n && ny >= 0 && ny < m && !v[nx][ny]) {
            v[nx][ny] = true;
            dfs(nx, ny, step + 1);
            v[nx][ny] = false;
        }
    }
}

int main() {
    int t;
    cin >> t;
    for (int i = 0; i < t; i++) {
        cin >> n >> m >> X >> Y;
        memset(v, false, sizeof(v));
        v[X][Y] = true;
        s = 0;
        dfs(X, Y, 1);
        cout << s << endl;
    }
    return 0;
}