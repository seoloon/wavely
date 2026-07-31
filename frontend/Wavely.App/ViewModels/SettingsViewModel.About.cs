using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Wavely.App.Resources;
using Wavely.App.Services;

namespace Wavely.App.ViewModels;

/// <summary>
/// Backs the "À propos" tab of the Settings window - split into its own partial-class file for
/// the same reason as SettingsViewModel.Appearance.cs (RULES.md's ~200-line guidance). The
/// UpdateService field/constructor wiring lives in the main SettingsViewModel.cs file alongside
/// the other injected dependencies; this file owns only the About tab's own state and commands.
/// </summary>
public partial class SettingsViewModel
{
    private UpdateService _updateService = null!;

    [ObservableProperty]
    private string _currentVersionText = string.Empty;

    [ObservableProperty]
    private string _updateStatusText = string.Empty;

    [ObservableProperty]
    private bool _isCheckingForUpdates;

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
        UpdateStatusText = IsUpdateReady ? Strings.SettingsAboutStatusReady : Strings.SettingsAboutStatusNotChecked;

        _updateService.UpdateReady += OnUpdateServiceUpdateReady;
    }

    private void OnUpdateServiceUpdateReady(object? sender, EventArgs e)
    {
        IsUpdateReady = true;
        UpdateStatusText = Strings.SettingsAboutStatusReady;
    }

    [RelayCommand]
    private async Task CheckForUpdates()
    {
        IsCheckingForUpdates = true;
        UpdateStatusText = Strings.SettingsAboutStatusChecking;

        var result = await _updateService.CheckAndDownloadAsync();
        UpdateStatusText = result switch
        {
            UpdateCheckResult.NotInstalled => Strings.SettingsAboutDevBuildText,
            UpdateCheckResult.UpToDate => Strings.SettingsAboutStatusUpToDate,
            UpdateCheckResult.Ready => Strings.SettingsAboutStatusReady,
            UpdateCheckResult.Failed => Strings.SettingsAboutStatusFailed,
            _ => UpdateStatusText,
        };
        IsUpdateReady = _updateService.IsUpdateReady;
        IsCheckingForUpdates = false;
    }

    [RelayCommand]
    private void RestartToUpdate() => _updateService.ApplyAndRestart();
}
