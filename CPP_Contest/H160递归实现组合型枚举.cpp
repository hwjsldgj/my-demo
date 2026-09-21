/*H160递归实现组合型枚举*/

#include<iostream>
using namespace std;
int n,m;
int p[105];
bool book[105];
void dfs(int u){
    if(u == m + 1){
        for(int i = 1 ; i <= m ; i++)
            cout << p[i] << " ";
        cout << endl;
        return ;
    }
    for(int i = p[u - 1] + 1 ; i <= n ; i++){
        if(!book[i]){
            p[u] = i;
            book[i] = true;
            dfs(u + 1);
            book[i] = false;
        }
    }
}
int main(){
    cin >> n >> m;
    dfs(1);
return 0;
}