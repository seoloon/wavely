# Settings Window Restyle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the default-Fluent-Windows look of the Settings window with a modern, simple, brand-consistent UI: left sidebar navigation instead of tabs, toggle switches instead of checkboxes, a brand accent color, and grouped "cards" instead of flat stacked controls — with zero behavioral change.

**Architecture:** Pure XAML/resource change to a single file, `frontend/Wavely.App/Views/SettingsWindow.axaml`. No `.cs` code-behind changes, no `SettingsViewModel*.cs` changes, no `App.axaml` changes. Avalonia's `TabControl` with `TabStripPlacement="Left"` provides the sidebar behavior natively (icon+label headers via a custom `TabItem.Header`), so navigation state stays fully declarative — no new selection logic to write or test. Visual identity comes from overriding the single `SystemAccentColor` resource (Avalonia's FluentTheme auto-derives all the light/dark accent shades controls need from that one value) scoped to the window via `Window.Resources`, plus a small reusable `Border.settingsCard` style for grouping.

**Tech Stack:** Avalonia UI 11 (FluentTheme), C#/.NET 8, existing `CommunityToolkit.Mvvm`-based `SettingsViewModel` (untouched).

## Global Constraints

- No hardcoded user-facing strings in AXAML (RULES.md §6) — reuse the existing `Strings.*` resx keys already bound in the current file; do not add new resx entries (card groupings are visual-only, no new header text).
- No `SettingsViewModel`/backend changes — every binding path (`Locked`, `ClickThroughEnabled`, `ResetSizeCommand`, `HideOnPauseEnabled`, `HideOnPauseDelaySeconds`, `LaunchAtStartup`, `PresetIndex`, `CoverShapeIndex`, `CoverGlowEnabled`, `CoverBlurEnabled`, `DynamicColorsEnabled`, `DynamicBackgroundEnabled`, `CustomAccentColor`, `SelectSpotifyAccentCommand`/`SelectDeezerAccentCommand`/`SelectAppleMusicAccentCommand`/`SelectYouTubeAccentCommand`/`SelectBlackAccentCommand`/`SelectWhiteAccentCommand`, `BackgroundOpacityPercent`, `ThemeIndex`, `CurrentVersionText`, `UpdateStatusText`, `CheckForUpdatesCommand`, `IsCheckingForUpdates`, `RestartToUpdateCommand`, `IsUpdateReady`, `ReloadWidgetCommand`) must stay exactly as-is.
- No `App.axaml` changes — all style/resource overrides are scoped to `SettingsWindow.axaml`'s own `Window.Resources`/`Window.Styles` so no other window/control is affected.
- Native Windows titlebar stays (no `SystemDecorations="None"`) — per the approved design, only the content and tab-strip are restyled.
- Every visual must keep working in both the app's Light and Dark theme variants (the existing "Thème" selector in the Appearance tab still flips `Application.RequestedThemeVariant` at runtime).

---

### Task 1: Sidebar navigation skeleton + window resize + icons

**Files:**
- Modify: `frontend/Wavely.App/Views/SettingsWindow.axaml` (full rewrite)

**Interfaces:**
- Consumes: `SettingsViewModel` bindings and `Strings.*` resx keys exactly as in the current file (no new keys, no renamed keys).
- Produces: no new public API. Purely a visual/structural change other tasks build on.

