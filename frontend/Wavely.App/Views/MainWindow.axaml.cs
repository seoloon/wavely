using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.WindowsRuntime;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Wavely.Backend;
using Windows.Storage.Streams;

namespace Wavely.App.Views;

/// <summary>
/// Frameless, translucent top-level window hosting the Wavely overlay widget: draggable via the
/// OS caption trick (multi-monitor for free, WM_NCHITTEST hook below), resizable 50%-150%,
/// toggleable click-through, and auto-hides after a delay when playback pauses.
/// </summary>
public partial class MainWindow : Window
{
    private const double DefaultWidth = 360;
    private const double DefaultHeight = 156;
    private const double CoverSize = 88;
    private const double WaveformHeight = 28;
    private const double ScaleStep = 0.1;
    private const int HandleMargin = 4;

    private const uint WM_NCHITTEST = 0x0084;
    private const uint WM_EXITSIZEMOVE = 0x0232;
    private const nint HTCAPTION = 2;
    private const int GWL_EXSTYLE = -20;
    private const long WS_EX_TRANSPARENT = 0x00000020;
    private const int VK_CONTROL = 0x11;

    private readonly AppConfig _config;
    private readonly MediaSessionManager _sessionManager;
    private readonly WaveformEngine _waveformEngine;
    private readonly DispatcherTimer _hideTimer;
    private readonly DispatcherTimer _hideAfterFadeTimer;
    private readonly ClickThroughHandle _clickThroughHandle = new();
    private Win32Properties.CustomWndProcHookCallback? _wndProcHook;
    private IntPtr _hwnd;
    private bool _hiddenByAutoHide;

    public MainWindow(AppConfig config, MediaSessionManager sessionManager, WaveformEngine waveformEngine)
    {
        InitializeComponent();

        _config = config;
        _sessionManager = sessionManager;
        _waveformEngine = waveformEngine;

        _hideTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromSeconds(Math.Clamp(_config.HideOnPauseDelaySeconds, 5, 30)),
        };
        _hideTimer.Tick += (_, _) => FadeOutAndHide();

