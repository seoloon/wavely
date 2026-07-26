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

    /// <summary>Index into <see cref="PresetNames"/> (Compact, Boxy, Gallery, Minimal, macOS,
    /// Shell, Discord - see assets/presets_reference). Persisted; the layouts themselves are
    /// Phase 7 work, not yet rendered by MainWindow.</summary>
    [ObservableProperty]
    private int _presetIndex;

    /// <summary>Index into <see cref="CoverShapeNames"/>, matching the CoverStyle enum ordinal
    /// (Square, Squircle, Vinyl). Persisted; cover shape rendering is Phase 6/7 work.</summary>
    [ObservableProperty]
    private int _coverShapeIndex;

    [ObservableProperty]
    private bool _coverGlowEnabled;

    [ObservableProperty]
    private bool _coverBlurEnabled;

    [ObservableProperty]
    private bool _dynamicColorsEnabled;

    [ObservableProperty]
    private bool _dynamicBackgroundEnabled;

    /// <summary>0-100 for slider display; converted to/from AppConfig's 0.0-1.0 range.</summary>
    [ObservableProperty]
    private double _backgroundOpacityPercent;

    /// <summary>Index into <see cref="ThemeNames"/>, matching the ThemeMode enum ordinal
    /// (Dark, Light).</summary>
    [ObservableProperty]
    private int _themeIndex;

    public static IReadOnlyList<string> PresetNames { get; } =
        ["Compact", "Boxy", "Gallery", "Minimal", "macOS", "Shell", "Discord"];

    public static IReadOnlyList<string> CoverShapeNames { get; } =
        ["Carré", "Squircle", "Vinyle"];

    public static IReadOnlyList<string> ThemeNames { get; } = ["Sombre", "Clair"];

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
        PresetIndex = _config.PresetIndex;
        CoverShapeIndex = (int)_config.CoverShape;
        CoverGlowEnabled = _config.CoverGlowEnabled;
        CoverBlurEnabled = _config.CoverBlurEnabled;
        DynamicColorsEnabled = _config.DynamicColorsEnabled;
        DynamicBackgroundEnabled = _config.DynamicBackgroundEnabled;
        BackgroundOpacityPercent = _config.BackgroundOpacity * 100.0;
        ThemeIndex = (int)_config.Theme;
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

    partial void OnPresetIndexChanged(int value)
    {
        if (_isLoading)
        {
            return;
        }
        _config.SetPresetIndex(value);
        ConfigChanged?.Invoke(this, EventArgs.Empty);
    }

    partial void OnCoverShapeIndexChanged(int value)
    {
        if (_isLoading)
        {
            return;
        }
        _config.SetCoverShape((CoverStyle)value);
        ConfigChanged?.Invoke(this, EventArgs.Empty);
    }

    partial void OnCoverGlowEnabledChanged(bool value)
    {
        if (_isLoading)
        {
            return;
        }
        _config.SetCoverGlowEnabled(value);
        ConfigChanged?.Invoke(this, EventArgs.Empty);
    }

    partial void OnCoverBlurEnabledChanged(bool value)
    {
        if (_isLoading)
        {
            return;
        }
        _config.SetCoverBlurEnabled(value);
        ConfigChanged?.Invoke(this, EventArgs.Empty);
    }

    partial void OnDynamicColorsEnabledChanged(bool value)
    {
        if (_isLoading)
        {
            return;
        }
        _config.SetDynamicColorsEnabled(value);
        ConfigChanged?.Invoke(this, EventArgs.Empty);
    }

    partial void OnDynamicBackgroundEnabledChanged(bool value)
    {
        if (_isLoading)
        {
            return;
        }
        _config.SetDynamicBackgroundEnabled(value);
        ConfigChanged?.Invoke(this, EventArgs.Empty);
    }

    partial void OnBackgroundOpacityPercentChanged(double value)
    {
        if (_isLoading)
        {
            return;
        }
        _config.SetBackgroundOpacity(value / 100.0);
        ConfigChanged?.Invoke(this, EventArgs.Empty);
    }

    partial void OnThemeIndexChanged(int value)
    {
        if (_isLoading)
        {
            return;
        }
        _config.SetTheme((ThemeMode)value);
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
