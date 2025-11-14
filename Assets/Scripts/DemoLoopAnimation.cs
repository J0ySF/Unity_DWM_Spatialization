using UnityEngine;

public class DemoLoopAnimation : MonoBehaviour
{
    private void Start()
    {
        var anim = gameObject.GetComponent<Animation>();
        anim.Play();
        anim.wrapMode = WrapMode.Loop;
    }
}
