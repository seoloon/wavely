# Build — Wavely

Tous les scripts de build vivent à la racine du repo. Aucun n'est à chercher ailleurs.

| Script | Rôle |
|---|---|
| `restore-packages.ps1` | Télécharge `Microsoft.Windows.CppWinRT` (une seule fois) |
| `build.ps1` | Compile backend (C++/WinRT) + frontend (Avalonia) — usage quotidien |
| `package.ps1` | Produit l'installateur `.exe` (Velopack) |
| `release.ps1` | Publie un installateur sur GitHub Releases |

## Prérequis

- **Visual Studio 2022**, workload *Desktop development with C++* (MSVC v143 + Windows SDK)
- **.NET 8 SDK** ([dotnet.microsoft.com](https://dotnet.microsoft.com/download/dotnet/8.0) ou `winget install Microsoft.DotNet.SDK.8`)
- Pour `package.ps1`/`release.ps1` : le CLI Velopack — `dotnet tool install -g vpk`

## Build de dev

```powershell
.\restore-packages.ps1        # une seule fois, restaure backend/packages/
.\build.ps1                   # -Configuration Debug (défaut) ou Release
```

Produit :
- Backend : `backend\Wavely.Backend\build\bin\<Config>\Wavely.Backend.{dll,winmd}`
- Frontend : `frontend\Wavely.App\bin\<Config>\net8.0-windows10.0.19041.0\Wavely.App.exe`

Pour lancer l'app après build : exécuter directement le `.exe` ci-dessus.

## Installateur (`.exe` de distribution)

```powershell
.\package.ps1 -Version 0.2.0
```

Build en Release, publie le frontend en self-contained win-x64, puis pack avec Velopack.
Produit `dist\Releases\Wavely-win-Setup.exe` (+ `*-full.nupkg`, `*-Portable.zip`, manifests
`RELEASES`/`*.json` — tous ignorés par git, `dist/` n'est jamais committé).

## Publier une release sur GitHub

```powershell
$env:GITHUB_TOKEN = '<PAT avec scope repo>'
.\release.ps1 -Version 0.2.0            # -> draft sur github.com/seoloon/wavely/releases
.\release.ps1 -Version 0.2.0 -Publish   # -> publiée immédiatement (auto-update la récupère)
```

Par défaut la release est créée en **draft** — rien n'atteint les auto-updaters tant qu'elle
n'est pas publiée manuellement sur GitHub (ou via `-Publish`). `-PreRelease` marque la release
comme pré-version.

`GITHUB_TOKEN` n'est jamais écrit sur disque ni committé — variable d'environnement uniquement,
à définir dans le shell avant d'appeler le script.

## Ordre de dépendance

```
restore-packages.ps1 → build.ps1 → package.ps1 → release.ps1
```

Chaque script appelle le précédent (`build.ps1` appelle `restore-packages.ps1`, `package.ps1`
appelle `build.ps1`, `release.ps1` appelle `package.ps1`) — inutile de les enchaîner à la main.

Détails techniques (pourquoi MSBuild et pas CMake pour le backend, pourquoi `<CsWinRTInputs>`
plutôt que `<ProjectReference>`, etc.) : voir `docs/TECHNICAL.md`.
