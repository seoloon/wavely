using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Wavely.App.Core;
using Wavely.Backend;

namespace Wavely.App.Controls;

/// <summary>
/// Self-contained cover art: square/squircle/vinyl clip shape, optional glow, and vinyl rotation
/// while playing. Extracted from MainWindow (Phase 6) so every Phase 7 preset can place a cover
/// without duplicating this logic - behavior is unchanged from the already-verified Phase 6
/// implementation, only the driving fields became properties.
/// </summary>
public partial class CoverArtControl : UserControl
{
    private const double DefaultCornerRadius = 8.0;
    private const double GlowBlurRadius = 18.0;
    private const double GlowOpacity = 0.9;
    private const double VinylRotationDegreesPerSecond = 90.0; // 360 degrees every 4 seconds.

    private readonly DispatcherTimer _rotationTimer;
    private readonly RotateTransform _rotateTransform;
    private CoverStyle _shape = CoverStyle.Square;
    private bool _glowEnabled;
    private Color _glowColor = Colors.White;
    private bool _isPlaying;

    public CoverArtControl()
    {
        InitializeComponent();
        _rotateTransform = (RotateTransform)CoverImage.RenderTransform!;
        _rotationTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(16) };
        _rotationTimer.Tick += (_, _) =>
            _rotateTransform.Angle = (_rotateTransform.Angle
                + VinylRotationDegreesPerSecond * _rotationTimer.Interval.TotalSeconds) % 360.0;
        Root.SizeChanged += (_, _) => ApplyShape();
    }

    public void SetSource(Bitmap? bitmap) => CoverImage.Source = bitmap;

    /// <summary>The currently displayed bitmap (or null), so callers that need to reuse the
    /// already-decoded pixels elsewhere (e.g. MainWindow's blurred background) don't have to keep
    /// their own separate copy of the same source in sync.</summary>
    public Bitmap? Source => (Bitmap?)CoverImage.Source;

    public CoverStyle Shape
    {
        get => _shape;
        set { _shape = value; ApplyShape(); }
    }

    public bool GlowEnabled
    {
        get => _glowEnabled;
        set { _glowEnabled = value; ApplyGlow(); }
    }

    public Color GlowColor
    {
        get => _glowColor;
        set { _glowColor = value; ApplyGlow(); }
    }

    public bool IsPlaying
    {
        get => _isPlaying;
        set { _isPlaying = value; UpdateRotationState(); }
    }

    private void ApplyShape()
    {
        var size = Root.Bounds.Width;
        switch (_shape)
        {
            case CoverStyle.Square:
                Root.ClipToBounds = true;
                Root.CornerRadius = new CornerRadius(DefaultCornerRadius);
                Root.Clip = null;
                VinylSpindle.IsVisible = false;
                break;
            case CoverStyle.Squircle:
                Root.ClipToBounds = false;
                Root.CornerRadius = new CornerRadius(0);
                Root.Clip = SquircleGeometry.ForSize(size);
                VinylSpindle.IsVisible = false;
                break;
            case CoverStyle.Vinyl:
                Root.ClipToBounds = false;
                Root.CornerRadius = new CornerRadius(0);
                Root.Clip = new EllipseGeometry(new Rect(0, 0, size, size));
                VinylSpindle.IsVisible = true;
                break;
        }
        UpdateRotationState();
    }

    private void ApplyGlow()
    {
        if (!_glowEnabled)
        {
            Root.Effect = null;
            return;
        }
        Root.Effect = new DropShadowDirectionEffect
        {
            Color = _glowColor,
            BlurRadius = GlowBlurRadius,
            Direction = 0.0,
            ShadowDepth = 0.0,
            Opacity = GlowOpacity,
        };
    }

    private void UpdateRotationState()
    {
        var shouldRotate = _shape == CoverStyle.Vinyl && _isPlaying;
        if (shouldRotate && !_rotationTimer.IsEnabled)
        {
            _rotationTimer.Start();
        }
        else if (!shouldRotate && _rotationTimer.IsEnabled)
        {
            _rotationTimer.Stop();
        }
    }
}
