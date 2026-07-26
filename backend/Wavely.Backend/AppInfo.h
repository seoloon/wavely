#pragma once
#include "AppInfo.g.h"

namespace winrt::Wavely::Backend::implementation
{
    struct AppInfo : AppInfoT<AppInfo>
    {
        AppInfo() = default;

        static hstring GetGreeting();
        hstring Version();
    };
}
namespace winrt::Wavely::Backend::factory_implementation
{
    struct AppInfo : AppInfoT<AppInfo, implementation::AppInfo>
    {
    };
}
