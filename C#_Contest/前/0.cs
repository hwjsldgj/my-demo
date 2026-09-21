using System;
using System.Drawing;
using System.IO;
using System.Media;
using System.Windows.Forms;

namespace FanSoundSmooth
{
    public class MainForm : Form
    {
        private Button btnLow, btnMid, btnHigh, btnStop;
        private Label lblStatus;
        private SoundPlayer startPlayer, steadyPlayer;
        private Timer startTimer;
        private bool isPlaying = false;
        private int currentRpm = 0;
        private float currentAmp = 0;

        public MainForm()
        {
            this.Text = "风扇切换 (启动音效)";
            this.Size = new Size(380, 200);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;

            btnLow = new Button { Text = "低档", Location = new Point(20, 20), Size = new Size(90, 40) };
            btnLow.Click += (s, e) => StartFan(800, 0.45f);

            btnMid = new Button { Text = "中档", Location = new Point(120, 20), Size = new Size(90, 40) };
            btnMid.Click += (s, e) => StartFan(1200, 0.58f);

            btnHigh = new Button { Text = "高档", Location = new Point(220, 20), Size = new Size(90, 40) };
            btnHigh.Click += (s, e) => StartFan(1800, 0.52f);

            btnStop = new Button { Text = "停止", Location = new Point(20, 80), Size = new Size(90, 40) };
            btnStop.Click += (s, e) => StopAll();

            lblStatus = new Label { Text = "点击切换", Location = new Point(130, 90), Size = new Size(220, 25) };

            this.Controls.Add(btnLow);
            this.Controls.Add(btnMid);
            this.Controls.Add(btnHigh);
            this.Controls.Add(btnStop);
            this.Controls.Add(lblStatus);

            startPlayer = new SoundPlayer();
            steadyPlayer = new SoundPlayer();
            startTimer = new Timer();
            startTimer.Interval = 2500; // 启动音效时长
            startTimer.Tick += (s, e) =>
            {
                startTimer.Stop();
                startPlayer.Stop();
                if (isPlaying && currentRpm > 0)
                {
                    steadyPlayer.Stream = GenerateSteadyWav(currentRpm, currentAmp);
                    steadyPlayer.PlayLooping();
                    lblStatus.Text = string.Format("稳态 {0} RPM", currentRpm);
                }
            };
        }

