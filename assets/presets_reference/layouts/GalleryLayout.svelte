<script lang="ts">
  import { playerStore } from "../../../stores/playerStore.svelte";
  import { formatDuration } from "../../player/format";
  import { marqueeClip } from "../../player/marquee";
  import AlbumArt from "../AlbumArt.svelte";

  let track = $derived(playerStore.raw?.item ?? null);
  let artistNames = $derived(track?.artists.map((a) => a.name).join(", ") ?? "");
  let duration = $derived(track?.duration_ms ?? 0);
  let progress = $derived(playerStore.progressMs);
  let progressPercent = $derived(duration > 0 ? Math.min(100, (progress / duration) * 100) : 0);
</script>

<div class="gallery">
  <div class="art-row">
    <AlbumArt fit="width" radius={16} />
  </div>
  <div class="panel info-panel">
    <div class="clip title" use:marqueeClip><span>{track?.name ?? ""}</span></div>
    <div class="clip artist" use:marqueeClip><span>{artistNames}</span></div>
  </div>
  <div class="panel meta-panel">
    <div class="meta-row">
      <span class="time">{formatDuration(progress)}</span>
      <span class="time">{formatDuration(duration)}</span>
    </div>
    <div class="progress-track">
      <div class="progress-fill" style={`width: ${progressPercent}%`}></div>
    </div>
  </div>
</div>

<style>
  .gallery {
    display: flex;
    flex-direction: column;
    gap: 8px;
    height: 100%;
  }

  .art-row {
    flex-shrink: 0;
    display: flex;
    justify-content: center;
  }

  .panel {
    background: color-mix(in srgb, var(--discify-text-primary, #fff) 10%, transparent);
    border-radius: 10px;
    padding: 8px 14px;
    flex-shrink: 0;
  }

  .clip {
    overflow: hidden;
    white-space: nowrap;
  }

  .clip span {
    display: inline-block;
    white-space: nowrap;
  }

  .clip:not(:global(.is-overflowing)) span {
    max-width: 100%;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .clip:global(.is-overflowing) span {
    animation: discify-marquee 6s ease-in-out infinite;
  }

  @media (prefers-reduced-motion: reduce) {
    .clip:global(.is-overflowing) span {
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
    font-size: 1rem;
    font-weight: 700;
    color: var(--discify-text-primary, #fff);
  }

  .artist {
    font-size: 0.82rem;
    color: var(--discify-text-secondary, inherit);
  }

  .meta-row {
    display: flex;
    justify-content: space-between;
    font-size: 0.78rem;
    font-weight: 700;
    color: var(--discify-text-primary, #fff);
    margin-bottom: 6px;
  }

  .progress-track {
    height: 7px;
    border-radius: 3.5px;
    background: color-mix(in srgb, var(--discify-accent, #e2a355) 35%, black);
    overflow: hidden;
  }

  .progress-fill {
    height: 100%;
    border-radius: 3.5px;
    background: var(--discify-accent, #e2a355);
  }
</style>
