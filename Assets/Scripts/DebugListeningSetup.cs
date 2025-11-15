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

        var forward = new Vector3(m[2], m[6], m[10]);

        {
            for (var z = 0; z < CachedMeshSurfaceSamplingCountZ; z++)
            {
                for (var x = 0; x < CachedMeshSurfaceSamplingCountX; x++)
                {
                    var dest = new Vector3(x * CachedMeshWidth / (CachedMeshSurfaceSamplingCountX - 1.0f),
                        CachedMeshHeight,
                        z * CachedMeshDepth / (CachedMeshSurfaceSamplingCountZ - 1.0f));
                    Gizmos.color = Color.Lerp(new Color32(0, 0, 0, 0), new Color32(255, 255, 0, 255),
                        1 / Mathf.Pow(Vector3.Distance(position, dest) + 1, 2));
                    Gizmos.DrawLine(position, dest);

                    // Uncomment to show transformed rays relative to listener look direction
                    /*var dir = dest - position;
                    var relativeListenVector = new Vector3(
                        m[0] * dir.x + m[4] * dir.y + m[8] * dir.z,
                        m[1] * dir.x + m[5] * dir.y + m[9] * dir.z,
                        m[2] * dir.x + m[6] * dir.y + m[10] * dir.z
                    );
                    Gizmos.color = Color.magenta;
                    Gizmos.DrawLine(position, position + relativeListenVector);*/
                }
            }
        }
        /*var relativeListenVector = new Vector3(
            -(m[0] * position.x + m[4] * position.y + m[8] * position.z),
            -(m[1] * position.x + m[5] * position.y + m[9] * position.z),
            -(m[2] * position.x + m[6] * position.y + m[10] * position.z)
        );
        Debug.Log(
            $"Azimuth{-Mathf.Atan2(relativeListenVector.x, relativeListenVector.z) * Mathf.Rad2Deg}, Elevation{Mathf.Atan2(relativeListenVector.y, Mathf.Sqrt(relativeListenVector.x * relativeListenVector.x + relativeListenVector.z * relativeListenVector.z)) * Mathf.Rad2Deg}");
*/

        Gizmos.color = Color.black;
        Gizmos.DrawWireSphere(position, CachedEarsDistance / 2.0f);
        Gizmos.color = Color.green;
        Gizmos.DrawLine(position, position + forward * CachedEarsDistance);
    }
}