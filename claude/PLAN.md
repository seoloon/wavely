# 🚀 PLAN DE TRAVAIL — Wavely
### Développement optimisé avec Claude Code (Skill Superpower)

> **Philosophie du plan :** Chaque étape produit un **livrable compilable et testable**. Aucune étape ne dépasse la capacité d'une session Claude Code. On construit du solide avant d'ajouter du poli.

> **⚠️ Architecture (mise à jour) :** Wavely est scindé en un **backend 100% C++/WinRT** (composant runtime `.winmd`, logique métier + accès système) et un **frontend 100% C#/Avalonia** (UI, consomme le backend via la projection C#/WinRT). Voir `claude\RULES.md` pour le détail des règles par côté de la frontière.
>
> **Statut des Phases 0-3 :** elles ont été **livrées sous l'ancienne architecture** (Qt6/C++ monolithique — tray, GSMTC, drag/resize/click-through, masquage différé, tous fonctionnels et testés dans cet ancien stack). Les descriptions ci-dessous sont réécrites pour la nouvelle architecture ; leur **portage effectif est un chantier séparé**, à planifier avant de reprendre la Phase 4 sur la nouvelle base.

---

## PHASE 0 — Fondations (Session 1)
> *Durée estimée : 1 session | Objectif : Les deux projets compilent, le frontend affiche une donnée venant du backend*

> **Statut (2026-07-26) :** 0.1/0.2 livrés et vérifiés — `backend/Wavely.Backend` (composant WinRT, runtime class `AppInfo`) compile avec MSBuild sans warning (`/W4 /WX`) et produit `Wavely.Backend.{dll,winmd}` (voir `docs/ADR-002-winrt-component-msbuild.md` pour le choix MSBuild plutôt que CMake). `frontend/Wavely.App` (Avalonia, consomme `AppInfo` via `Microsoft.Windows.CsWinRT`) est scaffoldé mais **non vérifié** : le SDK .NET 8 n'est pas installé sur la machine de dev actuelle (voir `docs/TECHNICAL.md`). 0.3 (RAII wrappers) et 0.4 (AppConfig JSON) restent à porter depuis `src/core`/`src/settings` — non commencés.

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

> **Statut (2026-07-26) : livré et vérifié en conditions réelles.** `Wavely.Backend.MediaSessionManager`/`TrackInfo` implémentés et testés en lançant réellement `Wavely.App.exe` : une session GSMTC active (vidéo lue dans le navigateur, faute de Spotify sur la machine de dev) a été détectée automatiquement, avec cover/titre/statut "Playing" affichés dans le widget Avalonia — capture d'écran à l'appui. Changement de piste / pause non testés isolément (pas de lecteur permettant de déclencher ces transitions à la demande sur cette machine), mais le code suit le même chemin d'événements que l'affichage initial.
>
> **Suivi (2026-07-26, plus tard en session) : filtrage par liste blanche d'apps musicales.** `GetCurrentSession()` (qui prend "la session la plus récemment active", sans notion d'app musicale) remplacé par `selectWhitelistedSession()` : itère `GetSessions()`, ne retient que les apps de streaming natif whitelistées (`Core/MusicAppAllowlist.h` : Spotify, Deezer, TIDAL, Apple Music — YouTube Music explicitement exclu, site web indiscernable de "n'importe quelle vidéo sur un site" via GSMTC), préfère une session `Playing`. Demande explicite utilisateur, confirmée nécessaire en conditions réelles : avec Spotify et Brave actifs simultanément, `GetCurrentSession()` retournait la session Brave alors que Spotify jouait. Voir `docs/ADR-004-music-app-whitelist-and-process-loopback.md`.

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

