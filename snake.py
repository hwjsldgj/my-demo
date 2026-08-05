# -*- coding: utf-8 -*-
import pygame
import random
import sys
import os
import traceback
from datetime import datetime

# ==================== 常量定义 ====================
os.environ["SDL_VIDEODRIVER"] = "windib"  # Windows 7兼容性设置

WIDTH = 640
HEIGHT = 480
GRID_SIZE = 20
INIT_SPEED = 7
COLORS = {
    "BLACK": (0, 0, 0),
    "WHITE": (255, 255, 255),
    "RED": (255, 0, 0),
    "GREEN": (0, 255, 0),
    "YELLOW": (255, 255, 0)
}
FONT_NAME = 'simhei'

# ==================== 工具函数 ====================
def get_timestamp():
    """获取带毫秒的正确时间戳"""
    now = datetime.now()
    return now.strftime('%H:%M:%S') + f".{now.microsecond//1000:03d}"

def print_log(message, level="INFO"):
    """带时间戳的控制台日志输出"""
    timestamp = get_timestamp()
    print(f"[{timestamp}] [{level}] {message}")

# ==================== 游戏初始化 ====================
try:
    pygame.init()
    print_log(f"Pygame版本: {pygame.__version__}")
    
    # 创建日志目录
    log_dir = "game_logs"
    if not os.path.exists(log_dir):
        os.makedirs(log_dir)
    
    # 初始化显示模式
    screen = pygame.display.set_mode((WIDTH, HEIGHT), pygame.HWSURFACE | pygame.DOUBLEBUF)
    pygame.display.set_caption("贪吃蛇-最终版")
    print_log("游戏窗口创建成功")

    # 初始化字体和时钟
    font = pygame.font.SysFont(FONT_NAME, 24)
    clock = pygame.time.Clock()
    
except Exception as e:
    print_log(f"初始化失败: {str(e)}", "ERROR")
    print_log(traceback.format_exc(), "ERROR")
    input("按回车键退出...")
    sys.exit()

# ==================== 游戏核心类 ====================
class GameState:
    def __init__(self):
        self.reset()
        self.high_score = self.load_high_score()
        
    def reset(self):
        """重置游戏状态"""
        self.score = 0
        self.speed = INIT_SPEED
        self.paused = False
        self.start_time = datetime.now()
        self.running = True
        self.window_active = True

    def load_high_score(self):
        """加载历史最高分"""
        try:
            with open("highscore.txt", "r") as f:
                return int(f.read())
        except:
            return 0

    def save_high_score(self):
        """保存最高分"""
        with open("highscore.txt", "w") as f:
            f.write(str(self.high_score))
        
    def check_window_status(self):
        """检查窗口是否仍然存在"""
        try:
            pygame.display.get_surface()
            self.window_active = True
        except:
            self.window_active = False

