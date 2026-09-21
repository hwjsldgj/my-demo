using System;
using System.Drawing;
using System.IO;
using System.Media;
using System.Windows.Forms;

namespace RealisticFanSound
{
    public partial class MainForm : Form
    {
        private Button btnLow, btnMid, btnHigh, btnStop;
        private Label lblStatus;
        private SoundPlayer startPlayer, steadyPlayer;
        private Timer startTimer;
        private bool isPlaying = false;
        private int currentRpm = 0;
        private float currentAmp = 0;

        // ========== 三档调优参数（基于真实频谱分析） ==========
        private readonly FanSoundParams lowParams = new FanSoundParams
        {
            RPM = 1520,                 // 低档实际转速（根据基频 76Hz 反算）
            OverallGain = 0.55f,
            BpfBaseAmp = 0.18f,
            HarmonicAmpMultipliers = new float[] { 1.0f, 0.65f, 0.65f, 0.3f },
            NoiseCenter = 650f,
            NoiseQ = 0.8f,
            NoiseAmp = 0.65f,
            HissCutoff = 4000f,
            HissAmp = 0.0f,
            GustDepth = 0.30f,
            GustFreqs = new float[] { 0.10f, 0.22f, 0.05f },
            EnableMotorVibration = false
        };

        private readonly FanSoundParams midParams = new FanSoundParams
        {
            RPM = 1760,
            OverallGain = 0.60f,
            BpfBaseAmp = 0.22f,
            HarmonicAmpMultipliers = new float[] { 1.0f, 1.5f, 1.0f, 0.4f },
            NoiseCenter = 850f,
            NoiseQ = 0.7f,
            NoiseAmp = 0.72f,
            HissCutoff = 3500f,
            HissAmp = 0.02f,
            GustDepth = 0.25f,
            GustFreqs = new float[] { 0.12f, 0.25f, 0.06f },
            EnableMotorVibration = false
        };

        private readonly FanSoundParams highParams = new FanSoundParams
        {
            RPM = 2000,
            OverallGain = 0.65f,
            BpfBaseAmp = 0.20f,
            HarmonicAmpMultipliers = new float[] { 1.0f, 1.0f, 0.7f, 0.8f, 0.4f },
            NoiseCenter = 1050f,
            NoiseQ = 0.9f,
            NoiseAmp = 0.75f,
            HissCutoff = 3000f,
            HissAmp = 0.04f,
            GustDepth = 0.25f,
            GustFreqs = new float[] { 0.12f, 0.28f, 0.07f },
            EnableMotorVibration = false
        };

        public MainForm()
        {
            InitializeComponent();
            SetupUI();
            startPlayer = new SoundPlayer();
            steadyPlayer = new SoundPlayer();
            startTimer = new Timer();
            startTimer.Interval = 3000; // 启动音效持续 3 秒（更接近真实）
            startTimer.Tick += StartTimer_Tick;
        }

        private void SetupUI()
        {
            this.Text = "真实风扇风声模拟器（三档调优）";
            this.Size = new Size(400, 180);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;

            btnLow = new Button { Text = "低档", Location = new Point(20, 20), Size = new Size(80, 40) };
            btnMid = new Button { Text = "中档", Location = new Point(110, 20), Size = new Size(80, 40) };
            btnHigh = new Button { Text = "高档", Location = new Point(200, 20), Size = new Size(80, 40) };
            btnStop = new Button { Text = "停止", Location = new Point(290, 20), Size = new Size(80, 40) };

            btnLow.Click += (s, e) => StartFan(lowParams);
            btnMid.Click += (s, e) => StartFan(midParams);
            btnHigh.Click += (s, e) => StartFan(highParams);
            btnStop.Click += (s, e) => StopAll();

            lblStatus = new Label { Text = "就绪", Location = new Point(20, 80), Size = new Size(360, 30) };
            lblStatus.Font = new Font("微软雅黑", 10);

            this.Controls.Add(btnLow);
            this.Controls.Add(btnMid);
            this.Controls.Add(btnHigh);
            this.Controls.Add(btnStop);
            this.Controls.Add(lblStatus);
        }

        private void StartFan(FanSoundParams param)
        {
            StopAll();
            currentRpm = param.RPM;
            currentAmp = param.OverallGain;
            isPlaying = true;

            startPlayer.Stream = GenerateFanWav(param, isStart: true);
            startPlayer.Play();
            lblStatus.Text = $"启动中 {param.RPM} RPM...";
            startTimer.Tag = param;
            startTimer.Start();
        }

