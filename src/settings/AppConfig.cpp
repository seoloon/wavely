#include "settings/AppConfig.hpp"

#include <algorithm>

namespace wavely::settings {

namespace {

constexpr double kMinScale = 0.5;
constexpr double kMaxScale = 1.5;
constexpr int kMinHideOnPauseDelaySeconds = 5;
constexpr int kMaxHideOnPauseDelaySeconds = 30;

constexpr auto kKeyGeometryPosition = "geometry/position";
constexpr auto kKeyGeometryScale = "geometry/scale";
constexpr auto kKeyLocked = "behavior/locked";
constexpr auto kKeyClickThrough = "behavior/clickThrough";
constexpr auto kKeyHideOnPause = "behavior/hideOnPause";
constexpr auto kKeyHideOnPauseDelay = "behavior/hideOnPauseDelaySeconds";
constexpr auto kKeyLaunchAtStartup = "behavior/launchAtStartup";
constexpr auto kKeyTheme = "appearance/theme";
constexpr auto kKeyLanguage = "behavior/language";

} // namespace

AppConfig::AppConfig()
    : m_store(QStringLiteral("Wavely"), QStringLiteral("Wavely")) {
    load();
}

void AppConfig::load() {
    m_settings.geometry.position = m_store.value(kKeyGeometryPosition, m_settings.geometry.position).toPoint();
    m_settings.geometry.scale = std::clamp(
        m_store.value(kKeyGeometryScale, m_settings.geometry.scale).toDouble(), kMinScale, kMaxScale);
    m_settings.locked = m_store.value(kKeyLocked, m_settings.locked).toBool();
    m_settings.clickThroughEnabled = m_store.value(kKeyClickThrough, m_settings.clickThroughEnabled).toBool();
    m_settings.hideOnPauseEnabled = m_store.value(kKeyHideOnPause, m_settings.hideOnPauseEnabled).toBool();
    m_settings.hideOnPauseDelaySeconds = std::clamp(
        m_store.value(kKeyHideOnPauseDelay, m_settings.hideOnPauseDelaySeconds).toInt(),
        kMinHideOnPauseDelaySeconds, kMaxHideOnPauseDelaySeconds);
    m_settings.launchAtStartup = m_store.value(kKeyLaunchAtStartup, m_settings.launchAtStartup).toBool();
    m_settings.theme = static_cast<ThemeMode>(
        m_store.value(kKeyTheme, static_cast<int>(m_settings.theme)).toInt());
    m_settings.languageCode = m_store.value(kKeyLanguage, m_settings.languageCode).toString();
}

void AppConfig::save() {
    m_store.setValue(kKeyGeometryPosition, m_settings.geometry.position);
    m_store.setValue(kKeyGeometryScale, m_settings.geometry.scale);
    m_store.setValue(kKeyLocked, m_settings.locked);
    m_store.setValue(kKeyClickThrough, m_settings.clickThroughEnabled);
    m_store.setValue(kKeyHideOnPause, m_settings.hideOnPauseEnabled);
    m_store.setValue(kKeyHideOnPauseDelay, m_settings.hideOnPauseDelaySeconds);
    m_store.setValue(kKeyLaunchAtStartup, m_settings.launchAtStartup);
    m_store.setValue(kKeyTheme, static_cast<int>(m_settings.theme));
    m_store.setValue(kKeyLanguage, m_settings.languageCode);
}

void AppConfig::setGeometry(const WidgetGeometry& geometry) {
    m_settings.geometry.position = geometry.position;
    m_settings.geometry.scale = std::clamp(geometry.scale, kMinScale, kMaxScale);
    save();
}

void AppConfig::setLocked(bool locked) {
    m_settings.locked = locked;
    save();
}

void AppConfig::setClickThroughEnabled(bool enabled) {
    m_settings.clickThroughEnabled = enabled;
    save();
}

void AppConfig::setHideOnPauseEnabled(bool enabled) {
    m_settings.hideOnPauseEnabled = enabled;
    save();
}

void AppConfig::setHideOnPauseDelaySeconds(int seconds) {
    m_settings.hideOnPauseDelaySeconds = std::clamp(seconds, kMinHideOnPauseDelaySeconds, kMaxHideOnPauseDelaySeconds);
    save();
}

void AppConfig::setLaunchAtStartup(bool enabled) {
    m_settings.launchAtStartup = enabled;
    save();
}

void AppConfig::setTheme(ThemeMode theme) {
    m_settings.theme = theme;
    save();
}

void AppConfig::setLanguageCode(const QString& languageCode) {
    m_settings.languageCode = languageCode;
    save();
}

} // namespace wavely::settings
