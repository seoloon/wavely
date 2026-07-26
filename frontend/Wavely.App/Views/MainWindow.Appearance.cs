using Avalonia;
using Avalonia.Media;
using Wavely.App.Services;
using Wavely.Backend;

namespace Wavely.App.Views;

/// <summary>
/// Phase 6 rendering logic for <see cref="MainWindow"/> - dynamic colors, blurred background,
/// glow, and cover shape/rotation - split into its own partial-class file so the main
/// MainWindow.axaml.cs (window lifecycle, drag, click-through) doesn't grow past RULES.md's
/// ~200-line guidance for a single class.
/// </summary>
public partial class MainWindow
{
    private const double BackgroundBlurRadius = 24.0;
    private const double GlowBlurRadius = 18.0;
    private const double GlowOpacity = 0.9;

    private static readonly IBrush LightTitleForeground = Brushes.White;
    private static readonly IBrush DarkTitleForeground = Brushes.Black;
    private static readonly IBrush LightArtistForeground = new SolidColorBrush(Color.FromArgb(0xB4, 0xFF, 0xFF, 0xFF));
    private static readonly IBrush DarkArtistForeground = new SolidColorBrush(Color.FromArgb(0xB4, 0x00, 0x00, 0x00));
    private static readonly IBrush LightStatusForeground = new SolidColorBrush(Color.FromArgb(0x78, 0xFF, 0xFF, 0xFF));
    private static readonly IBrush DarkStatusForeground = new SolidColorBrush(Color.FromArgb(0x78, 0x00, 0x00, 0x00));

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

    /// <summary>Applies the track's dominant-color palette to the background, waveform accent,
    /// and text - each gated by its own AppConfig toggle, falling back to
    /// <see cref="WidgetColorScheme.Default"/> per-element when that toggle is off.</summary>
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

        ApplyGlow(_config.DynamicColorsEnabled ? scheme.Glow : WidgetColorScheme.Default.Glow);
    }

    /// <summary>Shows a heavily blurred copy of the current cover art behind the widget's
    /// content when enabled - reuses the already-decoded CoverImage.Source bitmap rather than
    /// re-decoding the cover, since both images just need the same pixels at different
    /// treatments.</summary>
    private void ApplyBlurBackground()
    {
        BlurBackgroundImage.Source = CoverImage.Source;
        BlurBackgroundImage.IsVisible = _config.CoverBlurEnabled && CoverImage.Source is not null;
    }

    /// <summary>Applies (or clears) a colored halo around the cover. Color follows the dynamic
    /// palette when enabled, otherwise a neutral white glow - independent of whether dynamic
    /// colors are on, the glow's presence is controlled only by CoverGlowEnabled.</summary>
    private void ApplyGlow(Color glowColor)
    {
        if (!_config.CoverGlowEnabled)
        {
            CoverBorder.Effect = null;
            return;
        }

        CoverBorder.Effect = new DropShadowDirectionEffect
        {
            Color = glowColor,
            BlurRadius = GlowBlurRadius,
            Direction = 0.0,
            ShadowDepth = 0.0,
            Opacity = GlowOpacity,
        };
    }
}
