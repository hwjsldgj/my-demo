// 计算2的n次方，n可以是正数也可以是负数
#include <iostream>
#include <string>
#include <algorithm>
using namespace std;

// 计算2的n次方
string pow2(int n) {
    string s = "1";
    for (int i = 0; i < n; ++i) {
        int carry = 0;
        for (int j = s.size() - 1; j >= 0; --j) {
            int num = (s[j] - '0') * 2 + carry;
            s[j] = num % 10 + '0';
            carry = num / 10;
        }
        if (carry) s = char(carry + '0') + s;
    }
    return s;
}

// 计算1除以2的k次方，即5的k次方
string divideOneByPow2(int k) {
    string pow5 = "1";
    for (int i = 0; i < k; ++i) {
        int carry = 0;
        for (int j = pow5.size() - 1; j >= 0; --j) {
            int num = (pow5[j] - '0') * 5 + carry;
            pow5[j] = num % 10 + '0';
            carry = num / 10;
        }
        if (carry) pow5 = char(carry + '0') + pow5;
    }
    if (pow5.length() < k)
        pow5 = string(k - pow5.length(), '0') + pow5;
    return pow5;
}

// 主函数
int main() {
    int n;
    cin >> n;
    if (n >= 0) {
        cout << pow2(n) << endl;
    } else {
        string ans = divideOneByPow2(-n);
        cout << "0." << ans << endl;
    }
    return 0;
}