using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Wavely.App.Services;
using Wavely.App.Views;
using Wavely.Backend;

namespace Wavely.App;

public partial class App : Application
{
    private AppTrayIcon? _trayIcon;
    private MediaSessionManager? _sessionManager;

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

            var config = new AppConfig();
            _sessionManager = new MediaSessionManager();

            var window = new MainWindow(config, _sessionManager);
            desktop.MainWindow = window;

            _trayIcon = new AppTrayIcon(window, config, _sessionManager);

            _sessionManager.Start();

            desktop.ShutdownRequested += (_, _) =>
            {
                _sessionManager.Stop();
                _trayIcon.Dispose();
            };
        }

        base.OnFrameworkInitializationCompleted();
    }
}
