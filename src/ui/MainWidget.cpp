#include "ui/MainWidget.hpp"

#include "core/MediaMetadata.hpp"
#include "core/MediaSessionManager.hpp"
#include "ui/TrayIcon.hpp"
#include "ui/VisibilityController.hpp"

#include <QCloseEvent>
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <windows.h>

namespace wavely::ui {

namespace {

constexpr int kDefaultWidth = 360;
constexpr int kDefaultHeight = 120;
constexpr int kCoverSize = 88;
constexpr int kContentMargin = 16;
constexpr int kHandleSize = 14;
constexpr int kHandleMargin = 4;
constexpr qreal kCornerRadius = 16.0;
constexpr double kScaleStep = 0.1;
const QColor kPlaceholderBackground(20, 20, 24, 180);

bool isControlKeyDown() {
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}

} // namespace

MainWidget::MainWidget(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    resize(kDefaultWidth, kDefaultHeight);

    m_coverLabel = new QLabel(this);
    m_coverLabel->setScaledContents(true);

    m_titleLabel = new QLabel(tr("No track playing"), this);
    m_titleLabel->setStyleSheet(QStringLiteral("color: white; font-weight: bold;"));

    m_artistLabel = new QLabel(this);
    m_artistLabel->setStyleSheet(QStringLiteral("color: rgba(255, 255, 255, 180);"));

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: rgba(255, 255, 255, 120); font-size: 10px;"));

    auto* textLayout = new QVBoxLayout;
    textLayout->addStretch();
    textLayout->addWidget(m_titleLabel);
    textLayout->addWidget(m_artistLabel);
    textLayout->addWidget(m_statusLabel);
    textLayout->addStretch();

    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(kContentMargin, kContentMargin, kContentMargin, kContentMargin);
    rootLayout->addWidget(m_coverLabel);
    rootLayout->addLayout(textLayout);
    // Without this, the layout silently enforces a minimum size from the labels' natural
    // sizeHint, which would clamp setScale()'s resize() below 100% at the low end of 50%-150%.
    rootLayout->setSizeConstraint(QLayout::SetNoConstraint);

    // Always-clickable handle, shown only in click-through mode: WS_EX_TRANSPARENT makes the
    // OS route every click straight through the *entire* native window, including the area
    // under any Qt child widget drawn on top of it - so the handle must be its own top-level
    // window (own HWND) to stay clickable. This is the "safe zone" PROMPT.md asks for.
    m_clickThroughHandle = std::make_unique<QWidget>(nullptr, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    m_clickThroughHandle->setAttribute(Qt::WA_TranslucentBackground);
    m_clickThroughHandle->setFixedSize(kHandleSize, kHandleSize);
    m_clickThroughHandle->setStyleSheet(QStringLiteral("background-color: rgba(255, 200, 0, 200); border-radius: 7px;"));
    m_clickThroughHandle->setCursor(Qt::PointingHandCursor);
    m_clickThroughHandle->setToolTip(tr("Click to disable click-through"));
    m_clickThroughHandle->installEventFilter(this);

    m_visibilityController = new VisibilityController(this, this);
    m_visibilityController->setHideOnPauseEnabled(m_config.values().hideOnPauseEnabled);
    m_visibilityController->setHideOnPauseDelay(std::chrono::seconds(m_config.values().hideOnPauseDelaySeconds));

    auto& sessionManager = core::MediaSessionManager::instance();
    connect(&sessionManager, &core::MediaSessionManager::trackChanged, this, &MainWidget::onTrackChanged);
    connect(&sessionManager, &core::MediaSessionManager::playbackStateChanged, this, &MainWidget::onPlaybackStateChanged);
    connect(&sessionManager, &core::MediaSessionManager::playbackStateChanged,
        m_visibilityController, &VisibilityController::onPlaybackStateChanged);

    move(m_config.values().geometry.position);
    setScale(m_config.values().geometry.scale);
    setClickThroughEnabled(m_config.values().clickThroughEnabled);
    m_visibilityController->fadeIn();

    m_trayIcon = std::make_unique<TrayIcon>(this, m_config);
}

MainWidget::~MainWidget() = default;

void MainWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(kPlaceholderBackground);
    painter.drawRoundedRect(rect(), kCornerRadius, kCornerRadius);
}

void MainWidget::wheelEvent(QWheelEvent* event) {
    if (m_config.values().locked) {
        QWidget::wheelEvent(event);
        return;
    }
    const double direction = event->angleDelta().y() > 0 ? 1.0 : -1.0;
    setScale(m_config.values().geometry.scale + direction * kScaleStep);
    event->accept();
}

void MainWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && event->modifiers().testFlag(Qt::ControlModifier)) {
        setClickThroughEnabled(!m_config.values().clickThroughEnabled);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void MainWidget::moveEvent(QMoveEvent* event) {
    QWidget::moveEvent(event);
    m_clickThroughHandle->move(pos() + QPoint(kHandleMargin, kHandleMargin));
}

void MainWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    m_clickThroughHandle->move(pos() + QPoint(kHandleMargin, kHandleMargin));
    m_clickThroughHandle->setVisible(m_config.values().clickThroughEnabled);
}

void MainWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    m_clickThroughHandle->hide();
}

void MainWidget::closeEvent(QCloseEvent* event) {
    // The app lives in the tray (see TrayIcon): closing the widget only hides it, it does not
    // quit. Only the tray's "Quit" action terminates the process.
    event->ignore();
    hide();
}

bool MainWidget::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    if (eventType == "windows_generic_MSG") {
        auto* msg = static_cast<MSG*>(message);
        if (msg->message == WM_NCHITTEST) {
            if (!m_config.values().locked && !isControlKeyDown()) {
                *result = HTCAPTION;
                return true;
            }
        } else if (msg->message == WM_EXITSIZEMOVE) {
            persistCurrentPosition();
        }
    }
    return QWidget::nativeEvent(eventType, message, result);
}

bool MainWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_clickThroughHandle.get() && event->type() == QEvent::MouseButtonPress) {
        setClickThroughEnabled(false);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void MainWidget::onTrackChanged(const core::TrackInfo& track) {
    m_titleLabel->setText(track.title.isEmpty() ? tr("No track playing") : track.title);
    m_artistLabel->setText(track.artist);
    m_coverLabel->setPixmap(track.coverArt.isNull() ? QPixmap() : QPixmap::fromImage(track.coverArt));
}

void MainWidget::onPlaybackStateChanged(bool isPlaying) {
    m_statusLabel->setText(isPlaying ? tr("Playing") : tr("Paused"));
}

void MainWidget::setScale(double scale) {
    m_config.setGeometry({pos(), scale});
    const double appliedScale = m_config.values().geometry.scale;
    resize(static_cast<int>(kDefaultWidth * appliedScale), static_cast<int>(kDefaultHeight * appliedScale));
    const int coverSize = static_cast<int>(kCoverSize * appliedScale);
    m_coverLabel->setFixedSize(coverSize, coverSize);
}

void MainWidget::setClickThroughEnabled(bool enabled) {
    m_config.setClickThroughEnabled(enabled);
    setWindowFlag(Qt::WindowTransparentForInput, enabled);
    show();
    m_clickThroughHandle->move(pos() + QPoint(kHandleMargin, kHandleMargin));
    m_clickThroughHandle->setVisible(enabled && isVisible());
}

void MainWidget::persistCurrentPosition() {
    m_config.setGeometry({pos(), m_config.values().geometry.scale});
}

} // namespace wavely::ui
