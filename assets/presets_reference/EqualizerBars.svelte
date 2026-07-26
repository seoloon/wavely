<script lang="ts">
  let { active = false, count = 4, height = 14 }: { active?: boolean; count?: number; height?: number } = $props();

  // Fixed-looking but slightly irregular bar heights so it reads as a
  // waveform/equalizer glyph rather than a row of identical ticks, even
  // when not animating (prefers-reduced-motion, or simply paused).
  const pattern = [0.5, 1, 0.65, 0.85, 0.4, 0.9, 0.6];
  let bars = $derived(Array.from({ length: count }, (_, i) => pattern[i % pattern.length]));
</script>

<div class="equalizer" class:active style={`height: ${height}px`} aria-hidden="true">
  {#each bars as scale, i (i)}
    <span class="bar" style={`--scale: ${scale}; animation-delay: ${i * 0.12}s`}></span>
  {/each}
</div>

<style>
  .equalizer {
    display: inline-flex;
    align-items: center;
    gap: 2px;
  }

  .bar {
    display: block;
    width: 3px;
    height: 100%;
    border-radius: 2px;
    background: var(--discify-accent, #e2a355);
    transform: scaleY(var(--scale, 0.6));
    transform-origin: center;
  }

  .equalizer.active .bar {
    animation: discify-eq 0.9s ease-in-out infinite alternate;
  }

  @media (prefers-reduced-motion: reduce) {
    .equalizer.active .bar {
      animation: none;
    }
  }

  @keyframes discify-eq {
    from {
      transform: scaleY(calc(var(--scale, 0.6) * 0.5));
    }
    to {
      transform: scaleY(var(--scale, 0.6));
    }
  }
</style>
