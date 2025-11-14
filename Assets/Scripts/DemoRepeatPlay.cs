using UnityEngine;

public class DemoRepeatPlay : MonoBehaviour
{
    public float minimumWait;
    public float maximumWait;
    public float minimumPitch;
    public float maximumPitch;
    
    private float _wait;

    private AudioSource _audioSource;
    
    private void Start()
    {
        _audioSource = GetComponent<AudioSource>();
        _wait = Random.Range(minimumWait, maximumWait);
    }

    private void Update()
    {
        if (_wait > 0)
        {
            _wait -= Time.deltaTime;
            return;
        }
        
        _wait = Random.Range(minimumWait, maximumWait);
        _audioSource.pitch = Random.Range(minimumPitch, maximumPitch);
        _audioSource.Play();
    }
}
