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

<div class="compact">
  <AlbumArt size={72} radius={10} />
  <div class="panel info-panel">
    <div class="clip title" use:marqueeClip><span>{track?.name ?? ""}</span></div>
    <div class="clip artist" use:marqueeClip><span>{artistNames}</span></div>
  </div>
  <div class="panel meta-panel">
    <div class="meta-row">
      <span class="time">{formatDuration(progress)}</span>
      <EqualizerBars active={playerStore.status === "playing"} count={5} height={13} />
      <span class="time">{formatDuration(duration)}</span>
    </div>
    <div class="progress-track">
      <div class="progress-fill" style={`width: ${progressPercent}%`}></div>
    </div>
  </div>
</div>

<style>
  .compact {
    display: flex;
    /* Album art is a fixed size while the text panels size to their content —
       centering keeps the cover vertically level with both panels instead of
       stretch pinning it to the top of a taller row. */
    align-items: center;
    gap: 10px;
    height: 100%;
  }

  .panel {
    background: color-mix(in srgb, var(--discify-text-primary, #fff) 10%, transparent);
    border-radius: 10px;
    padding: 8px 14px;
    display: flex;
    flex-direction: column;
    justify-content: center;
    min-width: 0;
  }

  .info-panel {
    flex: 1.1;
    gap: 3px;
  }

  .meta-panel {
    flex: 1;
    gap: 6px;
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
    font-size: 0.95rem;
    font-weight: 700;
    color: var(--discify-text-primary, #fff);
  }

  .artist {
    font-size: 0.8rem;
    color: var(--discify-text-secondary, inherit);
  }

  .meta-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 6px;
  }

  .time {
    font-size: 0.78rem;
    font-weight: 700;
    color: var(--discify-text-primary, #fff);
  }

  .progress-track {
    height: 6px;
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
