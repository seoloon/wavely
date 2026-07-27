#pragma once
#include "MediaSessionManager.g.h"
#include "TrackInfo.h"

#include <atomic>

namespace winrt::Wavely::Backend::implementation
{
    /// Observes GSMTC sessions and republishes playback state, metadata and cover art as WinRT
    /// events consumed by the frontend. Callbacks guard against the object having been Stop()'d
    /// mid-flight (RULES.md SS4: the frontend can disconnect at any time).
    ///
    /// Only reacts to whitelisted native music-streaming apps (see Core/MusicAppAllowlist.h) -
    /// GSMTC's own GetCurrentSession() picks whichever session most recently had activity, which
    /// in practice means any browser tab playing a video routinely wins over an actually-playing
    /// music app (observed directly: a Brave tab pre-empted a concurrently playing Spotify
    /// track). GetSessions() + explicit filtering is used instead.
    struct MediaSessionManager : MediaSessionManagerT<MediaSessionManager>
    {
        MediaSessionManager() = default;

        void Start();
        void Stop();
        void Refresh();
        winrt::Wavely::Backend::TrackInfo CurrentTrack();

        winrt::event_token TrackChanged(winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::MediaSessionManager, winrt::Wavely::Backend::TrackInfo> const& handler);
        void TrackChanged(winrt::event_token const& token) noexcept;
        winrt::event_token PlaybackStateChanged(winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::MediaSessionManager, bool> const& handler);
        void PlaybackStateChanged(winrt::event_token const& token) noexcept;
        winrt::event_token CoverArtReceived(winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::MediaSessionManager, winrt::Windows::Storage::Streams::IBuffer> const& handler);
        void CoverArtReceived(winrt::event_token const& token) noexcept;
        winrt::event_token PositionChanged(winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::MediaSessionManager, std::int64_t> const& handler);
        void PositionChanged(winrt::event_token const& token) noexcept;

    private:
        winrt::fire_and_forget initializeAsync();
        void onSessionsChanged();
        winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession selectWhitelistedSession();
        void subscribeToCurrentSession();
        void unsubscribeFromCurrentSession();
        void refreshPlaybackInfo();
        winrt::fire_and_forget refreshMediaPropertiesAsync();
        void refreshTimelineProperties();

        winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager m_sessionManager{ nullptr };
        winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession m_currentSession{ nullptr };
        winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager::SessionsChanged_revoker m_sessionsChangedRevoker;
        winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession::MediaPropertiesChanged_revoker m_mediaPropertiesChangedRevoker;
        winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession::PlaybackInfoChanged_revoker m_playbackInfoChangedRevoker;
        winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession::TimelinePropertiesChanged_revoker m_timelinePropertiesChangedRevoker;

        winrt::Wavely::Backend::TrackInfo m_currentTrack{ winrt::make<TrackInfo>() };
        std::atomic<bool> m_started{ false };
        std::atomic<bool> m_stopped{ false };

        winrt::event<winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::MediaSessionManager, winrt::Wavely::Backend::TrackInfo>> m_trackChangedEvent;
        winrt::event<winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::MediaSessionManager, bool>> m_playbackStateChangedEvent;
        winrt::event<winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::MediaSessionManager, winrt::Windows::Storage::Streams::IBuffer>> m_coverArtReceivedEvent;
        winrt::event<winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::MediaSessionManager, std::int64_t>> m_positionChangedEvent;
    };
}
namespace winrt::Wavely::Backend::factory_implementation
{
    struct MediaSessionManager : MediaSessionManagerT<MediaSessionManager, implementation::MediaSessionManager>
    {
    };
}
