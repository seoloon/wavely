using System.IO;
using System.Runtime.InteropServices.WindowsRuntime;
using Avalonia;
using Avalonia.Media;
using Wavely.App.Controls;
using Wavely.App.Services;
using Wavely.Backend;

namespace Wavely.App.Views;

/// <summary>
/// Phase 6/7 rendering logic for <see cref="MainWindow"/> - dynamic colors, blurred background,
/// and cover shape/rotation - split into its own partial-class file so the main
/// MainWindow.axaml.cs (window lifecycle, drag, click-through) doesn't grow past RULES.md's
/// ~200-line guidance for a single class. Since Phase 7, per-element rendering (text, waveform,
/// glow) is delegated to whichever <see cref="IPresetView"/> is currently active - this file only
/// still owns what's outside PresetHost (the shared background chrome behind every preset).
/// </summary>
public partial class MainWindow
{
    private const double BackgroundBlurRadius = 24.0;

    /// <summary>Applies the appearance settings that don't depend on the cover's palette:
    /// background opacity (baked into the background brush's own alpha, not the whole window's
    /// Opacity, so text/icons stay fully readable) and the app-wide dark/light theme variant.</summary>
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

    /// <summary>Applies the track's dominant-color palette to the shared background tint and
    /// forwards the resolved scheme to the active preset for its own text/waveform/glow - each
    /// gated by its own AppConfig toggle, falling back to <see cref="WidgetColorScheme.Default"/>
    /// per-element when that toggle is off.</summary>
    private void ApplyDynamicColors(TrackInfo track)
    {
        var scheme = DynamicColorService.Resolve(track);

        if (BackgroundTintBorder.Background is SolidColorBrush backgroundBrush)
        {
            backgroundBrush.Color = _config.DynamicBackgroundEnabled ? scheme.Background : WidgetColorScheme.Default.Background;
        }

        _activePreset.ApplyColors(scheme, _config.DynamicColorsEnabled, _config.DynamicBackgroundEnabled);
    }

    /// <summary>Applies the cover's clip shape and glow toggle from AppConfig to the active
    /// preset - split out from <see cref="ApplyDynamicColors"/> so it also runs before the first
    /// track arrives (glow's color still defaults to <see cref="WidgetColorScheme.Default"/>
    /// until then) and whenever the preset itself is swapped.</summary>
    private void ApplyCoverAppearanceOnActivePreset()
    {
        _activePreset.ApplyCoverAppearance(_config.CoverShape, _config.CoverGlowEnabled);
    }

    /// <summary>Shows a heavily blurred copy of the current cover art behind the widget's
    /// content when enabled - reads straight from <see cref="_currentTrack"/> rather than
    /// through the active preset, since there's no single named cover element on MainWindow
    /// anymore (each preset owns its own <see cref="CoverArtControl"/>).</summary>
    private void ApplyBlurBackground()
    {
        Avalonia.Media.Imaging.Bitmap? bitmap = null;
        if (_currentTrack is { CoverArt: { Length: > 0 } coverArt })
        {
            using var stream = new MemoryStream(coverArt.ToArray());
            bitmap = new Avalonia.Media.Imaging.Bitmap(stream);
        }
        BlurBackgroundImage.Source = bitmap;
        BlurBackgroundImage.IsVisible = _config.CoverBlurEnabled && bitmap is not null && !_activePreset.HasOwnBlurredBackground;
        _activePreset.ApplyBlurredBackground(bitmap, _config.CoverBlurEnabled);
    }
}
