#pragma once

#include <QImage>
#include <QMetaType>
#include <QString>

#include <chrono>

namespace wavely::core {

/// Immutable snapshot of what the active GSMTC session is currently playing.
struct TrackInfo {
    QString title;
    QString artist;
    QString album;
    QImage coverArt;
    std::chrono::milliseconds duration{0};
    bool isPlaying = false;
};

} // namespace wavely::core

Q_DECLARE_METATYPE(wavely::core::TrackInfo)
