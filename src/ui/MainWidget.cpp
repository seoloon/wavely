#include "ui/MainWidget.hpp"

#include <QColor>
#include <QPainter>

namespace wavely::ui {

namespace {

constexpr int kDefaultWidth = 360;
constexpr int kDefaultHeight = 120;
constexpr qreal kCornerRadius = 16.0;
const QColor kPlaceholderBackground(20, 20, 24, 180);

} // namespace

MainWidget::MainWidget(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    resize(kDefaultWidth, kDefaultHeight);
}

void MainWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(kPlaceholderBackground);
    painter.drawRoundedRect(rect(), kCornerRadius, kCornerRadius);
}

} // namespace wavely::ui
