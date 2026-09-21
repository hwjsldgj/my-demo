using System;

namespace FanSound
{
    /// <summary>风声合成引擎</summary>
    public static class FanSoundSynthesizer
    {
        public static void GenerateSamples(short[] pcmData, int offset, int count,
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
}