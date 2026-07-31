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
    private Task<UpdateCheckResult>? _inFlightCheck;

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

    /// <summary>The outcome of the most recently completed check, or null if none has run yet -
    /// lets a freshly-opened Settings window reflect a check that already happened (e.g. the
    /// silent startup check) instead of always showing "not checked".</summary>
    public UpdateCheckResult? LastCheckResult { get; private set; }

    /// <summary>Checks GitHub Releases and, if a newer version exists, downloads it immediately.
    /// Never throws - every failure path returns UpdateCheckResult.Failed. Re-entrant-safe: the
    /// silent startup check (App.axaml.cs) and an explicit "Vérifier les mises à jour" click
    /// (SettingsViewModel.About.cs) can land on this at nearly the same time - rather than running
    /// two concurrent CheckForUpdatesAsync/DownloadUpdatesAsync pairs against the same download
    /// target (which throws), a caller that arrives while a check is already in flight is handed
    /// that same task instead of starting a second one. This also removes the only unsynchronized
    /// write to <see cref="_pendingUpdate"/>, since only one check ever runs at a time.</summary>
    public Task<UpdateCheckResult> CheckAndDownloadAsync()
    {
        if (_inFlightCheck is { IsCompleted: false } inFlight)
        {
            return inFlight;
        }

        return _inFlightCheck = CheckAndDownloadCoreAsync();
    }

    private async Task<UpdateCheckResult> CheckAndDownloadCoreAsync()
    {
        if (!_updateManager.IsInstalled)
        {
            return Remember(UpdateCheckResult.NotInstalled);
        }

        try
        {
            var updateInfo = await _updateManager.CheckForUpdatesAsync();
            if (updateInfo is null)
            {
                return Remember(UpdateCheckResult.UpToDate);
            }

            await _updateManager.DownloadUpdatesAsync(updateInfo);
            _pendingUpdate = updateInfo;
            Dispatcher.UIThread.Post(() => UpdateReady?.Invoke(this, EventArgs.Empty));
            return Remember(UpdateCheckResult.Ready);
        }
        catch (Exception)
        {
            // Network failure, GitHub unavailable, malformed release feed, etc. - translated to a
            // UI-understandable Failed status rather than crashing the silent startup check.
            return Remember(UpdateCheckResult.Failed);
        }
    }

    private UpdateCheckResult Remember(UpdateCheckResult result)
    {
        LastCheckResult = result;
        return result;
    }

    /// <summary>Applies the previously-downloaded update and restarts the app. No-op if nothing
    /// is pending (defensive - callers gate this behind IsUpdateReady already).
    /// <para>ApplyUpdatesAndRestart exits the process immediately per Velopack's documented
    /// contract - it does NOT raise Avalonia's ShutdownRequested, so none of App.axaml.cs's normal
    /// shutdown cleanup (tray icon disposal, session manager/waveform engine stop) would otherwise
    /// run, leaving a ghost tray icon behind. <paramref name="beforeRestart"/> lets the caller (see
    /// App.axaml.cs's RestartForUpdate) run that cleanup first.</para></summary>
    public void ApplyAndRestart(Action? beforeRestart = null)
    {
        if (_pendingUpdate is { } update)
        {
            beforeRestart?.Invoke();
            _updateManager.ApplyUpdatesAndRestart(update);
        }
    }
}
