using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace DesktopPet
{
    // ============================================================
    // 1. 3D 数学基础
    // ============================================================

    /// <summary>
    /// 用于通知外部调用方滑块值已变化。
    /// </summary>
    /// <param name="newSize">新的缩放值（滑块数值）</param>
    public delegate void SizeChangingHandler(int newSize);

    /// <summary>
    /// 三维向量结构体，提供基本向量运算。
    /// </summary>
    public struct Vector3
    {
        public float X, Y, Z;

        public Vector3(float x, float y, float z)
        {
            X = x; Y = y; Z = z;
        }

        // ---- 运算符重载 ----
        public static Vector3 operator +(Vector3 a, Vector3 b)
        {
            return new Vector3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        }
        public static Vector3 operator -(Vector3 a, Vector3 b)
        {
            return new Vector3(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        }
        public static Vector3 operator *(Vector3 a, float k)
        {
            return new Vector3(a.X * k, a.Y * k, a.Z * k);
        }
        public static Vector3 operator *(float k, Vector3 a)
        {
            return new Vector3(a.X * k, a.Y * k, a.Z * k);
        }

        /// <summary>向量的模长</summary>
        public float Length()
        {
            return (float)Math.Sqrt(X * X + Y * Y + Z * Z);
        }

        /// <summary>向量归一化</summary>
        public void Normalize()
        {
            float l = Length();
            if (l > 0) { X /= l; Y /= l; Z /= l; }
        }

        /// <summary>三维叉积</summary>
        public static Vector3 Cross(Vector3 a, Vector3 b)
        {
            return new Vector3(
                a.Y * b.Z - a.Z * b.Y,
                a.Z * b.X - a.X * b.Z,
                a.X * b.Y - a.Y * b.X
            );
        }

        /// <summary>三维点积</summary>
        public static float Dot(Vector3 a, Vector3 b)
        {
            return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
        }
    }

    /// <summary>
    /// 3D 变换矩阵，支持平移、绕 X/Y 轴旋转。
    /// </summary>
    public class Transform3D
    {
        private float[,] m = new float[4, 4];

        public Transform3D() { Identity(); }

        /// <summary>重置为单位矩阵</summary>
        public void Identity()
        {
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++)
                    m[i, j] = (i == j) ? 1f : 0f;
        }

        /// <summary>绕 Y 轴旋转</summary>
        /// <param name="angle">弧度</param>
        public void RotateY(float angle)
        {
            float cos = (float)Math.Cos(angle), sin = (float)Math.Sin(angle);
            Transform3D t = new Transform3D();
            t.m[0, 0] = cos; t.m[0, 2] = sin;
            t.m[2, 0] = -sin; t.m[2, 2] = cos;
            Multiply(t);
        }

        /// <summary>绕 X 轴旋转</summary>
        /// <param name="angle">弧度</param>
        public void RotateX(float angle)
        {
            float cos = (float)Math.Cos(angle), sin = (float)Math.Sin(angle);
            Transform3D t = new Transform3D();
            t.m[1, 1] = cos; t.m[1, 2] = -sin;
            t.m[2, 1] = sin; t.m[2, 2] = cos;
            Multiply(t);
        }

        /// <summary>平移</summary>
        public void Translate(float x, float y, float z)
        {
            Transform3D t = new Transform3D();
            t.m[0, 3] = x; t.m[1, 3] = y; t.m[2, 3] = z;
            Multiply(t);
        }

        /// <summary>矩阵相乘</summary>
        private void Multiply(Transform3D t)
        {
            float[,] res = new float[4, 4];
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++)
                    for (int k = 0; k < 4; k++)
                        res[i, j] += m[i, k] * t.m[k, j];
            m = res;
        }

        /// <summary>将向量从局部坐标变换到世界坐标（或反之，取决于矩阵）</summary>
        public Vector3 Apply(Vector3 v)
        {
            float x = m[0, 0] * v.X + m[0, 1] * v.Y + m[0, 2] * v.Z + m[0, 3];
            float y = m[1, 0] * v.X + m[1, 1] * v.Y + m[1, 2] * v.Z + m[1, 3];
            float z = m[2, 0] * v.X + m[2, 1] * v.Y + m[2, 2] * v.Z + m[2, 3];
            return new Vector3(x, y, z);
        }
    }

    // ============================================================
    // 2. 主窗体（桌宠）
    // ============================================================
    public class PetForm : Form
    {
        // ---- 窗口定位边距 ----
        private const int RIGHT_MARGIN = 320 + 20;  // 右侧留出控制面板宽度 + 间距
        private const int BOTTOM_MARGIN = 60;       // 底部边距

        // ---- 窗口消息常量（用于拦截系统消息，防止被最小化） ----
        private const int WM_WINDOWPOSCHANGING = 0x0046;
        private const int WM_SIZE = 0x0005;
        private const int WM_ACTIVATE = 0x0006;
        private const int SIZE_MINIMIZED = 0x0001;
        private const int SWP_HIDEWINDOW = 0x0080;

        // ---- 风扇状态变量 ----
        private float fanAngle = 0f;          // 当前叶片旋转角度（度）
        private int speedLevel = 0;           // 转速档位：0=关, 1=低, 5=中, 10=高
        private bool isSwing = false;         // 是否开启摇头
        private float swingAngle = 0f;        // 当前摇头角度（度）
        private float swingTime = 0f;         // 摇头动画时间累积

        private Timer animationTimer;          // 动画定时器
        private const int ANIMATION_INTERVAL = 8; // 每帧间隔（毫秒），约 125 FPS

        // ---- 窗口尺寸控制 ----
        private const int BASE_WIDTH = 150;    // 初始宽度
        private const int BASE_HEIGHT = 200;   // 初始高度
        private float scaleFactor = 1.0f;      // 当前缩放倍数（1.0 为原始大小）

        // ---- 鼠标拖动 ----
        private bool isDragging = false;
        private Point dragStartPoint = Point.Empty;
        private ContextMenuStrip contextMenu;

        // ---- 3D 模型参数（单位：模型空间） ----
        private const float FAN_RADIUS = 0.9f;          // 风扇圆盘半径
        private const float FAN_THICKNESS = 0.15f;      // 圆盘厚度
        private const float BLADE_LENGTH = 0.75f;       // 叶片长度
        private const float BLADE_WIDTH = 0.6f;         // 叶片最大宽度
        private const float PILLAR_HEIGHT = 1.6f;       // 立柱高度
        private const float PILLAR_RADIUS = 0.1f;       // 立柱半径
        private const float BASE_SIZE = 0.6f;           // 底座边长
        private const float BASE_BOX_HEIGHT = 0.12f;    // 底座厚度
        private const float VIEW_DIST = 4.0f;           // 透视投影的视距
        private const int SEG = 24;                     // 圆盘细分段数
        private const int PILLAR_SEG = 16;              // 立柱圆周细分

        // ---- 转速平滑过渡 ----
        private float currentRevPerSec = 0f;   // 当前实际转速（圈/秒）
        private float targetRevPerSec = 0f;    // 目标转速（圈/秒）

        // ============================================================
        // 构造函数
        // ============================================================
        public PetForm()
        {
            // 设置透明背景（使用极罕见的颜色作为透明键）
            Color rareColor = Color.FromArgb(1, 2, 3);
            this.BackColor = rareColor;
            this.TransparencyKey = rareColor;

            // 无边框、不在任务栏显示、不置顶
            this.FormBorderStyle = FormBorderStyle.None;
            this.ShowInTaskbar = false;
            this.TopMost = false;

            // 设置初始窗口大小
            this.Size = new Size(BASE_WIDTH, BASE_HEIGHT);
            // 启用双缓冲以减少闪烁
            this.SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.DoubleBuffer, true);

            // 窗口加载完成后精确定位（确保尺寸已就绪）
            this.Load += new EventHandler(PetForm_Load);

            // 保留旧方法以防万一，但不再使用
            SetInitialPosition();

            // 创建右键菜单
            CreateContextMenu();

            // 绑定绘制事件
            this.Paint += PetForm_Paint;

            // 启动动画定时器
            animationTimer = new Timer();
            animationTimer.Interval = ANIMATION_INTERVAL;
            animationTimer.Tick += new EventHandler(AnimationTimer_Tick);
            animationTimer.Start();

            // 防最小化备用定时器（每 500ms 检查一次）
            Timer checkTimer = new Timer();
            checkTimer.Interval = 500;
            checkTimer.Tick += new EventHandler(CheckTimer_Tick);
            checkTimer.Start();
        }

        /// <summary>
        /// 旧版初始位置（已弃用，但保留以兼容）。
        /// 实际位置由 PetForm_Load 精确定位。
        /// </summary>
        private void SetInitialPosition()
        {
            Rectangle screen = Screen.PrimaryScreen.WorkingArea;
            int x = screen.Right - this.Width - 300;
            int y = screen.Bottom - this.Height - 30;
            this.Location = new Point(x, y);
        }

        /// <summary>
        /// 窗口加载完成后定位到屏幕右下角，保留指定边距。
        /// </summary>
        private void PetForm_Load(object sender, EventArgs e)
        {
            Rectangle screen = Screen.PrimaryScreen.WorkingArea;
            int x = screen.Right - this.Width - RIGHT_MARGIN;
            int y = screen.Bottom - this.Height - BOTTOM_MARGIN;
            // 防止超出屏幕左/上边缘
            if (x < screen.Left + 10) x = screen.Left + 10;
            if (y < screen.Top + 10) y = screen.Top + 10;
            this.Location = new Point(x, y);
        }

        /// <summary>
        /// 防最小化定时器：如果窗口被意外最小化，立即恢复。
        /// </summary>
        private void CheckTimer_Tick(object sender, EventArgs e)
        {
            if (this.WindowState == FormWindowState.Minimized)
            {
                this.WindowState = FormWindowState.Normal;
                this.Show();
            }
        }

        /// <summary>
        /// 动画定时器：每帧更新转速、摇头角度，并触发重绘。
        /// 转速使用平滑逼近算法，避免突变。
        /// </summary>
        private void AnimationTimer_Tick(object sender, EventArgs e)
        {
            // ---- 平滑逼近目标转速 ----
            float acceleration = 2.5f; // 加速度（圈/秒²）
            float delta = acceleration * (ANIMATION_INTERVAL / 1000f);

            if (currentRevPerSec < targetRevPerSec)
            {
                currentRevPerSec += delta;
                if (currentRevPerSec > targetRevPerSec)
                    currentRevPerSec = targetRevPerSec;
            }
            else if (currentRevPerSec > targetRevPerSec)
            {
                currentRevPerSec -= delta;
                if (currentRevPerSec < targetRevPerSec)
                    currentRevPerSec = targetRevPerSec;
            }

            // 使用当前实际转速计算每帧角度增量
            if (currentRevPerSec > 0)
            {
                float degreesPerFrame = currentRevPerSec * 360f * (ANIMATION_INTERVAL / 1000f);
                fanAngle += degreesPerFrame;
                if (fanAngle > 360) fanAngle -= 360; // 保持在 0~360 范围
            }

            // ---- 摇头更新（正弦摆动） ----
            if (isSwing)
            {
                swingTime += 0.015f;          // 摆动速度（较慢）
                swingAngle = (float)(Math.Sin(swingTime) * 35); // ±35°
            }

            // ---- 触发窗口重绘 ----
            this.Invalidate();
        }

        /// <summary>
        /// 创建右键菜单。
        /// </summary>
        private void CreateContextMenu()
        {
            contextMenu = new ContextMenuStrip();

            // “调整大小”菜单
            ToolStripMenuItem resizeItem = new ToolStripMenuItem("调整大小");
            resizeItem.Click += new EventHandler(ResizeItem_Click);
            contextMenu.Items.Add(resizeItem);

            // “控制面板”菜单
            ToolStripMenuItem controlItem = new ToolStripMenuItem("控制面板");
            controlItem.Click += new EventHandler(ControlItem_Click);
            contextMenu.Items.Add(controlItem);

            contextMenu.Items.Add(new ToolStripSeparator());

            // “退出”菜单
            ToolStripMenuItem exitItem = new ToolStripMenuItem("退出");
            exitItem.Click += new EventHandler(ExitItem_Click);
            contextMenu.Items.Add(exitItem);

            this.ContextMenuStrip = contextMenu;
        }

        // ============================================================
        // 绘制主流程
        // ============================================================
        private void PetForm_Paint(object sender, PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;

            int w = this.Width;
            int h = this.Height;

            // 计算模型高度（底座底部到风扇头顶部）
            float maxHeight = BASE_BOX_HEIGHT / 2 + PILLAR_HEIGHT + FAN_RADIUS;
            // 根据窗口较小边和模型尺寸计算缩放比例，使模型完整显示
            float scale = Math.Min(w, h) / (0.85f * Math.Max(FAN_RADIUS, maxHeight));
            scale *= 0.9f; // 额外缩小一点，留出边距

            // 屏幕中心
            float cx = w / 2f;
            // 计算模型包围盒中心（Y 方向），使模型居中
            float modelCenterY = (PILLAR_HEIGHT + FAN_RADIUS + BASE_BOX_HEIGHT) / 2.0f;
            float cy = h / 2f + modelCenterY * scale * 0.82f;

            // 光源方向（从左上前方）
            Vector3 lightDir = new Vector3(0.4f, 0.7f, 0.3f);
            lightDir.Normalize();

            // ---- 绘制阴影（渐变椭圆） ----
            float shadowW = 1.6f * scale;
            float shadowH = 0.6f * scale;
            using (GraphicsPath shadowPath = new GraphicsPath())
            {
                shadowPath.AddEllipse(cx - shadowW / 2, cy + 0.1f * scale - shadowH / 2, shadowW, shadowH);
                using (PathGradientBrush shadowBrush = new PathGradientBrush(shadowPath))
                {
                    shadowBrush.CenterColor = Color.FromArgb(80, 0, 0, 0);
                    shadowBrush.SurroundColors = new Color[] { Color.FromArgb(0, 0, 0, 0) };
                    shadowBrush.FocusScales = new PointF(0.5f, 0.5f);
                    g.FillPath(shadowBrush, shadowPath);
                }
            }

            // ---- 创建可重用画笔 ----
            using (Pen darkPen = new Pen(Color.DarkGray, 1))
            using (Pen grayPen = new Pen(Color.Gray, 1))
            using (SolidBrush brushLight = new SolidBrush(Color.FromArgb(180, 180, 180)))
            using (SolidBrush brushDark = new SolidBrush(Color.FromArgb(120, 120, 120)))
            {
                DrawBase(g, cx, cy, scale, lightDir, brushLight, brushDark, darkPen);
                DrawPillar(g, cx, cy, scale, lightDir, brushLight, brushDark, darkPen);
                DrawFanHead(g, cx, cy, scale, lightDir, brushLight, brushDark, darkPen, grayPen);
            }
        }

        // ---- 绘制底座（长方体） ----
        private void DrawBase(Graphics g, float cx, float cy, float scale, Vector3 light,
            SolidBrush brushLight, SolidBrush brushDark, Pen darkPen)
        {
            Transform3D world = new Transform3D();
            float hs = BASE_SIZE / 2;
            float hh = BASE_BOX_HEIGHT / 2;
            // 8 个顶点（前面 4 个，后面 4 个）
            Vector3[] verts = new Vector3[]
            {
                new Vector3(-hs, -hh, -hs), new Vector3( hs, -hh, -hs),
                new Vector3( hs, -hh,  hs), new Vector3(-hs, -hh,  hs),
                new Vector3(-hs,  hh, -hs), new Vector3( hs,  hh, -hs),
                new Vector3( hs,  hh,  hs), new Vector3(-hs,  hh,  hs)
            };
            // 6 个面（每个面 4 个顶点索引）
            int[][] quads = new int[][]
            {
                new int[]{0,1,2,3}, // 底面
                new int[]{4,5,6,7}, // 顶面
                new int[]{0,1,5,4}, // 前面
                new int[]{1,2,6,5}, // 右面
                new int[]{2,3,7,6}, // 后面
                new int[]{3,0,4,7}  // 左面
            };
            foreach (var quad in quads)
            {
                PointF[] pts = new PointF[4];
                for (int i = 0; i < 4; i++)
                    pts[i] = Project(world.Apply(verts[quad[i]]), cx, cy, scale);
                g.FillPolygon(brushLight, pts);
                g.DrawPolygon(darkPen, pts);
            }
        }

        // ---- 绘制立柱（圆柱近似为八棱柱） ----
        private void DrawPillar(Graphics g, float cx, float cy, float scale, Vector3 light,
            SolidBrush brushLight, SolidBrush brushDark, Pen darkPen)
        {
            Transform3D world = new Transform3D();
            int seg = PILLAR_SEG;
            float r = PILLAR_RADIUS;
            float topY = BASE_BOX_HEIGHT / 2 + PILLAR_HEIGHT;
            float botY = BASE_BOX_HEIGHT / 2;
            Vector3[] topPts = new Vector3[seg];
            Vector3[] botPts = new Vector3[seg];
            for (int i = 0; i < seg; i++)
            {
                float a = i * 2 * (float)Math.PI / seg;
                float x = r * (float)Math.Cos(a);
                float z = r * (float)Math.Sin(a);
                topPts[i] = new Vector3(x, topY, z);
                botPts[i] = new Vector3(x, botY, z);
            }
            // 绘制侧面（四边形条带）
            for (int i = 0; i < seg; i++)
            {
                int j = (i + 1) % seg;
                Vector3[] quad = new Vector3[] { botPts[i], botPts[j], topPts[j], topPts[i] };
                PointF[] pts = new PointF[4];
                for (int k = 0; k < 4; k++)
                    pts[k] = Project(world.Apply(quad[k]), cx, cy, scale);
                g.FillPolygon(brushLight, pts);
                g.DrawPolygon(darkPen, pts);
            }
        }

        /// <summary>
        /// 绘制风扇头（圆盘 + 动态模糊叶片 + 中心装饰圆）。
        /// 叶片模糊强度随当前转速变化。
        /// </summary>
        private void DrawFanHead(Graphics g, float cx, float cy, float scale, Vector3 light,
            SolidBrush brushLight, SolidBrush brushDark, Pen darkPen, Pen grayPen)
        {
            // 头部变换（包含摇头旋转）
            Transform3D head = new Transform3D();
            head.Translate(0, BASE_BOX_HEIGHT / 2 + PILLAR_HEIGHT, 0);
            float swingRad = swingAngle * (float)Math.PI / 180f;
            head.RotateY(swingRad);

            int seg = SEG;
            float r = FAN_RADIUS;
            float th = FAN_THICKNESS / 2;

            // ---- 风扇圆盘（正面、背面、侧边） ----
            Vector3[] frontPts = new Vector3[seg];
            Vector3[] backPts = new Vector3[seg];
            for (int i = 0; i < seg; i++)
            {
                float a = i * 2 * (float)Math.PI / seg;
                float x = r * (float)Math.Cos(a);
                float y = r * (float)Math.Sin(a);
                frontPts[i] = new Vector3(x, y, th);
                backPts[i] = new Vector3(x, y, -th);
            }

            // 正面
            PointF[] screenFront = new PointF[seg];
            for (int i = 0; i < seg; i++)
                screenFront[i] = Project(head.Apply(frontPts[i]), cx, cy, scale);
            using (SolidBrush frontBrush = new SolidBrush(Color.FromArgb(230, 230, 230)))
                g.FillPolygon(frontBrush, screenFront);
            g.DrawPolygon(darkPen, screenFront);

            // 背面
            PointF[] screenBack = new PointF[seg];
            for (int i = 0; i < seg; i++)
                screenBack[i] = Project(head.Apply(backPts[i]), cx, cy, scale);
            using (SolidBrush backBrush = new SolidBrush(Color.FromArgb(150, 150, 150)))
                g.FillPolygon(backBrush, screenBack);
            g.DrawPolygon(darkPen, screenBack);

            // 侧边
            for (int i = 0; i < seg; i++)
            {
                int j = (i + 1) % seg;
                Vector3[] side = new Vector3[] { backPts[i], backPts[j], frontPts[j], frontPts[i] };
                PointF[] sidePts = new PointF[4];
                for (int k = 0; k < 4; k++)
                    sidePts[k] = Project(head.Apply(side[k]), cx, cy, scale);
                g.FillPolygon(brushDark, sidePts);
                g.DrawPolygon(darkPen, sidePts);
            }

            // ---- 叶片（连续运动模糊） ----
            // 直接使用当前实际转速（已在 AnimationTimer_Tick 中平滑变化）
            float revPerSec = currentRevPerSec;
            float bladeRad = fanAngle * (float)Math.PI / 180f;
            // 运动模糊范围：当前帧旋转角度 * 系数（1.2 倍）
            float motionAngle = revPerSec * 2 * (float)Math.PI * (ANIMATION_INTERVAL / 1000f) * 1.2f;
            bool enableBlur = (revPerSec > 0.5f);

            using (SolidBrush bladeBrush = new SolidBrush(Color.FromArgb(200, 160, 200, 255)))
            {
                for (int i = 0; i < 3; i++) // 三个叶片
                {
                    float baseAngle = i * 2 * (float)Math.PI / 3 + bladeRad;
                    float l = BLADE_LENGTH;
                    float w = BLADE_WIDTH;

                    // 生成叶片轮廓（12个点）
                    int pointCount = 12;
                    Vector3[] basePts = new Vector3[pointCount];
                    float rootWidth = 0.5f;
                    float tipWidth = 0.35f;

                    for (int p = 0; p < pointCount; p++)
                    {
                        float t;
                        float widthFactor;
                        if (p < pointCount / 2)
                        {
                            t = p / (float)(pointCount / 2 - 1);
                            widthFactor = rootWidth - (rootWidth - tipWidth) * t;
                        }
                        else
                        {
                            int idx = pointCount - p;
                            t = idx / (float)(pointCount / 2 - 1);
                            widthFactor = rootWidth - (rootWidth - tipWidth) * t;
                        }
                        float halfW = widthFactor * w / 2;
                        float len = t * l;
                        float angle = baseAngle;
                        float dx = len * (float)Math.Cos(angle);
                        float dy = len * (float)Math.Sin(angle);
                        float perpX = -(float)Math.Sin(angle);
                        float perpY = (float)Math.Cos(angle);
                        float sign = (p < pointCount / 2) ? 1f : -1f;
                        basePts[p] = new Vector3(dx + perpX * halfW * sign, dy + perpY * halfW * sign, 0);
                    }

                    // 如果没有模糊，直接绘制一个叶片
                    if (!enableBlur)
                    {
                        PointF[] screenPts = new PointF[pointCount];
                        for (int p = 0; p < pointCount; p++)
                            screenPts[p] = Project(head.Apply(basePts[p]), cx, cy, scale);
                        g.FillPolygon(bladeBrush, screenPts);
                        using (Pen contourPen = new Pen(Color.FromArgb(100, 120, 180), 0.5f))
                            g.DrawPolygon(contourPen, screenPts);
                        continue;
                    }

                    // ---- 有模糊：在 motionAngle 范围内生成多个副本 ----
                    int blurSteps = 10; // 副本数
                    for (int step = 0; step < blurSteps; step++)
                    {
                        float tBlur = step / (float)(blurSteps - 1);
                        float angleOffset = (tBlur - 0.5f) * motionAngle;
                        float currentAngle = baseAngle + angleOffset;
                        // 透明度：边缘低，中心高
                        float alphaFactor = 1.0f - (float)Math.Pow(Math.Abs(tBlur - 0.5f) * 2, 1.5f);
                        int alpha = (int)(60 + alphaFactor * 160);
                        if (alpha < 0) alpha = 0;
                        if (alpha > 255) alpha = 255;

                        Vector3[] blurredPts = new Vector3[pointCount];
                        for (int p = 0; p < pointCount; p++)
                        {
                            // 将叶片点绕原点旋转 angleOffset
                            float len = (float)Math.Sqrt(basePts[p].X * basePts[p].X + basePts[p].Y * basePts[p].Y);
                            float ang = (float)Math.Atan2(basePts[p].Y, basePts[p].X);
                            float newAng = ang + angleOffset;
                            blurredPts[p] = new Vector3(
                                len * (float)Math.Cos(newAng),
                                len * (float)Math.Sin(newAng),
                                basePts[p].Z
                            );
                        }

                        PointF[] screenPts = new PointF[pointCount];
                        for (int p = 0; p < pointCount; p++)
                            screenPts[p] = Project(head.Apply(blurredPts[p]), cx, cy, scale);

                        using (SolidBrush blurBrush = new SolidBrush(Color.FromArgb(alpha, 160, 200, 255)))
                        {
                            g.FillPolygon(blurBrush, screenPts);
                        }
                    }

                    // 绘制一个中心实心叶片（最亮，作为核心）
                    PointF[] centerPts = new PointF[pointCount];
                    for (int p = 0; p < pointCount; p++)
                        centerPts[p] = Project(head.Apply(basePts[p]), cx, cy, scale);
                    using (SolidBrush centerBrush = new SolidBrush(Color.FromArgb(225, 160, 200, 255)))
                    {
                        g.FillPolygon(centerBrush, centerPts);
                    }
                    using (Pen contourPen = new Pen(Color.FromArgb(100, 120, 180), 0.5f))
                        g.DrawPolygon(contourPen, centerPts);
                }
            }

            // ---- 中心装饰圆（3D空间，随头旋转，显示为圆盘） ----
            float axisR = 0.12f;
            int axisSeg = 16;
            Vector3[] axisPts = new Vector3[axisSeg];
            for (int i = 0; i < axisSeg; i++)
            {
                float a = i * 2 * (float)Math.PI / axisSeg;
                float x = axisR * (float)Math.Cos(a);
                float y = axisR * (float)Math.Sin(a);
                axisPts[i] = new Vector3(x, y, 0);
            }

            PointF[] screenAxis = new PointF[axisSeg];
            for (int i = 0; i < axisSeg; i++)
                screenAxis[i] = Project(head.Apply(axisPts[i]), cx, cy, scale);

            using (GraphicsPath path = new GraphicsPath())
            {
                path.AddPolygon(screenAxis);
                using (PathGradientBrush brush = new PathGradientBrush(path))
                {
                    brush.CenterColor = Color.FromArgb(160, 160, 230);
                    brush.SurroundColors = new Color[] { Color.FromArgb(160, 160, 170) };
                    g.FillPath(brush, path);
                }
                g.DrawPolygon(darkPen, screenAxis);
            }

            // 高光（白色小点）
            float hlR = 0.04f;
            Vector3[] hlPts = new Vector3[8];
            for (int i = 0; i < 8; i++)
            {
                float a = i * 2 * (float)Math.PI / 8;
                float x = hlR * (float)Math.Cos(a);
                float y = hlR * (float)Math.Sin(a);
                hlPts[i] = new Vector3(x, y, 0);
            }
            PointF[] screenHl = new PointF[8];
            for (int i = 0; i < 8; i++)
                screenHl[i] = Project(head.Apply(hlPts[i]), cx, cy, scale);
            using (SolidBrush hlBrush = new SolidBrush(Color.FromArgb(255, 255, 255)))
            {
                g.FillPolygon(hlBrush, screenHl);
            }
        }

        /// <summary>
        /// 绘制单个叶片（保留，但当前未使用）。
        /// </summary>
        private void DrawSingleBlade(Graphics g, Transform3D head, float cx, float cy, float scale,
            float baseAngle, int alpha)
        {
            float l = BLADE_LENGTH;
            float w = BLADE_WIDTH;
            int pointCount = 12;
            Vector3[] pts = new Vector3[pointCount];
            float rootWidth = 0.5f;
            float tipWidth = 0.35f;

            for (int p = 0; p < pointCount; p++)
            {
                float t;
                float widthFactor;
                if (p < pointCount / 2)
                {
                    t = p / (float)(pointCount / 2 - 1);
                    widthFactor = rootWidth - (rootWidth - tipWidth) * t;
                }
                else
                {
                    int idx = pointCount - p;
                    t = idx / (float)(pointCount / 2 - 1);
                    widthFactor = rootWidth - (rootWidth - tipWidth) * t;
                }
                float halfW = widthFactor * w / 2;
                float len = t * l;
                float angle = baseAngle;
                float dx = len * (float)Math.Cos(angle);
                float dy = len * (float)Math.Sin(angle);
                float perpX = -(float)Math.Sin(angle);
                float perpY = (float)Math.Cos(angle);
                float sign = (p < pointCount / 2) ? 1f : -1f;
                pts[p] = new Vector3(dx + perpX * halfW * sign, dy + perpY * halfW * sign, 0);
            }

            PointF[] screenPts = new PointF[pointCount];
            for (int p = 0; p < pointCount; p++)
                screenPts[p] = Project(head.Apply(pts[p]), cx, cy, scale);

            using (SolidBrush brush = new SolidBrush(Color.FromArgb(alpha, 160, 200, 255)))
            {
                g.FillPolygon(brush, screenPts);
            }
            if (alpha == 255)
            {
                using (Pen contour = new Pen(Color.FromArgb(100, 120, 180), 0.5f))
                    g.DrawPolygon(contour, screenPts);
            }
        }

        // ============================================================
        // 透视投影（将 3D 点投影到 2D 屏幕）
        // ============================================================
        private PointF Project(Vector3 v, float cx, float cy, float scale)
        {
            float d = VIEW_DIST;
            float inv = 1f / (1 + v.Z / d);
            float x = v.X * scale * inv + cx;
            float y = -v.Y * scale * inv + cy; // 屏幕 Y 向上，而 3D Y 向下
            return new PointF(x, y);
        }

        // ============================================================
        // 鼠标事件（拖动窗口）
        // ============================================================
        protected override void OnMouseDown(MouseEventArgs e)
        {
            base.OnMouseDown(e);
            if (e.Button == MouseButtons.Left)
            {
                isDragging = true;
                dragStartPoint = new Point(e.X, e.Y);
            }
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            base.OnMouseMove(e);
            if (isDragging && e.Button == MouseButtons.Left)
            {
                int newX = this.Location.X + e.X - dragStartPoint.X;
                int newY = this.Location.Y + e.Y - dragStartPoint.Y;
                Rectangle screen = Screen.PrimaryScreen.WorkingArea;
                int w = this.Width, h = this.Height;
                // 限制窗口完全在屏幕内
                int minX = screen.Left;
                int maxX = screen.Right - w;
                int minY = screen.Top;
                int maxY = screen.Bottom - h;
                if (newX < minX) newX = minX;
                if (newX > maxX) newX = maxX;
                if (newY < minY) newY = minY;
                if (newY > maxY) newY = maxY;
                this.Location = new Point(newX, newY);
            }
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            base.OnMouseUp(e);
            if (e.Button == MouseButtons.Left)
                isDragging = false;
        }

        // ============================================================
        // 菜单事件
        // ============================================================

        /// <summary>
        /// “调整大小”菜单：弹出滑块窗口，拖动滑块实时改变窗口大小。
        /// 窗口大小变化时保持右下角位置不变。
        /// </summary>
        private void ResizeItem_Click(object sender, EventArgs e)
        {
            int currentSliderValue = (int)(scaleFactor * 100f);
            using (ResizeForm resizeForm = new ResizeForm(currentSliderValue))
            {
                resizeForm.Owner = this;
                resizeForm.SizeChanging += new SizeChangingHandler(OnSizeChanging);
                resizeForm.ShowDialog(); // 模态，但无确认/取消按钮，关闭即完成调整
            }
        }

        /// <summary>
        /// 滑块值变化时更新窗口大小，并重新定位到右下角。
        /// </summary>
        private void OnSizeChanging(int newSize)
        {
            scaleFactor = newSize / 100f;
            if (scaleFactor < 0.5f) scaleFactor = 0.5f;
            if (scaleFactor > 3.0f) scaleFactor = 3.0f;

            int newWidth = (int)(BASE_WIDTH * scaleFactor);
            int newHeight = (int)(BASE_HEIGHT * scaleFactor);

            // 防止窗口过大超出屏幕
            Rectangle screen = Screen.PrimaryScreen.WorkingArea;
            if (newWidth > screen.Width) newWidth = screen.Width - 20;
            if (newHeight > screen.Height) newHeight = screen.Height - 20;

            this.Size = new Size(newWidth, newHeight);

            // 重新定位到右下角（保持边距）
            int x = screen.Right - newWidth - RIGHT_MARGIN;
            int y = screen.Bottom - newHeight - BOTTOM_MARGIN;
            if (x < screen.Left + 10) x = screen.Left + 10;
            if (y < screen.Top + 10) y = screen.Top + 10;
            this.Location = new Point(x, y);

            this.Invalidate();
        }

        /// <summary>
        /// “控制面板”菜单：弹出转速/摇头控制面板。
        /// </summary>
        private void ControlItem_Click(object sender, EventArgs e)
        {
            using (ControlPanelForm panel = new ControlPanelForm(speedLevel, isSwing))
            {
                panel.Owner = this;
                panel.SpeedChanged += new Action<int>(OnSpeedChanged);
                panel.SwingToggled += new Action<bool>(OnSwingToggled);
                panel.ShowDialog();
            }
        }

        /// <summary>
        /// 转速变化回调：设置目标转速，由动画定时器平滑过渡。
        /// </summary>
        private void OnSpeedChanged(int newSpeed)
        {
            speedLevel = newSpeed;
            switch (newSpeed)
            {
                case 0:  targetRevPerSec = 0f; break;
                case 1:  targetRevPerSec = 2.0f; break;
                case 5:  targetRevPerSec = 3.0f; break;
                case 10: targetRevPerSec = 4.0f; break;
                default: targetRevPerSec = 0f; break;
            }
        }

        /// <summary>
        /// 摇头开关回调。
        /// </summary>
        private void OnSwingToggled(bool newState)
        {
            isSwing = newState;
        }

        /// <summary>
        /// 退出程序。
        /// </summary>
        private void ExitItem_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }

        // ============================================================
        // 防最小化（WndProc 拦截系统消息）
        // ============================================================
        protected override void WndProc(ref Message m)
        {
            switch (m.Msg)
            {
                case WM_WINDOWPOSCHANGING:
                    // 如果系统试图隐藏窗口（如“显示桌面”），阻止该操作
                    WINDOWPOS wp = (WINDOWPOS)Marshal.PtrToStructure(m.LParam, typeof(WINDOWPOS));
                    if ((wp.flags & SWP_HIDEWINDOW) == SWP_HIDEWINDOW)
                    {
                        wp.flags &= ~SWP_HIDEWINDOW;
                        Marshal.StructureToPtr(wp, m.LParam, true);
                        return;
                    }
                    break;
                case WM_SIZE:
                    // 如果窗口被最小化，立即恢复
                    if ((int)m.WParam == SIZE_MINIMIZED)
                    {
                        this.BeginInvoke(new Action(delegate()
                        {
                            if (this.WindowState == FormWindowState.Minimized)
                            {
                                this.WindowState = FormWindowState.Normal;
                                this.Show();
                            }
                        }));
                        return;
                    }
                    break;
                case WM_ACTIVATE:
                    // 窗口激活时检查是否被最小化
                    base.WndProc(ref m);
                    if (this.WindowState == FormWindowState.Minimized)
                    {
                        this.BeginInvoke(new Action(delegate()
                        {
                            if (this.WindowState == FormWindowState.Minimized)
                            {
                                this.WindowState = FormWindowState.Normal;
                                this.Show();
                            }
                        }));
                    }
                    return;
            }
            base.WndProc(ref m);
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct WINDOWPOS
        {
            public IntPtr hwnd;
            public IntPtr hwndInsertAfter;
            public int x, y, cx, cy;
            public int flags;
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing && animationTimer != null) animationTimer.Dispose();
            base.Dispose(disposing);
        }
    }

    // ============================================================
    // 3. 控制面板（智能定位）
    // ============================================================
    public class ControlPanelForm : Form
    {
        private int currentSpeed;
        private bool currentSwing;
        private Button btnOff, btnLow, btnMid, btnHigh;
        private Button btnSwing;
        private Button btnClose;

        public event Action<int> SpeedChanged;
        public event Action<bool> SwingToggled;

        public ControlPanelForm(int initialSpeed, bool initialSwing)
        {
            currentSpeed = initialSpeed;
            currentSwing = initialSwing;

            this.Text = "风扇控制";
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.StartPosition = FormStartPosition.Manual;   // 手动定位
            this.Width = 320;
            this.Height = 180;
            this.BackColor = Color.White;

            // ---- 控件初始化 ----
            Label speedTitle = new Label();
            speedTitle.Text = "转速：";
            speedTitle.Font = new Font("Microsoft YaHei", 10F);
            speedTitle.Location = new Point(15, 20);
            speedTitle.Size = new Size(50, 30);
            this.Controls.Add(speedTitle);

            btnOff = new Button();
            btnOff.Text = "关";
            btnOff.Font = new Font("Microsoft YaHei", 9F, FontStyle.Bold);
            btnOff.Size = new Size(45, 30);
            btnOff.Location = new Point(70, 20);
            btnOff.Click += new EventHandler(SpeedButton_Click);
            btnOff.Tag = 0;
            this.Controls.Add(btnOff);

            btnLow = new Button();
            btnLow.Text = "低";
            btnLow.Font = new Font("Microsoft YaHei", 9F, FontStyle.Bold);
            btnLow.Size = new Size(45, 30);
            btnLow.Location = new Point(120, 20);
            btnLow.Click += new EventHandler(SpeedButton_Click);
            btnLow.Tag = 1;
            this.Controls.Add(btnLow);

            btnMid = new Button();
            btnMid.Text = "中";
            btnMid.Font = new Font("Microsoft YaHei", 9F, FontStyle.Bold);
            btnMid.Size = new Size(45, 30);
            btnMid.Location = new Point(170, 20);
            btnMid.Click += new EventHandler(SpeedButton_Click);
            btnMid.Tag = 5;
            this.Controls.Add(btnMid);

            btnHigh = new Button();
            btnHigh.Text = "高";
            btnHigh.Font = new Font("Microsoft YaHei", 9F, FontStyle.Bold);
            btnHigh.Size = new Size(45, 30);
            btnHigh.Location = new Point(220, 20);
            btnHigh.Click += new EventHandler(SpeedButton_Click);
            btnHigh.Tag = 10;
            this.Controls.Add(btnHigh);

            UpdateSpeedButtons();

            Label swingTitle = new Label();
            swingTitle.Text = "摇头：";
            swingTitle.Font = new Font("Microsoft YaHei", 10F);
            swingTitle.Location = new Point(15, 70);
            swingTitle.Size = new Size(50, 30);
            this.Controls.Add(swingTitle);

            btnSwing = new Button();
            btnSwing.Text = currentSwing ? "开" : "关";
            btnSwing.Font = new Font("Microsoft YaHei", 10F, FontStyle.Bold);
            btnSwing.Size = new Size(80, 30);
            btnSwing.Location = new Point(70, 70);
            btnSwing.BackColor = currentSwing ? Color.LightGreen : Color.LightCoral;
            btnSwing.Click += new EventHandler(Swing_Click);
            this.Controls.Add(btnSwing);

            btnClose = new Button();
            btnClose.Text = "关闭";
            btnClose.Font = new Font("Microsoft YaHei", 10F);
            btnClose.Size = new Size(80, 30);
            btnClose.Location = new Point(70, 120);
            btnClose.Click += new EventHandler(Close_Click);
            this.Controls.Add(btnClose);

            // ---- 注册 Load 事件，用于智能定位 ----
            this.Load += new EventHandler(ControlPanelForm_Load);
        }

        // Load 事件：智能定位到主窗口右侧（或左侧，若空间不足）
        private void ControlPanelForm_Load(object sender, EventArgs e)
        {
            Rectangle screen = Screen.PrimaryScreen.WorkingArea;
            Form owner = this.Owner as Form;
            if (owner != null)
            {
                int x = owner.Right + 10;
                int y = owner.Top;
                if (x + this.Width > screen.Right)
                {
                    x = owner.Left - this.Width - 10;
                }
                if (x < screen.Left)
                {
                    x = screen.Left + 10;
                }
                if (y + this.Height > screen.Bottom)
                {
                    y = screen.Bottom - this.Height - 10;
                }
                if (y < screen.Top) y = screen.Top + 10;
                this.Location = new Point(x, y);
            }
            else
            {
                this.Location = new Point(screen.Right - this.Width - 20, screen.Bottom - this.Height - 20);
            }
        }

        // ---- 事件处理 ----
        private void SpeedButton_Click(object sender, EventArgs e)
        {
            Button btn = sender as Button;
            if (btn != null)
            {
                int speed = (int)btn.Tag;
                if (currentSpeed != speed)
                {
                    currentSpeed = speed;
                    UpdateSpeedButtons();
                    if (SpeedChanged != null) SpeedChanged(currentSpeed);
                }
            }
        }

        private void UpdateSpeedButtons()
        {
            Button[] btns = { btnOff, btnLow, btnMid, btnHigh };
            foreach (Button b in btns)
            {
                b.BackColor = SystemColors.Control;
                b.ForeColor = SystemColors.ControlText;
            }
            if (currentSpeed == 0) btnOff.BackColor = Color.LightBlue;
            else if (currentSpeed == 1) btnLow.BackColor = Color.LightBlue;
            else if (currentSpeed == 5) btnMid.BackColor = Color.LightBlue;
            else if (currentSpeed == 10) btnHigh.BackColor = Color.LightBlue;
        }

        private void Swing_Click(object sender, EventArgs e)
        {
            currentSwing = !currentSwing;
            btnSwing.Text = currentSwing ? "开" : "关";
            btnSwing.BackColor = currentSwing ? Color.LightGreen : Color.LightCoral;
            if (SwingToggled != null) SwingToggled(currentSwing);
        }

        private void Close_Click(object sender, EventArgs e)
        {
            this.DialogResult = DialogResult.OK;
            this.Close();
        }
    }

    // ============================================================
    // 4. 调整大小弹窗（只有滑块，无按钮，高度减小）
    // ============================================================
    public class ResizeForm : Form
    {
        private TrackBar trackBar;
        private int selectedSize;

        public event SizeChangingHandler SizeChanging;
        public int SelectedSize
        {
            get { return selectedSize; }
        }

        public ResizeForm(int currentSize)
        {
            this.Text = "调整大小";
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.StartPosition = FormStartPosition.Manual;
            this.Width = 280;
            this.Height = 100;   // 仅滑块，高度减小
            this.BackColor = Color.White;

            // 滑块控件
            trackBar = new TrackBar();
            trackBar.Minimum = 50;
            trackBar.Maximum = 300;
            trackBar.Value = currentSize;
            trackBar.TickFrequency = 25;
            trackBar.LargeChange = 25;
            trackBar.SmallChange = 5;
            trackBar.Location = new Point(20, 20);
            trackBar.Width = 230;
            trackBar.Height = 45;
            trackBar.ValueChanged += new EventHandler(TrackBar_ValueChanged);

            this.Controls.Add(trackBar);

            selectedSize = currentSize;

            // 注册 Load 事件，用于智能定位
            this.Load += new EventHandler(ResizeForm_Load);
        }

        // Load 事件：智能定位到主窗口右侧（或左侧）
        private void ResizeForm_Load(object sender, EventArgs e)
        {
            Rectangle screen = Screen.PrimaryScreen.WorkingArea;
            Form owner = this.Owner as Form;
            if (owner != null)
            {
                int x = owner.Right + 10;
                int y = owner.Top;
                if (x + this.Width > screen.Right)
                {
                    x = owner.Left - this.Width - 10;
                }
                if (x < screen.Left)
                {
                    x = screen.Left + 10;
                }
                if (y + this.Height > screen.Bottom)
                {
                    y = screen.Bottom - this.Height - 10;
                }
                if (y < screen.Top) y = screen.Top + 10;
                this.Location = new Point(x, y);
            }
            else
            {
                this.Location = new Point(screen.Right - this.Width - 20, screen.Bottom - this.Height - 20);
            }
        }

        private void TrackBar_ValueChanged(object sender, EventArgs e)
        {
            selectedSize = trackBar.Value;
            if (SizeChanging != null) SizeChanging(selectedSize);
        }
    }

    // ============================================================
    // 5. 程序入口
    // ============================================================
    internal static class Program
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new PetForm());
        }
    }
}