# Direction finding (Oder-Warthe)

The hunt for the hidden Shield command post in `Ger_OderWarthe`. The rules are in
`Source/Docs/Campaign/OderWartheMissionFlow.md`, sections 4 and 10.

| Piece | Where |
|---|---|
| Rules without a world (wedges, bursts, candidates, fix, polygon clipping) | `Missions/DirectionFinding/DirectionFindingSolver` |
| The placed actor (stations, command post pick, timers, targets, sounds, events) | `Missions/DirectionFinding/DirectionFindingManager` |
| Lookup for the minimap and missions | `Missions/DirectionFinding/DirectionFindingSubsystem` |
| Wedges on the minimap and the tactical map | `GameUI/MiniMap/DirectionFinding/MiniMapDirectionFindingPainter`, called from `UW_MiniMap::NativePaint` |
| Map test | `Missions/DirectionFinding/Tests/DirectionFindingMapTest.cpp`, run with `run_df_test.ps1 [-Pictures] [-DebugGame]` |
| Placement in the map | `ow_map_build.step_direction_finding()` (also in `Saved/OderWarthe/c_build2.py`; alone: `c_df_manager.py`) |

## How it works

- At start (after the difficulty is known) the manager picks the command post among its Werke: random, or
  `ForcedCommandPostIndex`. It spawns the jammer, command porch and rail gate at that Werk, hidden and without
  collision, and starts the jammer noise bed and the routine burst timer.
- A station the player holds for the first time opens a wedge: half-angle 20 / 22 / 25 degrees (Normal / Hard /
  Brutal), centre off the true bearing by up to 35 % of the half-angle. A lost station keeps its wedge (grey) and stops
  narrowing; retaken, it continues from the same width.
- Bursts narrow every held wedge: routine x0.9 (timer 60-90 s, 90-120 s once two stations are held), reply x0.7
  (10-20 s after `NotifyPlayerAttackedSite`, 60 s cooldown per site), attack order x0.5 (`NotifyAttackOrder`). The
  floor is 2 degrees. A heard burst plays the radio kit's Morse and static cues and raises `OnBurstHeard`.
- Fix: three or more held wedges and exactly one standing Werk inside all of them, or all five stations held. The
  targets appear, routine bursts stop, and `OnFix` fires.
- Missions use the Blueprint events `OnStationChanged`, `OnBurstHeard`, `OnFirstOverlap`, `OnFix` and the calls
  `NotifyAttackOrder`, `NotifyPlayerAttackedSite`, `SetWerkEliminated`, `ForceBurst`, `ForceFix`, `StopJammer`,
  `GetCommandPostTargets`.

## Verified

2026-11-24, `run_df_test.ps1 -Pictures` (Development): RESULT PASS, 30 checks, 0 failed. Pictures in
`Saved/DirectionFindingTest/Pictures`. The minimap painter uses the image's paint-space geometry: the cached geometry
is in desktop space and was off by the window position in a windowed game.

## Not done

- The early target reveal when player units reach the command Werk's tunnel mouth (flow doc 4.1).
- The jammer bed starts with the hunt, which is before "Press To Start".
- The decoy radio truck (Hard+) does not send bursts yet.
- The subtitled intercept snippet per burst; bursts only play the Morse and static now.
- What happens to the rail gate if the command Werk's tunnel mouth is collapsed before the fix.
