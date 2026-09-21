/*H159 递归实现指数型枚举*/

#include<iostream>
using namespace std;
int n;
int p[100];
bool book[100];

void dfs(int u){
    if(u == n+1){
        for(int i = 1;i <= n;i++)
            if(book[i] == 1) 
                cout << i << " ";
        cout << endl;
        return ;
    }

    book[u] = true;
    dfs(u+1);
    book[u] = false;
    dfs(u+1);
    
}

int main(){
    cin >> n;
    dfs(1);
return 0;
}