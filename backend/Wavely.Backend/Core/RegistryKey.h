#pragma once

#include <Windows.h>

#include <memory>

namespace Wavely::Backend::Core
{
    /// Deleter for HKEY handles opened via RegOpenKeyEx/RegCreateKeyEx, closed via RegCloseKey.
    struct RegistryKeyDeleter
    {
        using pointer = HKEY;

        void operator()(HKEY key) const noexcept
        {
            if (key != nullptr)
            {
                RegCloseKey(key);
            }
        }
    };

    /// RAII ownership of an HKEY. Never manipulate a raw HKEY without wrapping it here.
    using UniqueRegistryKey = std::unique_ptr<HKEY, RegistryKeyDeleter>;
}
