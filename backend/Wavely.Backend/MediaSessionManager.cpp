#include "pch.h"
#include "MediaSessionManager.h"
#include "MediaSessionManager.g.cpp"

#include "Core/MusicAppAllowlist.h"

#include <chrono>
#include <sstream>

namespace winrt::Wavely::Backend::implementation
{
    namespace
    {
        using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager;
        using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionMediaProperties;
        using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus;
        using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionTimelineProperties;
        using winrt::Windows::Storage::Streams::Buffer;
        using winrt::Windows::Storage::Streams::IBuffer;
        using winrt::Windows::Storage::Streams::InputStreamOptions;
        using winrt::Windows::Storage::Streams::IRandomAccessStreamReference;

        void logFailure(const wchar_t* context, winrt::hresult_error const& ex)
        {
            std::wstringstream message;
            message << L"MediaSessionManager: " << context << L" (0x" << std::hex
                    << static_cast<unsigned>(ex.code().value) << L")\n";
            OutputDebugStringW(message.str().c_str());
        }


        void fillBasicMetadata(
            TrackInfo& track,
            GlobalSystemMediaTransportControlsSessionMediaProperties const& properties,
            GlobalSystemMediaTransportControlsSessionTimelineProperties const& timeline)
        {
            track.SetTitle(properties.Title());
            track.SetArtist(properties.Artist());
            track.SetAlbum(properties.AlbumTitle());
            const auto duration = timeline.EndTime() - timeline.StartTime();
            track.SetDurationMs(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
        }

        winrt::Windows::Foundation::IAsyncOperation<IBuffer> loadThumbnailBuffer(IRandomAccessStreamReference const& thumbnail)
        {
            const auto stream = co_await thumbnail.OpenReadAsync();
            const auto size = static_cast<uint32_t>(stream.Size());
            if (size == 0)
            {
                co_return nullptr;
            }
            Buffer buffer(size);
            co_return co_await stream.ReadAsync(buffer, size, InputStreamOptions::None);
        }
    }

    void MediaSessionManager::Start()
    {
        if (m_started.exchange(true))
        {
            return;
        }
        initializeAsync();
    }

    void MediaSessionManager::Stop()
    {
        m_stopped.store(true);
        unsubscribeFromCurrentSession();
        m_sessionsChangedRevoker = {};
        m_sessionManager = nullptr;
    }

    void MediaSessionManager::Refresh()
    {
        onSessionsChanged();
    }

    winrt::Wavely::Backend::TrackInfo MediaSessionManager::CurrentTrack()
    {
        return m_currentTrack;
    }

    winrt::fire_and_forget MediaSessionManager::initializeAsync()
    {
        // Keeps this object alive across the co_await below: unlike the lambdas registered with
        // auto_revoke (safe to capture `this` directly, since the revoker unsubscribes before
        // destruction), this coroutine genuinely suspends and must not resume into a freed object.
        auto strongThis = get_strong();
        try
        {
            auto manager = co_await GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
            if (m_stopped.load())
            {
                co_return;
            }
            m_sessionManager = manager;
            m_sessionsChangedRevoker = m_sessionManager.SessionsChanged(
                winrt::auto_revoke, [this](auto&&, auto&&) { onSessionsChanged(); });
            onSessionsChanged();
        }
        catch (winrt::hresult_error const& ex)
        {
            logFailure(L"failed to acquire the GSMTC session manager", ex);
        }
    }

    void MediaSessionManager::onSessionsChanged()
    {
        if (m_stopped.load() || !m_sessionManager)
        {
            return;
        }
        unsubscribeFromCurrentSession();
        m_currentSession = selectWhitelistedSession();
        if (!m_currentSession)
        {
            m_currentTrack = winrt::make<TrackInfo>();
            m_trackChangedEvent(*this, m_currentTrack);
            m_playbackStateChangedEvent(*this, false);
            return;
        }
        subscribeToCurrentSession();
        refreshPlaybackInfo();
        refreshMediaPropertiesAsync();
    }

