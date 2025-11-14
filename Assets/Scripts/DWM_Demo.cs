using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.InputSystem;

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

    [SerializeField] private GameObject[] tests;
    [SerializeField] private GameObject test3SourcePrefab;
    [SerializeField] private GameObject test4SourcePrefab;

    private int _selectedTest;

    private void PreviousTest()
    {
        tests[_selectedTest].SetActive(false);
        _selectedTest = _selectedTest - 1 < 0 ? tests.Length - 1 : _selectedTest - 1;
        tests[_selectedTest].SetActive(true);
    }

    private void NextTest()
    {
        tests[_selectedTest].SetActive(false);
        _selectedTest = (_selectedTest + 1) % tests.Length;
        tests[_selectedTest].SetActive(true);
    }

    private void Start()
    {
        var meshSize = new Vector3(NativePlugin.GetMeshWidth(), NativePlugin.GetMeshHeight(),
            NativePlugin.GetMeshDepth());
        transform.localScale = meshSize;

        foreach (var test in tests) test.SetActive(false);
        tests[0].SetActive(true);

        for (var i = 0; i < NativePlugin.GetMaxSourceCount(); i++)
        {
            var newSource = Instantiate(test3SourcePrefab, tests[3].transform, true);
            newSource.transform.localPosition = new Vector3(Random.value, Random.value, Random.value);
            newSource.GetComponent<DWM_AudioSource>().SetIndex(i);
            var s = newSource.GetComponent<AudioSource>();
            s.pitch = Mathf.Pow(2, (Random.Range(1, 36)) / 12.0f);
        }

        for (var i = 0; i < NativePlugin.GetMaxSourceCount(); i++)
        {
            var newSource = Instantiate(test4SourcePrefab, tests[4].transform, true);
            newSource.GetComponent<DWM_AudioSource>().SetIndex(i);
        }
    }

    private void Update()
    {
        if (Keyboard.current.leftArrowKey.wasPressedThisFrame) PreviousTest();
        else if (Keyboard.current.rightArrowKey.wasPressedThisFrame) NextTest();
    }
}