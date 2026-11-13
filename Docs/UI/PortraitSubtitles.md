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

## Adding a new voice line (subtitles included)

1. Save the line from the Voice Changer app (Save, or Batch) into a subfolder of
   `D:\Blender\BlenderProjects\BlenderTanks\VoiceLines\Announcer\AAAGerMissions`. Next to `<name>.wav` the app also
   writes `<name>_Old.wav` (the dry take) and `<name>.timing.json` (where the dry take sits in the final line). Keep
   both. Whisper hears the dry take far better than the radio-filtered line.
2. Import `<name>.wav` into Unreal from that folder as a **SoundWave**. The table finds the asset by its import
   source file, so it must be that file. Play it with `PlayPortrait` as usual.
3. Run `VoiceLines/VoiceChanger/NewVoiceLines.bat` (or `python new_voice_lines.py`). It:
   - runs `cinematic_transcripts.py update` on the lines that are new or whose audio changed, keeping every other
     entry and every hand edit;
   - rebuilds `DT_CinematicTranscripts` in a DebugGame commandlet. The table is always rebuilt whole, because UE's
     fill-from-JSON empties it first, and it is not saved if the read-back finds a problem;
   - lists rows added or removed, lines that still have no SoundWave, and lines worth a listen.
4. If an editor is open, restart it, or right-click `DT_CinematicTranscripts` > Asset Actions > Reload, before PIE.
   An editor that has the table loaded can keep its `.uasset` locked; the commandlet's save then fails
   ("SAVE FAILED"). Either close the editor and run it again, or run the import inside that editor: in the Output
   Log's Cmd box, `py "D:/UE5Projects/RTS_Survival/VoiceLines/VoiceChanger/ue_import_transcripts.py"`. That fills
   the table in place, so no reload is needed; save the table there afterwards if the editor doesn't.

### Exact words from a script (ElevenLabs text)

If you have the text the line was spoken from, run `VoiceLines/VoiceChanger/LineScripts.bat`:
1. Pick the line.
2. Paste the text and press **Save script + re-time** (Ctrl+S). ElevenLabs tags like `[whispers]` and
   `<break time="1s"/>` are ignored.
3. **Play with subtitle** shows the words lighting up in time with the audio.
4. **Send to Unreal** runs `NewVoiceLines`.

The text is saved as `<name>.script.txt` beside the line. How it is used:
- **The words come from the script; whisper only gives the timing.** `cinematic_transcripts.align_script` lines the
  script up against whisper's words:
  - a matched word takes the heard word's times;
  - a misheard run ("Eurasia's" heard as "your religious") shares the span of what was heard;
  - a skipped word gets the gap between its neighbours.

  In a test with whisper's real mistakes put back in, the skipped "Commandant," landed 10 ms from its true time.
- **Wrong scripts are flagged.** If fewer than 60% of the script's words were heard, the line gets a review flag.
- **A script beats a hand edit in the JSON.** Changing the script alone re-times from cached whisper output in
  under a second.

### Lines in another language: subtitles are always English

The Soviet portraits speak Russian (the Mosin rifleman, made with `mosin_lines.py`; see `VoiceLines/AGENTS.md`). For
these lines the subtitle is the English translation:
- `<name>.script.txt` holds the Russian words. A Cyrillic script makes whisper run with `-l ru`
  (`spoken_language`), and `align_script` times the Russian words as usual.
- `<name>.translation.txt` holds the English text, with the same sentences in the same order.
- `translate_words` spreads each English sentence over the matching Russian sentence's time span, by character
  count. If the sentence counts differ, the whole English text is spread over the line and the entry is flagged.
- The JSON entry gets `"translation": true`, and `"text"`/`"sentences"` are English. A line with a Cyrillic script and
  no translation is flagged ("subtitles must be English").

A line with no `.timing.json` (saved before 2026-11-11, or dropped in by hand) is transcribed from the final file.
That works, but on a test line it misheard words and was 0.25-0.35 s early, so the entry is flagged for review.
Re-saving the line from the app fixes it.

## Where the transcripts come from

All scripts are in `VoiceLines/VoiceChanger/`.

1. `cinematic_transcripts.py all` builds the transcripts for every line. That is slow; `update` does only the new
   and changed lines. Each line is timed in one of four ways (`"timing"` in the JSON):
   - `sidecar`: saved by the app. Whisper runs on `_Old` (the dry take), and each word time becomes
     `t / tempo + shift_s + 0.030 s` from `<name>.timing.json`. Measured against the audio at tempo 1.0 and 0.9:
     +15 to +30 ms, the same chain latency as below.
   - `render`: processed by `replace_announcer_lines.py`, as described below.
   - `own`: an mp3 that was never processed, at shift 0.
   - `final`: a processed wav with neither, at shift 0 and flagged.

   The `render` lines are built like this:
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

   It creates one row per SoundWave, finding each line's assets live by their import source
   (`ue_announcer_assets.find`, shared with `ue_announcer_reimport.py`). It then reads every row back and saves the
   table only if the read-back is clean. The auto-committer then commits the `.uasset`. Don't filter it to one line:
   the table would end up holding only that line.

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
