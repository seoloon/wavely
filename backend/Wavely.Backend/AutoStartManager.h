#pragma once
#include "AutoStartManager.g.h"

namespace winrt::Wavely::Backend::implementation
{
    /// Registers or unregisters Wavely's current executable path for auto-start via
    /// HKCU\Software\Microsoft\Windows\CurrentVersion\Run, using the native Win32 registry API
    /// (RULES.md SS5) rather than a settings library.
    struct AutoStartManager : AutoStartManagerT<AutoStartManager>
    {
        AutoStartManager() = default;

        static bool IsEnabled();
        static void SetEnabled(bool enabled);
    };
}
namespace winrt::Wavely::Backend::factory_implementation
{
    struct AutoStartManager : AutoStartManagerT<AutoStartManager, implementation::AutoStartManager>
    {
    };
}
