using Avalonia.Media;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Wavely.App.Services;
using Wavely.Backend;

namespace Wavely.App.ViewModels;

/// <summary>
/// Backs the "Apparence" tab of the Settings window - split into its own partial-class file so
/// the main SettingsViewModel.cs (Comportement tab, constructor, footer commands) doesn't grow
/// past RULES.md's ~200-line guidance for a single class, mirroring how MainWindow.Appearance.cs
/// was split out of MainWindow.axaml.cs for the same reason. The constructor that initializes
/// these properties still lives in the main file, since it legitimately touches the whole object
/// regardless of which partial file declares each property.
/// </summary>
public partial class SettingsViewModel
{
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

    /// <summary>The fallback accent color used everywhere <c>WidgetColorScheme.Default.Accent</c>
    /// is referenced, when dynamic colors is off. Persisted via <c>AppConfig.CustomAccentColor</c>
    /// (packed 0xAARRGGBB); see <see cref="DynamicColorService.PackColor"/>/<see cref="DynamicColorService.UnpackColor"/>.</summary>
    [ObservableProperty]
    private Color _customAccentColor;

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

    partial void OnCustomAccentColorChanged(Color value)
    {
        if (_isLoading)
        {
            return;
        }
        _config.SetCustomAccentColor(DynamicColorService.PackColor(value));
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
    private void SelectSpotifyAccent() => CustomAccentColor = Color.Parse("#1DB954");

    [RelayCommand]
    private void SelectDeezerAccent() => CustomAccentColor = Color.Parse("#A238FF");

    [RelayCommand]
    private void SelectAppleMusicAccent() => CustomAccentColor = Color.Parse("#FA243C");

    [RelayCommand]
    private void SelectYouTubeAccent() => CustomAccentColor = Color.Parse("#FF0000");

    [RelayCommand]
    private void SelectBlackAccent() => CustomAccentColor = Colors.Black;

    [RelayCommand]
    private void SelectWhiteAccent() => CustomAccentColor = Colors.White;
}
