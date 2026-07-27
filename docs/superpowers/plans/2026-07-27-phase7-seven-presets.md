# Phase 7 (7 Presets) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `MainWindow`'s single hardcoded layout with a generic, runtime-switchable preset engine rendering all 7 named presets (Compact, Boxy, Gallery, Minimal, macOS, Shell, Discord) exactly as specified in `assets/presets_reference/layouts/*.svelte` and `assets/presets_reference/layouts.ts`, driven by the already-persisted `AppConfig.PresetIndex`.

**Architecture:** `MainWindow` keeps window chrome (frameless drag/resize/click-through/hide-on-pause — untouched) but its content becomes a single `ContentControl x:Name="PresetHost"` that swaps between 7 `UserControl`s, one per preset, each implementing a shared `IPresetView` interface (`UpdateTrack`, `UpdatePlayback`, `UpdateWaveform`, `ApplyColors`, `ApplyCoverShape`). Two pieces of already-working MainWindow logic get extracted into reusable controls so every preset can use them without duplication: cover shape/glow/vinyl-spin becomes `Controls/CoverArtControl`, and a new `Controls/ProgressBarControl` draws the track+fill (+ optional thumb) every preset needs. Real playback position (absent from the backend today) is added as a small, scoped backend change and interpolated client-side for a smooth 60fps-adjacent progress bar between backend syncs.

**Tech Stack:** C++/WinRT (backend addition only), C#/.NET 8, Avalonia 11.3.18, `CommunityToolkit.Mvvm` untouched (MainWindow stays code-behind, matching Phases 1-6 precedent — no ViewModel introduced here).

## Global Constraints

- Frontend nullable reference types enabled; no empty `catch {}` (RULES.md §4).
- PascalCase types/methods/properties, `camelCase` locals, `_camelCase` private fields (RULES.md §3).
- No magic numbers — every tuning value is a named `const`/`static readonly`.
- Classes ~200 lines max; split further if a task's file would exceed it.
- No allocations in hot render paths (`Render(DrawingContext)`, the 16ms waveform tick, the 250ms position tick) — reuse buffers/fields, never allocate a new array/list per tick.
- **Deliberate scope cuts (confirmed with the user), not oversights:**
  - `EqualizerBars.svelte`'s decorative CSS glyph is **not** ported. Wavely already has a real FFT-driven `WaveformControl` (Phase 5) — reused as-is wherever the spec shows a small equalizer, omitted where the spec has no equalizer at all (Gallery, Minimal, Shell).
  - `BlurBackdrop.svelte` is **not** re-implemented per-preset. `MainWindow.Appearance.cs`'s existing `ApplyBlurBackground`/`BlurBackgroundImage` (Phase 6.3) already blurs the whole widget background; no preset gets its own separate blur layer.
  - CSS marquee-on-overflow for title/artist is **not** ported. Plain `TextBlock` with `TextTrimming="CharacterEllipsis"` is used instead — implementing overflow-only marquee animation is a real subsystem or its own right, out of proportion to a decorative detail, and ellipsis already communicates truncation.
- Build: `.\build.ps1 -Configuration Debug`. Run: `frontend\Wavely.App\bin\Debug\net8.0-windows10.0.19041.0\Wavely.App.exe`. Manual verification needs a real GSMTC session (Spotify) — same setup as Phases 1/4/5/6.
- Every preset's exact markup/sizing source is `assets/presets_reference/layouts/*Layout.svelte` + `assets/presets_reference/layouts.ts` (window sizes) — already committed reference material, not to be re-derived or asked for again.

---

## File Structure

- Backend: modify `TrackInfo.idl/.h/.cpp`, `MediaSessionManager.idl/.h/.cpp` (add `PositionMs` + `PositionChanged` event).
- Create: `frontend/Wavely.App/Services/PlaybackPositionTracker.cs`.
- Create: `frontend/Wavely.App/Controls/ProgressBarControl.cs`.
- Create: `frontend/Wavely.App/Controls/CoverArtControl.axaml(.cs)` — extracted from `MainWindow`.
- Create: `frontend/Wavely.App/Controls/IPresetView.cs`.
- Create: `frontend/Wavely.App/Controls/PresetCatalog.cs`.
- Create: `frontend/Wavely.App/Views/Presets/{Compact,Boxy,Gallery,Minimal,Macos,Shell,Discord}PresetView.axaml(.cs)`.
- Modify: `frontend/Wavely.App/Views/MainWindow.axaml`, `MainWindow.axaml.cs`, `MainWindow.Appearance.cs` — replace hardcoded content with `PresetHost`, remove now-extracted cover logic, add preset-switch wiring.
- Modify: `frontend/Wavely.App/Views/SettingsWindow.axaml` — remove the "not yet available" caveat text now that presets render.

---

### Task 1: Backend — expose live playback position

**Files:**
- Modify: `backend/Wavely.Backend/TrackInfo.idl`, `TrackInfo.h`, `TrackInfo.cpp`
- Modify: `backend/Wavely.Backend/MediaSessionManager.idl`, `MediaSessionManager.h`, `MediaSessionManager.cpp`

**Interfaces:**
- Produces: `TrackInfo.PositionMs` (`Int64`, ms, snapshot at last metadata/timeline refresh), `MediaSessionManager.PositionChanged` event (`TypedEventHandler<MediaSessionManager, Int64>`).

- [ ] **Step 1: Add `PositionMs` to `TrackInfo`**

`TrackInfo.idl` — add alongside `DurationMs`:
```
Int64 PositionMs{ get; };
```

`TrackInfo.h` — add getter + setter + field, mirroring `DurationMs`:
```cpp
std::int64_t PositionMs();
// ...
void SetPositionMs(std::int64_t value) { m_positionMs = value; }
// ...
std::int64_t m_positionMs = 0;
```

`TrackInfo.cpp` — add getter body:
```cpp
std::int64_t TrackInfo::PositionMs()
{
    return m_positionMs;
}
```

- [ ] **Step 2: Populate `PositionMs` at metadata refresh time**

In `MediaSessionManager.cpp`'s `fillBasicMetadata`, add the position read (the function already receives `timeline`):
```cpp
void fillBasicMetadata(
    TrackInfo& track,
    GlobalSystemMediaTransportControlsSessionMediaProperties const& properties,
    GlobalSystemMediaTransportControlsSessionTimelineProperties const& timeline)
{
    track.SetTitle(properties.Title());
    track.SetArtist(properties.Artist());
    track.SetAlbum(properties.AlbumTitle());
    const auto duration = timeline.EndTime() - timeline.StartTime();
    track.SetDurationMs(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
    const auto position = timeline.Position() - timeline.StartTime();
    track.SetPositionMs(std::chrono::duration_cast<std::chrono::milliseconds>(position).count());
}
```

- [ ] **Step 3: Add a lightweight `PositionChanged` event fed by GSMTC's own timeline-change notifications**

`MediaSessionManager.idl` — add:
```
event Windows.Foundation.TypedEventHandler<MediaSessionManager, Int64> PositionChanged;
```

`MediaSessionManager.h` — add declarations mirroring `PlaybackStateChanged`:
```cpp
winrt::event_token PositionChanged(winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::MediaSessionManager, std::int64_t> const& handler);
void PositionChanged(winrt::event_token const& token) noexcept;
// ...
void refreshTimelineProperties();
// ...
winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession::TimelinePropertiesChanged_revoker m_timelinePropertiesChangedRevoker;
// ...
winrt::event<winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::MediaSessionManager, std::int64_t>> m_positionChangedEvent;
```

`MediaSessionManager.cpp` — subscribe in `subscribeToCurrentSession`, unsubscribe in `unsubscribeFromCurrentSession`, and implement the handler + event accessors:
```cpp
void MediaSessionManager::subscribeToCurrentSession()
{
    m_mediaPropertiesChangedRevoker = m_currentSession.MediaPropertiesChanged(
        winrt::auto_revoke, [this](auto&&, auto&&) { refreshMediaPropertiesAsync(); });
    m_playbackInfoChangedRevoker = m_currentSession.PlaybackInfoChanged(
        winrt::auto_revoke, [this](auto&&, auto&&) { refreshPlaybackInfo(); });
    m_timelinePropertiesChangedRevoker = m_currentSession.TimelinePropertiesChanged(
        winrt::auto_revoke, [this](auto&&, auto&&) { refreshTimelineProperties(); });
}

void MediaSessionManager::unsubscribeFromCurrentSession()
{
    m_mediaPropertiesChangedRevoker = {};
    m_playbackInfoChangedRevoker = {};
    m_timelinePropertiesChangedRevoker = {};
    m_currentSession = nullptr;
}

/// Cheap resync point for the frontend's local position interpolation (see
/// Services/PlaybackPositionTracker.cs) - GSMTC fires TimelinePropertiesChanged far less often
/// than 60fps (roughly once per second for apps that fire it at all), so this only ever updates
/// the anchor the frontend interpolates from, never drives a visual redraw directly.
void MediaSessionManager::refreshTimelineProperties()
{
    if (m_stopped.load() || !m_currentSession)
    {
        return;
    }
    try
    {
        const auto timeline = m_currentSession.GetTimelineProperties();
        const auto position = timeline.Position() - timeline.StartTime();
        const auto positionMs = std::chrono::duration_cast<std::chrono::milliseconds>(position).count();
        winrt::get_self<TrackInfo>(m_currentTrack)->SetPositionMs(positionMs);
        m_positionChangedEvent(*this, positionMs);
    }
    catch (winrt::hresult_error const&)
    {
        // The session ended between the event firing and this read; the next
        // SessionsChanged event will settle the state.
    }
}

winrt::event_token MediaSessionManager::PositionChanged(
    winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::MediaSessionManager, std::int64_t> const& handler)
{
    return m_positionChangedEvent.add(handler);
}

void MediaSessionManager::PositionChanged(winrt::event_token const& token) noexcept
{
    m_positionChangedEvent.remove(token);
}
```

