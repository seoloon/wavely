#pragma once

#include <winrt/base.h>

namespace wavely::core {

/// Initializes the COM/WinRT apartment for the current thread on construction and guarantees
/// symmetric teardown on destruction, including when the stack unwinds via an exception.
/// Single-threaded: the app drives WinRT calls from the Qt UI thread's message loop.
class WinrtApartmentGuard {
public:
    WinrtApartmentGuard() {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
    }

    ~WinrtApartmentGuard() {
        winrt::uninit_apartment();
    }

    WinrtApartmentGuard(const WinrtApartmentGuard&) = delete;
    WinrtApartmentGuard& operator=(const WinrtApartmentGuard&) = delete;
    WinrtApartmentGuard(WinrtApartmentGuard&&) = delete;
    WinrtApartmentGuard& operator=(WinrtApartmentGuard&&) = delete;
};

} // namespace wavely::core
