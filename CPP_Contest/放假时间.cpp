// ============================================================================
//  放假时间.cpp
//  Windows 桌面右下角国庆放假倒计时窗口
//
//  平台：Windows 10 64 位
//  编译：MinGW-w64 g++，C++14
//  命令：见文件末尾
// ============================================================================

#include <windows.h>
#include <ctime>
#include <cwchar>
#include <cstdio>

// ============================================================================
//  配置常量
// ============================================================================

// 客户区基准尺寸（像素）
const int BASE_CLIENT_WIDTH  = 240;
const int BASE_CLIENT_HEIGHT = 140;

// 窗口最小宽度（像素）
const int MIN_WINDOW_WIDTH = 180;

// 目标时间：2026-09-30 21:20:00 本地时间
const int TARGET_YEAR = 2026;
const int TARGET_MON  = 9;
const int TARGET_DAY  = 30;
const int TARGET_HOUR = 21;
const int TARGET_MIN  = 20;
const int TARGET_SEC  = 0;

// 定时器
const UINT_PTR TIMER_ID       = 1;
const UINT     TIMER_INTERVAL = 1000;   // 毫秒

// 字体
const wchar_t FONT_FACE[]     = L"微软雅黑";
const wchar_t FONT_FALLBACK[] = L"Segoe UI";
const int     FONT_EXTRA      = 2;      // 在自适应字号基础上再放大
const int     FONT_SIZE_MIN   = 10;
const int     FONT_SIZE_MAX   = 200;

// 颜色
const COLORREF COLOR_BG   = RGB(255, 255, 255);
const COLORREF COLOR_TEXT = RGB(0, 0, 0);

// 窗口类名
const wchar_t WINDOW_CLASS_NAME[] = L"CountdownWindowClass";

// ============================================================================
//  全局状态
// ============================================================================

HINSTANCE g_hInstance    = nullptr;
HWND      g_hOwnerWindow = nullptr;   // 隐藏 owner，避免任务栏图标
HFONT     g_hFont        = nullptr;
HBRUSH    g_hBackBrush   = nullptr;

int    g_currentFontSize = 0;
time_t g_targetTime      = 0;
double g_windowAspect    = 1.0;       // 窗口宽 / 高

// 缩放锚点：进入缩放时窗口的原始矩形（右下角固定）
RECT g_resizeAnchor = { 0, 0, 0, 0 };

// ============================================================================
//  辅助函数
// ============================================================================

// 构造目标时间戳
time_t MakeTargetTime()
{
    std::tm tm = {};
    tm.tm_year  = TARGET_YEAR - 1900;
    tm.tm_mon   = TARGET_MON - 1;
    tm.tm_mday  = TARGET_DAY;
    tm.tm_hour  = TARGET_HOUR;
    tm.tm_min   = TARGET_MIN;
    tm.tm_sec   = TARGET_SEC;
    tm.tm_isdst = 0;   // 国内无夏令时
    return std::mktime(&tm);
}

// 生成倒计时文本；bufferSize 必须大于 0
void FormatCountdown(wchar_t* buffer, size_t bufferSize)
{
    if (bufferSize == 0) return;

    const time_t now  = std::time(nullptr);
    const double diff = std::difftime(g_targetTime, now);

    if (diff <= 0) {
        const wchar_t kDone[] = L"已放假";
        size_t i = 0;
        while (kDone[i] != L'\0' && i < bufferSize - 1) {
            buffer[i] = kDone[i];
            ++i;
        }
        buffer[i] = L'\0';
        return;
    }

    const int totalSec = static_cast<int>(diff);
    const int hours    = totalSec / 3600;
    const int minutes  = (totalSec % 3600) / 60;
    const int seconds  = totalSec % 60;

    std::swprintf(buffer, bufferSize, L"%02d:%02d:%02d", hours, minutes, seconds);
    buffer[bufferSize - 1] = L'\0';   // 双保险，确保结尾
}

