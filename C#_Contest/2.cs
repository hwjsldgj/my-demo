using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using NAudio.Wave;

namespace DesktopPet
{
    // ============================================================
    // 0. 风扇声音参数类
    // ============================================================
    public class FanSoundParams
    {
        private int rpm = 1200;
        private float overallGain = 0.6f;
        private float bpfBaseAmp = 0.18f;
        private float[] harmonicAmpMultipliers = new float[] { 1.0f, 1.1f, 0.6f, 0.3f, 0.1f };
        private float noiseCenter = 850f;
        private float noiseQ = 0.75f;
        private float noiseAmp = 0.72f;
        private float hissCutoff = 3500f;
        private float hissAmp = 0.03f;
        private float gustDepth = 0.25f;
        private float[] gustFreqs = new float[] { 0.12f, 0.28f, 0.07f };
        private bool enableMotorVibration = false;

        public int RPM { get { return rpm; } set { rpm = value; } }
        public float OverallGain { get { return overallGain; } set { overallGain = value; } }
        public float BpfBaseAmp { get { return bpfBaseAmp; } set { bpfBaseAmp = value; } }
        public float[] HarmonicAmpMultipliers { get { return harmonicAmpMultipliers; } set { harmonicAmpMultipliers = value; } }
        public float NoiseCenter { get { return noiseCenter; } set { noiseCenter = value; } }
        public float NoiseQ { get { return noiseQ; } set { noiseQ = value; } }
        public float NoiseAmp { get { return noiseAmp; } set { noiseAmp = value; } }
        public float HissCutoff { get { return hissCutoff; } set { hissCutoff = value; } }
        public float HissAmp { get { return hissAmp; } set { hissAmp = value; } }
        public float GustDepth { get { return gustDepth; } set { gustDepth = value; } }
        public float[] GustFreqs { get { return gustFreqs; } set { gustFreqs = value; } }
        public bool EnableMotorVibration { get { return enableMotorVibration; } set { enableMotorVibration = value; } }
    }

    // ============================================================
    // 1. 3D 数学基础
    // ============================================================
    public delegate void SizeChangingHandler(int newSize);

    public struct Vector3
    {
        public float X, Y, Z;
        public Vector3(float x, float y, float z) { X = x; Y = y; Z = z; }
        public static Vector3 operator +(Vector3 a, Vector3 b) { return new Vector3(a.X + b.X, a.Y + b.Y, a.Z + b.Z); }
        public static Vector3 operator -(Vector3 a, Vector3 b) { return new Vector3(a.X - b.X, a.Y - b.Y, a.Z - b.Z); }
        public static Vector3 operator *(Vector3 a, float k) { return new Vector3(a.X * k, a.Y * k, a.Z * k); }
        public static Vector3 operator *(float k, Vector3 a) { return new Vector3(a.X * k, a.Y * k, a.Z * k); }
        public float Length() { return (float)Math.Sqrt(X * X + Y * Y + Z * Z); }
        public void Normalize() { float l = Length(); if (l > 0) { X /= l; Y /= l; Z /= l; } }
        public static Vector3 Cross(Vector3 a, Vector3 b) { return new Vector3(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X); }
        public static float Dot(Vector3 a, Vector3 b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z; }
    }

