"""待办清单工具入口：默认启动 GUI，可用 --cli 切换到命令行模式。"""
import sys

from manager import TodoManager

HELP_TEXT = """可用命令：
  add <描述>        新建任务
  delete <ID>       删除任务
  done <ID>         标记任务完成
  undo <ID>         取消完成
  list              查看全部任务
  list open         仅查看未完成任务
  help              显示本帮助
  exit / quit       退出程序
"""


def print_tasks(tasks):
    """以可读格式输出任务列表。"""
    if not tasks:
        print("（暂无任务）")
        return
    for t in tasks:
        mark = "[x]" if t["done"] else "[ ]"
        print(f"{mark} #{t['id']} {t['description']}")


def parse_id(arg):
    """解析任务 ID，非法输入返回 None。"""
    try:
        return int(arg)
    except ValueError:
        return None


def cli_main():
    """原命令行交互循环。"""
    manager = TodoManager()
    print("欢迎使用待办清单工具，输入 help 查看命令。")

    while True:
        try:
            line = input("todo> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break

        if not line:
            continue

        parts = line.split(maxsplit=1)
        cmd = parts[0].lower()
        arg = parts[1].strip() if len(parts) > 1 else ""

        if cmd in ("exit", "quit"):
            break

        if cmd == "help":
            print(HELP_TEXT)

        elif cmd == "add":
            if not arg:
                print("用法: add <描述>")
                continue
            task = manager.add(arg)
            print(f"已添加任务 #{task['id']}: {task['description']}")

        elif cmd in ("delete", "done", "undo"):
            task_id = parse_id(arg)
            if task_id is None:
                print(f"用法: {cmd} <ID>")
                continue
            if cmd == "delete":
                task = manager.delete(task_id)
                if task:
                    print(f"已删除任务 #{task['id']}: {task['description']}")
                else:
                    print(f"未找到任务 #{task_id}")
            else:
                task = manager.set_done(task_id, done=(cmd == "done"))
                if task:
                    state = "已完成" if task["done"] else "已取消完成"
                    print(f"任务 #{task['id']} {state}: {task['description']}")
                else:
                    print(f"未找到任务 #{task_id}")

        elif cmd == "list":
            if arg.lower() == "open":
                print_tasks(manager.list_todos(done_only=False))
            else:
                print_tasks(manager.list_todos())

        else:
            print(f"未知命令: {cmd}，输入 help 查看帮助。")


def gui_main():
    from gui import main as gui_entry
    gui_entry()


def main():
    if "--cli" in sys.argv[1:]:
        cli_main()
    else:
        gui_main()


if __name__ == "__main__":
    main()