- [ ] **Step 4: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).` (backend `.winmd`/`.dll` regenerate with the new member; frontend CsWinRT projection picks up `TrackInfo.PositionMs` and `MediaSessionManager.PositionChanged` automatically on its own build step).

- [ ] **Step 5: Commit**

```bash
git add backend/Wavely.Backend/TrackInfo.idl backend/Wavely.Backend/TrackInfo.h backend/Wavely.Backend/TrackInfo.cpp backend/Wavely.Backend/MediaSessionManager.idl backend/Wavely.Backend/MediaSessionManager.h backend/Wavely.Backend/MediaSessionManager.cpp
git commit -m "feat: expose live playback position from GSMTC (Phase 7 prerequisite)"
```

---

### Task 2: Frontend — local position interpolation service

**Files:**
- Create: `frontend/Wavely.App/Services/PlaybackPositionTracker.cs`

**Interfaces:**
- Consumes: `Wavely.Backend.TrackInfo.PositionMs/DurationMs`, `MediaSessionManager.PositionChanged`.
- Produces: `PlaybackPositionTracker.Tick` event (`EventHandler<PlaybackPositionEventArgs>` with `Position`/`Duration` as `TimeSpan` and `Percent` as `double`), `PlaybackPositionTracker.Reset(TrackInfo track)`, `SetPlaying(bool isPlaying)`.

- [ ] **Step 1: Write the tracker**

```csharp
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
```

- [ ] **Step 2: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).` — unused until Task 6 wires it in.

- [ ] **Step 3: Commit**

```bash
git add frontend/Wavely.App/Services/PlaybackPositionTracker.cs
git commit -m "feat: add client-side playback position interpolation service"
```

---

### Task 3: Shared `ProgressBarControl`

**Files:**
- Create: `frontend/Wavely.App/Controls/ProgressBarControl.cs`

**Interfaces:**
- Produces: `ProgressBarControl.Percent` (double, 0-100), `AccentColor` (Color), `ShowThumb` (bool, default false) — every preset task below consumes this.

- [ ] **Step 1: Write the control**

```csharp
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;

namespace Wavely.App.Controls;

/// <summary>
/// Draws a track+fill progress bar (height-derived corner radius, like every *Layout.svelte's
/// .progress-track/.progress-fill), with an optional circular thumb (Discord preset only).
/// Custom-drawn rather than a styled Avalonia ProgressBar - the reference's rounded pill shape
/// and thumb aren't reachable through ProgressBar's default template without a full retemplate,
/// and this is a handful of DrawRectangle/DrawEllipse calls.
/// </summary>
public sealed class ProgressBarControl : Control
{
    private const double TrackTintWithBlack = 0.35;
    private const double ThumbWidth = 8.0;
    private const double ThumbHeightOverBarHeight = 3.2;

    private static readonly Color DefaultAccentColor = Color.FromArgb(220, 90, 170, 255);
    private static readonly IBrush ThumbBrush = Brushes.White;

    private double _percent;
    private Color _accentColor = DefaultAccentColor;
    private bool _showThumb;

    public double Percent
    {
        get => _percent;
        set { _percent = Math.Clamp(value, 0.0, 100.0); InvalidateVisual(); }
    }

    public Color AccentColor
    {
        get => _accentColor;
        set { _accentColor = value; InvalidateVisual(); }
    }

    public bool ShowThumb
    {
        get => _showThumb;
        set { _showThumb = value; InvalidateVisual(); }
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);

        var bounds = Bounds;
        if (bounds.Width <= 0 || bounds.Height <= 0)
        {
            return;
        }

        var radius = bounds.Height / 2.0;
        var trackColor = MixWithBlack(_accentColor, TrackTintWithBlack);
        context.DrawRectangle(new SolidColorBrush(trackColor), null, new Rect(0, 0, bounds.Width, bounds.Height), radius, radius);

        var fillWidth = bounds.Width * (_percent / 100.0);
        if (fillWidth > 0)
        {
            context.DrawRectangle(new SolidColorBrush(_accentColor), null, new Rect(0, 0, fillWidth, bounds.Height), radius, radius);
        }

        if (_showThumb)
        {
            var thumbHeight = bounds.Height * ThumbHeightOverBarHeight;
            var thumbRect = new Rect(fillWidth - ThumbWidth / 2.0, bounds.Height / 2.0 - thumbHeight / 2.0, ThumbWidth, thumbHeight);
            context.DrawRectangle(ThumbBrush, null, thumbRect, ThumbWidth / 2.0, ThumbWidth / 2.0);
        }
    }

    private static Color MixWithBlack(Color color, double colorWeight) =>
        Color.FromArgb(
            color.A,
            (byte)(color.R * colorWeight),
            (byte)(color.G * colorWeight),
            (byte)(color.B * colorWeight));
}
```

- [ ] **Step 2: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).`

- [ ] **Step 3: Commit**

```bash
git add frontend/Wavely.App/Controls/ProgressBarControl.cs
git commit -m "feat: add shared ProgressBarControl for preset progress bars"
```

---

### Task 4: Extract `CoverArtControl` from `MainWindow` (self-contained — builds and runs on its own)

**Files:**
- Create: `frontend/Wavely.App/Controls/CoverArtControl.axaml`, `CoverArtControl.axaml.cs`
- Modify: `frontend/Wavely.App/Views/MainWindow.axaml` — replace the hardcoded cover `Grid` (lines 28-37 today) with one `<controls:CoverArtControl>` instance.
- Modify: `frontend/Wavely.App/Views/MainWindow.Appearance.cs` — remove `ApplyCoverShape`, `UpdateVinylRotationState`, `CoverCornerRadius`, `VinylRotationDegreesPerSecond`, `ApplyGlow` (moved into the control); `ApplyDynamicColors`'s glow call and `ApplyVisualScale`'s cover sizing now go through the control's properties instead.
- Modify: `frontend/Wavely.App/Views/MainWindow.axaml.cs` — remove `_vinylRotationTimer` and its constructor wiring (owned by the control now).

**Note on sequencing:** this task keeps `MainWindow` as today's single hardcoded layout (cover + title/artist/status + waveform) — it does **not** yet introduce `PresetHost`/`IPresetView`. That restructuring is Task 6, once a first concrete preset view exists to put in it. This task is a pure, independently buildable and testable refactor: after it, the widget looks and behaves *exactly* as it did at the end of Phase 6 (same verified square/squircle/vinyl/glow/spin behavior), just with that logic extracted into a reusable control.

**Interfaces:**
- Produces: `CoverArtControl.SetSource(Bitmap? bitmap)`, `Shape` (`CoverStyle`), `GlowEnabled` (bool), `GlowColor` (Color), `IsPlaying` (bool) — spinning is `Shape == Vinyl && IsPlaying`, matching the exact rule already verified in Phase 6.
- Consumes: nothing new — this is a pure refactor of already-shipped, already-verified Phase 6 behavior into a reusable shape.

This is a **behavior-preserving extraction**: every case in `ApplyCoverShape`, the glow logic in `ApplyGlow`, and the rotation timer in `UpdateVinylRotationState`/the constructor's timer setup move verbatim into the new control, driven by property setters instead of reading `_config`/`_isPlaying` fields directly.

- [ ] **Step 1: Write `CoverArtControl.axaml`**

```xml
<UserControl xmlns="https://github.com/avaloniaui"
             xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
             x:Class="Wavely.App.Controls.CoverArtControl">
    <Border x:Name="Root">
        <Grid>
            <Image x:Name="CoverImage" Stretch="UniformToFill" RenderTransformOrigin="50%,50%">
                <Image.RenderTransform>
                    <RotateTransform Angle="0" />
                </Image.RenderTransform>
            </Image>
            <Ellipse x:Name="VinylSpindle" Width="7" Height="7" Fill="#1A1A1A" IsVisible="False" />
        </Grid>
    </Border>
</UserControl>
```

- [ ] **Step 2: Write `CoverArtControl.axaml.cs`**

```csharp
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Wavely.App.Core;
using Wavely.Backend;

namespace Wavely.App.Controls;

/// <summary>
/// Self-contained cover art: square/squircle/vinyl clip shape, optional glow, and vinyl rotation
/// while playing. Extracted from MainWindow (Phase 6) so every Phase 7 preset can place a cover
/// without duplicating this logic - behavior is unchanged from the already-verified Phase 6
/// implementation, only the driving fields became properties.
/// </summary>
public partial class CoverArtControl : UserControl
{
    private const double DefaultCornerRadius = 8.0;
    private const double GlowBlurRadius = 18.0;
    private const double GlowOpacity = 0.9;
    private const double VinylRotationDegreesPerSecond = 90.0; // 360 degrees every 4 seconds.

    private readonly DispatcherTimer _rotationTimer;
    private readonly RotateTransform _rotateTransform;
    private CoverStyle _shape = CoverStyle.Square;
    private bool _glowEnabled;
    private Color _glowColor = Colors.White;
    private bool _isPlaying;

