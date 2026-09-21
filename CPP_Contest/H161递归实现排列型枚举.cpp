/*H161递归实现排列型枚举*/

#include<iostream>
using namespace std;

int n;
int p[15];
bool book[15];

void dfs(int u){
    if(u == n + 1){
        for(int i = 1 ; i <= n ; i++)
            cout << p[i] << " ";
        cout << endl;
    }
    
    for(int i = 1 ; i <= n ; i++){
        if(!book[i]){
            p[u] = i;
            book[i] = true;
            dfs(u + 1);
            book[i] = false;
        }
    }
}
int main(){
    cin >> n;
    dfs(1);
return 0;
}