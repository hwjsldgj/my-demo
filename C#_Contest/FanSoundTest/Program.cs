using System;
using System.IO;
using NAudio.Wave;
using NAudio.Wave.SampleProviders;

namespace FanSoundTest
{
    class Program
    {
        private static WaveOutEvent? outputDevice;
        private static BufferedWaveProvider? waveProvider;
        private static FanSoundParams? currentParams;
        private static bool isPlaying = false;
        private static readonly object lockObj = new object();

        // 三档调优参数（同前）
        private static readonly FanSoundParams lowParams = new FanSoundParams
{
    RPM = 1520,
    OverallGain = 0.55f,
    BpfBaseAmp = 0.04f,                 // 原 0.18 → 降至 0.04
    HarmonicAmpMultipliers = new float[] { 1.0f, 0.65f, 0.65f, 0.3f },
    NoiseCenter = 650f,
    NoiseQ = 0.8f,
    NoiseAmp = 0.90f,                   // 原 0.65 → 提高至 0.90
    HissCutoff = 4000f,
    HissAmp = 0.0f,
    GustDepth = 0.35f,                  // 原 0.30 → 增至 0.35
    GustFreqs = new float[] { 0.10f, 0.22f, 0.05f },
    EnableMotorVibration = false
};

private static readonly FanSoundParams midParams = new FanSoundParams
{
    RPM = 1760,
    OverallGain = 0.60f,
    BpfBaseAmp = 0.05f,                 // 原 0.22 → 0.05
    HarmonicAmpMultipliers = new float[] { 1.0f, 1.5f, 1.0f, 0.4f },
    NoiseCenter = 850f,
    NoiseQ = 0.7f,
    NoiseAmp = 0.92f,                   // 原 0.72 → 0.92
    HissCutoff = 3500f,
    HissAmp = 0.02f,
    GustDepth = 0.35f,                  // 0.25 → 0.35
    GustFreqs = new float[] { 0.12f, 0.25f, 0.06f },
    EnableMotorVibration = false
};

private static readonly FanSoundParams highParams = new FanSoundParams
{
    RPM = 2000,
    OverallGain = 0.65f,
    BpfBaseAmp = 0.06f,                 // 原 0.20 → 0.06
    HarmonicAmpMultipliers = new float[] { 1.0f, 1.0f, 0.7f, 0.8f, 0.4f },
    NoiseCenter = 1050f,
    NoiseQ = 0.9f,
    NoiseAmp = 0.95f,                   // 原 0.75 → 0.95
    HissCutoff = 3000f,
    HissAmp = 0.04f,
    GustDepth = 0.35f,
    GustFreqs = new float[] { 0.12f, 0.28f, 0.07f },
    EnableMotorVibration = false
};

        static void Main(string[] args)
        {
            Console.WriteLine("风扇风声模拟器 (NAudio)");
            Console.WriteLine("按键: 1-低档  2-中档  3-高档  0-停止  Q-退出");
            Console.WriteLine("正在播放中档...");

            outputDevice = new WaveOutEvent();
            outputDevice.PlaybackStopped += OnPlaybackStopped;

            PlayParams(midParams);

            while (true)
            {
                var key = Console.ReadKey(true).KeyChar;
                switch (key)
                {
                    case '1': PlayParams(lowParams); break;
                    case '2': PlayParams(midParams); break;
                    case '3': PlayParams(highParams); break;
                    case '0': StopPlayback(); break;
                    case 'q':
                    case 'Q': StopPlayback(); return;
                    default: break;
                }
            }
        }

        private static void PlayParams(FanSoundParams param)
        {
            lock (lockObj)
            {
                StopPlaybackInternal();

                const int sampleRate = 44100;
                const float duration = 8.0f;
                int totalSamples = (int)(sampleRate * duration);
                short[] pcm = new short[totalSamples];
                GenerateSamples(pcm, 0, totalSamples, param, sampleRate, isStart: false);

                byte[] pcmBytes = new byte[pcm.Length * 2];
                Buffer.BlockCopy(pcm, 0, pcmBytes, 0, pcmBytes.Length);

                waveProvider = new BufferedWaveProvider(new WaveFormat(sampleRate, 16, 1));
                // 关键修复：允许溢出时丢弃旧数据，避免 Buffer full 异常
                waveProvider.DiscardOnBufferOverflow = true;
                waveProvider.BufferDuration = TimeSpan.FromSeconds(duration * 3); // 扩大缓冲区
                waveProvider.AddSamples(pcmBytes, 0, pcmBytes.Length);

                outputDevice!.Init(waveProvider);
                outputDevice.Play();
                isPlaying = true;
                currentParams = param;
                Console.WriteLine($"切换到 {param.RPM} RPM，持续播放...");
            }
        }

