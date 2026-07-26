# ADR-003 — WaveformEngine ne se fixe pas sur le périphérique de rendu par défaut

## Statut
Acceptée (2026-07-26). **Superseded pour la stratégie de capture elle-même par
[ADR-004](ADR-004-music-app-whitelist-and-process-loopback.md)** : la capture
par périphérique décrite ci-dessous a été entièrement remplacée par une
capture par processus, qui élimine le point 3 ci-dessous. Ce document reste
la référence pour le diagnostic original (le point 2 - session active ≠
données réelles - reste directement pertinent et est réutilisé par ADR-004).

## Contexte
`claude/PLAN.md` (5.1) et `claude/PROMPT.md` décrivent la capture waveform comme
« WASAPI loopback (default render device) ». La première implémentation suivait
cela littéralement : `IMMDeviceEnumerator::GetDefaultAudioEndpoint(eRender,
eConsole, ...)`, puis capture loopback sur ce device.

En testant en conditions réelles (pas seulement en compilant), la waveform
restait plate alors qu'un son jouait réellement. Diagnostic (voir historique
de session, log de diagnostic temporaire dans `WaveformEngine.cpp`) :

1. **Le périphérique par défaut système n'est pas forcément celui que les
   applications utilisent réellement.** Sur une machine avec un logiciel de
   routage audio par app (ici Elgato Wave Link), chaque application peut être
   redirigée individuellement vers un périphérique de sortie différent du
   périphérique par défaut (visible dans Windows : Paramètres → Système → Son
   → Mélangeur de volume → périphérique de sortie par application). Capturer
   uniquement le périphérique par défaut manque alors tout ce qui est
   redirigé ailleurs.
2. **Une session audio "active" ne garantit pas des données réelles en
   loopback.** Le périphérique virtuel "Elgato Virtual Audio" apparaît bien
   comme ayant une session `AudioSessionStateActive` (`IAudioSessionManager2`
   /`IAudioSessionEnumerator`), mais la capture loopback dessus renvoie des
   buffers systématiquement à zéro (`AUDCLNT_BUFFERFLAGS_SILENT` n'est
   jamais positionné — ce n'est donc pas un cas de silence légitime détectable
   via ce flag — mais chaque échantillon lu vaut exactement `0.0f`). C'est un
   comportement du pilote/de la couche de routage virtuelle, pas une erreur
   WASAPI détectable autrement qu'en lisant réellement des échantillons.
3. **Certains routages par app n'exposent aucune donnée capturable du tout,
   sur aucun périphérique énumérable.** Confirmé par l'utilisateur : avec
   Wave Link, chaque application (ex. Spotify) peut avoir son propre canal de
   routage interne à Wave Link, indépendant des autres, qui ne remonte jamais
   comme signal loopback-capturable sur un périphérique WASAPI standard.
   C'est une limite architecturale du logiciel de routage, pas quelque chose
   de détectable ou contournable depuis du code consommateur WASAPI.

## Décision
`WaveformEngine` ne capture plus uniquement le périphérique par défaut. La
sélection de périphérique (`findBestRenderDevice`, revérifiée toutes les 2s) :

1. Essaie le périphérique par défaut ; l'utilise s'il a une session active
   **et** que la capture loopback y produit réellement des échantillons non
   nuls (vérifié via une capture courte de sonde, `deviceHasRealAudioData`,
   ~six tentatives de 20ms).
2. Sinon, énumère tous les périphériques de rendu actifs
   (`EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, ...)`) et utilise le
   premier qui a une session active **et** des données réelles.
3. Sinon, retombe sur le périphérique par défaut malgré tout (pour toujours
   avoir quelque chose à ouvrir plutôt que de bloquer indéfiniment) — cas où
   rien n'est capturable nulle part (limite Wave Link ci-dessus point 3).

## Conséquences
- Fonctionne pour le cas courant (pas de logiciel de routage, ou routage vers
  un vrai périphérique matériel) et pour le cas où une app est routée
  individuellement vers un périphérique virtuel qui, lui, expose de vraies
  données en loopback.
- Ne peut pas capturer un signal qui ne traverse jamais un périphérique de
  rendu WASAPI énumérable — limite connue et documentée, pas un bug latent.
  Un utilisateur avec un routage par-app Wave Link doit s'assurer que
  l'application dont il veut voir la waveform sort vers un périphérique réel
  (ou vers le même bus que le moniteur Wave Link) plutôt que vers un canal
  Wave Link isolé par application.
- Coût : jusqu'à ~120ms de sondage par périphérique candidat testé, uniquement
  lors des réévaluations (toutes les 2s) et seulement pour les périphériques
  ayant déjà passé le filtre "session active" (généralement 0-2 candidats).
