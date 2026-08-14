"""核心逻辑：TodoManager 管理任务的新增、删除、完成、取消与查询。"""
from storage import load_todos, save_todos


class TodoManager:
    """封装对待办任务的操作，并在每次变更后自动持久化。"""

    def __init__(self):
        self.todos = load_todos()
        self._next_id = max((t["id"] for t in self.todos), default=0) + 1
        self._migrate_time_order()

    def add(self, description, date="", time=""):
        """新建任务，返回创建的任务。date 形如 YYYY-MM-DD，time 形如 HH:MM，可空。"""
        task = {"id": self._next_id, "description": description,
                "done": False, "date": date, "time": time,
                "time_order": self._group_max_order(date, time) + 1}
        self.todos.append(task)
        self._next_id += 1
        save_todos(self.todos)
        return task

    def delete(self, task_id):
        """按 ID 删除任务，返回被删任务；不存在则返回 None。"""
        for task in self.todos:
            if task["id"] == task_id:
                self.todos.remove(task)
                save_todos(self.todos)
                return task
        return None

    def set_done(self, task_id, done):
        """设置任务完成状态，返回更新后的任务；不存在则返回 None。"""
        for task in self.todos:
            if task["id"] == task_id:
                task["done"] = done
                save_todos(self.todos)
                return task
        return None

    def rename(self, task_id, description):
        """更新任务描述，返回更新后的任务；不存在则返回 None。"""
        for task in self.todos:
            if task["id"] == task_id:
                task["description"] = description
                save_todos(self.todos)
                return task
        return None

    def set_time(self, task_id, date="", time=""):
        """更新任务时间；时间变化时将该任务 time_order 重设为新时间组末尾。"""
        for task in self.todos:
            if task["id"] == task_id:
                changed = (task.get("date", "") != date
                           or task.get("time", "") != time)
                task["date"] = date
                task["time"] = time
                if changed:
                    task["time_order"] = self._group_max_order(date, time,
                                                                exclude_id=task_id) + 1
                save_todos(self.todos)
                return task
        return None

    def swap(self, id_a, id_b):
        """交换两个任务的顺序（即优先级）。任一任务不存在则返回 False。"""
        ia = ib = None
        for i, task in enumerate(self.todos):
            if task["id"] == id_a:
                ia = i
            elif task["id"] == id_b:
                ib = i
        if ia is None or ib is None:
            return False
        self.todos[ia], self.todos[ib] = self.todos[ib], self.todos[ia]
        save_todos(self.todos)
        return True

    def reorder(self, ordered_ids):
        """按 ordered_ids 重排这些任务的顺序（优先级），其余任务保持相对位置；持久化。"""
        target = set(ordered_ids)
        by_id = {t["id"]: t for t in self.todos}
        result = []
        queue = list(ordered_ids)
        for task in self.todos:
            if task["id"] in target:
                result.append(by_id[queue.pop(0)])
            else:
                result.append(task)
        self.todos = result
        save_todos(self.todos)
        return True

    def list_todos(self, done_only=None):
        """返回任务列表。done_only 为 True/False 时按完成状态过滤，None 返回全部。"""
        if done_only is None:
            return list(self.todos)
        return [t for t in self.todos if t["done"] == done_only]

    # ---------- 时间排序辅助 ----------
    @staticmethod
    def _time_key(task):
        return (task.get("date", ""), task.get("time", ""))

    def _migrate_time_order(self):
        """为缺少 time_order 的旧任务按时间组分配初始顺序（0 递增），并持久化。"""
        if all("time_order" in t for t in self.todos):
            return
        counter = {}
        for task in self.todos:
            key = self._time_key(task)
            task["time_order"] = counter.get(key, 0)
            counter[key] = counter.get(key, 0) + 1
        save_todos(self.todos)

    def _group_max_order(self, date, time, exclude_id=None):
        """返回指定时间组内当前最大 time_order（组空返回 -1）。"""
        key = (date, time)
        return max((t.get("time_order", 0) for t in self.todos
                    if self._time_key(t) == key and t["id"] != exclude_id),
                   default=-1)

    def move_time_order(self, task_ids, up):
        """时间视图下：调整同一时间组内任务的 time_order（不改基础顺序）。

        返回 (ok, message)；ok 为 False 时 message 为禁止原因。
        """
        task_ids = set(task_ids)
        if not task_ids:
            return False, ""
        keys = {self._time_key(t) for t in self.todos if t["id"] in task_ids}
        if len(keys) > 1:
            return False, "仅允许调整相同时间任务的顺序"
        key = next(iter(keys))
        group = sorted((t for t in self.todos if self._time_key(t) == key),
                       key=lambda t: t.get("time_order", 0))
        seq = [t["id"] for t in group]
        n = len(seq)
        index = {tid: i for i, tid in enumerate(seq)}
        if up and any(index[tid] == 0 for tid in task_ids):
            return False, "无法移动：任务已在相同时间组的最上方"
        if not up and any(index[tid] == n - 1 for tid in task_ids):
            return False, "无法移动：任务已在相同时间组的最下方"
        by_id = {t["id"]: t for t in self.todos}
        if up:
            i = 0
            while i < n:
                if seq[i] in task_ids:
                    a, b = by_id[seq[i]], by_id[seq[i - 1]]
                    a["time_order"], b["time_order"] = b["time_order"], a["time_order"]
                i += 1
        else:
            i = n - 1
            while i >= 0:
                if seq[i] in task_ids:
                    a, b = by_id[seq[i]], by_id[seq[i + 1]]
                    a["time_order"], b["time_order"] = b["time_order"], a["time_order"]
                i -= 1
        save_todos(self.todos)
        return True, ""
