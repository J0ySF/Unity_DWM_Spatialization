using System.Runtime.InteropServices;
using UnityEngine;

public class DebugListeningSetup : MonoBehaviour
{
    private static class NativePlugin
    {
        [DllImport("Unity_DWM_Spatializer")]
        public static extern float GetMeshWidth();

        [DllImport("Unity_DWM_Spatializer")]
        public static extern float GetMeshHeight();

        [DllImport("Unity_DWM_Spatializer")]
        public static extern float GetMeshDepth();

        [DllImport("Unity_DWM_Spatializer")]
        public static extern int GetMeshSurfaceSamplingCountX();

        [DllImport("Unity_DWM_Spatializer")]
        public static extern int GetMeshSurfaceSamplingCountZ();

        [DllImport("Unity_DWM_Spatializer")]
        public static extern float GetEarsDistance();
    }

    private static readonly float CachedMeshWidth = NativePlugin.GetMeshWidth();
    private static readonly float CachedMeshHeight = NativePlugin.GetMeshHeight();
    private static readonly float CachedMeshDepth = NativePlugin.GetMeshDepth();
    private static readonly int CachedMeshSurfaceSamplingCountX = NativePlugin.GetMeshSurfaceSamplingCountX();
    private static readonly int CachedMeshSurfaceSamplingCountZ = NativePlugin.GetMeshSurfaceSamplingCountZ();
    private static readonly float CachedEarsDistance = NativePlugin.GetEarsDistance();

    // Visualize the data as used in the native plugin
    private void OnDrawGizmos()
    {
        var m = Matrix4x4.TRS(transform.position, transform.rotation, Vector3.one).inverse;

        var position = new Vector3(
            -(m[0] * m[12] + m[1] * m[13] + m[2] * m[14]),
            -(m[4] * m[12] + m[5] * m[13] + m[6] * m[14]),
            -(m[8] * m[12] + m[9] * m[13] + m[10] * m[14])
        );

        var right = new Vector3(m[0], m[4], m[8]);
        var forward = new Vector3(m[2], m[6], m[10]);

        var leftPosition = position - right * CachedEarsDistance / 2.0f;
        var rightPosition = position + right * CachedEarsDistance / 2.0f;

        for (var z = 0; z < CachedMeshSurfaceSamplingCountZ; z++)
        {
            for (var x = 0; x < CachedMeshSurfaceSamplingCountX; x++)
            {
                var dest = new Vector3(x * CachedMeshWidth / (CachedMeshSurfaceSamplingCountX - 1.0f),
                    CachedMeshHeight,
                    z * CachedMeshDepth / (CachedMeshSurfaceSamplingCountZ - 1.0f));
                Gizmos.color = Color.Lerp(new Color32(0, 0, 0, 0), new Color32(255, 255, 0, 255),
                    1 / Mathf.Pow(Vector3.Distance(leftPosition, dest) + 1, 2));
                Gizmos.DrawLine(leftPosition, dest);
                Gizmos.color = Color.Lerp(new Color32(0, 0, 0, 0), new Color32(255, 255, 0, 255),
                    1 / Mathf.Pow(Vector3.Distance(rightPosition, dest) + 1, 2));
                Gizmos.DrawLine(rightPosition, dest);
            }
        }

        Gizmos.color = Color.black;
        Gizmos.DrawWireSphere(position, CachedEarsDistance / 2.0f);
        Gizmos.color = Color.blue;
        Gizmos.DrawWireSphere(leftPosition, CachedEarsDistance / 4.0f);
        Gizmos.color = Color.red;
        Gizmos.DrawWireSphere(rightPosition, CachedEarsDistance / 4.0f);
        Gizmos.color = Color.green;
        Gizmos.DrawLine(position, position + forward * CachedEarsDistance);
    }
}