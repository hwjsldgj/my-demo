#include <iostream>  // 用于输入输出
#include <string>    // 用于字符串处理

using namespace std;  // 使用标准命名空间

int main() {
    string s;
    getline(cin, s); // 从标准输入读取一行字符串
    // 遍历字符串中的每个字符，根据条件进行转换
    for (char &c : s) {
        // 如果字符是小写字母a-w或大写字母A-W，则将其转换为对应的字符（加3）
        if(c >= 'a' && c <= 'w' || c >= 'A' && c <= 'W') {
            c = c + 3;
        } 
        // 如果字符是小写字母x-z或大写字母X-Z，则将其转换为对应的字符（减23）
        else if(c >= 'x' && c <= 'z' || c >= 'X' && c <= 'Z') {
            c = c - 23;
        }
    }

    // 输出转换后的字符串
    cout << s <<endl;
    
    // 返回0表示程序成功执行
    return 0;
}
