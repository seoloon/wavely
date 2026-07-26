Tu es un architecte logiciel senior spécialisé Windows, à l'aise aussi bien en C++/WinRT natif qu'en C#/.NET moderne. Ta mission est de créer “Wavely” : un widget audio overlay pour Windows qui fonctionne avec les lecteurs/plateformes de streaming (Spotify, Deezer, Apple Music, YouTube Music, etc.) via les mécanismes standards Windows.

DÉCISIONS ARCHITECTURALES ACTÉES
- **Backend** : 100% C++ (C++/WinRT). Toute la logique métier et tous les accès système (GSMTC, WASAPI, registre, fichiers) y vivent exclusivement.
- **Frontend** : 100% C# (.NET + Avalonia UI). Toute l'interface utilisateur y vit exclusivement — aucune UI côté backend.
- **Interop** : le backend est compilé comme composant WinRT (runtime component, `.winmd`), consommé par le frontend via la projection C#/WinRT (`Microsoft.Windows.CsWinRT`). Pas de marshaling manuel pour les types déjà couverts par WinRT ; P/Invoke toléré côté frontend uniquement pour les comportements Win32 qu'Avalonia ne couvre pas (click-through, drag frameless).
- Cette scission remplace l'architecture précédente (Qt6/C++ monolithique). Le détail technique complet vit dans `claude\RULES.md` ; les Phases 0-3 déjà livrées sous l'ancienne architecture seront portées séparément.

OBJECTIF PRODUIT
- Appli Windows “widget” (fenêtre overlay) qui affiche des infos lecture + cover + waveform + contrôles selon un preset.
- Fonctionne par observation du média en cours (lecture/pause/progression/cover) via WinRT System Media Transport Controls (SMTC), côté backend.
- Affiche une waveform “réelle et dynamique” en capturant l'audio de sortie (capture WASAPI loopback), donc compatible avec la plupart des apps.
- UI rapide et moderne : frontend Avalonia (C#/.NET) consommant le backend C++ via le composant WinRT — rendu vectoriel Skia (natif à Avalonia), animations via le système de transitions Avalonia.
- Conteneur UI : Avalonia (AXAML) pour la fenêtre de paramètres et pour le widget overlay lui-même (contrôles personnalisés pour la waveform/cover, dessinés via `DrawingContext`).

CONTRAINTES TECHNIQUES / STACK
- Langages : C++ (backend) + C# (frontend). Aucune logique métier en C#, aucune UI en C++.
- WinRT : `Windows::Media::SystemMediaTransportControls` (SMTC) pour la lecture, implémenté et exposé côté backend.
- UI : Avalonia (dernière version stable). AXAML pour la structure, rendu custom (`DrawingContext`/contrôles personnalisés) pour la waveform et les effets visuels.
- Tray : API tray native d'Avalonia (`TrayIcon`) ; repli vers Win32 `Shell_NotifyIcon` uniquement si l'abstraction Avalonia s'avère insuffisante.
- Persistance : fichier JSON (`%AppData%\Wavely\settings.json`), lu/écrit exclusivement par le backend et exposé au frontend via le composant WinRT (source de vérité unique — voir RULES.md §5).
- Threads (backend) :
  - Thread capture audio (WASAPI loopback) -> ring buffer d'amplitudes
  - Thread/Task SMTC -> état média + metadata
  - Résultats poussés au frontend via événements WinRT (`TrackChanged`, `WaveformDataReady`, ...).
- Threads (frontend) :
  - Thread UI Avalonia -> rendu waveform à cadence fixe (30-60 fps), jamais bloqué : tout appel backend potentiellement long est asynchrone (`IAsyncOperation<T>` → `Task<T>`).
- Performance :
  - aucune décodification audio complexe
  - waveform basée sur amplitudes/energy (option FFT si utile) calculée côté backend à partir des buffers WASAPI
  - aucune copie redondante de buffer volumineux (cover, audio) à travers la frontière WinRT

FONCTIONNALITÉS À LIVRER

1) Démarrage & Tray
- Lancement au démarrage (optionnel mais doit être implémenté) :
  - via clé Run dans Registry (backend, API Win32 native) OU Startup folder
- Widget :
  - actif en permanence dans la zone de notification (tray, `Avalonia.Controls.TrayIcon`)
  - click sur tray -> ouvrir/masquer widget ou menus
  - option “exit”/“quitter” si nécessaire

2) Fenêtre Paramètres (2 onglets)
Créer une fenêtre Settings (Avalonia `Window`) avec 2 tabs :