    public CoverArtControl()
    {
        InitializeComponent();
        _rotateTransform = (RotateTransform)CoverImage.RenderTransform!;
        _rotationTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(16) };
        _rotationTimer.Tick += (_, _) =>
            _rotateTransform.Angle = (_rotateTransform.Angle
                + VinylRotationDegreesPerSecond * _rotationTimer.Interval.TotalSeconds) % 360.0;
        Root.SizeChanged += (_, _) => ApplyShape();
    }

    public void SetSource(Bitmap? bitmap) => CoverImage.Source = bitmap;

    public CoverStyle Shape
    {
        get => _shape;
        set { _shape = value; ApplyShape(); }
    }

    public bool GlowEnabled
    {
        get => _glowEnabled;
        set { _glowEnabled = value; ApplyGlow(); }
    }

    public Color GlowColor
    {
        get => _glowColor;
        set { _glowColor = value; ApplyGlow(); }
    }

    public bool IsPlaying
    {
        get => _isPlaying;
        set { _isPlaying = value; UpdateRotationState(); }
    }

    private void ApplyShape()
    {
        var size = Root.Bounds.Width;
        switch (_shape)
        {
            case CoverStyle.Square:
                Root.ClipToBounds = true;
                Root.CornerRadius = new CornerRadius(DefaultCornerRadius);
                Root.Clip = null;
                VinylSpindle.IsVisible = false;
                break;
            case CoverStyle.Squircle:
                Root.ClipToBounds = false;
                Root.CornerRadius = new CornerRadius(0);
                Root.Clip = SquircleGeometry.ForSize(size);
                VinylSpindle.IsVisible = false;
                break;
            case CoverStyle.Vinyl:
                Root.ClipToBounds = false;
                Root.CornerRadius = new CornerRadius(0);
                Root.Clip = new EllipseGeometry(new Rect(0, 0, size, size));
                VinylSpindle.IsVisible = true;
                break;
        }
        UpdateRotationState();
    }

    private void ApplyGlow()
    {
        if (!_glowEnabled)
        {
            Root.Effect = null;
            return;
        }
        Root.Effect = new DropShadowDirectionEffect
        {
            Color = _glowColor,
            BlurRadius = GlowBlurRadius,
            Direction = 0.0,
            ShadowDepth = 0.0,
            Opacity = GlowOpacity,
        };
    }

    private void UpdateRotationState()
    {
        var shouldRotate = _shape == CoverStyle.Vinyl && _isPlaying;
        if (shouldRotate && !_rotationTimer.IsEnabled)
        {
            _rotationTimer.Start();
        }
        else if (!shouldRotate && _rotationTimer.IsEnabled)
        {
            _rotationTimer.Stop();
        }
    }
}
```

- [ ] **Step 3: Replace the cover markup in `MainWindow.axaml`**

Replace:
```xml
                        <Border x:Name="CoverBorder" Grid.Column="0" Width="88" Height="88">
                            <Grid>
                                <Image x:Name="CoverImage" Stretch="UniformToFill" RenderTransformOrigin="50%,50%">
                                    <Image.RenderTransform>
                                        <RotateTransform Angle="0" />
                                    </Image.RenderTransform>
                                </Image>
                                <Ellipse x:Name="VinylSpindle" Width="7" Height="7" Fill="#1A1A1A" IsVisible="False" />
                            </Grid>
                        </Border>
```
with:
```xml
                        <controls:CoverArtControl x:Name="Cover" Grid.Column="0" Width="88" Height="88" />
```
(`xmlns:controls="using:Wavely.App.Controls"` is already declared on the `Window` root from Phase 5's `WaveformControl` usage — no new namespace import needed.)

- [ ] **Step 4: Update `MainWindow.axaml.cs`'s call sites**

Remove the `_vinylRotationTimer` field and its constructor setup block (the `coverRotateTransform`/`_vinylRotationTimer` lines - the control now owns this internally). In `ApplyVisualScale`, replace the `CoverBorder.Width`/`Height` lines with `Cover.Width`/`Cover.Height`:
```csharp
private void ApplyVisualScale(double scale)
{
    Width = DefaultWidth * scale;
    Height = DefaultHeight * scale;
    Cover.Width = CoverSize * scale;
    Cover.Height = CoverSize * scale;
    Waveform.Height = WaveformHeight * scale;
}
```
In `OnTrackChanged`, replace the `CoverImage.Source = ...` block with `Cover.SetSource(...)`:
```csharp
var coverArt = track.CoverArt;
Cover.SetSource(coverArt is { Length: > 0 } ? new Bitmap(new MemoryStream(coverArt.ToArray())) : null);
```
In `OnPlaybackStateChanged`, add `Cover.IsPlaying = isPlaying;` at the top of the `Dispatcher.UIThread.Post` lambda (replacing what `UpdateVinylRotationState()` used to do).

- [ ] **Step 5: Update `MainWindow.Appearance.cs`**

Delete `ApplyCoverShape`, `UpdateVinylRotationState`, the `CoverCornerRadius`/`VinylRotationDegreesPerSecond` constants, and the `ApplyGlow` method. In `ApplyDynamicColors`, replace the trailing `ApplyGlow(...)` call with direct property sets on the control, and set `Cover.Shape` from `_config.CoverShape` (previously done by the now-deleted `ApplyCoverShape`, called from `ApplyVisualScale` - move that call to `ApplyDynamicColors` and to a new call site in the constructor/`OnOpened` so shape is applied even before the first track arrives):
```csharp
private void ApplyDynamicColors(TrackInfo track)
{
    var scheme = DynamicColorService.Resolve(track);

    if (BackgroundTintBorder.Background is SolidColorBrush backgroundBrush)
    {
        backgroundBrush.Color = _config.DynamicBackgroundEnabled ? scheme.Background : WidgetColorScheme.Default.Background;
    }

    Waveform.AccentColor = _config.DynamicColorsEnabled ? scheme.Accent : WidgetColorScheme.Default.Accent;

    var textIsDark = _config.DynamicColorsEnabled && scheme.TextIsDark;
    TitleText.Foreground = textIsDark ? DarkTitleForeground : LightTitleForeground;
    ArtistText.Foreground = textIsDark ? DarkArtistForeground : LightArtistForeground;
    StatusText.Foreground = textIsDark ? DarkStatusForeground : LightStatusForeground;

    Cover.GlowColor = _config.DynamicColorsEnabled ? scheme.Glow : WidgetColorScheme.Default.Glow;
}

private void ApplyCoverAppearance()
{
    Cover.Shape = _config.CoverShape;
    Cover.GlowEnabled = _config.CoverGlowEnabled;
}
```
Call `ApplyCoverAppearance();` from `OnOpened` (next to the existing `ApplyAppearance();` call) and from `RefreshFromConfig` (next to its existing `ApplyAppearance();` call), so shape/glow apply both at startup and whenever Settings changes them.

- [ ] **Step 6: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).`

- [ ] **Step 7: Run and verify no regression**

Run `Wavely.App.exe` with a real GSMTC session. Confirm cover shape (Square/Squircle/Vinyl), glow, and vinyl spin/pause-freeze/resume all behave exactly as verified at the end of Phase 6 — this task changed *where* the logic lives, not what it does.

- [ ] **Step 8: Commit**

```bash
git add frontend/Wavely.App/Controls/CoverArtControl.axaml frontend/Wavely.App/Controls/CoverArtControl.axaml.cs frontend/Wavely.App/Views/MainWindow.axaml frontend/Wavely.App/Views/MainWindow.axaml.cs frontend/Wavely.App/Views/MainWindow.Appearance.cs
git commit -m "refactor: extract CoverArtControl from MainWindow for reuse across Phase 7 presets"
```

---

### Task 5: `IPresetView`

**Files:**
- Create: `frontend/Wavely.App/Controls/IPresetView.cs`

**Interfaces:**
- Produces: `IPresetView` (implemented by all 7 preset views, Tasks 6-12).

- [ ] **Step 1: Write `IPresetView`**

```csharp
using Wavely.Backend;

namespace Wavely.App.Controls;

/// <summary>
/// Implemented by every Phase 7 preset UserControl. MainWindow drives whichever preset is
/// currently hosted through this interface only - it never reaches into a specific preset's
/// named elements, which is what makes swapping presets at runtime (AppConfig.PresetIndex) a
/// single ContentControl.Content assignment instead of a per-preset special case.
/// </summary>
public interface IPresetView
{
    void UpdateTrack(TrackInfo track);
    void UpdatePlayback(bool isPlaying, TimeSpan position, TimeSpan duration, double percent);
    void UpdateWaveform(ReadOnlySpan<float> bands);
    void ApplyColors(Services.WidgetColorScheme scheme, bool dynamicColorsEnabled, bool dynamicBackgroundEnabled);
    void ApplyCoverAppearance(CoverStyle shape, bool glowEnabled);
}
```

- [ ] **Step 2: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).` — unused until Task 6 wires it in.

- [ ] **Step 3: Commit**

```bash
git add frontend/Wavely.App/Controls/IPresetView.cs
git commit -m "feat: add IPresetView, the interface every Phase 7 preset implements"
```

---

### Task 6: Preset engine + `CompactPresetView` (first preset — replaces `MainWindow`'s hardcoded layout)

**Files:**
- Create: `frontend/Wavely.App/Controls/PresetCatalog.cs`
- Create: `frontend/Wavely.App/Views/Presets/CompactPresetView.axaml`, `CompactPresetView.axaml.cs`
- Modify: `frontend/Wavely.App/Views/MainWindow.axaml` — replace the `StackPanel` content (the cover/text/waveform Task 4 left in place) with `PresetHost`.
- Modify: `frontend/Wavely.App/Views/MainWindow.axaml.cs` — track the active `IPresetView`, resize to the preset's base size, forward all events to it.
- Modify: `frontend/Wavely.App/Views/MainWindow.Appearance.cs` — `ApplyDynamicColors`/`ApplyAppearance`/`ApplyCoverAppearance` forward to the active preset instead of touching named elements directly.

**Interfaces:**
- Consumes: `IPresetView` (Task 5), `PlaybackPositionTracker` (Task 2), `CoverArtControl` (Task 4), `ProgressBarControl` (Task 3), `WaveformControl` (existing, Phase 5).
- Produces: `PresetCatalog.Entries`/`PresetEntry`/`Resolve` (Tasks 7-12 each append one entry), `MainWindow.ApplyPreset(int index)`.

**Spec source for `CompactPresetView`:** `assets/presets_reference/layouts/CompactLayout.svelte`. Cover 72px/radius 10. Two side-by-side panels: title+artist (marquee → ellipsis per Global Constraints), and a meta panel with time/waveform/time above a progress bar.

This task both stands up the generic engine AND is the first preset — after it, `MainWindow` no longer has any hardcoded cover/text/waveform elements of its own (those move into `CompactPresetView`, the only entry `PresetCatalog` has so far). The widget's visible behavior at the end of this task is Compact, unconditionally (no Settings toggle takes effect yet — that starts working the moment Task 7 gives `PresetCatalog` a second entry).

- [ ] **Step 1: Rewrite `MainWindow.axaml`**

```xml
<Window xmlns="https://github.com/avaloniaui"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        x:Class="Wavely.App.Views.MainWindow"
        Title="Wavely"
        SystemDecorations="None"
        Background="Transparent"
        TransparencyLevelHint="Transparent"
        WindowStartupLocation="Manual"
        CanResize="False"
        ShowInTaskbar="False"
        Topmost="True"
        Width="360" Height="110">
    <Window.Transitions>
        <Transitions>
            <DoubleTransition Property="Opacity" Duration="0:0:0.3" />
        </Transitions>
    </Window.Transitions>
    <Border x:Name="BackgroundBorder" CornerRadius="16" ClipToBounds="True">
        <Grid>
            <Image x:Name="BlurBackgroundImage" Stretch="UniformToFill" IsVisible="False" />
            <Border x:Name="BackgroundTintBorder" Padding="16">
                <Border.Background>
                    <SolidColorBrush Color="#141418" Opacity="0.7" />
                </Border.Background>
                <ContentControl x:Name="PresetHost" HorizontalContentAlignment="Stretch" VerticalContentAlignment="Stretch" />
            </Border>
        </Grid>
    </Border>
