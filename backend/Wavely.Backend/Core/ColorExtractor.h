#pragma once

#include <winrt/Windows.Storage.Streams.h>

#include <array>
#include <cstdint>
#include <optional>

namespace Wavely::Backend::Core
{
    inline constexpr std::size_t kColorPaletteSize = 5;

    /// A fixed-size dominant-color palette, packed 0xAARRGGBB (alpha always 0xFF - these are
    /// opaque UI-tint swatches, not source pixels with real transparency).
    using ColorPalette = std::array<std::uint32_t, kColorPaletteSize>;

    /// Extracts `kColorPaletteSize` dominant colors from encoded cover art (JPEG/PNG/whatever
    /// GSMTC's thumbnail stream provides). Returns std::nullopt for empty/undecodable input
    /// (no cover art is an expected, not exceptional, condition - RULES.md SS4).
    ///
    /// Decoding uses WIC (Windows Imaging Component, `<wincodec.h>`) - the native Windows image
    /// decoding API - rather than a third-party image library, per RULES.md SS1's preference for
    /// native platform APIs before reaching for a dependency.
    ///
    /// Quantization is a population histogram (colors bucketed into a coarse RGB grid, output
    /// colors are each bucket's true average - not the bucket's quantized center, which keeps the
    /// result faithful to the source rather than snapped to a fixed grid), not the Octree/K-Means
    /// alternatives PLAN.md names: for a 50x50-scaled thumbnail this is simpler to get correct
    /// (no tree-reduction pass, no random seeding/convergence like K-Means) while producing
    /// visually equivalent "a handful of representative swatches" results, which is all this
    /// palette is used for (background tint, glow, waveform accent - not archival-quality palette
    /// extraction). Buckets are ranked by population weighted toward saturated colors, so a cover
    /// with a large black or white background/letterboxing doesn't crowd out its actual accent
    /// colors - the direct product goal (RULES.md Phase 6: "le widget est visuellement
    /// magnifique") would otherwise fail on a very common case.
    std::optional<ColorPalette> ExtractDominantColors(winrt::Windows::Storage::Streams::IBuffer const& encodedImage);
}