// 根据客户区大小更新字体
void UpdateFont(int clientWidth, int clientHeight)
{
    int byWidth  = clientWidth  / 10;
    int byHeight = clientHeight / 4;
    int fontSize = (byWidth < byHeight ? byWidth : byHeight) + FONT_EXTRA;

    if (fontSize < FONT_SIZE_MIN) fontSize = FONT_SIZE_MIN;
    if (fontSize > FONT_SIZE_MAX) fontSize = FONT_SIZE_MAX;

    if (fontSize == g_currentFontSize) return;

    g_currentFontSize = fontSize;

    if (g_hFont) {
        DeleteObject(g_hFont);
        g_hFont = nullptr;
    }

    g_hFont = CreateFontW(
        fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, FONT_FACE);

    if (!g_hFont) {
        g_hFont = CreateFontW(
            fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, FONT_FALLBACK);
    }
}

// ============================================================================
//  窗口过程
// ============================================================================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        UpdateFont(rc.right - rc.left, rc.bottom - rc.top);
        SetTimer(hwnd, TIMER_ID, TIMER_INTERVAL, nullptr);
        return 0;
    }

    case WM_TIMER:
        if (wParam == TIMER_ID) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            UpdateFont(LOWORD(lParam), HIWORD(lParam));
            InvalidateRect(hwnd, nullptr, FALSE);   // 拖拽中实时刷新
        }
        return 0;

    case WM_ENTERSIZEMOVE:
        GetWindowRect(hwnd, &g_resizeAnchor);
        return 0;

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = MIN_WINDOW_WIDTH;
        mmi->ptMinTrackSize.y = static_cast<LONG>(MIN_WINDOW_WIDTH / g_windowAspect + 0.5);
        if (mmi->ptMinTrackSize.y < 1) mmi->ptMinTrackSize.y = 1;
        return 0;
    }

    case WM_SIZING:
    {
        RECT* rc = reinterpret_cast<RECT*>(lParam);

        // 只允许从左边、上边、左上角改变大小
        // 拖动其他方向（右、下、右下等）时恢复锚定矩形，避免异常跳动
        if (wParam != WMSZ_LEFT && wParam != WMSZ_TOP && wParam != WMSZ_TOPLEFT) {
            *rc = g_resizeAnchor;
            return TRUE;
        }

        int w = rc->right - rc->left;
        int h = rc->bottom - rc->top;

        // 保持宽高比
        if (wParam == WMSZ_TOP) {
            w = static_cast<int>(h * g_windowAspect + 0.5);
        } else {
            h = static_cast<int>(w / g_windowAspect + 0.5);
        }

        // 最小尺寸保护
        if (w < MIN_WINDOW_WIDTH) {
            w = MIN_WINDOW_WIDTH;
            h = static_cast<int>(w / g_windowAspect + 0.5);
        }
        if (h < 1) h = 1;

        // 右下角固定，向左、向上扩展
        rc->right  = g_resizeAnchor.right;
        rc->bottom = g_resizeAnchor.bottom;
        rc->left   = g_resizeAnchor.right  - w;
        rc->top    = g_resizeAnchor.bottom - h;
        return TRUE;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        const int w = rcClient.right;
        const int h = rcClient.bottom;

        // 背景
        if (g_hBackBrush) {
            FillRect(hdc, &rcClient, g_hBackBrush);
        }

        // 文字环境
        const int        oldBkMode = SetBkMode(hdc, TRANSPARENT);
        const COLORREF   oldColor  = SetTextColor(hdc, COLOR_TEXT);
        const HGDIOBJ    oldFont   = SelectObject(hdc, g_hFont);

        // 第一行：距离国庆放假还有
        int y1a = h * 15 / 100;
        int y1b = h * 52 / 100;
        if (y1b <= y1a) y1b = y1a + 1;
        RECT rcLine1 = { 0, y1a, w, y1b };
        DrawTextW(hdc, L"距离国庆放假还有", -1, &rcLine1,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 第二行：倒计时
        wchar_t timeText[64] = {};
        FormatCountdown(timeText, 64);

        int y2a = h * 52 / 100;
        int y2b = h * 90 / 100;
        if (y2b <= y2a) y2b = y2a + 1;
        RECT rcLine2 = { 0, y2a, w, y2b };
        DrawTextW(hdc, timeText, -1, &rcLine2,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 还原 GDI 状态
        SelectObject(hdc, oldFont);
        SetTextColor(hdc, oldColor);
        SetBkMode(hdc, oldBkMode);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        if (g_hFont)      { DeleteObject(g_hFont);      g_hFont      = nullptr; }
        if (g_hBackBrush) { DeleteObject(g_hBackBrush); g_hBackBrush = nullptr; }
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
//  程序入口
// ============================================================================

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    g_hInstance  = hInstance;
    g_targetTime = MakeTargetTime();

    // ---- 注册窗口类 ----
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc)) {
        return 0;
    }

    // ---- 创建背景画刷 ----
    g_hBackBrush = CreateSolidBrush(COLOR_BG);
    if (!g_hBackBrush) {
        return 0;
    }

    // ---- 计算窗口尺寸与宽高比 ----
    const DWORD style   = WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
    const DWORD exStyle = 0;

    RECT rc = { 0, 0, BASE_CLIENT_WIDTH, BASE_CLIENT_HEIGHT };
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);

    const int windowWidth  = rc.right - rc.left;
    const int windowHeight = rc.bottom - rc.top;

    g_windowAspect = static_cast<double>(windowWidth) / windowHeight;

    // ---- 定位到主显示器工作区右下角 ----
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int x = workArea.right  - windowWidth;
    const int y = workArea.bottom - windowHeight;

    // ---- 隐藏 owner 窗口：避免任务栏图标，保留标准标题栏外观 ----
    g_hOwnerWindow = CreateWindowW(
        L"STATIC", L"", WS_POPUP,
        0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);

    // ---- 创建主窗口 ----
    HWND hwnd = CreateWindowExW(
        exStyle, WINDOW_CLASS_NAME, L"", style,
        x, y, windowWidth, windowHeight,
        g_hOwnerWindow, nullptr, hInstance, nullptr);

    if (!hwnd) {
        if (g_hOwnerWindow) {
            DestroyWindow(g_hOwnerWindow);
            g_hOwnerWindow = nullptr;
        }
        if (g_hBackBrush) {
            DeleteObject(g_hBackBrush);
            g_hBackBrush = nullptr;
        }
        return 0;
    }

    // ---- 显示并置底 ----
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd);
    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    // ---- 消息循环 ----
    MSG msg = {};
    BOOL ret = 0;
    while ((ret = GetMessage(&msg, nullptr, 0, 0)) != 0) {
        if (ret == -1) {
            break;   // 消息循环出错，退出
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // ---- 清理隐藏 owner ----
    if (g_hOwnerWindow) {
        DestroyWindow(g_hOwnerWindow);
        g_hOwnerWindow = nullptr;
    }

    return static_cast<int>(msg.wParam);
}

// ============================================================================
//  编译命令
// ----------------------------------------------------------------------------
//  PowerShell：
//    g++ -std=c++14 -Os -s -mwindows -municode -static -static-libgcc
//        -static-libstdc++ -ffunction-sections -fdata-sections
//        "-Wl,--gc-sections" -DUNICODE -D_UNICODE -finput-charset=UTF-8
//        "C:\Users\90579\Desktop\放假时间.cpp"
//        -o "C:\Users\90579\Desktop\放假时间.exe"
//
//  CMD：
//    g++ -std=c++14 -Os -s -mwindows -municode -static -static-libgcc
//        -static-libstdc++ -ffunction-sections -fdata-sections
//        -Wl,--gc-sections -DUNICODE -D_UNICODE -finput-charset=UTF-8
//        "放假时间.cpp" -o "放假时间.exe"
// ============================================================================