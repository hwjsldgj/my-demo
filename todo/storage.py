"""数据持久化模块：负责本地 JSON 文件的读写。"""
import json
import os
import sys


def _app_dir():
    """返回数据目录所在的根：打包后为 exe 所在目录，否则为源码目录。"""
    if getattr(sys, "frozen", False):
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))


DATA_DIR = os.path.join(_app_dir(), "data")
DATA_FILE = os.path.join(DATA_DIR, "todos.json")


def load_todos():
    """从 JSON 文件加载任务列表；文件缺失或损坏时返回空列表。"""
    if not os.path.exists(DATA_FILE):
        return []
    try:
        with open(DATA_FILE, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data if isinstance(data, list) else []
    except (json.JSONDecodeError, OSError):
        return []


def save_todos(todos):
    """将任务列表写入 JSON 文件，目录不存在时自动创建。"""
    os.makedirs(DATA_DIR, exist_ok=True)
    with open(DATA_FILE, "w", encoding="utf-8") as f:
        json.dump(todos, f, ensure_ascii=False, indent=2)


UI_STATE_FILE = os.path.join(DATA_DIR, "ui_state.json")


def load_ui_state():
    """加载 UI 状态（当前仅日期输入的年/月/日）；文件缺失或损坏返回空 dict。"""
    if not os.path.exists(UI_STATE_FILE):
        return {}
    try:
        with open(UI_STATE_FILE, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else {}
    except (json.JSONDecodeError, OSError):
        return {}


def save_ui_state(state):
    """保存 UI 状态（如日期输入的年/月/日）到 JSON 文件。"""
    os.makedirs(DATA_DIR, exist_ok=True)
    with open(UI_STATE_FILE, "w", encoding="utf-8") as f:
        json.dump(state, f, ensure_ascii=False, indent=2)
