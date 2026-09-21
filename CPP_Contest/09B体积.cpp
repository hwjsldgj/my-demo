/*09B体积*/

#include<iostream>
using namespace std;

int n;
int Max;
int a[55];
bool book[55];
int s[1001];

void dfs(int u){
    if(u == n+1){
        int sum = 0;
        for(int i = 1 ; i <= n ; i++)
            if(book[i] == 1)
                sum += a[i-1];
        if(sum > Max)
            Max = sum;
        s[sum]++;
        return ;
    }

    book[u] = true;
    dfs(u+1);
    book[u] = false;
    dfs(u+1);
    
}
int main(){
    cin >> n;
    for (int i = 0 ; i < n ; i++)
        cin >> a[i];
    Max = 0; 
    dfs(1);
    int Sum = 0;
    for(int i = 1 ; i <= Max ; i++)
        if(s[i] >= 1)
            Sum++;
    cout << Sum;
    return 0;
}