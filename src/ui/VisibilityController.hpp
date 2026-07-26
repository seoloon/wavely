#pragma once

#include <QObject>

#include <chrono>

class QWidget;
class QPropertyAnimation;
class QTimer;

namespace wavely::ui {

/// Drives the overlay's fade in/out and pause-triggered auto-hide, decoupled from MainWidget so
/// window-flags/interaction code doesn't also have to own animation and timer bookkeeping.
/// Resuming playback while fully hidden reappears instantly (no fade) per the product spec;
/// only the pause-triggered hide animates.
class VisibilityController : public QObject {
    Q_OBJECT

public:
    explicit VisibilityController(QWidget* target, QObject* parent = nullptr);

    void setHideOnPauseEnabled(bool enabled);
    void setHideOnPauseDelay(std::chrono::seconds delay);

    /// Shows the target with a 300ms fade-in. Intended for initial startup only.
    void fadeIn();

public slots:
    void onPlaybackStateChanged(bool isPlaying);

private:
    void fadeOutAndHide();

    QWidget* m_target;
    QPropertyAnimation* m_fadeAnimation;
    QTimer* m_hideTimer;
    bool m_hideOnPauseEnabled = false;
};

} // namespace wavely::ui