- [ ] **Step 1: Rewrite `SettingsWindow.axaml`** to widen the window and move the tab strip to the left with icon+label headers, keeping every control/binding byte-for-byte identical to today (still `CheckBox`, no cards yet — that's Tasks 2 and 3):

```xml
<Window xmlns="https://github.com/avaloniaui"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        xmlns:vm="using:Wavely.App.ViewModels"
        xmlns:res="using:Wavely.App.Resources"
        x:Class="Wavely.App.Views.SettingsWindow"
        x:DataType="vm:SettingsViewModel"
        Title="{x:Static res:Strings.SettingsWindowTitle}"
        Width="600" Height="480"
        CanResize="False"
        WindowStartupLocation="CenterScreen">
    <DockPanel Margin="16">
        <StackPanel DockPanel.Dock="Bottom" Orientation="Horizontal" HorizontalAlignment="Right" Margin="0,16,0,0" Spacing="8">
            <Button Content="{x:Static res:Strings.SettingsFooterReloadWidgetButton}" Command="{Binding ReloadWidgetCommand}" />
        </StackPanel>
        <TabControl TabStripPlacement="Left">
            <TabControl.Styles>
                <Style Selector="TabItem">
                    <Setter Property="MinWidth" Value="164" />
                    <Setter Property="Padding" Value="12,10" />
                    <Setter Property="HorizontalContentAlignment" Value="Left" />
                </Style>
            </TabControl.Styles>
            <TabItem>
                <TabItem.Header>
                    <StackPanel Orientation="Horizontal" Spacing="10">
                        <TextBlock Text="&#xE713;" FontFamily="Segoe Fluent Icons" FontSize="16" VerticalAlignment="Center" />
                        <TextBlock Text="{x:Static res:Strings.SettingsTabBehavior}" VerticalAlignment="Center" />
                    </StackPanel>
                </TabItem.Header>
                <ScrollViewer>
                <StackPanel Margin="20,4,4,4" Spacing="16">
                    <CheckBox Content="{x:Static res:Strings.SettingsBehaviorLockedLabel}" IsChecked="{Binding Locked}" />
                    <CheckBox Content="{x:Static res:Strings.SettingsBehaviorClickThroughLabel}" IsChecked="{Binding ClickThroughEnabled}" />
                    <Button Content="{x:Static res:Strings.SettingsBehaviorResetSizeButton}" Command="{Binding ResetSizeCommand}" HorizontalAlignment="Left" />

                    <CheckBox Content="{x:Static res:Strings.SettingsBehaviorHideOnPauseLabel}" IsChecked="{Binding HideOnPauseEnabled}" />
                    <StackPanel Spacing="4" IsEnabled="{Binding HideOnPauseEnabled}">
                        <TextBlock Text="{x:Static res:Strings.SettingsBehaviorHideOnPauseDelayLabel}" />
                        <Slider Minimum="5" Maximum="30" TickFrequency="1" IsSnapToTickEnabled="True" Value="{Binding HideOnPauseDelaySeconds}" />
                        <TextBlock Text="{Binding HideOnPauseDelaySeconds}" HorizontalAlignment="Right" Opacity="0.7" FontSize="11" />
                    </StackPanel>

                    <CheckBox Content="{x:Static res:Strings.SettingsBehaviorLaunchAtStartupLabel}" IsChecked="{Binding LaunchAtStartup}" />

                    <StackPanel Spacing="4">
                        <TextBlock Text="{x:Static res:Strings.SettingsBehaviorLanguageLabel}" />
                        <ComboBox SelectedIndex="0" HorizontalAlignment="Left" MinWidth="160">
                            <ComboBoxItem Content="Français" />
                        </ComboBox>
                    </StackPanel>
                </StackPanel>
                </ScrollViewer>
            </TabItem>
            <TabItem>
                <TabItem.Header>
                    <StackPanel Orientation="Horizontal" Spacing="10">
                        <TextBlock Text="&#xE790;" FontFamily="Segoe Fluent Icons" FontSize="16" VerticalAlignment="Center" />
                        <TextBlock Text="{x:Static res:Strings.SettingsTabAppearance}" VerticalAlignment="Center" />
                    </StackPanel>
                </TabItem.Header>
                <ScrollViewer>
                <StackPanel Margin="20,4,4,4" Spacing="16">
                    <StackPanel Spacing="4">
                        <TextBlock Text="{x:Static res:Strings.SettingsAppearancePresetLabel}" />
                        <ComboBox ItemsSource="{x:Static vm:SettingsViewModel.PresetNames}" SelectedIndex="{Binding PresetIndex}" HorizontalAlignment="Left" MinWidth="160" />
                    </StackPanel>

                    <StackPanel Spacing="4">
                        <TextBlock Text="{x:Static res:Strings.SettingsAppearanceCoverShapeLabel}" />
                        <ComboBox ItemsSource="{x:Static vm:SettingsViewModel.CoverShapeNames}" SelectedIndex="{Binding CoverShapeIndex}" HorizontalAlignment="Left" MinWidth="160" />
                    </StackPanel>

                    <CheckBox Content="{x:Static res:Strings.SettingsAppearanceGlowLabel}" IsChecked="{Binding CoverGlowEnabled}" />
                    <CheckBox Content="{x:Static res:Strings.SettingsAppearanceBlurredCoverLabel}" IsChecked="{Binding CoverBlurEnabled}" />

                    <CheckBox Content="{x:Static res:Strings.SettingsAppearanceDynamicColorsLabel}" IsChecked="{Binding DynamicColorsEnabled}" />
                    <CheckBox Content="{x:Static res:Strings.SettingsAppearanceDynamicBackgroundLabel}" IsChecked="{Binding DynamicBackgroundEnabled}" IsEnabled="{Binding DynamicColorsEnabled}" Margin="20,0,0,0" />

                    <StackPanel Spacing="6" IsVisible="{Binding !DynamicColorsEnabled}" Margin="20,0,0,0">
                        <TextBlock Text="{x:Static res:Strings.SettingsAppearanceCustomAccentColorLabel}" />
                        <StackPanel Orientation="Horizontal" Spacing="6">
                            <Button Width="24" Height="24" CornerRadius="12" Background="#1DB954" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectSpotifyAccentCommand}" />
                            <Button Width="24" Height="24" CornerRadius="12" Background="#A238FF" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectDeezerAccentCommand}" />
                            <Button Width="24" Height="24" CornerRadius="12" Background="#FA243C" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectAppleMusicAccentCommand}" />
                            <Button Width="24" Height="24" CornerRadius="12" Background="#FF0000" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectYouTubeAccentCommand}" />
                            <Button Width="24" Height="24" CornerRadius="12" Background="#000000" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectBlackAccentCommand}" />
                            <Button Width="24" Height="24" CornerRadius="12" Background="#FFFFFF" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectWhiteAccentCommand}" />
                        </StackPanel>
                        <ColorPicker Color="{Binding CustomAccentColor}" HorizontalAlignment="Left" />
                    </StackPanel>

                    <StackPanel Spacing="4">
                        <TextBlock Text="{x:Static res:Strings.SettingsAppearanceOpacityLabel}" />
                        <Slider Minimum="0" Maximum="100" Value="{Binding BackgroundOpacityPercent}" />
                        <TextBlock Text="{Binding BackgroundOpacityPercent, StringFormat={}{0:0}%}" HorizontalAlignment="Right" Opacity="0.7" FontSize="11" />
                    </StackPanel>

                    <StackPanel Spacing="4">
                        <TextBlock Text="{x:Static res:Strings.SettingsAppearanceThemeLabel}" />
                        <ComboBox ItemsSource="{x:Static vm:SettingsViewModel.ThemeNames}" SelectedIndex="{Binding ThemeIndex}" HorizontalAlignment="Left" MinWidth="160" />
                    </StackPanel>
                </StackPanel>
                </ScrollViewer>
            </TabItem>
            <TabItem>
                <TabItem.Header>
                    <StackPanel Orientation="Horizontal" Spacing="10">
                        <TextBlock Text="&#xE946;" FontFamily="Segoe Fluent Icons" FontSize="16" VerticalAlignment="Center" />
                        <TextBlock Text="{x:Static res:Strings.SettingsTabAbout}" VerticalAlignment="Center" />
                    </StackPanel>
                </TabItem.Header>
                <StackPanel Margin="20,4,4,4" Spacing="16">
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
        </TabControl>
    </DockPanel>
</Window>
```

- [ ] **Step 2: Build**

Run: `.\build.ps1` from the repo root (backend is unaffected but the script is the documented single entry point — see `BUILD.md`).
Expected: `Build complete (Debug).` with no errors.

- [ ] **Step 3: Run and visually verify**

Run: `frontend\Wavely.App\bin\Debug\net8.0-windows10.0.19041.0\Wavely.App.exe`, then open Settings from the tray icon menu.
Expected: window is 600×480; three entries (gear/Comportement, palette/Apparence, info/À propos) are stacked vertically on the **left**, each showing an icon next to its label; clicking each one swaps the content on the right; every field present before (checkboxes, sliders, combo boxes, reset/reload buttons, color swatches, About text) is still there and functionally identical (e.g. toggling "Verrouiller" still locks the widget, "Recharger le widget" still works from any section).
If a glyph renders as a blank box, swap that `TextBlock`'s `Text` for a different Segoe Fluent Icons codepoint (e.g. `&#xE713;` behavior/gear, `&#xE790;` appearance/color, `&#xE946;` about/info) — cosmetic only, does not affect functionality.

- [ ] **Step 4: Commit**

```bash
git add frontend/Wavely.App/Views/SettingsWindow.axaml
git commit -m "feat: move Settings navigation to a left sidebar with icons"
```

---

### Task 2: Brand accent color + toggle switches

**Files:**
- Modify: `frontend/Wavely.App/Views/SettingsWindow.axaml` (full rewrite, builds on Task 1's output)

**Interfaces:**
- Consumes: same `SettingsViewModel` bindings as Task 1 (unchanged), plus Avalonia FluentTheme's `SystemAccentColor` resource key (built-in — overriding it is the documented way to recolor `ToggleSwitch`, `Slider`, the selected-tab indicator, and `Button Classes="accent"` without touching their control templates).
- Produces: no new public API.

- [ ] **Step 1: Rewrite `SettingsWindow.axaml`** to (a) override the accent color scoped to this window, (b) turn every boolean `CheckBox` into a `ToggleSwitch` in a label/toggle row, and (c) make the "Recharger le widget" button an accent-filled button:

```xml
<Window xmlns="https://github.com/avaloniaui"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        xmlns:vm="using:Wavely.App.ViewModels"
        xmlns:res="using:Wavely.App.Resources"
        x:Class="Wavely.App.Views.SettingsWindow"
        x:DataType="vm:SettingsViewModel"
        Title="{x:Static res:Strings.SettingsWindowTitle}"
        Width="600" Height="480"
        CanResize="False"
        WindowStartupLocation="CenterScreen">
    <Window.Resources>
        <Color x:Key="SystemAccentColor">#5AAAFF</Color>
    </Window.Resources>
    <DockPanel Margin="16">
        <StackPanel DockPanel.Dock="Bottom" Orientation="Horizontal" HorizontalAlignment="Right" Margin="0,16,0,0" Spacing="8">
            <Button Classes="accent" Content="{x:Static res:Strings.SettingsFooterReloadWidgetButton}" Command="{Binding ReloadWidgetCommand}" />
        </StackPanel>
        <TabControl TabStripPlacement="Left">
            <TabControl.Styles>
                <Style Selector="TabItem">
                    <Setter Property="MinWidth" Value="164" />
                    <Setter Property="Padding" Value="12,10" />
                    <Setter Property="HorizontalContentAlignment" Value="Left" />
                </Style>
            </TabControl.Styles>
            <TabItem>
                <TabItem.Header>
                    <StackPanel Orientation="Horizontal" Spacing="10">
                        <TextBlock Text="&#xE713;" FontFamily="Segoe Fluent Icons" FontSize="16" VerticalAlignment="Center" />
                        <TextBlock Text="{x:Static res:Strings.SettingsTabBehavior}" VerticalAlignment="Center" />
                    </StackPanel>
                </TabItem.Header>
                <ScrollViewer>
                <StackPanel Margin="20,4,4,4" Spacing="16">
                    <Grid ColumnDefinitions="*,Auto">
                        <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsBehaviorLockedLabel}" VerticalAlignment="Center" />
                        <ToggleSwitch Grid.Column="1" IsChecked="{Binding Locked}" OnContent="{x:Null}" OffContent="{x:Null}" />
                    </Grid>
                    <Grid ColumnDefinitions="*,Auto">
                        <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsBehaviorClickThroughLabel}" VerticalAlignment="Center" />
                        <ToggleSwitch Grid.Column="1" IsChecked="{Binding ClickThroughEnabled}" OnContent="{x:Null}" OffContent="{x:Null}" />
                    </Grid>
                    <Button Content="{x:Static res:Strings.SettingsBehaviorResetSizeButton}" Command="{Binding ResetSizeCommand}" HorizontalAlignment="Left" />

                    <Grid ColumnDefinitions="*,Auto">
                        <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsBehaviorHideOnPauseLabel}" VerticalAlignment="Center" />
                        <ToggleSwitch Grid.Column="1" IsChecked="{Binding HideOnPauseEnabled}" OnContent="{x:Null}" OffContent="{x:Null}" />
                    </Grid>
                    <StackPanel Spacing="4" IsEnabled="{Binding HideOnPauseEnabled}">
                        <TextBlock Text="{x:Static res:Strings.SettingsBehaviorHideOnPauseDelayLabel}" />
                        <Slider Minimum="5" Maximum="30" TickFrequency="1" IsSnapToTickEnabled="True" Value="{Binding HideOnPauseDelaySeconds}" />
                        <TextBlock Text="{Binding HideOnPauseDelaySeconds}" HorizontalAlignment="Right" Opacity="0.7" FontSize="11" />
                    </StackPanel>

                    <Grid ColumnDefinitions="*,Auto">
                        <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsBehaviorLaunchAtStartupLabel}" VerticalAlignment="Center" />
                        <ToggleSwitch Grid.Column="1" IsChecked="{Binding LaunchAtStartup}" OnContent="{x:Null}" OffContent="{x:Null}" />
                    </Grid>

                    <StackPanel Spacing="4">
                        <TextBlock Text="{x:Static res:Strings.SettingsBehaviorLanguageLabel}" />
                        <ComboBox SelectedIndex="0" HorizontalAlignment="Left" MinWidth="160">
                            <ComboBoxItem Content="Français" />
                        </ComboBox>
                    </StackPanel>
                </StackPanel>
                </ScrollViewer>
            </TabItem>
            <TabItem>
                <TabItem.Header>
                    <StackPanel Orientation="Horizontal" Spacing="10">
                        <TextBlock Text="&#xE790;" FontFamily="Segoe Fluent Icons" FontSize="16" VerticalAlignment="Center" />
                        <TextBlock Text="{x:Static res:Strings.SettingsTabAppearance}" VerticalAlignment="Center" />
                    </StackPanel>
                </TabItem.Header>
                <ScrollViewer>
                <StackPanel Margin="20,4,4,4" Spacing="16">
                    <StackPanel Spacing="4">
                        <TextBlock Text="{x:Static res:Strings.SettingsAppearancePresetLabel}" />
                        <ComboBox ItemsSource="{x:Static vm:SettingsViewModel.PresetNames}" SelectedIndex="{Binding PresetIndex}" HorizontalAlignment="Left" MinWidth="160" />
                    </StackPanel>

                    <StackPanel Spacing="4">
                        <TextBlock Text="{x:Static res:Strings.SettingsAppearanceCoverShapeLabel}" />
                        <ComboBox ItemsSource="{x:Static vm:SettingsViewModel.CoverShapeNames}" SelectedIndex="{Binding CoverShapeIndex}" HorizontalAlignment="Left" MinWidth="160" />
                    </StackPanel>

                    <Grid ColumnDefinitions="*,Auto">
                        <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsAppearanceGlowLabel}" VerticalAlignment="Center" />
                        <ToggleSwitch Grid.Column="1" IsChecked="{Binding CoverGlowEnabled}" OnContent="{x:Null}" OffContent="{x:Null}" />
                    </Grid>
                    <Grid ColumnDefinitions="*,Auto">
                        <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsAppearanceBlurredCoverLabel}" VerticalAlignment="Center" />
                        <ToggleSwitch Grid.Column="1" IsChecked="{Binding CoverBlurEnabled}" OnContent="{x:Null}" OffContent="{x:Null}" />
                    </Grid>

                    <Grid ColumnDefinitions="*,Auto">
                        <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsAppearanceDynamicColorsLabel}" VerticalAlignment="Center" />
                        <ToggleSwitch Grid.Column="1" IsChecked="{Binding DynamicColorsEnabled}" OnContent="{x:Null}" OffContent="{x:Null}" />
                    </Grid>
                    <Grid ColumnDefinitions="*,Auto" IsEnabled="{Binding DynamicColorsEnabled}" Margin="20,0,0,0">
                        <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsAppearanceDynamicBackgroundLabel}" VerticalAlignment="Center" />
                        <ToggleSwitch Grid.Column="1" IsChecked="{Binding DynamicBackgroundEnabled}" OnContent="{x:Null}" OffContent="{x:Null}" />
                    </Grid>

                    <StackPanel Spacing="6" IsVisible="{Binding !DynamicColorsEnabled}" Margin="20,0,0,0">
                        <TextBlock Text="{x:Static res:Strings.SettingsAppearanceCustomAccentColorLabel}" />
                        <StackPanel Orientation="Horizontal" Spacing="6">
                            <Button Width="24" Height="24" CornerRadius="12" Background="#1DB954" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectSpotifyAccentCommand}" />
                            <Button Width="24" Height="24" CornerRadius="12" Background="#A238FF" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectDeezerAccentCommand}" />
                            <Button Width="24" Height="24" CornerRadius="12" Background="#FA243C" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectAppleMusicAccentCommand}" />
                            <Button Width="24" Height="24" CornerRadius="12" Background="#FF0000" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectYouTubeAccentCommand}" />
                            <Button Width="24" Height="24" CornerRadius="12" Background="#000000" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectBlackAccentCommand}" />
                            <Button Width="24" Height="24" CornerRadius="12" Background="#FFFFFF" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectWhiteAccentCommand}" />
                        </StackPanel>
                        <ColorPicker Color="{Binding CustomAccentColor}" HorizontalAlignment="Left" />
                    </StackPanel>

                    <StackPanel Spacing="4">
                        <TextBlock Text="{x:Static res:Strings.SettingsAppearanceOpacityLabel}" />
                        <Slider Minimum="0" Maximum="100" Value="{Binding BackgroundOpacityPercent}" />
                        <TextBlock Text="{Binding BackgroundOpacityPercent, StringFormat={}{0:0}%}" HorizontalAlignment="Right" Opacity="0.7" FontSize="11" />
                    </StackPanel>

                    <StackPanel Spacing="4">
                        <TextBlock Text="{x:Static res:Strings.SettingsAppearanceThemeLabel}" />
                        <ComboBox ItemsSource="{x:Static vm:SettingsViewModel.ThemeNames}" SelectedIndex="{Binding ThemeIndex}" HorizontalAlignment="Left" MinWidth="160" />
                    </StackPanel>
                </StackPanel>
                </ScrollViewer>
            </TabItem>
            <TabItem>
                <TabItem.Header>
                    <StackPanel Orientation="Horizontal" Spacing="10">
                        <TextBlock Text="&#xE946;" FontFamily="Segoe Fluent Icons" FontSize="16" VerticalAlignment="Center" />
                        <TextBlock Text="{x:Static res:Strings.SettingsTabAbout}" VerticalAlignment="Center" />
                    </StackPanel>
                </TabItem.Header>
                <StackPanel Margin="20,4,4,4" Spacing="16">
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
        </TabControl>
    </DockPanel>
</Window>
```

- [ ] **Step 2: Build**

Run: `.\build.ps1`
Expected: `Build complete (Debug).` with no errors.

- [ ] **Step 3: Run and visually verify**

Run the built exe, open Settings.
Expected: every former checkbox is now a toggle switch (label on the left, switch on the right), all lit blue (`#5AAAFF`) when on; the selected sidebar item shows a blue indicator; "Recharger le widget" is a solid blue button. Toggle each switch and confirm the underlying behavior still fires exactly as before (e.g. "Click-through" still applies `WS_EX_TRANSPARENT` live on the widget, "Lancer au démarrage" still writes the registry `Run` key). Switch "Thème" between Clair/Sombre and confirm the Settings window itself stays readable and the accent stays blue in both.

- [ ] **Step 4: Commit**

```bash
git add frontend/Wavely.App/Views/SettingsWindow.axaml
git commit -m "style: brand accent color and toggle switches in Settings"
```

---

### Task 3: Card grouping + final polish

**Files:**
- Modify: `frontend/Wavely.App/Views/SettingsWindow.axaml` (full rewrite, builds on Task 2's output)

**Interfaces:**
- Consumes: same `SettingsViewModel` bindings as Tasks 1-2 (unchanged).
- Produces: no new public API. Final visual state matching `docs/superpowers/specs/2026-07-31-settings-window-restyle-design.md`.

- [ ] **Step 1: Rewrite `SettingsWindow.axaml`** to wrap related fields in theme-aware "card" borders (light/dark-adaptive background via `ResourceDictionary.ThemeDictionaries`) and move the `TabItem`/card styles into `Window.Styles` for reuse:

```xml
<Window xmlns="https://github.com/avaloniaui"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        xmlns:vm="using:Wavely.App.ViewModels"
        xmlns:res="using:Wavely.App.Resources"
        x:Class="Wavely.App.Views.SettingsWindow"
        x:DataType="vm:SettingsViewModel"
        Title="{x:Static res:Strings.SettingsWindowTitle}"
        Width="600" Height="480"
        CanResize="False"
        WindowStartupLocation="CenterScreen">
    <Window.Resources>
        <ResourceDictionary>
            <Color x:Key="SystemAccentColor">#5AAAFF</Color>
            <ResourceDictionary.ThemeDictionaries>
                <ResourceDictionary x:Key="Dark">
                    <SolidColorBrush x:Key="CardBackgroundBrush" Color="#12FFFFFF" />
                    <SolidColorBrush x:Key="CardBorderBrush" Color="#26FFFFFF" />
                </ResourceDictionary>
                <ResourceDictionary x:Key="Light">
                    <SolidColorBrush x:Key="CardBackgroundBrush" Color="#0A000000" />
                    <SolidColorBrush x:Key="CardBorderBrush" Color="#14000000" />
                </ResourceDictionary>
            </ResourceDictionary.ThemeDictionaries>
        </ResourceDictionary>
    </Window.Resources>
    <Window.Styles>
        <Style Selector="TabItem">
            <Setter Property="MinWidth" Value="164" />
            <Setter Property="Padding" Value="12,10" />
            <Setter Property="HorizontalContentAlignment" Value="Left" />
        </Style>
        <Style Selector="Border.settingsCard">
            <Setter Property="Background" Value="{DynamicResource CardBackgroundBrush}" />
            <Setter Property="BorderBrush" Value="{DynamicResource CardBorderBrush}" />
            <Setter Property="BorderThickness" Value="1" />
            <Setter Property="CornerRadius" Value="8" />
            <Setter Property="Padding" Value="16" />
        </Style>
    </Window.Styles>
    <DockPanel Margin="16">
        <StackPanel DockPanel.Dock="Bottom" Orientation="Horizontal" HorizontalAlignment="Right" Margin="0,16,0,0" Spacing="8">
            <Button Classes="accent" Content="{x:Static res:Strings.SettingsFooterReloadWidgetButton}" Command="{Binding ReloadWidgetCommand}" />
        </StackPanel>
        <TabControl TabStripPlacement="Left">
            <TabItem>
                <TabItem.Header>
                    <StackPanel Orientation="Horizontal" Spacing="10">
                        <TextBlock Text="&#xE713;" FontFamily="Segoe Fluent Icons" FontSize="16" VerticalAlignment="Center" />
                        <TextBlock Text="{x:Static res:Strings.SettingsTabBehavior}" VerticalAlignment="Center" />
                    </StackPanel>
                </TabItem.Header>
                <ScrollViewer>
                <StackPanel Margin="20,4,4,4" Spacing="20">
                    <Border Classes="settingsCard">
                        <StackPanel Spacing="14">
                            <Grid ColumnDefinitions="*,Auto">
                                <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsBehaviorLockedLabel}" VerticalAlignment="Center" />
                                <ToggleSwitch Grid.Column="1" IsChecked="{Binding Locked}" OnContent="{x:Null}" OffContent="{x:Null}" />
                            </Grid>
                            <Grid ColumnDefinitions="*,Auto">
                                <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsBehaviorClickThroughLabel}" VerticalAlignment="Center" />
                                <ToggleSwitch Grid.Column="1" IsChecked="{Binding ClickThroughEnabled}" OnContent="{x:Null}" OffContent="{x:Null}" />
                            </Grid>
                            <Button Content="{x:Static res:Strings.SettingsBehaviorResetSizeButton}" Command="{Binding ResetSizeCommand}" HorizontalAlignment="Left" />
                        </StackPanel>
                    </Border>

                    <Border Classes="settingsCard">
                        <StackPanel Spacing="14">
                            <Grid ColumnDefinitions="*,Auto">
                                <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsBehaviorHideOnPauseLabel}" VerticalAlignment="Center" />
                                <ToggleSwitch Grid.Column="1" IsChecked="{Binding HideOnPauseEnabled}" OnContent="{x:Null}" OffContent="{x:Null}" />
                            </Grid>
                            <StackPanel Spacing="4" IsEnabled="{Binding HideOnPauseEnabled}">
                                <TextBlock Text="{x:Static res:Strings.SettingsBehaviorHideOnPauseDelayLabel}" />
                                <Slider Minimum="5" Maximum="30" TickFrequency="1" IsSnapToTickEnabled="True" Value="{Binding HideOnPauseDelaySeconds}" />
                                <TextBlock Text="{Binding HideOnPauseDelaySeconds}" HorizontalAlignment="Right" Opacity="0.7" FontSize="11" />
                            </StackPanel>
                        </StackPanel>
                    </Border>

                    <Border Classes="settingsCard">
                        <StackPanel Spacing="14">
                            <Grid ColumnDefinitions="*,Auto">
                                <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsBehaviorLaunchAtStartupLabel}" VerticalAlignment="Center" />
                                <ToggleSwitch Grid.Column="1" IsChecked="{Binding LaunchAtStartup}" OnContent="{x:Null}" OffContent="{x:Null}" />
                            </Grid>
                            <StackPanel Spacing="4">
                                <TextBlock Text="{x:Static res:Strings.SettingsBehaviorLanguageLabel}" />
                                <ComboBox SelectedIndex="0" HorizontalAlignment="Left" MinWidth="160">
                                    <ComboBoxItem Content="Français" />
                                </ComboBox>
                            </StackPanel>
                        </StackPanel>
                    </Border>
                </StackPanel>
                </ScrollViewer>
            </TabItem>
            <TabItem>
                <TabItem.Header>
                    <StackPanel Orientation="Horizontal" Spacing="10">
                        <TextBlock Text="&#xE790;" FontFamily="Segoe Fluent Icons" FontSize="16" VerticalAlignment="Center" />
                        <TextBlock Text="{x:Static res:Strings.SettingsTabAppearance}" VerticalAlignment="Center" />
                    </StackPanel>
                </TabItem.Header>
                <ScrollViewer>
                <StackPanel Margin="20,4,4,4" Spacing="20">
                    <Border Classes="settingsCard">
                        <StackPanel Spacing="14">
                            <StackPanel Spacing="4">
                                <TextBlock Text="{x:Static res:Strings.SettingsAppearancePresetLabel}" />
                                <ComboBox ItemsSource="{x:Static vm:SettingsViewModel.PresetNames}" SelectedIndex="{Binding PresetIndex}" HorizontalAlignment="Left" MinWidth="160" />
                            </StackPanel>
                            <StackPanel Spacing="4">
                                <TextBlock Text="{x:Static res:Strings.SettingsAppearanceCoverShapeLabel}" />
                                <ComboBox ItemsSource="{x:Static vm:SettingsViewModel.CoverShapeNames}" SelectedIndex="{Binding CoverShapeIndex}" HorizontalAlignment="Left" MinWidth="160" />
                            </StackPanel>
                            <Grid ColumnDefinitions="*,Auto">
                                <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsAppearanceGlowLabel}" VerticalAlignment="Center" />
                                <ToggleSwitch Grid.Column="1" IsChecked="{Binding CoverGlowEnabled}" OnContent="{x:Null}" OffContent="{x:Null}" />
                            </Grid>
                            <Grid ColumnDefinitions="*,Auto">
                                <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsAppearanceBlurredCoverLabel}" VerticalAlignment="Center" />
                                <ToggleSwitch Grid.Column="1" IsChecked="{Binding CoverBlurEnabled}" OnContent="{x:Null}" OffContent="{x:Null}" />
                            </Grid>
                        </StackPanel>
                    </Border>

                    <Border Classes="settingsCard">
                        <StackPanel Spacing="14">
                            <Grid ColumnDefinitions="*,Auto">
                                <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsAppearanceDynamicColorsLabel}" VerticalAlignment="Center" />
                                <ToggleSwitch Grid.Column="1" IsChecked="{Binding DynamicColorsEnabled}" OnContent="{x:Null}" OffContent="{x:Null}" />
                            </Grid>
                            <Grid ColumnDefinitions="*,Auto" IsEnabled="{Binding DynamicColorsEnabled}" Margin="20,0,0,0">
                                <TextBlock Grid.Column="0" Text="{x:Static res:Strings.SettingsAppearanceDynamicBackgroundLabel}" VerticalAlignment="Center" />
                                <ToggleSwitch Grid.Column="1" IsChecked="{Binding DynamicBackgroundEnabled}" OnContent="{x:Null}" OffContent="{x:Null}" />
                            </Grid>
                            <StackPanel Spacing="6" IsVisible="{Binding !DynamicColorsEnabled}" Margin="20,0,0,0">
                                <TextBlock Text="{x:Static res:Strings.SettingsAppearanceCustomAccentColorLabel}" />
                                <StackPanel Orientation="Horizontal" Spacing="6">
                                    <Button Width="24" Height="24" CornerRadius="12" Background="#1DB954" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectSpotifyAccentCommand}" />
                                    <Button Width="24" Height="24" CornerRadius="12" Background="#A238FF" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectDeezerAccentCommand}" />
                                    <Button Width="24" Height="24" CornerRadius="12" Background="#FA243C" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectAppleMusicAccentCommand}" />
                                    <Button Width="24" Height="24" CornerRadius="12" Background="#FF0000" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectYouTubeAccentCommand}" />
                                    <Button Width="24" Height="24" CornerRadius="12" Background="#000000" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectBlackAccentCommand}" />
                                    <Button Width="24" Height="24" CornerRadius="12" Background="#FFFFFF" BorderBrush="#40FFFFFF" BorderThickness="1" Command="{Binding SelectWhiteAccentCommand}" />
                                </StackPanel>
                                <ColorPicker Color="{Binding CustomAccentColor}" HorizontalAlignment="Left" />
                            </StackPanel>
                        </StackPanel>
                    </Border>

                    <Border Classes="settingsCard">
                        <StackPanel Spacing="14">
                            <StackPanel Spacing="4">
                                <TextBlock Text="{x:Static res:Strings.SettingsAppearanceOpacityLabel}" />
                                <Slider Minimum="0" Maximum="100" Value="{Binding BackgroundOpacityPercent}" />
                                <TextBlock Text="{Binding BackgroundOpacityPercent, StringFormat={}{0:0}%}" HorizontalAlignment="Right" Opacity="0.7" FontSize="11" />
                            </StackPanel>
                            <StackPanel Spacing="4">
                                <TextBlock Text="{x:Static res:Strings.SettingsAppearanceThemeLabel}" />
                                <ComboBox ItemsSource="{x:Static vm:SettingsViewModel.ThemeNames}" SelectedIndex="{Binding ThemeIndex}" HorizontalAlignment="Left" MinWidth="160" />
                            </StackPanel>
                        </StackPanel>
                    </Border>
                </StackPanel>
                </ScrollViewer>
            </TabItem>
            <TabItem>
                <TabItem.Header>
                    <StackPanel Orientation="Horizontal" Spacing="10">
                        <TextBlock Text="&#xE946;" FontFamily="Segoe Fluent Icons" FontSize="16" VerticalAlignment="Center" />
                        <TextBlock Text="{x:Static res:Strings.SettingsTabAbout}" VerticalAlignment="Center" />
                    </StackPanel>
                </TabItem.Header>
                <StackPanel Margin="20,4,4,4">
                    <Border Classes="settingsCard">
                        <StackPanel Spacing="16">
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
                    </Border>
                </StackPanel>
            </TabItem>
        </TabControl>
    </DockPanel>
</Window>
```

- [ ] **Step 2: Build**

Run: `.\build.ps1`
Expected: `Build complete (Debug).` with no errors.

- [ ] **Step 3: Run and do the full manual test pass from the spec**

Run the built exe and, with Settings open:
1. Confirm each section's fields are grouped into visually distinct rounded cards with consistent spacing (20px between cards, 14px between rows inside a card).
2. Toggle every switch on both tabs and confirm the previously-verified live behaviors still fire (Locked, Click-through with `WS_EX_TRANSPARENT`, Hide-on-pause + delay slider enabling/disabling correctly, Launch at startup registry write, Dynamic colors enabling/disabling the Dynamic background row, Glow/Blur, Opacity live on the widget background, Theme switching `Application.RequestedThemeVariant` live).
3. Switch "Thème" to Clair then back to Sombre — confirm card backgrounds/borders stay legible and the accent stays the brand blue in both variants (this is what `ThemeDictionaries` is for; if a card is invisible or too strong in one variant, adjust that variant's `CardBackgroundBrush`/`CardBorderBrush` alpha in `Window.Resources`).
4. Click "Recharger le widget" from each of the 3 sections — confirm it still reloads the GSMTC hook regardless of which section is active.
5. Resize check: window is `CanResize="False"`, so just confirm nothing clips or overlaps at 600×480 with the longest content (Appearance tab with the custom accent color picker expanded).

- [ ] **Step 4: Commit**

```bash
git add frontend/Wavely.App/Views/SettingsWindow.axaml
git commit -m "style: group Settings fields into cards, finish restyle"
```

## Post-plan

Once all 3 tasks are done and verified, update `claude/PLAN.md`'s Phase 4 status note to mention the visual restyle (this project's established convention — every phase entry there records what was verified and how, see the existing Session 5/6 notes for Phase 4).
