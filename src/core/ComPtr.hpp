#pragma once

#include <winrt/base.h>

namespace wavely::core {

/// RAII smart pointer for classic COM interfaces (IMMDevice, IAudioClient,
/// IAudioCaptureClient, ...). Reuses winrt::com_ptr rather than introducing WRL::ComPtr as a
/// second COM smart pointer type, since C++/WinRT is already a required dependency for GSMTC.
template <typename Interface>
using ComPtr = winrt::com_ptr<Interface>;

} // namespace wavely::core
