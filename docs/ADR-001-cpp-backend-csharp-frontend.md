# ADR-001 — Scission backend C++/WinRT / frontend C#/Avalonia

## Statut
Acceptée (2026-07-26)

## Contexte
L'implémentation initiale de Wavely (Phases 0-3 du `PLAN.md`) a été livrée sous
une architecture Qt6/C++ Widgets monolithique : logique métier, accès système
(GSMTC, registre) et UI vivaient dans le même binaire C++, construit par
`CMakeLists.txt` à la racine.

`claude/PROMPT.md` et `claude/RULES.md` actent une nouvelle cible :
- **Backend** 100% C++ (C++/WinRT) : toute la logique métier et tous les accès
  système (GSMTC, WASAPI, registre, fichiers), compilé comme composant WinRT
  (runtime component, `.winmd` + DLL native).
- **Frontend** 100% C# (.NET 8+ / Avalonia UI) : toute l'UI, aucune logique
  métier.
- **Interop** : projection C#/WinRT (`Microsoft.Windows.CsWinRT`), pas de
  marshaling manuel pour les types couverts par WinRT.

## Décision
Le projet est scindé en deux arbres de sources indépendants :
- `backend/` — composant WinRT C++ (`Wavely.Backend`), voir ADR-002 pour le
  système de build.
- `frontend/` — application Avalonia C# (`Wavely.App`).

L'ancienne implémentation Qt6/C++ (`src/`, `CMakeLists.txt` racine) est
conservée telle quelle pendant la migration — elle reste la seule version
fonctionnelle tant que le portage n'est pas achevé — et sera retirée une fois
que `backend/` + `frontend/` couvrent au moins les Phases 0 à 3 du `PLAN.md`.

## Conséquences
- Deux chaînes de build distinctes à orchestrer (voir ADR-002 et `RULES.md` §8).
- Le portage des Phases 0-3 (tray, GSMTC, drag/resize/click-through, masquage
  différé, auto-start) est un chantier dédié, traité phase par phase après la
  Phase 0 (fondations) de la nouvelle architecture.
- Le code Qt6 legacy n'est plus étendu : les nouvelles fonctionnalités
  atterrissent exclusivement dans `backend/`/`frontend/`.
