#include "pch.h"
#include "AppInfo.h"
#include "AppInfo.g.cpp"

namespace winrt::Wavely::Backend::implementation
{
    constexpr std::wstring_view kBackendVersion = L"0.1.0";

    hstring AppInfo::GetGreeting()
    {
        return L"Wavely backend (C++/WinRT) is alive.";
    }

    hstring AppInfo::Version()
    {
        return hstring{ kBackendVersion };
    }
}
