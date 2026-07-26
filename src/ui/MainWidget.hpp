#pragma once

#include <QWidget>

#include "settings/AppConfig.hpp"

#include <memory>

class QLabel;

namespace wavely::core {
struct TrackInfo;
}

namespace wavely::ui {

class VisibilityController;

/// Frameless, translucent top-level window hosting the Wavely overlay widget: draggable via the
/// OS caption trick (multi-monitor for free), resizable 50%-150%, toggleable click-through, and
/// auto-hides after a delay when playback pauses. Presets land in a later phase.
class MainWidget : public QWidget {
    Q_OBJECT

public:
    explicit MainWidget(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onTrackChanged(const wavely::core::TrackInfo& track);
    void onPlaybackStateChanged(bool isPlaying);

private:
    void setScale(double scale);
    void setClickThroughEnabled(bool enabled);
    void persistCurrentPosition();

    settings::AppConfig m_config;
    VisibilityController* m_visibilityController;

    QLabel* m_coverLabel;
    QLabel* m_titleLabel;
    QLabel* m_artistLabel;
    QLabel* m_statusLabel;

    /// Parentless by design (own HWND, see setClickThroughEnabled) - Qt's parent-child
    /// ownership can't clean it up, hence explicit RAII here instead of a raw pointer.
    std::unique_ptr<QWidget> m_clickThroughHandle;
};

} // namespace wavely::ui
