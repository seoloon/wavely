# Velopack Auto-Update via GitHub Releases Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire real Velopack auto-update checking/downloading/applying into the running app, surface it in Settings and the tray, and add a local script that publishes packaged releases to GitHub Releases (as drafts by default).

**Architecture:** A new `Services/UpdateService.cs` wraps `Velopack.UpdateManager` + `Velopack.Sources.GithubSource` (targeting the public `https://github.com/seoloon/wavely` repo, no token needed to read). `App.axaml.cs` constructs it once, fires a silent check on startup, and hands it to both `SettingsViewModel` (new "About" tab: version, manual check button, status) and `AppTrayIcon` (new "Restart to Update" item, hidden until ready). A new `release.ps1` sits on top of the existing `package.ps1` and uploads its output to GitHub via the `vpk` CLI.

**Tech Stack:** C#/.NET 8, Avalonia 11.3.18, `CommunityToolkit.Mvvm` (existing MVVM pattern), `Velopack` 1.2.0 (already referenced in `Wavely.App.csproj`), `vpk` CLI 1.2.0 (already installed globally).

**Spec:** `docs/superpowers/specs/2026-07-31-velopack-auto-update-design.md` — read it first for the decisions this plan implements (trigger UX, release visibility default, etc.).

## Global Constraints

- Frontend nullable reference types enabled; no empty `catch {}` — every caught exception must be translated into UI-understandable state (RULES.md §4). `UpdateService`'s catch blocks do this by returning/exposing a `Failed` status, never by swallowing silently.
- PascalCase types/methods/public properties, `camelCase` locals, `_camelCase` private fields (RULES.md §3).
- No magic numbers — named `const`/`static readonly` for anything tuning-related.
- Classes ~200 lines max; split further if a task's file would exceed it (this plan already splits `SettingsViewModel`'s About tab into its own partial file, following the existing `SettingsViewModel.Appearance.cs` precedent).
- **No hardcoded user-facing strings** in AXAML or C# (RULES.md §6) — every new label/button/status text goes through `Resources/Strings.cs` + `Resources/Strings.resx`, French value, following the exact `Settings_Xxx_Yyy_Label`-style key convention already in use.
- Events/callbacks that can fire off the UI thread must marshal back via `Dispatcher.UIThread.Post` before touching any Avalonia control (RULES.md §2, same pattern as `MainWindow.OnTrackChanged`).
- Every public method/property gets a brief `///` XML doc comment describing what it does, not how.
- No test project exists in this codebase (all prior phases verify via build + manual runtime checks) — this plan follows the same convention; do not introduce a new test project.
- Build: `.\build.ps1 -Configuration Debug`. Run: `frontend\Wavely.App\bin\Debug\net8.0-windows10.0.19041.0\Wavely.App.exe`. Package: `.\package.ps1 -Version <x.y.z>`.

---

### Task 1: `Services/UpdateService.cs` — Velopack wrapper

**Files:**
- Create: `frontend/Wavely.App/Services/UpdateService.cs`

**Interfaces:**
- Produces: `UpdateService.IsInstalled` (bool), `CurrentVersion` (string?), `IsUpdateReady` (bool), `event EventHandler? UpdateReady`, `Task<UpdateCheckResult> CheckAndDownloadAsync()`, `void ApplyAndRestart()`. `UpdateCheckResult` enum: `NotInstalled`, `UpToDate`, `Ready`, `Failed`.
- Consumes: `Velopack.UpdateManager`, `Velopack.Sources.GithubSource` (NuGet package already referenced in `Wavely.App.csproj`).

This is the single point of contact with the Velopack API — same principle as `AutoStartManager`/`DynamicColorService`: one class, one external concern.

- [ ] **Step 1: Write `UpdateService.cs`**

```csharp
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
```

