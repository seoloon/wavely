#include <QApplication>

#include "core/WinrtGuard.hpp"
#include "ui/MainWidget.hpp"

int main(int argc, char* argv[]) {
    const wavely::core::WinrtApartmentGuard winrtGuard;

    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Wavely"));
    QApplication::setApplicationName(QStringLiteral("Wavely"));

    wavely::ui::MainWidget widget;
    widget.show();

    return QApplication::exec();
}
