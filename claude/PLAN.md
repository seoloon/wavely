# 🚀 PLAN DE TRAVAIL — Wavely
### Développement optimisé avec Claude Code (Skill Superpower)

> **Philosophie du plan :** Chaque étape produit un **livrable compilable et testable**. Aucune étape ne dépasse la capacité d'une session Claude Code. On construit du solide avant d'ajouter du poli.

---

## PHASE 0 — Fondations (Session 1)
> *Durée estimée : 1 session | Objectif : Le squelette compile et tourne*

| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 0.1 | **Structure CMake** | `CMakePresets.json`, dossiers `src/`, `src/core/`, `src/ui/`, `src/audio/`, `src/settings/`, `resources/`, `docs/` | CMakeLists.txt compilable |
| 0.2 | **Point d'entrée** | `main.cpp` minimal : initialisation WinRT, création `QApplication`, lancement fenêtre widget frameless transparente | Fenêtre noire transparente qui s'affiche |
| 0.3 | **RAII Wrappers Win32** | `src/core/Handle.hpp`, `src/core/ComPtr.hpp`, `src/core/WinrtGuard.hpp` — wrappers template pour HANDLE, IUnknown, etc. | Headers compilables, aucun leak |
| 0.4 | **Configuration Persistante** | `src/settings/AppConfig.hpp/.cpp` — wrapper QSettings avec struct `Settings` typée | Lecture/écriture de la config |

### ✅ Checkpoint Phase 0
```
Compilable ? → cmake --build build --config Release
Lancement ?  → Fenêtre apparaît, fermeture propre
```

---

## PHASE 1 — Contrôleur Média GSMTC (Session 2)
> *Durée : 1 session | Objectif : On lit les métadonnées de Spotify/YouTube/Deezer*

| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 1.1 | **Wrapper GSMTC** | `src/core/MediaSessionManager.hpp/.cpp` — Singleton qui écoute `GlobalSystemMediaTransportControlsSessionManager`, émet des signaux Qt au changement de track | Signals : `trackChanged`, `playbackStateChanged`, `coverArtReceived` |
| 1.2 | **Extraction métadonnées** | `src/core/MediaMetadata.hpp` — struct `TrackInfo { title, artist, album, coverBuffer, duration, isPlaying }` | Extraction complète via `SMTCMediaProperties` |
| 1.3 | **Couverture (Pochette)** | Conversion `IRandomAccessStream` WinRT → `QImage` → `QPixmap` (thread-safe, async) | Affichage de la cover dans un QLabel test |

### ✅ Checkpoint Phase 1
```
Test : Lancer Spotify → Lancer Wavely → Vérifier que le titre/artiste/cover s'affichent
Test : Changer de piste → mise à jour automatique
Test : Pause/Play → le bool isPlaying change
```

---

## PHASE 2 — Widget Fenêtre & Interactions (Session 3)
> *Durée : 1 session | Objectif : Le widget se déplace, redimensionne, s'affiche/cache*

| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 2.1 | **Fenêtre Frameless** | Flags Win32 : `WS_POPUP \| WS_THICKFRAME`, remove title bar, bordures custom | Fenêtre sans chrome Windows |
| 2.2 | **Drag'n'Drop global** | Override `nativeEvent` pour capturer `WM_NCHITTEST` → `HTCAPTION`. Support multi-écrans via `QScreen` | Déplacement sur tous les moniteurs |
| 2.3 | **Redimensionnement** | Slider logique 50%→150% via `QTransform::scale()`. Snap à la taille "natrice" du preset | Widget zoom in/out |
| 2.4 | **Click-Through** | Flag `WS_EX_TRANSPARENT \| WS_EX_LAYERED` toggable. Bascule via Ctrl+Click ou setting | Le widget devient traversable aux clics |
| 2.5 | **Animation In/Out** | `QPropertyAnimation` sur `windowOpacity()` (0.0↔1.0, duration 300ms) | Fade smooth au play/pause |
| 2.6 | **Masquage différé** | `QTimer` configurable (5s→30s) : si pause + toggle actif → fade out après délai | Le widget disparaît en pause |

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
| 3.1 | **System Tray** | `QSystemTrayIcon` + menu contextuel (Paramètres, Recharger, Quitter) | Icône dans le tray, menu fonctionnel |
| 3.2 | **Auto-Start** | Écriture/lecture clé `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` | Toggle "Lancer au démarrage" fonctionnel |
| 3.3 | **Quitting propre** | Sauvegarde position/taille à la fermeture, cleanup COM/WinRT | Pas de crash à la fermeture |

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
| 4.1 | **Fenêtre settings** | `QDialog` ou `QWidget` séparé, `QTabWidget` avec 2 onglets + footer dépannage | Fenêtre paramètres ouvrable depuis le tray |
| 4.2 | **Onglet Comportement** | Toggles : Verrouiller, Click-Through, Lancer au démarrage. Slider : Masquage (5-30s). Bouton : Reset taille/position. Sélecteur : Langue (i18n skeleton) | Tous les toggles mappés à `AppConfig` |

