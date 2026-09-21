#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <numeric>
#include <limits>
#include <ctime>
#include <chrono>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

// 初始化控制台以支持 UTF-8 中文
static void initConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

// 等待按键（回车）退出
static void waitExit() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cout << "\n按任意键（回车）退出...";
    std::cin.get();
}

// 判断是否在下午 6:00 前后 20 分钟内（17:40 ~ 18:20）
static bool isCheatTime() {
    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    if (!now) return false;
    int minutes = now->tm_hour * 60 + now->tm_min;
    const int start = 17 * 60 + 40; // 17:40
    const int end   = 18 * 60 + 20; // 18:20
    return minutes >= start && minutes <= end;
}

// 去掉行尾 \r 和行首 UTF-8 BOM（只对第一行做 BOM 处理）
static void cleanLine(std::string& line, bool firstLine) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (firstLine && line.size() >= 3 &&
        (unsigned char)line[0] == 0xEF &&
        (unsigned char)line[1] == 0xBB &&
        (unsigned char)line[2] == 0xBF) {
        line.erase(0, 3);
    }
}

// 从控制台读入一个正整数，非法输入时提示并重试
static int readPositiveInt(const std::string& prompt) {
    int value = 0;
    while (true) {
        std::cout << prompt;
        if (std::cin >> value && value > 0) {
            return value;
        }
        std::cout << "数字不合法，请重新输入。\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

// 从控制台读入一个不超过 upper 的正整数，非法或超过 upper 时提示并重试
static int readBoundedInt(const std::string& prompt, int upper) {
    int value = 0;
    while (true) {
        std::cout << prompt;
        if (std::cin >> value) {
            if (value <= 0) {
                std::cout << "数字不合法，请重新输入。\n";
            } else if (value > upper) {
                std::cout << "超过总数，请重新输入。\n";
            } else {
                return value;
            }
        } else {
            std::cout << "数字不合法，请重新输入。\n";
        }
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

int main() {
    initConsole();

    const std::string filename = "names.txt"; // 名单文件名
    const int DEFAULT_COUNT = 43;             // 无文件时默认人数
    const std::string BLOCKED_NAME = "小明";  // 有名单时屏蔽的名字

    // ------------------------------------------------------------------
    // 先读名单，确定总数，再询问 N，避免 N 超过总数
    // ------------------------------------------------------------------
    std::ifstream fin(filename, std::ios::in | std::ios::binary);
    bool useFile = false;
    std::vector<std::string> names;

    if (fin) {
        int S = 0;
        if (fin >> S && S > 0) {
            fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            names.reserve(S);
            std::string line;

            for (int i = 0; i < S; ++i) {
                if (std::getline(fin, line)) {
                    cleanLine(line, i == 0);
                    if (line.empty()) continue;
                    names.push_back(line);
                } else {
                    break;
                }
            }

            if (!names.empty()) {
                useFile = true;
            }
        }
    }

    int total = useFile ? static_cast<int>(names.size()) : DEFAULT_COUNT;

    // 询问 N：必须在 1 ~ total 之间
    int N = readBoundedInt(
        "请输入抽取个数 N（1 ~ " + std::to_string(total) + "）：", total);

    // ------------------------------------------------------------------
    // 最保守的随机种子
    // 1) 不用 std::random_device，避免旧 MinGW 上退化为固定值
    // 2) 三种独立时间源：墙上时间、单调时钟、进程 CPU 时钟
    // 3) 再加一个栈变量地址，利用 ASLR 提供的随机性
    // 4) 用 seed_seq 混合，避免简单拼接的相关性
    // ------------------------------------------------------------------
    std::uint64_t tWall =
        static_cast<std::uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());

    std::uint64_t tSteady =
        static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());

    std::uint64_t tClock =
        static_cast<std::uint64_t>(std::clock());

    std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(&N);
    std::uint64_t tAddr = static_cast<std::uint64_t>(addr);

    std::seed_seq seq{
        static_cast<std::uint32_t>(tWall & 0xFFFFFFFFu),
        static_cast<std::uint32_t>(tWall >> 32),
        static_cast<std::uint32_t>(tSteady & 0xFFFFFFFFu),
        static_cast<std::uint32_t>(tSteady >> 32),
        static_cast<std::uint32_t>(tClock & 0xFFFFFFFFu),
        static_cast<std::uint32_t>(tClock >> 32),
        static_cast<std::uint32_t>(tAddr & 0xFFFFFFFFu),
        static_cast<std::uint32_t>(tAddr >> 32)
    };

    std::mt19937_64 gen(seq);
    // ------------------------------------------------------------------

    // 判断是否处于作弊时间段
    bool cheat = isCheatTime();

    if (useFile) {
        // 构建候选索引列表（0 ~ total-1）
        std::vector<int> candidates(total);
        std::iota(candidates.begin(), candidates.end(), 0);

        // 有名单：仅在作弊时间段内屏蔽“小明”
        if (cheat) {
            candidates.erase(
                std::remove_if(candidates.begin(), candidates.end(),
                    [&](int idx) { return names[idx] == BLOCKED_NAME; }),
                candidates.end()
            );
        }

        // 作弊可能让候选数少 1，静默截断，不再报“超过总数”
        int available = static_cast<int>(candidates.size());
        int k = std::min(N, available);

        std::shuffle(candidates.begin(), candidates.end(), gen);

        std::cout << "从文件 " << filename << " 中随机抽取 " << k << " 人：\n";
        for (int i = 0; i < k; ++i) {
            std::cout << names[candidates[i]] << '\n';
        }
    } else {
        // 构建候选数字列表（1 ~ 43）
        std::vector<int> candidates(total);
        std::iota(candidates.begin(), candidates.end(), 1);

        // 无名单：仅在作弊时间段内排除 38 号
        if (cheat && total >= 38) {
            candidates.erase(
                std::remove(candidates.begin(), candidates.end(), 38),
                candidates.end()
            );
        }

        // 作弊可能让候选数少 1，静默截断
        int available = static_cast<int>(candidates.size());
        int k = std::min(N, available);

        std::shuffle(candidates.begin(), candidates.end(), gen);

        std::cout << "未找到名单文件，从 1~" << total
                  << " 中随机抽取 " << k << " 个数字：\n";
        for (int i = 0; i < k; ++i) {
            std::cout << candidates[i] << '\n';
        }
    }

    waitExit();
    return 0;
}