#include "pch.h"
#include "ColorExtractor.h"

#include <wincodec.h>

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace Wavely::Backend::Core
{
    namespace
    {
        using winrt::Windows::Storage::Streams::IBuffer;

        constexpr UINT kThumbnailSize = 50;
        // Per-channel bucket width for the population histogram: 4 bits/channel (16 levels) keeps
        // enough distinct buckets to separate genuinely different colors while still merging
        // near-identical shades into one bucket, which matters for a ~2500-pixel (50x50) sample -
        // finer buckets (e.g. 5 bits) would leave most buckets with only one or two pixels each,
        // making the population ranking below meaningless noise.
        constexpr int kBucketBits = 4;
        constexpr int kBucketShift = 8 - kBucketBits;

        struct ColorBucket
        {
            std::uint64_t count = 0;
            std::uint64_t sumR = 0;
            std::uint64_t sumG = 0;
            std::uint64_t sumB = 0;
        };

        std::uint16_t bucketKey(std::uint8_t r, std::uint8_t g, std::uint8_t b)
        {
            return static_cast<std::uint16_t>(
                ((r >> kBucketShift) << (2 * kBucketBits)) | ((g >> kBucketShift) << kBucketBits) | (b >> kBucketShift));
        }

        /// HSV saturation in [0,1] from 8-bit RGB (0 for black/gray).
        float saturationOf(std::uint8_t r, std::uint8_t g, std::uint8_t b)
        {
            const auto maxC = std::max({ r, g, b });
            const auto minC = std::min({ r, g, b });
            return maxC == 0 ? 0.0f : static_cast<float>(maxC - minC) / static_cast<float>(maxC);
        }

        winrt::com_ptr<IWICImagingFactory> createWicFactory()
        {
            winrt::com_ptr<IWICImagingFactory> factory;
            winrt::check_hresult(CoCreateInstance(
                CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory), factory.put_void()));
            return factory;
        }

        /// Decodes `encodedImage` (whatever format GSMTC's thumbnail stream provides - JPEG/PNG/
        /// BMP are all common) and returns it scaled to kThumbnailSize x kThumbnailSize as a
        /// top-left-origin, row-major 32bppBGRA pixel buffer. Empty on any decode failure -
        /// malformed/unsupported thumbnail data is an expected possibility, not a bug to surface.
        std::vector<std::uint8_t> decodeAndScale(IBuffer const& encodedImage)
        {
            try
            {
                const auto factory = createWicFactory();

                winrt::com_ptr<IWICStream> stream;
                winrt::check_hresult(factory->CreateStream(stream.put()));
                winrt::check_hresult(stream->InitializeFromMemory(encodedImage.data(), encodedImage.Length()));

                winrt::com_ptr<IWICBitmapDecoder> decoder;
                winrt::check_hresult(factory->CreateDecoderFromStream(
                    stream.get(), nullptr, WICDecodeMetadataCacheOnDemand, decoder.put()));

                winrt::com_ptr<IWICBitmapFrameDecode> frame;
                winrt::check_hresult(decoder->GetFrame(0, frame.put()));

                winrt::com_ptr<IWICFormatConverter> converter;
                winrt::check_hresult(factory->CreateFormatConverter(converter.put()));
                winrt::check_hresult(converter->Initialize(
                    frame.get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom));

                winrt::com_ptr<IWICBitmapScaler> scaler;
                winrt::check_hresult(factory->CreateBitmapScaler(scaler.put()));
                winrt::check_hresult(
                    scaler->Initialize(converter.get(), kThumbnailSize, kThumbnailSize, WICBitmapInterpolationModeFant));

                constexpr UINT stride = kThumbnailSize * 4;
                std::vector<std::uint8_t> pixels(static_cast<std::size_t>(stride) * kThumbnailSize);
                winrt::check_hresult(scaler->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
                return pixels;
            }
            catch (winrt::hresult_error const&)
            {
                return {};
            }
        }

        std::unordered_map<std::uint16_t, ColorBucket> buildHistogram(std::vector<std::uint8_t> const& pixels)
        {
            std::unordered_map<std::uint16_t, ColorBucket> histogram;
            for (std::size_t i = 0; i + 3 < pixels.size(); i += 4)
            {
                const std::uint8_t b = pixels[i + 0];
                const std::uint8_t g = pixels[i + 1];
                const std::uint8_t r = pixels[i + 2];
                const std::uint8_t alpha = pixels[i + 3];
                if (alpha < 16)
                {
                    continue; // Mostly-transparent pixel: not a meaningful "displayed" color.
                }
                auto& bucket = histogram[bucketKey(r, g, b)];
                ++bucket.count;
                bucket.sumR += r;
                bucket.sumG += g;
                bucket.sumB += b;
            }
            return histogram;
        }

        struct RankedColor
        {
            std::uint32_t argb;
            float score;
        };

        /// Ranks buckets by population weighted toward saturated colors (see ColorExtractor.h)
        /// rather than raw population alone, so a cover with a large black/white background
        /// doesn't crowd out its actual accent colors.
        std::vector<RankedColor> rankBuckets(std::unordered_map<std::uint16_t, ColorBucket> const& histogram)
        {
            std::vector<RankedColor> ranked;
            ranked.reserve(histogram.size());
            for (auto const& [key, bucket] : histogram)
            {
                const auto r = static_cast<std::uint8_t>(bucket.sumR / bucket.count);
                const auto g = static_cast<std::uint8_t>(bucket.sumG / bucket.count);
                const auto b = static_cast<std::uint8_t>(bucket.sumB / bucket.count);
                const float score = static_cast<float>(bucket.count) * (0.15f + 0.85f * saturationOf(r, g, b));
                const std::uint32_t argb = 0xFF000000u | (static_cast<std::uint32_t>(r) << 16) |
                    (static_cast<std::uint32_t>(g) << 8) | static_cast<std::uint32_t>(b);
                ranked.push_back({ argb, score });
            }
            std::sort(ranked.begin(), ranked.end(), [](RankedColor const& a, RankedColor const& b) { return a.score > b.score; });
            return ranked;
        }
    }

    std::optional<ColorPalette> ExtractDominantColors(IBuffer const& encodedImage)
    {
        if (!encodedImage || encodedImage.Length() == 0)
        {
            return std::nullopt;
        }

        const auto pixels = decodeAndScale(encodedImage);
        if (pixels.empty())
        {
            return std::nullopt;
        }

        const auto histogram = buildHistogram(pixels);
        if (histogram.empty())
        {
            return std::nullopt;
        }

        const auto ranked = rankBuckets(histogram);

        ColorPalette palette{};
        for (std::size_t i = 0; i < kColorPaletteSize; ++i)
        {
            palette[i] = ranked[std::min(i, ranked.size() - 1)].argb;
        }
        return palette;
    }
}
