# Wavely — Instructions pour Claude Code

Avant toute tâche sur ce projet, lis dans l'ordre :
1. `claude/PROMPT.md` — contexte produit, architecture cible, mission
2. `claude/RULES.md` — règles permanentes (conventions C++/C#, performance, build...) à respecter à chaque réponse, sauf indication contraire explicite de l'utilisateur
3. `claude/PLAN.md` — plan de développement phase par phase, avec le statut actuel

## Statut actuel (2026-07-26)

- **Architecture cible** : backend 100% C++/WinRT (composant runtime `.winmd`) + frontend 100% C#/.NET avec Avalonia UI, interop via la projection C#/WinRT (`Microsoft.Windows.CsWinRT`). Voir `claude/RULES.md` en tête de fichier.
- **Le code source actuel** (`src/`, `CMakeLists.txt` à la racine) est l'**ancienne** implémentation Qt6/C++ Widgets des Phases 0 à 3 du plan (tray, intégration GSMTC, drag/resize/click-through, masquage différé, auto-start) — fonctionnelle, testée, mais **pas encore migrée** vers l'architecture ci-dessus.
- Avant d'écrire du code, vérifie l'état réel du repo (présence ou non d'un projet composant C++/WinRT et d'un `.csproj` Avalonia) plutôt que de supposer que la migration a eu lieu — les documents `claude/*.md` décrivent la cible, pas nécessairement l'état courant.
