# CLAUDE.md — Wavely Project Rules

> Ce fichier contient les règles permanentes que tu dois **toujours respecter** sauf indication contraire explicite de l'utilisateur.
> Ces règles s'appliquent à chaque réponse, chaque fichier généré, chaque décision architecturale.

> **⚠️ Mise à jour architecture (voir `docs/ADR-001-cpp-backend-csharp-frontend.md` à créer) :** Wavely est désormais scindé en deux composants :
> - **Backend** — 100% **C++** (C++/WinRT), aucune logique métier ni aucun accès système ne doit exister côté frontend.
> - **Frontend** — 100% **C#** (.NET + Avalonia UI), aucune UI ne doit être écrite en C++.
> - **Interop** — le backend est compilé comme **composant WinRT** (runtime component, `.winmd`), consommé par le frontend via la projection **C#/WinRT** (`Microsoft.Windows.CsWinRT`). Pas de P/Invoke direct vers le backend ; le P/Invoke reste toléré côté frontend uniquement pour les comportements Win32 qu'Avalonia ne couvre pas nativement (click-through, drag frameless via `WM_NCHITTEST`).
> - Les Phases 0 à 3 du `PLAN.md` ont été livrées sous l'ancienne architecture (Qt6/C++ monolithique). Leur portage vers ce découpage est un chantier séparé, à planifier avant de reprendre la Phase 4.

---

## 1. TECHNOLIBRES & DÉPENDANCES

