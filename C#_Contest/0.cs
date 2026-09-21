using System;
using System.Drawing;
using System.Windows.Forms;
using NAudio.Wave;

namespace NAudioTest
{
    public class MainForm : Form
    {
        private WaveOut waveOut;
        private BufferedWaveProvider waveProvider;
        private bool isPlaying = false;
        private float frequency = 440f;
        private Timer timer;

        private Button btnStart, btnStop, btnUp, btnDown;
        private Label lblStatus;

        public MainForm()
        {
            this.Text = "NAudio 实时合成测试";
            this.Size = new Size(380, 200);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;

            btnStart = new Button { Text = "启动", Location = new Point(20, 20), Size = new Size(80, 40) };
            btnStart.Click += (s, e) => StartAudio();

            btnStop = new Button { Text = "停止", Location = new Point(120, 20), Size = new Size(80, 40) };
            btnStop.Click += (s, e) => StopAudio();

            btnUp = new Button { Text = "升频", Location = new Point(220, 20), Size = new Size(60, 40) };
            btnUp.Click += (s, e) => { frequency += 100; lblStatus.Text = "频率：" + frequency + " Hz"; };

            btnDown = new Button { Text = "降频", Location = new Point(290, 20), Size = new Size(60, 40) };
            btnDown.Click += (s, e) => { frequency -= 100; if (frequency < 100) frequency = 100; lblStatus.Text = "频率：" + frequency + " Hz"; };

            lblStatus = new Label { Text = "频率：440 Hz", Location = new Point(20, 80), Size = new Size(300, 25) };

            this.Controls.Add(btnStart);
            this.Controls.Add(btnStop);
            this.Controls.Add(btnUp);
            this.Controls.Add(btnDown);
            this.Controls.Add(lblStatus);

            // 初始化 NAudio（使用 WaveOut）
            waveOut = new WaveOut();
            var waveFormat = new WaveFormat(44100, 16, 1);
            waveProvider = new BufferedWaveProvider(waveFormat);
            waveProvider.DiscardOnBufferOverflow = true;
            waveOut.Init(waveProvider);
        }

        private void StartAudio()
        {
            if (isPlaying) return;
            isPlaying = true;
            waveOut.Play();
            timer = new Timer();
            timer.Interval = 50;
            timer.Tick += (s, e) => GenerateAudio();
            timer.Start();
            lblStatus.Text = "播放中...";
        }

        private void StopAudio()
        {
            if (!isPlaying) return;
            isPlaying = false;
            waveOut.Stop();
            if (timer != null)
            {
                timer.Stop();
                timer.Dispose();
                timer = null;
            }
            lblStatus.Text = "已停止";
        }

        private void GenerateAudio()
        {
            if (!isPlaying) return;
            int sampleRate = 44100;
            int durationMs = 50;
            int sampleCount = (int)(sampleRate * (durationMs / 1000.0));
            short[] samples = new short[sampleCount];
            for (int i = 0; i < sampleCount; i++)
            {
                double time = i / (double)sampleRate;
                double sample = Math.Sin(2 * Math.PI * frequency * time);
                samples[i] = (short)(sample * 0.3 * short.MaxValue);
            }
            byte[] byteBuffer = new byte[sampleCount * 2];
            Buffer.BlockCopy(samples, 0, byteBuffer, 0, byteBuffer.Length);
            waveProvider.AddSamples(byteBuffer, 0, byteBuffer.Length);
        }

        protected override void OnFormClosed(FormClosedEventArgs e)
        {
            StopAudio();
            if (waveOut != null)
            {
                waveOut.Dispose();
            }
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