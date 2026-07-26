import type { PlayerLayoutId } from "../../types/appearance";

export interface LayoutDefinition {
  id: PlayerLayoutId;
  name: string;
  description: string;
  badge?: string;
  showControls: boolean;
  /** Window size the widget resizes to when this layout becomes active. */
  windowSize: { width: number; height: number };
}

export const LAYOUTS: LayoutDefinition[] = [
  {
    id: "compact",
    name: "Compact",
    description: "Pochette, titre/artiste et progression sur une ligne.",
    showControls: true,
    // Height trimmed by the ~32px PlayerControls used to occupy (its 26px
    // tall primary button + 6px margin-top) — controls aren't rendered in
    // the widget for now, so keeping the old height left a disproportionate
    // gap at the bottom.
    windowSize: { width: 360, height: 110 },
  },
  {
    id: "boxy",
    name: "Boxy",
    description: "Pochette large, blocs distincts, barre pleine largeur.",
    showControls: true,
    windowSize: { width: 340, height: 170 },
  },
  {
    id: "gallery",
    name: "Gallery",
    description: "Grande pochette carrée, texte minimal en dessous.",
    showControls: true,
    windowSize: { width: 240, height: 350 },
  },
  {
    id: "minimal",
    name: "Minimal",
    badge: "New",
    description: "Une capsule fine : titre, artiste et progression.",
    showControls: false,
    windowSize: { width: 300, height: 54 },
  },
  {
    id: "macos",
    name: "macOS",
    description: "Style barre de fenêtre macOS.",
    showControls: false,
    windowSize: { width: 340, height: 122 },
  },
  {
    id: "shell",
    name: "Shell",
    description: "Terminal — affichage façon ligne de commande.",
    showControls: false,
    windowSize: { width: 360, height: 156 },
  },
  {
    id: "discord",
    name: "Discord",
    description: "Carte façon statut Discord.",
    showControls: false,
    // 168 (the previous value) over-corrected a cover-overflow bug by
    // growing the window instead of shrinking the cover, leaving the card
    // visibly taller/gappier than the reference's lean, wide proportions.
    // 146 restores that aspect ratio; the cover itself was sized down to
    // actually fit it (see DiscordLayout.svelte).
    windowSize: { width: 360, height: 146 },
  },
];

export function findLayout(id: string): LayoutDefinition {
  return LAYOUTS.find((l) => l.id === id) ?? LAYOUTS[0];
}
