#pragma once

#include <QWidget>

class QLabel;

namespace wavely::core {
struct TrackInfo;
}

namespace wavely::ui {

/// Frameless, translucent top-level window hosting the Wavely overlay widget. Phase 1 adds a
/// minimal title/artist/cover/status display wired to MediaSessionManager; presets, drag,
/// resize and hide-on-pause animation land in later phases.
class MainWidget : public QWidget {
    Q_OBJECT

public:
    explicit MainWidget(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onTrackChanged(const wavely::core::TrackInfo& track);
    void onPlaybackStateChanged(bool isPlaying);

private:
    QLabel* m_coverLabel;
    QLabel* m_titleLabel;
    QLabel* m_artistLabel;
    QLabel* m_statusLabel;
};

} // namespace wavely::ui
