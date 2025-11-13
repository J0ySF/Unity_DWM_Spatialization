using System.Runtime.InteropServices;
using UnityEngine;

// ReSharper disable once InconsistentNaming
public class DWM_Demo : MonoBehaviour
{
    private static class NativePlugin
    {
        [DllImport("Unity_DWM_Spatializer")]
        public static extern int GetMaxSourceCount();

        [DllImport("Unity_DWM_Spatializer")]
        public static extern float GetMeshWidth();

        [DllImport("Unity_DWM_Spatializer")]
        public static extern float GetMeshHeight();

        [DllImport("Unity_DWM_Spatializer")]
        public static extern float GetMeshDepth();
    }

    [SerializeField] private GameObject source0;
    [SerializeField] private AudioClip instantiatedClip;
    [SerializeField] private Transform model;
    
    private void Start()
    {
        var meshSize = new Vector3(NativePlugin.GetMeshWidth(), NativePlugin.GetMeshHeight(),
            NativePlugin.GetMeshDepth());
        model.transform.localScale = meshSize;
        for (var i = 1; i < NativePlugin.GetMaxSourceCount(); i++)
        {
            var newSource = Instantiate(source0, transform, true);
            newSource.transform.localPosition = new Vector3(Random.value * meshSize.x, Random.value * meshSize.y,
                Random.value * meshSize.z);
            newSource.GetComponent<DWM_AudioSource>().SetIndex(i);
            var s = newSource.GetComponent<AudioSource>();
            s.clip = instantiatedClip;
            s.pitch = Mathf.Pow(2, (Random.Range(1, 36))/12.0f);
            s.Play();
        }
    }
}