A. Onglet “Comportement”
- Vérouiller le Widget (empêche drag/réposition/resize)
- Click-Through (widget transparent aux clics)
  - Implémentation : ajuster styles Win32 `WS_EX_TRANSPARENT`/`WS_EX_LAYERED` sur le handle natif de la fenêtre Avalonia selon état
  - Attention : quand “click-through” = ON, le widget ne doit pas capter les clics souris
- Réinitialiser la taille
  - si la fenêtre a été redimensionnée : remettre taille à la valeur par défaut
- Masquer le widget en pause (si activé)
  - slider “délai avant masquage” entre 5s et 30s
  - déclenché sur state SMTC = Paused/Stopped (event backend)
  - doit réafficher quand lecture reprend
- Lancer avec la session (différent du “lancement au démarrage” du widget ? -> gérer séparément si besoin, sinon uniformiser en une seule option claire)
- Langue
  - afficher “Français” maintenant
  - architecture i18n via ressources `.resx` par langue (voir RULES.md §6), extensible sans recompilation du code métier
  - prévoir structure de ressources dès le début

B. Onglet “Apparence”
Sections :

i) “Lecteur”
- 7 presets précis à convertir depuis votre base (Rust/Svelte -> Avalonia/C#).
  - TU DOIS demander/recevoir le contenu exact des 7 presets (ou leurs specs UI) si non fourni.
- Pour chaque preset :
  - règles d'affichage des contrôles (certains ont boutons play/pause/next/prev ou slider, d'autres non)
  - animations in-out quand play/pause (hide on pause si prévu)
  - drag'n'drop partout sur le(s) écran(s) : le widget se déplace selon preset + comportements globaux
  - redimensionnement : autoriser taille entre 50% et 150% (slider ou poignée)
- Style cover à intégrer :
  - carré léger bord arrondi
  - canva (squircle)
  - vinyle (rond + animation rotation à la lecture)
  - glow de la pochette (léger contour glow)
- Important : le moteur de rendu (frontend) doit être générique pour changer de preset en runtime.

ii) “Couleurs”
- Couleurs dynamiques basées sur la cover (extraction backend) :
  - extraire dominante (et secondaire) de l'image de cover
  - appliquer ces couleurs à :
    - duration bar
    - waveform (couleur principale et éventuellement accent secondaire)
    - texte/éléments graphiques selon contraste
  - colorer tout le fond (fond = dominante de la cover)
  - flou d'arrière-plan : afficher la cover flouté en fond (effet Avalonia ou rendu Skia custom)
- Opacité du fond : slider (ex 0-100%) appliqué au background (dominante)
- Thème : sombre / clair (switch, `ThemeVariant` Avalonia)
  - la couleur dominante doit s'adapter pour rester lisible en clair/sombre

iii) “Dépannage”
- Bouton “Recharger le widget”
  - force refresh metadata (cover), recalcul palette couleurs, relancer rendu, et réinitialiser waveform UI (appel backend + refresh frontend)

3) Wire/Behavior global
- Drag'n'drop : déplacer le widget sur les écrans
  - si verrouillé -> aucune redéfinition
  - si click-through ON -> drag activé uniquement via une zone “safe” ou désactivé (décide une règle cohérente et implémente-la)
- Redimensionnement : 50% à 150%
- “Actif dans le tray” : le widget doit survivre aux changements d'état (hide/show, click-through, etc.)

INTÉGRATION MÉDIA (SMTC) — backend
- Implémenter un `MediaSessionManager` (composant WinRT) :
  - s'abonner aux événements SMTC (play/pause, media change, timeline update si possible)
  - récupérer :
    - state (Playing/Paused/Stopped)
    - duration / position (si disponible)
    - title / artist / artwork thumbnail (si disponible)
  - exposer des événements WinRT consommés tels quels comme des `event` C# côté frontend
  - quand pause : le frontend déclenche “masquer widget” si activé avec timer (slider 5-30s)
  - quand play : le frontend annule le timer + show widget

INTÉGRATION WAVEFORM (WASAPI loopback) — backend
- Implémenter un `WaveformEngine` (composant WinRT) :
  - capture WASAPI loopback (default render device)
  - calcul d'une représentation “waveform” (amplitudes RMS sur fenêtres successives)
  - ring buffer -> échantillons récents
  - smoothing pour rendu stable
  - interface de rendu :
    - événement WinRT fournissant un buffer d'amplitudes normalisées à cadence régulière (~60fps)
    - le frontend gère les différents “styles” selon preset (barres, line, etc.) au rendu
