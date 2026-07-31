using System;
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
            Icon = LoadTrayIcon(),
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

    /// <summary>Loads the branded waveform mark (transparent background, reads on both light and
    /// dark taskbars) as the tray icon (RULES.md SS8.5).</summary>
    private static WindowIcon LoadTrayIcon()
    {
        using var stream = AssetLoader.Open(new Uri("avares://Wavely.App/Assets/logo_trans.png"));
        return new WindowIcon(new Bitmap(stream));
    }

    public void Dispose()
    {
        _trayIcon.IsVisible = false;
        _trayIcon.Dispose();
    }
}
