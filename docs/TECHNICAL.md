# Wavely — Documentation technique

> **Deux arbres de code coexistent pendant la migration** (voir `docs/ADR-001-cpp-backend-csharp-frontend.md`) :
> - `backend/` + `frontend/` — **nouvelle architecture cible** (C++/WinRT + C#/Avalonia), en cours de construction phase par phase (`claude/PLAN.md`). Documentée ci-dessous en premier.
> - `src/` + `CMakeLists.txt` racine — **implémentation legacy Qt6/C++ Widgets**, seule version fonctionnelle de bout en bout pour l'instant. Documentée en fin de fichier, retirée une fois le portage des Phases 0-3 achevé.

## Nouvelle architecture — Backend C++/WinRT + Frontend C#/Avalonia

### Stack

- Backend : C++20, MSVC 2022 (toolset v143), Windows SDK 10.0.22621+, C++/WinRT via le package NuGet `Microsoft.Windows.CppWinRT` (voir `docs/ADR-002-winrt-component-msbuild.md` — build MSBuild, pas CMake, pour ce composant spécifiquement).
- Frontend : .NET 8+ (TFM `net8.0-windows10.0.19041.0`), Avalonia UI 11.3.x, `Microsoft.Windows.CsWinRT` pour la projection C# du composant backend.

### Prérequis machine

- **Visual Studio 2022** avec le workload *Desktop development with C++* (MSVC v143 + Windows SDK).
- **.NET 8 SDK** — non installé par défaut avec Visual Studio ; installer via [dotnet.microsoft.com](https://dotnet.microsoft.com/download/dotnet/8.0) ou `winget install Microsoft.DotNet.SDK.8`. **Nécessaire pour compiler `frontend/`** — sans lui, seul `backend/` est buildable.
- Le package NuGet `Microsoft.Windows.CppWinRT` n'est **pas commité** (voir `.gitignore`) : exécuter `backend\restore-packages.ps1` une fois pour le télécharger dans `backend/packages/` (pas de VSIX C++/WinRT requis, pas de `nuget.exe` ni de SDK .NET nécessaires pour cette étape).

### Build

```
.\backend\restore-packages.ps1   # une seule fois
.\build.ps1
```

Ou séparément :
- Backend seul : `msbuild backend\Wavely.Backend\Wavely.Backend.vcxproj /p:Configuration=Debug /p:Platform=x64` — produit `backend\Wavely.Backend\build\bin\Debug\Wavely.Backend.{dll,winmd}`.
- Frontend (build aussi le backend via `ProjectReference`) : `dotnet build frontend\Wavely.App\Wavely.App.csproj -c Debug` — nécessite le .NET 8 SDK.

### Structure du projet (nouvelle architecture)

```
backend/Wavely.Backend/   Composant WinRT C++ (.idl, implémentation, packages/ vendorisé)
frontend/Wavely.App/      Application Avalonia C# (Views/, App.axaml, projection CsWinRT générée au build)
```

### Décisions architecturales (nouvelles)

- **Backend WinRT construit via MSBuild, pas CMake** : voir `docs/ADR-002-winrt-component-msbuild.md` — CMake n'a pas de chemin fiable pour générer un `.winmd` depuis un IDL C++/WinRT.
- **Package `Microsoft.Windows.CppWinRT` vendorisé** dans `backend/packages/` (téléchargé et extrait manuellement) plutôt que restauré via NuGet/VS, car cette machine de dev n'a ni `nuget.exe` ni le SDK .NET pour piloter une restauration `PackageReference` côté `.vcxproj`.
- **Le frontend consomme le `.winmd` du backend via `<CsWinRTInputs>`**, pas `<ProjectReference>` ni `<Reference><HintPath>` — voir l'addendum de `docs/ADR-002-winrt-component-msbuild.md` pour pourquoi (les deux autres approches cassent le build).

### ⚠️ Checklist en ajoutant une nouvelle runtime class WinRT

Toute nouvelle `runtimeclass` côté backend (nouveau `.idl` ou ajout à un `.idl`
existant) nécessite ces étapes, sinon elle compile mais lève `REGDB_E_CLASSNOTREG`
/ `CO_E_ERRORINDLL` à l'exécution (voir addendum `docs/ADR-002-...md`) :

1. Ajouter le `.idl` à `<Midl Include="..." />` dans `Wavely.Backend.vcxproj`.
2. Après un premier build (qui échoue au link, c'est normal), copier le stub
   généré depuis `build\Wavely.Backend\<Config>\Generated Files\sources\` vers
   la racine du projet et l'implémenter (voir les classes existantes).
3. Ajouter une entrée `<activatableClass name="Wavely.Backend.MaClasse" .../>`
   dans `frontend/Wavely.App/app.manifest` (le fichier `.def` n'a besoin
   d'aucun changement — il exporte `DllGetActivationFactory` une fois pour
   toutes les classes du module).

---

## Implémentation legacy — Qt6/C++ Widgets (`src/`)

### Stack

- C++20, CMake 3.24+, Qt 6.5+ (Core/Gui/Widgets), C++/WinRT (GSMTC) et WASAPI (loopback).
- MSVC 2022 (toolset v143). Voir `claude/RULES.md` pour l'ensemble des conventions.

### Prérequis machine (legacy)

- **Visual Studio 2022** avec le workload *Desktop development with C++* (fournit MSVC v143 et le Windows SDK).
- **CMake 3.24+** — non fourni par défaut avec Visual Studio ; installer séparément (`winget install Kitware.CMake`) ou cocher le composant *C++ CMake tools for Windows* dans le Visual Studio Installer.
- **Qt 6.5+** (composants Core, Gui, Widgets, kit `msvc2022_64`) — installer via le [Qt Online Installer](https://www.qt.io/download-qt-installer) ou `aqtinstall`.

### Configuration de l'environnement (legacy)

Définir la variable d'environnement `QT6_DIR` pointant vers le dossier du kit MSVC de l'installation Qt, par ex. :

```
setx QT6_DIR "C:\Qt\6.7.0\msvc2022_64"
```

### Build (legacy)

```
cmake --preset windows-msvc
cmake --build build --config Release
```

Le binaire est généré dans `build/Release/Wavely.exe` (ou `build/Debug/`, `build/RelWithDebInfo/` selon le preset de build utilisé : `debug`, `release`, `profile`).

### Structure du projet (legacy)

```
src/
  core/      RAII wrappers Win32/WinRT (Handle, ComPtr, WinrtGuard), pas de dépendance Qt
  ui/        Fenêtres et widgets Qt (overlay, settings, waveform)
  audio/     Capture WASAPI loopback et traitement du signal
  settings/  Persistance de la configuration (AppConfig / QSettings)
resources/   Icônes, assets embarqués
```

### Décisions architecturales (legacy, historiques)

- **Qt6/Widgets plutôt que WinUI3/XAML** : le brief initial (`claude/PROMPT.md`) envisageait déjà une scission WinRT/frontend, mais au moment de cette implémentation les règles projet figeaient temporairement la stack sur Qt6/Widgets monolithique. Ce choix est **remplacé** par la scission C++/WinRT + C#/Avalonia actée dans `docs/ADR-001-cpp-backend-csharp-frontend.md` ; cette section est conservée à titre historique tant que `src/` n'est pas retiré.
- **QSettings (registre HKCU) plutôt que JSON** pour les préférences utilisateur simples (géométrie, thème, comportement). La nouvelle architecture revient à un fichier JSON (`RULES.md` §5), porté lors du remplacement de `src/settings/AppConfig`.
