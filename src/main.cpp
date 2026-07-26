#include <QApplication>

#include "core/MediaSessionManager.hpp"
#include "core/WinrtGuard.hpp"
#include "ui/MainWidget.hpp"

int main(int argc, char* argv[]) {
    const wavely::core::WinrtApartmentGuard winrtGuard;

    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Wavely"));
    QApplication::setApplicationName(QStringLiteral("Wavely"));
    // The tray icon (see TrayIcon) is what keeps the app alive; closing the widget only hides
    // it (MainWidget::closeEvent), so the app must not quit just because it became invisible.
    QApplication::setQuitOnLastWindowClosed(false);

    wavely::ui::MainWidget widget;

    wavely::core::MediaSessionManager::instance().start();

    const int exitCode = QApplication::exec();

    // Must run before winrtGuard is destroyed (see MediaSessionManager::stop doc comment).
    wavely::core::MediaSessionManager::instance().stop();

    return exitCode;
}
