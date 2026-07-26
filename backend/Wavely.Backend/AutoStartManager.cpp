#include "pch.h"
#include "AutoStartManager.h"
#include "AutoStartManager.g.cpp"

#include "Core/RegistryKey.h"

namespace winrt::Wavely::Backend::implementation
{
    namespace
    {
        constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
        constexpr wchar_t kValueName[] = L"Wavely";

        ::Wavely::Backend::Core::UniqueRegistryKey openRunKey(REGSAM accessRights)
        {
            HKEY rawKey = nullptr;
            const LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, accessRights, &rawKey);
            if (status != ERROR_SUCCESS)
            {
                return {};
            }
            return ::Wavely::Backend::Core::UniqueRegistryKey(rawKey);
        }

        std::wstring currentExecutablePath()
        {
            wchar_t buffer[MAX_PATH];
            const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            winrt::check_bool(length > 0 && length < MAX_PATH);
            return std::wstring(buffer, length);
        }
    }

    bool AutoStartManager::IsEnabled()
    {
        const auto key = openRunKey(KEY_READ);
        if (!key)
        {
            return false;
        }
        const LSTATUS status = RegQueryValueExW(key.get(), kValueName, nullptr, nullptr, nullptr, nullptr);
        return status == ERROR_SUCCESS;
    }

    void AutoStartManager::SetEnabled(bool enabled)
    {
        if (enabled)
        {
            HKEY rawKey = nullptr;
            const LSTATUS createStatus = RegCreateKeyExW(
                HKEY_CURRENT_USER, kRunKeyPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &rawKey, nullptr);
            winrt::check_win32(static_cast<DWORD>(createStatus));
            const ::Wavely::Backend::Core::UniqueRegistryKey key(rawKey);

            const std::wstring quotedPath = L"\"" + currentExecutablePath() + L"\"";
            const LSTATUS setStatus = RegSetValueExW(
                key.get(), kValueName, 0, REG_SZ,
                reinterpret_cast<const BYTE*>(quotedPath.c_str()),
                static_cast<DWORD>((quotedPath.size() + 1) * sizeof(wchar_t)));
            winrt::check_win32(static_cast<DWORD>(setStatus));
        }
        else
        {
            const auto key = openRunKey(KEY_SET_VALUE);
            if (key)
            {
                RegDeleteValueW(key.get(), kValueName);
            }
        }
    }
}
