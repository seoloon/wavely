using Avalonia;

namespace Wavely.App.Controls;

public sealed record PresetEntry(string Name, Size WindowSize, Func<IPresetView> Factory);

/// <summary>
/// The presets in AppConfig.PresetIndex order (matches SettingsViewModel.PresetNames and
/// assets/presets_reference/layouts.ts exactly). MainWindow indexes into this to know both which
/// view to host and what base window size to resize to before the user's 50%-150% scale is
/// applied on top. Grows by one entry per Phase 7 task (Tasks 7-12) until all 7 are present.
/// </summary>
public static class PresetCatalog
{
    public static IReadOnlyList<PresetEntry> Entries { get; } =
    [
        new("Compact", new Size(360, 110), () => new Views.Presets.CompactPresetView()),
        new("Boxy", new Size(340, 170), () => new Views.Presets.BoxyPresetView()),
        new("Gallery", new Size(240, 350), () => new Views.Presets.GalleryPresetView()),
        new("Minimal", new Size(300, 54), () => new Views.Presets.MinimalPresetView()),
        new("macOS", new Size(340, 122), () => new Views.Presets.MacosPresetView()),
    ];

    public static PresetEntry Resolve(int index) =>
        index >= 0 && index < Entries.Count ? Entries[index] : Entries[0];
}
