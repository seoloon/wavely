#pragma once

namespace wavely::core::autostart {

/// Whether Wavely is registered to launch with the current Windows session
/// (HKCU\Software\Microsoft\Windows\CurrentVersion\Run).
bool isEnabled();

/// Registers or unregisters Wavely's current executable path for auto-start.
void setEnabled(bool enabled);

} // namespace wavely::core::autostart