        private void StartTimer_Tick(object sender, EventArgs e)
        {
            startTimer.Stop();
            startPlayer.Stop();
            if (!isPlaying) return;

            var param = startTimer.Tag as FanSoundParams;
            if (param != null)
            {
                steadyPlayer.Stream = GenerateFanWav(param, isStart: false);
                steadyPlayer.PlayLooping();
                lblStatus.Text = $"稳态 {param.RPM} RPM";
            }
        }

        private void StopAll()
        {
            isPlaying = false;
            startTimer.Stop();
            startPlayer.Stop();
            steadyPlayer.Stop();
            startPlayer.Stream = null;
            steadyPlayer.Stream = null;
            currentRpm = 0;
            currentAmp = 0;
            lblStatus.Text = "已停止";
        }

        // ============================================================
        // 合成引擎
        // ============================================================

        private MemoryStream GenerateFanWav(FanSoundParams param, bool isStart)
        {
            const int sampleRate = 44100;
            float duration = isStart ? 3.0f : 8.0f; // 稳态片段稍长
            int totalSamples = (int)(sampleRate * duration);
            short[] pcm = new short[totalSamples];
            GenerateSamples(pcm, 0, totalSamples, param, sampleRate, isStart);
            return BuildWavStream(pcm, sampleRate);
        }

        private void GenerateSamples(short[] pcmData, int offset, int count,
                                     FanSoundParams param, int sampleRate, bool isStart)
        {
            Random rnd = new Random(42);
            // 粉红噪声状态（七阶近似）
            float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
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

            // 白噪声（用于高频嘶声）
            Random rndWhite = new Random(999);
            float[] whiteNoise = new float[count];
            for (int i = 0; i < count; i++)
                whiteNoise[i] = (float)(rndWhite.NextDouble() * 2 - 1);

            // 设计二阶带通滤波器（巴特沃斯）
            double w0 = 2 * Math.PI * param.NoiseCenter / sampleRate;
            double alpha = Math.Sin(w0) / (2 * param.NoiseQ);
            double b0n = alpha / (1 + alpha);
            double b1n = 0;
            double b2n = -alpha / (1 + alpha);
            double a1n = -2 * Math.Cos(w0) / (1 + alpha);
            double a2n = (1 - alpha) / (1 + alpha);

            // 滤波器状态
            float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

            // 高频嘶声一阶高通系数
            double rc = 1.0 / (2 * Math.PI * param.HissCutoff);
            double dt = 1.0 / sampleRate;
            double alphaH = dt / (rc + dt);
            float prevH = 0;

            // 阵风相位
            float[] gustPhase = new float[param.GustFreqs.Length];
            Random rndGust = new Random(789);
            for (int i = 0; i < gustPhase.Length; i++)
                gustPhase[i] = (float)(rndGust.NextDouble() * 2 * Math.PI);

            // BPF谐波相位（固定，使谐波相干）
            int maxHarm = param.HarmonicAmpMultipliers.Length;
            float[] harmonicPhase = new float[maxHarm];
            Random rndHarm = new Random(123);
            for (int i = 0; i < maxHarm; i++)
                harmonicPhase[i] = (float)(rndHarm.NextDouble() * 2 * Math.PI);

            // 启动包络
            double windEnv = 1.0;
            double motorVib = 0.0;

            for (int i = 0; i < count; i++)
            {
                double t = i / (double)sampleRate;
                if (isStart)
                {
                    // 风声渐强（指数上升）
                    windEnv = 1 - Math.Exp(-4 * t);
                    // 电机振动（衰减）
                    motorVib = Math.Exp(-6 * t) * 0.08;
                }
                else
                {
                    windEnv = 1.0;
                    motorVib = 0.0;
                }

                // 当前转速（启动时逐渐增加）
                double currentRpm = isStart ? param.RPM * windEnv : param.RPM;
                double bpf = (currentRpm * 3) / 60.0; // 三叶片

                // ---- 1. BPF 谐波 ----
                double harmonicSum = 0;
                for (int h = 0; h < maxHarm; h++)
                {
                    double amp = param.BpfBaseAmp * param.HarmonicAmpMultipliers[h];
                    double freq = bpf * (h + 1);
                    if (freq > sampleRate / 2) break;
                    harmonicSum += amp * Math.Sin(2 * Math.PI * freq * t + harmonicPhase[h]);
                }

                // ---- 2. 宽带噪声（粉红 + 带通） ----
                float xn = pink[i];
                float yn = (float)(b0n * xn + b1n * x1 + b2n * x2 - a1n * y1 - a2n * y2);
                x2 = x1; x1 = xn;
                y2 = y1; y1 = yn;
                float filteredPink = yn;

                // ---- 3. 高频嘶声（白噪声 + 高通） ----
                float hiss = 0;
                if (param.HissAmp > 0.001f)
                {
                    float hp = (float)(alphaH * (whiteNoise[i] - prevH));
                    prevH = whiteNoise[i];
                    hiss = hp * param.HissAmp;
                }

                // ---- 4. 阵风调制 ----
                float gust = 1.0f;
                for (int g = 0; g < param.GustFreqs.Length; g++)
                {
                    gust += param.GustDepth / param.GustFreqs.Length *
                            (float)Math.Sin(2 * Math.PI * param.GustFreqs[g] * t + gustPhase[g]);
                }
                gust = Math.Max(0.3f, Math.Min(1.7f, gust));

                // ---- 5. 电机振动（启动时） ----
                double vib = 0;
                if (param.EnableMotorVibration && isStart)
                {
                    vib = motorVib * (0.7 * Math.Sin(2 * Math.PI * 50 * t) +
                                       0.3 * Math.Sin(2 * Math.PI * 100 * t + 0.5));
                }

                // ---- 合成 ----
                double sample = (filteredPink * param.NoiseAmp + harmonicSum) * gust * windEnv
                                + hiss * windEnv
                                + vib;

                sample *= param.OverallGain;

                // 限幅
                if (sample > 1.0) sample = 1.0;
                if (sample < -1.0) sample = -1.0;
                pcmData[offset + i] = (short)(sample * short.MaxValue);
            }

            // 淡入淡出（防止点击）
            int fadeSamples = (int)(0.005 * sampleRate);
            if (fadeSamples > count / 2) fadeSamples = count / 2;
            for (int i = 0; i < fadeSamples; i++)
            {
                float ratio = i / (float)fadeSamples;
                pcmData[offset + i] = (short)(pcmData[offset + i] * ratio);
                pcmData[offset + count - 1 - i] = (short)(pcmData[offset + count - 1 - i] * ratio);
            }
        }

