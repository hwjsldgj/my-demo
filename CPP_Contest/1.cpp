#include <iostream>  // 用于输入输出
#include <ctime>     // 用于获取当前时间作为随机种子
#include <cstdlib>   // 用于 rand() 和 srand()
#include <string>    // 用于使用 string 类型
using namespace std; // 使用标准命名空间，避免每次使用标准库函数时都要加上 std:: 前缀

// 这是一个简单的程序，用于随机选择一个游戏名并输出
int main() {

    // 切换控制台编码为 UTF-8，防止中文乱码（Windows 需要）
    system("chcp 65001 > nul");

    // 用当前时间初始化随机种子，保证每次运行结果不同
    srand(static_cast<unsigned>(time(nullptr)));

    // 游戏选项列表
    string options[2] = {"海洋方块","饥荒"};

    // 随机选择并输出一个游戏名
    cout << options[rand() % 2];

    // 程序结束，返回 0 表示成功
    return 0;
}