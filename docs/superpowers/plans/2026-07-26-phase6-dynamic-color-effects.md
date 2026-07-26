# Phase 6 (6.2-6.6) — Dynamic Color Binding & Visual Effects Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bind the backend's already-shipped dominant-color extraction (Phase 6.1, commit `7a50339`) to the widget's visuals, and implement the four GPU visual effects (blurred background, glow, squircle, vinyl) that the Settings window's Appearance tab already persists but nothing renders yet.

**Architecture:** Pure-logic color resolution (`Services/DynamicColorService.cs`) feeds a small set of `Apply*` methods on `MainWindow` (moved into a new `MainWindow.Appearance.cs` partial-class file to keep the file under RULES.md's ~200-line guidance) that mutate existing Avalonia controls' brushes/effects/clips/transforms. No new WinRT surface — everything consumes `TrackInfo.DominantColors` and the `AppConfig` toggles that already exist and are already wired to the Settings UI.

**Tech Stack:** C#/.NET 8, Avalonia 11.3.18 (`Avalonia.Media.BlurEffect`/`DropShadowDirectionEffect` — verified there is no `Avalonia.Media.Effects` sub-namespace in this version, and no plain `DropShadowEffect`; see Task 4's and Task 5's amendments — `Avalonia.Media.StreamGeometry`/`EllipseGeometry`, `Avalonia.Media.RotateTransform`), `Avalonia.Threading.DispatcherTimer`.

## Global Constraints

- Frontend nullable reference types enabled (`<Nullable>enable</Nullable>`); no `catch {}` empty blocks (RULES.md §4).
- PascalCase types/methods/properties, `camelCase` locals, `_camelCase` private fields (RULES.md §3).
- No magic numbers — every tuning value is a named `const`/`static readonly` (RULES.md §3).
- Visual effects (blur, glow) must be GPU-accelerated (Avalonia's Skia-backed effects), never a software fallback (RULES.md §2) — this plan only uses Avalonia's built-in `BlurEffect`/`DropShadowDirectionEffect`, never a custom shader.
- Classes should not exceed ~200 lines (RULES.md §3) — `MainWindow.axaml.cs` is already at 351 lines before this phase; Task 3 splits appearance-related methods into a new `MainWindow.Appearance.cs` partial-class file rather than growing the existing one further.
- **No automated test project exists anywhere in this repo** (checked: zero `*.Tests` projects across all 5 shipped phases). Every prior phase's `PLAN.md` status note describes verification as *"vérifié par interaction réelle"* — building, running `Wavely.App.exe` against a real GSMTC session, and confirming behavior visually (screenshots) — not unit tests. This plan follows that established practice: each task's verification step is "build, run, observe," not "write a failing unit test." A unit-testable core (`DynamicColorService`, `SquircleGeometry`) is still isolated into its own file even without a test harness, so one could be added later without touching UI code.
- Build: `.\build.ps1 -Configuration Debug` from the repo root (builds backend via MSBuild, then frontend via `dotnet build`). Backend is unchanged in this plan (6.1 already committed) — only the frontend half needs rebuilding per task, but running the full script is harmless and simplest.
- Run: `frontend\Wavely.App\bin\Debug\net8.0-windows10.0.19041.0\Wavely.App.exe`
- Manual verification needs a real GSMTC session with cover art (e.g. Spotify) — same setup used to verify Phases 1/4/5.

---

## File Structure

- Create: `frontend/Wavely.App/Services/DynamicColorService.cs` — pure logic: unpack `TrackInfo.DominantColors`, resolve a `WidgetColorScheme`.
- Modify: `frontend/Wavely.App/Controls/WaveformControl.cs` — replace the fixed bar brush with a settable `AccentColor` property.
- Create: `frontend/Wavely.App/Views/MainWindow.Appearance.cs` — new partial-class file holding all Phase 6 rendering logic (moved `ApplyAppearance`, plus `ApplyDynamicColors`/`ApplyGlow`/`ApplyBlurBackground`/`ApplyCoverShape`/`UpdateVinylRotationState`).
- Modify: `frontend/Wavely.App/Views/MainWindow.axaml.cs` — add `_currentTrack`/`_isPlaying` fields and the handful of call sites that invoke the new `Apply*` methods; remove `ApplyAppearance` (moved out).
- Modify: `frontend/Wavely.App/Views/MainWindow.axaml` — name the background brush; layer in a blurred background image; restructure the cover into a `Grid` with a rotation-ready `Image` and a vinyl spindle overlay.
- Create: `frontend/Wavely.App/Core/SquircleGeometry.cs` — superellipse clip-geometry builder, cached by size.

---

### Task 1: DynamicColorService

**Files:**
- Create: `frontend/Wavely.App/Services/DynamicColorService.cs`

**Interfaces:**
- Produces: `WidgetColorScheme` record (`Background`, `Accent`, `Glow` : `Avalonia.Media.Color`, `TextIsDark` : `bool`), `WidgetColorScheme.Default` (the pre-Phase-6 static look), `DynamicColorService.Resolve(Wavely.Backend.TrackInfo track) : WidgetColorScheme`.

- [ ] **Step 1: Write the service**

```csharp
using System.Runtime.InteropServices;
using Avalonia.Media;
using Wavely.Backend;
using Windows.Storage.Streams;

namespace Wavely.App.Services;

/// <summary>Resolved colors for one track, ready to apply to the widget's background, waveform
/// accent, glow, and text. See
/// docs/superpowers/specs/2026-07-26-phase6-dynamic-color-effects-design.md.</summary>
public sealed record WidgetColorScheme(Color Background, Color Accent, Color Glow, bool TextIsDark)
{
    /// <summary>The look from before Phase 6: fixed dark background, blue waveform accent, white
    /// text, white glow. Used whenever there's no cover or its palette couldn't be decoded -
    /// callers additionally fall back to this per-element when the user has a dynamic-color
    /// toggle turned off.</summary>
    public static readonly WidgetColorScheme Default = new(
        Background: Color.FromRgb(0x14, 0x14, 0x18),
        Accent: Color.FromArgb(220, 90, 170, 255),
        Glow: Colors.White,
        TextIsDark: false);
}

/// <summary>Turns a track's backend-extracted <see cref="TrackInfo.DominantColors"/> into a
/// <see cref="WidgetColorScheme"/>. Does not know about <c>AppConfig</c> toggles - callers decide
/// per-element whether to use the resolved scheme or <see cref="WidgetColorScheme.Default"/>,
/// since background/accent/glow each have their own independent enable toggle.</summary>
public static class DynamicColorService
{
    /// <summary>Relative luminance above which white text stops being reliably legible and the
    /// widget should switch to dark text instead (WCAG relative luminance, 0=black, 1=white).</summary>
    private const double DarkTextLuminanceThreshold = 0.6;

    public static WidgetColorScheme Resolve(TrackInfo track)
    {
        var palette = Unpack(track.DominantColors);
        if (palette is null)
        {
            return WidgetColorScheme.Default;
        }

        var background = palette[0];
        var accent = palette[1];
        var textIsDark = RelativeLuminance(background) > DarkTextLuminanceThreshold;
        return new WidgetColorScheme(background, accent, accent, textIsDark);
    }

    /// <summary>Decodes the 5x little-endian-uint32 0xAARRGGBB buffer packed by the backend's
    /// ColorExtractor (see ColorExtractor.h). Null for no cover / undecodable cover, matching how
    /// MainWindow already treats an empty CoverArt buffer.</summary>
    private static Color[]? Unpack(IBuffer dominantColors)
    {
        if (dominantColors is not { Length: > 0 } buffer)
        {
            return null;
        }

        var packed = MemoryMarshal.Cast<byte, uint>(buffer.ToArray());
        var colors = new Color[packed.Length];
        for (var i = 0; i < packed.Length; i++)
        {
            var argb = packed[i];
            colors[i] = Color.FromArgb((byte)(argb >> 24), (byte)(argb >> 16), (byte)(argb >> 8), (byte)argb);
        }
        return colors;
    }

    private static double RelativeLuminance(Color color) =>
        (0.2126 * color.R + 0.7152 * color.G + 0.0722 * color.B) / 255.0;
}
```

- [ ] **Step 2: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).` — this file has no callers yet, so a clean build is the only check possible at this step.

- [ ] **Step 3: Commit**

```bash
git add frontend/Wavely.App/Services/DynamicColorService.cs
git commit -m "feat: add DynamicColorService to resolve per-track widget color schemes"
```

---

### Task 2: WaveformControl accent color

**Files:**
- Modify: `frontend/Wavely.App/Controls/WaveformControl.cs`

**Interfaces:**
- Consumes: nothing new.
- Produces: `WaveformControl.AccentColor` (`Avalonia.Media.Color`, get/set) — Task 3 sets this from `MainWindow`.

- [ ] **Step 1: Replace the fixed brush with a settable accent color**

Replace the whole file with:

```csharp
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;

namespace Wavely.App.Controls;

/// <summary>
/// Draws the live waveform as an equalizer: bars grow symmetrically from the vertical center
/// (matching assets/presets_reference/EqualizerBars.svelte's look), each one reflecting a
/// log-spaced frequency band's *current* magnitude - not a left-to-right amplitude-over-time
/// strip, which reads as a scrolling timeline rather than a live EQ. Custom-drawn (RULES.md:
/// GPU-accelerated via Avalonia's Skia renderer) rather than one control per bar, since bars
/// redrawing at ~60fps as individual bound elements would allocate/layout far more than a single
/// Render call.
/// </summary>
public sealed class WaveformControl : Control
{
    private static readonly Color DefaultAccentColor = Color.FromArgb(220, 90, 170, 255);
    private const double MinBarScale = 0.12;
    private const double BarGap = 3.0;
    private const double BarCornerRadius = 2.0;

    private float[] _bands = [];
    private Color _accentColor = DefaultAccentColor;
    private IBrush _barBrush = new SolidColorBrush(DefaultAccentColor);

    /// <summary>Bar fill color. Defaults to the static blue accent; overridden by Phase 6's
    /// dynamic-color binding (<see cref="Wavely.App.Services.DynamicColorService"/>) when the
    /// user has that enabled.</summary>
    public Color AccentColor
    {
        get => _accentColor;
        set
        {
            _accentColor = value;
            _barBrush = new SolidColorBrush(value);
            InvalidateVisual();
        }
    }

    public void UpdateBands(ReadOnlySpan<float> bands)
    {
        if (_bands.Length != bands.Length)
        {
            _bands = new float[bands.Length];
        }
        bands.CopyTo(_bands);
        InvalidateVisual();
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);

        var bounds = Bounds;
        var barCount = _bands.Length;
        if (bounds.Width <= 0 || bounds.Height <= 0 || barCount == 0)
        {
            return;
        }

        var totalGap = BarGap * (barCount - 1);
        var barWidth = Math.Max(1.0, (bounds.Width - totalGap) / barCount);
        var centerY = bounds.Height / 2.0;

        for (var i = 0; i < barCount; i++)
        {
            var amplitude = Math.Clamp(_bands[i], 0f, 1f);
            var scale = Math.Max(MinBarScale, amplitude);
            var barHeight = scale * bounds.Height;
            var x = i * (barWidth + BarGap);
            var y = centerY - barHeight / 2.0;
            context.DrawRectangle(_barBrush, null, new Rect(x, y, barWidth, barHeight), BarCornerRadius, BarCornerRadius);
        }
    }
}
```

- [ ] **Step 2: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).`

