using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Assertions;
using UnityEngine.Audio;

// ReSharper disable once InconsistentNaming
public class DWM_AudioManager : MonoBehaviour
{
    private static class NativePlugin
    {
        [DllImport("Unity_DWM_Spatializer")]
        public static extern int GetSampleRate();

        [DllImport("Unity_DWM_Spatializer")]
        public static extern int GetBufferSize();

        [DllImport("Unity_DWM_Spatializer")]
        public static extern int GetMaxSourceCount();
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

    [SerializeField] private AudioMixer dwmMixer;
    [SerializeField] private AudioSource[] dwmSources;

    private AudioMixerGroup[] dwmMixerGroups;

    private void Start()
    {
        var dwmMixerGroupRaw = dwmMixer.FindMatchingGroups("");

        // Limit to the max plugin supported source count
        // The first entry refers to the "DWM" group, not the children, so it is ignored
        var pluginSourceCount = NativePlugin.GetMaxSourceCount();
        var effectiveSourceCount = Math.Min(dwmMixerGroupRaw.Length - 1, pluginSourceCount);
        dwmMixerGroups = new AudioMixerGroup[effectiveSourceCount];
        for (var i = 0; i < effectiveSourceCount; i++)
        {
            dwmMixerGroups[i] = dwmMixerGroupRaw[i + 1];
        }

        for (var i = 0; i < dwmSources.Length; i++)
        {
            if (i < effectiveSourceCount)
                dwmSources[i].outputAudioMixerGroup = dwmMixerGroups[i];
            else
                dwmSources[i].mute = true;
        }
    }
}