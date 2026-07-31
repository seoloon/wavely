#include "pch.h"
#include "AppConfig.h"
#include "AppConfig.g.cpp"

#include "ThirdParty/nlohmann/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace winrt::Wavely::Backend::implementation
{
    namespace
    {
        constexpr double kMinScale = 0.5;
        constexpr double kMaxScale = 1.5;
        constexpr std::int32_t kMinHideOnPauseDelaySeconds = 5;
        constexpr std::int32_t kMaxHideOnPauseDelaySeconds = 30;
        constexpr std::int32_t kMinPresetIndex = 0;
        constexpr std::int32_t kMaxPresetIndex = 6;
        constexpr double kMinBackgroundOpacity = 0.0;
        constexpr double kMaxBackgroundOpacity = 1.0;
        constexpr wchar_t kAppFolderName[] = L"Wavely";
        constexpr wchar_t kSettingsFileName[] = L"settings.json";

        std::filesystem::path settingsFilePath()
        {
            wchar_t buffer[MAX_PATH];
            const DWORD length = GetEnvironmentVariableW(L"APPDATA", buffer, MAX_PATH);
            winrt::check_bool(length > 0 && length < MAX_PATH);

            const std::filesystem::path directory = std::filesystem::path(std::wstring(buffer, length)) / kAppFolderName;
            std::filesystem::create_directories(directory);
            return directory / kSettingsFileName;
        }
    }

    AppConfig::AppConfig()
    {
        load();
    }

    void AppConfig::load()
    {
        const std::lock_guard lock(m_mutex);

        std::ifstream file(settingsFilePath());
        if (!file)
        {
            return;
        }

        nlohmann::json json;
        try
        {
            file >> json;
        }
        catch (const nlohmann::json::parse_error&)
        {
            return;
        }

        m_geometry.PositionX = json.value("geometryPositionX", m_geometry.PositionX);
        m_geometry.PositionY = json.value("geometryPositionY", m_geometry.PositionY);
        m_geometry.Scale = std::clamp(json.value("geometryScale", m_geometry.Scale), kMinScale, kMaxScale);
        m_locked = json.value("locked", m_locked);
        m_clickThroughEnabled = json.value("clickThroughEnabled", m_clickThroughEnabled);
        m_hideOnPauseEnabled = json.value("hideOnPauseEnabled", m_hideOnPauseEnabled);
        m_hideOnPauseDelaySeconds = std::clamp(
            json.value("hideOnPauseDelaySeconds", m_hideOnPauseDelaySeconds),
            kMinHideOnPauseDelaySeconds, kMaxHideOnPauseDelaySeconds);
        m_launchAtStartup = json.value("launchAtStartup", m_launchAtStartup);
        m_theme = static_cast<ThemeMode>(json.value("theme", static_cast<int>(m_theme)));
        m_languageCode = winrt::to_hstring(json.value("languageCode", winrt::to_string(m_languageCode)));

        m_presetIndex = std::clamp(json.value("presetIndex", m_presetIndex), kMinPresetIndex, kMaxPresetIndex);
        m_coverShape = static_cast<CoverStyle>(json.value("coverShape", static_cast<int>(m_coverShape)));
        m_coverGlowEnabled = json.value("coverGlowEnabled", m_coverGlowEnabled);
        m_coverBlurEnabled = json.value("coverBlurEnabled", m_coverBlurEnabled);
        m_dynamicColorsEnabled = json.value("dynamicColorsEnabled", m_dynamicColorsEnabled);
        m_dynamicBackgroundEnabled = json.value("dynamicBackgroundEnabled", m_dynamicBackgroundEnabled);
        m_backgroundOpacity = std::clamp(
            json.value("backgroundOpacity", m_backgroundOpacity), kMinBackgroundOpacity, kMaxBackgroundOpacity);
        m_customAccentColor = json.value("customAccentColor", m_customAccentColor);
    }

    void AppConfig::saveLocked() const
    {
        nlohmann::json json;
        json["geometryPositionX"] = m_geometry.PositionX;
        json["geometryPositionY"] = m_geometry.PositionY;
        json["geometryScale"] = m_geometry.Scale;
        json["locked"] = m_locked;
        json["clickThroughEnabled"] = m_clickThroughEnabled;
        json["hideOnPauseEnabled"] = m_hideOnPauseEnabled;
        json["hideOnPauseDelaySeconds"] = m_hideOnPauseDelaySeconds;
        json["launchAtStartup"] = m_launchAtStartup;
        json["theme"] = static_cast<int>(m_theme);
        json["languageCode"] = winrt::to_string(m_languageCode);

        json["presetIndex"] = m_presetIndex;
        json["coverShape"] = static_cast<int>(m_coverShape);
        json["coverGlowEnabled"] = m_coverGlowEnabled;
        json["coverBlurEnabled"] = m_coverBlurEnabled;
        json["dynamicColorsEnabled"] = m_dynamicColorsEnabled;
        json["dynamicBackgroundEnabled"] = m_dynamicBackgroundEnabled;
        json["backgroundOpacity"] = m_backgroundOpacity;
        json["customAccentColor"] = m_customAccentColor;

        std::ofstream file(settingsFilePath());
        file << json.dump(2);
    }

    winrt::Wavely::Backend::WidgetGeometry AppConfig::Geometry()
    {
        const std::lock_guard lock(m_mutex);
        return m_geometry;
    }

    bool AppConfig::Locked()
    {
        const std::lock_guard lock(m_mutex);
        return m_locked;
    }

    bool AppConfig::ClickThroughEnabled()
    {
        const std::lock_guard lock(m_mutex);
        return m_clickThroughEnabled;
    }

    bool AppConfig::HideOnPauseEnabled()
    {
        const std::lock_guard lock(m_mutex);
        return m_hideOnPauseEnabled;
    }

    std::int32_t AppConfig::HideOnPauseDelaySeconds()
    {
        const std::lock_guard lock(m_mutex);
        return m_hideOnPauseDelaySeconds;
    }

    bool AppConfig::LaunchAtStartup()
    {
        const std::lock_guard lock(m_mutex);
        return m_launchAtStartup;
    }

    winrt::Wavely::Backend::ThemeMode AppConfig::Theme()
    {
        const std::lock_guard lock(m_mutex);
        return m_theme;
    }

    hstring AppConfig::LanguageCode()
    {
        const std::lock_guard lock(m_mutex);
        return m_languageCode;
    }

    void AppConfig::SetGeometry(winrt::Wavely::Backend::WidgetGeometry const& geometry)
    {
        const std::lock_guard lock(m_mutex);
        m_geometry = geometry;
        m_geometry.Scale = std::clamp(m_geometry.Scale, kMinScale, kMaxScale);
        saveLocked();
    }

    void AppConfig::SetLocked(bool locked)
    {
        const std::lock_guard lock(m_mutex);
        m_locked = locked;
        saveLocked();
    }

    void AppConfig::SetClickThroughEnabled(bool enabled)
    {
        const std::lock_guard lock(m_mutex);
        m_clickThroughEnabled = enabled;
        saveLocked();
    }

    void AppConfig::SetHideOnPauseEnabled(bool enabled)
    {
        const std::lock_guard lock(m_mutex);
        m_hideOnPauseEnabled = enabled;
        saveLocked();
    }

    void AppConfig::SetHideOnPauseDelaySeconds(std::int32_t seconds)
    {
        const std::lock_guard lock(m_mutex);
        m_hideOnPauseDelaySeconds = std::clamp(seconds, kMinHideOnPauseDelaySeconds, kMaxHideOnPauseDelaySeconds);
        saveLocked();
    }

    void AppConfig::SetLaunchAtStartup(bool enabled)
    {
        const std::lock_guard lock(m_mutex);
        m_launchAtStartup = enabled;
        saveLocked();
    }

    void AppConfig::SetTheme(winrt::Wavely::Backend::ThemeMode const& theme)
    {
        const std::lock_guard lock(m_mutex);
        m_theme = theme;
        saveLocked();
    }

    void AppConfig::SetLanguageCode(hstring const& languageCode)
    {
        const std::lock_guard lock(m_mutex);
        m_languageCode = languageCode;
        saveLocked();
    }

    std::int32_t AppConfig::PresetIndex()
    {
        const std::lock_guard lock(m_mutex);
        return m_presetIndex;
    }

    winrt::Wavely::Backend::CoverStyle AppConfig::CoverShape()
    {
        const std::lock_guard lock(m_mutex);
        return m_coverShape;
    }

    bool AppConfig::CoverGlowEnabled()
    {
        const std::lock_guard lock(m_mutex);
        return m_coverGlowEnabled;
    }

    bool AppConfig::CoverBlurEnabled()
    {
        const std::lock_guard lock(m_mutex);
        return m_coverBlurEnabled;
    }

    bool AppConfig::DynamicColorsEnabled()
    {
        const std::lock_guard lock(m_mutex);
        return m_dynamicColorsEnabled;
    }

    bool AppConfig::DynamicBackgroundEnabled()
    {
        const std::lock_guard lock(m_mutex);
        return m_dynamicBackgroundEnabled;
    }

    double AppConfig::BackgroundOpacity()
    {
        const std::lock_guard lock(m_mutex);
        return m_backgroundOpacity;
    }

    std::uint32_t AppConfig::CustomAccentColor()
    {
        const std::lock_guard lock(m_mutex);
        return m_customAccentColor;
    }

    void AppConfig::SetPresetIndex(std::int32_t index)
    {
        const std::lock_guard lock(m_mutex);
        m_presetIndex = std::clamp(index, kMinPresetIndex, kMaxPresetIndex);
        saveLocked();
    }

    void AppConfig::SetCoverShape(winrt::Wavely::Backend::CoverStyle const& shape)
    {
        const std::lock_guard lock(m_mutex);
        m_coverShape = shape;
        saveLocked();
    }

    void AppConfig::SetCoverGlowEnabled(bool enabled)
    {
        const std::lock_guard lock(m_mutex);
        m_coverGlowEnabled = enabled;
        saveLocked();
    }

    void AppConfig::SetCoverBlurEnabled(bool enabled)
    {
        const std::lock_guard lock(m_mutex);
        m_coverBlurEnabled = enabled;
        saveLocked();
    }

    void AppConfig::SetDynamicColorsEnabled(bool enabled)
    {
        const std::lock_guard lock(m_mutex);
        m_dynamicColorsEnabled = enabled;
        saveLocked();
    }

    void AppConfig::SetDynamicBackgroundEnabled(bool enabled)
    {
        const std::lock_guard lock(m_mutex);
        m_dynamicBackgroundEnabled = enabled;
        saveLocked();
    }

    void AppConfig::SetBackgroundOpacity(double opacity)
    {
        const std::lock_guard lock(m_mutex);
        m_backgroundOpacity = std::clamp(opacity, kMinBackgroundOpacity, kMaxBackgroundOpacity);
        saveLocked();
    }

    void AppConfig::SetCustomAccentColor(std::uint32_t color)
    {
        const std::lock_guard lock(m_mutex);
        m_customAccentColor = color;
        saveLocked();
    }
}
