#pragma once
#include "TrackInfo.g.h"

namespace winrt::Wavely::Backend::implementation
{
    /// Immutable-to-the-frontend snapshot of what the active GSMTC session is currently playing.
    /// The WinRT surface (TrackInfo.idl) only exposes getters; MediaSessionManager populates a
    /// freshly constructed instance through the Set* methods below, which are plain C++ (not
    /// part of the WinRT ABI, since they are not declared in TrackInfo.idl).
    struct TrackInfo : TrackInfoT<TrackInfo>
    {
        TrackInfo() = default;

        hstring Title();
        hstring Artist();
        hstring Album();
        winrt::Windows::Storage::Streams::IBuffer CoverArt();
        /// 5 dominant colors extracted from CoverArt, packed as 5 little-endian 0xAARRGGBB
        /// uint32s (same zero-copy IBuffer convention as WaveformEngine's band buffer). Null if
        /// there is no cover art or it couldn't be decoded - see Core::ExtractDominantColors.
        winrt::Windows::Storage::Streams::IBuffer DominantColors();
        std::int64_t DurationMs();
        std::int64_t PositionMs();
        bool IsPlaying();

        void SetTitle(hstring const& value) { m_title = value; }
        void SetArtist(hstring const& value) { m_artist = value; }
        void SetAlbum(hstring const& value) { m_album = value; }
        void SetCoverArt(winrt::Windows::Storage::Streams::IBuffer const& value) { m_coverArt = value; }
        void SetDominantColors(winrt::Windows::Storage::Streams::IBuffer const& value) { m_dominantColors = value; }
        void SetDurationMs(std::int64_t value) { m_durationMs = value; }
        void SetPositionMs(std::int64_t value) { m_positionMs = value; }
        void SetIsPlaying(bool value) { m_isPlaying = value; }

    private:
        hstring m_title;
        hstring m_artist;
        hstring m_album;
        winrt::Windows::Storage::Streams::IBuffer m_coverArt{ nullptr };
        winrt::Windows::Storage::Streams::IBuffer m_dominantColors{ nullptr };
        std::int64_t m_durationMs = 0;
        std::int64_t m_positionMs = 0;
        bool m_isPlaying = false;
    };
}
namespace winrt::Wavely::Backend::factory_implementation
{
    struct TrackInfo : TrackInfoT<TrackInfo, implementation::TrackInfo>
    {
    };
}
