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

    /// <summary>True when the preset hosts its own dedicated blurred-cover background layer
    /// (see <see cref="ApplyBlurredBackground"/>) and its own complete card background, so
    /// MainWindow should suppress its shared window-level blur AND its shared tint entirely -
    /// including in any outer margin/gutter around the preset's own chrome - rather than let a
    /// second, differently-treated background layer show through underneath (see Tasks 17, 24, 27).
    /// Defaults to false for the other 5 presets, which need no change.</summary>
    bool HasOwnBlurredBackground => false;
}
