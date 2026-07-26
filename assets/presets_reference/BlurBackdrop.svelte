<script lang="ts">
  import { playerStore } from "../../stores/playerStore.svelte";
  import { appearanceStore } from "../../stores/appearanceStore.svelte";

  let track = $derived(playerStore.raw?.item ?? null);
  let albumArt = $derived(track?.album.images[0]?.url ?? null);
  let show = $derived(appearanceStore.settings.cover_blur && albumArt);
</script>

{#if show}
  <div class="blur-backdrop" style={`background-image: url(${albumArt})`}></div>
{/if}

<style>
  .blur-backdrop {
    position: absolute;
    inset: 0;
    background-size: cover;
    background-position: center;
    filter: blur(24px) brightness(0.55);
    transform: scale(1.2);
    z-index: 0;
  }
</style>