- [ ] **Step 2: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).` If `GithubSource`/`UpdateManager` member names don't match (Velopack API drift), the compiler error will name the exact mismatch — fix against the real error, don't guess.

- [ ] **Step 3: Commit**

```bash
git add frontend/Wavely.App/Services/UpdateService.cs
git commit -m "feat: add UpdateService wrapping Velopack UpdateManager/GithubSource"
```

---

### Task 2: New strings for the About tab and tray item

**Files:**
- Modify: `frontend/Wavely.App/Resources/Strings.resx`
- Modify: `frontend/Wavely.App/Resources/Strings.cs`

**Interfaces:**
- Produces: `Strings.SettingsTabAbout`, `SettingsAboutVersionFormat`, `SettingsAboutDevBuildText`, `SettingsAboutStatusNotChecked`, `SettingsAboutStatusChecking`, `SettingsAboutStatusUpToDate`, `SettingsAboutStatusReady`, `SettingsAboutStatusFailed`, `SettingsAboutCheckButton`, `SettingsAboutRestartButton`, `TrayIconRestartToUpdateMenuItem` — all consumed by Tasks 3 and 4.

- [ ] **Step 1: Add entries to `Strings.resx`**

Insert before the closing `</root>` tag:

```xml
  <data name="Settings_Tab_About" xml:space="preserve">
    <value>À propos</value>
  </data>
  <data name="Settings_About_Version_Format" xml:space="preserve">
    <value>Wavely v{0}</value>
  </data>
  <data name="Settings_About_DevBuild_Text" xml:space="preserve">
    <value>Build de développement (non installée)</value>
  </data>
  <data name="Settings_About_Status_NotChecked" xml:space="preserve">
    <value>Non vérifié</value>
  </data>
  <data name="Settings_About_Status_Checking" xml:space="preserve">
    <value>Vérification en cours...</value>
  </data>
  <data name="Settings_About_Status_UpToDate" xml:space="preserve">
    <value>Wavely est à jour</value>
  </data>
  <data name="Settings_About_Status_Ready" xml:space="preserve">
    <value>Mise à jour prête — redémarrez pour l'appliquer</value>
  </data>
  <data name="Settings_About_Status_Failed" xml:space="preserve">
    <value>Échec de la vérification des mises à jour</value>
  </data>
  <data name="Settings_About_Check_Button" xml:space="preserve">
    <value>Vérifier les mises à jour</value>
  </data>
  <data name="Settings_About_Restart_Button" xml:space="preserve">
    <value>Redémarrer pour mettre à jour</value>
  </data>
  <data name="TrayIcon_RestartToUpdate_MenuItem" xml:space="preserve">
    <value>Redémarrer pour mettre à jour</value>
  </data>
```

- [ ] **Step 2: Add matching accessors to `Strings.cs`**

Insert after the existing `SettingsTabAppearance` line (keeps About grouped with the other tab accessors):

```csharp
    public static string SettingsTabAbout => Get("Settings_Tab_About");
```

Insert after the existing `SettingsAppearanceThemeLabel` line (keeps About's own fields grouped together, before the footer/tray accessors):

```csharp
    public static string SettingsAboutVersionFormat => Get("Settings_About_Version_Format");
    public static string SettingsAboutDevBuildText => Get("Settings_About_DevBuild_Text");
    public static string SettingsAboutStatusNotChecked => Get("Settings_About_Status_NotChecked");
    public static string SettingsAboutStatusChecking => Get("Settings_About_Status_Checking");
    public static string SettingsAboutStatusUpToDate => Get("Settings_About_Status_UpToDate");
    public static string SettingsAboutStatusReady => Get("Settings_About_Status_Ready");
    public static string SettingsAboutStatusFailed => Get("Settings_About_Status_Failed");
    public static string SettingsAboutCheckButton => Get("Settings_About_Check_Button");
    public static string SettingsAboutRestartButton => Get("Settings_About_Restart_Button");
```

Insert after the existing `TrayIconQuitMenuItem` line:

```csharp
    public static string TrayIconRestartToUpdateMenuItem => Get("TrayIcon_RestartToUpdate_MenuItem");
```

- [ ] **Step 3: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).` (these accessors are unused until Tasks 3-4 wire them in — a clean build here just confirms the resx/cs pairing is well-formed.)