</Window>
```

(`Width="360" Height="110"` is Compact's base size, `PresetIndex` 0's default — `OnOpened` immediately overwrites this via `ApplyPreset` before the window is shown, so this is only a placeholder pre-layout value, matching how `DefaultWidth`/`DefaultHeight` worked before.)

- [ ] **Step 2: Update `MainWindow.axaml.cs`**

Replace `DefaultWidth`/`DefaultHeight`/`CoverSize`/`WaveformHeight` constants (no longer meaningful — each preset defines its own base size and internal layout) and add preset-switching state:

```csharp
private readonly PlaybackPositionTracker _positionTracker = new();
private IPresetView _activePreset = null!;
```

Add, near `ApplyVisualScale`:
```csharp
/// <summary>Swaps PresetHost's content to AppConfig.PresetIndex's view, resizes the window to
/// that preset's base size (before the user's 50%-150% scale is applied), and re-applies
/// everything the new view needs to render correctly (it starts blank otherwise).</summary>
private void ApplyPreset(int index)
{
    var entry = PresetCatalog.Resolve(index);
    var view = entry.Factory();
    PresetHost.Content = (Control)view;
    _activePreset = view;
    _presetBaseSize = entry.WindowSize;
    ApplyVisualScale(_config.Geometry.Scale);
    ApplyAppearance();
    ApplyCoverAppearanceOnActivePreset();
    if (_currentTrack is not null)
    {
        _activePreset.UpdateTrack(_currentTrack);
        ApplyDynamicColors(_currentTrack);
    }
}

private Avalonia.Size _presetBaseSize;
```

Change `ApplyVisualScale` to use the active preset's base size instead of the old constants:
```csharp
private void ApplyVisualScale(double scale)
{
    Width = _presetBaseSize.Width * scale;
    Height = _presetBaseSize.Height * scale;
}
```

In `OnOpened`, call `ApplyPreset(_config.PresetIndex);` before `ApplyScale`/`ApplyClickThrough` (it must run first — `ApplyVisualScale` depends on `_presetBaseSize` being set):
```csharp
private void OnOpened(object? sender, EventArgs e)
{
    var handle = TryGetPlatformHandle();
    if (handle is not null)
    {
        _hwnd = handle.Handle;
        _wndProcHook = WndProcHook;
        Win32Properties.AddWndProcHookCallback(this, _wndProcHook);
    }

    ApplyPreset(_config.PresetIndex);

    var geometry = _config.Geometry;
    Position = new PixelPoint(geometry.PositionX, geometry.PositionY);
    ApplyScale(geometry.Scale);
    ApplyClickThrough(_config.ClickThroughEnabled);
    BlurBackgroundImage.Effect = new Avalonia.Media.BlurEffect { Radius = BackgroundBlurRadius };
    ApplyBlurBackground();

    FadeIn();
}
```

In `RefreshFromConfig`, re-run `ApplyPreset` when the index changed (cheap to always re-run — a preset switch is a rare, deliberate user action, not a hot path):
```csharp
public void RefreshFromConfig()
{
    ApplyPreset(_config.PresetIndex);
    ApplyClickThrough(_config.ClickThroughEnabled);
    _hideTimer.Interval = TimeSpan.FromSeconds(Math.Clamp(_config.HideOnPauseDelaySeconds, 5, 30));
    ApplyBlurBackground();
}
```

In `OnTrackChanged`, forward to the active preset and sync the position tracker (replacing the direct `TitleText.Text =`/`CoverImage.Source =` lines):
```csharp
private void OnTrackChanged(MediaSessionManager sender, TrackInfo track)
{
    Dispatcher.UIThread.Post(() =>
    {
        _currentTrack = track;
        _activePreset.UpdateTrack(track);
        _positionTracker.Sync(track);
        ApplyBlurBackground();
        ApplyDynamicColors(track);
    });
}
```
(`ApplyBlurBackground` still reads `CoverImage.Source` today - Task 6 Step 3 below changes it to read the cover bitmap from `_currentTrack` directly instead, since there's no single `CoverImage` element anymore.)

In `OnPlaybackStateChanged`, drop the direct `StatusText.Text =` line (no single `StatusText` anymore - each preset shows its own status/progress via `UpdatePlayback`) and notify the tracker:
```csharp
private void OnPlaybackStateChanged(MediaSessionManager sender, bool isPlaying)
{
    Dispatcher.UIThread.Post(() =>
    {
        _isPlaying = isPlaying;
        _positionTracker.SetPlaying(isPlaying);

        if (isPlaying)
        {
            _hideTimer.Stop();
            if (_hiddenByAutoHide)
            {
                _hideAfterFadeTimer.Stop();
                Opacity = 1;
                Show();
                MoveClickThroughHandle();
                if (_config.ClickThroughEnabled)
                {
                    _clickThroughHandle.Show();
                }
                _hiddenByAutoHide = false;
            }
        }
        else if (_config.HideOnPauseEnabled)
        {
            _hideTimer.Start();
        }
    });
}
```

Wire the position tracker's `Tick` and cover appearance in the constructor, next to the other event subscriptions:
```csharp
_positionTracker.Tick += (_, e) =>
    Dispatcher.UIThread.Post(() => _activePreset.UpdatePlayback(_isPlaying, e.Position, e.Duration, e.Percent));
```

Change `OnWaveformDataReady` to forward to the active preset instead of a named `Waveform` element:
```csharp
private void OnWaveformDataReady(WaveformEngine sender, IBuffer bands)
{
    var floats = MemoryMarshal.Cast<byte, float>(bands.ToArray()).ToArray();
    Dispatcher.UIThread.Post(() => _activePreset.UpdateWaveform(floats));
}
```

- [ ] **Step 3: Update `MainWindow.Appearance.cs`**

`ApplyAppearance` no longer touches text foregrounds (each preset owns its own text elements) - it now only does background opacity + theme:
```csharp
private void ApplyAppearance()
{
    if (BackgroundTintBorder.Background is SolidColorBrush backgroundBrush)
    {
        backgroundBrush.Opacity = _config.BackgroundOpacity;
    }

    if (Application.Current is { } app)
    {
        app.RequestedThemeVariant = _config.Theme == ThemeMode.Dark
            ? Avalonia.Styling.ThemeVariant.Dark
            : Avalonia.Styling.ThemeVariant.Light;
    }
}
```

`ApplyDynamicColors` forwards to the active preset instead of touching named text/waveform elements, and calls the new `ApplyCoverAppearanceOnActivePreset` helper for glow (shape is applied separately since it doesn't depend on the color scheme):
```csharp
private void ApplyDynamicColors(TrackInfo track)
{
    var scheme = DynamicColorService.Resolve(track);
    _activePreset.ApplyColors(scheme, _config.DynamicColorsEnabled, _config.DynamicBackgroundEnabled);
}

private void ApplyCoverAppearanceOnActivePreset()
{
    _activePreset.ApplyCoverAppearance(_config.CoverShape, _config.CoverGlowEnabled);
}
```

Remove `ApplyBlurBackground`'s dependency on `CoverImage.Source` - it now reads the cover bitmap straight from `_currentTrack`:
```csharp
private void ApplyBlurBackground()
{
    if (_currentTrack is { CoverArt: { Length: > 0 } coverArt })
    {
        using var stream = new MemoryStream(coverArt.ToArray());
        BlurBackgroundImage.Source = new Avalonia.Media.Imaging.Bitmap(stream);
    }
    else
    {
        BlurBackgroundImage.Source = null;
    }
    BlurBackgroundImage.IsVisible = _config.CoverBlurEnabled && BlurBackgroundImage.Source is not null;
}
```
(`using Wavely.App.Controls;` and `using Wavely.App.Services;` need adding to this file's usings for `IPresetView`/`WidgetColorScheme`/`PresetCatalog`.)

- [ ] **Step 4: Write `PresetCatalog` (Compact-only for now)**

```csharp
using Avalonia;

namespace Wavely.App.Controls;

public sealed record PresetEntry(string Name, Size WindowSize, Func<IPresetView> Factory);

/// <summary>
/// The presets in AppConfig.PresetIndex order (matches SettingsViewModel.PresetNames and
/// assets/presets_reference/layouts.ts exactly). MainWindow indexes into this to know both which
/// view to host and what base window size to resize to before the user's 50%-150% scale is
/// applied on top. Grows by one entry per Phase 7 task (Tasks 7-12) until all 7 are present.
/// </summary>
public static class PresetCatalog
{
    public static IReadOnlyList<PresetEntry> Entries { get; } =
    [
        new("Compact", new Size(360, 110), () => new Views.Presets.CompactPresetView()),
    ];

