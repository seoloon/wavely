#include "core/MediaSessionManager.hpp"

#include <QDebug>

#include <atomic>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

namespace wavely::core {

namespace {

using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;
using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager;
using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus;
using winrt::Windows::Storage::Streams::DataReader;
using winrt::Windows::Storage::Streams::IRandomAccessStreamReference;

QString toQString(const winrt::hstring& value) {
    return QString::fromWCharArray(value.c_str(), static_cast<int>(value.size()));
}

void fillBasicMetadata(
    TrackInfo& track,
    const winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionMediaProperties& properties,
    const winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionTimelineProperties& timeline) {
    track.title = toQString(properties.Title());
    track.artist = toQString(properties.Artist());
    track.album = toQString(properties.AlbumTitle());
    track.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        timeline.EndTime() - timeline.StartTime());
}

} // namespace

/// WinRT-facing implementation, kept out of the public header so consumers never need to
/// include the generated C++/WinRT projection headers for GSMTC.
struct MediaSessionManager::Impl {
    explicit Impl(MediaSessionManager& owner)
        : m_owner(owner) {}

    void start() {
        if (m_started.exchange(true)) {
            return;
        }
        initializeAsync();
    }

    void stop() {
        m_stopped.store(true);
        unsubscribeFromCurrentSession();
        m_sessionsChangedRevoker = {};
        m_sessionManager = nullptr;
    }

    winrt::fire_and_forget initializeAsync() {
        try {
            auto manager = co_await GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
            if (m_stopped.load()) {
                co_return;
            }
            m_sessionManager = manager;
            m_sessionsChangedRevoker = m_sessionManager.SessionsChanged(
                winrt::auto_revoke, [this](auto&&, auto&&) { onSessionsChanged(); });
            onSessionsChanged();
        } catch (const winrt::hresult_error& ex) {
            qWarning("MediaSessionManager: failed to acquire the GSMTC session manager (0x%08X)",
                static_cast<unsigned>(ex.code().value));
        }
    }

    void onSessionsChanged() {
        if (m_stopped.load()) {
            return;
        }
        unsubscribeFromCurrentSession();
        m_currentSession = m_sessionManager.GetCurrentSession();
        if (!m_currentSession) {
            m_owner.m_currentTrack = TrackInfo{};
            emit m_owner.trackChanged(m_owner.m_currentTrack);
            emit m_owner.playbackStateChanged(false);
            return;
        }
        subscribeToCurrentSession();
        refreshPlaybackInfo();
        refreshMediaPropertiesAsync();
    }

    void subscribeToCurrentSession() {
        m_mediaPropertiesChangedRevoker = m_currentSession.MediaPropertiesChanged(
            winrt::auto_revoke, [this](auto&&, auto&&) { refreshMediaPropertiesAsync(); });
        m_playbackInfoChangedRevoker = m_currentSession.PlaybackInfoChanged(
            winrt::auto_revoke, [this](auto&&, auto&&) { refreshPlaybackInfo(); });
    }

    void unsubscribeFromCurrentSession() {
        m_mediaPropertiesChangedRevoker = {};
        m_playbackInfoChangedRevoker = {};
        m_currentSession = nullptr;
    }

    void refreshPlaybackInfo() {
        if (m_stopped.load() || !m_currentSession) {
            return;
        }
        try {
            const auto info = m_currentSession.GetPlaybackInfo();
            const bool isPlaying = info.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
            m_owner.m_currentTrack.isPlaying = isPlaying;
            emit m_owner.playbackStateChanged(isPlaying);
        } catch (const winrt::hresult_error&) {
            // The session ended between the event firing and this read; the next
            // SessionsChanged/PlaybackInfoChanged event will settle the state.
        }
    }

    winrt::fire_and_forget refreshMediaPropertiesAsync() {
        auto session = m_currentSession;
        if (!session) {
            co_return;
        }

        TrackInfo track;
        try {
            const auto properties = co_await session.TryGetMediaPropertiesAsync();
            if (m_stopped.load()) {
                co_return;
            }
            fillBasicMetadata(track, properties, session.GetTimelineProperties());

            if (const auto thumbnail = properties.Thumbnail()) {
                co_await loadThumbnailInto(track, thumbnail);
            }
        } catch (const winrt::hresult_error& ex) {
            qWarning("MediaSessionManager: failed to read media properties (0x%08X)",
                static_cast<unsigned>(ex.code().value));
            co_return;
        }

        if (m_stopped.load()) {
            co_return;
        }
        track.isPlaying = m_owner.m_currentTrack.isPlaying;
        m_owner.m_currentTrack = track;
        emit m_owner.trackChanged(track);
        if (!track.coverArt.isNull()) {
            emit m_owner.coverArtReceived(track.coverArt);
        }
    }

    static winrt::Windows::Foundation::IAsyncAction loadThumbnailInto(
        TrackInfo& track, IRandomAccessStreamReference const& thumbnail) {
        const auto stream = co_await thumbnail.OpenReadAsync();
        const auto size = static_cast<uint32_t>(stream.Size());
        if (size == 0) {
            co_return;
        }
        DataReader reader(stream);
        co_await reader.LoadAsync(size);
        std::vector<uint8_t> bytes(size);
        reader.ReadBytes(bytes);
        track.coverArt = QImage::fromData(bytes.data(), static_cast<int>(bytes.size()));
    }

    MediaSessionManager& m_owner;
    GlobalSystemMediaTransportControlsSessionManager m_sessionManager{nullptr};
    GlobalSystemMediaTransportControlsSession m_currentSession{nullptr};
    GlobalSystemMediaTransportControlsSessionManager::SessionsChanged_revoker m_sessionsChangedRevoker;
    GlobalSystemMediaTransportControlsSession::MediaPropertiesChanged_revoker m_mediaPropertiesChangedRevoker;
    GlobalSystemMediaTransportControlsSession::PlaybackInfoChanged_revoker m_playbackInfoChangedRevoker;
    std::atomic<bool> m_started{false};
    std::atomic<bool> m_stopped{false};
};

MediaSessionManager& MediaSessionManager::instance() {
    static MediaSessionManager instance;
    return instance;
}

MediaSessionManager::MediaSessionManager()
    : m_impl(std::make_unique<Impl>(*this)) {}

MediaSessionManager::~MediaSessionManager() = default;

void MediaSessionManager::start() {
    m_impl->start();
}

void MediaSessionManager::stop() {
    m_impl->stop();
}

} // namespace wavely::core
