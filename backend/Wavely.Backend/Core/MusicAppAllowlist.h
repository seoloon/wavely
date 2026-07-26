#pragma once

#include <algorithm>
#include <array>
#include <cwctype>
#include <string_view>

namespace Wavely::Backend::Core
{
    /// A native music-streaming app Wavely reacts to - both for choosing which GSMTC session
    /// counts as "now playing" (MediaSessionManager) and for choosing which process's audio to
    /// loopback-capture (WaveformEngine). Deliberately excludes browser-based sources (YouTube
    /// Music and similar): a browser's GSMTC session reports the browser itself as the source
    /// (observed directly on this project - Brave reports the bare AUMID "Brave", indistinguishable
    /// from any other tab/site playing media), so there is no reliable way to isolate "just YouTube
    /// Music" from "any video on any site" at this API surface.
    ///
    /// Matching is substring/case-insensitive on purpose: packaged (MSIX/Store) apps report a
    /// SourceAppUserModelId like "SpotifyAB.SpotifyMusic_<hash>!Spotify" where the hash suffix
    /// isn't guaranteed stable across installs/regions, and Win32 desktop apps report whatever
    /// bare string they self-registered. Fragment matching is robust to both without needing a
    /// fully-verified exact identifier per app.
    ///
    /// Only Spotify was confirmed against a live running instance this session (AUMID observed:
    /// "SpotifyAB.SpotifyMusic_zpdnekdrzrea0!Spotify"; process image name: "Spotify.exe", which
    /// Spotify's Chromium-based client runs as multiple processes under - see WaveformEngine's
    /// process discovery, which matches by *active audio session* rather than by picking an
    /// arbitrary same-named PID). Deezer/TIDAL/Apple Music entries are best-effort based on their
    /// well-known desktop process names and are unverified - if a user reports one doesn't match,
    /// extend this table with the real observed AUMID/process name.
    struct MusicApp
    {
        std::wstring_view name;
        std::wstring_view aumidFragment;
        std::wstring_view processNameFragment;
    };

    inline constexpr std::array<MusicApp, 4> kMusicAppAllowlist{ {
        { L"Spotify", L"spotify", L"spotify" },
        { L"Deezer", L"deezer", L"deezer" },
        { L"TIDAL", L"tidal", L"tidal" },
        { L"Apple Music", L"applemusic", L"applemusic" },
    } };

    inline bool ContainsCaseInsensitive(std::wstring_view haystack, std::wstring_view needle)
    {
        if (needle.empty() || needle.size() > haystack.size())
        {
            return false;
        }
        const auto it = std::search(
            haystack.begin(), haystack.end(), needle.begin(), needle.end(),
            [](wchar_t a, wchar_t b) { return std::towlower(a) == std::towlower(b); });
        return it != haystack.end();
    }

    /// True if `aumid` (a GlobalSystemMediaTransportControlsSession::SourceAppUserModelId)
    /// belongs to a whitelisted music-streaming app.
    inline bool IsWhitelistedAumid(std::wstring_view aumid)
    {
        return std::any_of(kMusicAppAllowlist.begin(), kMusicAppAllowlist.end(), [&](MusicApp const& app) {
            return ContainsCaseInsensitive(aumid, app.aumidFragment);
        });
    }

    /// True if `processImageName` (e.g. "Spotify.exe", possibly with a full path prefix) belongs
    /// to a whitelisted music-streaming app.
    inline bool IsWhitelistedProcessName(std::wstring_view processImageName)
    {
        return std::any_of(kMusicAppAllowlist.begin(), kMusicAppAllowlist.end(), [&](MusicApp const& app) {
            return ContainsCaseInsensitive(processImageName, app.processNameFragment);
        });
    }
}