- [ ] **Step 4: Commit**

```bash
git add frontend/Wavely.App/Resources/Strings.resx frontend/Wavely.App/Resources/Strings.cs
git commit -m "feat: add About-tab and tray update strings"
```

---

### Task 3: About tab (`SettingsViewModel.About.cs` + `SettingsWindow.axaml`)

**Files:**
- Create: `frontend/Wavely.App/ViewModels/SettingsViewModel.About.cs`
- Modify: `frontend/Wavely.App/ViewModels/SettingsViewModel.cs` — constructor gains an `UpdateService` parameter, stores it, initializes the About tab's state.
- Modify: `frontend/Wavely.App/Views/SettingsWindow.axaml` — new "About" `TabItem`.

**Interfaces:**
- Consumes: `UpdateService` (Task 1), `Strings.SettingsTabAbout`/`SettingsAbout*` (Task 2).
- Produces: nothing later tasks depend on — Task 5 only needs to know the constructor signature changed.

- [ ] **Step 1: Write `SettingsViewModel.About.cs`**

```csharp
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
```

- [ ] **Step 2: Wire `UpdateService` into the main `SettingsViewModel.cs` constructor**

In `frontend/Wavely.App/ViewModels/SettingsViewModel.cs`, change the constructor signature and body:

```csharp
    public SettingsViewModel(AppConfig config, MediaSessionManager sessionManager, UpdateService updateService)
    {
        _config = config;
        _sessionManager = sessionManager;
        _updateService = updateService;

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
        CustomAccentColor = DynamicColorService.UnpackColor(_config.CustomAccentColor);
        BackgroundOpacityPercent = _config.BackgroundOpacity * 100.0;
        ThemeIndex = (int)_config.Theme;
        _isLoading = false;

        InitializeAbout();
    }
```

(Only the parameter list, the `_updateService = updateService;` line, and the trailing `InitializeAbout();` call are new — every other line is unchanged from the existing constructor.)

- [ ] **Step 3: Add the About tab to `SettingsWindow.axaml`**

Insert as a new `TabItem`, immediately after the existing `Appearance` `TabItem`'s closing `</TabItem>` and before the closing `</TabControl>`:

```xml
            <TabItem Header="{x:Static res:Strings.SettingsTabAbout}">
                <StackPanel Margin="12" Spacing="16">
                    <TextBlock Text="{Binding CurrentVersionText}" FontWeight="Bold" FontSize="14" />
                    <TextBlock Text="{Binding UpdateStatusText}" Opacity="0.8" />
                    <Button Content="{x:Static res:Strings.SettingsAboutCheckButton}"
                            Command="{Binding CheckForUpdatesCommand}"
                            IsEnabled="{Binding !IsCheckingForUpdates}"
                            HorizontalAlignment="Left" />
                    <Button Content="{x:Static res:Strings.SettingsAboutRestartButton}"
                            Command="{Binding RestartToUpdateCommand}"
                            IsVisible="{Binding IsUpdateReady}"
                            HorizontalAlignment="Left" />
                </StackPanel>
            </TabItem>
```

- [ ] **Step 4: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: **fails** — `App.axaml.cs`'s existing `new SettingsViewModel(_config!, _sessionManager!)` call no longer matches the 3-parameter constructor. This is expected; Task 5 fixes the call site. Continue to Task 4 before attempting a clean build (matches the phase7 plan's own precedent for interdependent tasks).

- [ ] **Step 5: Commit (deferred to Task 5)**

Commit this task's files together with Task 4's and Task 5's, once all three compile — see Task 5's commit step.

---

### Task 4: Tray "Restart to Update" item

**Files:**
- Modify: `frontend/Wavely.App/Services/AppTrayIcon.cs` — constructor gains an `UpdateService` parameter, adds a conditional menu item.

**Interfaces:**
- Consumes: `UpdateService` (Task 1), `Strings.TrayIconRestartToUpdateMenuItem` (Task 2).
- Produces: nothing later tasks depend on.

- [ ] **Step 1: Update `AppTrayIcon.cs`**