        _hideAfterFadeTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(300) };
        _hideAfterFadeTimer.Tick += (_, _) =>
        {
            _hideAfterFadeTimer.Stop();
            Hide();
            _clickThroughHandle.Hide();
            _hiddenByAutoHide = true;
        };

        _clickThroughHandle.HandleClicked += (_, _) => SetClickThroughEnabled(false);

        PointerWheelChanged += OnPointerWheelChanged;
        PointerPressed += OnPointerPressed;
        PositionChanged += (_, _) => MoveClickThroughHandle();

        _sessionManager.TrackChanged += OnTrackChanged;
        _sessionManager.PlaybackStateChanged += OnPlaybackStateChanged;
        _waveformEngine.WaveformDataReady += OnWaveformDataReady;

        Opened += OnOpened;
        Closing += OnClosing;
    }

    private void OnOpened(object? sender, EventArgs e)
    {
        var handle = TryGetPlatformHandle();
        if (handle is not null)
        {
            _hwnd = handle.Handle;
            _wndProcHook = WndProcHook;
            Win32Properties.AddWndProcHookCallback(this, _wndProcHook);
        }

        var geometry = _config.Geometry;
        Position = new PixelPoint(geometry.PositionX, geometry.PositionY);
        ApplyScale(geometry.Scale);
        ApplyClickThrough(_config.ClickThroughEnabled);
        ApplyAppearance();

        FadeIn();
    }

    private void OnClosing(object? sender, WindowClosingEventArgs e)
    {
        // The tray icon is what keeps the app alive: closing the widget only hides it, it does
        // not quit. Only the tray's "Quit" action terminates the process.
        e.Cancel = true;
        Hide();
        _clickThroughHandle.Hide();
    }

    private IntPtr WndProcHook(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam, ref bool handled)
    {
        if (msg == WM_NCHITTEST)
        {
            if (!_config.Locked && !IsControlKeyDown())
            {
                handled = true;
                return HTCAPTION;
            }
        }
        else if (msg == WM_EXITSIZEMOVE)
        {
            PersistCurrentPosition();
        }
        return IntPtr.Zero;
    }

    private static bool IsControlKeyDown() => (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    private void OnPointerWheelChanged(object? sender, PointerWheelEventArgs e)
    {
        if (_config.Locked)
        {
            return;
        }
        var direction = e.Delta.Y > 0 ? 1.0 : -1.0;
        ApplyScale(_config.Geometry.Scale + direction * ScaleStep);
        e.Handled = true;
    }

    private void OnPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        var point = e.GetCurrentPoint(this);
        if (point.Properties.IsLeftButtonPressed && e.KeyModifiers.HasFlag(KeyModifiers.Control))
        {
            SetClickThroughEnabled(!_config.ClickThroughEnabled);
            e.Handled = true;
        }
    }

    private void ApplyScale(double scale)
    {
        var geometry = _config.Geometry;
        _config.SetGeometry(new WidgetGeometry
        {
            PositionX = geometry.PositionX,
            PositionY = geometry.PositionY,
            Scale = scale,
        });
        ApplyVisualScale(_config.Geometry.Scale);
    }

    private void ApplyVisualScale(double scale)
    {
        Width = DefaultWidth * scale;
        Height = DefaultHeight * scale;
        CoverBorder.Width = CoverSize * scale;
        CoverBorder.Height = CoverSize * scale;
        Waveform.Height = WaveformHeight * scale;
    }

    /// <summary>Re-applies state that the Settings window may have changed on the shared
    /// AppConfig (scale, click-through, hide-on-pause delay) - those windows don't share a
    /// ViewModel, so this is how the live widget picks up the change immediately instead of on
    /// its next unrelated interaction.</summary>
    public void RefreshFromConfig()
    {
        ApplyVisualScale(_config.Geometry.Scale);
        ApplyClickThrough(_config.ClickThroughEnabled);
        _hideTimer.Interval = TimeSpan.FromSeconds(Math.Clamp(_config.HideOnPauseDelaySeconds, 5, 30));
        ApplyAppearance();
    }

    /// <summary>Applies the appearance settings that already have a visual effect without the
    /// cover-color-extraction/blur/preset rendering work planned for later phases: background
    /// opacity (baked into the background brush's own alpha, not the whole window's Opacity, so
    /// text/icons stay fully readable) and the app-wide dark/light theme variant.</summary>
    private void ApplyAppearance()
    {
        if (BackgroundBorder.Background is Avalonia.Media.SolidColorBrush backgroundBrush)
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

    private void SetClickThroughEnabled(bool enabled)
    {
        _config.SetClickThroughEnabled(enabled);
        ApplyClickThrough(enabled);
    }

    private void ApplyClickThrough(bool enabled)
    {
        if (_hwnd != IntPtr.Zero)
        {
            var exStyle = GetWindowLongPtr(_hwnd, GWL_EXSTYLE);
            exStyle = enabled ? (exStyle | WS_EX_TRANSPARENT) : (exStyle & ~WS_EX_TRANSPARENT);
            SetWindowLongPtr(_hwnd, GWL_EXSTYLE, exStyle);
        }

        MoveClickThroughHandle();
        if (enabled && IsVisible)
        {
            _clickThroughHandle.Show();
        }
        else
        {
            _clickThroughHandle.Hide();
        }
    }

    private void MoveClickThroughHandle()
    {
        _clickThroughHandle.Position = new PixelPoint(Position.X + HandleMargin, Position.Y + HandleMargin);
    }

    private void PersistCurrentPosition()
    {
        var geometry = _config.Geometry;
        _config.SetGeometry(new WidgetGeometry
        {
            PositionX = Position.X,
            PositionY = Position.Y,
            Scale = geometry.Scale,
        });
    }

    private void FadeIn()
    {
        _hideAfterFadeTimer.Stop();
        Opacity = 0;
        Show();
        MoveClickThroughHandle();
        if (_config.ClickThroughEnabled)
        {
            _clickThroughHandle.Show();
        }
        Opacity = 1;
    }

    private void FadeOutAndHide()
    {
        _hideTimer.Stop();
        Opacity = 0;
        _hideAfterFadeTimer.Start();
    }

    private void OnTrackChanged(MediaSessionManager sender, TrackInfo track)
    {
        Dispatcher.UIThread.Post(() =>
        {
            TitleText.Text = string.IsNullOrEmpty(track.Title) ? "No track playing" : track.Title;
            ArtistText.Text = track.Artist;

            var coverArt = track.CoverArt;
            if (coverArt is { Length: > 0 })
            {
                var bytes = coverArt.ToArray();
                using var stream = new MemoryStream(bytes);
                CoverImage.Source = new Bitmap(stream);
            }
            else
            {
                CoverImage.Source = null;
            }
        });
    }

    private void OnPlaybackStateChanged(MediaSessionManager sender, bool isPlaying)
    {
        Dispatcher.UIThread.Post(() =>
        {
            StatusText.Text = isPlaying ? "Playing" : "Paused";

            if (isPlaying)
            {
                _hideTimer.Stop();
                if (_hiddenByAutoHide)
                {
                    // Reappearing after a full hide is instant by design; only the hide fades.
                    _hideAfterFadeTimer.Stop();
                    Opacity = 1;
                    Show();
                    MoveClickThroughHandle();
                    if (_config.ClickThroughEnabled)
                    {
                        _clickThroughHandle.Show();
                    }
                    _hiddenByAutoHide = false;
                }
            }
            else if (_config.HideOnPauseEnabled)
            {
                _hideTimer.Start();
            }
        });
    }

    private void OnWaveformDataReady(WaveformEngine sender, IBuffer bands)
    {
        var floats = MemoryMarshal.Cast<byte, float>(bands.ToArray()).ToArray();
        Dispatcher.UIThread.Post(() => Waveform.UpdateBands(floats));
    }

    [DllImport("user32.dll")]
    private static extern short GetKeyState(int nVirtKey);

    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtr")]
    private static extern IntPtr GetWindowLongPtr64(IntPtr hWnd, int nIndex);

    [DllImport("user32.dll", EntryPoint = "GetWindowLong")]
    private static extern IntPtr GetWindowLong32(IntPtr hWnd, int nIndex);

    private static long GetWindowLongPtr(IntPtr hWnd, int nIndex) =>
        (IntPtr.Size == 8 ? GetWindowLongPtr64(hWnd, nIndex) : GetWindowLong32(hWnd, nIndex)).ToInt64();

    [DllImport("user32.dll", EntryPoint = "SetWindowLongPtr")]
    private static extern IntPtr SetWindowLongPtr64(IntPtr hWnd, int nIndex, IntPtr dwNewLong);

    [DllImport("user32.dll", EntryPoint = "SetWindowLong")]
    private static extern int SetWindowLong32(IntPtr hWnd, int nIndex, int dwNewLong);

    private static void SetWindowLongPtr(IntPtr hWnd, int nIndex, long dwNewLong)
    {
        if (IntPtr.Size == 8)
        {
            SetWindowLongPtr64(hWnd, nIndex, new IntPtr(dwNewLong));
        }
        else
        {
            SetWindowLong32(hWnd, nIndex, unchecked((int)dwNewLong));
        }
    }
}
