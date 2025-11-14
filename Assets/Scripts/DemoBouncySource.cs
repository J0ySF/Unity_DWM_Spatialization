using UnityEngine;

public class DemoBouncySource : MonoBehaviour
{
    [SerializeField] private float throwStrength;

    private Rigidbody _rigidbody;
    private AudioSource _audioSource;

    private void OnEnable()
    {
        _rigidbody = GetComponent<Rigidbody>();
        _audioSource = GetComponent<AudioSource>();
        _rigidbody.linearVelocity = Vector3.zero;
        _rigidbody.angularVelocity = Vector3.zero;
        _rigidbody.Move(new Vector3(Random.value, Random.value, Random.value), Quaternion.identity);
        _rigidbody.AddForce( Random.insideUnitCircle.normalized  * throwStrength + Vector2.up * throwStrength * 2.0f, ForceMode.Impulse);
    }

    private void OnCollisionEnter(Collision _)
    {
        _audioSource.pitch = Random.Range(0.9f, 1.1f);
        _audioSource.Play();
    } 
}