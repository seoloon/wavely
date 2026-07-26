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
        std::int64_t DurationMs();
        bool IsPlaying();

        void SetTitle(hstring const& value) { m_title = value; }
        void SetArtist(hstring const& value) { m_artist = value; }
        void SetAlbum(hstring const& value) { m_album = value; }
        void SetCoverArt(winrt::Windows::Storage::Streams::IBuffer const& value) { m_coverArt = value; }
        void SetDurationMs(std::int64_t value) { m_durationMs = value; }
        void SetIsPlaying(bool value) { m_isPlaying = value; }

    private:
        hstring m_title;
        hstring m_artist;
        hstring m_album;
        winrt::Windows::Storage::Streams::IBuffer m_coverArt{ nullptr };
        std::int64_t m_durationMs = 0;
        bool m_isPlaying = false;
    };
}
namespace winrt::Wavely::Backend::factory_implementation
{
    struct TrackInfo : TrackInfoT<TrackInfo, implementation::TrackInfo>
    {
    };
}
