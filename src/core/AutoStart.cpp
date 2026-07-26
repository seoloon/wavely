#include "core/AutoStart.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace wavely::core::autostart {

namespace {
constexpr auto kRunKeyPath = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr auto kValueName = "Wavely";
} // namespace

bool isEnabled() {
    QSettings runKey(kRunKeyPath, QSettings::NativeFormat);
    return runKey.contains(kValueName);
}

void setEnabled(bool enabled) {
    QSettings runKey(kRunKeyPath, QSettings::NativeFormat);
    if (enabled) {
        const QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        runKey.setValue(kValueName, QStringLiteral("\"%1\"").arg(exePath));
    } else {
        runKey.remove(kValueName);
    }
}

} // namespace wavely::core::autostart
