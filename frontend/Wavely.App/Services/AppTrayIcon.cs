using System;
using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Wavely.App.Resources;
using Wavely.App.Views;
using Wavely.Backend;

namespace Wavely.App.Services;

/// <summary>
/// Owns the system tray icon and its context menu (Settings, Reload widget, Launch at startup,
/// Quit). Clicking the icon toggles the overlay widget's visibility; closing the widget
/// (see MainWindow.OnClosing) only hides it, so the tray is what actually keeps the process
/// alive and is the only path that terminates it.
/// </summary>
public sealed class AppTrayIcon : IDisposable
{
    private readonly TrayIcon _trayIcon;
    private readonly MainWindow _window;
    private readonly AppConfig _config;
    private readonly MediaSessionManager _sessionManager;
    private readonly NativeMenuItem _launchAtStartupItem;
    private readonly NativeMenuItem _restartToUpdateItem;

    public AppTrayIcon(MainWindow window, AppConfig config, MediaSessionManager sessionManager, UpdateService updateService, Action openSettings, Action restartForUpdate)
    {
        _window = window;
        _config = config;
        _sessionManager = sessionManager;

        var settingsItem = new NativeMenuItem(Strings.TrayIconSettingsMenuItem);
        settingsItem.Click += (_, _) => openSettings();

        var reloadItem = new NativeMenuItem(Strings.TrayIconReloadWidgetMenuItem);
        reloadItem.Click += (_, _) => _sessionManager.Refresh();

        _restartToUpdateItem = new NativeMenuItem(Strings.TrayIconRestartToUpdateMenuItem)
        {
            IsVisible = updateService.IsUpdateReady,
        };
        _restartToUpdateItem.Click += (_, _) => restartForUpdate();
        updateService.UpdateReady += (_, _) => _restartToUpdateItem.IsVisible = true;

        _launchAtStartupItem = new NativeMenuItem(Strings.TrayIconLaunchAtStartupMenuItem)
        {
            ToggleType = NativeMenuItemToggleType.CheckBox,
            IsChecked = _config.LaunchAtStartup,
        };
        _launchAtStartupItem.Click += OnLaunchAtStartupClicked;

        var quitItem = new NativeMenuItem(Strings.TrayIconQuitMenuItem);
        quitItem.Click += (_, _) =>
        {
            if (Avalonia.Application.Current?.ApplicationLifetime is
                Avalonia.Controls.ApplicationLifetimes.IClassicDesktopStyleApplicationLifetime desktop)
            {
                desktop.Shutdown();
            }
        };

        _trayIcon = new TrayIcon
        {
            Icon = CreatePlaceholderIcon(),
            ToolTipText = "Wavely",
            Menu = new NativeMenu
            {
                Items = { settingsItem, reloadItem, _restartToUpdateItem, new NativeMenuItemSeparator(), _launchAtStartupItem, new NativeMenuItemSeparator(), quitItem },
            },
        };
        _trayIcon.Clicked += OnClicked;
        _trayIcon.IsVisible = true;
    }

    /// <summary>Reflects an externally-changed AppConfig.LaunchAtStartup (e.g. from the Settings
    /// window) back onto the tray's checkbox, since NativeMenuItem has no data binding.</summary>
    public void RefreshLaunchAtStartup()
    {
        _launchAtStartupItem.IsChecked = _config.LaunchAtStartup;
    }

    private void OnClicked(object? sender, EventArgs e)
    {
        if (_window.IsVisible)
        {
            _window.Hide();
        }
        else
        {
            _window.Opacity = 1;
            _window.Show();
        }
    }

    private void OnLaunchAtStartupClicked(object? sender, EventArgs e)
    {
        var enabled = !_launchAtStartupItem.IsChecked;
        _launchAtStartupItem.IsChecked = enabled;
        AutoStartManager.SetEnabled(enabled);
        _config.SetLaunchAtStartup(enabled);
    }

    /// <summary>
    /// No branded asset exists yet (RULES.md SS8.5 schedules the real app icon for Phase 8
    /// packaging); a simple filled circle keeps the tray from showing a blank/default icon.
    /// </summary>
    private static WindowIcon CreatePlaceholderIcon()
    {
        const int size = 32;
        var bitmap = new WriteableBitmap(new PixelSize(size, size), new Avalonia.Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Premul);
        using (var buffer = bitmap.Lock())
        {
            var stride = buffer.RowBytes;
            var pixels = new byte[stride * size];
            const double center = size / 2.0;
            const double radius = size / 2.0 - 2;
            for (var y = 0; y < size; y++)
            {
                for (var x = 0; x < size; x++)
                {
                    var dx = x + 0.5 - center;
                    var dy = y + 0.5 - center;
                    if (dx * dx + dy * dy > radius * radius)
                    {
                        continue;
                    }
                    var offset = y * stride + x * 4;
                    pixels[offset + 0] = 255; // B
                    pixels[offset + 1] = 170; // G
                    pixels[offset + 2] = 90;  // R
                    pixels[offset + 3] = 255; // A
                }
            }
            Marshal.Copy(pixels, 0, buffer.Address, pixels.Length);
        }
        return new WindowIcon(bitmap);
    }

    public void Dispose()
    {
        _trayIcon.IsVisible = false;
        _trayIcon.Dispose();
    }
}
