# CLAUDE.md — Wavely Project Rules

> Ce fichier contient les règles permanentes que tu dois **toujours respecter** sauf indication contraire explicite de l'utilisateur.
> Ces règles s'appliquent à chaque réponse, chaque fichier généré, chaque décision architecturale.

---

## 1. TECHNOLIBRES & DÉPENDANCES

- **Ne JAMAIS** utiliser de bibliothèques dépréciées, abandonnées ou en fin de vie (ex: Boost.Random quand <random> existe nativement).
- Privilégier les **APIs natives du système cible** (WinRT, Win32, WASAPI) avant toute lib tierce.
- Si une lib tierce est nécessaire, elle doit être :
  - **Header-only** si possible (réduit le temps de compilation).
  - Sous licence permissive (MIT, BSD, Apache 2.0).
  - Maintenue activement (> 1 commit dans les 6 derniers mois).
  - Comparée avec des alternatives dans un commentaire justificatif dans le code ou le CMakeLists.txt.
- **Versions minimales autorisées :** C++20, CMake 3.24+, Qt 6.5+, Windows SDK 10.0.22621+.
- **Interdiction explicite :** Boost, Qt5, MFC, ATL, toute lib C++17 ou antérieure pour les nouvelles parties du code.

---

## 2. PERFORMANCE & OPTIMISATION

- **Aucun `std::shared_ptr`** sauf si le partage de ownership est une nécessité architecturale avérée. Utiliser `std::unique_ptr` par défaut.
- **Aucune allocation mémoire dans la boucle principale de rendu** (pas de `new`, `malloc`, `std::string` temporaire, `std::vector` réalloué dans le *hot path*).
- Toute opération lourde (extraction de couleur, FFT, lecture de métadonnées) doit tourner sur un **thread dédié ou thread pool**, jamais sur le thread UI.
- Utiliser **`constexpr`** et **`consteval`** autant que possible pour les calculs compile-time.
- Utiliser **`std::span`**, **`std::string_view`**, **`std::array`** plutôt que leurs équivalents heavies (vector, string, C-array) quand le lifetime le permet.
- Les shaders et effets visuels (flou, glow, squircle) doivent être **GPU-accelérés** (pas de fallback CPU).
- **Zero-copy** pour le passage de données audio (buffer circulaire lock-free pour la WASAPI → waveform).

---

## 3. QUALITÉ & MAINTENABILITÉ DU CODE

- **Tout code généré doit compiler sans warning** avec les warnings max (`/W4` MSVC, `-Wall -Wextra -Wpedantic` GCC/Clang).
- **Pas de magic numbers.** Toute constante doit avoir un nom explicite dans un `constexpr` ou un `enum class`.
- **Nommage :** Conventions du projet :
  - Types (classes, structs, enums) : `PascalCase`
  - Fonctions / méthodes : `camelCase`
  - Variables locales : `snake_case`
  - Variables membres : `m_` prefix (ex: `m_sessionManager`)
  - Constantes compile-time : `kCamelCase` (ex: `kMaxOpacity`)
  - Namespaces : `wavely::core`, `wavely::ui`, `wavely::audio`
- **Chaque fichier `.h` / `.hpp`** doit avoir des **include guards** (`#pragma once`) et ne doit contenir que ce qui est strictement nécessaire (principe de la responsabilité unique).
- **RAII partout.** Pas de `new`/`delete` manuel, pas de handles bruts Win32 non wrappés. Tout objet système (HANDLE, HWND, IAudioCaptureClient) doit avoir un wrapper RAII.
- Les fonctions ne doivent pas dépasser **40 lignes**. Au-delà, les découper en sous-fonctions nommées.
- Les classes ne doivent pas dépasser **~200 lignes**. Au-delà, les découper en modules.
- **Pas de `using namespace std;`** ni `using namespace` dans un header. Toléré temporairement dans un `.cpp` avec justification.
- Chaque fonction publique doit avoir un **bref commentaire Doxygen** ou un commentaire `//` décrivant ce qu'elle fait, pas comment elle le fait.

---

## 4. GESTION DES ERREURS & ROBUSTESSE