### Session 6 — Onglet Apparence
| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 4.3 | **Section Lecteur** | ComboBox 7 presets (noms placeholder), sélecteur format cover (Carré/Squircle/Vinyle), toggle Glow | Le preset change la disposition |
| 4.4 | **Section Couleurs** | Toggles : Couleurs dynamiques, Fond couleur dominante, Fond cover floutée | Les toggles sont persistés |
| 4.5 | **Section Opacité** | Slider 0%→100% pour le fond du widget | Changement en temps réel |
| 4.6 | **Section Thème** | Toggle Sombre/Clair avec `QPalette` switch | Le thème change sans restart |
| 4.7 | **Bouton Recharger** | Reset du hook GSMTC + re-render | Le widget se "recharge" |

### ✅ Checkpoint Phase 4
```
Test : Ouvrir settings → Modifier un paramètre → Vérifier persistance au restart
Test : Thème sombre/clair → changement immédiat
```

---

## PHASE 5 — Waveform Dynamique WASAPI (Session 7-8)
> *Durée : 2 sessions | Objectif : La waveform réagit à l'audio en temps réel*

### Session 7 — Capture Audio
| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 5.1 | **WASAPI Loopback** | `src/audio/WasapiCapture.hpp/.cpp` — Device enumeration, `IAudioCaptureClient`, buffer circulaire lock-free (SPSC ring buffer) | Audio capture sans click/popping |
| 5.2 | **FFT** | Intégration KissFFT (header-only) ou `FFTCompute` custom. Buffer → fréquences normalisées (60 bands) | Tableau de 60 float [0.0→1.0] en temps réel |
| 5.3 | **Thread Management** | Thread WASAPI dédié, signal Qt émis toutes les 16ms (~60fps) avec le tableau FFT | Signal `waveformDataReady(QByteArray)` |

### Session 8 — Rendu Waveform
| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 5.4 | **Widget Waveform** | `WaveformWidget` : dessin custom via `QPainter` ou `QQuickPaintedItem` (barres verticales, courbe lissée) | Waveform animée en temps réel |
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

### Session 9 — Extraction de couleur
| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 6.1 | **Color Quantizer** | `src/core/ColorExtractor.hpp/.cpp` — K-Means (k=5) ou Octree sur la cover réduite (50x50). Thread pool, cache par hash de cover | Palette de 5 couleurs dominantes |
| 6.2 | **Binding UI** | Application des couleurs aux : progress bar, waveform, accents. Opacité de fond configurable | Le widget change de couleur par piste |

### Session 10 — Effets GPU
| # | Tâche | Détail | Livrable |
|---|-------|--------|----------|
| 6.3 | **Fond flouté** | `QGraphicsBlurEffect` (simple) ou shader custom `QSGNode` (Qt Quick) sur la cover comme fond | Blur gaussien sur le fond |
| 6.4 | **Glow pochette** | Shader ou `QGraphicsDropShadowEffect` avec couleur dominante | Halo lumineux autour de la cover |
| 6.5 | **Squircle** | Path mathématique via `QPainterPath` (Superellipse : `|x|^n + |y|^n = 1`, n≈5) | Cover en squircle |
| 6.6 | **Vinyle rotatif** | Animation `QPropertyAnimation` sur angle de rotation (360° en ~4s, ease linear) + overlay cercle noir central | Cover ronde qui tourne quand playing |

### ✅ Checkpoint Phase 6
```
Test : Changer de piste → les couleurs changent dynamiquement
Test : Fond flouté visible, glow présent, squircle correct
Test : Vinyle tourne en play, s'arrête en pause
```

---

## PHASE 7 — Les 7 Presets (Session 11-13)
> *Durée : 3 sessions | Objectif : 7 layouts visuels distincts et fonctionnels*

> ⚠️ **Note :** Cette phase est la plus longue car elle est purement **visuelle/artistique**. C'est ici que le portage Rust/Svelte → C++/QML se fait. Fournis les **maquettes visuelles** (screenshots, Figma, descriptions pixel-perfect) à Claude Code pour chaque preset.

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
| 8.2 | Crash resilience : try/catch autour de chaque appel WinRT/WASAPI |
| 8.3 | Nettoyage mémoire : vérification avec `Application Verifier` ou `AddressSanitizer` |
| 8.4 | Test de charge : 100 changements de piste rapides → pas de leak |

### Session 15 — Packaging
| # | Tâche |
|---|-------|
| 8.5 | Icône d'application (tray + window) |
| 8.6 | Versionnage (`Wavely v1.0.0` dans les settings/about) |
| 8.7 | Build Release final : static linking, UPX compression optionnelle |
| 8.8 | README.md avec screenshots, build instructions, feature list |
