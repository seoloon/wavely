#pragma once

#include <QPoint>
#include <QSettings>
#include <QString>

namespace wavely::settings {

/// Widget colour theme; the dominant cover colour is adapted to stay legible against it.
enum class ThemeMode {
    Dark,
    Light
};

/// Persisted position and zoom factor of the overlay widget.
struct WidgetGeometry {
    QPoint position{0, 0};
    double scale = 1.0;
};

/// Typed snapshot of every user preference persisted across sessions.
struct Settings {
    WidgetGeometry geometry;
    bool locked = false;
    bool clickThroughEnabled = false;
    bool hideOnPauseEnabled = false;
    int hideOnPauseDelaySeconds = 10;
    bool launchAtStartup = false;
    ThemeMode theme = ThemeMode::Dark;
    QString languageCode = QStringLiteral("fr");
};

/// Loads Wavely's settings from the Windows registry (via QSettings) on construction and
/// persists each change immediately, so a crash never loses the last known geometry or state.
class AppConfig {
public:
    AppConfig();

    const Settings& values() const noexcept { return m_settings; }

    void setGeometry(const WidgetGeometry& geometry);
    void setLocked(bool locked);
    void setClickThroughEnabled(bool enabled);
    void setHideOnPauseEnabled(bool enabled);
    void setHideOnPauseDelaySeconds(int seconds);
    void setLaunchAtStartup(bool enabled);
    void setTheme(ThemeMode theme);
    void setLanguageCode(const QString& languageCode);

private:
    void load();
    void save();

    QSettings m_store;
    Settings m_settings;
};

} // namespace wavely::settings
