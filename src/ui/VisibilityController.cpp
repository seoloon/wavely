#include "ui/VisibilityController.hpp"

#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>

namespace wavely::ui {

namespace {
constexpr int kFadeDurationMs = 300;
} // namespace

VisibilityController::VisibilityController(QWidget* target, QObject* parent)
    : QObject(parent)
    , m_target(target)
    , m_fadeAnimation(new QPropertyAnimation(target, "windowOpacity", this))
    , m_hideTimer(new QTimer(this)) {
    m_fadeAnimation->setDuration(kFadeDurationMs);

    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &VisibilityController::fadeOutAndHide);

    connect(m_fadeAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (m_target->windowOpacity() <= 0.0) {
            m_target->hide();
            m_hiddenByAutoHide = true;
        }
    });
}

void VisibilityController::setHideOnPauseEnabled(bool enabled) {
    m_hideOnPauseEnabled = enabled;
    if (!enabled) {
        m_hideTimer->stop();
    }
}

void VisibilityController::setHideOnPauseDelay(std::chrono::seconds delay) {
    m_hideTimer->setInterval(static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(delay).count()));
}

void VisibilityController::fadeIn() {
    m_target->setWindowOpacity(0.0);
    m_target->show();
    m_fadeAnimation->stop();
    m_fadeAnimation->setStartValue(0.0);
    m_fadeAnimation->setEndValue(1.0);
    m_fadeAnimation->start();
}

void VisibilityController::onPlaybackStateChanged(bool isPlaying) {
    if (isPlaying) {
        m_hideTimer->stop();
        if (m_hiddenByAutoHide) {
            // Reappearing after a full hide is instant by design; only the hide fades.
            m_target->setWindowOpacity(1.0);
            m_target->show();
            m_hiddenByAutoHide = false;
        }
    } else if (m_hideOnPauseEnabled) {
        m_hideTimer->start();
    }
}

void VisibilityController::fadeOutAndHide() {
    m_fadeAnimation->stop();
    m_fadeAnimation->setStartValue(m_target->windowOpacity());
    m_fadeAnimation->setEndValue(0.0);
    m_fadeAnimation->start();
}

} // namespace wavely::ui
