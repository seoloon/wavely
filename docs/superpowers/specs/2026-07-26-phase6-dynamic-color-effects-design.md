# Phase 6 — Couleur Dynamique & Effets Visuels — Design

> Correspond à `claude/PLAN.md` Phase 6 (Session 9-10). Statut avant cette session : 6.1
> (backend) était déjà codé lors d'une session précédente mais **non commité et non vérifié par
> compilation/exécution réelle** ; 6.2-6.6 n'existaient pas.

## Décisions actées en amont (confirmées avec l'utilisateur)

- **6.1 existant** : vérifié (compilé, testé avec une vraie cover), corrigé si besoin, puis gardé
  comme base — pas réécrit à zéro.
- **Progress bar** : `PLAN.md` 6.2 mentionne la coloration d'une "progress bar", mais aucune
  barre de progression n'existe dans l'UI actuelle, et le backend n'expose même pas la position
  de lecture (seulement `DurationMs`, pas de suivi de `Position`). L'ajouter est un chantier SMTC
  timeline distinct, hors périmètre "couleur/effets visuels". **Reporté explicitement** (même
  logique que 5.5 en Phase 5) : la coloration dynamique s'applique au fond, à la waveform et au
  glow — pas de progress bar dans cette phase.
- **Découpage** : Session 9 et Session 10 sont traitées dans la même vague d'implémentation
  (pas d'arrêt intermédiaire demandé), mais restent deux livrables distincts en interne (6.1-6.2
  backend+binding, puis 6.3-6.6 effets GPU) pour garder chaque étape testable indépendamment.

## Architecture / flux de données

```
Backend (C++/WinRT)                          Frontend (C#/Avalonia)
─────────────────────                        ───────────────────────
ColorExtractor::ExtractDominantColors   -->   DynamicColorService
  (déjà implémenté, à vérifier)                 - unpack IBuffer -> Color[5]
        |                                        - luminance -> WidgetColorScheme
        v                                              |
TrackInfo.DominantColors (IBuffer,                     v
  5x uint32 ARGB, même convention           MainWindow.OnTrackChanged
  zero-copy que WaveformEngine)               - applique le scheme aux éléments
                                                 existants, gated par AppConfig
```

Aucune nouvelle surface WinRT nécessaire pour le flux couleur : `TrackInfo.DominantColors`
traverse la frontière exactement comme `CoverArt` le fait déjà (Phase 1).

Les toggles `AppConfig` nécessaires existent déjà et sont persistés depuis la Phase 4 Session 6
(`CoverShape`, `CoverGlowEnabled`, `CoverBlurEnabled`, `DynamicColorsEnabled`,
`DynamicBackgroundEnabled`) — cette phase les branche au rendu, elle n'en ajoute aucun.

## 6.1 — Color Quantizer (backend) — vérification

Le code existant (`Core/ColorExtractor.h/.cpp`, non commité) fait un histogramme de population
(bucket 4 bits/canal) sur une miniature 50x50 décodée via WIC, pondéré vers la saturation pour
éviter qu'un grand fond noir/blanc écrase les couleurs d'accent réelles. Avant de construire
dessus :
1. Compiler le backend (MSBuild, `/W4 /WX`) — corriger tout warning/erreur.
2. Lancer `Wavely.App.exe` avec une vraie session média ayant une cover, vérifier par log/inspection
   que `TrackInfo.DominantColors` contient bien 5 `uint32` ARGB plausibles (pas juste "ça compile").
3. Committer comme base de la Phase 6 (`feat: extract dominant colors from cover art (Phase 6.1)`),
   séparément du reste du travail de cette phase.

Aucun changement de conception prévu ici sauf si la vérification révèle un bug réel.

## 6.2 — Binding couleurs dynamiques (frontend)

Nouveau `Services/DynamicColorService.cs` :
- `WidgetColorScheme Resolve(TrackInfo track, AppConfig config)` :
  - Si `!config.DynamicColorsEnabled`, cover absente, ou `DominantColors` vide/non décodable :
    retourne le scheme statique actuel (fond `#141418`, accent bleu existant de
    `WaveformControl.BarBrush`, texte blanc) — comportement inchangé par défaut.
  - Sinon : `palette[0]` = fond, `palette[1]` = accent (waveform + glow), calcul de luminance
    relative (`0.2126R+0.7152G+0.0722B`) sur le fond choisi pour décider `TextIsDark` (seuil
    0.6) — bascule titre/artiste/statut vers une variante sombre au lieu du blanc actuel si le
    fond est trop clair, pour rester lisible.
- `WidgetColorScheme` : `Background`, `Accent`, `Glow`, `TextIsDark` (types `Avalonia.Media.Color`).

