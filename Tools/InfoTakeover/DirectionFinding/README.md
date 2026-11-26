# Direction Finding field manual: clip and assets

The Oder-Warthe field manual on radio direction finding (`UW_DirectionFindingTakeover`, kind `DirectionFinding`). It
opens when DF 1 is taken (`Mission_OW_ListeningPost`). The clip is filmed from the real tactical map of
`Ger_OderWarthe`.

## Rebuild the clip

1. Check `nvidia-smi`, then film the clip. This runs about 2 minutes and saves 240 frames at a fixed 24 fps to
   `Saved/DirectionFindingClip/Frames`:

   ```
   powershell -File Source/Tools/InfoTakeover/DirectionFinding/capture_clip.ps1
   ```

   It runs `-DirectionFindingClip` (`Missions/DirectionFinding/Tests/DirectionFindingClipCapture.cpp`) with
   `-DirectionFindingCommandPost=1` (Werk 2, north of both stations) and does the following:
   1. presses Start;
   2. skips the arrival tour;
   3. opens the tactical map;
   4. takes DF 1 at frame 24;
   5. sends an attack order at frame 72;
   6. takes DF 2 at frame 132;
   7. sends another attack order at frame 180.

   The `DF_CLIP` log lines give these frames.
2. Pack the atlas:

   ```
   python Source/Tools/InfoTakeover/DirectionFinding/make_clip_atlas.py
   ```

   - It crops the map square (`CROP`, 1920 x 1080 pixels) and centres it in the 480 x 270 cell.
   - It keeps every second frame from 12: 96 frames at 12 fps, 8 x 12 cells, 3840 x 3240.
   - It tags each part ("DF 1 HELD: A BEARING", "ATTACK ORDER: NARROWER", "DF 2 HELD: CROSS BEARING").
   - It also writes `clip_preview.gif`.
3. Import the texture and (re)make the widget headless:

   ```
   UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
       -script=Source/Tools/InfoTakeover/DirectionFinding/create_direction_finding_takeover.py
       -unattended -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate
   ```

   - It imports `T_DirectionFinding_ClipAtlas` as a UI texture with no mips that is never streamed.
   - It makes `WBP_DirectionFindingTakeover` as a copy of `WBP_DigInTakeover` (Rotsignal skin), reparented, with the
     grid from `clip_atlas.json`. Both go to `GameUI/InfoTakeover/DirectionFinding`.

If the grid changes, also change `ClipColumns / ClipRows / ClipFrameCount` in `W_DirectionFindingTakeover.cpp`.

## Texts

The texts are one page (owner pick, 2026-11-25):
- **Kind label:** ODER-WARTHE - RADIO DIRECTION FINDING.
- **Facts:**
  - LISTEN ("Hold a listening post");
  - NARROW ("When they attack, they talk");
  - CROSS ("Cross the bearings").

There are no tuning numbers in the text. The archive entry is under Game Mechanics (`InfoTakeover.DirectionFinding`).