Change the constructor signature and body:

```csharp
    private readonly NativeMenuItem _restartToUpdateItem;

    public AppTrayIcon(MainWindow window, AppConfig config, MediaSessionManager sessionManager, UpdateService updateService, Action openSettings)
    {
        _window = window;
        _config = config;
        _sessionManager = sessionManager;

        var settingsItem = new NativeMenuItem(Strings.TrayIconSettingsMenuItem);
        settingsItem.Click += (_, _) => openSettings();

        var reloadItem = new NativeMenuItem(Strings.TrayIconReloadWidgetMenuItem);
        reloadItem.Click += (_, _) => _sessionManager.Refresh();

        _restartToUpdateItem = new NativeMenuItem(Strings.TrayIconRestartToUpdateMenuItem)
        {
            IsVisible = updateService.IsUpdateReady,
        };
        _restartToUpdateItem.Click += (_, _) => updateService.ApplyAndRestart();
        updateService.UpdateReady += (_, _) => _restartToUpdateItem.IsVisible = true;

        _launchAtStartupItem = new NativeMenuItem(Strings.TrayIconLaunchAtStartupMenuItem)
        {
            ToggleType = NativeMenuItemToggleType.CheckBox,
            IsChecked = _config.LaunchAtStartup,
        };
        _launchAtStartupItem.Click += OnLaunchAtStartupClicked;

        var quitItem = new NativeMenuItem(Strings.TrayIconQuitMenuItem);
        quitItem.Click += (_, _) =>
        {
            if (Avalonia.Application.Current?.ApplicationLifetime is
                Avalonia.Controls.ApplicationLifetimes.IClassicDesktopStyleApplicationLifetime desktop)
            {
                desktop.Shutdown();
            }
        };

        _trayIcon = new TrayIcon
        {
            Icon = CreatePlaceholderIcon(),
            ToolTipText = "Wavely",
            Menu = new NativeMenu
            {
                Items = { settingsItem, reloadItem, _restartToUpdateItem, new NativeMenuItemSeparator(), _launchAtStartupItem, new NativeMenuItemSeparator(), quitItem },
            },
        };
        _trayIcon.Clicked += OnClicked;
        _trayIcon.IsVisible = true;
    }
```

(`UpdateReady` is already raised on the UI thread by `UpdateService` itself — Task 1, Step 1 — so this handler can touch `NativeMenuItem.IsVisible` directly without its own `Dispatcher.UIThread.Post`. No new `using` is needed: `AppTrayIcon` and `UpdateService` are both in the `Wavely.App.Services` namespace already.)

- [ ] **Step 2: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: **fails** — `App.axaml.cs`'s existing `new AppTrayIcon(_mainWindow, _config, _sessionManager, OpenSettings)` call doesn't match the new 5-parameter constructor. Expected; Task 5 fixes it.

- [ ] **Step 3: Commit (deferred to Task 5)**

---

### Task 5: Wire `UpdateService` into `App.axaml.cs` (first successful build across Tasks 1-5)

**Files:**
- Modify: `frontend/Wavely.App/App.axaml.cs`

**Interfaces:**
- Consumes: `UpdateService` (Task 1), the updated `SettingsViewModel` (Task 3) and `AppTrayIcon` (Task 4) constructors.

- [ ] **Step 1: Update `App.axaml.cs`**

