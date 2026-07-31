using System;
using System.Threading;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Animation;
using Avalonia.Animation.Easings;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Styling;

namespace Wavely.App.Controls;

/// <summary>
/// Drop-in replacement for a trimmed TextBlock: shows text normally when it fits, and otherwise
/// scrolls it back and forth (ease-in-out, pausing at each edge) so long titles/artists stay
/// fully readable instead of being cut off.
/// </summary>
public partial class MarqueeTextBlock : UserControl
{
    private const double PixelsPerSecond = 38.0;
    private const double OverflowThresholdPx = 1.0;
    private static readonly TimeSpan EdgePause = TimeSpan.FromSeconds(1.1);
    private static readonly TimeSpan MinTravelDuration = TimeSpan.FromSeconds(1.2);

    public static readonly StyledProperty<string?> TextProperty =
        AvaloniaProperty.Register<MarqueeTextBlock, string?>(nameof(Text));

    public new static readonly StyledProperty<IBrush?> ForegroundProperty =
        AvaloniaProperty.Register<MarqueeTextBlock, IBrush?>(nameof(Foreground));

    private readonly TranslateTransform _translate;
    private CancellationTokenSource? _cts;

    public MarqueeTextBlock()
    {
        InitializeComponent();
        _translate = (TranslateTransform)InnerText.RenderTransform!;
        SizeChanged += (_, _) => RestartMarquee();
    }

    public string? Text
    {
        get => GetValue(TextProperty);
        set => SetValue(TextProperty, value);
    }

    public new IBrush? Foreground
    {
        get => GetValue(ForegroundProperty);
        set => SetValue(ForegroundProperty, value);
    }

    protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change)
    {
        base.OnPropertyChanged(change);
        if (change.Property == TextProperty)
        {
            InnerText.Text = Text;
            RestartMarquee();
        }
        else if (change.Property == ForegroundProperty)
        {
            InnerText.Foreground = Foreground;
        }
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnDetachedFromVisualTree(e);
        _cts?.Cancel();
    }

    /// <summary>Re-measures the text against the available width and (re)starts the ping-pong
    /// scroll loop only when it actually overflows; called on every Text/size change so a preset
    /// swap or a shorter track title cleanly falls back to a static, centered label.</summary>
    private void RestartMarquee()
    {
        _cts?.Cancel();
        _translate.X = 0;

        var available = Bounds.Width;
        if (available <= 0)
        {
            return;
        }

        InnerText.Measure(Size.Infinity);
        var overflow = InnerText.DesiredSize.Width - available;
        if (overflow <= OverflowThresholdPx)
        {
            return;
        }

        _cts = new CancellationTokenSource();
        _ = RunLoopAsync(overflow, _cts.Token);
    }

    private async Task RunLoopAsync(double distance, CancellationToken token)
    {
        var duration = TimeSpan.FromSeconds(Math.Max(distance / PixelsPerSecond, MinTravelDuration.TotalSeconds));
        try
        {
            while (!token.IsCancellationRequested)
            {
                await Task.Delay(EdgePause, token);
                await AnimateAsync(0, -distance, duration, token);
                await Task.Delay(EdgePause, token);
                await AnimateAsync(-distance, 0, duration, token);
            }
        }
        catch (OperationCanceledException)
        {
            // Expected when the text/size changes or the control leaves the visual tree.
        }
    }

    private async Task AnimateAsync(double from, double to, TimeSpan duration, CancellationToken token)
    {
        var animation = new Animation
        {
            Duration = duration,
            Easing = new CubicEaseInOut(),
            FillMode = FillMode.Forward,
            Children =
            {
                new KeyFrame { Cue = new Cue(0.0), Setters = { new Setter(TranslateTransform.XProperty, from) } },
                new KeyFrame { Cue = new Cue(1.0), Setters = { new Setter(TranslateTransform.XProperty, to) } },
            },
        };
        await animation.RunAsync(_translate, token);
    }
}
