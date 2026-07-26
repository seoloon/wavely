# 🚀 PLAN DE TRAVAIL — Wavely
### Développement optimisé avec Claude Code (Skill Superpower)

> **Philosophie du plan :** Chaque étape produit un **livrable compilable et testable**. Aucune étape ne dépasse la capacité d'une session Claude Code. On construit du solide avant d'ajouter du poli.

> **⚠️ Architecture (mise à jour) :** Wavely est scindé en un **backend 100% C++/WinRT** (composant runtime `.winmd`, logique métier + accès système) et un **frontend 100% C#/Avalonia** (UI, consomme le backend via la projection C#/WinRT). Voir `claude\RULES.md` pour le détail des règles par côté de la frontière.
>
> **Statut des Phases 0-3 :** elles ont été **livrées sous l'ancienne architecture** (Qt6/C++ monolithique — tray, GSMTC, drag/resize/click-through, masquage différé, tous fonctionnels et testés dans cet ancien stack). Les descriptions ci-dessous sont réécrites pour la nouvelle architecture ; leur **portage effectif est un chantier séparé**, à planifier avant de reprendre la Phase 4 sur la nouvelle base.

---

## PHASE 0 — Fondations (Session 1)
> *Durée estimée : 1 session | Objectif : Les deux projets compilent, le frontend affiche une donnée venant du backend*

| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 0.1 | **Structure des deux projets** | Backend : `CMakePresets.json`, `src/`, `src/core/`, `src/audio/` (C++, pas d'UI). Frontend : solution .NET Avalonia, `Views/`, `ViewModels/`, `Services/` | `CMakeLists.txt` backend compilable + `dotnet build` frontend compilable |
| 0.2 | **Composant WinRT minimal** | IDL définissant une runtime class triviale (`Wavely.Backend.AppInfo`) exposée et consommée depuis un frontend Avalonia "Hello World" | Fenêtre Avalonia affichant une valeur venant du backend C++ |
| 0.3 | **RAII Wrappers Win32 (backend)** | `src/core/Handle.hpp`, `src/core/ComPtr.hpp`, `src/core/WinrtGuard.hpp` — wrappers template pour HANDLE, IUnknown, etc. | Headers compilables, aucun leak |
| 0.4 | **Configuration Persistante (backend)** | `src/core/AppConfig.hpp/.cpp` — struct `Settings` typée, persistée en JSON (`%AppData%\Wavely\settings.json` via `nlohmann::json`), exposée au frontend via le composant WinRT | Lecture/écriture de la config depuis le frontend |

### ✅ Checkpoint Phase 0
```
Backend compilable  ? → cmake --build build --config Release
Frontend compilable ? → dotnet build
Intégration         ? → le frontend affiche au lancement une donnée venant du backend
```

---

## PHASE 1 — Contrôleur Média GSMTC (Session 2)
> *Durée : 1 session | Objectif : On lit les métadonnées de Spotify/YouTube/Deezer, affichées côté Avalonia*

| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 1.1 | **Wrapper GSMTC (backend, WinRT)** | `Wavely.Backend.MediaSessionManager` — écoute `GlobalSystemMediaTransportControlsSessionManager`, expose des événements WinRT projetés en `event` C# | Events : `TrackChanged`, `PlaybackStateChanged`, `CoverArtReceived` |
| 1.2 | **Extraction métadonnées (backend)** | Runtime class `TrackInfo` (title, artist, album, coverArt en `IBuffer`, duration, isPlaying) — types WinRT-compatibles | Extraction complète via `SMTCMediaProperties` |
| 1.3 | **Couverture (Pochette)** | Backend expose l'artwork en `IRandomAccessStream`/`IBuffer` ; le frontend Avalonia le décode en `Bitmap` | Affichage de la cover dans un contrôle Avalonia de test |

### ✅ Checkpoint Phase 1
```
Test : Lancer Spotify → Lancer Wavely → Vérifier que le titre/artiste/cover s'affichent (UI Avalonia)
Test : Changer de piste → mise à jour automatique
Test : Pause/Play → le bool isPlaying change
```

---

## PHASE 2 — Widget Fenêtre & Interactions (Session 3)
> *Durée : 1 session | Objectif : Le widget se déplace, redimensionne, s'affiche/cache*

| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 2.1 | **Fenêtre Frameless (frontend)** | Avalonia `Window` : `SystemDecorations="None"`, `TransparencyLevelHint` ; complément Win32 (`WS_EX_LAYERED`) via le handle natif si nécessaire | Fenêtre sans chrome Windows |
| 2.2 | **Drag'n'Drop global** | Récupération du handle natif (`TopLevel.TryGetPlatformHandle()`), interception `WM_NCHITTEST` → `HTCAPTION` via P/Invoke. Multi-écrans géré nativement par l'OS | Déplacement sur tous les moniteurs |
| 2.3 | **Redimensionnement** | Logique 50%→150% côté ViewModel, binding sur la taille de fenêtre Avalonia. Snap à la taille "native" du preset | Widget zoom in/out |
| 2.4 | **Click-Through** | Toggle `WS_EX_TRANSPARENT` via P/Invoke sur le handle natif. Bascule via Ctrl+Click ou setting | Le widget devient traversable aux clics |
| 2.5 | **Animation In/Out** | Transition Avalonia (`DoubleTransition` sur `Opacity`, 300ms) | Fade smooth au play/pause |
| 2.6 | **Masquage différé** | `DispatcherTimer` Avalonia configurable (5s→30s) : si pause + toggle actif → fade out après délai | Le widget disparaît en pause |

### ✅ Checkpoint Phase 2
```
Test : Drag sur 2 écrans → OK
Test : Ctrl+Click → toggle click-through → OK
Test : Play → Pause → timer → disparition → Play → réapparition instantanée → OK
```

---

## PHASE 3 — System Tray & Auto-Start (Session 4)
> *Durée : 1 session | Objectif : L'app vit dans la barre des tâches*

| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 3.1 | **System Tray (frontend)** | `Avalonia.Controls.TrayIcon` + menu contextuel (Paramètres, Recharger, Quitter) | Icône dans le tray, menu fonctionnel |
| 3.2 | **Auto-Start (backend)** | `AutoStartManager` (composant WinRT) — écrit/lit `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` via l'API registre Win32 native, wrappée RAII | Toggle "Lancer au démarrage" fonctionnel depuis le frontend |
| 3.3 | **Quitting propre** | Sauvegarde position/taille via le backend à chaque déplacement (Phase 0.4), cleanup COM/WinRT à la fermeture | Pas de crash à la fermeture |

### ✅ Checkpoint Phase 3
```
Test : Fermer la fenêtre widget → l'app reste dans le tray
Test : Quitter via tray → processus se termine proprement
Test : Redémarrer Windows → Wavely relance automatiquement
```

---

## PHASE 4 — Fenêtre Paramètres (Session 5-6)
> *Durée : 2 sessions | Objectif : Tous les settings fonctionnent*

### Session 5 — Structure & Onglet Comportement
| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 4.1 | **Fenêtre settings** | `Window` Avalonia séparée, `TabControl` avec 2 onglets + footer dépannage, `SettingsViewModel` (MVVM) | Fenêtre paramètres ouvrable depuis le tray |
| 4.2 | **Onglet Comportement** | Toggles : Verrouiller, Click-Through, Lancer au démarrage. Slider : Masquage (5-30s). Bouton : Reset taille/position. Sélecteur : Langue (i18n `.resx`) | Tous les toggles mappés au backend `AppConfig` via le ViewModel |

### Session 6 — Onglet Apparence
| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 4.3 | **Section Lecteur** | ComboBox 7 presets (noms placeholder), sélecteur format cover (Carré/Squircle/Vinyle), toggle Glow | Le preset change la disposition |
| 4.4 | **Section Couleurs** | Toggles : Couleurs dynamiques, Fond couleur dominante, Fond cover floutée | Les toggles sont persistés (backend) |
| 4.5 | **Section Opacité** | Slider 0%→100% pour le fond du widget | Changement en temps réel |
| 4.6 | **Section Thème** | Toggle Sombre/Clair via `ThemeVariant` Avalonia | Le thème change sans restart |
| 4.7 | **Bouton Recharger** | Appel backend : reset du hook GSMTC + re-render frontend | Le widget se "recharge" |

### ✅ Checkpoint Phase 4
```
Test : Ouvrir settings → Modifier un paramètre → Vérifier persistance au restart
Test : Thème sombre/clair → changement immédiat
```

---

## PHASE 5 — Waveform Dynamique WASAPI (Session 7-8)
> *Durée : 2 sessions | Objectif : La waveform réagit à l'audio en temps réel*

### Session 7 — Capture Audio (backend)
| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 5.1 | **WASAPI Loopback** | `src/audio/WasapiCapture.hpp/.cpp` — Device enumeration, `IAudioCaptureClient`, buffer circulaire lock-free (SPSC ring buffer) | Audio capture sans click/popping |
| 5.2 | **FFT** | Intégration KissFFT (header-only) ou `FFTCompute` custom. Buffer → fréquences normalisées (60 bands) | Tableau de 60 float [0.0→1.0] en temps réel |
| 5.3 | **Thread Management** | Thread WASAPI dédié, événement WinRT `WaveformDataReady` émis toutes les 16ms (~60fps) avec le buffer de floats | Event consommé côté frontend comme un `event` C# standard |

### Session 8 — Rendu Waveform (frontend)
| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 5.4 | **Contrôle Waveform** | `Control` Avalonia personnalisé, override `Render(DrawingContext)` (barres verticales, courbe lissée) | Waveform animée en temps réel |
| 5.5 | **Binding preset** | Seuls les presets compatibles affichent la waveform. Les autres l'ignorent | Affichage conditionnel |

### ✅ Checkpoint Phase 5
```
Test : Lancer Spotify → Jouer de la musique → Waveform bouge en temps réel
Test : Pause → waveform s'arrête → Play → reprend
Test : CPU usage < 2% au repos, < 5% avec musique
```

---

## PHASE 6 — Couleur Dynamique & Effets Visuels (Session 9-10)
> *Durée : 2 sessions | Objectif : Le widget est visuellement magnifique*

### Session 9 — Extraction de couleur (backend)
| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 6.1 | **Color Quantizer** | `src/core/ColorExtractor.hpp/.cpp` — K-Means (k=5) ou Octree sur la cover réduite (50x50). Thread pool, cache par hash de cover | Palette de 5 couleurs dominantes, exposée via le composant WinRT |
| 6.2 | **Binding UI (frontend)** | Application des couleurs (reçues du backend) aux : progress bar, waveform, accents via des `Brush` Avalonia. Opacité de fond configurable | Le widget change de couleur par piste |

### Session 10 — Effets GPU (frontend)
| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 6.3 | **Fond flouté** | Effet de flou Avalonia (`Effects`) ou rendu Skia custom sur la cover comme fond | Blur gaussien sur le fond |
| 6.4 | **Glow pochette** | `DropShadowEffect` Avalonia avec couleur dominante | Halo lumineux autour de la cover |
| 6.5 | **Squircle** | `Geometry` custom Avalonia (Superellipse : `|x|^n + |y|^n = 1`, n≈5) | Cover en squircle |
| 6.6 | **Vinyle rotatif** | `RotateTransform` + transition Avalonia (360° en ~4s, ease linear) + overlay cercle noir central | Cover ronde qui tourne quand playing |

### ✅ Checkpoint Phase 6
```
Test : Changer de piste → les couleurs changent dynamiquement
Test : Fond flouté visible, glow présent, squircle correct
Test : Vinyle tourne en play, s'arrête en pause
```

---

## PHASE 7 — Les 7 Presets (Session 11-13)
> *Durée : 3 sessions | Objectif : 7 layouts visuels distincts et fonctionnels*

> ⚠️ **Note :** Cette phase est la plus longue car elle est purement **visuelle/artistique**. C'est ici que le portage Rust/Svelte → Avalonia/C# se fait (frontend uniquement, le backend ne change pas). Fournis les **maquettes visuelles** (screenshots, Figma, descriptions pixel-perfect) à Claude Code pour chaque preset.

| Session | Presets | Description |
|---------|---------|-------------|
| 11 | Preset 1-2 | Mini compact (cover + titre) / Barre horizontale (cover + titre + progress) |
| 12 | Preset 3-4 | Waveform centrée / Vinyle + contrôles |
| 13 | Preset 5-7 | Cover glow + waveform / Minimaliste (juste titre) / Full featured (tout) |

### ✅ Checkpoint Phase 7
```
Pour chaque preset :
Test : Sélectionner le preset → rendu correct
Test : Tous les éléments s'affichent (cover, titre, artiste, progress, waveform si applicable)
Test : Changement de piste → mise à jour immédiate
```

---

## PHASE 8 — Polish & Finalisation (Session 14-15)
> *Durée : 2 sessions | Objectif : Qualité production*

### Session 14 — Stabilité
| # | Tâche |
|---|-------|
| 8.1 | Gestion des edge cases : lecteur fermé, cover absente (fallback cover par défaut), track sans titre |
| 8.2 | Crash resilience : try/catch autour de chaque appel WinRT/WASAPI (backend) et autour de chaque appel au composant backend (frontend) |
| 8.3 | Nettoyage mémoire : `Application Verifier`/`AddressSanitizer` côté backend C++, audit des `IDisposable`/handles côté frontend C# |
| 8.4 | Test de charge : 100 changements de piste rapides → pas de leak (ni backend ni frontend) |

### Session 15 — Packaging
| # | Tâche |
|---|-------|
| 8.5 | Icône d'application (tray + window) |
| 8.6 | Versionnage (`Wavely v1.0.0` dans les settings/about) |
| 8.7 | Build Release final : backend statically linked, frontend `dotnet publish -r win-x64 --self-contained` |
| 8.8 | README.md avec screenshots, build instructions, feature list |
