using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Wavely.App.Services;
using Wavely.App.ViewModels;
using Wavely.App.Views;
using Wavely.Backend;

namespace Wavely.App;

public partial class App : Application
{
    private AppTrayIcon? _trayIcon;
    private MediaSessionManager? _sessionManager;
    private WaveformEngine? _waveformEngine;
    private AppConfig? _config;
    private UpdateService? _updateService;
    private MainWindow? _mainWindow;
    private SettingsWindow? _settingsWindow;

    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);
    }

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            // The tray icon (see AppTrayIcon) is what keeps the app alive: closing the widget
            // only hides it (MainWindow.OnClosing), so the app must not quit just because it
            // became invisible.
            desktop.ShutdownMode = ShutdownMode.OnExplicitShutdown;

            _config = new AppConfig();
            _sessionManager = new MediaSessionManager();
            _waveformEngine = new WaveformEngine();
            _updateService = new UpdateService();

            _mainWindow = new MainWindow(_config, _sessionManager, _waveformEngine);
            desktop.MainWindow = _mainWindow;

            _trayIcon = new AppTrayIcon(_mainWindow, _config, _sessionManager, _updateService, OpenSettings);

            _sessionManager.Start();
            _waveformEngine.Start();

            // Silent background check - never awaited, never surfaces a failure to the user
            // beyond what the About tab/tray already show (UpdateService never throws out of
            // CheckAndDownloadAsync, see Task 1).
            _ = _updateService.CheckAndDownloadAsync();

            desktop.ShutdownRequested += (_, _) =>
            {
                _sessionManager.Stop();
                _waveformEngine.Stop();
                _trayIcon.Dispose();
            };
        }

        base.OnFrameworkInitializationCompleted();
    }

    /// <summary>Opens the Settings window, or activates it if already open (avoids stacking
    /// duplicate windows if the user clicks the tray's "Settings..." item repeatedly).</summary>
    private void OpenSettings()
    {
        if (_settingsWindow is not null)
        {
            _settingsWindow.Activate();
            return;
        }

        var viewModel = new SettingsViewModel(_config!, _sessionManager!, _updateService!);
        viewModel.ConfigChanged += (_, _) =>
        {
            _mainWindow?.RefreshFromConfig();
            _trayIcon?.RefreshLaunchAtStartup();
        };

        _settingsWindow = new SettingsWindow(viewModel);
        _settingsWindow.Closed += (_, _) =>
        {
            // A fresh SettingsViewModel is constructed on every open, and it subscribes to the
            // single app-lifetime UpdateService (see InitializeAbout in SettingsViewModel.About.cs).
            // Without this, each open/close cycle would leak one more permanent subscriber.
            viewModel.Dispose();
            _settingsWindow = null;
        };
        _settingsWindow.Show();
    }
}
