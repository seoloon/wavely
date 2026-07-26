# ADR-002 — Le composant backend WinRT est construit via MSBuild, pas CMake

## Statut
Acceptée (2026-07-26)

## Contexte
`claude/RULES.md` §8 impose CMake 3.24+ pour le backend, y compris pour
produire le composant WinRT (`.winmd` + DLL native).

En pratique, la génération d'un `.winmd` à partir d'un IDL C++/WinRT (étape
`midl.exe` puis `cppwinrt.exe -component`) n'a pas de chemin CMake mature et
fiable :
- Le générateur CMake pour Visual Studio expose `VS_WINRT_COMPONENT`, mais
  cette propriété cible le C++/CX historique, pas le C++/WinRT moderne à base
  d'IDL — constat partagé par la communauté CMake elle-même
  ([discussion CMake Discourse](https://discourse.cmake.org/t/creating-reliable-c-winrt-builds-with-cmake/3375),
  aucune solution fonctionnelle n'y est démontrée).
- Le support CMake expérimental du Windows App SDK (`add_cppwinrt_projection`,
  depuis 2.0.0-experimental7) est explicitement documenté comme précoce et
  instable par Microsoft ([discussion #6446](https://github.com/microsoft/WindowsAppSDK/discussions/6446)) :
  fichiers manquants dans certaines versions du package NuGet, patterns
  susceptibles de changer.
- Le seul chemin documenté et éprouvé par Microsoft est le projet MSBuild
  « Windows Runtime Component (C++/WinRT) » (`.vcxproj`), piloté par les
  `.targets` du package NuGet `Microsoft.Windows.CppWinRT`.

Décision validée par l'utilisateur le 2026-07-26 (option recommandée) après
présentation des trois alternatives (MSBuild vcxproj / CMake custom_command
brut / CMake expérimental Windows App SDK).

## Décision
Le composant backend WinRT (`backend/Wavely.Backend/Wavely.Backend.vcxproj`)
est un projet MSBuild standard « Windows Runtime Component (C++/WinRT) »,
utilisant le package NuGet `Microsoft.Windows.CppWinRT` pour la chaîne
`midl.exe` → `.winmd` → `cppwinrt.exe -component` → headers de projection.

Ceci constitue une exception documentée à `RULES.md` §8, dans le même esprit
que l'exception déjà actée pour le « script d'orchestration / solution Visual
Studio mixte C++/C# » : CMake ne peut pas piloter nativement cette chaîne
d'outils, comme il ne peut pas piloter `dotnet build`.

Toute logique C++ pure sans dépendance WinRT directe (ex : traitement du
signal WASAPI) reste écrite pour être testable indépendamment de `.vcxproj`
si une telle séparation s'avère utile plus tard ; elle est compilée au sein du
même projet MSBuild tant qu'aucun besoin concret de découplage n'apparaît
(YAGNI).

## Conséquences
- `backend/Wavely.Backend.sln` (ou une solution racine `Wavely.sln`) devient
  le point d'entrée de build du backend, via `msbuild` / `dotnet build` (MSBuild
  sait construire des `.vcxproj`) au lieu de `cmake --build`.
- Le frontend référence directement le `.winmd` généré (via
  `Microsoft.Windows.CsWinRT` côté `.csproj`), sans étape CMake intermédiaire.
- `docs/TECHNICAL.md` documente la nouvelle procédure de build.

## Addendum (2026-07-26) — pièges rencontrés lors de la mise en service

Découverts en construisant et **exécutant** réellement le premier binaire de
bout en bout (pas seulement en compilant) — à connaître avant de reproduire ce
montage ailleurs ou d'ajouter une nouvelle runtime class :

1. **Le SDK .NET et MSBuild de Visual Studio ne peuvent pas se construire l'un
   l'autre.** `dotnet build` ne résout pas `$(VCTargetsPath)` (pas de
   toolset C++ enregistré) ; le `MSBuild.exe` de VS sur cette machine n'a pas
   le résolveur SDK `Microsoft.NET.Sdk`. Un `<ProjectReference>` du `.csproj`
   vers le `.vcxproj` échoue donc des deux côtés. Le frontend référence à la
   place les artefacts déjà construits du backend via `<CsWinRTInputs>`
   (voir point 2) + une copie explicite du `.dll`, jamais via
   `<ProjectReference>` ni `<Reference><HintPath>`.
2. **`<Reference><HintPath>` vers un `.winmd` casse tout le build C#.**
   `ResolveAssemblyReferences` ne sait pas lire les métadonnées d'un `.winmd`
   de composant natif (« PE image does not have metadata », `MSB3246`) et
   peut faire échouer la résolution de **toutes** les autres références
   (y compris les références implicites du framework — symptôme observé :
   `STAThreadAttribute` introuvable). Le point d'entrée documenté et fiable
   pour un `.winmd` externe brut est l'item `<CsWinRTInputs Include="...">`
   (avec métadonnée `<Implementation>` pour le nom du `.dll` natif), qui
   contourne entièrement `ResolveAssemblyReferences`.
3. **`.winmd` + `.dll` seuls ne suffisent pas à activer les classes.** Deux
   pièces supplémentaires, absentes du strict minimum "IDL + implémentation",
   sont nécessaires pour qu'une classe WinRT native soit réellement
   activable depuis le frontend :
   - `<WindowsDesktopCompatible>true</WindowsDesktopCompatible>` dans le
     `.vcxproj` (sinon `CO_E_ERRORINDLL` côté C#).
   - Un fichier `.def` exportant `DllGetActivationFactory` et
     `DllCanUnloadNow` (alias de `WINRT_GetActivationFactory`/
     `WINRT_CanUnloadNow`, générés par `module.g.cpp` mais **jamais exportés
     sans `.def`** — le NuGet `Microsoft.Windows.CppWinRT` ne le fait pas ;
     seul le template Visual Studio le fournissait). Sans lui, le `.dll` n'a
     strictement aucun export et l'activation échoue.
   - Une entrée `<activatableClass>` par runtime class dans `app.manifest`
     du frontend (registration-free WinRT — l'app n'étant pas empaquetée en
     MSIX, rien n'enregistre les classes dans le registre). **Toute nouvelle
     runtime class ajoutée côté backend doit être ajoutée à
     `frontend/Wavely.App/app.manifest` et, si elle vit dans un nouveau
     `.idl`, au `.def`** — sinon elle compile des deux côtés mais lève
     `REGDB_E_CLASSNOTREG` / `CO_E_ERRORINDLL` à l'exécution.
4. **`dpiAwareness` (PerMonitorV2) peut faire échouer l'activation du
   manifeste entier** sur certaines configurations (« paramètre ... non
   inscrit », `SideBySide` event id 79) ; `dpiAware=true/PM` (V1) suffit et
   est plus largement supporté.

Ces quatre points ont été validés empiriquement : build réel + lancement
réel du binaire, GSMTC en direct (session média du navigateur détectée,
cover/titre/statut affichés dans le widget).
