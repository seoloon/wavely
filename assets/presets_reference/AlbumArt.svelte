<script lang="ts">
  import { playerStore } from "../../stores/playerStore.svelte";
  import { appearanceStore } from "../../stores/appearanceStore.svelte";

  let {
    size,
    radius,
    fit,
  }: { size?: number; radius?: number; fit?: "width" | "height" } = $props();

  let track = $derived(playerStore.raw?.item ?? null);
  let albumArt = $derived(track?.album.images[0]?.url ?? null);
  let appearance = $derived(appearanceStore.settings);
  let coverShape = $derived(appearance.cover_shape);
  let showArt = $derived(coverShape !== "none");
</script>

{#if showArt}
  <div
    class="art"
    class:glow={appearance.cover_glow}
    class:vinyl={coverShape === "vinyl"}
    class:canvas={coverShape === "canvas"}
    class:spinning={coverShape === "vinyl" && playerStore.status === "playing"}
    class:fit-width={fit === "width"}
    class:fit-height={fit === "height"}
    style={`${size !== undefined ? `--art-size: ${size}px;` : ""} ${radius !== undefined ? `--art-radius: ${radius}px;` : ""}`}
  >
    {#if albumArt}
      <img src={albumArt} alt="" draggable="false" />
    {:else}
      <div class="art-placeholder"></div>
    {/if}
  </div>
{/if}

<style>
  .art {
    flex-shrink: 0;
    border-radius: var(--art-radius, 6px);
    overflow: hidden;
    transition:
      box-shadow 0.25s ease,
      border-radius 0.25s ease;
  }

  .art:not(.fit-width):not(.fit-height) {
    width: var(--art-size, 48px);
    height: var(--art-size, 48px);
  }

  /* Fill the row's cross axis instead of a fixed pixel size, so the cover's
     rendered size always matches a sibling element (e.g. the text column)
     rather than an arbitrary constant that can drift out of sync with it. */
  .art.fit-width {
    width: 100%;
    height: auto;
    aspect-ratio: 1;
  }

  .art.fit-height {
    width: auto;
    height: 100%;
    aspect-ratio: 1;
  }

  .art.canvas {
    border-radius: calc(var(--art-radius, 6px) + 8px);
    box-shadow: inset 0 0 0 1px rgba(255, 255, 255, 0.12);
  }

  .art.vinyl {
    border-radius: 50%;
  }

  .art.vinyl img {
    border-radius: 50%;
  }

  .art.spinning img {
    animation: discify-spin 4s linear infinite;
  }

  @media (prefers-reduced-motion: reduce) {
    .art.spinning img {
      animation: none;
    }
  }

  @keyframes discify-spin {
    to {
      transform: rotate(360deg);
    }
  }

  .art.glow {
    box-shadow: var(--discify-cover-glow, none);
  }

  .art img {
    width: 100%;
    height: 100%;
    object-fit: cover;
    display: block;
    -webkit-user-drag: none;
  }

  .art-placeholder {
    width: 100%;
    height: 100%;
    background: rgba(255, 255, 255, 0.08);
  }
</style>
