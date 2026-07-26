<script lang="ts">
  import { playerStore } from "../../../stores/playerStore.svelte";
  import { formatDuration } from "../../player/format";
  import { marqueeClip } from "../../player/marquee";
  import AlbumArt from "../AlbumArt.svelte";
  import EqualizerBars from "../EqualizerBars.svelte";
  import BlurBackdrop from "../BlurBackdrop.svelte";

  let track = $derived(playerStore.raw?.item ?? null);
  let artistNames = $derived(track?.artists.map((a) => a.name).join(", ") ?? "");
  let duration = $derived(track?.duration_ms ?? 0);
  let progress = $derived(playerStore.progressMs);
  let progressPercent = $derived(duration > 0 ? Math.min(100, (progress / duration) * 100) : 0);
</script>

<div class="discord">
  <div class="blur-clip"><BlurBackdrop /></div>
  <div class="header">
    <span class="wordmark">Discify</span>
    <EqualizerBars active={playerStore.status === "playing"} count={4} height={16} />
  </div>
  <div class="body">
    <div class="art-col">
      <div class="art-wrap">
        <div class="active-indicator" aria-hidden="true"></div>
        <AlbumArt size={66} radius={13} />
      </div>
      <span class="chrome-pill" aria-hidden="true"></span>
    </div>
    <div class="info">
      <div class="clip title" use:marqueeClip><span>{track?.name ?? ""}</span></div>
      <div class="clip artist" use:marqueeClip><span>{artistNames}</span></div>
      <div class="progress-row">
        <span class="time">{formatDuration(progress)}</span>
        <div class="progress-track">
          <div class="progress-fill" style={`width: ${progressPercent}%`}></div>
          <div class="progress-thumb" style={`left: ${progressPercent}%`}></div>
        </div>
        <span class="time">{formatDuration(duration)}</span>
      </div>
    </div>
  </div>
</div>

<style>
  .discord {
    /* Stays overflow:visible — .active-indicator (below) is deliberately
       positioned to poke out past this card's left edge, which a
       overflow:hidden here would clip away. The blur backdrop needs its own
       dedicated clip region instead — see .blur-clip. */
    position: relative;
    display: flex;
    flex-direction: column;
    gap: 8px;
    height: 100%;
    background: #2b2d31;
    border-radius: 16px;
    /* Left margin, not padding — leaves room outside the card's own rounded
       border for .active-indicator to poke out into, matching the reference
       (the bar visibly overlaps/exceeds the card edge, not sitting flush
       inside it). */
    margin-left: 8px;
    padding: 10px 16px 10px 20px;
    box-sizing: border-box;
  }

  /* Sized/rounded to exactly match .discord's own box and clips its one
     child (BlurBackdrop) to that shape — separate from .discord's own
     overflow (which must stay visible for .active-indicator). Without this,
     the blurred image is sized to the outer *widget*, which doesn't match
     this card's left margin or border-radius, so it showed through in that
     gap and at the mismatched corners instead of being clipped to the card. */
  .blur-clip {
    position: absolute;
    inset: 0;
    overflow: hidden;
    border-radius: 16px;
    z-index: 0;
  }

  /* BlurBackdrop is position:absolute with z-index:0 — an absolutely
     positioned element always paints above plain in-flow content regardless
     of that z-index, so .header/.body need their own stacking position to
     actually render on top of it instead of being covered by it. */
  .header {
    position: relative;
    z-index: 1;
    display: flex;
    align-items: center;
    justify-content: space-between;
  }

  .wordmark {
    font-family: "gg sans", "Poppins", "Nunito", "Segoe UI", sans-serif;
    font-weight: 800;
    font-size: 1.6rem;
    letter-spacing: -0.02em;
    color: #dbdee1;
  }

  .body {
    position: relative;
    z-index: 1;
    display: flex;
    gap: 14px;
    flex: 1;
    min-height: 0;
  }

  .art-col {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 6px;
    flex-shrink: 0;
  }

  /* Positioned against .art-wrap (just the cover) rather than .body or
     .art-col — those also include the chrome-pill below the cover, which
     pulls their own vertical center a few px lower than the cover's actual
     center, throwing the bar visibly out of alignment with it. */
  .art-wrap {
    position: relative;
  }

  .active-indicator {
    position: absolute;
    left: -23px;
    top: 50%;
    transform: translateY(-50%);
    width: 8px;
    height: 60%;
    border-radius: 4px;
    background: #fff;
  }

  .chrome-pill {
    width: 36px;
    height: 4px;
    border-radius: 2px;
    background: #4a4d54;
  }

  .info {
    flex: 1;
    min-width: 0;
    display: flex;
    flex-direction: column;
    justify-content: center;
    gap: 5px;
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
    font-size: 1.3rem;
    font-weight: 800;
    color: var(--discify-text-primary, #fff);
  }

  .artist {
    font-size: 1rem;
    color: var(--discify-text-secondary, inherit);
  }

  .progress-row {
    display: flex;
    align-items: center;
    gap: 12px;
    margin-top: 6px;
  }

  .time {
    font-size: 1rem;
    font-weight: 800;
    color: var(--discify-text-primary, #fff);
    flex-shrink: 0;
  }

  .progress-track {
    position: relative;
    flex: 1;
    height: 7px;
    border-radius: 3.5px;
    background: #4a5568;
  }

  .progress-fill {
    height: 100%;
    border-radius: 3.5px;
    background: var(--discify-accent, #e2a355);
  }

  .progress-thumb {
    position: absolute;
    top: 50%;
    width: 8px;
    height: 20px;
    border-radius: 4px;
    background: #fff;
    transform: translate(-50%, -50%);
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.4);
  }
</style>
