#pragma once
#include "AppConfig.g.h"

#include <mutex>

namespace winrt::Wavely::Backend::implementation
{
    /// Loads Wavely's settings from %AppData%\Wavely\settings.json on construction and persists
    /// each change immediately (RULES.md SS5), so a crash never loses the last known geometry or
    /// state. Thread-safe: WinRT calls from the frontend are not guaranteed to land on a single
    /// thread.
    struct AppConfig : AppConfigT<AppConfig>
    {
        AppConfig();

        winrt::Wavely::Backend::WidgetGeometry Geometry();
        bool Locked();
        bool ClickThroughEnabled();
        bool HideOnPauseEnabled();
        std::int32_t HideOnPauseDelaySeconds();
        bool LaunchAtStartup();
        winrt::Wavely::Backend::ThemeMode Theme();
        hstring LanguageCode();
        std::int32_t PresetIndex();
        winrt::Wavely::Backend::CoverStyle CoverShape();
        bool CoverGlowEnabled();
        bool CoverBlurEnabled();
        bool DynamicColorsEnabled();
        bool DynamicBackgroundEnabled();
        double BackgroundOpacity();
        std::uint32_t CustomAccentColor();
        void SetGeometry(winrt::Wavely::Backend::WidgetGeometry const& geometry);
        void SetLocked(bool locked);
        void SetClickThroughEnabled(bool enabled);
        void SetHideOnPauseEnabled(bool enabled);
        void SetHideOnPauseDelaySeconds(std::int32_t seconds);
        void SetLaunchAtStartup(bool enabled);
        void SetTheme(winrt::Wavely::Backend::ThemeMode const& theme);
        void SetLanguageCode(hstring const& languageCode);
        void SetPresetIndex(std::int32_t index);
        void SetCoverShape(winrt::Wavely::Backend::CoverStyle const& shape);
        void SetCoverGlowEnabled(bool enabled);
        void SetCoverBlurEnabled(bool enabled);
        void SetDynamicColorsEnabled(bool enabled);
        void SetDynamicBackgroundEnabled(bool enabled);
        void SetBackgroundOpacity(double opacity);
        void SetCustomAccentColor(std::uint32_t color);

    private:
        void load();
        void saveLocked() const;

        std::mutex m_mutex;
        winrt::Wavely::Backend::WidgetGeometry m_geometry{ 0, 0, 1.0 };
        bool m_locked = false;
        bool m_clickThroughEnabled = false;
        bool m_hideOnPauseEnabled = false;
        std::int32_t m_hideOnPauseDelaySeconds = 10;
        bool m_launchAtStartup = false;
        winrt::Wavely::Backend::ThemeMode m_theme = winrt::Wavely::Backend::ThemeMode::Dark;
        hstring m_languageCode = L"fr";
        std::int32_t m_presetIndex = 0;
        winrt::Wavely::Backend::CoverStyle m_coverShape = winrt::Wavely::Backend::CoverStyle::Square;
        bool m_coverGlowEnabled = false;
        bool m_coverBlurEnabled = false;
        bool m_dynamicColorsEnabled = true;
        bool m_dynamicBackgroundEnabled = false;
        double m_backgroundOpacity = 1.0;
        std::uint32_t m_customAccentColor = 0xDC5AAAFFu;
    };
}
namespace winrt::Wavely::Backend::factory_implementation
{
    struct AppConfig : AppConfigT<AppConfig, implementation::AppConfig>
    {
    };
}