- **Ne JAMAIS** utiliser de bibliothèques dépréciées, abandonnées ou en fin de vie (ex: Boost.Random quand `<random>` existe nativement, .NET Framework quand .NET moderne existe).
- Privilégier les **APIs natives de la plateforme cible** avant toute lib tierce : WinRT/Win32/WASAPI côté backend, APIs Avalonia/.NET côté frontend.
- Si une lib tierce est nécessaire, elle doit être :
  - **Header-only** (C++) ou **NuGet officiel/actif** (C#) si possible.
  - Sous licence permissive (MIT, BSD, Apache 2.0).
  - Maintenue activement (> 1 commit dans les 6 derniers mois).
  - Comparée avec des alternatives dans un commentaire justificatif dans le code, le CMakeLists.txt ou le .csproj.
- **Versions minimales autorisées :**
  - Backend : C++20, CMake 3.24+, Windows SDK 10.0.22621+, C++/WinRT (cppwinrt) 2.0+.
  - Frontend : .NET 8+ (LTS), Avalonia UI 11+, Microsoft.Windows.CsWinRT (dernière version stable).
- **Interdiction explicite :** Boost, **Qt (toute version)**, MFC, ATL, WinForms, .NET Framework (< .NET 8), toute lib C++17 ou antérieure pour les nouvelles parties du code.

---

## 2. PERFORMANCE & OPTIMISATION

### Backend (C++)
- **Aucun `std::shared_ptr`** sauf si le partage de ownership est une nécessité architecturale avérée. Utiliser `std::unique_ptr` par défaut.
- **Aucune allocation mémoire dans la boucle principale de rendu/capture** (pas de `new`, `malloc`, `std::string` temporaire, `std::vector` réalloué dans le *hot path*).
- Toute opération lourde (extraction de couleur, FFT, lecture de métadonnées) doit tourner sur un **thread dédié ou thread pool**, jamais sur le thread qui répond aux appels WinRT du frontend.
- Utiliser **`constexpr`** et **`consteval`** autant que possible pour les calculs compile-time.
- Utiliser **`std::span`**, **`std::string_view`**, **`std::array`** plutôt que leurs équivalents heavies (vector, string, C-array) quand le lifetime le permet.
- **Zero-copy** pour le passage de données audio (buffer circulaire lock-free pour la WASAPI → waveform).
- **Frontière WinRT :** ne jamais copier des buffers volumineux (cover art, échantillons audio) plus d'une fois en traversant la frontière backend → frontend. Préférer `IBuffer`/`IRandomAccessStream` projetés plutôt que des tableaux managés reconstruits à chaque appel.

### Frontend (C#)
- Toute méthode backend potentiellement longue est **asynchrone** côté WinRT (`IAsyncOperation<T>` / `IAsyncAction`), projetée en `Task<T>` / `Task` côté C# — jamais d'appel bloquant sur le thread UI Avalonia.
- Éviter les allocations superflues dans les chemins de rendu (`Render(DrawingContext)`) : pas de LINQ, pas de boxing, pas de `new` de collections à chaque frame.
- Tout type projeté WinRT implémentant `IDisposable` doit être disposé explicitement (`using`) — ne jamais compter sur le GC pour libérer une ressource native.
- Les événements WinRT projetés (`TrackChanged`, etc.) sont livrés sur un thread arrière-plan : tout accès à l'UI depuis un handler doit repasser par le thread UI Avalonia (`Dispatcher.UIThread.Post`/`InvokeAsync`).
- Les shaders et effets visuels (flou, glow, squircle) doivent être **accélérés GPU** (rendu Skia d'Avalonia), pas de fallback logiciel.

---

## 3. QUALITÉ & MAINTENABILITÉ DU CODE

- **Tout code généré doit compiler sans warning** : `/W4` MSVC pour le backend, `<WarningsAsErrors>` / nullable enabled pour le frontend C#.
- **Pas de magic numbers.** Toute constante doit avoir un nom explicite (`constexpr`/`enum class` en C++, `const`/`static readonly` en C#).
- **Nommage backend (C++) :**
  - Types (classes, structs, enums) : `PascalCase`
  - Fonctions / méthodes : `camelCase`
  - Variables locales : `snake_case`
  - Variables membres : `m_` prefix (ex: `m_sessionManager`)
  - Constantes compile-time : `kCamelCase` (ex: `kMaxOpacity`)
  - Namespaces : `Wavely.Backend` (racine du composant WinRT), sous-namespaces `Wavely.Backend.Audio`, `Wavely.Backend.Media`, etc.
- **Nommage frontend (C#) :** conventions .NET standard
  - Types, méthodes, propriétés publiques : `PascalCase`
  - Variables locales et paramètres : `camelCase`
  - Champs privés : `_camelCase` (préfixe underscore)
  - Constantes : `PascalCase` (`const`) ou `k` + PascalCase si cohérence avec le backend souhaitée
  - Namespaces : `Wavely.App`, `Wavely.App.Views`, `Wavely.App.ViewModels`, `Wavely.App.Services`
  - Pattern d'architecture UI : **MVVM** (imposé par Avalonia) — toolkit à confirmer à l'implémentation (CommunityToolkit.Mvvm recommandé pour sa légèreté et son usage de source generators).
- **Chaque fichier `.h`/`.hpp`** doit avoir des **include guards** (`#pragma once`) et ne doit contenir que ce qui est strictement nécessaire (responsabilité unique). Chaque fichier `.cs` = une classe/interface (convention .NET).
- **RAII partout côté backend.** Pas de `new`/`delete` manuel, pas de handles bruts Win32 non wrappés. Tout objet système (HANDLE, HWND, IAudioCaptureClient) doit avoir un wrapper RAII.
- Les fonctions/méthodes ne doivent pas dépasser **40 lignes**. Au-delà, les découper en sous-fonctions nommées.
- Les classes ne doivent pas dépasser **~200 lignes**. Au-delà, les découper en modules.
- **Pas de `using namespace std;`** ni `using namespace` dans un header C++. Toléré temporairement dans un `.cpp` avec justification. Les `using` C# globaux (`ImplicitUsings`) sont acceptés.
- Chaque fonction/méthode publique doit avoir un **bref commentaire Doxygen** (C++) ou **XML doc comment** `///` (C#) décrivant ce qu'elle fait, pas comment elle le fait.

---

## 4. GESTION DES ERREURS & ROBUSTESSE

### Backend (C++)
- **Ne jamais ignorer silencieusement une erreur.** Tout appel système qui peut échouer doit être vérifié.
- Utiliser les **exceptions** uniquement pour les erreurs véritablement exceptionnelles (échec d'ouverture de device WASAPI, erreur de session GSMTC). Une exception C++ traversant la frontière WinRT devient une `Exception` .NET côté frontend — la documenter dans l'IDL.
- Pour les erreurs attendues (lecteur non lancé, track sans pochette), utiliser des **`std::optional`** ou des **`std::expected`**, ou une valeur nullable côté API WinRT exposée (ex: `IReference<T>`).
- Toute ressource système acquise doit être **release dans un destructeur** (RAII) ou via `scope_guard`.
- Les callbacks WinRT doivent gérer le cas où l'objet est déjà détruit (capture de `weak_ref`/vérification de lifetime) — le frontend peut se déconnecter à tout moment.

### Frontend (C#)
- Ne jamais avaler une exception silencieusement (`catch { }` vide interdit).
- Utiliser les **types nullables** (`T?`, nullable reference types activés) pour les valeurs optionnelles plutôt que des sentinelles.
- Toute erreur remontée du backend (exception WinRT) doit être interceptée au point d'appel et traduite en état UI compréhensible, jamais laissée remonter jusqu'à un crash non géré.

---

## 5. PERSISTANCE & CONFIGURATION

- Le backend est **seul propriétaire** de la configuration : fichier JSON (`%AppData%\Wavely\settings.json`, via `nlohmann/json` header-only). Le frontend ne lit/écrit jamais ce fichier directement — il passe exclusivement par les méthodes/propriétés exposées par le composant WinRT `Wavely.Backend.AppConfig`.
- La clé de démarrage Windows (`HKCU\Software\Microsoft\Windows\CurrentVersion\Run`) est lue/écrite par le backend via l'API registre Win32 native (`RegOpenKeyEx`/`RegSetValueEx`), wrappée RAII — pas de dépendance à une lib de settings tierce.
- Les presets (layouts du widget) sont stockés dans des fichiers **JSON externes**, chargés côté frontend, pour permettre l'ajout futur sans recompilation.
- **Sauvegarde automatique** de la position et taille du widget à chaque déplacement/redimensionnement (pas uniquement à la fermeture, pour éviter la perte en cas de crash) — le frontend appelle le backend à chaque évènement, le backend débounce/persiste.

---

## 6. INTERNATIONALISATION (i18n)

- Toute chaîne visible par l'utilisateur vit dans des fichiers de ressources **`.resx`** par langue (`Strings.fr.resx`, `Strings.en.resx`, ...), générant une classe fortement typée.
- **Aucune chaîne hardcodée** dans le XAML (AXAML) ou le code C#. Passer par la classe de ressources générée ou un service de traduction injecté dans les ViewModels.
- Les clés de ressource doivent être **contextuelles** (ex: `Settings_HideOnPause_Label` et non `Masquer`).
- La structure doit permettre d'ajouter une langue en ajoutant un `.resx` sans modifier le code source.

---

## 7. SÉCURITÉ & CONFIDENTIALITÉ

- **Aucune donnée utilisateur** (écoutée, interceptée, métadonnées de lecture) ne doit quitter la machine locale.
- **Aucune télémétrie, aucun analytics, aucun appel réseau** sauf si explicitement demandé par l'utilisateur.
- Les flux WASAPI loopback captent l'audio système : les buffers doivent être **purgés immédiatement après traitement FFT** (côté backend) et **jamais persistés sur disque**, ni jamais transmis bruts au frontend.
- Si un serveur de mise à jour est implémenté plus tard, il doit utiliser **HTTPS uniquement** avec vérification de certificat.

---

## 8. BUILD & CI

- **Backend :** CMake 3.24+, MSVC 2022+ (toolset v143), CMake Presets (`CMakePresets.json`) pour Debug/Release/Profile. Compile en composant WinRT (`.winmd` + DLL native).
- **Frontend :** SDK .NET (`dotnet build`/`dotnet publish`), projet Avalonia standard (`.csproj`), NuGet pour les dépendances (Avalonia, CsWinRT).
- **Orchestration :** un script d'orchestration minimal (PowerShell ou solution Visual Studio mixte C++/C#) est **autorisé en exception** à la règle « pas de script si CMake peut tout faire », car CMake ne pilote pas nativement une chaîne de build .NET. Ce script doit se limiter à enchaîner `cmake --build` (backend) puis `dotnet build/publish` (frontend) — aucune logique métier dedans.
- Le build Release doit générer un binaire **auto-suffisant** : `dotnet publish -r win-x64 --self-contained` pour le frontend, DLL backend copiée/bundlée à côté (pas d'installation à part si possible, ou installeur léger NSIS/Inno Setup si demandé).
- **Pas de dépendances runtime cachées.** Tout doit être statically linked (backend) ou self-contained (frontend), ou bundlé dans le dossier de l'app.

---

## 9. DOCUMENTATION & COMMUNICATION

- Chaque PR ou commit message doit suivre le format **Conventional Commits** (`feat:`, `fix:`, `perf:`, `refactor:`, `chore:`).
- Les décisions architecturales majeures doivent être documentées dans un fichier `docs/ADR-XXX-nom.md` (Architecture Decision Record) — notamment le découpage backend C++/frontend C# (`ADR-001`).
- Quand tu me proposes du code, **explique brièvement tes choix** avant le bloc de code si le choix n'est pas évident (ex: "J'expose ce buffer en `IBuffer` plutôt qu'en tableau managé pour éviter une copie à chaque frame de waveform").

---

## 10. RÈGLES MÉTA (COMMENT TU DOIS RÉPONDRE)

- **Ne jamais inventer d'API Windows, WinRT, Avalonia ou .NET inexistante.** Si tu n'es pas sûr de l'existence d'une API, dis-le explicitement et propose une vérification.
- **Ne jamais générer de code "placeholder"** (`// TODO`, `// implement later`) sauf si l'utilisateur l'accepte explicitement. Chaque fonction doit être fonctionnelle ou explicitement marquée comme stub avec une raison.
- **Si un problème a plusieurs solutions**, présente-les sous forme de tableau comparatif (Performance, Complexité, Maintenabilité) avant de recommander.
- **Propose toujours le code minimal nécessaire** pour résoudre le problème actuel. Pas de over-engineering anticipé.
- **Si la tâche est trop grande** pour une seule réponse, divise-la en étapes numérotées et exécute uniquement l'étape en cours, en résumant ce qui vient d'être fait et ce qui suit.

---

## 11. DOCUMENTATION PUBLIQUE & README

- Le `README.md` est une **page de vente / vitrine GitHub** : il doit être visuellement soigné,
  accrocheur et orienté utilisateur final. Il ne contient **pas** de documentation technique
  détaillée (pas d'instructions de build complexes, pas d'architecture interne, pas de liste
  de dépendances exhaustive).
- Le README doit contenir :
  - Un **hero visuel** (screenshot, GIF ou banner) en tête de fichier.
  - Une **phrase d'accroche** claire (ce que fait l'app, en une ligne).
  - Les **features clés** sous forme de liste courte et percutante (max 6-8 points).
  - Un **Getting Started minimal** (télécharger, lancer — pas compiler).
  - Des **badges** pertinents (version, licence, build status, plateforme).
  - Un lien vers `docs/TECHNICAL.md` pour les développeurs qui veulent aller plus loin.
- **Toute information technique** (architecture, dépendances, instructions de build, CMake
  presets, contribution guide, ADRs) appartient exclusivement à **`docs/TECHNICAL.md`**.
- Le README ne doit **jamais** contenir de bloc de code de plus de 3-4 lignes
  (une commande d'installation max, pas de snippet d'API).
- Le ton du README est **humain, moderne et enthousiaste** — pas corporate, pas académique.