    public static PresetEntry Resolve(int index) =>
        index >= 0 && index < Entries.Count ? Entries[index] : Entries[0];
}
```

- [ ] **Step 5: Write `CompactPresetView.axaml`**

**Spec source:** `assets/presets_reference/layouts/CompactLayout.svelte`. Cover 72px/radius 10. Two side-by-side panels: title+artist (marquee → ellipsis per Global Constraints), and a meta panel with time/waveform/time above a progress bar.

```xml
<UserControl xmlns="https://github.com/avaloniaui"
             xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
             xmlns:controls="using:Wavely.App.Controls"
             x:Class="Wavely.App.Views.Presets.CompactPresetView">
    <Grid ColumnDefinitions="Auto,*,*" ColumnSpacing="10">
        <controls:CoverArtControl x:Name="Cover" Grid.Column="0" Width="72" Height="72" />
        <Border Grid.Column="1" Background="#1AFFFFFF" CornerRadius="10" Padding="14,8" VerticalAlignment="Stretch">
            <StackPanel VerticalAlignment="Center" Spacing="3">
                <TextBlock x:Name="TitleText" FontWeight="Bold" FontSize="14" Foreground="White" TextTrimming="CharacterEllipsis" />
                <TextBlock x:Name="ArtistText" FontSize="12" Foreground="#B4FFFFFF" TextTrimming="CharacterEllipsis" />
            </StackPanel>
        </Border>
        <Border Grid.Column="2" Background="#1AFFFFFF" CornerRadius="10" Padding="14,8" VerticalAlignment="Stretch">
            <StackPanel VerticalAlignment="Center" Spacing="6">
                <Grid ColumnDefinitions="Auto,*,Auto">
                    <TextBlock x:Name="PositionText" Grid.Column="0" FontSize="11" FontWeight="Bold" Foreground="White" VerticalAlignment="Center" />
                    <controls:WaveformControl x:Name="Waveform" Grid.Column="1" Height="13" Margin="6,0" />
                    <TextBlock x:Name="DurationText" Grid.Column="2" FontSize="11" FontWeight="Bold" Foreground="White" VerticalAlignment="Center" />
                </Grid>
                <controls:ProgressBarControl x:Name="Progress" Height="6" />
            </StackPanel>
        </Border>
    </Grid>
</UserControl>
```

- [ ] **Step 6: Write `CompactPresetView.axaml.cs`**

```csharp
using System.IO;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using Wavely.App.Services;
using Wavely.Backend;

namespace Wavely.App.Views.Presets;

public partial class CompactPresetView : UserControl, Controls.IPresetView
{
    public CompactPresetView() => InitializeComponent();

    public void UpdateTrack(TrackInfo track)
    {
        TitleText.Text = string.IsNullOrEmpty(track.Title) ? "No track playing" : track.Title;
        ArtistText.Text = track.Artist;
        var coverArt = track.CoverArt;
        Cover.SetSource(coverArt is { Length: > 0 } ? new Bitmap(new MemoryStream(coverArt.ToArray())) : null);
    }

    public void UpdatePlayback(bool isPlaying, TimeSpan position, TimeSpan duration, double percent)
    {
        Cover.IsPlaying = isPlaying;
        PositionText.Text = Format(position);
        DurationText.Text = Format(duration);
        Progress.Percent = percent;
    }

    public void UpdateWaveform(ReadOnlySpan<float> bands) => Waveform.UpdateBands(bands);

    public void ApplyColors(WidgetColorScheme scheme, bool dynamicColorsEnabled, bool dynamicBackgroundEnabled)
    {
        var accent = dynamicColorsEnabled ? scheme.Accent : WidgetColorScheme.Default.Accent;
        Waveform.AccentColor = accent;
        Progress.AccentColor = accent;
    }

    public void ApplyCoverAppearance(CoverStyle shape, bool glowEnabled)
    {
        Cover.Shape = shape;
        Cover.GlowEnabled = glowEnabled;
    }

    private static string Format(TimeSpan value) => value.ToString(@"m\:ss");
}
```

- [ ] **Step 7: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).`

- [ ] **Step 8: Run and verify**

Run `Wavely.App.exe` with a real GSMTC session. Confirm: window is Compact's 360×110 base size (× the persisted scale), cover/title/artist/waveform/progress bar all render and update live, cover shape/glow/vinyl-spin still work exactly as before (Task 4 behavior, now reached through `IPresetView.ApplyCoverAppearance`), and the progress bar advances smoothly and the time labels tick up over a couple of seconds of real playback (first real exercise of Tasks 1-2's position tracking).

- [ ] **Step 9: Commit**

```bash
git add frontend/Wavely.App/Controls/PresetCatalog.cs frontend/Wavely.App/Views/Presets/CompactPresetView.axaml frontend/Wavely.App/Views/Presets/CompactPresetView.axaml.cs frontend/Wavely.App/Views/MainWindow.axaml frontend/Wavely.App/Views/MainWindow.axaml.cs frontend/Wavely.App/Views/MainWindow.Appearance.cs
git commit -m "feat: add the runtime preset-switching engine and the Compact preset (Phase 7.1)"
```

---

### Task 7: `BoxyPresetView`

**Files:**
- Create: `frontend/Wavely.App/Views/Presets/BoxyPresetView.axaml`, `.axaml.cs`
- Modify: `frontend/Wavely.App/Controls/PresetCatalog.cs` — append one entry.

**Spec source:** `BoxyLayout.svelte`. Cover 92px/radius 12 next to a stacked info panel (title/artist) + meta panel (time/waveform(11)/time, row layout not column), full-width progress bar below both.

- [ ] **Step 1: Write `BoxyPresetView.axaml`**

```xml
<UserControl xmlns="https://github.com/avaloniaui"
             xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
             xmlns:controls="using:Wavely.App.Controls"
             x:Class="Wavely.App.Views.Presets.BoxyPresetView">
    <StackPanel Spacing="8">
        <Grid ColumnDefinitions="Auto,*" ColumnSpacing="10">
            <controls:CoverArtControl x:Name="Cover" Grid.Column="0" Width="92" Height="92" />
            <StackPanel Grid.Column="1" Spacing="8">
                <Border Background="#1AFFFFFF" CornerRadius="10" Padding="14,8">
                    <StackPanel Spacing="3">
                        <TextBlock x:Name="TitleText" FontWeight="Bold" FontSize="15" Foreground="White" TextTrimming="CharacterEllipsis" />
                        <TextBlock x:Name="ArtistText" FontSize="12" Foreground="#B4FFFFFF" TextTrimming="CharacterEllipsis" />
                    </StackPanel>
                </Border>
                <Border Background="#1AFFFFFF" CornerRadius="10" Padding="14,8">
                    <Grid ColumnDefinitions="Auto,*,Auto">
                        <TextBlock x:Name="PositionText" Grid.Column="0" FontSize="12" FontWeight="Bold" Foreground="White" VerticalAlignment="Center" />
                        <controls:WaveformControl x:Name="Waveform" Grid.Column="1" Height="20" Margin="8,0" />
                        <TextBlock x:Name="DurationText" Grid.Column="2" FontSize="12" FontWeight="Bold" Foreground="White" VerticalAlignment="Center" />
                    </Grid>
                </Border>
            </StackPanel>
        </Grid>
        <controls:ProgressBarControl x:Name="Progress" Height="9" />
    </StackPanel>
</UserControl>
```

- [ ] **Step 2: Write `BoxyPresetView.axaml.cs`**

```csharp
using System.IO;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using Wavely.App.Services;
using Wavely.Backend;

namespace Wavely.App.Views.Presets;

public partial class BoxyPresetView : UserControl, Controls.IPresetView
{
    public BoxyPresetView() => InitializeComponent();

    public void UpdateTrack(TrackInfo track)
    {
        TitleText.Text = string.IsNullOrEmpty(track.Title) ? "No track playing" : track.Title;
        ArtistText.Text = track.Artist;
        var coverArt = track.CoverArt;
        Cover.SetSource(coverArt is { Length: > 0 } ? new Bitmap(new MemoryStream(coverArt.ToArray())) : null);
    }

    public void UpdatePlayback(bool isPlaying, TimeSpan position, TimeSpan duration, double percent)
    {
        Cover.IsPlaying = isPlaying;
        PositionText.Text = Format(position);
        DurationText.Text = Format(duration);
        Progress.Percent = percent;
    }

    public void UpdateWaveform(ReadOnlySpan<float> bands) => Waveform.UpdateBands(bands);

    public void ApplyColors(WidgetColorScheme scheme, bool dynamicColorsEnabled, bool dynamicBackgroundEnabled)
    {
        var accent = dynamicColorsEnabled ? scheme.Accent : WidgetColorScheme.Default.Accent;
        Waveform.AccentColor = accent;
        Progress.AccentColor = accent;
    }

    public void ApplyCoverAppearance(CoverStyle shape, bool glowEnabled)
    {
        Cover.Shape = shape;
        Cover.GlowEnabled = glowEnabled;
    }

    private static string Format(TimeSpan value) => value.ToString(@"m\:ss");
}
```

- [ ] **Step 3: Register it in `PresetCatalog`**

In `PresetCatalog.cs`, append to `Entries` (after `"Compact"`):
```csharp
new("Boxy", new Size(340, 170), () => new Views.Presets.BoxyPresetView()),
```

- [ ] **Step 4: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).`

- [ ] **Step 5: Run and verify**

Run `Wavely.App.exe`, switch to Boxy in Settings → Apparence → Preset. Confirm the 340×170 layout renders (92px cover, stacked title/artist + time/waveform/time panels, full-width progress bar), updates live, and cover shape/glow/vinyl-spin still work.

- [ ] **Step 6: Commit**

```bash
git add frontend/Wavely.App/Views/Presets/BoxyPresetView.axaml frontend/Wavely.App/Views/Presets/BoxyPresetView.axaml.cs frontend/Wavely.App/Controls/PresetCatalog.cs
git commit -m "feat: add Boxy preset (Phase 7.2)"
```

---

### Task 8: `GalleryPresetView`

**Files:**
- Create: `frontend/Wavely.App/Views/Presets/GalleryPresetView.axaml`, `.axaml.cs`
- Modify: `frontend/Wavely.App/Controls/PresetCatalog.cs` — append one entry.

**Spec source:** `GalleryLayout.svelte`. Large square cover filling the width (radius 16), title/artist panel below, then a time-labels row + progress bar. **No waveform** (matches the reference — Gallery has no `EqualizerBars`).

- [ ] **Step 1: Write `GalleryPresetView.axaml`**

```xml
<UserControl xmlns="https://github.com/avaloniaui"
             xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
             xmlns:controls="using:Wavely.App.Controls"
             x:Class="Wavely.App.Views.Presets.GalleryPresetView">
    <StackPanel Spacing="8">
        <controls:CoverArtControl x:Name="Cover" HorizontalAlignment="Stretch" Height="208" />
        <Border Background="#1AFFFFFF" CornerRadius="10" Padding="14,8">
            <StackPanel Spacing="3">
                <TextBlock x:Name="TitleText" FontWeight="Bold" FontSize="14" Foreground="White" TextTrimming="CharacterEllipsis" />
                <TextBlock x:Name="ArtistText" FontSize="12" Foreground="#B4FFFFFF" TextTrimming="CharacterEllipsis" />
            </StackPanel>
        </Border>
        <Border Background="#1AFFFFFF" CornerRadius="10" Padding="14,8">
            <StackPanel Spacing="6">
                <Grid ColumnDefinitions="*,Auto">
                    <TextBlock x:Name="PositionText" Grid.Column="0" FontSize="11" FontWeight="Bold" Foreground="White" />
                    <TextBlock x:Name="DurationText" Grid.Column="1" FontSize="11" FontWeight="Bold" Foreground="White" />
                </Grid>
                <controls:ProgressBarControl x:Name="Progress" Height="7" />
            </StackPanel>
        </Border>
    </StackPanel>
