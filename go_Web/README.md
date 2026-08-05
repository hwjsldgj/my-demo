# 围棋 AI 对弈程序

一个基于 **C++** 和 **ONNX Runtime** 的围棋 AI 对战平台，支持 9×9 和 19×9 棋盘，提供多种 AI 难度级别，并配有简洁的 Web 可视化界面。

---

## ✨ 特性

- 🎮 **双棋盘支持**：9×9 和 19×19 自由切换
- 🤖 **五档 AI 难度**：
  - 极简（规则）
  - 简单（贪心）
  - 中等（2层搜索）
  - 困难（6层搜索）
  - 困难（MCTS 搜索，仅 19×19）
- 🖥️ **Web 可视化**：基于 HTML5 Canvas 的交互式棋盘，自适应窗口
- 🧠 **神经网络推理**：MCTS 模式使用 ONNX Runtime 加载预训练模型（TinyGo）
- 🌐 **前后端分离**：C++ 后端提供 RESTful API，前端独立渲染
- 🔄 **动态适配**：前端自动适配后端实际棋盘大小，解决尺寸不一致问题
- 📦 **一键打包**：提供打包脚本，方便分发部署

---

## 🛠️ 技术栈

| 组件 | 技术 |
|------|------|
| 后端 | C++17, httplib, nlohmann/json, ONNX Runtime |
| AI 算法 | 规则/贪心/搜索/MCTS |
| 神经网络 | ONNX (TinyGo 模型) |
| 前端 | HTML5, CSS3, JavaScript (Canvas) |
| 构建 | MinGW (g++), Make / 手动编译 |
| 打包 | PowerShell / Batch 脚本 |

---

## 📂 项目结构（写反了）

```
go/
├── main.cpp              # HTTP 服务器主程序
├── ai.cpp                # AI 核心算法（含 MCTS、ONNX 推理）
├── board.cpp             # 棋盘规则（落子、提子、数子）
├── go_common.h           # 公共定义与类声明
├── index.html            # 前端界面（含动态适配）
├── httplib.h             # HTTP 库（单头文件）
├── json.hpp              # JSON 库（单头文件）
├── export_onnx.py        # PyTorch 模型 → ONNX 导出脚本（可选）
├── model.py              # TinyGo 网络定义（可选）
├── package.bat           # Windows 打包脚本
├── package.ps1           # Windows PowerShell 打包脚本
├── go_model.onnx         # 19×9 预训练模型（需自行下载）
└── go_Web/               # 打包输出目录（运行时文件）
```

---

## 🚀 运行与编译

### 方式一：直接运行（无需编译）

如果你已有编译好的 `go_web.exe` 和依赖文件（`onnxruntime.dll`, `go_model.onnx`），只需：

1. 进入 `go_Web\bin` 目录
2. 双击 `go_web.exe`
3. 在浏览器打开 `http://localhost:8080`

### 方式二：自行编译

#### 前置条件
- **MinGW** (g++ 15+ 或兼容版本)
- **ONNX Runtime** 下载并解压到 `D:\onnxruntime`（或自定义路径）
- **Windows 10/11**（其他系统需调整编译参数）

#### 编译步骤
```bash
cd C:\Users\90579\Desktop\go
g++ -std=c++17 -c board.cpp -o board.o -ID:/onnxruntime/include
g++ -std=c++17 -c ai.cpp -o ai.o -ID:/onnxruntime/include
g++ -std=c++17 -c main.cpp -o main.o -ID:/onnxruntime/include
g++ board.o ai.o main.o -o go_web.exe -lws2_32 -LD:/onnxruntime/lib -lonnxruntime
```

如果 ONNX Runtime 路径不同，请修改 `-I` 和 `-L` 参数。

#### 运行编译后的程序
```bash
go_web.exe
```

---

## 🎯 AI 难度说明

| 难度 | 名称 | 算法 | 说明 |
|------|------|------|------|
| 0 | 极简 | 规则 | 只下能提子的位置或逃生 |
| 1 | 简单 | 贪心 | 评估当前局部价值选点 |
| 2 | 中等 | 2层搜索 | 简单 minimax 搜索 |
| 3 | 困难 | 6层搜索 | 深度搜索 + α-β 剪枝 |
| 4 | 困难 (MCTS) | 蒙特卡洛树搜索 + 神经网络 | 仅 19×9，需 ONNX 模型；9×9 时自动降级为难度3 |

> **注意**：9×9 棋盘不支持 MCTS，前端会自动禁用该选项，后端也会降级。

---

## 🧠 模型转换（可选）

如果你拥有 PyTorch 的 TinyGo 预训练权重（`best_model.pth`），可以使用提供的 `export_onnx.py` 导出 ONNX 模型：

```bash
pip install torch onnx
python export_onnx.py
```

生成的 `go_model.onnx` 即可用于本程序。

---

## 📦 打包与分发

项目提供了打包脚本，可将运行时文件、源码、依赖库等打包到 `go_Web` 目录：

- **Windows 批处理**：`package.bat`
- **PowerShell**：`package.ps1`

运行后，桌面将生成 `go_Web` 文件夹，可直接压缩分发。

---

## 🐛 已知问题与限制

- MCTS 仅支持 19×9 棋盘，且需要 `go_model.onnx` 存在。
- 9×9 棋盘仅使用非神经网络算法，棋力有限。
- 本程序为单机版，不支持多人对弈。
- ONNX Runtime 仅提供 Windows 下的 DLL，其他平台需自行适配。

---

## 🔧 贡献指南

欢迎提交 Issue 或 Pull Request。请确保代码风格与现有代码一致，并附带测试说明。

---

## 📜 许可证

本项目基于 **MIT License** 开源，详情请见 [LICENSE](LICENSE) 文件。

---

## 🙏 致谢

- [cpp-httplib](https://github.com/yhirose/cpp-httplib) – 简洁的 HTTP 库
- [nlohmann/json](https://github.com/nlohmann/json) – 现代 JSON 处理库
- [ONNX Runtime](https://onnxruntime.ai/) – 跨平台推理引擎
- [TinyGo](https://github.com/laochoupro/TinyGo) – 轻量级围棋神经网络参考实现
- [deppseek](https://chat.deepseek.com/) - 编写主要代码

