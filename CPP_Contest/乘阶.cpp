#include <iostream>
#include <vector>
using namespace std;

int main() {
    // 100! 最多有 158 位，数组大小 200 位足够容纳
    vector<int> res(200, 0);
    res[0] = 1; // 个位初始化为 1
    int len = 1; // 当前结果的有效位数

    int n;
    // 1. 读取输入的 n
    if (!(cin >> n)) return 0;

    // 2. 核心计算逻辑：从 2 乘到 n
    for (int i = 2; i <= n; ++i) {
        int carry = 0; // 进位
        
        // 逐位进行乘法运算
        for (int j = 0; j < len; ++j) {
            long long product = (long long)res[j] * i + carry; // 使用 long long 防止溢出
            res[j] = product % 10;
            carry = product / 10;
        }
        
        // 处理高位剩余的进位
        while (carry > 0) {
            res[len++] = carry % 10;
            carry /= 10;
        }
    }

    // 3. 逆序输出结果（从最高位到最低位）
    for (int i = len - 1; i >= 0; --i) {
        cout << res[i];
    }
    // 确保结果独占一行
    cout << endl;

    return 0;
}