</UserControl>
```

(`Height="208"` is Gallery's cover slot: window is 240 wide, minus 16px padding each side from `BackgroundTintBorder` = 208 content width; `fit="width"` in the Svelte spec means the cover is square at the content's full width, so height matches.)

- [ ] **Step 2: Write `GalleryPresetView.axaml.cs`**

```csharp
using System.IO;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using Wavely.App.Services;
using Wavely.Backend;

namespace Wavely.App.Views.Presets;

public partial class GalleryPresetView : UserControl, Controls.IPresetView
{
    public GalleryPresetView() => InitializeComponent();

    public void UpdateTrack(TrackInfo track)
    {
        TitleText.Text = string.IsNullOrEmpty(track.Title) ? "No track playing" : track.Title;
        ArtistText.Text = track.Artist;
        var coverArt = track.CoverArt;
        Cover.SetSource(coverArt is { Length: > 0 } ? new Bitmap(new MemoryStream(coverArt.ToArray())) : null);
    }

    public void UpdatePlayback(bool isPlaying, TimeSpan position, TimeSpan duration, double percent)
    {
        Cover.IsPlaying = isPlaying;
        PositionText.Text = Format(position);
        DurationText.Text = Format(duration);
        Progress.Percent = percent;
    }

    public void UpdateWaveform(ReadOnlySpan<float> bands)
    {
        // Gallery has no waveform slot (matches GalleryLayout.svelte - no EqualizerBars).
    }

    public void ApplyColors(WidgetColorScheme scheme, bool dynamicColorsEnabled, bool dynamicBackgroundEnabled)
    {
        Progress.AccentColor = dynamicColorsEnabled ? scheme.Accent : WidgetColorScheme.Default.Accent;
    }

    public void ApplyCoverAppearance(CoverStyle shape, bool glowEnabled)
    {
        Cover.Shape = shape;
        Cover.GlowEnabled = glowEnabled;
    }

    private static string Format(TimeSpan value) => value.ToString(@"m\:ss");
}
```

- [ ] **Step 3: Register it in `PresetCatalog`**

In `PresetCatalog.cs`, append to `Entries` (after `"Boxy"`):
```csharp
new("Gallery", new Size(240, 350), () => new Views.Presets.GalleryPresetView()),
```

- [ ] **Step 4: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).`

- [ ] **Step 5: Run and verify**

Run `Wavely.App.exe`, switch to Gallery. Confirm the 240×350 layout renders (large square cover, title/artist panel, time labels + progress bar below, no waveform), updates live.

- [ ] **Step 6: Commit**

```bash
git add frontend/Wavely.App/Views/Presets/GalleryPresetView.axaml frontend/Wavely.App/Views/Presets/GalleryPresetView.axaml.cs frontend/Wavely.App/Controls/PresetCatalog.cs
git commit -m "feat: add Gallery preset (Phase 7.3)"
```

---

### Task 9: `MinimalPresetView`

**Files:**
- Create: `frontend/Wavely.App/Views/Presets/MinimalPresetView.axaml`, `.axaml.cs`
- Modify: `frontend/Wavely.App/Controls/PresetCatalog.cs` — append one entry.

**Spec source:** `MinimalLayout.svelte`. A single pill: 34px cover, "Title • Artist" on one line (ellipsis), thin progress bar filling remaining width. No waveform, no separate panels.

- [ ] **Step 1: Write `MinimalPresetView.axaml`**

```xml
<UserControl xmlns="https://github.com/avaloniaui"
             xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
             xmlns:controls="using:Wavely.App.Controls"
             x:Class="Wavely.App.Views.Presets.MinimalPresetView">
    <Border Background="#1AFFFFFF" CornerRadius="999" Padding="0,0,14,0">
        <Grid ColumnDefinitions="Auto,*,Auto" ColumnSpacing="10" VerticalAlignment="Center">
            <controls:CoverArtControl x:Name="Cover" Grid.Column="0" Width="34" Height="34" />
            <TextBlock x:Name="LabelText" Grid.Column="1" FontSize="12" Foreground="White" TextTrimming="CharacterEllipsis" VerticalAlignment="Center" />
            <controls:ProgressBarControl x:Name="Progress" Grid.Column="2" Width="60" Height="5" VerticalAlignment="Center" />
        </Grid>
    </Border>
</UserControl>
```

- [ ] **Step 2: Write `MinimalPresetView.axaml.cs`**

```csharp
using System.IO;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using Wavely.App.Services;
using Wavely.Backend;

namespace Wavely.App.Views.Presets;

public partial class MinimalPresetView : UserControl, Controls.IPresetView
{
    public MinimalPresetView() => InitializeComponent();

    public void UpdateTrack(TrackInfo track)
    {
        var title = string.IsNullOrEmpty(track.Title) ? "No track playing" : track.Title;
        LabelText.Text = string.IsNullOrEmpty(track.Artist) ? title : $"{title} • {track.Artist}";
        var coverArt = track.CoverArt;
        Cover.SetSource(coverArt is { Length: > 0 } ? new Bitmap(new MemoryStream(coverArt.ToArray())) : null);
    }

    public void UpdatePlayback(bool isPlaying, TimeSpan position, TimeSpan duration, double percent)
    {
        Cover.IsPlaying = isPlaying;
        Progress.Percent = percent;
    }

    public void UpdateWaveform(ReadOnlySpan<float> bands)
    {
        // Minimal has no waveform slot (matches MinimalLayout.svelte - no EqualizerBars).
    }

    public void ApplyColors(WidgetColorScheme scheme, bool dynamicColorsEnabled, bool dynamicBackgroundEnabled)
    {
        Progress.AccentColor = dynamicColorsEnabled ? scheme.Accent : WidgetColorScheme.Default.Accent;
    }

    public void ApplyCoverAppearance(CoverStyle shape, bool glowEnabled)
    {
        Cover.Shape = shape;
        Cover.GlowEnabled = glowEnabled;
    }
}
```

- [ ] **Step 3: Register it in `PresetCatalog`**

In `PresetCatalog.cs`, append to `Entries` (after `"Gallery"`):
```csharp
new("Minimal", new Size(300, 54), () => new Views.Presets.MinimalPresetView()),
```

- [ ] **Step 4: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).`

- [ ] **Step 5: Run and verify**

Run `Wavely.App.exe`, switch to Minimal. Confirm the 300×54 pill renders (34px cover, "Title • Artist" label, thin progress bar), updates live.

- [ ] **Step 6: Commit**

```bash
git add frontend/Wavely.App/Views/Presets/MinimalPresetView.axaml frontend/Wavely.App/Views/Presets/MinimalPresetView.axaml.cs frontend/Wavely.App/Controls/PresetCatalog.cs
git commit -m "feat: add Minimal preset (Phase 7.4)"
```

---

### Task 10: `MacosPresetView`

**Files:**
- Create: `frontend/Wavely.App/Views/Presets/MacosPresetView.axaml`, `.axaml.cs`
- Modify: `frontend/Wavely.App/Controls/PresetCatalog.cs` — append one entry.

**Spec source:** `MacosLayout.svelte`. Titlebar with red/yellow/green traffic-light dots + a small waveform(4), accent-colored bottom border; content row below is cover (fit=height) + info column (title/artist/time-row/progress).

- [ ] **Step 1: Write `MacosPresetView.axaml`**

```xml
<UserControl xmlns="https://github.com/avaloniaui"
             xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
             xmlns:controls="using:Wavely.App.Controls"
             x:Class="Wavely.App.Views.Presets.MacosPresetView">
    <Border x:Name="Chrome" Background="#D82B2B2F" CornerRadius="12" ClipToBounds="True">
        <Grid RowDefinitions="Auto,*">
            <Border x:Name="TitleBar" Grid.Row="0" BorderThickness="0,0,0,2" BorderBrush="#4A7FD6" Padding="12,8">
                <Grid ColumnDefinitions="Auto,*">
                    <StackPanel Grid.Column="0" Orientation="Horizontal" Spacing="6" VerticalAlignment="Center">
                        <Ellipse Width="11" Height="11" Fill="#FF5F57" />
                        <Ellipse Width="11" Height="11" Fill="#FEBC2E" />
                        <Ellipse Width="11" Height="11" Fill="#28C840" />
                    </StackPanel>
                    <controls:WaveformControl x:Name="Waveform" Grid.Column="1" Height="12" HorizontalAlignment="Right" Width="40" />
                </Grid>
            </Border>
            <Grid Grid.Row="1" ColumnDefinitions="Auto,*" Margin="14,10" ColumnSpacing="12">
                <controls:CoverArtControl x:Name="Cover" Grid.Column="0" Width="72" Height="72" />
                <StackPanel Grid.Column="1" VerticalAlignment="Center" Spacing="4">
                    <TextBlock x:Name="TitleText" FontWeight="Bold" FontSize="13" Foreground="White" TextTrimming="CharacterEllipsis" />
                    <TextBlock x:Name="ArtistText" FontSize="11" Foreground="#B4FFFFFF" TextTrimming="CharacterEllipsis" />
                    <Grid ColumnDefinitions="*,Auto">
                        <TextBlock x:Name="PositionText" Grid.Column="0" FontSize="10" FontWeight="Bold" Foreground="White" />
                        <TextBlock x:Name="DurationText" Grid.Column="1" FontSize="10" FontWeight="Bold" Foreground="White" />
                    </Grid>
                    <controls:ProgressBarControl x:Name="Progress" Height="5" />
                </StackPanel>
            </Grid>
        </Grid>
    </Border>