        private static void StopPlaybackInternal()
        {
            if (outputDevice != null && isPlaying)
            {
                outputDevice.Stop();
                isPlaying = false;
            }
        }

        private static void StopPlayback()
        {
            lock (lockObj)
            {
                StopPlaybackInternal();
                Console.WriteLine("已停止");
            }
        }

        private static void OnPlaybackStopped(object? sender, StoppedEventArgs e)
        {
            if (isPlaying && waveProvider != null)
            {
                lock (lockObj)
                {
                    if (isPlaying && currentParams != null)
                    {
                        const int sampleRate = 44100;
                        const float duration = 8.0f;
                        int totalSamples = (int)(sampleRate * duration);
                        short[] pcm = new short[totalSamples];
                        GenerateSamples(pcm, 0, totalSamples, currentParams, sampleRate, isStart: false);
                        byte[] pcmBytes = new byte[pcm.Length * 2];
                        Buffer.BlockCopy(pcm, 0, pcmBytes, 0, pcmBytes.Length);
                        waveProvider.AddSamples(pcmBytes, 0, pcmBytes.Length);
                        outputDevice?.Play();
                    }
                }
            }
        }

        // ========== 合成引擎（与之前相同） ==========
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
            for (int i = 0; i < count; i++)
                whiteNoise[i] = (float)(rndWhite.NextDouble() * 2 - 1);

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
            for (int i = 0; i < gustPhase.Length; i++)
                gustPhase[i] = (float)(rndGust.NextDouble() * 2 * Math.PI);

            int maxHarm = param.HarmonicAmpMultipliers.Length;
            float[] harmonicPhase = new float[maxHarm];
            Random rndHarm = new Random(123);
            for (int i = 0; i < maxHarm; i++)
                harmonicPhase[i] = (float)(rndHarm.NextDouble() * 2 * Math.PI);

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

                // ---- 1. BPF 谐波（幅度调制+频率抖动） ----
double harmonicSum = 0;
// 计算一个共同的慢变调制
double harmonicMod = 0.6 + 0.4 * (0.5 + 0.5 * Math.Sin(2 * Math.PI * 0.25 * t + gustPhase[0]));
// 也可用第二个调制
double harmonicMod2 = 0.7 + 0.3 * Math.Sin(2 * Math.PI * 0.11 * t + gustPhase[1]);
double finalMod = harmonicMod * harmonicMod2;

for (int h = 0; h < maxHarm; h++)
{
    double amp = param.BpfBaseAmp * param.HarmonicAmpMultipliers[h] * finalMod;
    double freq = bpf * (h + 1);
    // 添加微小的频率抖动（±0.5 Hz）
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
                    vib = motorVib * (0.7 * Math.Sin(2 * Math.PI * 50 * t) +
                                       0.3 * Math.Sin(2 * Math.PI * 100 * t + 0.5));
                }

                double sample = (filteredPink * param.NoiseAmp + harmonicSum) * gust * windEnv
                                + hiss * windEnv
                                + vib;

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
    }

    public class FanSoundParams
    {
        public int RPM { get; set; } = 1200;
        public float OverallGain { get; set; } = 0.6f;
        public float BpfBaseAmp { get; set; } = 0.18f;
        public float[] HarmonicAmpMultipliers { get; set; } = new float[] { 1.0f, 1.1f, 0.6f, 0.3f, 0.1f };
        public float NoiseCenter { get; set; } = 850f;
        public float NoiseQ { get; set; } = 0.75f;
        public float NoiseAmp { get; set; } = 0.72f;
        public float HissCutoff { get; set; } = 3500f;
        public float HissAmp { get; set; } = 0.03f;
        public float GustDepth { get; set; } = 0.25f;
        public float[] GustFreqs { get; set; } = new float[] { 0.12f, 0.28f, 0.07f };
        public bool EnableMotorVibration { get; set; } = false;
    }
}