# 📋 迷你待办

一个轻量、纯标准库实现的待办清单（Todo List）工具，提供 **命令行**、**桌面 GUI** 与 **移动 Web** 三种形态，本地 JSON 持久化，无任何第三方依赖。

## ✨ 功能

- **任务管理**：新建、删除、标记完成 / 取消完成、双击 / 长按编辑描述与时间
- **时间字段**：日期（年/月/日）+ 时刻（时/分，24 小时制），可选填；校验完整性与日期合法性
- **双排序模式（完全隔离）**
  - **按优先级**：按基础顺序显示（用 ↑↓ 调整）
  - **按时间**：无时间任务优先 → 按时间升序 → 相同时间按 `time_order`（可在组内用 ↑↓ 微调，不影响基础优先级）
- **多选批量操作**：标记完成 / 取消完成 / 删除 / 移动，均作用于所有选中项
- **键盘 / 触摸友好**：输入框焦点全选、回车流转焦点；移动端长按编辑、点行多选、底部大按钮操作栏
- **数据一致性**：每次操作即时写回本地 JSON（桌面/GUI）或 `localStorage`（Web），启动自动加载

## 📦 三种形态

| 形态 | 入口 | 说明 |
| --- | --- | --- |
| 命令行 | `main.py --cli` | 交互式命令循环（add / delete / done / undo / list …） |
| 桌面 GUI | `python main.py` | Tkinter + ttk.Treeview 图形界面 |
| 移动 / Web | `todo.html` | 单文件 Web 应用，手机浏览器直接运行 |

## 🚀 快速开始

```bash
# 桌面 GUI
python main.py

# 命令行模式
python main.py --cli

# 移动 / Web 版（任意静态服务器，或直接双击打开）
python -m http.server 8000
# 浏览器访问 http://localhost:8000/todo.html
```

## 📄 打包为 Windows 可执行文件

```bash
pip install pyinstaller
python -m PyInstaller --onefile --windowed --name todo_app main.py
```

> 打包后数据目录 `data/` 自动创建于 `.exe` 同目录下（`storage.py` 已适配 frozen 环境）。

## 🤳 移动端使用

1. 将 `todo.html` 传到安卓手机；
2. 用浏览器打开（或点击浏览器菜单“添加到主屏幕”，全屏运行）；
3. 数据保存在浏览器 `localStorage`，无需联网。

## 📁 目录结构

```
todo/
├── main.py          # 入口：默认启动 GUI，--cli 走命令行
├── gui.py           # 桌面 GUI（Tkinter + Treeview）
├── manager.py       # 核心业务逻辑（TodoManager）
├── storage.py       # JSON 持久化 + UI 状态持久化
├── todo.html        # 移动 / Web 单文件应用
├── todo_app.exe     # 可选：打包好的 Windows 可执行文件
└── data/
    └── todos.json   # 任务数据（自动生成）
```

## 🧠 核心逻辑说明

- **优先级 = 基础顺序**：任务在列表中的顺序即优先级，`time_order`（同一时间组内相对顺序）与之完全独立。
- **时间视图排序键**：`无时间优先 → (date, time) 升序 → time_order 升序`。
- **移动语义**：优先级视图下 ↑↓ 调整基础顺序；时间视图下 ↑↓ 仅调整**相同时间任务**组内顺序，多选时要求所有选中任务时间相同，且受组边界限制。

## 🛠 技术栈

- Python 3（仅标准库：`tkinter`、`json`、`datetime`）
- HTML + CSS + Vanilla JS（Web 版，无框架）

## 📝 许可

MIT License