</UserControl>
```

- [ ] **Step 2: Write `MacosPresetView.axaml.cs`**

```csharp
using System.IO;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using Wavely.App.Services;
using Wavely.Backend;

namespace Wavely.App.Views.Presets;

public partial class MacosPresetView : UserControl, Controls.IPresetView
{
    public MacosPresetView() => InitializeComponent();

    public void UpdateTrack(TrackInfo track)
    {
        TitleText.Text = string.IsNullOrEmpty(track.Title) ? "No track playing" : track.Title;
        ArtistText.Text = track.Artist;
        var coverArt = track.CoverArt;
        Cover.SetSource(coverArt is { Length: > 0 } ? new Bitmap(new MemoryStream(coverArt.ToArray())) : null);
    }

    public void UpdatePlayback(bool isPlaying, TimeSpan position, TimeSpan duration, double percent)
    {
        Cover.IsPlaying = isPlaying;
        PositionText.Text = Format(position);
        DurationText.Text = Format(duration);
        Progress.Percent = percent;
    }

    public void UpdateWaveform(ReadOnlySpan<float> bands) => Waveform.UpdateBands(bands);

    public void ApplyColors(WidgetColorScheme scheme, bool dynamicColorsEnabled, bool dynamicBackgroundEnabled)
    {
        var accent = dynamicColorsEnabled ? scheme.Accent : WidgetColorScheme.Default.Accent;
        Waveform.AccentColor = accent;
        Progress.AccentColor = accent;
        TitleBar.BorderBrush = new Avalonia.Media.SolidColorBrush(accent);
    }

    public void ApplyCoverAppearance(CoverStyle shape, bool glowEnabled)
    {
        Cover.Shape = shape;
        Cover.GlowEnabled = glowEnabled;
    }

    private static string Format(TimeSpan value) => value.ToString(@"m\:ss");
}
```

- [ ] **Step 3: Register it in `PresetCatalog`**

In `PresetCatalog.cs`, append to `Entries` (after `"Minimal"`):
```csharp
new("macOS", new Size(340, 122), () => new Views.Presets.MacosPresetView()),
```

- [ ] **Step 4: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).`

- [ ] **Step 5: Run and verify**

Run `Wavely.App.exe`, switch to macOS. Confirm the 340×122 layout renders (traffic-light dots, small waveform in the titlebar, accent-colored bottom border, cover + info column below), updates live.

- [ ] **Step 6: Commit**

```bash
git add frontend/Wavely.App/Views/Presets/MacosPresetView.axaml frontend/Wavely.App/Views/Presets/MacosPresetView.axaml.cs frontend/Wavely.App/Controls/PresetCatalog.cs
git commit -m "feat: add macOS preset (Phase 7.5)"
```

---

### Task 11: `ShellPresetView`

**Files:**
- Create: `frontend/Wavely.App/Views/Presets/ShellPresetView.axaml`, `.axaml.cs`
- Modify: `frontend/Wavely.App/Controls/PresetCatalog.cs` — append one entry.

**Spec source:** `ShellLayout.svelte`. Terminal window chrome (title bar with "root@wavely" + `– ▢ ✕` controls), monospace body: a command line, `Title:`/`Artist:` rows, an ASCII `[####----]` bar (26 chars wide, ported literally per the reference's own `BAR_WIDTH`), and a `m:ss - m:ss` times line. **No separate `ProgressBarControl`/`CoverArtControl`** — the reference has no cover art or graphical progress bar at all here, it's pure text; that omission is the preset's whole visual identity, not a cut corner.

- [ ] **Step 1: Write `ShellPresetView.axaml`**

```xml
<UserControl xmlns="https://github.com/avaloniaui"
             xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
             x:Class="Wavely.App.Views.Presets.ShellPresetView"
             FontFamily="Cascadia Code,Consolas,monospace">
    <Border Background="#1B1E26" CornerRadius="12" ClipToBounds="True">
        <Grid RowDefinitions="Auto,*">
            <Border Grid.Row="0" Background="#23262F" Padding="10,7">
                <Grid>
                    <TextBlock Text="root@wavely" FontSize="11" FontWeight="Bold" Foreground="#E8E8EC" HorizontalAlignment="Center" />
                    <TextBlock Text="– ▢ ✕" FontSize="9" Foreground="#9A9AA5" HorizontalAlignment="Right" />
                </Grid>
            </Border>
            <StackPanel Grid.Row="1" Margin="12,8" Spacing="3">
                <TextBlock FontSize="10.5">
                    <Run Text="root@wavely&gt;" Foreground="#E2A355" FontWeight="Bold" /><Run Text="./wavely" Foreground="#F0D0A8" /><Run Text=" --nowplaying" Foreground="#8A8A95" />
                </TextBlock>
                <StackPanel Orientation="Horizontal" Spacing="4">
                    <TextBlock Text="Title:" FontSize="10.5" FontWeight="Bold" Foreground="#F2F2F5" />
                    <TextBlock x:Name="TitleText" FontSize="10.5" Foreground="#F2F2F5" TextTrimming="CharacterEllipsis" />
                </StackPanel>
                <StackPanel Orientation="Horizontal" Spacing="4">
                    <TextBlock Text="Artist:" FontSize="10.5" FontWeight="Bold" Foreground="#F2F2F5" />
                    <TextBlock x:Name="ArtistText" FontSize="10.5" Foreground="#F2F2F5" TextTrimming="CharacterEllipsis" />
                </StackPanel>
                <TextBlock x:Name="BarText" FontSize="10.5" Foreground="#5A5A66" />
                <TextBlock x:Name="TimesText" FontSize="10.5" FontWeight="Bold" Foreground="#F2F2F5" />
            </StackPanel>
        </Grid>
    </Border>
</UserControl>
```

- [ ] **Step 2: Write `ShellPresetView.axaml.cs`**

```csharp
using Avalonia.Controls;
using Avalonia.Controls.Documents;
using Avalonia.Media;
using Wavely.App.Services;
using Wavely.Backend;

namespace Wavely.App.Views.Presets;

public partial class ShellPresetView : UserControl, Controls.IPresetView
{
    private const int BarWidth = 26;
    private static readonly IBrush FilledBrush = new SolidColorBrush(Color.FromRgb(0xE2, 0xA3, 0x55));
    private static readonly IBrush EmptyBrush = new SolidColorBrush(Color.FromRgb(0x5A, 0x5A, 0x66));

    public ShellPresetView() => InitializeComponent();

    public void UpdateTrack(TrackInfo track)
    {
        TitleText.Text = track.Title;
        ArtistText.Text = track.Artist;
    }

    public void UpdatePlayback(bool isPlaying, TimeSpan position, TimeSpan duration, double percent)
    {
        var filled = (int)Math.Round(percent / 100.0 * BarWidth);
        BarText.Inlines?.Clear();
        BarText.Inlines ??= [];
        BarText.Inlines.Add(new Run("[") { Foreground = EmptyBrush });
        BarText.Inlines.Add(new Run(new string('#', filled)) { Foreground = FilledBrush });
        BarText.Inlines.Add(new Run(new string('-', BarWidth - filled)) { Foreground = EmptyBrush });
        BarText.Inlines.Add(new Run("]") { Foreground = EmptyBrush });
        TimesText.Text = $"{Format(position)} - {Format(duration)}";
    }

    public void UpdateWaveform(ReadOnlySpan<float> bands)
    {
        // Shell has no waveform slot - it's a pure-text terminal, matching ShellLayout.svelte.
    }

    public void ApplyColors(WidgetColorScheme scheme, bool dynamicColorsEnabled, bool dynamicBackgroundEnabled)
    {
        // Shell's palette is fixed terminal colors in the reference, independent of the cover -
        // matches ShellLayout.svelte, which hardcodes every color rather than using --wavely-accent.
    }

    public void ApplyCoverAppearance(CoverStyle shape, bool glowEnabled)
    {
        // No cover art in this preset.
    }

    private static string Format(TimeSpan value) => value.ToString(@"m\:ss");
}
```

- [ ] **Step 3: Register it in `PresetCatalog`**

In `PresetCatalog.cs`, append to `Entries` (after `"macOS"`):
```csharp
new("Shell", new Size(360, 156), () => new Views.Presets.ShellPresetView()),
```

- [ ] **Step 4: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).`

- [ ] **Step 5: Run and verify**

Run `Wavely.App.exe`, switch to Shell. Confirm the 360×156 terminal card renders (title bar, command line, Title:/Artist: rows with real values, ASCII bar, times line), and the `#`/`-` split in the bar and the times line advance as playback progresses.

- [ ] **Step 6: Commit**

```bash
git add frontend/Wavely.App/Views/Presets/ShellPresetView.axaml frontend/Wavely.App/Views/Presets/ShellPresetView.axaml.cs frontend/Wavely.App/Controls/PresetCatalog.cs
git commit -m "feat: add Shell preset (Phase 7.6)"
```

---

### Task 12: `DiscordPresetView` (final preset)

**Files:**
- Create: `frontend/Wavely.App/Views/Presets/DiscordPresetView.axaml`, `.axaml.cs`
- Modify: `frontend/Wavely.App/Controls/PresetCatalog.cs` — append the final entry (7 of 7).

**Spec source:** `DiscordLayout.svelte`. "Wavely" wordmark + waveform(4) header; cover (66px/radius 13) with a white "active" bar and a chrome pill below it, next to title/artist/progress-row-with-thumb. The reference's `.active-indicator` pokes outside the card's left edge via a negative `left: -23px` — reproduced with `Margin` instead of `Canvas`-style absolute positioning, since Avalonia's default panels don't support negative-offset overflow without `Canvas`; a small `Canvas` wraps just that one element.

