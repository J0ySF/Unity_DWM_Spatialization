using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Assertions;

public static class AudioManager
{
    private static class NativePlugin
    {
        [DllImport("Unity_DWM_Spatializer")]
        public static extern int GetSampleRate();

        [DllImport("Unity_DWM_Spatializer")]
        public static extern int GetBufferSize();
    }

    // Important: must be called as soon as possible in order not to interfere audio sources in scenes
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSplashScreen)]
    private static void OnBeforeSplashScreen()
    {
        // Set up the audio configuration
        var c = AudioSettings.GetConfiguration();
        c.speakerMode = AudioSpeakerMode.Stereo;
        c.dspBufferSize = NativePlugin.GetBufferSize();
        c.sampleRate = NativePlugin.GetSampleRate();
        AudioSettings.Reset(c);

        // Check that the mandatory parts have been applied as expected
        var c1 = AudioSettings.GetConfiguration();
        Assert.AreEqual(c.speakerMode, c1.speakerMode);
        Assert.AreEqual(c.dspBufferSize, c1.dspBufferSize);
        Assert.AreEqual(c.sampleRate, c1.sampleRate);
    }
}