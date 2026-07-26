<script lang="ts">
  import { playerStore } from "../../../stores/playerStore.svelte";
  import { formatDuration } from "../../player/format";
  import { marqueeClip } from "../../player/marquee";
  import AlbumArt from "../AlbumArt.svelte";
  import EqualizerBars from "../EqualizerBars.svelte";

  let track = $derived(playerStore.raw?.item ?? null);
  let artistNames = $derived(track?.artists.map((a) => a.name).join(", ") ?? "");
  let duration = $derived(track?.duration_ms ?? 0);
  let progress = $derived(playerStore.progressMs);
  let progressPercent = $derived(duration > 0 ? Math.min(100, (progress / duration) * 100) : 0);
</script>

<div class="boxy">
  <div class="row">
    <AlbumArt size={92} radius={12} />
    <div class="stack">
      <div class="panel info-panel">
        <div class="clip title" use:marqueeClip><span>{track?.name ?? ""}</span></div>
        <div class="clip artist" use:marqueeClip><span>{artistNames}</span></div>
      </div>
      <div class="panel meta-panel">
        <span class="time">{formatDuration(progress)}</span>
        <EqualizerBars active={playerStore.status === "playing"} count={11} height={20} />
        <span class="time">{formatDuration(duration)}</span>
      </div>
    </div>
  </div>
  <div class="progress-track">
    <div class="progress-fill" style={`width: ${progressPercent}%`}></div>
  </div>
</div>

<style>
  .boxy {
    display: flex;
    flex-direction: column;
    gap: 8px;
    height: 100%;
  }

  .row {
    display: flex;
    /* Fixed-size cover vs. content-sized text stack — center them on the
       same axis instead of the default stretch, which pins the cover to
       the top of the row while the panels stack expands past it. */
    align-items: center;
    gap: 10px;
    flex: 1;
    min-height: 0;
  }

  .stack {
    flex: 1;
    display: flex;
    flex-direction: column;
    gap: 8px;
    min-width: 0;
  }

  .panel {
    background: color-mix(in srgb, var(--discify-text-primary, #fff) 10%, transparent);
    border-radius: 10px;
    padding: 8px 14px;
    display: flex;
    flex-direction: column;
    justify-content: center;
    flex: 1;
    min-width: 0;
  }

  .meta-panel {
    flex-direction: row;
    align-items: center;
    justify-content: space-between;
    gap: 8px;
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
    font-size: 1.05rem;
    font-weight: 700;
    color: var(--discify-text-primary, #fff);
  }

  .artist {
    font-size: 0.85rem;
    color: var(--discify-text-secondary, inherit);
  }

  .time {
    font-size: 0.85rem;
    font-weight: 700;
    color: var(--discify-text-primary, #fff);
  }

  .progress-track {
    height: 9px;
    border-radius: 4.5px;
    background: color-mix(in srgb, var(--discify-accent, #e2a355) 35%, black);
    overflow: hidden;
    flex-shrink: 0;
  }

  .progress-fill {
    height: 100%;
    border-radius: 4.5px;
    background: var(--discify-accent, #e2a355);
  }
</style>
