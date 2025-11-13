using System.Runtime.InteropServices;
using UnityEngine;

// ReSharper disable once InconsistentNaming
public class DWM_AudioSource : MonoBehaviour
{
    private static class NativePlugin
    {
        [DllImport("Unity_DWM_Spatializer")]
        // ReSharper disable InconsistentNaming
        public static extern int WriteSource(int index, float p_x, float p_y, float p_z,
            [In, Out] float[] buffer, int num_channels);
        // ReSharper restore InconsistentNaming
    }

    private Vector3 _cachedPosition;

    private void Start()
    {
        _cachedPosition = transform.position;
    }

    private void LateUpdate()
    {
        _cachedPosition = transform.position;
    }

    [SerializeField] private int index;

    public void SetIndex(int i) => this.index = i;
    
    private void OnAudioFilterRead(float[] data, int channels)
    {
        // TODO: _cachedPosition is not atomic, can it be modified by LateUpdate at the same time as OnAudioFilterRead runs?
        NativePlugin.WriteSource(index, _cachedPosition.x, _cachedPosition.y, _cachedPosition.z, data, channels);
    }
}