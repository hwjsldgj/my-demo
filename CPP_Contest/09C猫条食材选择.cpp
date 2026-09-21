/*09C猫条食材选择*/

#include <cstdlib>
#include<iostream>
using namespace std;

int n;
int sd[15];
int kd[15];
bool book[15];
int sum_min = 1e9;

void dfs(int u) {
    if(u == n+1) {
        int sd_sum = 1;
        int kd_sum = 0;
        bool yx = false;
        
        for(int i = 1; i <= n; i++) {
            if(book[i]) {
                yx = true;
                sd_sum *= sd[i-1];
                kd_sum += kd[i-1];
            }
        }
        
        if(yx) {
            int sum = abs(sd_sum - kd_sum);
            if(sum < sum_min) {
                sum_min = sum;
            }
        }
        return;
    }

    book[u] = true;
    dfs(u+1);
    book[u] = false;
    dfs(u+1);
}

int main() {
    cin >> n;
    for(int i = 0; i < n; i++) {
        cin >> sd[i] >> kd[i];
    }
    dfs(1);
    cout << sum_min;
    return 0;
}