- [ ] **Step 3: Run and confirm no regression**

Run `Wavely.App.exe` with a music app playing. Confirm the waveform still renders in its original blue — `AccentColor` defaults to the same color the old fixed brush used, so this step should be visually identical to before.

- [ ] **Step 4: Commit**

```bash
git add frontend/Wavely.App/Controls/WaveformControl.cs
git commit -m "feat: make WaveformControl bar color configurable via AccentColor"
```

---

### Task 3: Dynamic color binding (background, waveform accent, text) — ✅ implemented, plan amended below

**⚠️ Amendment (post-implementation, verified by the controller):** Step 1 as originally written
below asked to name the background `SolidColorBrush` with `x:Name="BackgroundBrush"` and access it
as a generated C# field. **This does not compile in this project** — this project's Avalonia XAML
codegen only emits backing fields for `Control`-derived named elements (`Border`, `TextBlock`,
`Image`, custom controls), not a `SolidColorBrush` nested inside a property element like
`<Border.Background>`. Verified directly (not just taken on the implementer's word): adding
`x:Name="TestNamedBrushProbe"` to the brush and referencing it from code produced
`CS0103: the name 'TestNamedBrushProbe' does not exist in the current context` on build; reverted
after confirming.

**What was actually implemented instead:** `MainWindow.axaml`'s background brush stays **unnamed**
(no diff from `main` at all). Both `ApplyAppearance()` and `ApplyDynamicColors()` access it via the
pre-existing cast pattern the original `ApplyAppearance` already used:
`if (BackgroundBorder.Background is SolidColorBrush backgroundBrush) { ... }` — `BackgroundBorder`
*is* a named `Border` (a `Control`), so it does get a generated field; the brush hanging off its
`.Background` property is reached through that field instead of its own name. Behavior is
identical; only the access path differs.

**This means:** any later task in this plan that assumed a `BackgroundBrush` named field exists
must instead use this same `BackgroundBorder.Background is SolidColorBrush` cast. Task 4's
original text (below) still shows the old `x:Name="BackgroundBrush"` assumption in its "before"
snippets — the amendment inside Task 4 corrects this.

**Files (as actually touched):**
- `frontend/Wavely.App/Views/MainWindow.axaml` — **unchanged** (no net diff; the naming attempt was reverted).
- Created: `frontend/Wavely.App/Views/MainWindow.Appearance.cs`
- Modified: `frontend/Wavely.App/Views/MainWindow.axaml.cs` — removed `ApplyAppearance` (moved), added `_currentTrack` field and call sites.

**Interfaces:**
- Consumes: `DynamicColorService.Resolve(TrackInfo)`, `WidgetColorScheme`, `WaveformControl.AccentColor` (Tasks 1-2).
- Produces: `MainWindow.ApplyDynamicColors(TrackInfo track)` — Task 5 extends this to also apply glow; `MainWindow._currentTrack` (`TrackInfo?`) — Task 4 reads this too. `BackgroundBorder.Background is SolidColorBrush` is the established access pattern for the background brush — there is no `BackgroundBrush` field.

- [ ] **Step 1 (original text, superseded by the amendment above — kept for history): Name the background brush**

In `MainWindow.axaml`, change:

```xml
        <Border.Background>
            <SolidColorBrush Color="#141418" Opacity="0.7" />
        </Border.Background>
```

to:

```xml
        <Border.Background>
            <SolidColorBrush x:Name="BackgroundBrush" Color="#141418" Opacity="0.7" />
        </Border.Background>
```

**This step was not applied — see the amendment above. `MainWindow.axaml` has no changes from this task.**

- [ ] **Step 2: Create the appearance partial-class file**

Create `frontend/Wavely.App/Views/MainWindow.Appearance.cs`:

```csharp
using Avalonia;
using Avalonia.Media;
using Wavely.App.Services;
using Wavely.Backend;

namespace Wavely.App.Views;

/// <summary>
/// Phase 6 rendering logic for <see cref="MainWindow"/> - dynamic colors, blurred background,
/// glow, and cover shape/rotation - split into its own partial-class file so the main
/// MainWindow.axaml.cs (window lifecycle, drag, click-through) doesn't grow past RULES.md's
/// ~200-line guidance for a single class.
/// </summary>
public partial class MainWindow
{
    private static readonly IBrush LightTitleForeground = Brushes.White;
    private static readonly IBrush DarkTitleForeground = Brushes.Black;
    private static readonly IBrush LightArtistForeground = new SolidColorBrush(Color.FromArgb(0xB4, 0xFF, 0xFF, 0xFF));
    private static readonly IBrush DarkArtistForeground = new SolidColorBrush(Color.FromArgb(0xB4, 0x00, 0x00, 0x00));
    private static readonly IBrush LightStatusForeground = new SolidColorBrush(Color.FromArgb(0x78, 0xFF, 0xFF, 0xFF));
    private static readonly IBrush DarkStatusForeground = new SolidColorBrush(Color.FromArgb(0x78, 0x00, 0x00, 0x00));

    /// <summary>Applies the appearance settings that don't depend on the cover's palette:
    /// background opacity (baked into the background brush's own alpha, not the whole window's
    /// Opacity, so text/icons stay fully readable) and the app-wide dark/light theme variant.</summary>
    private void ApplyAppearance()
    {
        if (BackgroundBorder.Background is SolidColorBrush backgroundBrush)
        {
            backgroundBrush.Opacity = _config.BackgroundOpacity;
        }

        if (Application.Current is { } app)
        {
            app.RequestedThemeVariant = _config.Theme == ThemeMode.Dark
                ? Avalonia.Styling.ThemeVariant.Dark
                : Avalonia.Styling.ThemeVariant.Light;
        }
    }

    /// <summary>Applies the track's dominant-color palette to the background, waveform accent,
    /// and text - each gated by its own AppConfig toggle, falling back to
    /// <see cref="WidgetColorScheme.Default"/> per-element when that toggle is off.</summary>
    private void ApplyDynamicColors(TrackInfo track)
    {
        var scheme = DynamicColorService.Resolve(track);

        if (BackgroundBorder.Background is SolidColorBrush backgroundBrush)
        {
            backgroundBrush.Color = _config.DynamicBackgroundEnabled ? scheme.Background : WidgetColorScheme.Default.Background;
        }

        Waveform.AccentColor = _config.DynamicColorsEnabled ? scheme.Accent : WidgetColorScheme.Default.Accent;

        var textIsDark = _config.DynamicColorsEnabled && scheme.TextIsDark;
        TitleText.Foreground = textIsDark ? DarkTitleForeground : LightTitleForeground;
        ArtistText.Foreground = textIsDark ? DarkArtistForeground : LightArtistForeground;
        StatusText.Foreground = textIsDark ? DarkStatusForeground : LightStatusForeground;
    }
}
```

(Partial classes share fields and named XAML elements across files, but each file needs its own `using` directives — the code block above already includes `using Wavely.Backend;` for `TrackInfo`.)

- [ ] **Step 3: Remove `ApplyAppearance` from `MainWindow.axaml.cs` and wire the new call sites**

In `MainWindow.axaml.cs`, delete the existing `ApplyAppearance` method (currently right after `RefreshFromConfig`):

```csharp
    /// <summary>Applies the appearance settings that already have a visual effect without the
    /// cover-color-extraction/blur/preset rendering work planned for later phases: background
    /// opacity (baked into the background brush's own alpha, not the whole window's Opacity, so
    /// text/icons stay fully readable) and the app-wide dark/light theme variant.</summary>
    private void ApplyAppearance()
    {
        if (BackgroundBorder.Background is Avalonia.Media.SolidColorBrush backgroundBrush)
        {
            backgroundBrush.Opacity = _config.BackgroundOpacity;
        }

        if (Application.Current is { } app)
        {
            app.RequestedThemeVariant = _config.Theme == ThemeMode.Dark
                ? Avalonia.Styling.ThemeVariant.Dark
                : Avalonia.Styling.ThemeVariant.Light;
        }
    }
```

(it now lives in `MainWindow.Appearance.cs` from Step 2 - `RefreshFromConfig`'s and `OnOpened`'s existing calls to `ApplyAppearance()` keep working unchanged since it's the same partial class.)

Add a field next to the other private fields (near `_hiddenByAutoHide`):

```csharp
    private bool _hiddenByAutoHide;
    private TrackInfo? _currentTrack;
```

In `RefreshFromConfig`, add a call after `ApplyAppearance();`:

```csharp
    public void RefreshFromConfig()
    {
        ApplyVisualScale(_config.Geometry.Scale);
        ApplyClickThrough(_config.ClickThroughEnabled);
        _hideTimer.Interval = TimeSpan.FromSeconds(Math.Clamp(_config.HideOnPauseDelaySeconds, 5, 30));
        ApplyAppearance();
        if (_currentTrack is not null)
        {
            ApplyDynamicColors(_currentTrack);
        }
    }
```

In `OnTrackChanged`, set `_currentTrack` and call `ApplyDynamicColors`:

```csharp
    private void OnTrackChanged(MediaSessionManager sender, TrackInfo track)
    {
        Dispatcher.UIThread.Post(() =>
        {
            _currentTrack = track;
            TitleText.Text = string.IsNullOrEmpty(track.Title) ? "No track playing" : track.Title;
            ArtistText.Text = track.Artist;

            var coverArt = track.CoverArt;
            if (coverArt is { Length: > 0 })
            {
                var bytes = coverArt.ToArray();
                using var stream = new MemoryStream(bytes);
                CoverImage.Source = new Bitmap(stream);
            }
            else
            {
                CoverImage.Source = null;
            }

            ApplyDynamicColors(track);
        });
    }
```

- [ ] **Step 4: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).`

- [ ] **Step 5: Run and verify**

Run `Wavely.App.exe` with a music app playing a track that has contrasting cover art. In Settings → Apparence, enable "Couleurs dynamiques" and "Fond dominante" (`DynamicColorsEnabled`/`DynamicBackgroundEnabled`). Confirm: the widget's background and waveform bars change to colors pulled from the current cover; switching tracks (different-colored cover) updates them again; toggling the settings back off reverts to the original dark background / blue waveform. Try a very light cover and confirm the title text switches to dark/black for legibility.

- [ ] **Step 6: Commit**

```bash
git add frontend/Wavely.App/Views/MainWindow.axaml frontend/Wavely.App/Views/MainWindow.axaml.cs frontend/Wavely.App/Views/MainWindow.Appearance.cs
git commit -m "feat: bind dynamic cover colors to widget background, waveform, and text (Phase 6.2)"
```

---

### Task 4: Blurred background

**Files:**
- Modify: `frontend/Wavely.App/Views/MainWindow.axaml` — layer a blurred cover image behind the content.
- Modify: `frontend/Wavely.App/Views/MainWindow.Appearance.cs` — add `ApplyBlurBackground`.
- Modify: `frontend/Wavely.App/Views/MainWindow.axaml.cs` — call it from `OnOpened`, `OnTrackChanged`, `RefreshFromConfig`.

**Interfaces:**
- Consumes: `CoverImage.Source` (already set in `OnTrackChanged`), `_config.CoverBlurEnabled`.
- Produces: `MainWindow.ApplyBlurBackground()`.

- [ ] **Step 1: Restructure the XAML to add a blurred background layer**

**Note (per Task 3's amendment above): the background brush is unnamed** (naming it doesn't compile in this project) — keep it unnamed here too; every task accesses it via `BackgroundBorder.Background is SolidColorBrush`, never a `BackgroundBrush` field.

In `MainWindow.axaml`, replace:

```xml
    <Border x:Name="BackgroundBorder" CornerRadius="16" Padding="16">
        <Border.Background>
            <SolidColorBrush Color="#141418" Opacity="0.7" />
        </Border.Background>
        <StackPanel Spacing="8">
```

with:

```xml
    <Border x:Name="BackgroundBorder" CornerRadius="16" ClipToBounds="True">
        <Grid>
            <Image x:Name="BlurBackgroundImage" Stretch="UniformToFill" IsVisible="False" />
            <Border Padding="16">
                <Border.Background>
                    <SolidColorBrush Color="#141418" Opacity="0.7" />
                </Border.Background>
                <StackPanel Spacing="8">
```

And replace the matching closing tags at the end of the file:

```xml
        </StackPanel>
    </Border>
</Window>
```

with:

```xml
                </StackPanel>
            </Border>
        </Grid>
    </Border>
</Window>
```

(The inner `StackPanel`'s content - the `Grid` with cover/title/artist/status and the `WaveformControl` - is unchanged; only its ancestor wrapping changes, from one `Border` to `Border > Grid > (Image, Border > StackPanel)`.)

- [ ] **Step 2: Add `ApplyBlurBackground` to `MainWindow.Appearance.cs`**

Add this constant near the top of the class and the method anywhere among the other `Apply*` methods:

```csharp
    private const double BackgroundBlurRadius = 24.0;
```

```csharp
    /// <summary>Shows a heavily blurred copy of the current cover art behind the widget's
    /// content when enabled - reuses the already-decoded CoverImage.Source bitmap rather than
    /// re-decoding the cover, since both images just need the same pixels at different
    /// treatments.</summary>
    private void ApplyBlurBackground()
    {
        BlurBackgroundImage.Source = CoverImage.Source;
        BlurBackgroundImage.IsVisible = _config.CoverBlurEnabled && CoverImage.Source is not null;
    }
```

- [ ] **Step 3: Wire it up in `MainWindow.axaml.cs`**

**⚠️ Amendment (found by Task 4's implementer, confirmed by a successful build):** there is no
`Avalonia.Media.Effects` namespace in this project's Avalonia 11.3.18 — `BlurEffect` lives
directly under `Avalonia.Media`.

In `OnOpened`, set the blur effect once and apply the initial state - add after `ApplyAppearance();`:

```csharp
        BlurBackgroundImage.Effect = new Avalonia.Media.BlurEffect { Radius = BackgroundBlurRadius };
        ApplyBlurBackground();
```

In `RefreshFromConfig`, add a call (the toggle can change without a new track):

```csharp
    public void RefreshFromConfig()
    {
        ApplyVisualScale(_config.Geometry.Scale);
        ApplyClickThrough(_config.ClickThroughEnabled);
        _hideTimer.Interval = TimeSpan.FromSeconds(Math.Clamp(_config.HideOnPauseDelaySeconds, 5, 30));
        ApplyAppearance();
        ApplyBlurBackground();
        if (_currentTrack is not null)
        {
            ApplyDynamicColors(_currentTrack);
        }
    }
```

In `OnTrackChanged`, add a call right before `ApplyDynamicColors(track);`:

```csharp
            ApplyBlurBackground();
            ApplyDynamicColors(track);
```

- [ ] **Step 4: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).`

- [ ] **Step 5: Run and verify**

Run `Wavely.App.exe`, enable "Fond flouté" (`CoverBlurEnabled`) in Settings → Apparence. Confirm a blurred version of the current cover fills the widget's background, with the existing dark tint still on top for text legibility. Toggle it off and confirm the background reverts to the flat color from Task 3. Change tracks and confirm the blurred background updates too.

- [ ] **Step 6: Commit**

```bash
git add frontend/Wavely.App/Views/MainWindow.axaml frontend/Wavely.App/Views/MainWindow.Appearance.cs frontend/Wavely.App/Views/MainWindow.axaml.cs
git commit -m "feat: add blurred cover background (Phase 6.3)"
```

---

### Task 5: Glow

**⚠️ Amendment (post-Task-4, verified by the controller before dispatch):** the original text
below assumed `Avalonia.Media.Effects.BlurEffect`/`DropShadowEffect` (an `Effects` sub-namespace,
and a WPF-style `OffsetX`/`OffsetY` drop shadow). Task 4 found — and the controller independently
re-verified via a throwaway build probe — that Avalonia 11.3.18 has **no `Avalonia.Media.Effects`
namespace at all**: `BlurEffect` lives directly under `Avalonia.Media`, and there is no plain
`DropShadowEffect` type either — only `Avalonia.Media.DropShadowDirectionEffect`, whose XML docs
say it's "compatible with WPF's DropShadowEffect" but exposes **`Direction` (degrees) and
`ShadowDepth` (double) instead of `OffsetX`/`OffsetY`**. Verified compiling with `Color`,
`BlurRadius`, `Direction`, `ShadowDepth`, `Opacity` all present. The code below uses the corrected
type/properties; a symmetric halo (no directional offset) is `Direction = 0.0, ShadowDepth = 0.0`.

**Files:**
- Modify: `frontend/Wavely.App/Views/MainWindow.Appearance.cs` — add `ApplyGlow`, call it from `ApplyDynamicColors`.

**Interfaces:**
- Consumes: `_config.CoverGlowEnabled`, `WidgetColorScheme.Glow`/`WidgetColorScheme.Default.Glow`.
- Produces: `MainWindow.ApplyGlow(Color glowColor)`.

- [ ] **Step 1: Add the glow constants and method**

Add near `BackgroundBlurRadius`:

```csharp
    private const double GlowBlurRadius = 18.0;
    private const double GlowOpacity = 0.9;
```

```csharp
    /// <summary>Applies (or clears) a colored halo around the cover. Color follows the dynamic
    /// palette when enabled, otherwise a neutral white glow - independent of whether dynamic
    /// colors are on, the glow's presence is controlled only by CoverGlowEnabled.</summary>
    private void ApplyGlow(Color glowColor)
    {
        if (!_config.CoverGlowEnabled)
        {
            CoverBorder.Effect = null;
            return;
        }

        CoverBorder.Effect = new DropShadowDirectionEffect
        {
            Color = glowColor,
            BlurRadius = GlowBlurRadius,
            Direction = 0.0,
            ShadowDepth = 0.0,
            Opacity = GlowOpacity,
        };
    }
```

(`DropShadowDirectionEffect` resolves via the file's existing `using Avalonia.Media;`.)

- [ ] **Step 2: Call it from `ApplyDynamicColors`**

In `ApplyDynamicColors`, add as the last line:

```csharp
        ApplyGlow(_config.DynamicColorsEnabled ? scheme.Glow : WidgetColorScheme.Default.Glow);
```

- [ ] **Step 3: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).`

- [ ] **Step 4: Run and verify**

Run `Wavely.App.exe`, enable "Glow" (`CoverGlowEnabled`) in Settings → Apparence with dynamic colors off - confirm a white halo appears around the cover. Enable dynamic colors too - confirm the glow recolors to match the cover's palette and changes with the track. Disable glow - confirm it disappears (`CoverBorder.Effect` back to null).

- [ ] **Step 5: Commit**

```bash
git add frontend/Wavely.App/Views/MainWindow.Appearance.cs
git commit -m "feat: add cover glow effect (Phase 6.4)"
```

---

### Task 6: Squircle and static cover shapes

**Files:**
- Create: `frontend/Wavely.App/Core/SquircleGeometry.cs`
- Modify: `frontend/Wavely.App/Views/MainWindow.axaml` — wrap the cover in a `Grid`, add a vinyl spindle overlay, remove the static `CornerRadius`/`ClipToBounds` from `CoverBorder` (now managed in code).
- Modify: `frontend/Wavely.App/Views/MainWindow.Appearance.cs` — add `ApplyCoverShape`.
- Modify: `frontend/Wavely.App/Views/MainWindow.axaml.cs` — call `ApplyCoverShape` from `ApplyVisualScale`.

**Interfaces:**
- Consumes: `_config.CoverShape` (`Wavely.Backend.CoverStyle`: `Square`/`Squircle`/`Vinyl`).
- Produces: `SquircleGeometry.ForSize(double size) : StreamGeometry`, `MainWindow.ApplyCoverShape()` (Task 7 extends this to also start/stop vinyl rotation).

At this point the Vinyl case only clips the cover into a circle and shows the spindle overlay - it doesn't spin yet (Task 7 adds that). This is a complete, independently valid state (a static vinyl-shaped cover), not a stub.

- [ ] **Step 1: Write the squircle geometry builder**

Create `frontend/Wavely.App/Core/SquircleGeometry.cs`:

```csharp
using Avalonia;
using Avalonia.Media;

namespace Wavely.App.Core;

/// <summary>
/// Builds superellipse ("squircle") clip geometries: |x/a|^n + |y/a|^n = 1, n=5 - noticeably more
/// square than an ellipse, much rounder than a rounded-rect corner. Avalonia has no built-in
/// rounded-rect Geometry type (only Border's own corner-radius clipping, which only produces
/// circular corners, not this curve), so this samples the parametric form of the curve into a
/// closed polygon rather than composing arcs.
/// </summary>
public static class SquircleGeometry
{
    private const double Exponent = 5.0;
    private const int SamplePoints = 64;

    private static readonly Dictionary<double, StreamGeometry> Cache = new();

    /// <summary>Returns a cached squircle geometry for a size x size square, in the owning
    /// Visual's local coordinate space (top-left origin). Cached by size so resizing the widget
    /// (50%-150% scale) doesn't rebuild the polygon every frame.</summary>
    public static StreamGeometry ForSize(double size)
    {
        if (Cache.TryGetValue(size, out var cached))
        {
            return cached;
        }

        var geometry = Build(size);
        Cache[size] = geometry;
        return geometry;
    }

    private static StreamGeometry Build(double size)
    {
        var radius = size / 2.0;
        var geometry = new StreamGeometry();
        using (var context = geometry.Open())
        {
            context.BeginFigure(PointOnCurve(0, radius), isFilled: true);
            for (var i = 1; i <= SamplePoints; i++)
            {
                var t = i * (2.0 * Math.PI / SamplePoints);
                context.LineTo(PointOnCurve(t, radius));
            }
            context.EndFigure(isClosed: true);
        }
        return geometry;

        Point PointOnCurve(double t, double a)
        {
            var cos = Math.Cos(t);
            var sin = Math.Sin(t);
            var x = Math.Sign(cos) * Math.Pow(Math.Abs(cos), 2.0 / Exponent) * a;
            var y = Math.Sign(sin) * Math.Pow(Math.Abs(sin), 2.0 / Exponent) * a;
            return new Point(a + x, a + y);
        }
    }
}
```

- [ ] **Step 2: Restructure the cover markup in `MainWindow.axaml`**

Replace:

```xml
                <Border x:Name="CoverBorder" Grid.Column="0" Width="88" Height="88" CornerRadius="8" ClipToBounds="True">
                    <Image x:Name="CoverImage" Stretch="UniformToFill" />
                </Border>
```

with:

```xml
                <Border x:Name="CoverBorder" Grid.Column="0" Width="88" Height="88">
                    <Grid>
                        <Image x:Name="CoverImage" Stretch="UniformToFill" RenderTransformOrigin="0.5,0.5">
                            <Image.RenderTransform>
                                <RotateTransform x:Name="CoverRotateTransform" Angle="0" />
                            </Image.RenderTransform>
                        </Image>
                        <Ellipse x:Name="VinylSpindle" Width="14" Height="14" Fill="#1A1A1A" IsVisible="False" />
                    </Grid>
                </Border>
```

- [ ] **Step 3: Add `ApplyCoverShape` to `MainWindow.Appearance.cs`**

Add `using Wavely.App.Core;` to the file's usings, then the constant and method:

```csharp
    private const double CoverCornerRadius = 8.0;
```

```csharp
    /// <summary>Switches the cover's clip shape between the three CoverStyle values. Square
    /// keeps using Border's own corner-radius clipping (already verified in earlier phases);
    /// Squircle and Vinyl switch to an explicit Clip geometry instead, since neither shape is
    /// expressible via CornerRadius.</summary>
    private void ApplyCoverShape()
    {
        var size = CoverBorder.Width;
        switch (_config.CoverShape)
        {
            case CoverStyle.Square:
                CoverBorder.ClipToBounds = true;
                CoverBorder.CornerRadius = new CornerRadius(CoverCornerRadius);
                CoverBorder.Clip = null;
                VinylSpindle.IsVisible = false;
                break;
            case CoverStyle.Squircle:
                CoverBorder.ClipToBounds = false;
                CoverBorder.CornerRadius = new CornerRadius(0);
                CoverBorder.Clip = SquircleGeometry.ForSize(size);
                VinylSpindle.IsVisible = false;
                break;
            case CoverStyle.Vinyl:
                CoverBorder.ClipToBounds = false;
                CoverBorder.CornerRadius = new CornerRadius(0);
                CoverBorder.Clip = new EllipseGeometry(new Rect(0, 0, size, size));
                VinylSpindle.IsVisible = true;
                break;
        }
    }
```

- [ ] **Step 4: Call it from `ApplyVisualScale` in `MainWindow.axaml.cs`**

`ApplyCoverShape` needs `CoverBorder.Width` to already reflect the current scale, and must re-run whenever scale changes (squircle/vinyl clip size depends on it) - so it belongs at the end of `ApplyVisualScale`, not as a separate call site:

```csharp
    private void ApplyVisualScale(double scale)
    {
        Width = DefaultWidth * scale;
        Height = DefaultHeight * scale;
        CoverBorder.Width = CoverSize * scale;
        CoverBorder.Height = CoverSize * scale;
        Waveform.Height = WaveformHeight * scale;
        ApplyCoverShape();
    }
```

- [ ] **Step 5: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).`

- [ ] **Step 6: Run and verify**

Run `Wavely.App.exe`. In Settings → Apparence, cycle "Style pochette" through Carré/Squircle/Vinyle (`CoverShape`). Confirm: Carré looks identical to before (rounded-rect, unchanged); Squircle shows a distinctly superellipse-shaped cover (rounder than the square, not a circle); Vinyle shows a circular cover with a small dark spindle dot centered on top (not yet spinning). Resize the widget (mouse wheel) in each shape and confirm the clip scales correctly with no stretching/clipping artifacts.

- [ ] **Step 7: Commit**

```bash
git add frontend/Wavely.App/Core/SquircleGeometry.cs frontend/Wavely.App/Views/MainWindow.axaml frontend/Wavely.App/Views/MainWindow.Appearance.cs frontend/Wavely.App/Views/MainWindow.axaml.cs
git commit -m "feat: add squircle and vinyl cover shapes (Phase 6.5)"
```

---

### Task 7: Vinyl rotation

**Files:**
- Modify: `frontend/Wavely.App/Views/MainWindow.Appearance.cs` — rotation timer + `UpdateVinylRotationState`.
- Modify: `frontend/Wavely.App/Views/MainWindow.axaml.cs` — `_isPlaying` field, timer field init, wire into `OnPlaybackStateChanged`.

**Interfaces:**
- Consumes: `CoverRotateTransform` (named element from Task 6), `_config.CoverShape`.
- Produces: nothing further consumed by later tasks (this is the last task in the plan).

- [ ] **Step 1: Add the rotation timer and state method to `MainWindow.Appearance.cs`**

Add the constant:

```csharp
    private const double VinylRotationDegreesPerSecond = 90.0; // 360 degrees every 4 seconds.
```

Add the method (place near `ApplyCoverShape`):

```csharp
    /// <summary>Starts or stops the vinyl spin timer to match whether the cover is currently a
    /// spinning vinyl (shape == Vinyl AND playing). Stopping the timer leaves
    /// CoverRotateTransform.Angle wherever it was - resuming continues from that angle rather
    /// than snapping back to 0, matching how a real turntable behaves.</summary>
    private void UpdateVinylRotationState()
    {
        var shouldRotate = _config.CoverShape == CoverStyle.Vinyl && _isPlaying;
        if (shouldRotate && !_vinylRotationTimer.IsEnabled)
        {
            _vinylRotationTimer.Start();
        }
        else if (!shouldRotate && _vinylRotationTimer.IsEnabled)
        {
            _vinylRotationTimer.Stop();
        }
    }
```

Call it at the end of `ApplyCoverShape` (added in Task 6), right after the `switch`:

```csharp
    private void ApplyCoverShape()
    {
        var size = CoverBorder.Width;
        switch (_config.CoverShape)
        {
            // ... unchanged cases from Task 6 ...
        }
        UpdateVinylRotationState();
    }
```

- [ ] **Step 2: Add the timer field and `_isPlaying` field in `MainWindow.axaml.cs`, initialize the timer, wire playback state**

Add fields next to `_hideAfterFadeTimer`:

```csharp
    private readonly DispatcherTimer _hideAfterFadeTimer;
    private readonly DispatcherTimer _vinylRotationTimer;
```

and next to `_currentTrack` (added in Task 3):

```csharp
    private TrackInfo? _currentTrack;
    private bool _isPlaying;
```

In the constructor, initialize the timer next to `_hideAfterFadeTimer`'s setup:

```csharp
        _vinylRotationTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(16) };
        _vinylRotationTimer.Tick += (_, _) =>
            CoverRotateTransform.Angle = (CoverRotateTransform.Angle
                + VinylRotationDegreesPerSecond * _vinylRotationTimer.Interval.TotalSeconds) % 360.0;
```

In `OnPlaybackStateChanged`, record `_isPlaying` and update rotation state - add at the very top of the `Dispatcher.UIThread.Post` lambda body:

```csharp
    private void OnPlaybackStateChanged(MediaSessionManager sender, bool isPlaying)
    {
        Dispatcher.UIThread.Post(() =>
        {
            _isPlaying = isPlaying;
            UpdateVinylRotationState();

            StatusText.Text = isPlaying ? "Playing" : "Paused";
            // ... rest of the existing method body is unchanged ...
```

- [ ] **Step 3: Build**

Run: `.\build.ps1 -Configuration Debug`
Expected: `Build complete (Debug).`

- [ ] **Step 4: Run and verify**

Run `Wavely.App.exe` with `CoverShape` set to Vinyle. Play music - confirm the cover spins continuously (~4s per rotation). Pause - confirm it stops immediately, holding its current angle (not snapping to 0°). Resume - confirm it continues spinning from that same angle. Switch to Squircle/Carré while playing - confirm rotation stops and doesn't resume even though playback continues (only Vinyl spins).

- [ ] **Step 5: Commit**

```bash
git add frontend/Wavely.App/Views/MainWindow.Appearance.cs frontend/Wavely.App/Views/MainWindow.axaml.cs
git commit -m "feat: spin the vinyl cover during playback, pausing in place (Phase 6.6)"
```

---

## Plan Self-Review

**Spec coverage:** 6.2 (background/waveform/text binding) → Task 3. 6.3 (blur) → Task 4. 6.4 (glow) → Task 5. 6.5 (squircle) → Task 6. 6.6 (vinyl rotation) → Task 7. Progress bar and SMTC position tracking are explicitly out of scope per the design doc. All `AppConfig` toggles named in the design (`DynamicColorsEnabled`, `DynamicBackgroundEnabled`, `CoverBlurEnabled`, `CoverGlowEnabled`, `CoverShape`) are consumed by exactly one task each.

**Type/name consistency check:** `WidgetColorScheme` (Task 1) fields `Background`/`Accent`/`Glow`/`TextIsDark` match every later usage in Tasks 3 and 5. `WaveformControl.AccentColor` (Task 2) matches its call site in Task 3. `SquircleGeometry.ForSize` (Task 6) matches its only call site in the same task. `ApplyCoverShape`/`UpdateVinylRotationState`/`ApplyGlow`/`ApplyBlurBackground`/`ApplyDynamicColors` names are consistent between the task that defines them and every task that calls or extends them.
