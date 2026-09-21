using System;
using NAudio.Wave;

namespace FanSound
{
    /// <summary>风扇音频播放器（基于 NAudio）</summary>
    public class FanSoundPlayer : IDisposable
    {
        private WaveOutEvent outputDevice;
        private BufferedWaveProvider waveProvider;
        private FanSoundParams currentParams;
        private bool isPlaying = false;
        private readonly object lockObj = new object();
        private System.Threading.Timer replenishTimer;
        private const int SAMPLE_RATE = 44100;
        private const float DURATION = 8.0f;
        private bool disposed = false;

        public FanSoundPlayer()
        {
            outputDevice = new WaveOutEvent();
            outputDevice.PlaybackStopped += OnPlaybackStopped;
        }

        /// <summary>播放指定档位</summary>
        public void Play(FanSoundParams param)
        {
            lock (lockObj)
            {
                if (disposed) return;

                StopInternal();

                currentParams = param;
                isPlaying = true;

                // 初始化缓冲区
                var waveFormat = new WaveFormat(SAMPLE_RATE, 16, 1);
                waveProvider = new BufferedWaveProvider(waveFormat);
                waveProvider.DiscardOnBufferOverflow = true;
                waveProvider.BufferDuration = TimeSpan.FromSeconds(DURATION * 3);

                // 填充初始数据
                RefillBuffer();

                outputDevice.Init(waveProvider);
                outputDevice.Play();

                // 启动定时器定期补充数据（每 2 秒补充一次）
                replenishTimer = new System.Threading.Timer(
                    _ => RefillBuffer(),
                    null,
                    2000,
                    2000);
            }
        }

        /// <summary>停止播放</summary>
        public void Stop()
        {
            lock (lockObj)
            {
                StopInternal();
            }
        }

        private void StopInternal()
        {
            isPlaying = false;
            if (replenishTimer != null)
            {
                replenishTimer.Dispose();
                replenishTimer = null;
            }
            if (outputDevice != null && outputDevice.PlaybackState == PlaybackState.Playing)
            {
                outputDevice.Stop();
            }
            if (waveProvider != null)
            {
                waveProvider.ClearBuffer();
                waveProvider = null;
            }
        }

        private void RefillBuffer()
        {
            lock (lockObj)
            {
                if (!isPlaying || disposed || waveProvider == null || currentParams == null)
                    return;

                int totalSamples = (int)(SAMPLE_RATE * DURATION);
                short[] pcm = new short[totalSamples];
                FanSoundSynthesizer.GenerateSamples(pcm, 0, totalSamples, currentParams, SAMPLE_RATE, false);

                byte[] pcmBytes = new byte[pcm.Length * 2];
                Buffer.BlockCopy(pcm, 0, pcmBytes, 0, pcmBytes.Length);

                try
                {
                    waveProvider.AddSamples(pcmBytes, 0, pcmBytes.Length);
                }
                catch (Exception)
                {
                    // 缓冲区可能已满，忽略
                }
            }
        }

        private void OnPlaybackStopped(object sender, StoppedEventArgs e)
        {
            // 如果仍在播放状态，补充数据并继续
            if (isPlaying && waveProvider != null)
            {
                lock (lockObj)
                {
                    if (isPlaying && !disposed && waveProvider != null && currentParams != null)
                    {
                        RefillBuffer();
                        try
                        {
                            outputDevice?.Play();
                        }
                        catch (Exception) { }
                    }
                }
            }
        }

        public void Dispose()
        {
            if (disposed) return;
            disposed = true;
            StopInternal();
            if (outputDevice != null)
            {
                outputDevice.Dispose();
                outputDevice = null;
            }
            GC.SuppressFinalize(this);
        }
    }
}