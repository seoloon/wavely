<script lang="ts">
  import { playerStore } from "../../../stores/playerStore.svelte";
  import { formatDuration } from "../../player/format";
  import { marqueeClip } from "../../player/marquee";

  const BAR_WIDTH = 26;

  let track = $derived(playerStore.raw?.item ?? null);
  let artistNames = $derived(track?.artists.map((a) => a.name).join(", ") ?? "");
  let duration = $derived(track?.duration_ms ?? 0);
  let progress = $derived(playerStore.progressMs);
  let progressPercent = $derived(duration > 0 ? Math.min(1, progress / duration) : 0);
  let filled = $derived(Math.round(progressPercent * BAR_WIDTH));
  let asciiBar = $derived("#".repeat(filled) + "-".repeat(BAR_WIDTH - filled));
</script>

<div class="shell">
  <div class="titlebar">
    <span class="window-title">root@discify</span>
    <div class="window-controls" aria-hidden="true">
      <span class="ctrl">–</span>
      <span class="ctrl">▢</span>
      <span class="ctrl close">✕</span>
    </div>
  </div>
  <div class="body">
    <p class="line">
      <span class="prompt">root@discify&gt;</span><span class="cmd">./discify</span>
      <span class="flag"> --nowplaying</span>
    </p>
    <p class="line row">
      <span class="key">Title:</span>
      <span class="clip" use:marqueeClip><span>{track?.name ?? ""}</span></span>
    </p>
    <p class="line row">
      <span class="key">Artist:</span>
      <span class="clip" use:marqueeClip><span>{artistNames}</span></span>
    </p>
    <p class="line bar">[<span class="filled">{"#".repeat(filled)}</span><span class="empty">{"-".repeat(BAR_WIDTH - filled)}</span>]</p>
    <p class="line times">{formatDuration(progress)} - {formatDuration(duration)}</p>
  </div>
</div>

<style>
  .shell {
    display: flex;
    flex-direction: column;
    height: 100%;
    background: #1b1e26;
    border-radius: 12px;
    overflow: hidden;
    font-family: "Cascadia Code", "Consolas", "SFMono-Regular", Menlo, monospace;
  }

  .titlebar {
    display: flex;
    align-items: center;
    justify-content: center;
    position: relative;
    padding: 7px 10px;
    background: #23262f;
    flex-shrink: 0;
  }

  .window-title {
    font-size: 0.78rem;
    font-weight: 700;
    color: #e8e8ec;
  }

  .window-controls {
    position: absolute;
    right: 8px;
    top: 50%;
    transform: translateY(-50%);
    display: flex;
    gap: 6px;
    font-size: 0.6rem;
    color: #9a9aa5;
  }

  .ctrl {
    width: 15px;
    height: 15px;
    display: flex;
    align-items: center;
    justify-content: center;
  }

  .ctrl.close {
    background: #e0507a;
    border-radius: 50%;
    color: #fff;
  }

  .body {
    padding: 8px 12px;
    display: flex;
    flex-direction: column;
    gap: 3px;
    flex: 1;
    min-height: 0;
    overflow: hidden;
  }

  .line {
    margin: 0;
    font-size: 0.72rem;
    line-height: 1.35;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    color: #f2f2f5;
  }

  .line.row {
    display: flex;
    align-items: center;
    gap: 0.3em;
  }

  .clip {
    flex: 1;
    min-width: 0;
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

  .prompt {
    color: var(--discify-accent, #e2a355);
    font-weight: 700;
  }

  .cmd {
    color: #f0d0a8;
  }

  .flag {
    color: #8a8a95;
  }

  .key {
    flex-shrink: 0;
    color: #f2f2f5;
    font-weight: 700;
  }

  .bar {
    letter-spacing: -0.5px;
  }

  .filled {
    color: var(--discify-accent, #e2a355);
  }

  .empty {
    color: #5a5a66;
  }

  .times {
    font-weight: 700;
  }
</style>
