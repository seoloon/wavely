Tu es un développeur C++ senior spécialisé Windows/WinRT. Ta mission est de créer, en C++ (WinRT + Windows App SDK), l’application “Wavely” : un widget audio overlay pour Windows qui fonctionne avec les lecteurs/plateformes de streaming (Spotify, Deezer, Apple Music, YouTube Music, etc.) via les mécanismes standards Windows.

OBJECTIF PRODUIT
- Appli Windows “widget” (fenêtre overlay) qui affiche des infos lecture + cover + waveform + contrôles selon un preset.
- Fonctionne par observation du média en cours (lecture/pause/progression/cover) via WinRT System Media Transport Controls (SMTC).
- Affiche une waveform “réelle et dynamique” en capturant l’audio de sortie (capture WASAPI loopback), donc compatible avec la plupart des apps.
- UI rapide et universelle : WinRT / Windows App SDK (WinUI 3 si nécessaire) + rendu Direct2D/Win2D + animations via Windows.UI.Composition.
- Conteneur UI : XAML pour les settings, et rendu performant côté widget (canvas/controls personnalisés).

CONTRAINTES TECHNIQUES / STACK
- Langage : C++
- WinRT : utiliser Windows::Media::SystemMediaTransportControls (SMTC).
- UI : Windows App SDK (WinUI 3 / XAML). Pour le widget overlay : préférer un composant XAML léger + rendu graphique custom (Win2D/Direct2D) pour waveform + visuels.
- Tray : API Win32 Shell_NotifyIcon ou équivalent (pas de frameworks lourds).
- Persistance : fichier de config JSON (ex: %AppData%\Wavely\settings.json).
- Threads :
  - Thread capture audio (WASAPI loopback) -> ring buffer d’amplitudes
  - Thread/Task SMTC -> état média + metadata
  - UI thread -> rendu waveform à cadence fixe (ex 30-60 fps) via Dispatcher/Composition.
- Performance :
  - aucune décodification audio complexe
  - waveform basée sur amplitudes/energy (option FFT si utile) calculée à partir des buffers WASAPI.

FONCTIONNALITÉS À LIVRER

1) Démarrage & Tray
- Lancement au démarrage (optionnel mais doit être implémenté) :
  - via clé Run dans Registry OU Startup folder
- Widget :
  - actif en permanence dans la zone de notification (tray)
  - click sur tray -> ouvrir/masquer widget ou menus
  - option “exit”/“quitter” si nécessaire

2) Fenêtre Paramètres (2 onglets)
Créer une fenêtre Settings avec 2 tabs :

A. Onglet “Comportement”
- Vérouiller le Widget (empêche drag/réposition/resize)
- Click-Through (widget transparent aux clics)
  - Implémentation : ajuster styles WS_EX_TRANSPARENT/WS_EX_LAYERED selon état
  - Attention : quand “click-through” = ON, le widget ne doit pas capter les clics souris
- Réinitialiser la taille
  - si la fenêtre a été redimensionnée : remettre taille à la valeur par défaut
- Masquer le widget en pause (si activé)
  - slider “délai avant masquage” entre 5s et 30s
  - déclenché sur state SMTC = Paused/Stopped
  - doit réafficher quand lecture reprend
- Lancer avec la session (différent du “lancement au démarrage” du widget ? -> gérer séparément si besoin, sinon uniformiser en une seule option claire)
- Langue
  - afficher “Français” maintenant
  - implémenter une architecture i18n extensible pour langues hors français (traductions plus tard)
  - prévoir structure Resource strings / JSON i18n

B. Onglet “Apparence”
Sections :

i) “Lecteur”
- 7 presets précis à convertir depuis votre base (Rust/Svelte -> C++).
  - TU DOIS demander/recevoir le contenu exact des 7 presets (ou leurs specs UI) si non fourni.
- Pour chaque preset :
  - règles d’affichage des contrôles (certains ont boutons play/pause/next/prev ou slider, d’autres non)
  - animations in-out quand play/pause (hide on pause si prévu)
  - drag’n’drop partout sur le(s) écran(s) : le widget se déplace selon preset + comportements globaux
  - redimensionnement : autoriser taille entre 50% et 150% (slider ou poignée)
- Style cover à intégrer :
  - carré léger bord arrondi
  - canva (squircle)
  - vinyle (rond + animation rotation à la lecture)
  - glow de la pochette (léger contour glow)
