#include <iostream>  // 用于输入输出
#include <windows.h> // 用于设置控制台编码
using namespace std; // 使用标准命名空间，避免每次使用标准库函数时都要加上 std:: 前缀

int main() {
    SetConsoleOutputCP(936); // 设置控制台输出编码为 GBK，以支持中文显示
    SetConsoleCP(936); // 设置控制台输入编码为 GBK，以支持中文输入
    double d,n;
    while (1) {
        cout << "请输入 距离 和 装弹量：";
        cin >> d >> n;
        if(n > 6) {
            cout << "装弹量 不能大于 6" << endl;
            continue;
        }
        if(d > n*5) {
            cout << "距离 过大" << endl;
            continue;
        }
        if(d<=0 || n<=0) {
            cout << "输入不能为 负数 或 零" << endl;
            continue;
        }
        cout << (d*12)/n << endl;
    }
    
    return 0;
}
