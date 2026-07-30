using System.IO;
using System.Runtime.InteropServices.WindowsRuntime;
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