        private void StartFan(int rpm, float amp)
        {
            StopAll();
            currentRpm = rpm;
            currentAmp = amp;
            isPlaying = true;

            startPlayer.Stream = GenerateStartWav(rpm, amp);
            startPlayer.Play();
            lblStatus.Text = string.Format("启动中 {0} RPM...", rpm);
            startTimer.Start();
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
        // 合成引擎（高品质）
        // ============================================================

        private MemoryStream GenerateStartWav(int rpm, float amp)
        {
            const int sampleRate = 44100;
            const float duration = 2.5f;
            int totalSamples = (int)(sampleRate * duration);
            short[] pcmData = new short[totalSamples];
            GenerateSamples(pcmData, 0, totalSamples, rpm, amp, sampleRate, true);
            return BuildWavStream(pcmData, sampleRate);
        }

        private MemoryStream GenerateSteadyWav(int rpm, float amp)
        {
            const int sampleRate = 44100;
            const float duration = 4.0f;
            int totalSamples = (int)(sampleRate * duration);
            short[] pcmData = new short[totalSamples];
            GenerateSamples(pcmData, 0, totalSamples, rpm, amp, sampleRate, false);
            return BuildWavStream(pcmData, sampleRate);
        }

        private void GenerateSamples(short[] pcmData, int offset, int count, int rpm, float amp, int sampleRate, bool isStart)
        {
            Random rnd = new Random(42);
            float[] pinkNoise = new float[count];
            float b0 = 0f, b1 = 0f, b2 = 0f, b3 = 0f, b4 = 0f, b5 = 0f, b6 = 0f;
            for (int i = 0; i < count; i++)
            {
                float white = (float)(rnd.NextDouble() * 2 - 1);
                b0 = 0.99886f * b0 + white * 0.0555179f;
                b1 = 0.99332f * b1 + white * 0.0750759f;
                b2 = 0.96900f * b2 + white * 0.1538520f;
                b3 = 0.86650f * b3 + white * 0.3104856f;
                b4 = 0.55000f * b4 + white * 0.5329522f;
                b5 = -0.7616f * b5 - white * 0.0168980f;
                pinkNoise[i] = b0 + b1 + b2 + b3 + b4 + b5 + white * 0.5362f;
                pinkNoise[i] *= 0.11f;
                if (pinkNoise[i] > 1.0f) pinkNoise[i] = 1.0f;
                if (pinkNoise[i] < -1.0f) pinkNoise[i] = -1.0f;
            }

            // 固定低频振动参数
            Random rndVib = new Random(456);
            double vibFreq1 = 50 + 10 * rndVib.NextDouble();
            double vibFreq2 = 100 + 20 * rndVib.NextDouble();
            double vibPhase1 = rndVib.NextDouble() * 2 * Math.PI;
            double vibPhase2 = rndVib.NextDouble() * 2 * Math.PI;

            // 阵风参数
            Random rndGust = new Random(789);
            double gustSeed = rndGust.NextDouble();

            // 叶片调制相位
            Random rndMod = new Random(123);
            double[] modPhases = new double[3];
            for (int i = 0; i < 3; i++) modPhases[i] = rndMod.NextDouble() * 2 * Math.PI;

            float prevFiltered = 0f;
            Random rndHi = new Random(999);
            float prevHi = 0f;

            int bladeCount = 3;

            for (int i = 0; i < count; i++)
            {
                double t = i / (double)sampleRate;
                double progress = isStart ? t / 2.5 : 1.0; // 启动时从0到1，稳态为1

                // 启动包络：电机衰减，风声渐强
                double motorEnv = isStart ? Math.Exp(-5 * t) : 0.0;
                double windEnv = isStart ? 1 - Math.Exp(-3 * t) : 1.0;

                // 当前转速和幅度：启动时逐渐上升，稳态恒定
                double currentRpm = isStart ? rpm * windEnv : rpm;
                double currentAmp = isStart ? amp * windEnv : amp;
                int curRpm = (int)currentRpm;
                float curAmp = (float)currentAmp;

                // 叶片通过频率
                double bpf = (curRpm * bladeCount) / 60.0;

                // 叶片调制
                double[] modAmps = { 0.18, 0.08, 0.04 };
                double bladeMod = 0.5;
                for (int h = 0; h < 3; h++)
                {
                    double freq = bpf * (h + 1);
                    bladeMod += modAmps[h] * Math.Sin(2 * Math.PI * freq * t + modPhases[h]);
                }
                bladeMod = Math.Max(0.25, Math.Min(0.75, bladeMod));

                // 阵风
                double gust = 0.6 + 0.4 * (0.5 + 0.5 * Math.Sin(2 * Math.PI * 0.15 * t + gustSeed * 100));

                // 低通滤波
                double cutoff = 400 + (curRpm - 800) * 0.8;
                if (cutoff < 400) cutoff = 400;
                if (cutoff > 1800) cutoff = 1800;
                double modCut = 0.8 + 0.2 * Math.Sin(2 * Math.PI * 0.3 * t + 0.5);
                double currentCutoff = cutoff * modCut;
                float alpha = 1.0f - (float)Math.Exp(-2 * Math.PI * currentCutoff / sampleRate);
                float filteredNoise = alpha * pinkNoise[i] + (1 - alpha) * prevFiltered;
                prevFiltered = filteredNoise;

                double noiseAmp = 0.7 * bladeMod * gust;
                double rpmNoiseFactor = 0.8 + 0.2 * Math.Pow(curRpm / 800.0, 0.6);
                if (rpmNoiseFactor > 1.2) rpmNoiseFactor = 1.2;
                noiseAmp *= rpmNoiseFactor;

                double sample = filteredNoise * noiseAmp * curAmp * 0.8;

                // 高频细节
                if (curRpm > 1500)
                {
                    float hiNoise = (float)(rndHi.NextDouble() * 2 - 1);
                    float hiPass = hiNoise - 0.85f * prevHi;
                    prevHi = hiNoise;
                    double hiAmp = 0.02 * (curRpm - 1500) / 300.0;
                    sample += hiPass * hiAmp * 0.3;
                }

                // 电机嗡嗡（仅在启动时出现）
                double vibAmp = 0.08 * motorEnv;
                sample += vibAmp * (0.12 * Math.Sin(2 * Math.PI * vibFreq1 * t + vibPhase1)
                                   + 0.08 * Math.Sin(2 * Math.PI * vibFreq2 * t + vibPhase2));

                // 应用风声渐强（稳态 windEnv=1）
                sample *= windEnv;

                // 限幅
                if (sample > 1.0) sample = 1.0;
                if (sample < -1.0) sample = -1.0;
                pcmData[offset + i] = (short)(sample * short.MaxValue);
            }

            // 首尾短淡化（0.005秒）
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