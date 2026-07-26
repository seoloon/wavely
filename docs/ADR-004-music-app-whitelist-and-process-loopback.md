# ADR-004 — Liste blanche d'apps musicales + capture WASAPI par processus

## Statut
Acceptée (2026-07-26)

## Contexte
Deux problèmes distincts, remontés ensemble par l'utilisateur en conditions
réelles :

1. **`MediaSessionManager` réagissait à n'importe quelle session GSMTC.**
   `GetCurrentSession()` retourne la session ayant eu le plus d'activité
   récente, sans notion d'app "musicale" - un onglet de navigateur lisant une
   vidéo l'emporte régulièrement sur une app de streaming musical en cours de
   lecture. Confirmé en conditions réelles : avec Spotify et Brave tous deux
   actifs, `GetCurrentSession()` retournait la session Brave
   (`SourceAppUserModelId = "Brave"`) alors que Spotify jouait activement
   (`SourceAppUserModelId = "SpotifyAB.SpotifyMusic_zpdnekdrzrea0!Spotify"`).

2. **`WaveformEngine` capturait le mix complet d'un périphérique de rendu**
   (voir ADR-003), donc reflétait l'audio de *toutes* les apps sorties sur ce
   périphérique - jeux, notifications, autres onglets - pas seulement l'app
   musicale affichée. De plus, cette approche restait vulnérable à la limite
   documentée en ADR-003 point 3 (certains routages par app, ex. Elgato Wave
   Link, n'exposent jamais de données réelles sur aucun périphérique
   énumérable).

## Décision

### 1. Liste blanche d'apps de streaming musical natif
`Core/MusicAppAllowlist.h` (nouveau, partagé entre `MediaSessionManager` et
`WaveformEngine`) définit une table `{ nom, fragment AUMID, fragment nom de
processus }` pour Spotify, Deezer, TIDAL, Apple Music. YouTube Music est
explicitement exclu (demande utilisateur) : en tant que site web, sa session
GSMTC est rapportée par le navigateur lui-même (`SourceAppUserModelId =
"Brave"`, observé directement), indiscernable de n'importe quel autre onglet
lisant un média - il n'y a pas de signal fiable à ce niveau d'API pour isoler
"YouTube Music" de "une vidéo sur un site quelconque".

Le matching est une sous-chaîne insensible à la casse plutôt qu'un identifiant
exact : les apps packagées (MSIX/Store) exposent un AUMID du type
`Éditeur.NomApp_<hash>!App` dont le suffixe de hash n'est pas garanti stable
d'une installation à l'autre, et les apps Win32 exposent une chaîne libre
auto-enregistrée. Seul Spotify a été vérifié en conditions réelles cette
session ; les entrées Deezer/TIDAL/Apple Music sont best-effort (noms de
processus/éditeurs publiquement connus, non vérifiés).

`MediaSessionManager::selectWhitelistedSession()` remplace
`GetCurrentSession()` : itère `GetSessions()`, filtre par
`IsWhitelistedAumid`, préfère une session `Playing` à une session
Paused/Stopped.

### 2. `WaveformEngine` capture par processus, pas par périphérique
Remplace entièrement l'approche par sélection de périphérique (ADR-003) par
la capture loopback **par processus**, une API Windows 10 2004+/Windows 11
(`ActivateAudioInterfaceAsync` avec
`AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK`,
`audioclientactivationparams.h`) : capture l'audio d'un PID cible (et de son
arbre de processus) directement à la sortie de l'app, avant tout routage
(Elgato Wave Link, VoiceMeeter, ...). Ceci élimine structurellement la
limite ADR-003 point 3 - il n'y a plus de périphérique intermédiaire du tout.

- `findWhitelistedActiveProcessId` : scanne les sessions actives de *tous*
  les périphériques de rendu (le signal "session active" reste fiable même
  pour un périphérique virtuel dont les données loopback sont cassées - c'est
  la conclusion d'ADR-003 point 2), résout le PID de chaque session via
  `IAudioSessionControl2::GetProcessId()`, résout le nom d'exécutable via
  `QueryFullProcessImageNameW`, et retient le premier PID dont le nom
  correspond à la liste blanche.
- Nécessaire car une app comme Spotify tourne comme plusieurs processus
  identiquement nommés (observé : 7 PIDs `Spotify.exe` simultanés - un par
  rôle Chromium renderer/GPU/utility) ; un simple matching par nom de
  processus sans vérifier la session active choisirait un PID arbitraire, pas
  forcément celui qui rend réellement de l'audio.
- `activateProcessLoopbackClient` encapsule
  `ActivateAudioInterfaceAsync`/`IActivateAudioInterfaceCompletionHandler` de
  façon synchrone (attente sur un `Event` avec timeout de sécurité) : le
  thread de capture bloque déjà sur des appels WASAPI classiques partout
  ailleurs dans ce fichier, pas de raison de garder celui-ci asynchrone.
- Un client audio activé en mode process-loopback ne supporte pas
  `GetMixFormat` (pas de périphérique réel à interroger) : le format de
  capture est fourni explicitement (float32/48kHz/stéréo, le format moteur
  WASAPI partagé standard sur Windows moderne) plutôt que négocié.
- Réévaluation toutes les 2s (`kTargetReevaluationInterval`, ex-
  `kDeviceReevaluationInterval`) : si le PID cible n'a plus de session active
  dans la liste blanche (app en pause/fermée, ou une autre app de la liste
  blanche a pris le relais), la session de capture se termine et
  `captureThreadProc` reboucle sur `findWhitelistedActiveProcessId`.
- `Mmdevapi.lib` ajouté aux dépendances de link du projet (nécessaire pour
  `ActivateAudioInterfaceAsync`, absent de l'ensemble par défaut contrairement
  à `ole32.lib` déjà utilisé pour `CoCreateInstance`).

## Conséquences
- La waveform ne reflète plus que l'app de streaming musical native
  effectivement en train de jouer, plus aucune app tierce (jeux,
  notifications, navigateur, ...) ne peut la faire bouger.
- Corrige à la racine le cas Elgato Wave Link/Spotify d'ADR-003 point 3 : la
  capture ne dépend plus du tout de la présence de données réelles sur un
  périphérique loopback-able. `findBestRenderDevice`,
  `deviceHasActiveSession` (au sens "device") et `deviceHasRealAudioData`
  sont supprimées, remplacées par la découverte de PID ci-dessus.
- YouTube Music reste hors périmètre (limite architecturale de GSMTC pour les
  sources navigateur, pas un oubli) - repoussé explicitement par
  l'utilisateur à une itération future si besoin.
- Vérifié en conditions réelles : Spotify en lecture -> waveform bouge et le
  widget affiche le morceau Spotify (pas la session Brave concurrente) ;
  Spotify en pause -> waveform revient au repos ; un son système joué en
  boucle (processus non whitelisté) pendant que Spotify est en pause ->
  waveform reste au repos (preuve que la capture est bien scoping-processus,
  pas un simple mix de périphérique par défaut qui aurait capté ce son).
- Deezer/TIDAL/Apple Music non vérifiés en conditions réelles (apps non
  disponibles pour test cette session) - si un utilisateur rapporte qu'une de
  ces apps n'est pas détectée, étendre `Core/MusicAppAllowlist.h` avec
  l'AUMID/nom de processus réellement observé.
