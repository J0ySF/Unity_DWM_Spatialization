using System.Runtime.InteropServices;
using UnityEngine;

public class DebugListeningSetup : MonoBehaviour
{
    private static class NativePlugin
    {
        [DllImport("Unity_DWM_Spatializer")]
        public static extern float GetEarsDistance();
    }

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
        // TODO: this does not allow tilting the head
        var forwardYLess = new Vector3(
            m[2], //m[0] * m[2] + m[4] * m[6] + m[8] * m[10],
            m[1] * m[2] + m[5] * m[6] + m[9] * m[10],
            m[10] // m[2] * m[2] + m[6] * m[6] + m[10] * m[10]
        );
        Debug.Log($"Elevation {-Mathf.Atan2(forward.y, Mathf.Sqrt(forward.x * forward.x + forward.z * forward.z)) * Mathf.Rad2Deg}");

        Gizmos.color = Color.black;
        Gizmos.DrawWireSphere(position, CachedEarsDistance / 2.0f);
        Gizmos.color = Color.green;
        Gizmos.DrawLine(position, position + forward * CachedEarsDistance);
        Gizmos.color = Color.magenta;
        Gizmos.DrawLine(position, position + forwardYLess * CachedEarsDistance);
    }
}