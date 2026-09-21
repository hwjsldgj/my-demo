/*P1125 [NOIP 2008 提高组] 笨小猴*/

#include <iostream>
#include <string>
using namespace std;

string a;
int z[26];
int Max = 0;
int Min = 500;

bool zs(int n , int m){
    int cha = abs(n - m);
    if(cha <= 1)
        return false;
    for(int i = 2 ; i * i < cha ; i++)
        if(cha % i == 0)
            return false;
    return true;
}

int main(){
    cin >> a;
    int n = a.size();
    for(int i = 0 ; i < n ; i++)
        z[int (char (a[i]) - 'a')]++;

    for(int i = 0 ; i < 26 ; i++){
        if(z[i] > Max)
            Max = z[i];
        if(z[i] < Min && z[i] != 0)
            Min = z[i];
    }

    if(zs(Max , Min)) {
        cout << "Lucky Word" << endl;
        cout << Max -Min;
    }
    else {
        cout << "No Answer" << endl;
        cout << 0;
    }
    return 0;
}