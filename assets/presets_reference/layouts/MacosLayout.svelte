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

<div class="macos">
  <div class="titlebar">
    <div class="traffic-lights" aria-hidden="true">
      <span class="dot red"></span>
      <span class="dot yellow"></span>
      <span class="dot green"></span>
    </div>
    <EqualizerBars active={playerStore.status === "playing"} count={4} height={12} />
  </div>
  <div class="content">
    <AlbumArt fit="height" radius={10} />
    <div class="info">
      <div class="clip title" use:marqueeClip><span>{track?.name ?? ""}</span></div>
      <div class="clip artist" use:marqueeClip><span>{artistNames}</span></div>
      <div class="meta-row">
        <span class="time">{formatDuration(progress)}</span>
        <span class="time">{formatDuration(duration)}</span>
      </div>
      <div class="progress-track">
        <div class="progress-fill" style={`width: ${progressPercent}%`}></div>
      </div>
    </div>
  </div>
</div>

<style>
  .macos {
    display: flex;
    flex-direction: column;
    height: 100%;
    background: color-mix(in srgb, var(--discify-bg, #2b2b2f) 92%, black);
    border-radius: 12px;
    overflow: hidden;
  }

  .titlebar {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 8px 12px;
    border-bottom: 2px solid var(--discify-accent, #4a7fd6);
    flex-shrink: 0;
  }

  .traffic-lights {
    display: flex;
    gap: 6px;
  }

  .dot {
    width: 11px;
    height: 11px;
    border-radius: 50%;
  }

  .dot.red {
    background: #ff5f57;
  }

  .dot.yellow {
    background: #febc2e;
  }

  .dot.green {
    background: #28c840;
  }

  .content {
    display: flex;
    /* Stretch, not center — .content has a definite height here (flex:1
       against the .macos column's own fixed height), so this makes the
       cover (fit="height") exactly match the .info column's height instead
       of the two being independently sized and merely centered together. */
    align-items: stretch;
    gap: 12px;
    padding: 10px 14px;
    flex: 1;
    min-height: 0;
  }

  .info {
    flex: 1;
    min-width: 0;
    display: flex;
    flex-direction: column;
    justify-content: center;
    gap: 4px;
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
    justify-content: space-between;
    font-size: 0.7rem;
    font-weight: 700;
    color: var(--discify-text-primary, #fff);
    margin-top: 2px;
  }

  .progress-track {
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
