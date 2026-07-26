using System.Runtime.InteropServices;
using System.Runtime.InteropServices.WindowsRuntime;
using Avalonia.Media;
using Wavely.Backend;
using Windows.Storage.Streams;

namespace Wavely.App.Services;

/// <summary>Resolved colors for one track, ready to apply to the widget's background, waveform
/// accent, glow, and text. See
/// docs/superpowers/specs/2026-07-26-phase6-dynamic-color-effects-design.md.</summary>
public sealed record WidgetColorScheme(Color Background, Color Accent, Color Glow, bool TextIsDark)
{
    /// <summary>The look from before Phase 6: fixed dark background, blue waveform accent, white
    /// text, white glow. Used whenever there's no cover or its palette couldn't be decoded -
    /// callers additionally fall back to this per-element when the user has a dynamic-color
    /// toggle turned off.</summary>
    public static readonly WidgetColorScheme Default = new(
        Background: Color.FromRgb(0x14, 0x14, 0x18),
        Accent: Color.FromArgb(220, 90, 170, 255),
        Glow: Colors.White,
        TextIsDark: false);
}

/// <summary>Turns a track's backend-extracted <see cref="TrackInfo.DominantColors"/> into a
/// <see cref="WidgetColorScheme"/>. Does not know about <c>AppConfig</c> toggles - callers decide
/// per-element whether to use the resolved scheme or <see cref="WidgetColorScheme.Default"/>,
/// since background/accent/glow each have their own independent enable toggle.</summary>
public static class DynamicColorService
{
    /// <summary>Relative luminance above which white text stops being reliably legible and the
    /// widget should switch to dark text instead (WCAG relative luminance, 0=black, 1=white).</summary>
    private const double DarkTextLuminanceThreshold = 0.6;

    public static WidgetColorScheme Resolve(TrackInfo track)
    {
        var palette = Unpack(track.DominantColors);
        if (palette is null)
        {
            return WidgetColorScheme.Default;
        }

        var background = palette[0];
        var accent = palette[1];
        var textIsDark = RelativeLuminance(background) > DarkTextLuminanceThreshold;
        return new WidgetColorScheme(background, accent, accent, textIsDark);
    }

    /// <summary>Decodes the 5x little-endian-uint32 0xAARRGGBB buffer packed by the backend's
    /// ColorExtractor (see ColorExtractor.h). Null for no cover / undecodable cover, matching how
    /// MainWindow already treats an empty CoverArt buffer.</summary>
    private static Color[]? Unpack(IBuffer dominantColors)
    {
        if (dominantColors is not { Length: > 0 } buffer)
        {
            return null;
        }

        var packed = MemoryMarshal.Cast<byte, uint>(buffer.ToArray());
        var colors = new Color[packed.Length];
        for (var i = 0; i < packed.Length; i++)
        {
            var argb = packed[i];
            colors[i] = Color.FromArgb((byte)(argb >> 24), (byte)(argb >> 16), (byte)(argb >> 8), (byte)argb);
        }
        return colors;
    }

    private static double RelativeLuminance(Color color) =>
        (0.2126 * color.R + 0.7152 * color.G + 0.0722 * color.B) / 255.0;
}
