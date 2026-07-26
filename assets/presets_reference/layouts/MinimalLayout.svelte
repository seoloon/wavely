<script lang="ts">
  import { playerStore } from "../../../stores/playerStore.svelte";
  import { marqueeClip } from "../../player/marquee";
  import AlbumArt from "../AlbumArt.svelte";

  let track = $derived(playerStore.raw?.item ?? null);
  let artistNames = $derived(track?.artists.map((a) => a.name).join(", ") ?? "");
  let duration = $derived(track?.duration_ms ?? 0);
  let progress = $derived(playerStore.progressMs);
  let progressPercent = $derived(duration > 0 ? Math.min(100, (progress / duration) * 100) : 0);
</script>

<div class="minimal">
  <AlbumArt size={34} radius={17} />
  <div class="clip label" use:marqueeClip title={`${track?.name ?? ""} — ${artistNames}`}>
    <span>
      <span class="title">{track?.name ?? ""}</span>
      <span class="dot">•</span>
      <span class="artist">{artistNames}</span>
    </span>
  </div>
  <div class="progress-track">
    <div class="progress-fill" style={`width: ${progressPercent}%`}></div>
  </div>
</div>

<style>
  .minimal {
    display: flex;
    align-items: center;
    gap: 10px;
    height: 100%;
    background: color-mix(in srgb, var(--discify-text-primary, #fff) 10%, transparent);
    border-radius: 999px;
    /* Cover is sized to exactly fill the pill's height (34px art in a 34px
       capsule), so its circle is concentric with the pill's own end-cap
       curve only with zero left inset — any left padding here throws that
       nesting off and the circle visibly cuts into the rounded corner. */
    padding: 0 14px 0 0;
  }

  .label {
    flex-shrink: 1;
    min-width: 40px;
    overflow: hidden;
    white-space: nowrap;
    font-size: 0.85rem;
  }

  .label > span {
    display: inline-block;
    white-space: nowrap;
  }

  .label:not(:global(.is-overflowing)) > span {
    max-width: 100%;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .label:global(.is-overflowing) > span {
    animation: discify-marquee 6s ease-in-out infinite;
  }

  @media (prefers-reduced-motion: reduce) {
    .label:global(.is-overflowing) > span {
      animation: none !important;
      text-overflow: ellipsis;
      max-width: 100%;
      overflow: hidden;
    }
  }

  @keyframes discify-marquee {
    0%,
    15% {
      transform: translateX(0);
    }
    45%,
    55% {
      transform: translateX(var(--marquee-shift, 0));
    }
    85%,
    100% {
      transform: translateX(0);
    }
  }

  .title {
    font-weight: 700;
    color: var(--discify-text-primary, #fff);
  }

  .dot {
    margin: 0 4px;
    color: var(--discify-text-secondary, inherit);
    opacity: 0.6;
  }

  .artist {
    color: var(--discify-text-secondary, inherit);
  }

  .progress-track {
    flex: 1;
    min-width: 40px;
    height: 5px;
    border-radius: 3px;
    background: color-mix(in srgb, var(--discify-accent, #e2a355) 35%, black);
    overflow: hidden;
  }

  .progress-fill {
    height: 100%;
    border-radius: 3px;
    background: var(--discify-accent, #e2a355);
  }
</style>
