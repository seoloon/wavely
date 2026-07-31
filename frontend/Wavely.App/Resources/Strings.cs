using System.Globalization;
using System.Resources;

namespace Wavely.App.Resources;

/// <summary>
/// Thin, hand-written wrapper around the embedded Strings.resx (RULES.md SS6: no hardcoded
/// strings in AXAML/C#). Deliberately not the Visual Studio-generated ResXFileCodeGenerator
/// class - that codegen only runs at IDE design-time, not from a command-line MSBuild/dotnet
/// build, which is what actually builds this project (see docs/ADR-002 addendum). Adding a
/// language later only requires a new Strings.&lt;culture&gt;.resx next to this one; the SDK's
/// default item globbing turns it into a satellite resource assembly automatically.
/// </summary>
internal static class Strings
{
    private static readonly ResourceManager ResourceManager = new("Wavely.App.Resources.Strings", typeof(Strings).Assembly);

    /// <summary>Overrides the culture used to resolve strings; null uses the current UI culture.</summary>
    public static CultureInfo? Culture { get; set; }

    public static string SettingsWindowTitle => Get("Settings_Window_Title");
    public static string SettingsTabBehavior => Get("Settings_Tab_Behavior");
    public static string SettingsTabAppearance => Get("Settings_Tab_Appearance");
    public static string SettingsTabAbout => Get("Settings_Tab_About");
    public static string SettingsBehaviorLockedLabel => Get("Settings_Behavior_Locked_Label");
    public static string SettingsBehaviorClickThroughLabel => Get("Settings_Behavior_ClickThrough_Label");
    public static string SettingsBehaviorResetSizeButton => Get("Settings_Behavior_ResetSize_Button");
    public static string SettingsBehaviorHideOnPauseLabel => Get("Settings_Behavior_HideOnPause_Label");
    public static string SettingsBehaviorHideOnPauseDelayLabel => Get("Settings_Behavior_HideOnPauseDelay_Label");
    public static string SettingsBehaviorLaunchAtStartupLabel => Get("Settings_Behavior_LaunchAtStartup_Label");
    public static string SettingsBehaviorLanguageLabel => Get("Settings_Behavior_Language_Label");
    public static string SettingsAppearancePresetLabel => Get("Settings_Appearance_Preset_Label");
    public static string SettingsAppearanceCoverShapeLabel => Get("Settings_Appearance_CoverShape_Label");
    public static string SettingsAppearanceGlowLabel => Get("Settings_Appearance_Glow_Label");
    public static string SettingsAppearanceDynamicColorsLabel => Get("Settings_Appearance_DynamicColors_Label");
    public static string SettingsAppearanceDynamicBackgroundLabel => Get("Settings_Appearance_DynamicBackground_Label");
    public static string SettingsAppearanceCustomAccentColorLabel => Get("Settings_Appearance_CustomAccentColor_Label");
    public static string SettingsAppearanceBlurredCoverLabel => Get("Settings_Appearance_BlurredCover_Label");
    public static string SettingsAppearanceOpacityLabel => Get("Settings_Appearance_Opacity_Label");
    public static string SettingsAppearanceThemeLabel => Get("Settings_Appearance_Theme_Label");
    public static string SettingsAboutVersionFormat => Get("Settings_About_Version_Format");
    public static string SettingsAboutDevBuildText => Get("Settings_About_DevBuild_Text");
    public static string SettingsAboutStatusNotChecked => Get("Settings_About_Status_NotChecked");
    public static string SettingsAboutStatusChecking => Get("Settings_About_Status_Checking");
    public static string SettingsAboutStatusUpToDate => Get("Settings_About_Status_UpToDate");
    public static string SettingsAboutStatusReady => Get("Settings_About_Status_Ready");
    public static string SettingsAboutStatusFailed => Get("Settings_About_Status_Failed");
    public static string SettingsAboutCheckButton => Get("Settings_About_Check_Button");
    public static string SettingsAboutRestartButton => Get("Settings_About_Restart_Button");
    public static string SettingsFooterReloadWidgetButton => Get("Settings_Footer_ReloadWidget_Button");
    public static string TrayIconSettingsMenuItem => Get("TrayIcon_Settings_MenuItem");
    public static string TrayIconReloadWidgetMenuItem => Get("TrayIcon_ReloadWidget_MenuItem");
    public static string TrayIconLaunchAtStartupMenuItem => Get("TrayIcon_LaunchAtStartup_MenuItem");
    public static string TrayIconQuitMenuItem => Get("TrayIcon_Quit_MenuItem");
    public static string TrayIconRestartToUpdateMenuItem => Get("TrayIcon_RestartToUpdate_MenuItem");

    private static string Get(string name) => ResourceManager.GetString(name, Culture) ?? name;
}