    /// Picks the session to react to among every GSMTC-tracked app, restricted to
    /// Core::IsWhitelistedAumid matches (see that header for why). Prefers one that is actually
    /// Playing over one that's merely Paused/Stopped, so e.g. a paused Spotify session doesn't
    /// keep winning over a Deezer session someone just started playing.
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession MediaSessionManager::selectWhitelistedSession()
    {
        if (!m_sessionManager)
        {
            return nullptr;
        }

        winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession firstMatch{ nullptr };
        for (auto const& session : m_sessionManager.GetSessions())
        {
            if (!::Wavely::Backend::Core::IsWhitelistedAumid(session.SourceAppUserModelId()))
            {
                continue;
            }
            if (!firstMatch)
            {
                firstMatch = session;
            }
            try
            {
                if (session.GetPlaybackInfo().PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing)
                {
                    return session;
                }
            }
            catch (winrt::hresult_error const&)
            {
                // The session ended between GetSessions() and this read; keep looking.
            }
        }
        return firstMatch;
    }

    void MediaSessionManager::subscribeToCurrentSession()
    {
        m_mediaPropertiesChangedRevoker = m_currentSession.MediaPropertiesChanged(
            winrt::auto_revoke, [this](auto&&, auto&&) { refreshMediaPropertiesAsync(); });
        m_playbackInfoChangedRevoker = m_currentSession.PlaybackInfoChanged(
            winrt::auto_revoke, [this](auto&&, auto&&) { refreshPlaybackInfo(); });
    }

    void MediaSessionManager::unsubscribeFromCurrentSession()
    {
        m_mediaPropertiesChangedRevoker = {};
        m_playbackInfoChangedRevoker = {};
        m_currentSession = nullptr;
    }

    void MediaSessionManager::refreshPlaybackInfo()
    {
        if (m_stopped.load() || !m_currentSession)
        {
            return;
        }
        try
        {
            const auto info = m_currentSession.GetPlaybackInfo();
            const bool isPlaying = info.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
            winrt::get_self<TrackInfo>(m_currentTrack)->SetIsPlaying(isPlaying);
            m_playbackStateChangedEvent(*this, isPlaying);
        }
        catch (winrt::hresult_error const&)
        {
            // The session ended between the event firing and this read; the next
            // SessionsChanged/PlaybackInfoChanged event will settle the state.
        }
    }

    winrt::fire_and_forget MediaSessionManager::refreshMediaPropertiesAsync()
    {
        auto strongThis = get_strong();
        auto session = m_currentSession;
        if (!session)
        {
            co_return;
        }

        auto track = winrt::make<TrackInfo>();
        auto& trackImpl = *winrt::get_self<TrackInfo>(track);
        try
        {
            const auto properties = co_await session.TryGetMediaPropertiesAsync();
            if (m_stopped.load())
            {
                co_return;
            }
            fillBasicMetadata(trackImpl, properties, session.GetTimelineProperties());

            if (const auto thumbnail = properties.Thumbnail())
            {
                if (const auto buffer = co_await loadThumbnailBuffer(thumbnail))
                {
                    trackImpl.SetCoverArt(buffer);
                }
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            logFailure(L"failed to read media properties", ex);
            co_return;
        }

        if (m_stopped.load())
        {
            co_return;
        }
        trackImpl.SetIsPlaying(m_currentTrack.IsPlaying());
        m_currentTrack = track;
        m_trackChangedEvent(*this, track);
        if (track.CoverArt())
        {
            m_coverArtReceivedEvent(*this, track.CoverArt());
        }
    }

    winrt::event_token MediaSessionManager::TrackChanged(
        winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::MediaSessionManager, winrt::Wavely::Backend::TrackInfo> const& handler)
    {
        return m_trackChangedEvent.add(handler);
    }

    void MediaSessionManager::TrackChanged(winrt::event_token const& token) noexcept
    {
        m_trackChangedEvent.remove(token);
    }

    winrt::event_token MediaSessionManager::PlaybackStateChanged(
        winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::MediaSessionManager, bool> const& handler)
    {
        return m_playbackStateChangedEvent.add(handler);
    }

    void MediaSessionManager::PlaybackStateChanged(winrt::event_token const& token) noexcept
    {
        m_playbackStateChangedEvent.remove(token);
    }

    winrt::event_token MediaSessionManager::CoverArtReceived(
        winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::MediaSessionManager, winrt::Windows::Storage::Streams::IBuffer> const& handler)
    {
        return m_coverArtReceivedEvent.add(handler);
    }

    void MediaSessionManager::CoverArtReceived(winrt::event_token const& token) noexcept
    {
        m_coverArtReceivedEvent.remove(token);
    }
}