```csharp
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Wavely.App.Services;
using Wavely.App.ViewModels;
using Wavely.App.Views;
using Wavely.Backend;

namespace Wavely.App;

public partial class App : Application
{
    private AppTrayIcon? _trayIcon;
    private MediaSessionManager? _sessionManager;
    private WaveformEngine? _waveformEngine;
    private AppConfig? _config;
    private UpdateService? _updateService;
    private MainWindow? _mainWindow;
    private SettingsWindow? _settingsWindow;

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

            _config = new AppConfig();
            _sessionManager = new MediaSessionManager();
            _waveformEngine = new WaveformEngine();
            _updateService = new UpdateService();

            _mainWindow = new MainWindow(_config, _sessionManager, _waveformEngine);
            desktop.MainWindow = _mainWindow;

            _trayIcon = new AppTrayIcon(_mainWindow, _config, _sessionManager, _updateService, OpenSettings);

            _sessionManager.Start();
            _waveformEngine.Start();

            // Silent background check - never awaited, never surfaces a failure to the user
            // beyond what the About tab/tray already show (UpdateService never throws out of
            // CheckAndDownloadAsync, see Task 1).
            _ = _updateService.CheckAndDownloadAsync();

            desktop.ShutdownRequested += (_, _) =>
            {
                _sessionManager.Stop();
                _waveformEngine.Stop();
                _trayIcon.Dispose();
            };
        }

        base.OnFrameworkInitializationCompleted();
    }

    /// <summary>Opens the Settings window, or activates it if already open (avoids stacking
    /// duplicate windows if the user clicks the tray's "Settings..." item repeatedly).</summary>
    private void OpenSettings()
    {
        if (_settingsWindow is not null)
        {
            _settingsWindow.Activate();
            return;
        }

        var viewModel = new SettingsViewModel(_config!, _sessionManager!, _updateService!);
        viewModel.ConfigChanged += (_, _) =>
        {
            _mainWindow?.RefreshFromConfig();
            _trayIcon?.RefreshLaunchAtStartup();
        };

        _settingsWindow = new SettingsWindow(viewModel);
        _settingsWindow.Closed += (_, _) => _settingsWindow = null;
        _settingsWindow.Show();
    }
}
```

- [ ] **Step 2: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).` — first clean build across Tasks 1-5.

- [ ] **Step 3: Run and smoke-test the dev build**

Run: `frontend\Wavely.App\bin\Debug\net8.0-windows10.0.19041.0\Wavely.App.exe`
Expected: app launches normally (unrelated to updates - confirms nothing in this wiring broke startup). Open Settings → About tab: shows "Build de développement (non installée)" and "Non vérifié", since `IsInstalled` is false for a raw build. Click "Vérifier les mises à jour": status briefly shows "Vérification en cours...", then settles on the same dev-build text (still `NotInstalled` under the hood) without crashing. Tray menu shows no "Redémarrer pour mettre à jour" item (stays hidden).

- [ ] **Step 4: Commit**

```bash
git add frontend/Wavely.App/App.axaml.cs frontend/Wavely.App/Services/AppTrayIcon.cs frontend/Wavely.App/ViewModels/SettingsViewModel.cs frontend/Wavely.App/ViewModels/SettingsViewModel.About.cs frontend/Wavely.App/Views/SettingsWindow.axaml
git commit -m "feat: wire UpdateService into startup, About tab, and tray menu"
```

---

### Task 6: `release.ps1` — publish to GitHub Releases

**Files:**
- Create: `release.ps1` (repo root, alongside `build.ps1`/`package.ps1`)

**Interfaces:**
- Consumes: `package.ps1` (Task 28, existing), `vpk upload github` CLI.
- Produces: nothing later tasks depend on — this is the terminal script in the build→package→release chain.

- [ ] **Step 1: Write `release.ps1`**

```powershell
<#
Publishes a packaged Wavely release to GitHub Releases (github.com/seoloon/wavely) via the
Velopack CLI. Reuses package.ps1 for the build+publish+pack steps (same layering as
build.ps1 -> package.ps1: this script only adds the upload step, it doesn't reimplement
packaging).

Requires:
- The Velopack CLI (`vpk`) installed: `dotnet tool install -g vpk`.
- A GITHUB_TOKEN environment variable holding a GitHub PAT with `repo` scope (needed even to
  create a draft release). Never committed, never written to disk by this script.

By default the GitHub release is created as a DRAFT (vpk's own default when --publish is
omitted) - nothing reaches users' auto-updaters until the draft is published manually on
GitHub. Pass -Publish to publish immediately instead.

