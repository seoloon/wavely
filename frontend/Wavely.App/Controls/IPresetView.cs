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

    /// <summary>Forwards the shared blurred-cover bitmap to presets that host their own
    /// dedicated blurred-cover background layer (currently Discord and macOS - see Task 17).
    /// Default no-op body so the other presets, which rely solely on the window-level shared
    /// blur behind their own translucent/opaque chrome, don't need any change.</summary>
    void ApplyBlurredBackground(Avalonia.Media.Imaging.Bitmap? blurredCover, bool enabled) { }
}
