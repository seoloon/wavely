using System.IO;
using System.Runtime.InteropServices.WindowsRuntime;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Wavely.App.Services;
using Wavely.Backend;

namespace Wavely.App.Views.Presets;

public partial class MacosPresetView : UserControl, Controls.IPresetView
{
    private const int WaveformBarCount = 4;

    private static readonly IBrush LightTitleForeground = Brushes.White;
    private static readonly IBrush DarkTitleForeground = Brushes.Black;
    private static readonly IBrush LightArtistForeground = new SolidColorBrush(Color.FromArgb(0xB4, 0xFF, 0xFF, 0xFF));
    private static readonly IBrush DarkArtistForeground = new SolidColorBrush(Color.FromArgb(0xB4, 0x00, 0x00, 0x00));

    public MacosPresetView()
    {
        InitializeComponent();
        Waveform.DisplayBarCount = WaveformBarCount;
    }

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
        TitleBar.BorderBrush = new SolidColorBrush(accent);
        Cover.GlowColor = dynamicColorsEnabled ? scheme.Glow : WidgetColorScheme.Default.Glow;

        var textIsDark = dynamicColorsEnabled && scheme.TextIsDark;
        TitleText.Foreground = textIsDark ? DarkTitleForeground : LightTitleForeground;
        ArtistText.Foreground = textIsDark ? DarkArtistForeground : LightArtistForeground;
    }

    public void ApplyCoverAppearance(CoverStyle shape, bool glowEnabled)
    {
        Cover.Shape = shape;
        Cover.GlowEnabled = glowEnabled;
    }

    public void ApplyBlurredBackground(Bitmap? blurredCover, bool enabled)
    {
        BlurredCoverImage.Source = blurredCover;
        BlurredCoverImage.IsVisible = enabled && blurredCover is not null;
    }

    private static string Format(TimeSpan value) => value.ToString(@"m\:ss");
}
