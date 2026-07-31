using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Wavely.App.Resources;
using Wavely.App.Services;

namespace Wavely.App.ViewModels;

/// <summary>
/// Backs the "À propos" tab of the Settings window - split into its own partial-class file for
/// the same reason as SettingsViewModel.Appearance.cs (RULES.md's ~200-line guidance). The
/// UpdateService field/constructor wiring lives in the main SettingsViewModel.cs file alongside
/// the other injected dependencies; this file owns only the About tab's own state and commands,
/// including the <see cref="Dispose"/> that undoes its <see cref="UpdateService.UpdateReady"/>
/// subscription (the ViewModel doesn't own UpdateService's lifetime, so it must detach here
/// rather than leaking a subscriber every time the Settings window is closed and reopened).
/// </summary>
public partial class SettingsViewModel
{
    /// <summary>Display text for the "Version actuelle" row - either the installed version or the
    /// dev-build placeholder, set once in <see cref="InitializeAbout"/>.</summary>
    [ObservableProperty]
    private string _currentVersionText = string.Empty;

    /// <summary>Display text for the update-check status row (e.g. "Vérification en cours...",
    /// "À jour", "Prêt à installer").</summary>
    [ObservableProperty]
    private string _updateStatusText = string.Empty;

    /// <summary>True while an explicit "Vérifier les mises à jour" click is in flight.</summary>
    [ObservableProperty]
    private bool _isCheckingForUpdates;

    /// <summary>True once an update has been downloaded and is ready to apply on restart.</summary>
    [ObservableProperty]
    private bool _isUpdateReady;

    /// <summary>Reads UpdateService's already-known state (does not trigger a network check) -
    /// called once from the main constructor after _updateService is assigned.</summary>
    private void InitializeAbout()
    {
        CurrentVersionText = _updateService.IsInstalled
            ? string.Format(Strings.SettingsAboutVersionFormat, _updateService.CurrentVersion)
            : Strings.SettingsAboutDevBuildText;

        IsUpdateReady = _updateService.IsUpdateReady;
        UpdateStatusText = IsUpdateReady
            ? Strings.SettingsAboutStatusReady
            : _updateService.LastCheckResult switch
            {
                // Seeds the status row from a check that already completed before this window
                // opened (typically the silent startup check in App.axaml.cs) instead of always
                // showing "not checked" even when a real result is already known.
                UpdateCheckResult.UpToDate => Strings.SettingsAboutStatusUpToDate,
                UpdateCheckResult.Failed => Strings.SettingsAboutStatusFailed,
                _ => Strings.SettingsAboutStatusNotChecked,
            };

        _updateService.UpdateReady += OnUpdateServiceUpdateReady;
    }

    /// <summary>Detaches from the shared, app-lifetime <see cref="UpdateService"/> so this
    /// (per-window-open) ViewModel instance doesn't outlive the Settings window it backs. Must be
    /// called whenever the Settings window closes - see App.axaml.cs's <c>_settingsWindow.Closed</c>
    /// handler - otherwise every open/close cycle adds another permanent subscriber.</summary>
    public void Dispose()
    {
        _updateService.UpdateReady -= OnUpdateServiceUpdateReady;
    }

    private void OnUpdateServiceUpdateReady(object? sender, EventArgs e)
    {
        IsUpdateReady = true;
        UpdateStatusText = Strings.SettingsAboutStatusReady;
    }

    /// <summary>Explicit "Vérifier les mises à jour" button handler - runs a network check even if
    /// one already happened silently at startup.</summary>
    [RelayCommand]
    private async Task CheckForUpdates()
    {
        IsCheckingForUpdates = true;
        UpdateStatusText = Strings.SettingsAboutStatusChecking;

        var result = await _updateService.CheckAndDownloadAsync();
        UpdateStatusText = result switch
        {
            // CurrentVersionText already conveys "dev build" (see InitializeAbout) - repeating
            // that exact sentence on the status row too reads oddly stacked, so this uses the
            // generic "not checked" status text instead.
            UpdateCheckResult.NotInstalled => Strings.SettingsAboutStatusNotChecked,
            UpdateCheckResult.UpToDate => Strings.SettingsAboutStatusUpToDate,
            UpdateCheckResult.Ready => Strings.SettingsAboutStatusReady,
            UpdateCheckResult.Failed => Strings.SettingsAboutStatusFailed,
            _ => UpdateStatusText,
        };
        IsUpdateReady = _updateService.IsUpdateReady;
        IsCheckingForUpdates = false;
    }

    /// <summary>"Redémarrer pour mettre à jour" button handler - routes through App.RestartForUpdate
    /// rather than calling UpdateService.ApplyAndRestart() directly, so App's shutdown cleanup
    /// (tray icon disposal, etc.) runs before Velopack exits the process (see
    /// App.axaml.cs's CleanupBeforeExit/RestartForUpdate).</summary>
    [RelayCommand]
    private void RestartToUpdate() => _restartForUpdate();
}
