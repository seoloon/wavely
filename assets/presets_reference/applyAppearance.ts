import type { Palette } from "../../types/palette";
import type { AppearanceSettings } from "../../types/appearance";

function hexToRgba(hex: string, alpha: number): string {
  const clean = hex.replace("#", "");
  const full = clean.length === 3
    ? clean.split("").map((c) => c + c).join("")
    : clean.padEnd(6, "0").slice(0, 6);
  const r = parseInt(full.slice(0, 2), 16);
  const g = parseInt(full.slice(2, 4), 16);
  const b = parseInt(full.slice(4, 6), 16);
  return `rgba(${r}, ${g}, ${b}, ${alpha})`;
}

/**
 * Applies live appearance settings (colors, opacity, glow) as CSS custom
 * properties on `root`. Structural choices (layout, cover shape) are read
 * directly from the store by components since they change markup/arrangement
 * rather than a single property value.
 *
 * Opacity is baked into the background color's alpha channel rather than
 * applied as a global CSS `opacity` on the widget — that would fade the
 * text/icons too, which reads as broken rather than "translucent".
 */
export function applyAppearance(root: HTMLElement, appearance: AppearanceSettings, palette: Palette | null) {
  const dark = appearance.theme_mode === "dark";
  const useDynamicAccent = appearance.magic_colors && palette !== null;
  // The dominant color painting the *entire* background is what makes very
  // light/bright album art hurt readability — gated separately so accents
  // can stay dynamic while the background/text fall back to the neutral
  // theme.
  const useDynamicBg = useDynamicAccent && appearance.dynamic_background;
  const accent = useDynamicAccent ? palette!.accent : appearance.tint_color;
  const bgHex = useDynamicBg ? palette!.dominant : dark ? "#141418" : "#f5f5f7";
  const textPrimary = useDynamicBg ? (palette!.is_dark ? "#ffffff" : "#141418") : dark ? "#ffffff" : "#141418";
  const textSecondary = useDynamicBg
    ? palette!.is_dark
      ? "rgba(255,255,255,0.72)"
      : "rgba(20,20,24,0.65)"
    : dark
      ? "rgba(255,255,255,0.7)"
      : "rgba(20,20,24,0.6)";

  const vars: Record<string, string> = {
    "--discify-bg": hexToRgba(bgHex, appearance.opacity),
    "--discify-text-primary": textPrimary,
    "--discify-text-secondary": textSecondary,
    "--discify-accent": accent,
    "--discify-cover-glow": appearance.cover_glow ? `0 0 20px 2px ${accent}` : "none",
  };

  for (const [key, value] of Object.entries(vars)) {
    root.style.setProperty(key, value);
  }
}
