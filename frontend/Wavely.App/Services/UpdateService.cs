using Avalonia.Threading;
using Velopack;
using Velopack.Sources;

namespace Wavely.App.Services;

public enum UpdateCheckResult
{
    NotInstalled,
    UpToDate,
    Ready,
    Failed,
}

/// <summary>
/// Wraps Velopack's UpdateManager against the public github.com/seoloon/wavely repo. The single
/// point of contact with the Velopack API - callers never touch UpdateManager/GithubSource
/// directly. All Velopack exceptions (network failure, malformed release feed, etc.) are caught
/// here and translated into UpdateCheckResult.Failed (RULES.md SS4: never let a backend/library
/// exception cross into UI code or crash the silent startup check).
/// </summary>
public sealed class UpdateService
{
    private const string RepoUrl = "https://github.com/seoloon/wavely";

    private readonly UpdateManager _updateManager;
    private UpdateInfo? _pendingUpdate;

    /// <summary>Fired once a downloaded update is ready to apply. Always raised on the UI thread
    /// so subscribers (AppTrayIcon, SettingsViewModel) can touch Avalonia controls directly.</summary>
    public event EventHandler? UpdateReady;

    public UpdateService()
    {
        _updateManager = new UpdateManager(new GithubSource(RepoUrl, null, false));
    }

    /// <summary>False when running from a raw build (dotnet run/build.ps1) rather than an
    /// installed Setup.exe - update checks must no-op cleanly in that case, never throw.</summary>
    public bool IsInstalled => _updateManager.IsInstalled;

    /// <summary>Null when not installed. Otherwise the currently-running version string.</summary>
    public string? CurrentVersion => _updateManager.IsInstalled ? _updateManager.CurrentVersion?.ToString() : null;

    /// <summary>True once a downloaded update is sitting ready for ApplyAndRestart - lets a
    /// freshly-opened Settings window reflect state from a check that already completed (e.g. the
    /// silent startup check), not just future UpdateReady events.</summary>
    public bool IsUpdateReady => _pendingUpdate is not null;

    /// <summary>Checks GitHub Releases and, if a newer version exists, downloads it immediately.
    /// Never throws - every failure path returns UpdateCheckResult.Failed.</summary>
    public async Task<UpdateCheckResult> CheckAndDownloadAsync()
    {
        if (!_updateManager.IsInstalled)
        {
            return UpdateCheckResult.NotInstalled;
        }

        try
        {
            var updateInfo = await _updateManager.CheckForUpdatesAsync();
            if (updateInfo is null)
            {
                return UpdateCheckResult.UpToDate;
            }

            await _updateManager.DownloadUpdatesAsync(updateInfo);
            _pendingUpdate = updateInfo;
            Dispatcher.UIThread.Post(() => UpdateReady?.Invoke(this, EventArgs.Empty));
            return UpdateCheckResult.Ready;
        }
        catch (Exception)
        {
            // Network failure, GitHub unavailable, malformed release feed, etc. - translated to a
            // UI-understandable Failed status rather than crashing the silent startup check.
            return UpdateCheckResult.Failed;
        }
    }

    /// <summary>Applies the previously-downloaded update and restarts the app. No-op if nothing
    /// is pending (defensive - callers gate this behind IsUpdateReady already).</summary>
    public void ApplyAndRestart()
    {
        if (_pendingUpdate is { } update)
        {
            _updateManager.ApplyUpdatesAndRestart(update);
        }
    }
}
