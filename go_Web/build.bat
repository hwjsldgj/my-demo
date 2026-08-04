@echo off
echo ????AI??
echo ?????? MinGW (g++ 15+) ? ONNX Runtime
echo.
echo ???????????? ONNX Runtime ?????
set ONNXRUNTIME_INCLUDE=D:\onnxruntime\include
set ONNXRUNTIME_LIB=D:\onnxruntime\lib
echo.
echo ????...
g++ -std=c++17 -c src/board.cpp -o src/board.o -I%%ONNXRUNTIME_INCLUDE%%
g++ -std=c++17 -c src/ai.cpp -o src/ai.o -I%%ONNXRUNTIME_INCLUDE%%
g++ -std=c++17 -c src/main.cpp -o src/main.o -I%%ONNXRUNTIME_INCLUDE%%
g++ src/board.o src/ai.o src/main.o -o bin/go_web.exe -lws2_32 -L%%ONNXRUNTIME_LIB%% -lonnxruntime
if exist bin\go_web.exe (
    echo ???????????bin\go_web.exe
) else (
    echo ?????????????
)
pause
