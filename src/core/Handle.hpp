#pragma once

#include <Windows.h>

#include <memory>

namespace wavely::core {

/// Deleter for Win32 HANDLE objects closed via CloseHandle (events, threads, files opened
/// with CreateFile, mutexes, etc.). Handles nullptr and INVALID_HANDLE_VALUE safely.
struct HandleDeleter {
    using pointer = HANDLE;

    void operator()(HANDLE handle) const noexcept {
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }
};

/// RAII ownership of a Win32 HANDLE. Never manipulate a raw HANDLE without wrapping it here.
using UniqueHandle = std::unique_ptr<HANDLE, HandleDeleter>;

} // namespace wavely::core
