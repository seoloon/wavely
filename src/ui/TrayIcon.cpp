#include "ui/TrayIcon.hpp"

#include "core/AutoStart.hpp"
#include "core/MediaSessionManager.hpp"
#include "settings/AppConfig.hpp"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QWidget>

namespace wavely::ui {

namespace {

constexpr int kIconSize = 32;
const QColor kIconColor(90, 170, 255);

/// No branded asset exists yet (RULES.md §8.5 schedules the real app icon for Phase 8
/// packaging); a simple filled circle keeps the tray from showing a blank/default icon.
QIcon makePlaceholderIcon() {
    QPixmap pixmap(kIconSize, kIconSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(kIconColor);
    painter.drawEllipse(pixmap.rect().adjusted(2, 2, -2, -2));
    return QIcon(pixmap);
}

} // namespace

TrayIcon::TrayIcon(QWidget* widget, settings::AppConfig& config, QObject* parent)
    : QObject(parent)
    , m_widget(widget)
    , m_config(config)
    , m_trayIcon(new QSystemTrayIcon(makePlaceholderIcon(), this))
    , m_menu(std::make_unique<QMenu>()) {
    auto* settingsAction = m_menu->addAction(tr("Settings..."));
    settingsAction->setEnabled(false); // The settings window lands in Phase 4.

    auto* reloadAction = m_menu->addAction(tr("Reload widget"));
    connect(reloadAction, &QAction::triggered, this, &TrayIcon::onReloadTriggered);

    m_menu->addSeparator();

    auto* launchAtStartupAction = m_menu->addAction(tr("Launch at startup"));
    launchAtStartupAction->setCheckable(true);
    launchAtStartupAction->setChecked(m_config.values().launchAtStartup);
    connect(launchAtStartupAction, &QAction::toggled, this, &TrayIcon::onLaunchAtStartupToggled);

    m_menu->addSeparator();

    auto* quitAction = m_menu->addAction(tr("Quit"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    m_trayIcon->setContextMenu(m_menu.get());
    m_trayIcon->setToolTip(QStringLiteral("Wavely"));
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);
    m_trayIcon->show();
}

TrayIcon::~TrayIcon() = default;

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick) {
        return;
    }
    if (m_widget->isVisible()) {
        m_widget->hide();
    } else {
        m_widget->setWindowOpacity(1.0);
        m_widget->show();
    }
}

void TrayIcon::onReloadTriggered() {
    core::MediaSessionManager::instance().refresh();
}

void TrayIcon::onLaunchAtStartupToggled(bool checked) {
    core::autostart::setEnabled(checked);
    m_config.setLaunchAtStartup(checked);
}

} // namespace wavely::ui
