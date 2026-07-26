#pragma once

#include <winrt/base.h>

namespace Wavely::Backend::Core
{
    /// Initializes the COM/WinRT apartment for the current thread on construction and guarantees
    /// symmetric teardown on destruction, including when the stack unwinds via an exception.
    /// The frontend's UI thread already runs in an STA (see Program.cs [STAThread]) and .NET's
    /// WinRT interop initializes it as needed, so this guard is only for backend worker threads
    /// that call into WinRT/COM APIs outside the default thread pool (which initializes its own
    /// MTA implicitly).
    class WinrtApartmentGuard
    {
    public:
        WinrtApartmentGuard()
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
        }

        ~WinrtApartmentGuard()
        {
            winrt::uninit_apartment();
        }

        WinrtApartmentGuard(const WinrtApartmentGuard&) = delete;
        WinrtApartmentGuard& operator=(const WinrtApartmentGuard&) = delete;
        WinrtApartmentGuard(WinrtApartmentGuard&&) = delete;
        WinrtApartmentGuard& operator=(WinrtApartmentGuard&&) = delete;
    };
}
