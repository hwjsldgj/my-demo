/*09A. 字符串全排列*/

#include<iostream>
#include<string>
using namespace std;

string s;
int n;
int p[15];
bool book[15];

void dfs(int u){
    if(u == n + 1){
        for(int i = 1 ; i <= n ; i++){
            int xb = p[i] - 1;
            cout << s[xb];
        }
        cout << endl;
    }
    for(int i = 1 ; i <= n ; i++)
        if(!book[i]){
            p[u] = i;
            book[i] = true;
            dfs(u + 1);
            book[i] = false;
        }
}

int main(){
    cin >> s;
    n = s.size();
    dfs(1);
return 0;
}