using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;

namespace Wavely.App.Controls;

/// <summary>
/// Draws a track+fill progress bar (height-derived corner radius, like every *Layout.svelte's
/// .progress-track/.progress-fill), with an optional circular thumb (Discord preset only).
/// Custom-drawn rather than a styled Avalonia ProgressBar - the reference's rounded pill shape
/// and thumb aren't reachable through ProgressBar's default template without a full retemplate,
/// and this is a handful of DrawRectangle/DrawEllipse calls.
/// </summary>
public sealed class ProgressBarControl : Control
{
    private const double TrackTintWithBlack = 0.35;
    private const double ThumbWidth = 8.0;
    private const double ThumbHeightOverBarHeight = 3.2;

    private static readonly Color DefaultAccentColor = Color.FromArgb(220, 90, 170, 255);
    private static readonly IBrush ThumbBrush = Brushes.White;

    private double _percent;
    private Color _accentColor = DefaultAccentColor;
    private bool _showThumb;

    public double Percent
    {
        get => _percent;
        set { _percent = Math.Clamp(value, 0.0, 100.0); InvalidateVisual(); }
    }

    public Color AccentColor
    {
        get => _accentColor;
        set { _accentColor = value; InvalidateVisual(); }
    }

    public bool ShowThumb
    {
        get => _showThumb;
        set { _showThumb = value; InvalidateVisual(); }
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);

        var bounds = Bounds;
        if (bounds.Width <= 0 || bounds.Height <= 0)
        {
            return;
        }

        var radius = bounds.Height / 2.0;
        var trackColor = MixWithBlack(_accentColor, TrackTintWithBlack);
        context.DrawRectangle(new SolidColorBrush(trackColor), null, new Rect(0, 0, bounds.Width, bounds.Height), radius, radius);

        var fillWidth = bounds.Width * (_percent / 100.0);
        if (fillWidth > 0)
        {
            context.DrawRectangle(new SolidColorBrush(_accentColor), null, new Rect(0, 0, fillWidth, bounds.Height), radius, radius);
        }

        if (_showThumb)
        {
            var thumbHeight = bounds.Height * ThumbHeightOverBarHeight;
            var thumbRect = new Rect(fillWidth - ThumbWidth / 2.0, bounds.Height / 2.0 - thumbHeight / 2.0, ThumbWidth, thumbHeight);
            context.DrawRectangle(ThumbBrush, null, thumbRect, ThumbWidth / 2.0, ThumbWidth / 2.0);
        }
    }

    private static Color MixWithBlack(Color color, double colorWeight) =>
        Color.FromArgb(
            color.A,
            (byte)(color.R * colorWeight),
            (byte)(color.G * colorWeight),
            (byte)(color.B * colorWeight));
}