- [ ] **Step 1: Write `DiscordPresetView.axaml`**

```xml
<UserControl xmlns="https://github.com/avaloniaui"
             xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
             xmlns:controls="using:Wavely.App.Controls"
             x:Class="Wavely.App.Views.Presets.DiscordPresetView">
    <Border Background="#2B2D31" CornerRadius="16" Margin="8,0,0,0" Padding="16,10,16,10" ClipToBounds="True">
        <StackPanel Spacing="8">
            <Grid ColumnDefinitions="*,Auto">
                <TextBlock Grid.Column="0" Text="Wavely" FontWeight="Black" FontSize="20" Foreground="#DBDEE1" />
                <controls:WaveformControl x:Name="Waveform" Grid.Column="1" Height="16" Width="44" />
            </Grid>
            <Grid ColumnDefinitions="Auto,*" ColumnSpacing="14">
                <StackPanel Grid.Column="0" HorizontalAlignment="Center" Spacing="6">
                    <Canvas Width="66" Height="66">
                        <Rectangle Canvas.Left="-23" Canvas.Top="13" Width="8" Height="40" Fill="White" RadiusX="4" RadiusY="4" />
                        <controls:CoverArtControl x:Name="Cover" Width="66" Height="66" />
                    </Canvas>
                    <Rectangle Width="36" Height="4" Fill="#4A4D54" RadiusX="2" RadiusY="2" />
                </StackPanel>
                <StackPanel Grid.Column="1" VerticalAlignment="Center" Spacing="5">
                    <TextBlock x:Name="TitleText" FontSize="16" FontWeight="Black" Foreground="White" TextTrimming="CharacterEllipsis" />
                    <TextBlock x:Name="ArtistText" FontSize="12" Foreground="#B4FFFFFF" TextTrimming="CharacterEllipsis" />
                    <Grid ColumnDefinitions="Auto,*,Auto" ColumnSpacing="12">
                        <TextBlock x:Name="PositionText" Grid.Column="0" FontSize="12" FontWeight="Black" Foreground="White" VerticalAlignment="Center" />
                        <controls:ProgressBarControl x:Name="Progress" Grid.Column="1" Height="7" ShowThumb="True" VerticalAlignment="Center" />
                        <TextBlock x:Name="DurationText" Grid.Column="2" FontSize="12" FontWeight="Black" Foreground="White" VerticalAlignment="Center" />
                    </Grid>
                </StackPanel>
            </Grid>
        </StackPanel>
    </Border>
</UserControl>
```

- [ ] **Step 2: Write `DiscordPresetView.axaml.cs`**

```csharp
using System.IO;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using Wavely.App.Services;
using Wavely.Backend;

namespace Wavely.App.Views.Presets;

public partial class DiscordPresetView : UserControl, Controls.IPresetView
{
    public DiscordPresetView() => InitializeComponent();

    public void UpdateTrack(TrackInfo track)
    {
        TitleText.Text = string.IsNullOrEmpty(track.Title) ? "No track playing" : track.Title;
        ArtistText.Text = track.Artist;
        var coverArt = track.CoverArt;
        Cover.SetSource(coverArt is { Length: > 0 } ? new Bitmap(new MemoryStream(coverArt.ToArray())) : null);
    }

    public void UpdatePlayback(bool isPlaying, TimeSpan position, TimeSpan duration, double percent)
    {
        Cover.IsPlaying = isPlaying;
        PositionText.Text = Format(position);
        DurationText.Text = Format(duration);
        Progress.Percent = percent;
    }

    public void UpdateWaveform(ReadOnlySpan<float> bands) => Waveform.UpdateBands(bands);

    public void ApplyColors(WidgetColorScheme scheme, bool dynamicColorsEnabled, bool dynamicBackgroundEnabled)
    {
        var accent = dynamicColorsEnabled ? scheme.Accent : WidgetColorScheme.Default.Accent;
        Waveform.AccentColor = accent;
        Progress.AccentColor = accent;
    }

    public void ApplyCoverAppearance(CoverStyle shape, bool glowEnabled)
    {
        Cover.Shape = shape;
        Cover.GlowEnabled = glowEnabled;
    }

    private static string Format(TimeSpan value) => value.ToString(@"m\:ss");
}
```

- [ ] **Step 3: Register it in `PresetCatalog`**

In `PresetCatalog.cs`, append to `Entries` (after `"Shell"` — this is the 7th and final entry):
```csharp
new("Discord", new Size(360, 146), () => new Views.Presets.DiscordPresetView()),
```

- [ ] **Step 4: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).`

- [ ] **Step 5: Run and verify**

Run `Wavely.App.exe`, switch to Discord. Confirm the 360×146 card renders: wordmark + small waveform header, cover with the white active-indicator bar poking out past the card's left edge (not clipped) and the chrome pill below it, title/artist, and a progress bar whose thumb sits at the fill's right edge and tracks it as playback advances.

- [ ] **Step 6: Commit**

```bash
git add frontend/Wavely.App/Views/Presets/DiscordPresetView.axaml frontend/Wavely.App/Views/Presets/DiscordPresetView.axaml.cs frontend/Wavely.App/Controls/PresetCatalog.cs
git commit -m "feat: add Discord preset, completing all 7 Phase 7 presets"
```

---

### Task 13: Remove the "not yet available" caveat from Settings

**Files:**
- Modify: `frontend/Wavely.App/Views/SettingsWindow.axaml`

- [ ] **Step 1: Find and remove the caveat**

Search `SettingsWindow.axaml` for the text referenced by `Settings_Appearance_NotYetAvailable` (or similar key) in the Apparence tab and delete that `TextBlock`/row — preset, cover shape, glow, dynamic colors, and blur are now all rendered, so the caveat is stale. If the exact key name differs from this guess, grep for `NotYetAvailable` first to confirm.

- [ ] **Step 2: Build and verify**

Run: `.\build.ps1 -Configuration Debug`
Open Settings → Apparence and confirm the tab no longer shows a "not yet available" notice.

- [ ] **Step 3: Commit**

```bash
git add frontend/Wavely.App/Views/SettingsWindow.axaml
git commit -m "chore: remove stale not-yet-available notice now that Phase 7 presets render"
```

---

### Task 14: Full manual verification pass

**No new files** — this task is pure verification, run with a real GSMTC session (Spotify), following the same screenshot-driven methodology already used for Phases 1-6 (UI Automation to drive the Settings preset ComboBox, screenshots to confirm each preset's rendering, real pause/resume to confirm vinyl-spin and progress-bar behavior carry over into every preset that has a `CoverArtControl`/`ProgressBarControl`).

- [ ] **Step 1:** For each of the 7 presets (cycle "Preset" in Settings → Apparence), screenshot the widget with a real playing track and confirm: correct window size (matches `PresetCatalog`), cover renders, title/artist readable, progress bar advances over ~2s between two screenshots, time labels are accurate against real elapsed playback.
- [ ] **Step 2:** With a preset that has a waveform slot (Compact, Boxy, macOS, Discord), confirm the bars react to real audio.
- [ ] **Step 3:** With Shell selected, confirm the ASCII bar's `#`/`-` count moves as playback progresses and the two text rows show real title/artist.
- [ ] **Step 4:** With Discord selected, confirm the progress thumb tracks the fill's right edge and the white active-indicator bar renders outside the card's left edge without being clipped.
- [ ] **Step 5:** Toggle CoverShape (Square/Squircle/Vinyl) and CoverGlow while on a preset with a cover, confirm both apply live exactly as they did pre-Phase-7 (Task 4 was a behavior-preserving extraction).
- [ ] **Step 6:** Resize the widget (mouse wheel, 50%-150%) on at least 2 presets, confirm all elements scale together with no overlap/clipping.
- [ ] **Step 7:** Update `claude/PLAN.md`'s Phase 7 section with a "Statut" note in the same style as Phases 0-6, documenting what was verified and any deviations found during this pass.

---

## Plan Self-Review

**Spec coverage:** All 7 presets (Compact, Boxy, Gallery, Minimal, macOS, Shell, Discord) → Task 6 (Compact, bundled with the engine) + Tasks 7-12 (the remaining six), each grounded in its own `*Layout.svelte` file and `layouts.ts`'s window size. Progress bar + time labels (present in every preset per the reference) → Tasks 1-3 (backend position + interpolation + shared control). Cover shape/glow/vinyl-spin (Phase 6, must survive into every preset) → Task 4's behavior-preserving extraction, consumed by every preset task except Shell (which has no cover in the reference). Generic runtime preset-switching (`PROMPT.md`'s explicit requirement) → Task 5 (`IPresetView`) + Task 6 (`PresetCatalog` + `MainWindow` wiring off `AppConfig.PresetIndex`, already persisted since Phase 4), grown by one `PresetCatalog` entry per preset task thereafter so every task stays independently buildable. Settings copy cleanup → Task 13.

**Explicitly out of scope, by user confirmation:** `EqualizerBars.svelte`, `BlurBackdrop.svelte` verbatim ports (real `WaveformControl` and the existing Phase 6 blur mechanism are used instead — see Global Constraints).

**Type/name consistency check:** `IPresetView`'s five methods (`UpdateTrack`, `UpdatePlayback`, `UpdateWaveform`, `ApplyColors`, `ApplyCoverAppearance`) match exactly across Task 5's declaration and all 7 implementations in Tasks 6-12. `PresetCatalog.Entries`/`PresetEntry`/`Resolve` (introduced in Task 6, grown by Tasks 7-12) match their only call site in Task 6's `MainWindow` wiring. `CoverArtControl.SetSource/Shape/GlowEnabled/GlowColor/IsPlaying` (Task 4) match every preset's usage. `ProgressBarControl.Percent/AccentColor/ShowThumb` (Task 3) match every preset's usage (only Discord sets `ShowThumb`). `PlaybackPositionTracker.Tick`/`Sync`/`SetPlaying` (Task 2) match their only call sites in Task 6. `TrackInfo.PositionMs` (Task 1) matches `PlaybackPositionTracker.Sync`'s usage (Task 2).
