# Add-on armour break test: Niagara debris vs geometry collection

Test-only comparison (2026-11-04) of two ways to break add-on armour (side skirts) off a tank where a shell hits it.
Nothing here ships: the C++ is compiled only with `RTS_WITH_MAP_TESTS` (non-shipping) or `WITH_EDITOR`, and the test
content in `/Game/RTS_Survival/Tests/AddOnArmorBreak` is referenced by nothing that is cooked.

## What was built

| Piece | Where |
|---|---|
| Skirt meshes: one 90 x 90 x 0.8 cm panel cut into 8 Voronoi shards, plus a rail | `Tools/AddOnArmorBreak/make_skirt_pieces.py` (Blender 5.2, headless) → `ue_import_skirt_pieces.py` |
| `GC_TestSkirtSide`: one skirt side (5 panels x 8 shards), flat root → 40 leaves, every leaf anchored, removal 3 s after break + 0.6 s shrink | console command `RTS.AddOnArmorTest.BuildCollection` (`AddOnArmorBreakAuthoring.cpp`) |
| `NS_TestSkirtDebris`: pooled CPU mesh-particle burst; meshes are user parameters `User.DebrisMesh0..7`, one particle per released piece starting exactly where the piece hung; branch-free HLSL for gravity, drag, spin, ground bounce, lying flat, shrink-out (3.6 s total) | console command `RTS.AddOnArmorTest.BuildNiagara`, HLSL in `Utils/RTSMapTests/AddOnArmorBreak/DebrisHlsl/` |
| `AAddOnArmorTestVehicle`: Panzer IV stand-in (hull, tracks, turret meshes, no gameplay) with skirts for one technique; picks the pieces nearest the shell's line, gives both techniques the same push and spin per hit | `Utils/RTSMapTests/AddOnArmorBreak/` |
| `UAddOnArmorBreakTestSubsystem`: `-AddOnArmorBreakTest=Spike|Capture|Perf` | same folder; runners `Tools/AddOnArmorBreak/run_test.sh`, `run_matrix.sh`, `contact_sheet.py`, `matrix_report.py` |

Techniques: **Niagara** = intact skirts are instanced shard meshes (8 instanced components per tank); a hit hides the
broken instances and fires one pooled burst. **GeometryCollection** = one collection per skirt side (2 per tank); a
hit unanchors and strains the broken leaves, which Chaos then simulates. **None** = no skirts (baseline).

## Findings

### Release mechanics (spike, one tank driving a 12 m circle at 3.5 m/s, 6 hits)

- Niagara: PASS. Every pooled burst carried exactly one particle per released piece (6/6), with the mesh chosen per
  particle from the user parameters.
- Geometry collection: works only as **Dynamic + every leaf anchored**. A Kinematic collection keeps released leaves
  kinematic (engine `UpdateKinematicProperties` only changes clusters), so they never fall. Released leaves take no
  velocity until their break event; the push is applied in `OnChaosBreakEvent` (18/18 break events matched).
- Geometry collection, **lag**: with async physics (project default) anchored pieces trail a moving hull by ~49 cm at
  3.5 m/s; with synchronous physics the lag is 0. The visible hole also appears 0.1-0.2 s after the hit.
- Geometry collection, **tunnelling**: the 0.8 cm plates fell through the landscape: 9/18 by default, 3/18 with CCD
  (`BodyInstance.bUseCCD`), 2/18 with box collision without margin, 1/18 with pieces not colliding with each other.
  Still not zero.

### Pictures

`Saved/AddOnArmorBreak/Capture/compare_<hit>_close.png` (close view, Niagara top, GC bottom),
`compare_HE105_game.png` (RTS camera). From the RTS camera (-70 degrees, 30 m) both effects are small: the skirts are
mostly under the hull outline and the shards are a few pixels each.

### Cost (3 repeats, medians; editor closed, no other Unreal runs, 2026-11-04 10:56)

Offscreen 1080p game view rendered every frame (about 25 tanks in view), vehicles driving circles,
`-benchmark -fps=30`, ms per frame. Matrix file: `Saved/AddOnArmorBreak/matrix_20261104_105614.txt`.

| 100 tanks, 30 hits/s | None | Niagara | Geometry collection |
|---|---|---|---|
| idle (driving, skirts intact): frame / game thread / GPU | 10.1 / 7.0 / 8.4 | 13.7 / 11.3 / 8.8 | 12.1 / 10.5 / 8.6 |
| hitting (~300 live pieces): frame / game thread / GPU | 10.3 / 6.9 / 8.6 | 14.2 / 11.4 / 9.0 | 19.2 / 19.0 / 8.8 |
| settle (debris removed during it): frame / game thread / GPU | 10.2 / 6.7 / 8.6 | 13.8 / 11.2 / 8.8 | 18.8 / 18.9 / 8.6 |

Within each run, hitting minus idle (the debris itself): None +0.5 ms, Niagara +0.6 ms, geometry collection +7.6 ms
frame (+9.3 ms game thread). At 25 tanks and 10 hits/s every technique stayed within about 1 ms of the baseline,
which is the run-to-run noise there.

- GPU: within 0.5 ms of the baseline for both techniques; the frame is bound by the game and render threads.
- Niagara: a flat ~3.6 ms that is the **intact representation** (800 instanced components moving every frame, game
  thread +4.3 ms); the debris (300 pieces, pooled) adds ~0.1-0.6 ms.
- Geometry collection: ~2 ms while intact (200 collections following the hulls), but every collection that has
  broken stays expensive on the game thread for the rest of the run, also after its debris is removed (~0.05 ms per
  damaged skirt side). The cost grows with the number of damaged tanks, not with live debris.

### Limits of these numbers

- Three repeats per cell; the third geometry collection repeat ran ~30% higher than the other two (the median is
  used). An earlier 2-repeat matrix with the editor open gave the same picture.
- The intact-skirt cost compares two representations chosen for the test (8 instanced components per tank for
  Niagara, 2 collections per tank for the GC); only the debris cost compares the techniques themselves.
- Lag and break delay were measured on the stand-in, whose root moves on the game thread. A real tank's hull pose
  already comes back from the physics thread, so the lag relative to the hull is unverified on a real tank.
- The geometry collection spike still FAILS (1/18 pieces through the ground, 62 intact pieces off the moving hull).
- No Shipping configuration was compiled; exclusion from shipping rests on the `RTS_WITH_MAP_TESTS` / `WITH_EDITOR`
  guards that the other map tests use.

## Conclusion so far

Niagara debris is the better base: deterministic, cheap per hit, no lag on moving tanks, no tunnelling (analytic
ground plane), and the mesh-per-armour-kind user parameters work with pooling. Its measured cost is the instanced
intact skirts, which a production version would replace (for example one static mesh per skirt side, swapped to a
per-panel damaged state on a hit). The geometry collection looks as good up close but lags behind moving hulls,
loses thin pieces through the ground and leaves a permanent game-thread cost on every damaged tank.

## Running it

```
# isolated copy when the shared DLLs are locked: PROJECT=D:/UE5Projects/RTS_Survival_AddOnTest/RTS_Survival.uproject
TECHNIQUE=GeometryCollection Tools/AddOnArmorBreak/run_test.sh Spike
Tools/AddOnArmorBreak/run_test.sh Capture && python Tools/AddOnArmorBreak/contact_sheet.py Saved/AddOnArmorBreak/Capture
REPEATS=3 SCALES="25:10 100:30" Tools/AddOnArmorBreak/run_matrix.sh && python Tools/AddOnArmorBreak/matrix_report.py <matrix file>
```
