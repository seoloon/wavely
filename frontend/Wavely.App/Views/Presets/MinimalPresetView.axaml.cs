using System.IO;
using System.Runtime.InteropServices.WindowsRuntime;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Wavely.App.Services;
using Wavely.Backend;

namespace Wavely.App.Views.Presets;

public partial class MinimalPresetView : UserControl, Controls.IPresetView
{
    private static readonly IBrush LightLabelForeground = Brushes.White;
    private static readonly IBrush DarkLabelForeground = Brushes.Black;

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
        Cover.GlowColor = dynamicColorsEnabled ? scheme.Glow : WidgetColorScheme.Default.Glow;

        var textIsDark = dynamicColorsEnabled && scheme.TextIsDark;
        LabelText.Foreground = textIsDark ? DarkLabelForeground : LightLabelForeground;
    }

    public void ApplyCoverAppearance(CoverStyle shape, bool glowEnabled)
    {
        Cover.Shape = shape;
        Cover.GlowEnabled = glowEnabled;
    }
}
