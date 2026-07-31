# Auto-updates Velopack via GitHub Releases — Design

> Fonctionnalité hors plan de phase existant (`claude/PLAN.md` 8.6 ne couvre que l'affichage de
> version, pas l'auto-update). Demandée directement par l'utilisateur.
>
> État avant cette session : `frontend/Wavely.App` référence déjà `Velopack` 1.2.0 et appelle
> `VelopackApp.Build().Run()` en première ligne de `Program.Main()` (Task 28, packaging local via
> `package.ps1` → `Wavely-win-Setup.exe`). **Aucun code d'update runtime n'existe** (pas
> d'`UpdateManager`/`GithubSource` dans le code), **aucun `.github/workflows/`**, **aucune release
> GitHub publiée**. Le dépôt cible est public : `https://github.com/seoloon/wavely`.

## Décisions actées avec l'utilisateur

- **Déclenchement** : check silencieux automatique au démarrage (télécharge en tâche de fond si
  une mise à jour existe) **et** bouton "Check for Updates" manuel dans Settings — les deux
  cohabitent, pas l'un ou l'autre.
- **Pipeline de publication** : script local uniquement (`release.ps1`), pas de GitHub Actions.
  L'utilisateur publie lui-même depuis sa machine avec un `GITHUB_TOKEN` personnel.
- **UI** : nouvel onglet "About" dans `SettingsWindow` (version courante, bouton Check for
  Updates, texte de statut) **+** item de menu tray "Restart to Update" qui n'apparaît que
  lorsqu'une mise à jour est prête. Pas de toast/notification Windows.
- **Visibilité des releases** : `vpk upload github` sans `--publish` par défaut → release créée en
  **draft** sur GitHub, publiée manuellement par l'utilisateur une fois satisfait. `release.ps1`
  expose un `-Publish` pour publier directement quand souhaité.

## Architecture / flux de données

```
Démarrage app                    Services/UpdateService.cs           github.com/seoloon/wavely
──────────────                   ──────────────────────────          ─────────────────────────
App.axaml.cs (post-fenêtre) --> CheckAndDownloadAsync()  -- GithubSource -->  Releases (RELEASES
                                   |  (Velopack.UpdateManager)                feed, *-full.nupkg,
                                   v                                          Setup.exe)
                                 UpdateReady event
                                   |
                    ┌──────────────┴──────────────┐
                    v                              v
        SettingsWindow "About" tab        AppTrayIcon "Restart to Update"
        (bouton manuel + statut)          (item caché par défaut, visible
                                            au fire de UpdateReady)
```

`UpdateService` est le seul point de contact avec l'API Velopack — même principe que
`AutoStartManager`/`MediaSessionManager` : une classe, une responsabilité externe, gérée en un
seul endroit.

## `Services/UpdateService.cs`

Enveloppe `Velopack.UpdateManager`, construit une fois avec
`new GithubSource("https://github.com/seoloon/wavely", null, false)` — pas de token en lecture
(dépôt public), pas de pre-releases (`false`).

API exposée (vérifiée contre l'assembly `Velopack.dll` 1.2.0 réellement installée, pas supposée de
mémoire — voir noms réels ci-dessous) :

```csharp
public sealed class UpdateService
{
    public bool IsInstalled { get; }              // UpdateManager.IsInstalled
    public string? CurrentVersion { get; }         // UpdateManager.CurrentVersion, null si !IsInstalled

    public event EventHandler? UpdateReady;

    public Task<UpdateCheckResult> CheckAndDownloadAsync();  // CheckForUpdatesAsync + DownloadUpdatesAsync
    public void ApplyAndRestart();                            // ApplyUpdatesAndRestart(updateInfo)
}

public enum UpdateCheckResult { NotInstalled, UpToDate, Downloading, Ready, Failed }
```

Symboles Velopack réels confirmés présents dans `Velopack.dll` (net8.0) : `UpdateManager`,
`GithubSource`, `IUpdateSource`, `CheckForUpdatesAsync`, `DownloadUpdatesAsync`,
`ApplyUpdatesAndRestart`, `WaitExitThenApplyUpdates`, `get_IsInstalled`, `get_CurrentVersion`,
`UpdateInfo`, `VelopackAsset`.

Toute exception Velopack (réseau, GitHub indisponible, etc.) est catchée **dans**
`CheckAndDownloadAsync` et convertie en `UpdateCheckResult.Failed` — frontière unique
d'exception-handling (RULES.md §4), jamais propagée à l'appelant ni au check silencieux du
démarrage (qui ne doit jamais planter l'app).

## Déclenchement au démarrage

`App.axaml.cs`, après construction de la fenêtre principale :
```csharp
_ = _updateService.CheckAndDownloadAsync();
```
Fire-and-forget, non bloquant, silencieux sauf si une mise à jour est trouvée (auquel cas
`UpdateReady` se déclenche et pilote la tray + le tab About). Pas de re-check périodique pendant
l'exécution — uniquement au lancement.

## UI — onglet "About" (`SettingsWindow.axaml` + `SettingsViewModel.About.cs`)

- `CurrentVersionText` : `"Wavely v{UpdateService.CurrentVersion}"`, ou un texte "build de
  développement (non installée)" si `!IsInstalled` — jamais vide/trompeur en `dotnet run`.
- `UpdateStatusText` : reflète directement l'état d'`UpdateService` ("À jour" / "Vérification…" /
  "Téléchargement…" / "Mise à jour prête — redémarrer pour appliquer" / "Échec de la
  vérification").
- `CheckForUpdatesCommand` : appelle `CheckAndDownloadAsync()`, désactivé pendant qu'un check est
  déjà en cours.
- `RestartToUpdateCommand` : visible/actif uniquement après `UpdateReady`, appelle
  `ApplyAndRestart()`.

## UI — tray (`AppTrayIcon.cs`)

Nouveau `NativeMenuItem` "Restart to Update" inséré après "Reload widget", `IsVisible = false` par
défaut. `AppTrayIcon` reçoit `UpdateService` en constructeur, s'abonne à `UpdateReady` pour rendre
l'item visible et brancher son `Click` sur `ApplyAndRestart()` — même pattern que
`RefreshLaunchAtStartup()` (état externe reflété sur un menu non-bindable).

## `release.ps1` (nouveau script, au-dessus de `package.ps1`)

Même principe de layering que `build.ps1` → `package.ps1` : un script de plus, une responsabilité
de plus (publication GitHub), ne réimplémente rien du build/packaging existant.

```powershell
param(
    [Parameter(Mandatory)][string]$Version,   # ex. '0.2.0' -> tag 'v0.2.0'
    [switch]$Publish,                          # omis = draft GitHub (défaut)
    [switch]$PreRelease
)
$ErrorActionPreference = 'Stop'
$repoRoot = $PSScriptRoot

if (-not $env:GITHUB_TOKEN) { throw "GITHUB_TOKEN env var not set - needed to upload to GitHub Releases." }

& "$repoRoot\package.ps1" -Version $Version
if ($LASTEXITCODE -ne 0) { throw "Packaging failed." }

vpk upload github `
    --outputDir "$repoRoot\dist\Releases" `
    --repoUrl "https://github.com/seoloon/wavely" `
    --token $env:GITHUB_TOKEN `
    --tag "v$Version" `
    --releaseName "Wavely v$Version" `
    --publish $($Publish.IsPresent) `
    --pre $($PreRelease.IsPresent)
if ($LASTEXITCODE -ne 0) { throw "vpk upload failed." }
```

Flags `vpk upload github` vérifiés en direct contre le CLI 1.2.0 réellement installé
(`vpk upload github --help`) : `--outputDir`, `--channel` (défaut `win`, non surchargé ici),
`--repoUrl` (requis), `--token`, `--timeout`, `--publish`, `--pre`, `--merge`, `--releaseName`,
`--tag`, `--targetCommitish`. `GITHUB_TOKEN` lu depuis l'environnement, jamais écrit sur disque ni
committé — PAT GitHub avec scope `repo` (nécessaire même pour créer un draft).

`$ErrorActionPreference = 'Stop'` + `throw` explicite sur token manquant / exit code non-nul,
même convention que `package.ps1`.

## Plan de test

Nécessairement manuel pour la majeure partie (API GitHub réelle, état "application installée",
redémarrage réel) :

1. Lancer en dev (`dotnet run`/`build.ps1`) : l'onglet About affiche l'état "build de
   développement", "Check for Updates" ne plante pas (`IsInstalled == false`, no-op propre).
2. `package.ps1 -Version 0.1.0`, installer le `Setup.exe` produit localement : l'onglet About
   affiche `v0.1.0`, `IsInstalled == true`.
3. `release.ps1 -Version 0.1.1` (draft), publier le draft manuellement sur GitHub, relancer
   l'app installée en `0.1.0` : le check au démarrage trouve `0.1.1`, le télécharge, l'item tray
   apparaît, "Restart to Update" relance bien en `0.1.1`.
4. Couper le réseau ou viser un dépôt invalide : `UpdateStatusText` passe à "Échec de la
   vérification" sans crash de l'app.
