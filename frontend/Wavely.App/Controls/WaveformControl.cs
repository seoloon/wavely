using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;

namespace Wavely.App.Controls;

/// <summary>
/// Draws the live waveform as an equalizer: bars grow symmetrically from the vertical center
/// (matching assets/presets_reference/EqualizerBars.svelte's look), each one reflecting a
/// log-spaced frequency band's *current* magnitude - not a left-to-right amplitude-over-time
/// strip, which reads as a scrolling timeline rather than a live EQ. Custom-drawn (RULES.md:
/// GPU-accelerated via Avalonia's Skia renderer) rather than one control per bar, since bars
/// redrawing at ~60fps as individual bound elements would allocate/layout far more than a single
/// Render call.
/// </summary>
public sealed class WaveformControl : Control
{
    private static readonly IBrush BarBrush = new SolidColorBrush(Color.FromArgb(220, 90, 170, 255));
    private const double MinBarScale = 0.12;
    private const double BarGap = 3.0;
    private const double BarCornerRadius = 2.0;

    private float[] _bands = [];

    public void UpdateBands(ReadOnlySpan<float> bands)
    {
        if (_bands.Length != bands.Length)
        {
            _bands = new float[bands.Length];
        }
        bands.CopyTo(_bands);
        InvalidateVisual();
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);

        var bounds = Bounds;
        var barCount = _bands.Length;
        if (bounds.Width <= 0 || bounds.Height <= 0 || barCount == 0)
        {
            return;
        }

        var totalGap = BarGap * (barCount - 1);
        var barWidth = Math.Max(1.0, (bounds.Width - totalGap) / barCount);
        var centerY = bounds.Height / 2.0;

        for (var i = 0; i < barCount; i++)
        {
            var amplitude = Math.Clamp(_bands[i], 0f, 1f);
            var scale = Math.Max(MinBarScale, amplitude);
            var barHeight = scale * bounds.Height;
            var x = i * (barWidth + BarGap);
            var y = centerY - barHeight / 2.0;
            context.DrawRectangle(BarBrush, null, new Rect(x, y, barWidth, barHeight), BarCornerRadius, BarCornerRadius);
        }
    }
}