        private MemoryStream BuildWavStream(short[] pcmData, int sampleRate)
        {
            byte[] pcmBytes = new byte[pcmData.Length * 2];
            Buffer.BlockCopy(pcmData, 0, pcmBytes, 0, pcmBytes.Length);

            MemoryStream stream = new MemoryStream();
            BinaryWriter writer = new BinaryWriter(stream);

            writer.Write(new char[] { 'R', 'I', 'F', 'F' });
            writer.Write(36 + pcmBytes.Length);
            writer.Write(new char[] { 'W', 'A', 'V', 'E' });
            writer.Write(new char[] { 'f', 'm', 't', ' ' });
            writer.Write(16);
            writer.Write((short)1);
            writer.Write((short)1);
            writer.Write(sampleRate);
            writer.Write(sampleRate * 1 * 2);
            writer.Write((short)(1 * 2));
            writer.Write((short)16);
            writer.Write(new char[] { 'd', 'a', 't', 'a' });
            writer.Write(pcmBytes.Length);
            writer.Write(pcmBytes);

            writer.Flush();
            stream.Position = 0;
            return stream;
        }

        protected override void OnFormClosed(FormClosedEventArgs e)
        {
            StopAll();
            base.OnFormClosed(e);
        }
    }

    // ============================================================
    // 参数类
    // ============================================================
    public class FanSoundParams
    {
        public int RPM { get; set; } = 1200;
        public float OverallGain { get; set; } = 0.6f;

        // BPF谐波
        public float BpfBaseAmp { get; set; } = 0.18f;
        public float[] HarmonicAmpMultipliers { get; set; } = new float[] { 1.0f, 1.1f, 0.6f, 0.3f, 0.1f };

        // 宽带噪声（粉红+带通）
        public float NoiseCenter { get; set; } = 850f;
        public float NoiseQ { get; set; } = 0.75f;
        public float NoiseAmp { get; set; } = 0.72f;

        // 高频嘶声
        public float HissCutoff { get; set; } = 3500f;
        public float HissAmp { get; set; } = 0.03f;

        // 阵风
        public float GustDepth { get; set; } = 0.25f;
        public float[] GustFreqs { get; set; } = new float[] { 0.12f, 0.28f, 0.07f };

        // 电机振动（仅在启动时启用）
        public bool EnableMotorVibration { get; set; } = false;
    }

    // ============================================================
    // 程序入口
    // ============================================================
    internal static class Program
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
        }
    }
}