- Important : le moteur de rendu doit être générique pour changer de preset en runtime.

ii) “Couleurs”
- Couleurs dynamiques basées sur la cover :
  - extraire dominante (et secondaire) de l’image de cover
  - appliquer ces couleurs à :
    - duration bar
    - waveform (couleur principale et éventuellement accent secondaire)
    - texte/éléments graphiques selon contraste
  - colorer tout le fond (fond = dominante de la cover)
  - flou d’arrière-plan : afficher la cover flouté en fond (blur léger)
- Opacité du fond : slider (ex 0-100%) appliqué au background (dominante)
- Thème : sombre / clair (switch)
  - la couleur dominante doit s’adapter pour rester lisible en clair/sombre

iii) “Dépannage”
- Bouton “Recharger le widget”
  - force refresh metadata (cover), recalcul palette couleurs, relancer rendu, et réinitialiser waveform UI

3) Wire/Behavior global
- Drag’n’drop : déplacer le widget sur les écrans
  - si verrouillé -> aucune redéfinition
  - si click-through ON -> drag activé uniquement via une zone “safe” ou désactivé (décide une règle cohérente et implémente-la)
- Redimensionnement : 50% à 150%
- “Actif dans le tray” : le widget doit survivre aux changements d’état (hide/show, click-through, etc.)

INTÉGRATION MÉDIA (SMTC)
- Implémenter un SMTCManager :
  - s’abonner aux événements SMTC (play/pause, media change, timeline update si possible)
  - récupérer :
    - state (Playing/Paused/Stopped)
    - duration / position (si disponible)
    - title / artist / artwork thumbnail (si disponible)
  - quand pause : déclencher “masquer widget” si activé avec timer (slider 5-30s)
  - quand play : annuler timer + show widget

INTÉGRATION WAVEFORM (WASAPI loopback)
- Implémenter WaveformEngine :
  - capture WASAPI loopback (default render device)
  - calcul d’une représentation “waveform” (amplitudes RMS sur fenêtres successives)
  - ring buffer -> échantillons récents
  - smoothing pour rendu stable
  - interface de rendu :
    - fournir tableau/stream d’amplitudes normalisées
    - gérer différents “styles” selon preset (barres, line, etc.)
- Sur in-out play/pause :
  - quand play : show + animation
  - quand pause : hide on pause si preset prévu (sinon show mais waveform peut ralentir)

COULEURS + COVER
- CoverRenderer :
  - charger image artwork (de SMTC thumbnail) -> décoder bitmap
  - appliquer mask/shape (rounded rect / squircle / circle / vinyl)
  - appliquer glow si preset le demande
  - produire background blur texture (blur via Composition)
- CoverColorExtractor :
  - extraire palette (dominante + accent)
  - fallback si cover indisponible (couleurs theme)
  - recalcul à chaque “recharger widget” ou change media

DELIVERABLES ATTENDUS (CODE)
1) Architecture et structure de solution CMake / Visual Studio
2) Widget overlay (fenêtre Win32 + XAML/Composition ou XAML-only selon le meilleur compromis)
3) Tray icon + menu
4) Settings window (XAML) avec 2 onglets + bindings settings
5) SMTCManager (state, metadata, artwork, timeline)
6) WaveformEngine (WASAPI loopback) + WaveformRenderer (graphique)
7) Système de presets (7 presets) :
   - abstraction Preset : paramètres UI + style cover + style waveform + contrôles + animations
8) Persistence settings.json + migration minimale (si besoin)
9) Bouton “Recharger le widget”
10) Gestion lock/click-through/resize + drag multi-écrans

ACCEPTANCE CRITERIA (KO/OK)
- Widget visible au démarrage si l’utilisateur l’active
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
- Commence par proposer une architecture concrète (classes/modules + flux des données).
- Ensuite crée la structure du projet (fichiers + CMake/VS solution) et génère un squelette compilable.
- Implémente d’abord : tray + widget fenêtre + settings skeleton + SMTC state updates.
- Puis branche cover loading + color extraction + rendus de base.
- Ensuite seulement : WASAPI loopback + waveform renderer + presets.
- À chaque étape : indique les fichiers créés et les points d’intégration.
- Si une info est manquante (ex presets), STOP et demande-la avant d’implémenter les détails.

Nom du projet : “Wavely”
Veille à toujours, sauf indications contraire, à respecter le fichier claude\RULES.md
Go.