- Sur in-out play/pause (frontend, piloté par les events backend) :
  - quand play : show + animation
  - quand pause : hide on pause si preset prévu (sinon show mais waveform peut ralentir)

COULEURS + COVER — backend
- `CoverRenderer` (ou logique de préparation côté backend) :
  - charger image artwork (de SMTC thumbnail) -> décoder bitmap, exposer en `IBuffer`/`IRandomAccessStream`
  - le masque/shape (rounded rect / squircle / circle / vinyl) et le glow sont appliqués côté **frontend** au rendu (ce sont des préoccupations visuelles)
- `CoverColorExtractor` (backend) :
  - extraire palette (dominante + accent)
  - fallback si cover indisponible (couleurs theme, décidé côté frontend)
  - recalcul à chaque “recharger widget” ou changement de média

DELIVERABLES ATTENDUS (CODE)
1) Architecture solution : projet CMake pour le backend C++/WinRT (composant `.winmd`) + solution .NET pour le frontend Avalonia, avec script d'orchestration de build entre les deux
2) Backend : composant WinRT exposant `MediaSessionManager`, `WaveformEngine`, `CoverRenderer`, `CoverColorExtractor`, `AppConfig`, `AutoStartManager`
3) Frontend Avalonia : widget overlay (fenêtre custom-drawn) consommant le backend via la projection WinRT
4) Tray icon + menu (Avalonia `TrayIcon`)
5) Settings window (Avalonia AXAML) avec 2 onglets + bindings vers le composant backend
6) Système de presets (7 presets) : abstraction Preset côté frontend (paramètres UI + style cover + style waveform + contrôles + animations)
7) Persistence settings.json (backend) + migration minimale (si besoin)
8) Bouton “Recharger le widget”
9) Gestion lock/click-through/resize + drag multi-écrans (frontend, accès Win32 natif via le handle de fenêtre Avalonia si besoin)

ACCEPTANCE CRITERIA (KO/OK)
- Widget visible au démarrage si l'utilisateur l'active
- Dans tray : option de show/hide + exit
- Le widget réagit à play/pause du média SMTC
- La cover affiche et les couleurs changent dynamiquement à partir de la cover
- La waveform est dynamique (mise à jour en continu) et change quand la musique change
- Les 7 presets fonctionnent (au moins visuellement, et les comportements play/pause/hide prévus par chaque preset)
- Drag fonctionne sur plusieurs écrans, et le resize respecte 50%-150%
- Click-through : quand activé, le widget ne bloque pas les clics apps sous-jacentes
- “Masquer le widget en pause” : hide après délai configurable 5-30s et show au resume

QUESTIONS OBLIGATOIRES AVANT DE CODER (si infos manquantes)
1) Donne-moi les 7 presets exacts (code Rust/Svelte ou description précise des composants/animations/controls).
2) Les contrôles SMTC à mapper pour “controls prévus” : play/pause uniquement ? next/prev aussi ?
3) Format attendu pour “duration bar” : utilisent-ils position/duration venant SMTC uniquement ?
4) Confirmes-tu que “GSMTC” = SMTC (System Media Transport Controls) ?
5) Est-ce que le widget doit supporter plusieurs langues UI dès maintenant (au moins structure) ou seulement architecture ?

MODE DE TRAVAIL POUR CLAUDE CODE
- Commence par proposer une architecture concrète : découpage backend WinRT / frontend Avalonia + le contrat d'interop entre les deux (IDL, événements, types exposés).
- Ensuite crée la structure des deux projets (CMake backend + solution .NET frontend) et génère un squelette compilable pour chacun.
- Implémente d'abord : composant backend minimal (config, GSMTC) + frontend minimal qui le consomme (fenêtre + tray).
- Puis branche cover loading + color extraction + rendus de base.
- Ensuite seulement : WASAPI loopback + waveform renderer + presets.
- À chaque étape : indique les fichiers créés et les points d'intégration (des deux côtés de la frontière WinRT).
- Si une info est manquante (ex presets), STOP et demande-la avant d'implémenter les détails.
- Note migration : les Phases 0 à 3 du `PLAN.md` ont été initialement livrées sous l'ancienne architecture (Qt6/C++ monolithique). Leur portage vers ce découpage backend/frontend est un chantier dédié, à planifier séparément avant de reprendre la Phase 4.

Nom du projet : “Wavely”
Veille à toujours, sauf indications contraire, à respecter le fichier claude\RULES.md
Go.
