# Refonte visuelle de la fenêtre Settings

**Date** : 2026-07-31
**Statut** : Approuvé

## Contexte

`frontend/Wavely.App/Views/SettingsWindow.axaml` (Avalonia, `FluentTheme` par défaut sans override) affiche ses 3 sections (Comportement, Apparence, À propos) dans un `TabControl` standard avec `CheckBox`/`ComboBox`/`Slider` au look Fluent par défaut — jugé peu soigné et peu pratique par rapport au reste de l'app (le widget principal a déjà une identité visuelle propre : fond sombre `#141418`, coins arrondis, accent bleu `#5AAAFF`).

Objectif : moderniser l'apparence de cette fenêtre, sans toucher au comportement ni à la logique métier.

## Périmètre

**Dans le périmètre** :
- Remplacement de la navigation par onglets par une sidebar gauche (icône + label), un item sélectionné par section.
- Restyle des contrôles (CheckBox → ToggleSwitch, accent de marque sur sliders/toggle/bouton principal).
- Regroupement visuel des réglages liés en "cartes" avec en-têtes de section.
- Agrandissement de la fenêtre (~600×480) pour accueillir la sidebar.

**Hors périmètre** (explicitement exclu) :
- Aucun changement de `SettingsViewModel` (mêmes propriétés, mêmes commandes, mêmes bindings).
- Aucun ajout/suppression de réglage.
- Le chrome de fenêtre reste natif (titlebar Windows standard) — pas de fenêtre borderless custom.
- Pas de changement à `App.axaml` global : les styles ajoutés sont scoped aux ressources de `SettingsWindow.axaml` pour ne pas impacter d'autres fenêtres/contrôles.

## Design

### Layout

- Fenêtre `Width="600" Height="480"`, `CanResize="False"` (inchangé), titlebar native Windows.
- `Grid` racine à deux colonnes : sidebar gauche (`Width="180"`) / zone de contenu (`*`).
- Sidebar : liste verticale de 3 entrées (Comportement, Apparence, À propos), chacune = icône (glyphe Segoe Fluent Icons via `TextBlock FontFamily="Segoe Fluent Icons"`) + label. Sélection pilotée par un `int SelectedSectionIndex` géré côté vue (pas besoin de le remonter au ViewModel — c'est un état de navigation UI pur, pas une préférence persistée).
- Contenu à droite : un `ScrollViewer` par section, affiché/masqué selon `SelectedSectionIndex` (ou un `Carousel`/`ContentControl` simple avec `DataTemplate` par index — au choix de l'implémentation, tant que le contenu de chaque section reste identique à l'existant).
- Le bouton "Recharger le widget" reste ancré en bas à droite de la fenêtre, visible quelle que soit la section active (comme aujourd'hui, `DockPanel.Dock="Bottom"`).

### Style visuel

- Couleurs par `DynamicResource` (brushes Fluent standard `SystemControlBackgroundChromeMediumLowBrush` etc.) pour que le thème clair/sombre piloté par le sélecteur "Thème" (Apparence) continue de fonctionner sans logique supplémentaire.
- Couleur d'accent de marque `#5AAAFF` définie une fois en resource locale (`<SolidColorBrush x:Key="BrandAccentBrush" Color="#5AAAFF"/>`) dans `SettingsWindow.axaml.Resources`, utilisée pour :
  - la barre verticale + fond léger de l'item sidebar sélectionné,
  - la couleur "On" des `ToggleSwitch`,
  - le thumb/track actif des `Slider`,
  - le bouton "Recharger le widget" (bouton plein accent).
- Regroupement des réglages liés en cartes : `Border` avec `CornerRadius="8"`, fond légèrement teinté (`DynamicResource` d'un brush de fond secondaire), `Padding="16"`, `Spacing` interne 12-16px. En-tête de section en `FontWeight="Bold"` au-dessus de chaque carte (ou groupe de cartes).
- Espacements généraux resserrés/homogénéisés (16-20px entre cartes, 12px entre champs d'une même carte) plutôt que le `Spacing="16"` plat actuel.

### Contrôles

- `CheckBox` → `ToggleSwitch` pour tous les booléens : Verrouiller, Click-through, Masquage sur pause, Lancer au démarrage, Couleurs dynamiques, Fond couleur dominante, Glow, Fond flouté.
- `ComboBox`, `Slider`, `ColorPicker` conservés tels quels fonctionnellement, seule leur couleur d'accent est overridée localement (`ControlTheme` scoped aux resources de la fenêtre, pas global).
- Les swatches de couleur rapide (Spotify/Deezer/Apple Music/YouTube/Noir/Blanc) et le `ColorPicker` personnalisé restent inchangés (déjà custom).

### Implémentation

- Tout le travail se fait dans `frontend/Wavely.App/Views/SettingsWindow.axaml` (+ éventuellement `SettingsWindow.axaml.cs` pour la logique de sélection de section si elle ne peut pas être un pur binding déclaratif).
- Aucun fichier backend, aucun `SettingsViewModel*.cs` à modifier.
- Pas de nouvelle dépendance NuGet (tout est faisable avec Avalonia/FluentTheme stock + `ToggleSwitch` qui existe déjà dans Avalonia.Controls).

## Test manuel

- Ouvrir Settings depuis le tray → sidebar visible, 3 sections navigables, contenu correspondant à chaque section identique aux champs actuels.
- Basculer chaque ToggleSwitch → vérifier que le binding fonctionne toujours (comportement observable inchangé, ex. Click-through applique bien `WS_EX_TRANSPARENT` en direct comme avant).
- Basculer Thème Clair/Sombre → la fenêtre Settings elle-même reste lisible et cohérente dans les deux thèmes (contrairement à un design qui hardcoderait du sombre).
- Bouton "Recharger le widget" toujours fonctionnel depuis n'importe quelle section.