class Snake:
    def __init__(self):
        self.reset()
        
    def reset(self):
        """重置蛇状态"""
        self.body = [(WIDTH//2, HEIGHT//2)]
        self.direction = (0, -1)  # 初始向上移动
        self.grow = False
        print_log("蛇对象已重置")

    def move(self):
        """移动蛇"""
        head_x, head_y = self.body[0]
        dx, dy = self.direction
        new_head = (
            (head_x + dx * GRID_SIZE) % WIDTH,
            (head_y + dy * GRID_SIZE) % HEIGHT
        )
        
        if new_head in self.body:
            raise Exception(f"撞到自身，位置：({new_head[0]}, {new_head[1]})")
            
        self.body.insert(0, new_head)
        if not self.grow:
            self.body.pop()
        else:
            self.grow = False

    def draw(self):
        """绘制蛇"""
        for segment in self.body:
            x, y = segment
            pygame.draw.rect(screen, COLORS["GREEN"], 
                           (x, y, GRID_SIZE-1, GRID_SIZE-1))

class Food:
    def __init__(self, snake_body):
        try:
            self.position = self.generate_position(snake_body)
            log_msg = f"食物生成成功：({self.position[0]}, {self.position[1]})"
            print_log(log_msg)
            self._write_log(log_msg)
        except Exception as e:
            error_msg = f"食物生成失败：{str(e)}"
            print_log(error_msg, "ERROR")
            self._write_log(error_msg, "ERROR")
            raise
            
    def draw(self):
        """绘制食物"""
        try:
            x, y = self.position
            pygame.draw.rect(screen, COLORS["RED"], 
                           (x, y, GRID_SIZE-1, GRID_SIZE-1))
        except Exception as e:
            print_log(f"食物绘制失败: {str(e)}", "ERROR")
            raise

    @staticmethod
    def generate_position(snake_body):
        """生成食物位置"""
        while True:
            x = random.randint(0, (WIDTH-GRID_SIZE)//GRID_SIZE) * GRID_SIZE
            y = random.randint(0, (HEIGHT-GRID_SIZE)//GRID_SIZE) * GRID_SIZE
            if (x, y) not in snake_body:
                return (x, y)
    
    def _write_log(self, message, level="INFO"):
        """写入操作日志"""
        try:
            with open("game_logs/game_events.log", "a", encoding="utf-8") as f:
                timestamp = get_timestamp()
                f.write(f"[{timestamp}] [{level}] {message}\n")
        except Exception as e:
            print_log(f"日志写入失败: {str(e)}", "ERROR")

# ==================== 游戏功能 ====================
def save_game_log(state, end_reason):
    """保存最终游戏日志"""
    try:
        duration = (datetime.now() - state.start_time).total_seconds()
        filename = f"game_logs/game_{state.start_time:%Y%m%d_%H%M%S}.log"
        
        # 构建日志内容
        log_content = (
            f"=== 游戏统计 ===\n"
            f"开始时间: {state.start_time:%Y-%m-%d %H:%M:%S.%f}\n"
            f"持续时间: {duration:.3f}秒\n"
            f"最终得分: {state.score}\n"
            f"最高得分: {state.high_score}\n"
            f"游戏速度: {state.speed:.1f}\n"
            f"结束原因: {end_reason}\n"
            f"=== 操作记录 ===\n"
        )
        
        # 合并事件日志
        try:
            with open("game_logs/game_events.log", "r", encoding="utf-8", errors="ignore") as f:
                log_content += f.read()
        except Exception as e:
            log_content += f"读取事件日志失败: {str(e)}\n"
            
        # 写入日志文件
        with open(filename, "w", encoding="utf-8") as f:
            f.write(log_content)
            
        print_log(f"完整日志已保存: {filename}")
    except Exception as e:
        print_log(f"日志保存失败: {str(e)}", "ERROR")

def draw_text(text, position, color=COLORS["WHITE"]):
    """绘制文本到屏幕"""
    try:
        text_surface = font.render(text, True, color)
        screen.blit(text_surface, position)
    except Exception as e:
        print_log(f"文本渲染失败: {str(e)}", "ERROR")

# ==================== 游戏主循环 ====================
def main():
    state = GameState()
    try:
        snake = Snake()
        food = Food(snake.body)
        
        # 显式检查关键方法是否存在
        if not hasattr(food, 'draw'):
            raise AttributeError("Food类缺少draw方法")

        exception_occurred = False
        exception_info = None
        
        while state.running:
            # 处理系统事件
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    state.running = False
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_SPACE:
                        state.paused = not state.paused
                        log_msg = f"游戏暂停状态：{state.paused}"
                        print_log(log_msg)
                        with open("game_logs/game_events.log", "a") as f:
                            f.write(f"[{get_timestamp()}] [INFO] {log_msg}\n")
                    elif event.key == pygame.K_ESCAPE:
                        state.running = False
                    elif not state.paused:
                        # 处理方向键输入
                        key_mapping = {
                            pygame.K_UP: (0, -1),
                            pygame.K_DOWN: (0, 1),
                            pygame.K_LEFT: (-1, 0),
                            pygame.K_RIGHT: (1, 0)
                        }
                        if event.key in key_mapping:
                            new_dir = key_mapping[event.key]
                            if (new_dir[0] != -snake.direction[0] or 
                                new_dir[1] != -snake.direction[1]):
                                snake.direction = new_dir
                                log_msg = f"方向变更：({new_dir[0]}, {new_dir[1]})"
                                print_log(log_msg)
                                with open("game_logs/game_events.log", "a") as f:
                                    f.write(f"[{get_timestamp()}] [INFO] {log_msg}\n")

            # 游戏逻辑更新
            if not state.paused and state.running:
                try:
                    snake.move()
                    
                    # 检测食物碰撞
                    if snake.body[0] == food.position:
                        snake.grow = True
                        food = Food(snake.body)
                        state.score += 1
                        state.speed += 0.3
                        if state.score > state.high_score:
                            state.high_score = state.score
                            state.save_high_score()
                        log_msg = f"得分更新：{state.score} 速度：{state.speed:.1f} 最高分：{state.high_score}"
                        print_log(log_msg)
                        with open("game_logs/game_events.log", "a") as f:
                            f.write(f"[{get_timestamp()}] [INFO] {log_msg}\n")
                        
                except Exception as e:
                    save_game_log(state, f"异常退出: {str(e)}")
                    raise

            # 画面渲染
            try:
                screen.fill(COLORS["BLACK"])
                snake.draw()
                food.draw()
                
                # 显示游戏信息
                draw_text(f"分数: {state.score}", (10, 10))
                draw_text(f"最高分: {state.high_score}", (10, 40))
                draw_text(f"速度: {state.speed:.1f}", (10, 70))
                
                if state.paused:
                    draw_text("暂停中 - 按空格继续", 
                            (WIDTH//2-100, HEIGHT//2), COLORS["YELLOW"])
                
                pygame.display.flip()
                
            except pygame.error as e:
                print_log(f"显示刷新失败: {str(e)}", "WARNING")
                state.running = False

            clock.tick(state.speed)
            
    except Exception as e:
        exception_occurred = True
        exception_info = e
        print_log(f"游戏异常: {str(e)}", "ERROR")
        print_log(traceback.format_exc(), "ERROR")
        save_game_log(state, f"异常退出: {str(e)}")
    finally:
        pygame.quit()
        # 清理临时日志
        try:
            os.remove("game_logs/game_events.log")
        except:
            pass
        # 保存最终日志
        if exception_occurred:
            print_log(f"异常退出，原因：{exception_info}", "ERROR")
        else:
            save_game_log(state, "玩家主动退出")
        input("按回车键退出...")

if __name__ == "__main__":
    main()