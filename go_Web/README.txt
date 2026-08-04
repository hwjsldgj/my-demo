围棋 AI 对弈程序（C++ + ONNX Runtime）
======================================

【运行方法】
1. 进入 bin 文件夹
2. 双击 go_web.exe
3. 在浏览器中打开 http://localhost:8080

【重新编译】
1. 安装 MinGW (g++ 15+) 和 ONNX Runtime
2. 修改 build.bat 中的 ONNX Runtime 路径
3. 运行 build.bat

【模型转换（可选）】
1. 进入 tools 文件夹
2. 安装 Python 依赖：pip install -r requirements.txt
3. 运行 export_onnx.py 导出 ONNX 模型

【注意事项】
- 需要 Windows 10/11 和 Visual C++ Redistributable
- 如果缺少 DLL，请从本机复制 onnxruntime.dll 到 bin 目录

打包时间：2026-08-04 23:37:33
