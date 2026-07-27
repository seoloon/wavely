using Avalonia.Threading;
using Wavely.Backend;

namespace Wavely.App.Services;

public sealed class PlaybackPositionEventArgs(TimeSpan position, TimeSpan duration, double percent) : EventArgs
{
    public TimeSpan Position { get; } = position;
    public TimeSpan Duration { get; } = duration;
    public double Percent { get; } = percent;
}

/// <summary>
/// GSMTC only reports position on track/timeline-change events (see MediaSessionManager's
/// PositionChanged, fired roughly once a second at most by apps that fire it at all) - far too
/// coarse for a smoothly moving progress bar. This interpolates locally between those backend
/// snapshots using a wall-clock anchor, resyncing every time a fresher backend value arrives.
/// </summary>
public sealed class PlaybackPositionTracker : IDisposable
{
    private static readonly TimeSpan TickInterval = TimeSpan.FromMilliseconds(200);

    private readonly DispatcherTimer _timer;
    private TimeSpan _anchorPosition = TimeSpan.Zero;
    private DateTime _anchorAtUtc = DateTime.UtcNow;
    private TimeSpan _duration = TimeSpan.Zero;
    private bool _isPlaying;

    public event EventHandler<PlaybackPositionEventArgs>? Tick;

    public PlaybackPositionTracker()
    {
        _timer = new DispatcherTimer { Interval = TickInterval };
        _timer.Tick += (_, _) => RaiseTick();
        _timer.Start();
    }

    /// <summary>Resyncs the interpolation anchor to a fresh backend snapshot - called on
    /// TrackChanged and on MediaSessionManager.PositionChanged.</summary>
    public void Sync(TrackInfo track)
    {
        _anchorPosition = TimeSpan.FromMilliseconds(track.PositionMs);
        _duration = TimeSpan.FromMilliseconds(track.DurationMs);
        _anchorAtUtc = DateTime.UtcNow;
        RaiseTick();
    }

    public void SetPlaying(bool isPlaying)
    {
        if (_isPlaying == isPlaying)
        {
            return;
        }
        // Re-anchor at the transition so the elapsed-time math below starts fresh from "now"
        // instead of compounding time that passed while paused.
        _anchorPosition = CurrentPosition();
        _anchorAtUtc = DateTime.UtcNow;
        _isPlaying = isPlaying;
    }

    private TimeSpan CurrentPosition()
    {
        if (!_isPlaying)
        {
            return _anchorPosition;
        }
        var elapsed = DateTime.UtcNow - _anchorAtUtc;
        var position = _anchorPosition + elapsed;
        return position > _duration ? _duration : position;
    }

    private void RaiseTick()
    {
        var position = CurrentPosition();
        var percent = _duration > TimeSpan.Zero ? Math.Min(100.0, position.TotalMilliseconds / _duration.TotalMilliseconds * 100.0) : 0.0;
        Tick?.Invoke(this, new PlaybackPositionEventArgs(position, _duration, percent));
    }

    public void Dispose() => _timer.Stop();
}
