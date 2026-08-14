"""迷你待办 GUI：基于 Tkinter + ttk.Treeview，复用 manager/storage 数据层。

- 时间输入区位于描述框左侧（年/月/日 + 时/分 Spinbox，可手动输入或箭头微调）。
- 回车键在时间框间跳转焦点（年→月→日→时→分→描述），描述框回车添加。
- 时间完整性仅在添加时检查：时刻空则不检查也不存储；时刻有输入须时分齐全且日期完整，
  才存储为 YYYY-MM-DD HH:MM；否则任务无时间。
- 列表为表格（状态/序号/时间/任务），时间列在左。
- 双排序模式（优先级/时间）完全隔离；上下移动只调整基础顺序。
- 状态列 ☐/☑ 点击切换完成状态且不改选区；支持多选批量、空白取消全选。
"""
import tkinter as tk
from tkinter import ttk, messagebox
from datetime import datetime

from manager import TodoManager
from storage import load_ui_state, save_ui_state

YEARS = list(range(2020, 2036))


class TodoApp:
    def __init__(self, root):
        self.root = root
        self.manager = TodoManager()
        self.sort_var = tk.StringVar(value="priority")  # priority | time

        root.title("📋 迷你待办")
        root.resizable(False, False)
        self._center_window(root, 720, 540)

        self._build_ui(root)
        self._restore_date_inputs()
        self._update_sort_btn()
        self.refresh()
        self._status("就绪")

    # ---------- 布局 ----------
    @staticmethod
    def _center_window(root, w, h):
        root.update_idletasks()
        x = (root.winfo_screenwidth() - w) // 2
        y = (root.winfo_screenheight() - h) // 3
        root.geometry(f"{w}x{h}+{x}+{y}")

    def _build_ui(self, root):
        # 1. 输入区：时间框(年/月/日 + 时/分) + 描述 + 添加
        top = tk.Frame(root)
        top.pack(fill="x", padx=10, pady=(10, 4))

        self.year_var = tk.StringVar(value="")
        self.month_var = tk.StringVar(value="")
        self.day_var = tk.StringVar(value="")
        self.hour_var = tk.StringVar(value="")
        self.minute_var = tk.StringVar(value="")

        self.year_sb = tk.Spinbox(top, from_=2020, to=2035, width=4, justify="center",
                                  textvariable=self.year_var)
        self.month_sb = tk.Spinbox(top, from_=1, to=12, width=2, justify="center",
                                   textvariable=self.month_var)
        self.day_sb = tk.Spinbox(top, from_=1, to=31, width=2, justify="center",
                                 textvariable=self.day_var)
        self.hour_sb = tk.Spinbox(top, from_=0, to=23, width=2, justify="center",
                                  textvariable=self.hour_var)
        self.minute_sb = tk.Spinbox(top, from_=0, to=59, width=2, justify="center",
                                    textvariable=self.minute_var)
        for sb in (self.year_sb, self.month_sb, self.day_sb,
                   self.hour_sb, self.minute_sb):
            sb.pack(side="left", padx=1)

        self.entry = tk.Entry(top)
        self.entry.pack(side="left", fill="x", expand=True, padx=(6, 4))
        self.entry.bind("<Return>", lambda e: self.add_task())
        tk.Button(top, text="添加", command=self.add_task, width=6).pack(side="left")

        # 焦点流转：回车在时间框间前进；Tab 在 时间框+描述 之间循环
        chain = (self.year_sb, self.month_sb, self.day_sb,
                 self.hour_sb, self.minute_sb, self.entry)
        for i, w in enumerate(chain[:-1]):
            w.bind("<Return>", lambda e, n=i: self._focus_next(n))
        for i, w in enumerate(chain):
            nxt = chain[(i + 1) % len(chain)]
            w.bind("<Tab>", lambda e, t=nxt: self._tab_next(t))
        for w in chain:
            w.bind("<FocusIn>", lambda e, t=w: self._select_all_text(t))

        # 2. 过滤区 + 排序切换
        filt = tk.Frame(root)
        filt.pack(fill="x", padx=10, pady=4)
        self.filter_var = tk.StringVar(value="all")
        for text, val in (("全部", "all"), ("未完成", "open"), ("已完成", "done")):
            tk.Radiobutton(filt, text=text, variable=self.filter_var, value=val,
                           command=self.refresh).pack(side="left", padx=(0, 14))
        self.sort_btn = tk.Button(filt, command=self.toggle_sort, width=14)
        self.sort_btn.pack(side="right")

        # 3. 列表区：Treeview 表格 + 右侧 ↑↓
        list_frame = tk.Frame(root)
        list_frame.pack(fill="both", expand=True, padx=10, pady=4)

        left = tk.Frame(list_frame)
        left.pack(side="left", fill="both", expand=True)
        cols = ("status", "rank", "time", "task")
        self.tree = ttk.Treeview(left, columns=cols, show="headings",
                                 selectmode="extended", height=13)
        for cid, text, width, anchor in (("status", "状态", 48, "center"),
                                         ("rank", "序号", 48, "center"),
                                         ("time", "时间", 150, "w"),
                                         ("task", "任务", 340, "w")):
            self.tree.heading(cid, text=text)
            self.tree.column(cid, width=width, anchor=anchor, stretch=False)
        scroll = ttk.Scrollbar(left, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=scroll.set)
        scroll.pack(side="right", fill="y")
        self.tree.pack(side="left", fill="both", expand=True)
        self.tree.bind("<Button-1>", self.on_row_click)
        self.tree.bind("<Double-Button-1>", self.edit_task_at)
        self.tree.bind("<Control-a>", lambda e: self.select_all() or "break")
        # 状态列对应的 identify_column 编号（columns 顺序 +1）
        self._status_col = "#" + str(cols.index("status") + 1)

        right = tk.Frame(list_frame)
        right.pack(side="right", fill="y", padx=(6, 0))
        tk.Button(right, text="↑", command=self.move_up, width=3).pack(pady=(2, 0))
        tk.Button(right, text="↓", command=self.move_down, width=3).pack(pady=(2, 0))

        # 4. 操作区
        ops = tk.Frame(root)
        ops.pack(fill="x", padx=10, pady=4)
        tk.Button(ops, text="标记完成", command=self.mark_done, width=9).pack(side="left")
        tk.Button(ops, text="取消完成", command=self.mark_undone, width=9).pack(side="left", padx=(6, 0))
        tk.Button(ops, text="删除", command=self.delete_task, width=9).pack(side="left", padx=(6, 0))

        # 5. 状态栏
        self.status_var = tk.StringVar(value="就绪")
        tk.Label(root, textvariable=self.status_var, anchor="w", relief="sunken",
                 bd=1).pack(fill="x", side="bottom", padx=8, pady=8)

    def _focus_next(self, idx):
        chain = (self.year_sb, self.month_sb, self.day_sb,
                 self.hour_sb, self.minute_sb, self.entry)
        if idx + 1 < len(chain):
            chain[idx + 1].focus_set()

    def _tab_next(self, target):
        target.focus_set()
        return "break"

    def _select_all_text(self, widget):
        """焦点进入输入框时全选当前文本，便于直接覆盖输入（Entry 与 Spinbox 通用）。"""
        try:
            widget.selection_range(0, "end")
        except tk.TclError:
            pass

    def _restore_date_inputs(self):
        """启动时回填上次保存的日期输入（年/月/日）。"""
        state = load_ui_state()
        self.year_var.set(state.get("year", ""))
        self.month_var.set(state.get("month", ""))
        self.day_var.set(state.get("day", ""))

    # ---------- 辅助 ----------
    def _status(self, msg, auto_clear=3000):
        self.status_var.set(msg)
        if auto_clear:
            self.root.after(auto_clear, lambda: self.status_var.set("就绪"))

    def _find_task(self, task_id):
        for task in self.manager.todos:
            if task["id"] == task_id:
                return task
        return None

    # ---------- 排序 ----------
    def _update_sort_btn(self):
        label = "按时间" if self.sort_var.get() == "time" else "按优先级"
        self.sort_btn.config(text=f"排序：{label}")

    def toggle_sort(self):
        new = "time" if self.sort_var.get() == "priority" else "priority"
        self.sort_var.set(new)
        self._update_sort_btn()
        self.refresh()
        self._status("已切换为按时间排序" if new == "time" else "已切换为按优先级排序")

    def _display_order(self, tasks):
        """按当前排序模式计算显示顺序（不改变基础顺序）。"""
        if self.sort_var.get() == "priority":
            return list(tasks)

        def key(task):
            has_time = bool(task.get("date") and task.get("time"))
            return (0 if not has_time else 1,
                    task.get("date", ""), task.get("time", ""),
                    task.get("time_order", 0))
        return sorted(tasks, key=key)

    @staticmethod
    def _time_str(task):
        """任务时间显示：仅日期与时刻都完整时返回 YYYY-MM-DD HH:MM，否则空。"""
        date = task.get("date", "")
        time = task.get("time", "")
        if date and time:
            return f"{date} {time}"
        return ""

    # ---------- 选区 ----------
    def _selected_ids(self):
        return [int(iid) for iid in self.tree.selection()]

    def _select_ids(self, id_set):
        valid = [str(i) for i in id_set if str(i) in set(self.tree.get_children())]
        self.tree.selection_set(valid)
        if valid:
            self.tree.see(valid[0])

    def select_all(self):
        self.tree.selection_set(self.tree.get_children())
        return "break"

    # ---------- 列表刷新 ----------
    def refresh(self):
        mode = self.filter_var.get()
        if mode == "all":
            tasks = self.manager.list_todos()
        elif mode == "open":
            tasks = self.manager.list_todos(done_only=False)
        else:
            tasks = self.manager.list_todos(done_only=True)

        tasks = self._display_order(tasks)

        self.tree.delete(*self.tree.get_children())
        for rank, task in enumerate(tasks, 1):
            cb = "☑" if task["done"] else "☐"
            self.tree.insert("", "end", iid=str(task["id"]),
                             values=(cb, rank, self._time_str(task), task["description"]))

    def _refresh_keep_selection(self):
        sel = set(self._selected_ids())
        self.refresh()
        self._select_ids(sel)

    # ---------- 点击处理 ----------
    def on_row_click(self, event):
        """单击：状态列切换完成（不改选区）；空白取消全选；其他列交给默认多选。"""
        if getattr(event, "num", 1) != 1:
            return
        iid = self.tree.identify_row(event.y)
        if not iid:
            # 空白区域：取消全选
            self.tree.selection_remove(*self.tree.selection())
            return "break"
        if self.tree.identify_column(event.x) == self._status_col:
            self._toggle_one(int(iid))
            return "break"

    def _toggle_one(self, task_id):
        task = self._find_task(task_id)
        if task is None:
            return
        new_state = not task["done"]
        self.manager.set_done(task_id, new_state)
        self._refresh_keep_selection()
        self._status("任务已标记完成" if new_state else "任务已取消完成")

    # ---------- 时间输入校验 ----------
    def _read_time(self):
        """读取主界面时间输入并校验。"""
        return self._validate_time(self.year_var.get(), self.month_var.get(),
                                   self.day_var.get(), self.hour_var.get(),
                                   self.minute_var.get())

    def _validate_time(self, y, mo, d, h, mi):
        """通用时间校验：返回 (date, time, error)。

        时刻为空：不检查也不存储时间；时刻有输入：须时分齐全且日期完整才存。
        """
        y, mo, d, h, mi = (str(x).strip() for x in (y, mo, d, h, mi))
        if not h and not mi:
            return "", "", ""
        if not h or not mi:
            return "", "", "时刻必须同时填写时和分"
        if not (y and mo and d):
            return "", "", "日期必须完整"
        try:
            datetime(int(y), int(mo), int(d))
        except ValueError:
            return "", "", "日期无效，请检查所选日期"
        date = f"{int(y):04d}-{int(mo):02d}-{int(d):02d}"
        time = f"{int(h):02d}:{int(mi):02d}"
        return date, time, ""

    # ---------- 操作 ----------
    def add_task(self):
        text = self.entry.get().strip()
        if not text:
            messagebox.showwarning("提示", "任务描述不能为空。")
            return
        date, time, err = self._read_time()
        if err:
            messagebox.showwarning("提示", err)
            return
        self.manager.add(text, date=date, time=time)
        save_ui_state({"year": self.year_var.get(),
                       "month": self.month_var.get(),
                       "day": self.day_var.get()})
        self.entry.delete(0, tk.END)
        # 日期保留原值，时刻清空
        self.hour_var.set("")
        self.minute_var.set("")
        self.refresh()
        self._status("任务已添加")

    def mark_done(self):
        ids = self._selected_ids()
        if not ids:
            messagebox.showwarning("提示", "请先在列表中选择要操作的任务。")
            return
        changed = 0
        for tid in ids:
            task = self._find_task(tid)
            if task and not task["done"]:
                self.manager.set_done(tid, True)
                changed += 1
        self._refresh_keep_selection()
        self._status(f"已标记 {changed} 个任务为完成" if changed else "所选任务无需操作")

    def mark_undone(self):
        ids = self._selected_ids()
        if not ids:
            messagebox.showwarning("提示", "请先在列表中选择要操作的任务。")
            return
        changed = 0
        for tid in ids:
            task = self._find_task(tid)
            if task and task["done"]:
                self.manager.set_done(tid, False)
                changed += 1
        self._refresh_keep_selection()
        self._status(f"已取消 {changed} 个任务的完成" if changed else "所选任务无需操作")

    def delete_task(self):
        ids = self._selected_ids()
        if not ids:
            messagebox.showwarning("提示", "请先在列表中选择要操作的任务。")
            return
        if messagebox.askyesno("确认删除",
                               f"确定删除选中的 {len(ids)} 个任务吗？此操作不可撤销！"):
            for tid in ids:
                self.manager.delete(tid)
            self._refresh_keep_selection()
            self._status(f"已删除 {len(ids)} 个任务")

    # ---------- 移动（始终作用于基础顺序） ----------
    def move_up(self):
        ids = self._selected_ids()
        if not ids:
            messagebox.showwarning("提示", "请先在列表中选择要操作的任务。")
            return
        if self.sort_var.get() == "time":
            self._move_time_view(ids, up=True)
            return
        base = [t["id"] for t in self.manager.todos]
        pos = {tid: i for i, tid in enumerate(base)}
        if any(pos[tid] == 0 for tid in ids):
            messagebox.showwarning("提示", "部分任务无法移动，操作取消")
            return
        self._batch_shift_base(base, set(ids), up=True)
        self.refresh()
        self._select_ids(set(ids))
        self._status(f"已上移 {len(ids)} 个任务（优先级）")

    def move_down(self):
        ids = self._selected_ids()
        if not ids:
            messagebox.showwarning("提示", "请先在列表中选择要操作的任务。")
            return
        if self.sort_var.get() == "time":
            self._move_time_view(ids, up=False)
            return
        base = [t["id"] for t in self.manager.todos]
        pos = {tid: i for i, tid in enumerate(base)}
        if any(pos[tid] == len(base) - 1 for tid in ids):
            messagebox.showwarning("提示", "部分任务无法移动，操作取消")
            return
        self._batch_shift_base(base, set(ids), up=False)
        self.refresh()
        self._select_ids(set(ids))
        self._status(f"已下移 {len(ids)} 个任务（优先级）")

    def _move_time_view(self, ids, up):
        """时间视图下：调整同一时间组内任务的 time_order（不改基础顺序）。"""
        ok, msg = self.manager.move_time_order(ids, up=up)
        if not ok:
            if msg:
                messagebox.showwarning("提示", msg)
            return
        self.refresh()
        self._select_ids(set(ids))
        self._status("已调整相同时间任务顺序")

    def _batch_shift_base(self, base_ids, sel_set, up):
        ids = list(base_ids)
        n = len(ids)
        if up:
            i = 0
            while i < n:
                if ids[i] in sel_set:
                    ids[i - 1], ids[i] = ids[i], ids[i - 1]
                i += 1
        else:
            i = n - 1
            while i >= 0:
                if ids[i] in sel_set:
                    ids[i], ids[i + 1] = ids[i + 1], ids[i]
                i -= 1
        self.manager.reorder(ids)

    # ---------- 双击编辑（仅描述） ----------
    def edit_task_at(self, event):
        iid = self.tree.identify_row(event.y)
        if not iid:
            return
        task = self._find_task(int(iid))
        if task is not None:
            self._make_edit_dialog(task)

    def _split_datetime(self, task):
        """把任务时间拆分为 年/月/日/时/分 字符串（无则空串）。"""
        y = mo = d = h = mi = ""
        parts = task.get("date", "").split("-")
        if len(parts) == 3:
            y, mo, d = parts
        parts = task.get("time", "").split(":")
        if len(parts) == 2:
            h, mi = parts
        return y, mo, d, h, mi

    def _make_edit_dialog(self, task):
        """双击编辑弹窗：描述 + 时间输入（复用同一套校验与存储规则）。"""
        y, mo, d, h, mi = self._split_datetime(task)

        dlg = tk.Toplevel(self.root)
        dlg.title("编辑任务")
        dlg.resizable(False, False)
        dlg.transient(self.root)
        dlg.grab_set()

        # 时间输入行（与主界面一致）
        tframe = tk.Frame(dlg)
        tframe.pack(fill="x", padx=10, pady=(10, 2))
        dlg.year_var = tk.StringVar(value=y)
        dlg.month_var = tk.StringVar(value=mo)
        dlg.day_var = tk.StringVar(value=d)
        dlg.hour_var = tk.StringVar(value=h)
        dlg.minute_var = tk.StringVar(value=mi)
        specs = ((2020, 2035, 4, dlg.year_var), (1, 12, 2, dlg.month_var),
                 (1, 31, 2, dlg.day_var), (0, 23, 2, dlg.hour_var),
                 (0, 59, 2, dlg.minute_var))
        for frm, to, w_, var in specs:
            sp = tk.Spinbox(tframe, from_=frm, to=to, width=w_,
                            justify="center", textvariable=var)
            sp.pack(side="left", padx=1)
            sp.bind("<FocusIn>", lambda e, t=sp: self._select_all_text(t))
        tk.Label(tframe, text="  时间").pack(side="left")

        # 描述
        tk.Label(dlg, text="任务描述").pack(anchor="w", padx=10)
        dlg.desc_var = tk.StringVar(value=task["description"])
        desc_entry = tk.Entry(dlg, textvariable=dlg.desc_var)
        desc_entry.pack(fill="x", padx=10)
        desc_entry.bind("<FocusIn>", lambda e: self._select_all_text(desc_entry))

        # 按钮
        btn = tk.Frame(dlg)
        btn.pack(fill="x", padx=10, pady=(8, 10))
        tk.Button(btn, text="保存", width=8,
                  command=lambda: self._edit_save(dlg, task)).pack(side="left")
        tk.Button(btn, text="取消", width=8, command=dlg.destroy).pack(side="right")
        return dlg

    def _edit_save(self, dlg, task):
        desc = dlg.desc_var.get().strip()
        if not desc:
            messagebox.showwarning("提示", "任务描述不能为空。")
            return
        date, time, err = self._validate_time(
            dlg.year_var.get(), dlg.month_var.get(), dlg.day_var.get(),
            dlg.hour_var.get(), dlg.minute_var.get())
        if err:
            messagebox.showwarning("提示", err)
            return
        self.manager.rename(task["id"], desc)
        self.manager.set_time(task["id"], date, time)
        self.refresh()
        self._select_ids({task["id"]})
        self._status("任务已更新")
        dlg.destroy()


def main():
    root = tk.Tk()
    TodoApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