`MainWindow` :
- `ApplyDynamicColors(TrackInfo track)` appelé depuis `OnTrackChanged`, en plus de
  `ApplyAppearance()` existant (ne le remplace pas — l'opacité/thème restent gérés là).
- Application par élément, chacun indépendamment gated :
  - `BackgroundBorder.Background` (opacity du brush préservée, comme `ApplyAppearance` le fait
    déjà) — gated par `DynamicBackgroundEnabled`.
  - `WaveformControl` reçoit une nouvelle propriété `AccentColor` (remplace le `BarBrush` fixe
    par un brush réassignable) — gated par `DynamicColorsEnabled`.
  - `TitleText`/`ArtistText`/`StatusText.Foreground` basculent clair/sombre — gated par
    `DynamicColorsEnabled` (suit le même toggle que l'accent, pas un toggle séparé).
  - Glow (voir 6.4) — gated par `CoverGlowEnabled`, couleur = `Accent` si `DynamicColorsEnabled`
    sinon blanc neutre.

## 6.3 — Fond flouté (frontend)

Un `Image` supplémentaire (cover art, `Stretch=UniformToFill`) positionné derrière
`BackgroundBorder`'s content, avec `Effect="blur(20)"` (`Avalonia.Media.Effects.BlurEffect`,
rendu Skia GPU — pas de shader custom, RULES.md §2 demande GPU-accéléré et Avalonia le fournit
nativement) plus un `Border` de tint semi-opaque par-dessus pour la lisibilité du texte. Gated
par `CoverBlurEnabled` ; masqué (Opacity 0 / `IsVisible=false`) sinon, fond classique inchangé.

## 6.4 — Glow pochette (frontend)

`CoverBorder.Effect = new DropShadowEffect { Color = ..., BlurRadius = ..., OffsetX = 0, OffsetY = 0 }`
(built-in Avalonia, GPU). Couleur = `WidgetColorScheme.Accent` si `DynamicColorsEnabled`, sinon
blanc neutre. Gated par `CoverGlowEnabled` seul (indépendant de `DynamicColorsEnabled` pour la
présence du glow, seule sa couleur en dépend).

## 6.5 — Squircle (frontend)

`Core/SquircleGeometry.cs` (nouveau, frontend) : génère un `StreamGeometry` par échantillonnage
paramétrique (~64 points) de la superellipse `|x/a|^n + |y/b|^n = 1`, `n≈5`, mis en cache par
taille (évite de régénérer à chaque frame si la taille du cover ne change pas — pas d'allocation
dans `Render`). Appliqué comme `CoverBorder.Clip` quand `config.CoverShape == CoverStyle.Squircle`,
remplaçant le `CornerRadius` actuel. `CoverStyle.Square` garde le comportement actuel
(`CornerRadius=8`).

## 6.6 — Vinyle rotatif (frontend)

Quand `CoverShape == CoverStyle.Vinyl` :
- `CoverImage.Clip` = ellipse (cercle).
- `CoverImage.RenderTransform = RotateTransform`, animé via une Avalonia `Animation`
  (`KeyFrame` 0°→360°, `Duration=4s`, `IterationCount=Infinite`, `Easing=LinearEasing`) —
  démarrée sur `PlaybackStateChanged(isPlaying=true)`, **mise en pause** (pas reset à 0°) sur
  `isPlaying=false`, pour un comportement vinyle réaliste (elle reprend depuis l'angle courant).
- Un petit cercle sombre plein (spindle), centré, dessiné par-dessus (`Ellipse` fixe, pas
  d'anti-aliasing custom nécessaire).

## Tests / vérification

Comme pour les phases précédentes : vérification par interaction réelle (lancer l'app avec une
vraie session média ayant une cover contrastée), captures d'écran à l'appui, pas seulement
compilation :
- Changer de piste (covers de couleurs différentes) → fond/accent/glow changent visiblement.
- `DynamicColorsEnabled=false` → fond/accent reviennent au thème statique actuel.
- `CoverBlurEnabled` / `CoverGlowEnabled` toggle → effet apparaît/disparaît en direct (déjà le
  pattern `RefreshFromConfig` existant en Phase 4).
- `CoverShape` = Square/Squircle/Vinyl → forme correcte à chaque valeur.
- Vinyle : tourne en `Playing`, s'arrête (sans reset) en `Paused`.
- Cover très claire (texte lisible) et très sombre → vérifier la bascule `TextIsDark`.

## Hors périmètre (explicite)

- Progress bar coloring (voir décision ci-dessus).
- Les 7 presets (Phase 7) — cette phase ne touche que `MainWindow` tel qu'il existe aujourd'hui.
- Tout suivi de position de lecture SMTC.
