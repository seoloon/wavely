using Avalonia;
using Velopack;

namespace Wavely.App;

internal static class Program
{
    [STAThread]
    public static void Main(string[] args)
    {
        // Must run first, before any other startup logic: this is what lets Velopack's
        // installer/updater intercept its own special-purpose invocations of this exe
        // (e.g. post-install/uninstall hooks) rather than launching the app UI for them.
        // See docs/TECHNICAL.md / package.ps1 (Task 28) for the packaging pipeline this
        // supports.
        VelopackApp.Build().Run();

        BuildAvaloniaApp()
            .StartWithClassicDesktopLifetime(args);
    }

    public static AppBuilder BuildAvaloniaApp() => AppBuilder.Configure<App>()
        .UsePlatformDetect()
        .WithInterFont()
        .LogToTrace();
}
