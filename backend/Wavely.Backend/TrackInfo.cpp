#include "pch.h"
#include "TrackInfo.h"
#include "TrackInfo.g.cpp"

namespace winrt::Wavely::Backend::implementation
{
    hstring TrackInfo::Title()
    {
        return m_title;
    }

    hstring TrackInfo::Artist()
    {
        return m_artist;
    }

    hstring TrackInfo::Album()
    {
        return m_album;
    }

    winrt::Windows::Storage::Streams::IBuffer TrackInfo::CoverArt()
    {
        return m_coverArt;
    }

    std::int64_t TrackInfo::DurationMs()
    {
        return m_durationMs;
    }

    bool TrackInfo::IsPlaying()
    {
        return m_isPlaying;
    }
}
