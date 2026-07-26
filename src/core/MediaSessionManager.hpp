#pragma once

#include <QObject>

#include <memory>

#include "core/MediaMetadata.hpp"

namespace wavely::core {

/// Observes the system-wide GSMTC session (Spotify, YouTube Music, Deezer, ...) and republishes
/// playback state, metadata and cover art as Qt signals. WinRT types stay behind a pImpl so
/// consumers of this header never need to include the generated C++/WinRT projection headers.
class MediaSessionManager : public QObject {
    Q_OBJECT

public:
    static MediaSessionManager& instance();

    /// Starts observing the system session manager. Safe to call once; later calls are ignored.
    void start();

    /// Revokes every WinRT subscription and releases session references. Must run before the
    /// process's WinRT apartment is uninitialized, so main() calls it explicitly before returning
    /// rather than relying on this singleton's static-storage destruction order.
    void stop();

    const TrackInfo& currentTrack() const noexcept { return m_currentTrack; }

signals:
    void trackChanged(const wavely::core::TrackInfo& track);
    void playbackStateChanged(bool isPlaying);
    void coverArtReceived(const QImage& coverArt);

private:
    MediaSessionManager();
    ~MediaSessionManager() override;

    MediaSessionManager(const MediaSessionManager&) = delete;
    MediaSessionManager& operator=(const MediaSessionManager&) = delete;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    TrackInfo m_currentTrack;
};

} // namespace wavely::core