> **Statut (2026-07-26) : livré et vérifié par interaction réelle (entrée souris/clavier synthétique via SendInput/mouse_event/keybd_event Win32, pas seulement compilation).** 2.1-2.4 vérifiés : drag (WM_NCHITTEST/HTCAPTION) déplace bien la fenêtre et persiste la position dans `settings.json` ; molette redimensionne exactement (360×120 → 468×156 à l'échelle 1.3) et clampe pile à 0.5 et 1.5 ; Ctrl+Click active le click-through (bit `WS_EX_TRANSPARENT` posé, vérifié via `GetWindowLong`, position persistée) et fait apparaître le handle "safe zone" à la position attendue ; cliquer le handle désactive le click-through (bit retiré, handle recaché). 2.5 (fade) vérifié indirectement via le fade-in au démarrage (Phase 1). **Non testé** : 2.6 (masquage différé sur pause) — aucun lecteur média disponible sur cette machine pour déclencher un vrai événement pause à la demande ; le code suit le même mécanisme `DispatcherTimer`/`Transitions` déjà vérifié pour le fade-in.

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

> **Statut (2026-07-26) : `AutoStartManager` vérifié en isolation, `AppTrayIcon` non vérifié interactivement.** `AutoStartManager.IsEnabled()`/`SetEnabled()` testés via un harnais C# jetable consommant le même `Wavely.Backend.dll` : `SetEnabled(true)` écrit bien `HKCU\...\Run\Wavely` = `REG_SZ` `"<chemin.exe>"` (vérifié indépendamment via `reg query`, pas seulement en relisant via `IsEnabled()`), `SetEnabled(false)` la supprime. **Non testé** : clic réel sur l'icône tray (positionnement pixel-exact d'une icône system tray peu fiable à automatiser sans UI Automation dédiée), et donc le chemin `AppTrayIcon.OnClicked`/menu "Launch at startup"/"Quit" bout-en-bout — la logique de ces handlers est revue par code et appelle exactement les mêmes méthodes `AutoStartManager`/`MediaSessionManager.Refresh()`/`desktop.Shutdown()` déjà vérifiées ou triviales.

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

> **Statut (2026-07-26) : Session 5 (4.1-4.2) livrée et vérifiée par interaction réelle** (UI Automation `TogglePattern`/`InvokePattern`, pas seulement compilation) : `SettingsWindow` + `SettingsViewModel` (`CommunityToolkit.Mvvm`), i18n via `Resources/Strings.resx` (RULES.md SS6 - wrapper `Strings.cs` écrit à la main plutôt que le générateur Visual Studio `ResXFileCodeGenerator`, qui ne tourne qu'en design-time IDE et pas depuis `dotnet build`/`MSBuild.exe` en ligne de commande). Vérifié : cocher "Lancer au démarrage" écrit bien la clé registre (confirmé par `reg query` indépendant) ; "Réinitialiser la taille" persiste ET **met à jour le widget en direct** sans redémarrage (mécanisme `SettingsViewModel.ConfigChanged` → `MainWindow.RefreshFromConfig()`, nécessaire car Settings et le widget sont deux fenêtres qui ne partagent pas de ViewModel) ; cocher "Click-through" applique bien `WS_EX_TRANSPARENT` en direct sur le widget. Le bouton "Recharger le widget" (4.7, en avance sur la Session 6) est aussi dans le footer. Le menu tray "Paramètres..." (précédemment désactivé) ouvre maintenant cette fenêtre. Écart mineur avec ce tableau : "Reset taille/position" n'a été implémenté que pour la **taille** (position conservée), conformément au texte plus précis de `claude/PROMPT.md` ("Réinitialiser la taille").

### Session 5 — Structure & Onglet Comportement
| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 4.1 | **Fenêtre settings** | `Window` Avalonia séparée, `TabControl` avec 2 onglets + footer dépannage, `SettingsViewModel` (MVVM) | Fenêtre paramètres ouvrable depuis le tray |
| 4.2 | **Onglet Comportement** | Toggles : Verrouiller, Click-Through, Lancer au démarrage. Slider : Masquage (5-30s). Bouton : Reset taille/position. Sélecteur : Langue (i18n `.resx`) | Tous les toggles mappés au backend `AppConfig` via le ViewModel |

> **Statut (2026-07-26) : Session 6 (4.3-4.7) livrée et vérifiée par interaction réelle.** Backend `AppConfig` étendu (`PresetIndex`, `CoverShape` enum Square/Squircle/Vinyl, `CoverGlowEnabled`, `CoverBlurEnabled`, `DynamicColorsEnabled`, `DynamicBackgroundEnabled`, `BackgroundOpacity`), noms des 7 presets repris tels quels de `assets/presets_reference/layouts.ts` (Compact, Boxy, Gallery, Minimal, macOS, Shell, Discord) plutôt que des placeholders génériques. Deux réglages ont un effet visuel **déjà branché et vérifié en direct** (capture d'écran à l'appui) car réalisables sans le travail de rendu des Phases 6-7 : le slider Opacité change en direct la transparence du fond du widget (alpha du brush de fond uniquement, texte/icônes restent lisibles, comme dans `applyAppearance.ts`) ; le sélecteur Thème change en direct `Application.RequestedThemeVariant` (visible sur le chrome Fluent de la fenêtre Settings elle-même). Les autres réglages (preset, forme de pochette, glow, couleurs dynamiques, fond flouté) sont **persistés mais sans rendu** — la table `Settings_Appearance_NotYetAvailable` le dit explicitement dans l'UI ; leur rendu appartient aux Phases 6 (extraction couleur, effets GPU) et 7 (les 7 presets). 4.7 (bouton Recharger) était déjà livré en Session 5.

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

> **Statut (2026-07-26) : livrée et vérifiée en conditions réelles, avec un écart volontaire par rapport au tableau ci-dessous** (décidé en session, pas juste "présumé fonctionner") :
> - **5.1** livré : `Wavely.Backend.WaveformEngine`, capture WASAPI loopback sur thread dédié, ring buffer SPSC lock-free (`Core/RingBuffer.h`). **Ne se limite pas au périphérique par défaut** — un premier test réel (son système audible, waveform plate) a révélé que sur une machine avec routage audio par app (Elgato Wave Link), le périphérique par défaut n'est pas forcément celui utilisé par l'app dont on veut voir la waveform, et qu'un périphérique peut avoir une session "active" tout en renvoyant des buffers loopback à zéro. Voir `docs/ADR-003-waveform-device-selection.md` pour le diagnostic complet et la logique de sélection (session active + sonde de données réelles, réévaluée toutes les 2s).
> - **5.2** livré, **avec vraie FFT** (pas juste RMS) : radix-2 Cooley-Tukey maison (`std::complex<float>`, fenêtre de Hann), sur la demande explicite de l'utilisateur après un premier rendu jugé trop "timeline" plutôt que "EQ". 20 bandes (pas 60) log-espacées, concentrées sur la plage perceptuellement utile ~40Hz-16kHz (pas 0-Nyquist) pour donner plus de résolution/mouvement aux aigus — également une demande explicite en session, pas une valeur par défaut arbitraire.
> - **5.3** livré : thread dédié, `WaveformDataReady` (toutes les 16ms) avec les bandes packées en `IBuffer` (même pattern déjà vérifié pour la cover art), consommé côté C# via `MemoryMarshal.Cast<byte, float>`.
> - **5.4** livré, **style différent du tableau** : pas de "courbe lissée", des barres façon égaliseur centrées verticalement (croissance symétrique haut/bas depuis le centre), reprenant le look de `assets/presets_reference/EqualizerBars.svelte` — demande explicite en session après un premier rendu (barres alignées en bas, façon timeline) jugé inadapté.
> - **5.5 non fait** : aucun système de presets n'existe encore (Phase 7), donc pas de notion de "preset compatible" à brancher. Le waveform s'affiche inconditionnellement dans `MainWindow` pour l'instant.
> - Vérifié par captures d'écran répétées avec du vrai audio (sons système via `SoundPlayer`, vidéos/musique via navigateur) : les barres réagissent en direct, des bandes différentes s'allument différemment selon le contenu (graves vs aigus, preuve d'une vraie analyse fréquentielle et non d'un pouls uniforme), et reviennent à l'état de repos au silence. **Non mesuré** : l'utilisation CPU précise (checkpoint "< 2%/< 5%" ci-dessous non chiffré formellement).
>
> **Suivi (2026-07-26, plus tard en session) : capture par processus, plus par périphérique.** Demande explicite utilisateur suite à un bug réel (waveform plate sur une vidéo détectée, diagnostiqué comme du routage Elgato Wave Link par app) et à la remarque que la waveform reflétait l'audio de *toutes* les apps du périphérique, pas seulement l'app musicale affichée. `findBestRenderDevice`/`deviceHasActiveSession`/`deviceHasRealAudioData` (ADR-003) remplacés par une capture WASAPI par processus (`ActivateAudioInterfaceAsync` + `AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK`, Windows 10 2004+) ciblant le PID actif d'une app de la liste blanche (`Core/MusicAppAllowlist.h`, partagée avec Phase 1). Élimine structurellement la limite ADR-003 point 3 (plus de périphérique intermédiaire) et restreint la waveform à la seule app musicale whitelistée. Vérifié en conditions réelles : Spotify en lecture → waveform bouge ; Spotify en pause → repos ; son système (processus non whitelisté) joué en boucle pendant que Spotify est en pause → waveform reste au repos (preuve du scoping par processus, pas juste "rien d'autre ne jouait"). Voir `docs/ADR-004-music-app-whitelist-and-process-loopback.md`.

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

> **Statut (2026-07-27) : livrée et vérifiée par interaction réelle** (lecture Spotify réelle, capture d'écran à l'appui, pas seulement compilation). Plan détaillé : `docs/superpowers/plans/2026-07-26-phase6-dynamic-color-effects.md` (7 tâches, toutes cochées). 6.1 (`ColorExtractor`, K-Means sur cover 50x50) livré côté backend. 6.2 (`DynamicColorService` + binding fond/waveform/texte) vérifié : 3 changements de piste consécutifs (Spotify réel) ont chacun changé la couleur de fond du widget et le texte a basculé clair/sombre selon le contraste du fond. 6.3 (fond flouté) et 6.4 (glow) vérifiés visuellement actifs simultanément avec 6.2 sur le widget réel. 6.5 (squircle/vinyle) vérifié en basculant "Forme de la pochette" en direct dans Settings → Apparence : Squircle produit une superellipse nettement distincte du cercle, Vinyle un disque circulaire. 6.6 (rotation vinyle) vérifié par comparaison d'angle entre captures d'écran successives : tourne en continu pendant "Playing", se fige immédiatement (même angle sur 2 captures à 700ms d'écart) sur "Paused" (touche média Play/Pause réelle), reprend la rotation à la reprise de lecture sans revenir à 0°. Écart mineur : `ApplyAppearance()`/`ApplyDynamicColors()` accèdent au brush de fond via `BackgroundTintBorder.Background is SolidColorBrush` (pas de nom direct sur le `SolidColorBrush` — non compilable dans ce projet, voir amendement Tâche 3 du plan) plutôt que le `BackgroundBrush` nommé initialement prévu ; comportement identique.

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
