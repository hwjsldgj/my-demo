namespace FanSound
{
    /// <summary>风扇声音合成参数</summary>
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

        // 三档预置
        public static FanSoundParams Low => new FanSoundParams
        {
            RPM = 1520,
            OverallGain = 0.55f,
            BpfBaseAmp = 0.04f,
            HarmonicAmpMultipliers = new float[] { 1.0f, 0.65f, 0.65f, 0.3f },
            NoiseCenter = 650f,
            NoiseQ = 0.8f,
            NoiseAmp = 0.90f,
            HissCutoff = 4000f,
            HissAmp = 0.0f,
            GustDepth = 0.35f,
            GustFreqs = new float[] { 0.10f, 0.22f, 0.05f },
            EnableMotorVibration = false
        };

        public static FanSoundParams Mid => new FanSoundParams
        {
            RPM = 1760,
            OverallGain = 0.60f,
            BpfBaseAmp = 0.05f,
            HarmonicAmpMultipliers = new float[] { 1.0f, 1.5f, 1.0f, 0.4f },
            NoiseCenter = 850f,
            NoiseQ = 0.7f,
            NoiseAmp = 0.92f,
            HissCutoff = 3500f,
            HissAmp = 0.02f,
            GustDepth = 0.35f,
            GustFreqs = new float[] { 0.12f, 0.25f, 0.06f },
            EnableMotorVibration = false
        };

        public static FanSoundParams High => new FanSoundParams
        {
            RPM = 2000,
            OverallGain = 0.65f,
            BpfBaseAmp = 0.06f,
            HarmonicAmpMultipliers = new float[] { 1.0f, 1.0f, 0.7f, 0.8f, 0.4f },
            NoiseCenter = 1050f,
            NoiseQ = 0.9f,
            NoiseAmp = 0.95f,
            HissCutoff = 3000f,
            HissAmp = 0.04f,
            GustDepth = 0.35f,
            GustFreqs = new float[] { 0.12f, 0.28f, 0.07f },
            EnableMotorVibration = false
        };
    }
}