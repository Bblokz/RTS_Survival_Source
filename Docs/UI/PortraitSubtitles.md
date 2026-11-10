# Cinematic portrait subtitles

Under the cinematic portrait (`W_BP_Portrait`) a small box shows the speaker's name and what they say. Words
appear in step with the voice line, and a finished sentence is wiped before the next one starts, so the box stays
narrow (fixed width, at most about two lines).

## How it works

- `UPlayerPortraitManager::StartNextPortraitFromQueue` plays the line, then calls `UW_Portrait::StartSubtitles`.
- `UW_Portrait` creates a `UW_PortraitSubtitle` and adds it to its root canvas, centred under the canvas child
  that holds `PortraitImage`. Because it's a child of the portrait, it fades and hides with it.
- The transcript is looked up in `DT_CinematicTranscripts` (`/Game/RTS_Survival/Sounds/VoiceLines`, rows are
  `FCinematicTranscriptRow`) by the voice line's asset path. A line without a row shows only the name.
- The speaker name comes from `UW_Portrait::SpeakerNames` (per `ERTSPortraitTypes`, for example `GermanAnnouncer`
  = "Advisor"), unless the row sets `SpeakerOverride`.
- Timing follows the audio renderer's playback position:
  `UPlayerAudioController::GetVoiceLinePlaybackSeconds`, fed by `OnAudioPlaybackPercentNative`. Queued lines,
  frame hitches and pauses therefore can't put the words out of step. World audio time was tried first and ran
  ahead after a 6 s hitch.
- Only sound waves get timed words; a sound cue as the voice line shows just the name.
- Style (panel brush, fonts, colours, box width, reserved lines, gap, sentence hold time) is
  `UW_Portrait::SubtitleStyle`. The C++ defaults are Roboto Regular for the spoken words (`RR_mainFont` is an all-caps display face, hard to
  read in sentences), `RR_mainFont` for the speaker name, and a dark panel with a thin teal outline;
  override them on `W_BP_Portrait`.

## Cinematic overlay (subtitle strip + countdown)

Every mission cinematic takeover (`UMissionBase::OnCinematicTakeOverWithSkipping`, then
`UMissionCinematicTakeOverSession::StartSession`) shows `UW_CinematicOverlay`:
- **Subtitle strip:** one strip at the top centre, the speaker and then the words on the same line, revealed and
  wiped like the portrait box.
- **Countdown:** the time left (m:ss) at the bottom right, which is clear of the hold-to-skip prompt at bottom
  centre.

The overlay closes itself when the sequence finishes or stops, and the session also closes it on cancel or skip.

**No per-cinematic setup.** `CinematicSubtitleTimeline::BuildCues` reads the sequence's own audio tracks: master
tracks, tracks bound to objects, sub-sequences and shots.
- Every active section whose sound has a transcript row becomes a cue.
- Each word plays at `section start + word time - front trim` on the sequence clock.
- Words in trimmed-off parts are dropped, and muted tracks and inactive sections are skipped.

So: drag the voice line in, move it, trim it, and the subtitles follow. Sub-sequence time scale is not applied
(none of the cinematics use it).
- **Clock:** the sequence player's current time, so pause, play rate and skip all follow. The countdown is
  `(end time - current time) / play rate`, rounded up.
- **Overlapping voice lines:** the one that started last owns the strip.
- **Speaker:** the row's `SpeakerOverride`, otherwise the first `SpeakerByAssetPath` key that the sound's path
  contains (default `/Portraits/RusHazmat/` = "Hazmat"), otherwise `DefaultSpeaker` ("Announcer").
- **Restyling:** make a Blueprint subclass of `UW_CinematicOverlay` (style, speaker rules, `bShowCountdown`) and set
  it as `M_CinematicOverlayWidgetClass` on the player controller Blueprint. Null = the C++ default look.
- **Other cinematics:** `UW_CinematicOverlay::ShowForSequence(Player, SequencePlayer)` is Blueprint-callable for
  sequences played outside a takeover.

The portrait box and the overlay share `TranscriptReveal::BuildVisibleText` (the reveal and wipe),
`UCinematicTranscriptSubsystem` (the table lookup) and `SubtitleStyleDefaults` (the font and panel).

## Where the transcripts come from

All scripts are in `VoiceLines/VoiceChanger/`.

1. `cinematic_transcripts.py all` builds the transcripts:
   - It runs whisper.cpp (`D:\Tools\whisper`, model large-v3-turbo, `-nfa`) on the unprocessed `_Old` backups of
     every line under `VoiceLines/Announcer/AAAGerMissions`. The mp3 lines are their own source.
   - It shifts each word time onto the processed in-game file: `t + prefix_s - trim_start_s + 0.030 s`. The
     30 ms is measured filter latency, and the offsets are verified by re-rendering each line.
   - It writes `VoiceLines/Announcer/AAAGerMissions/_CinematicTranscripts.json`: sentences of at most 12 words,
     split near the middle at a comma, and "review" flags for suspicious lines.
2. Correct misheard words in that JSON and set `"edited": true` on the line, so later builds keep it. Names
   whisper gets wrong every time go in `CORRECTIONS` in the script.
3. Import into Unreal with `ue_import_transcripts.py`, in a commandlet whose RTS_Survival DLL has the row struct:

   ```
   UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
       -script="D:/UE5Projects/RTS_Survival/VoiceLines/VoiceChanger/ue_import_transcripts.py"
   ```

   It creates one row per SoundWave, using the asset-to-source map from `ue_announcer_reimport.py`, reads every
   row back and saves the table. The auto-committer then commits the `.uasset`.

Measured accuracy: whisper's token times hit speech onsets to about 0.1 s. DTW timestamps were about 0.7 s late,
and flash attention made the words after a pause bunch together.

## Tests

`Source/Tools/PortraitSubtitles/portrait_subtitle_test.py` runs in a throwaway DebugGame editor on
`RTS_ToCopySmall`. It presses start, plays `FirstMission_ClearRoad_1` through the portrait manager and takes UI
screenshots on the wall clock (`Saved/Screenshots/WindowsEditor`). Compare each one with the transcript at its
real time: a screenshot can stall the game for seconds, but the audio and the subtitles carry on through it.

`Source/Tools/PortraitSubtitles/cinematic_overlay_test.py` does the same with `LS_FirstMission_FuelDepot`: it plays
the sequence with `ShowForSequence` and logs the sequence time of every screenshot. `shot showui` sometimes captures
the bare 3D view without any UI; that's a capture quirk, not the overlay.
