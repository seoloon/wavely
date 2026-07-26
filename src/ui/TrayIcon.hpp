#pragma once

#include <QObject>
#include <QSystemTrayIcon>

#include <memory>

class QMenu;
class QWidget;

namespace wavely::settings {
class AppConfig;
}

namespace wavely::ui {

/// Owns the system tray icon and its context menu (Settings, Reload widget, Launch at startup,
/// Quit). A left-click or double-click on the icon toggles the overlay widget's visibility;
/// closing the widget (see MainWidget::closeEvent) only hides it, so the tray is what actually
/// keeps the process alive and is the only path that terminates it.
class TrayIcon : public QObject {
    Q_OBJECT

public:
    TrayIcon(QWidget* widget, settings::AppConfig& config, QObject* parent = nullptr);
    // Declared (defined as = default in the .cpp) so std::unique_ptr<QMenu>'s destructor
    // instantiates where QMenu is a complete type, not wherever this header is included.
    ~TrayIcon() override;

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onReloadTriggered();
    void onLaunchAtStartupToggled(bool checked);

private:
    QWidget* m_widget;
    settings::AppConfig& m_config;
    QSystemTrayIcon* m_trayIcon;

    /// QSystemTrayIcon::setContextMenu() does not take ownership of the menu, and TrayIcon
    /// (a plain QObject) can't parent a QWidget-derived QMenu through Qt's tree - hence RAII here.
    std::unique_ptr<QMenu> m_menu;
};

} // namespace wavely::ui