CLI syntax verified live against vpk 1.2.0 (`vpk upload github --help`) - do not assume flag
names without re-checking if vpk is upgraded, its flags have changed across versions before
(see package.ps1's own header comment).
#>
param(
    [Parameter(Mandatory)][string]$Version,
    [switch]$Publish,
    [switch]$PreRelease
)

$ErrorActionPreference = 'Stop'
$repoRoot = $PSScriptRoot

if (-not $env:GITHUB_TOKEN) {
    throw "GITHUB_TOKEN environment variable is not set - needed to upload to GitHub Releases (PAT with 'repo' scope)."
}

Write-Host "== Package (Release) ==" -ForegroundColor Cyan
& "$repoRoot\package.ps1" -Version $Version
if ($LASTEXITCODE -ne 0) { throw "Packaging failed." }

$releasesDir = "$repoRoot\dist\Releases"

Write-Host "== Upload to GitHub Releases ==" -ForegroundColor Cyan
vpk upload github `
    --outputDir $releasesDir `
    --repoUrl "https://github.com/seoloon/wavely" `
    --token $env:GITHUB_TOKEN `
    --tag "v$Version" `
    --releaseName "Wavely v$Version" `
    --publish:$Publish.IsPresent `
    --pre:$PreRelease.IsPresent
if ($LASTEXITCODE -ne 0) { throw "vpk upload failed." }

if ($Publish.IsPresent) {
    Write-Host "Release v$Version published live on GitHub." -ForegroundColor Green
} else {
    Write-Host "Release v$Version uploaded as a DRAFT - publish it manually on GitHub when ready." -ForegroundColor Yellow
}
```

- [ ] **Step 2: Verify the script's own preconditions fail loudly**

Run (in a shell where `GITHUB_TOKEN` is not set): `.\release.ps1 -Version 0.0.0-test`
Expected: throws immediately with the "GITHUB_TOKEN environment variable is not set" message, before touching `package.ps1` at all — confirms the guard clause runs first and the `$ErrorActionPreference = 'Stop'`/`throw` pattern matches `package.ps1`'s existing convention.

- [ ] **Step 3: Commit**

```bash
git add release.ps1
git commit -m "feat: add release.ps1 - publish packaged builds to GitHub Releases"
```

---

### Task 7: End-to-end verification (real GitHub release, real update)

This task has no code changes — it's the spec's own test plan (section "Plan de test"), executed for real. Do not mark this plan complete without running it; the entire feature is unverifiable by build success alone (it depends on GitHub's real API and an installed app's real update behavior).

- [ ] **Step 1: Confirm the dev-build state (already done in Task 5, Step 3)** — skip if still fresh, otherwise repeat.

- [ ] **Step 2: Install a first version locally**

Run: `.\package.ps1 -Version 0.1.0`
Then run the produced `dist\Releases\Wavely-win-Setup.exe` and complete the install.
Expected: installed app's About tab shows `Wavely v0.1.0` and `IsInstalled == true` (status reads "Non vérifié" until you click check, or whatever the startup check already resolved to).

- [ ] **Step 3: Publish a second version as a draft, then promote it**

Run: `$env:GITHUB_TOKEN = '<your PAT>'; .\release.ps1 -Version 0.1.1`
Expected: script completes with "uploaded as a DRAFT" message. Go to `https://github.com/seoloon/wavely/releases`, find the `v0.1.1` draft, click "Publish release".

- [ ] **Step 4: Confirm the installed app picks it up**

Relaunch the `0.1.0` app installed in Step 2 (or click "Vérifier les mises à jour" in its About tab if already running).
Expected: status moves to "Mise à jour prête — redémarrez pour l'appliquer", the tray gains a "Redémarrer pour mettre à jour" item, and clicking either the tray item or the About tab's restart button relaunches the app now running `0.1.1`.

- [ ] **Step 5: Confirm graceful failure**

Disconnect network (or temporarily point `RepoUrl` at a nonexistent repo, rebuild, test, then revert), click "Vérifier les mises à jour".
Expected: status settles on "Échec de la vérification des mises à jour" without the app crashing or hanging.

- [ ] **Step 6: Record results**

No commit needed for this task (no source changes) — report the outcome of Steps 2-5 back before considering this plan done.
