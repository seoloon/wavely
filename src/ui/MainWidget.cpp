#include "ui/MainWidget.hpp"

#include "core/MediaMetadata.hpp"
#include "core/MediaSessionManager.hpp"

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>

namespace wavely::ui {

namespace {

constexpr int kDefaultWidth = 360;
constexpr int kDefaultHeight = 120;
constexpr int kCoverSize = 88;
constexpr int kContentMargin = 16;
constexpr qreal kCornerRadius = 16.0;
const QColor kPlaceholderBackground(20, 20, 24, 180);

} // namespace

MainWidget::MainWidget(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    resize(kDefaultWidth, kDefaultHeight);

    m_coverLabel = new QLabel(this);
    m_coverLabel->setFixedSize(kCoverSize, kCoverSize);
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

    auto& sessionManager = core::MediaSessionManager::instance();
    connect(&sessionManager, &core::MediaSessionManager::trackChanged, this, &MainWidget::onTrackChanged);
    connect(&sessionManager, &core::MediaSessionManager::playbackStateChanged, this, &MainWidget::onPlaybackStateChanged);
}

void MainWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(kPlaceholderBackground);
    painter.drawRoundedRect(rect(), kCornerRadius, kCornerRadius);
}

void MainWidget::onTrackChanged(const core::TrackInfo& track) {
    m_titleLabel->setText(track.title.isEmpty() ? tr("No track playing") : track.title);
    m_artistLabel->setText(track.artist);
    m_coverLabel->setPixmap(track.coverArt.isNull() ? QPixmap() : QPixmap::fromImage(track.coverArt));
}

void MainWidget::onPlaybackStateChanged(bool isPlaying) {
    m_statusLabel->setText(isPlaying ? tr("Playing") : tr("Paused"));
}

} // namespace wavely::ui
