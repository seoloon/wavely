using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Wavely.Backend;

namespace Wavely.App.ViewModels;

/// <summary>
/// Backs the "Comportement" tab of the Settings window. Every property setter writes straight
/// through to the shared backend AppConfig (RULES.md SS5: save on every change, not just on
/// close) and raises <see cref="ConfigChanged"/> so the live overlay widget and tray icon -
/// which don't share this ViewModel - can re-read state that needs an immediate visual update.
/// </summary>
public partial class SettingsViewModel : ObservableObject
{
    private readonly AppConfig _config;
    private readonly MediaSessionManager _sessionManager;
    private bool _isLoading;

    public event EventHandler? ConfigChanged;

    [ObservableProperty]
    private bool _locked;

    [ObservableProperty]
    private bool _clickThroughEnabled;

    [ObservableProperty]
    private bool _hideOnPauseEnabled;

    [ObservableProperty]
    private int _hideOnPauseDelaySeconds;

    [ObservableProperty]
    private bool _launchAtStartup;

    public SettingsViewModel(AppConfig config, MediaSessionManager sessionManager)
    {
        _config = config;
        _sessionManager = sessionManager;

        _isLoading = true;
        Locked = _config.Locked;
        ClickThroughEnabled = _config.ClickThroughEnabled;
        HideOnPauseEnabled = _config.HideOnPauseEnabled;
        HideOnPauseDelaySeconds = _config.HideOnPauseDelaySeconds;
        LaunchAtStartup = _config.LaunchAtStartup;
        _isLoading = false;
    }

    partial void OnLockedChanged(bool value)
    {
        if (_isLoading)
        {
            return;
        }
        _config.SetLocked(value);
        ConfigChanged?.Invoke(this, EventArgs.Empty);
    }

    partial void OnClickThroughEnabledChanged(bool value)
    {
        if (_isLoading)
        {
            return;
        }
        _config.SetClickThroughEnabled(value);
        ConfigChanged?.Invoke(this, EventArgs.Empty);
    }

    partial void OnHideOnPauseEnabledChanged(bool value)
    {
        if (_isLoading)
        {
            return;
        }
        _config.SetHideOnPauseEnabled(value);
        ConfigChanged?.Invoke(this, EventArgs.Empty);
    }

    partial void OnHideOnPauseDelaySecondsChanged(int value)
    {
        if (_isLoading)
        {
            return;
        }
        _config.SetHideOnPauseDelaySeconds(value);
        ConfigChanged?.Invoke(this, EventArgs.Empty);
    }

    partial void OnLaunchAtStartupChanged(bool value)
    {
        if (_isLoading)
        {
            return;
        }
        AutoStartManager.SetEnabled(value);
        _config.SetLaunchAtStartup(value);
        ConfigChanged?.Invoke(this, EventArgs.Empty);
    }

    [RelayCommand]
    private void ResetSize()
    {
        var geometry = _config.Geometry;
        _config.SetGeometry(new WidgetGeometry
        {
            PositionX = geometry.PositionX,
            PositionY = geometry.PositionY,
            Scale = 1.0,
        });
        ConfigChanged?.Invoke(this, EventArgs.Empty);
    }

    [RelayCommand]
    private void ReloadWidget()
    {
        _sessionManager.Refresh();
    }
}
