# Wavely — Documentation technique

## Stack

- C++20, CMake 3.24+, Qt 6.5+ (Core/Gui/Widgets), C++/WinRT (GSMTC) et WASAPI (loopback).
- MSVC 2022 (toolset v143). Voir `claude/RULES.md` pour l'ensemble des conventions.

## Prérequis machine

- **Visual Studio 2022** avec le workload *Desktop development with C++* (fournit MSVC v143 et le Windows SDK).
- **CMake 3.24+** — non fourni par défaut avec Visual Studio ; installer séparément (`winget install Kitware.CMake`) ou cocher le composant *C++ CMake tools for Windows* dans le Visual Studio Installer.
- **Qt 6.5+** (composants Core, Gui, Widgets, kit `msvc2022_64`) — installer via le [Qt Online Installer](https://www.qt.io/download-qt-installer) ou `aqtinstall`.

## Configuration de l'environnement

Définir la variable d'environnement `QT6_DIR` pointant vers le dossier du kit MSVC de l'installation Qt, par ex. :

```
setx QT6_DIR "C:\Qt\6.7.0\msvc2022_64"
```

## Build

```
cmake --preset windows-msvc
cmake --build build --config Release
```

Le binaire est généré dans `build/Release/Wavely.exe` (ou `build/Debug/`, `build/RelWithDebInfo/` selon le preset de build utilisé : `debug`, `release`, `profile`).

## Structure du projet

```
src/
  core/      RAII wrappers Win32/WinRT (Handle, ComPtr, WinrtGuard), pas de dépendance Qt
  ui/        Fenêtres et widgets Qt (overlay, settings, waveform)
  audio/     Capture WASAPI loopback et traitement du signal
  settings/  Persistance de la configuration (AppConfig / QSettings)
resources/   Icônes, assets embarqués
```

## Décisions architecturales

- **Qt6/Widgets plutôt que WinUI3/XAML** : le brief initial (`claude/PROMPT.md`) envisageait WinUI3, mais les règles projet (`claude/RULES.md`) et le plan (`claude/PLAN.md`) figent la stack sur Qt6. WinRT reste utilisé uniquement pour GSMTC (métadonnées lecture) et WASAPI (capture audio), pas pour l'UI.
- **QSettings (registre HKCU) plutôt que JSON** pour les préférences utilisateur simples (géométrie, thème, comportement), conformément à `RULES.md` §5. Un fichier JSON externe reste prévu pour les presets (layouts), qui doivent rester éditables sans recompilation.