    public class Transform3D
    {
        private float[,] m = new float[4, 4];
        public Transform3D() { Identity(); }
        public void Identity() { for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) m[i, j] = (i == j) ? 1f : 0f; }
        public void RotateY(float angle) { float cos = (float)Math.Cos(angle), sin = (float)Math.Sin(angle); Transform3D t = new Transform3D(); t.m[0,0]=cos; t.m[0,2]=sin; t.m[2,0]=-sin; t.m[2,2]=cos; Multiply(t); }
        public void RotateX(float angle) { float cos = (float)Math.Cos(angle), sin = (float)Math.Sin(angle); Transform3D t = new Transform3D(); t.m[1,1]=cos; t.m[1,2]=-sin; t.m[2,1]=sin; t.m[2,2]=cos; Multiply(t); }
        public void Translate(float x, float y, float z) { Transform3D t = new Transform3D(); t.m[0,3]=x; t.m[1,3]=y; t.m[2,3]=z; Multiply(t); }
        private void Multiply(Transform3D t) { float[,] res = new float[4,4]; for(int i=0;i<4;i++) for(int j=0;j<4;j++) for(int k=0;k<4;k++) res[i,j] += m[i,k]*t.m[k,j]; m = res; }
        public Vector3 Apply(Vector3 v) { float x = m[0,0]*v.X + m[0,1]*v.Y + m[0,2]*v.Z + m[0,3]; float y = m[1,0]*v.X + m[1,1]*v.Y + m[1,2]*v.Z + m[1,3]; float z = m[2,0]*v.X + m[2,1]*v.Y + m[2,2]*v.Z + m[2,3]; return new Vector3(x,y,z); }
    }

    // ============================================================
    // 2. 主窗体（桌宠 + 声音）
    // ============================================================
    public class PetForm : Form
    {
        private const int RIGHT_MARGIN = 320 + 20;
        private const int BOTTOM_MARGIN = 60;
        private const int WM_WINDOWPOSCHANGING = 0x0046;
        private const int WM_SIZE = 0x0005;
        private const int WM_ACTIVATE = 0x0006;
        private const int SIZE_MINIMIZED = 0x0001;
        private const int SWP_HIDEWINDOW = 0x0080;

        private float fanAngle = 0f;
        private int speedLevel = 0;
        private bool isSwing = false;
        private float swingAngle = 0f;
        private float swingTime = 0f;
        private Timer animationTimer;
        private const int ANIMATION_INTERVAL = 8;

        private const int BASE_WIDTH = 150;
        private const int BASE_HEIGHT = 200;
        private float scaleFactor = 1.0f;

        private bool isDragging = false;
        private Point dragStartPoint = Point.Empty;
        private ContextMenuStrip contextMenu;

        private const float FAN_RADIUS = 0.9f;
        private const float FAN_THICKNESS = 0.15f;
        private const float BLADE_LENGTH = 0.75f;
        private const float BLADE_WIDTH = 0.6f;
        private const float PILLAR_HEIGHT = 1.6f;
        private const float PILLAR_RADIUS = 0.1f;
        private const float BASE_SIZE = 0.6f;
        private const float BASE_BOX_HEIGHT = 0.12f;
        private const float VIEW_DIST = 4.0f;
        private const int SEG = 24;
        private const int PILLAR_SEG = 16;

        private float currentRevPerSec = 0f;
        private float targetRevPerSec = 0f;

        // ---- 声音播放器 ----
        private WaveOutEvent outputDevice;
        private BufferedWaveProvider waveProvider;
        private FanSoundParams currentSoundParams;
        private bool isSoundPlaying = false;
        private bool isDeviceInitialized = false;
        private readonly object soundLock = new object();

        private static FanSoundParams lowSoundParams;
        private static FanSoundParams midSoundParams;
        private static FanSoundParams highSoundParams;

        static PetForm()
        {
            lowSoundParams = new FanSoundParams();
            lowSoundParams.RPM = 1520; lowSoundParams.OverallGain = 0.55f; lowSoundParams.BpfBaseAmp = 0.04f;
            lowSoundParams.HarmonicAmpMultipliers = new float[] { 1.0f, 0.65f, 0.65f, 0.3f };
            lowSoundParams.NoiseCenter = 650f; lowSoundParams.NoiseQ = 0.8f; lowSoundParams.NoiseAmp = 0.90f;
            lowSoundParams.HissCutoff = 4000f; lowSoundParams.HissAmp = 0.0f; lowSoundParams.GustDepth = 0.35f;
            lowSoundParams.GustFreqs = new float[] { 0.10f, 0.22f, 0.05f }; lowSoundParams.EnableMotorVibration = false;

            midSoundParams = new FanSoundParams();
            midSoundParams.RPM = 1760; midSoundParams.OverallGain = 0.60f; midSoundParams.BpfBaseAmp = 0.05f;
            midSoundParams.HarmonicAmpMultipliers = new float[] { 1.0f, 1.5f, 1.0f, 0.4f };
            midSoundParams.NoiseCenter = 850f; midSoundParams.NoiseQ = 0.7f; midSoundParams.NoiseAmp = 0.92f;
            midSoundParams.HissCutoff = 3500f; midSoundParams.HissAmp = 0.02f; midSoundParams.GustDepth = 0.35f;
            midSoundParams.GustFreqs = new float[] { 0.12f, 0.25f, 0.06f }; midSoundParams.EnableMotorVibration = false;

            highSoundParams = new FanSoundParams();
            highSoundParams.RPM = 2000; highSoundParams.OverallGain = 0.65f; highSoundParams.BpfBaseAmp = 0.06f;
            highSoundParams.HarmonicAmpMultipliers = new float[] { 1.0f, 1.0f, 0.7f, 0.8f, 0.4f };
            highSoundParams.NoiseCenter = 1050f; highSoundParams.NoiseQ = 0.9f; highSoundParams.NoiseAmp = 0.95f;
            highSoundParams.HissCutoff = 3000f; highSoundParams.HissAmp = 0.04f; highSoundParams.GustDepth = 0.35f;
            highSoundParams.GustFreqs = new float[] { 0.12f, 0.28f, 0.07f }; highSoundParams.EnableMotorVibration = false;
        }

        public PetForm()
        {
            Color rareColor = Color.FromArgb(1, 2, 3);
            this.BackColor = rareColor;
            this.TransparencyKey = rareColor;
            this.FormBorderStyle = FormBorderStyle.None;
            this.ShowInTaskbar = false;
            this.TopMost = false;
            this.Size = new Size(BASE_WIDTH, BASE_HEIGHT);
            this.SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.DoubleBuffer, true);
            this.Load += new EventHandler(PetForm_Load);
            SetInitialPosition();
            CreateContextMenu();
            this.Paint += PetForm_Paint;

            animationTimer = new Timer();
            animationTimer.Interval = ANIMATION_INTERVAL;
            animationTimer.Tick += new EventHandler(AnimationTimer_Tick);
            animationTimer.Start();

            Timer checkTimer = new Timer();
            checkTimer.Interval = 500;
            checkTimer.Tick += new EventHandler(CheckTimer_Tick);
            checkTimer.Start();

            // 声音初始化（不播放）
            outputDevice = new WaveOutEvent();
            outputDevice.PlaybackStopped += OnPlaybackStopped;
        }

        private void SetInitialPosition()
        {
            Rectangle screen = Screen.PrimaryScreen.WorkingArea;
            int x = screen.Right - this.Width - 300;
            int y = screen.Bottom - this.Height - 30;
            this.Location = new Point(x, y);
        }

        private void PetForm_Load(object sender, EventArgs e)
        {
            Rectangle screen = Screen.PrimaryScreen.WorkingArea;
            int x = screen.Right - this.Width - RIGHT_MARGIN;
            int y = screen.Bottom - this.Height - BOTTOM_MARGIN;
            if (x < screen.Left + 10) x = screen.Left + 10;
            if (y < screen.Top + 10) y = screen.Top + 10;
            this.Location = new Point(x, y);
        }

        private void CheckTimer_Tick(object sender, EventArgs e)
        {
            if (this.WindowState == FormWindowState.Minimized)
            {
                this.WindowState = FormWindowState.Normal;
                this.Show();
            }
        }

        private void AnimationTimer_Tick(object sender, EventArgs e)
        {
            float acceleration = 2.5f;
            float delta = acceleration * (ANIMATION_INTERVAL / 1000f);
            if (currentRevPerSec < targetRevPerSec)
            {
                currentRevPerSec += delta;
                if (currentRevPerSec > targetRevPerSec) currentRevPerSec = targetRevPerSec;
            }
            else if (currentRevPerSec > targetRevPerSec)
            {
                currentRevPerSec -= delta;
                if (currentRevPerSec < targetRevPerSec) currentRevPerSec = targetRevPerSec;
            }
            if (currentRevPerSec > 0)
            {
                float degreesPerFrame = currentRevPerSec * 360f * (ANIMATION_INTERVAL / 1000f);
                fanAngle += degreesPerFrame;
                if (fanAngle > 360) fanAngle -= 360;
            }
            if (isSwing)
            {
                swingTime += 0.015f;
                swingAngle = (float)(Math.Sin(swingTime) * 35);
            }
            this.Invalidate();
        }

        private void CreateContextMenu()
        {
            contextMenu = new ContextMenuStrip();
            ToolStripMenuItem resizeItem = new ToolStripMenuItem("调整大小");
            resizeItem.Click += new EventHandler(ResizeItem_Click);
            contextMenu.Items.Add(resizeItem);
            ToolStripMenuItem controlItem = new ToolStripMenuItem("控制面板");
            controlItem.Click += new EventHandler(ControlItem_Click);
            contextMenu.Items.Add(controlItem);
            contextMenu.Items.Add(new ToolStripSeparator());
            ToolStripMenuItem exitItem = new ToolStripMenuItem("退出");
            exitItem.Click += new EventHandler(ExitItem_Click);
            contextMenu.Items.Add(exitItem);
            this.ContextMenuStrip = contextMenu;
        }

        // ---- 3D 绘制 ----
        private void PetForm_Paint(object sender, PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            int w = this.Width;
            int h = this.Height;
            float maxHeight = BASE_BOX_HEIGHT / 2 + PILLAR_HEIGHT + FAN_RADIUS;
            float scale = Math.Min(w, h) / (0.85f * Math.Max(FAN_RADIUS, maxHeight));
            scale *= 0.9f;
            float cx = w / 2f;
            float modelCenterY = (PILLAR_HEIGHT + FAN_RADIUS + BASE_BOX_HEIGHT) / 2.0f;
            float cy = h / 2f + modelCenterY * scale * 0.82f;

            Vector3 lightDir = new Vector3(0.4f, 0.7f, 0.3f);
            lightDir.Normalize();

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

        private void DrawBase(Graphics g, float cx, float cy, float scale, Vector3 light,
            SolidBrush brushLight, SolidBrush brushDark, Pen darkPen)
        {
            Transform3D world = new Transform3D();
            float hs = BASE_SIZE / 2;
            float hh = BASE_BOX_HEIGHT / 2;
            Vector3[] verts = new Vector3[]
            {
                new Vector3(-hs, -hh, -hs), new Vector3( hs, -hh, -hs),
                new Vector3( hs, -hh,  hs), new Vector3(-hs, -hh,  hs),
                new Vector3(-hs,  hh, -hs), new Vector3( hs,  hh, -hs),
                new Vector3( hs,  hh,  hs), new Vector3(-hs,  hh,  hs)
            };
            int[][] quads = new int[][]
            {
                new int[]{0,1,2,3}, new int[]{4,5,6,7},
                new int[]{0,1,5,4}, new int[]{1,2,6,5},
                new int[]{2,3,7,6}, new int[]{3,0,4,7}
            };
            foreach (var quad in quads)
            {
                PointF[] pts = new PointF[4];
                for (int i = 0; i < 4; i++) pts[i] = Project(world.Apply(verts[quad[i]]), cx, cy, scale);
                g.FillPolygon(brushLight, pts);
                g.DrawPolygon(darkPen, pts);
            }
        }

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
            for (int i = 0; i < seg; i++)
            {
                int j = (i + 1) % seg;
                Vector3[] quad = new Vector3[] { botPts[i], botPts[j], topPts[j], topPts[i] };
                PointF[] pts = new PointF[4];
                for (int k = 0; k < 4; k++) pts[k] = Project(world.Apply(quad[k]), cx, cy, scale);
                g.FillPolygon(brushLight, pts);
                g.DrawPolygon(darkPen, pts);
            }
        }

        private void DrawFanHead(Graphics g, float cx, float cy, float scale, Vector3 light,
            SolidBrush brushLight, SolidBrush brushDark, Pen darkPen, Pen grayPen)
        {
            Transform3D head = new Transform3D();
            head.Translate(0, BASE_BOX_HEIGHT / 2 + PILLAR_HEIGHT, 0);
            float swingRad = swingAngle * (float)Math.PI / 180f;
            head.RotateY(swingRad);

            int seg = SEG;
            float r = FAN_RADIUS;
            float th = FAN_THICKNESS / 2;

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

            PointF[] screenFront = new PointF[seg];
            for (int i = 0; i < seg; i++) screenFront[i] = Project(head.Apply(frontPts[i]), cx, cy, scale);
            using (SolidBrush frontBrush = new SolidBrush(Color.FromArgb(230, 230, 230)))
                g.FillPolygon(frontBrush, screenFront);
            g.DrawPolygon(darkPen, screenFront);

            PointF[] screenBack = new PointF[seg];
            for (int i = 0; i < seg; i++) screenBack[i] = Project(head.Apply(backPts[i]), cx, cy, scale);
            using (SolidBrush backBrush = new SolidBrush(Color.FromArgb(150, 150, 150)))
                g.FillPolygon(backBrush, screenBack);
            g.DrawPolygon(darkPen, screenBack);

            for (int i = 0; i < seg; i++)
            {
                int j = (i + 1) % seg;
                Vector3[] side = new Vector3[] { backPts[i], backPts[j], frontPts[j], frontPts[i] };
                PointF[] sidePts = new PointF[4];
                for (int k = 0; k < 4; k++) sidePts[k] = Project(head.Apply(side[k]), cx, cy, scale);
                g.FillPolygon(brushDark, sidePts);
                g.DrawPolygon(darkPen, sidePts);
            }

            float revPerSec = currentRevPerSec;
            float bladeRad = fanAngle * (float)Math.PI / 180f;
            float motionAngle = revPerSec * 2 * (float)Math.PI * (ANIMATION_INTERVAL / 1000f) * 1.2f;
            bool enableBlur = (revPerSec > 0.5f);

            using (SolidBrush bladeBrush = new SolidBrush(Color.FromArgb(200, 160, 200, 255)))
            {
                for (int i = 0; i < 3; i++)
                {
                    float baseAngle = i * 2 * (float)Math.PI / 3 + bladeRad;
                    float l = BLADE_LENGTH;
                    float w = BLADE_WIDTH;
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

                    if (!enableBlur)
                    {
                        PointF[] screenPts = new PointF[pointCount];
                        for (int p = 0; p < pointCount; p++) screenPts[p] = Project(head.Apply(basePts[p]), cx, cy, scale);
                        g.FillPolygon(bladeBrush, screenPts);
                        using (Pen contourPen = new Pen(Color.FromArgb(100, 120, 180), 0.5f))
                            g.DrawPolygon(contourPen, screenPts);
                        continue;
                    }

                    int blurSteps = 10;
                    for (int step = 0; step < blurSteps; step++)
                    {
                        float tBlur = step / (float)(blurSteps - 1);
                        float angleOffset = (tBlur - 0.5f) * motionAngle;
                        float currentAngle = baseAngle + angleOffset;
                        float alphaFactor = 1.0f - (float)Math.Pow(Math.Abs(tBlur - 0.5f) * 2, 1.5f);
                        int alpha = (int)(60 + alphaFactor * 160);
                        if (alpha < 0) alpha = 0;
                        if (alpha > 255) alpha = 255;

                        Vector3[] blurredPts = new Vector3[pointCount];
                        for (int p = 0; p < pointCount; p++)
                        {
                            float len = (float)Math.Sqrt(basePts[p].X * basePts[p].X + basePts[p].Y * basePts[p].Y);
                            float ang = (float)Math.Atan2(basePts[p].Y, basePts[p].X);
                            float newAng = ang + angleOffset;
                            blurredPts[p] = new Vector3(len * (float)Math.Cos(newAng), len * (float)Math.Sin(newAng), basePts[p].Z);
                        }
                        PointF[] screenPtsBlur = new PointF[pointCount];
                        for (int p = 0; p < pointCount; p++) screenPtsBlur[p] = Project(head.Apply(blurredPts[p]), cx, cy, scale);
                        using (SolidBrush blurBrush = new SolidBrush(Color.FromArgb(alpha, 160, 200, 255)))
                            g.FillPolygon(blurBrush, screenPtsBlur);
                    }

                    PointF[] centerPts = new PointF[pointCount];
                    for (int p = 0; p < pointCount; p++) centerPts[p] = Project(head.Apply(basePts[p]), cx, cy, scale);
                    using (SolidBrush centerBrush = new SolidBrush(Color.FromArgb(225, 160, 200, 255)))
                        g.FillPolygon(centerBrush, centerPts);
                    using (Pen contourPen = new Pen(Color.FromArgb(100, 120, 180), 0.5f))
                        g.DrawPolygon(contourPen, centerPts);
                }
            }

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
            for (int i = 0; i < axisSeg; i++) screenAxis[i] = Project(head.Apply(axisPts[i]), cx, cy, scale);
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
            for (int i = 0; i < 8; i++) screenHl[i] = Project(head.Apply(hlPts[i]), cx, cy, scale);
            using (SolidBrush hlBrush = new SolidBrush(Color.FromArgb(255, 255, 255)))
                g.FillPolygon(hlBrush, screenHl);
        }

        private PointF Project(Vector3 v, float cx, float cy, float scale)
        {
            float d = VIEW_DIST;
            float inv = 1f / (1 + v.Z / d);
            float x = v.X * scale * inv + cx;
            float y = -v.Y * scale * inv + cy;
            return new PointF(x, y);
        }

        // ---- 鼠标事件 ----
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
            if (e.Button == MouseButtons.Left) isDragging = false;
        }

        // ---- 菜单事件 ----
        private void ResizeItem_Click(object sender, EventArgs e)
        {
            int currentSliderValue = (int)(scaleFactor * 100f);
            using (ResizeForm resizeForm = new ResizeForm(currentSliderValue))
            {
                resizeForm.Owner = this;
                resizeForm.SizeChanging += new SizeChangingHandler(OnSizeChanging);
                resizeForm.ShowDialog();
            }
        }

        private void OnSizeChanging(int newSize)
        {
            scaleFactor = newSize / 100f;
            if (scaleFactor < 0.5f) scaleFactor = 0.5f;
            if (scaleFactor > 3.0f) scaleFactor = 3.0f;
            int newWidth = (int)(BASE_WIDTH * scaleFactor);
            int newHeight = (int)(BASE_HEIGHT * scaleFactor);
            Rectangle screen = Screen.PrimaryScreen.WorkingArea;
            if (newWidth > screen.Width) newWidth = screen.Width - 20;
            if (newHeight > screen.Height) newHeight = screen.Height - 20;
            this.Size = new Size(newWidth, newHeight);
            int x = screen.Right - newWidth - RIGHT_MARGIN;
            int y = screen.Bottom - newHeight - BOTTOM_MARGIN;
            if (x < screen.Left + 10) x = screen.Left + 10;
            if (y < screen.Top + 10) y = screen.Top + 10;
            this.Location = new Point(x, y);
            this.Invalidate();
        }

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

        // ---- 核心修改：优化换挡响应 ----
        private void OnSpeedChanged(int newSpeed)
        {
            speedLevel = newSpeed;
            switch (newSpeed)
            {
                case 0: targetRevPerSec = 0f; break;
                case 1: targetRevPerSec = 2.0f; break;
                case 5: targetRevPerSec = 3.0f; break;
                case 10: targetRevPerSec = 4.0f; break;
                default: targetRevPerSec = 0f; break;
            }

            lock (soundLock)
            {
                if (newSpeed == 0)
                {
                    isSoundPlaying = false;
                    if (outputDevice != null && outputDevice.PlaybackState == PlaybackState.Playing)
                        outputDevice.Stop();
                    if (waveProvider != null)
                        waveProvider.ClearBuffer();
                    return;
                }

                FanSoundParams param;
                if (newSpeed == 1) param = lowSoundParams;
                else if (newSpeed == 5) param = midSoundParams;
                else if (newSpeed == 10) param = highSoundParams;
                else return;

                const int sampleRate = 44100;
                const float duration = 0.5f;
                int totalSamples = (int)(sampleRate * duration);
                short[] pcm = new short[totalSamples];
                GenerateSamples(pcm, 0, totalSamples, param, sampleRate, false);
                byte[] pcmBytes = new byte[pcm.Length * 2];
                Buffer.BlockCopy(pcm, 0, pcmBytes, 0, pcmBytes.Length);

                if (!isDeviceInitialized)
                {
                    waveProvider = new BufferedWaveProvider(new WaveFormat(sampleRate, 16, 1));
                    waveProvider.DiscardOnBufferOverflow = true;
                    waveProvider.BufferDuration = TimeSpan.FromSeconds(1.0);
                    outputDevice.Init(waveProvider);
                    isDeviceInitialized = true;
                    for (int i = 0; i < 3; i++)
                        waveProvider.AddSamples(pcmBytes, 0, pcmBytes.Length);
                }
                else
                {
                    for (int i = 0; i < 2; i++)
                        waveProvider.AddSamples(pcmBytes, 0, pcmBytes.Length);
                }

                if (outputDevice.PlaybackState != PlaybackState.Playing)
                    outputDevice.Play();

                isSoundPlaying = true;
                currentSoundParams = param;
            }
        }

        private void OnSwingToggled(bool newState) { isSwing = newState; }

        private void ExitItem_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }

        // ---- 声音播放器事件 ----
        private void OnPlaybackStopped(object sender, StoppedEventArgs e)
        {
            if (isSoundPlaying && waveProvider != null && currentSoundParams != null)
            {
                lock (soundLock)
                {
                    if (isSoundPlaying && waveProvider != null && currentSoundParams != null)
                    {
                        const int sampleRate = 44100;
                        const float duration = 0.5f;
                        int totalSamples = (int)(sampleRate * duration);
                        short[] pcm = new short[totalSamples];
                        GenerateSamples(pcm, 0, totalSamples, currentSoundParams, sampleRate, false);
                        byte[] pcmBytes = new byte[pcm.Length * 2];
                        Buffer.BlockCopy(pcm, 0, pcmBytes, 0, pcmBytes.Length);
                        waveProvider.AddSamples(pcmBytes, 0, pcmBytes.Length);
                        if (outputDevice != null && outputDevice.PlaybackState != PlaybackState.Playing)
                            outputDevice.Play();
                    }
                }
            }
        }

        // ---- 声音合成引擎 ----
        private static void GenerateSamples(short[] pcmData, int offset, int count,
                                            FanSoundParams param, int sampleRate, bool isStart)
        {
            Random rnd = new Random(42);
            float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0;
            float[] pink = new float[count];
            for (int i = 0; i < count; i++)
            {
                float white = (float)(rnd.NextDouble() * 2 - 1);
                b0 = 0.99886f * b0 + white * 0.0555179f;
                b1 = 0.99332f * b1 + white * 0.0750759f;
                b2 = 0.96900f * b2 + white * 0.1538520f;
                b3 = 0.86650f * b3 + white * 0.3104856f;
                b4 = 0.55000f * b4 + white * 0.5329522f;
                b5 = -0.7616f * b5 - white * 0.0168980f;
                pink[i] = b0 + b1 + b2 + b3 + b4 + b5 + white * 0.5362f;
                pink[i] *= 0.11f;
                if (pink[i] > 1.0f) pink[i] = 1.0f;
                if (pink[i] < -1.0f) pink[i] = -1.0f;
            }

            Random rndWhite = new Random(999);
            float[] whiteNoise = new float[count];
            for (int i = 0; i < count; i++) whiteNoise[i] = (float)(rndWhite.NextDouble() * 2 - 1);

            double w0 = 2 * Math.PI * param.NoiseCenter / sampleRate;
            double alpha = Math.Sin(w0) / (2 * param.NoiseQ);
            double b0n = alpha / (1 + alpha);
            double b1n = 0;
            double b2n = -alpha / (1 + alpha);
            double a1n = -2 * Math.Cos(w0) / (1 + alpha);
            double a2n = (1 - alpha) / (1 + alpha);

            float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

            double rc = 1.0 / (2 * Math.PI * param.HissCutoff);
            double dt = 1.0 / sampleRate;
            double alphaH = dt / (rc + dt);
            float prevH = 0;

            float[] gustPhase = new float[param.GustFreqs.Length];
            Random rndGust = new Random(789);
            for (int i = 0; i < gustPhase.Length; i++) gustPhase[i] = (float)(rndGust.NextDouble() * 2 * Math.PI);

            int maxHarm = param.HarmonicAmpMultipliers.Length;
            float[] harmonicPhase = new float[maxHarm];
            Random rndHarm = new Random(123);
            for (int i = 0; i < maxHarm; i++) harmonicPhase[i] = (float)(rndHarm.NextDouble() * 2 * Math.PI);

            double windEnv = 1.0;
            double motorVib = 0.0;

            for (int i = 0; i < count; i++)
            {
                double t = i / (double)sampleRate;
                if (isStart)
                {
                    windEnv = 1 - Math.Exp(-4 * t);
                    motorVib = Math.Exp(-6 * t) * 0.08;
                }
                else
                {
                    windEnv = 1.0;
                    motorVib = 0.0;
                }

                double currentRpm = isStart ? param.RPM * windEnv : param.RPM;
                double bpf = (currentRpm * 3) / 60.0;

                double harmonicSum = 0;
                double harmonicMod = 0.6 + 0.4 * (0.5 + 0.5 * Math.Sin(2 * Math.PI * 0.25 * t + gustPhase[0]));
                double harmonicMod2 = 0.7 + 0.3 * Math.Sin(2 * Math.PI * 0.11 * t + gustPhase[1]);
                double finalMod = harmonicMod * harmonicMod2;

                for (int h = 0; h < maxHarm; h++)
                {
                    double amp = param.BpfBaseAmp * param.HarmonicAmpMultipliers[h] * finalMod;
                    double freq = bpf * (h + 1);
                    double jitter = 0.5 * Math.Sin(2 * Math.PI * 0.6 * t + h * 1.3);
                    freq += jitter;
                    if (freq > sampleRate / 2) break;
                    harmonicSum += amp * Math.Sin(2 * Math.PI * freq * t + harmonicPhase[h]);
                }

                float xn = pink[i];
                float yn = (float)(b0n * xn + b1n * x1 + b2n * x2 - a1n * y1 - a2n * y2);
                x2 = x1; x1 = xn;
                y2 = y1; y1 = yn;
                float filteredPink = yn;

                float hiss = 0;
                if (param.HissAmp > 0.001f)
                {
                    float hp = (float)(alphaH * (whiteNoise[i] - prevH));
                    prevH = whiteNoise[i];
                    hiss = hp * param.HissAmp;
                }

                float gust = 1.0f;
                for (int g = 0; g < param.GustFreqs.Length; g++)
                {
                    gust += param.GustDepth / param.GustFreqs.Length *
                            (float)Math.Sin(2 * Math.PI * param.GustFreqs[g] * t + gustPhase[g]);
                }
                gust = Math.Max(0.3f, Math.Min(1.7f, gust));

                double vib = 0;
                if (param.EnableMotorVibration && isStart)
                {
                    vib = motorVib * (0.7 * Math.Sin(2 * Math.PI * 50 * t) + 0.3 * Math.Sin(2 * Math.PI * 100 * t + 0.5));
                }

                double sample = (filteredPink * param.NoiseAmp + harmonicSum) * gust * windEnv
                                + hiss * windEnv + vib;

                sample *= param.OverallGain;

                if (sample > 1.0) sample = 1.0;
                if (sample < -1.0) sample = -1.0;
                pcmData[offset + i] = (short)(sample * short.MaxValue);
            }

            int fadeSamples = (int)(0.005 * sampleRate);
            if (fadeSamples > count / 2) fadeSamples = count / 2;
            for (int i = 0; i < fadeSamples; i++)
            {
                float ratio = i / (float)fadeSamples;
                pcmData[offset + i] = (short)(pcmData[offset + i] * ratio);
                pcmData[offset + count - 1 - i] = (short)(pcmData[offset + count - 1 - i] * ratio);
            }
        }

        // ---- WndProc ----
        protected override void WndProc(ref Message m)
        {
            switch (m.Msg)
            {
                case WM_WINDOWPOSCHANGING:
                    WINDOWPOS wp = (WINDOWPOS)Marshal.PtrToStructure(m.LParam, typeof(WINDOWPOS));
                    if ((wp.flags & SWP_HIDEWINDOW) == SWP_HIDEWINDOW)
                    {
                        wp.flags &= ~SWP_HIDEWINDOW;
                        Marshal.StructureToPtr(wp, m.LParam, true);
                        return;
                    }
                    break;
                case WM_SIZE:
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
            if (disposing)
            {
                if (animationTimer != null) animationTimer.Dispose();
                if (outputDevice != null)
                {
                    outputDevice.Stop();
                    outputDevice.Dispose();
                    outputDevice = null;
                }
                if (waveProvider != null)
                {
                    waveProvider.ClearBuffer();
                    waveProvider = null;
                }
            }
            base.Dispose(disposing);
        }
    }

    // ============================================================
    // 3. 控制面板
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
            this.StartPosition = FormStartPosition.Manual;
            this.Width = 320;
            this.Height = 180;
            this.BackColor = Color.White;

            Label speedTitle = new Label();
            speedTitle.Text = "转速："; speedTitle.Font = new Font("Microsoft YaHei", 10F);
            speedTitle.Location = new Point(15, 20); speedTitle.Size = new Size(50, 30);
            this.Controls.Add(speedTitle);

            btnOff = new Button();
            btnOff.Text = "关"; btnOff.Font = new Font("Microsoft YaHei", 9F, FontStyle.Bold);
            btnOff.Size = new Size(45, 30); btnOff.Location = new Point(70, 20);
            btnOff.Click += new EventHandler(SpeedButton_Click); btnOff.Tag = 0;
            this.Controls.Add(btnOff);

            btnLow = new Button();
            btnLow.Text = "低"; btnLow.Font = new Font("Microsoft YaHei", 9F, FontStyle.Bold);
            btnLow.Size = new Size(45, 30); btnLow.Location = new Point(120, 20);
            btnLow.Click += new EventHandler(SpeedButton_Click); btnLow.Tag = 1;
            this.Controls.Add(btnLow);

            btnMid = new Button();
            btnMid.Text = "中"; btnMid.Font = new Font("Microsoft YaHei", 9F, FontStyle.Bold);
            btnMid.Size = new Size(45, 30); btnMid.Location = new Point(170, 20);
            btnMid.Click += new EventHandler(SpeedButton_Click); btnMid.Tag = 5;
            this.Controls.Add(btnMid);

            btnHigh = new Button();
            btnHigh.Text = "高"; btnHigh.Font = new Font("Microsoft YaHei", 9F, FontStyle.Bold);
            btnHigh.Size = new Size(45, 30); btnHigh.Location = new Point(220, 20);
            btnHigh.Click += new EventHandler(SpeedButton_Click); btnHigh.Tag = 10;
            this.Controls.Add(btnHigh);

            UpdateSpeedButtons();

            Label swingTitle = new Label();
            swingTitle.Text = "摇头："; swingTitle.Font = new Font("Microsoft YaHei", 10F);
            swingTitle.Location = new Point(15, 70); swingTitle.Size = new Size(50, 30);
            this.Controls.Add(swingTitle);

            btnSwing = new Button();
            btnSwing.Text = currentSwing ? "开" : "关";
            btnSwing.Font = new Font("Microsoft YaHei", 10F, FontStyle.Bold);
            btnSwing.Size = new Size(80, 30); btnSwing.Location = new Point(70, 70);
            btnSwing.BackColor = currentSwing ? Color.LightGreen : Color.LightCoral;
            btnSwing.Click += new EventHandler(Swing_Click);
            this.Controls.Add(btnSwing);

            btnClose = new Button();
            btnClose.Text = "关闭"; btnClose.Font = new Font("Microsoft YaHei", 10F);
            btnClose.Size = new Size(80, 30); btnClose.Location = new Point(70, 120);
            btnClose.Click += new EventHandler(Close_Click);
            this.Controls.Add(btnClose);

            this.Load += new EventHandler(ControlPanelForm_Load);
        }

        private void ControlPanelForm_Load(object sender, EventArgs e)
        {
            Rectangle screen = Screen.PrimaryScreen.WorkingArea;
            Form owner = this.Owner as Form;
            if (owner != null)
            {
                int x = owner.Right + 10;
                int y = owner.Top;
                if (x + this.Width > screen.Right) x = owner.Left - this.Width - 10;
                if (x < screen.Left) x = screen.Left + 10;
                if (y + this.Height > screen.Bottom) y = screen.Bottom - this.Height - 10;
                if (y < screen.Top) y = screen.Top + 10;
                this.Location = new Point(x, y);
            }
            else
            {
                this.Location = new Point(screen.Right - this.Width - 20, screen.Bottom - this.Height - 20);
            }
        }

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
                    if (SpeedChanged != null) SpeedChanged(speed);
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
    // 4. 调整大小弹窗
    // ============================================================
    public class ResizeForm : Form
    {
        private TrackBar trackBar;
        private int selectedSize;
        public event SizeChangingHandler SizeChanging;
        public int SelectedSize { get { return selectedSize; } }

        public ResizeForm(int currentSize)
        {
            this.Text = "调整大小";
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.StartPosition = FormStartPosition.Manual;
            this.Width = 280;
            this.Height = 100;
            this.BackColor = Color.White;

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
            this.Load += new EventHandler(ResizeForm_Load);
        }

        private void ResizeForm_Load(object sender, EventArgs e)
        {
            Rectangle screen = Screen.PrimaryScreen.WorkingArea;
            Form owner = this.Owner as Form;
            if (owner != null)
            {
                int x = owner.Right + 10;
                int y = owner.Top;
                if (x + this.Width > screen.Right) x = owner.Left - this.Width - 10;
                if (x < screen.Left) x = screen.Left + 10;
                if (y + this.Height > screen.Bottom) y = screen.Bottom - this.Height - 10;
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