- **Ne jamais ignorer silencieusement une erreur.** Tout appel système qui peut échouer doit être vérifié.
- Utiliser les **exceptions** uniquement pour les erreurs véritablement exceptionnelles (échec d'ouverture de device WASAPI, erreur de session GSMTC).
- Pour les erreurs attendues (lecteur non lancé, track sans pochette), utiliser des **`std::optional`** ou des **`std::expected`** (C++23 si disponible, sinon variant/optional avec code d'erreur).
- Toute ressource système acquise doit être **release dans un destructeur** (RAII) ou via `scope_guard`.
- Les callbacks WinRT doivent gérer le cas où l'objet est déjà détruit (capture de `weak_ptr` ou vérification de lifetime).

---

## 5. PERSISTANCE & CONFIGURATION

- Utiliser **`QSettings`** (registre Windows nativement) pour les préférences utilisateur (position, taille, thème, comportement).
- Fichier de config alternatif : **JSON** (via `nlohmann/json` header-only si besoin au-delà de QSettings).
- Les presets (layouts du widget) doivent être stockés dans des fichiers **extérieux** (JSON ou ressources compilées Qt) pour permettre l'ajout futur sans recompilation.
- **Sauvegarde automatique** de la position et taille du widget à chaque déplacement/redimensionnement (pas uniquement à la fermeture, pour éviter la perte en cas de crash).

---

## 6. INTERNATIONALISATION (i18n)

- Toute chaîne visible par l'utilisateur doit être dans un **fichier `.ts` Qt** (ou futur système i18n).
- **Aucune chaîne hardcodée** dans le code C++ ou QML. Toujours utiliser `tr()` ou `qsTr()`.
- Les chaînes doivent être **contextuelles** (ex: `tr("Hide on pause")` et non `tr("Masquer")` seul).
- La structure doit permettre d'ajouter une langue en ajoutant un fichier `.ts` sans modifier le code source.

---

## 7. SÉCURITÉ & CONFIDENTIALITÉ

- **Aucune donnée utilisateur** (écoutée, interceptée, métadonnées de lecture) ne doit quitter la machine locale.
- **Aucune télémétrie, aucun analytics, aucun遥遥 call** sauf si explicitement demandé par l'utilisateur.
- Les flux WASAPI loopback captent l'audio système : les buffers doivent être **purgés immédiatement après traitement FFT** et **jamais persistés sur disque**.
- Si un serveur de mise à jour est implémenté plus tard, il doit utiliser **HTTPS uniquement** avec vérification de certificat.

---

## 8. BUILD & CI

- Le projet doit compiler avec **MSVC 2022+** (toolset v143) et idéalement être testable avec **Clang-CL**.
- **CMake** : pas de scripts `.bat` ou `.ps1` custom si CMake peut tout faire.
- Utiliser les **CMake Presets** (`CMakePresets.json`) pour les configurations Debug/Release/Profile.
- Le build Release doit générer un **binaire unique portable** (pas d'installation, pas de registry beyond QSettings) ou un **installeur léger** (NSIS ou Inno Setup si demandé).
- **Pas de dépendances runtime cachées.** Tout doit être statically linked ou bundlé dans le dossier de l'app.

---

## 9. DOCUMENTATION & COMMUNICATION

- Chaque PR ou commit message doit suivre le format **Conventional Commits** (`feat:`, `fix:`, `perf:`, `refactor:`, `chore:`).
- Les décisions architecturales majeures doivent être documentées dans un fichier `docs/ADR-XXX-nom.md` (Architecture Decision Record).
- Quand tu me proposes du code, **explique brièvement tes choix** avant le bloc de code si le choix n'est pas évident (ex: "J'utilise WASAPI Loopback plutôt que GSMTC pour la waveform car GSMTC n'expose pas le flux audio brut à cause des DRM").

---

## 10. RÈGLES MÉTA (COMMENT TU DOIS RÉPONDRE)

- **Ne jamais inventer d'API Windows inexistante.** Si tu n'es pas sûr de l'existence d'une API, dis-le explicitement et propose une vérification.
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