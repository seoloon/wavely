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
