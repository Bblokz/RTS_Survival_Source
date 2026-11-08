# Add-on armour break test: Niagara debris vs geometry collection vs baked skirts

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
| `SM_TestPzIVHull_Skirted`: the Pz IV hull with both skirt sides (80 shards) and rails merged in, slot "Skirt"; each shard's id in UV channel 2 | console command `RTS.AddOnArmorTest.BuildBakedHull` (`AddOnArmorBakedAuthoring.cpp`) |
| `M_TestSkirtSteelBakedMask` (masked: a broken shard is clipped) and `M_TestSkirtSteelBakedWpo` (opaque: a broken shard's vertices fold onto the object origin); broken shards are a bitmask in Custom Primitive Data 0..3, 20 shards per float | `Tools/AddOnArmorBreak/ue_make_baked_materials.py`, console command `RTS.AddOnArmorTest.BuildBakedWpoMaterial` |
| `UAddOnArmorBreakTestSubsystem`: `-AddOnArmorBreakTest=Spike|Capture|Perf` | same folder; runners `Tools/AddOnArmorBreak/run_test.sh`, `run_matrix.sh`, `contact_sheet.py`, `matrix_report.py` |

Techniques: **Niagara** = intact skirts are instanced shard meshes (8 instanced components per tank); a hit hides the
broken instances and fires one pooled burst. **GeometryCollection** = one collection per skirt side (2 per tank); a
hit unanchors and strains the broken leaves, which Chaos then simulates. **None** = no skirts (baseline).
**BakedMask / BakedWpo** = the skirts are part of the hull mesh; a hit sets the broken shards' bits in the hull's
Custom Primitive Data (one float write, no components) and fires the same pooled Niagara burst. **BakedOpaque** = the
baked hull with the plain opaque skirt material, which cannot hide anything: the reference for shader cost.

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

## Baked skirts (2026-11-04 afternoon)

### Release and pictures

- Spike: BakedMask and BakedWpo PASS (18/18 pieces released, 6/6 bursts with exact particle counts).
- `Saved/AddOnArmorBreak/CaptureBaked/compare_baked_<hit>.png`: Niagara instanced / baked mask / baked WPO remove the
  same shards. Under the skirts' bottom edge the baked variants show small bright triangles; the geometry and normals
  are correct and BakedOpaque shows them too, so it is lighting on the merged mesh (likely its distance field), not
  the hiding material. Not fixed.
- The merged mesh is 21,886 triangles (hull 20,422) in 2 sections. It has only LOD0: the build takes the hull's
  LOD0 description.

### Shader cost

Instruction counts (vertex / pixel shader, base pass):

| Material | VS | PS |
|---|---|---|
| `M_TestSkirtSteel` (opaque reference) | 284 | 278 |
| `M_TestSkirtSteelBakedMask` | 294 | 291 |
| `M_TestSkirtSteelBakedWpo` | 369 | 278 |
| `MI_PZIV_Hull_DesertAPO` (the real hull, for scale) | 554 | 500 |

Mask: +5% pixel work, and the masked section must run a pixel shader in the depth prepass and shadow depths (loses
early-Z there). WPO: +30% vertex work in every pass that draws the skirts, and its shadows cannot be cached.

Measured (`-AddOnArmorBreakProfileGpu`: one ProfileGPU frame halfway through idle and hitting; pass times of the
measured game view's scene capture, median of the clean dumps):

| ms (capture total / BasePass / ShadowDepths) | None | Niagara (instanced) | BakedOpaque | BakedMask | BakedWpo |
|---|---|---|---|---|---|
| RTS view, 100 tanks, idle | 3.08 / 0.20 / 0.62 | 3.71 / 0.20 / 0.61 | 3.08 / 0.20 / 0.65 | 3.10 / 0.21 / 0.65 | 3.21 / 0.21 / 0.67 |
| RTS view, 100 tanks, hitting | 3.16 / 0.21 / 0.64 | 3.49 / 0.21 / 0.64 | 3.13 / 0.21 / 0.65 | 3.25 / 0.22 / 0.66 | 3.23 / 0.22 / 0.67 |
| close view (25 m, -25 deg), 25 tanks, idle | 3.25 / 0.17 / 0.66 | 3.35 / 0.17 / 0.59 | 2.92 / 0.17 / 0.57 | 3.04 / 0.19 / 0.60 | 2.91 / 0.18 / 0.53 |
| close view, 25 tanks, hitting | 3.05 / 0.17 / 0.56 | 3.39 / 0.16 / 0.58 | 3.00 / 0.17 / 0.57 | 2.99 / 0.18 / 0.58 | 2.92 / 0.17 / 0.52 |

Draws in the RTS view base pass: None 33, baked 34, Niagara instanced 252 (idle). Whole-frame GPU time from the
per-frame stats, clean runs: close view BakedOpaque 7.7-8.3 ms, BakedMask 8.0-8.4, BakedWpo 7.4-7.9.

- The mask and WPO cost is **below what this test resolves** (about 0.1-0.2 ms per pass, 0.5 ms per frame), at both
  camera distances: BakedMask's base pass is +0.02 ms over BakedOpaque. This fits the instruction counts: the
  skirts are a small part of each tank's pixels.
- CPU: baked variants match None on the game thread (RTS view, best repeat: None 9.6 ms idle, BakedMask 9.1,
  BakedOpaque 9.9, BakedWpo 10.7), against +5 ms for the 800 instanced intact pieces of the Niagara technique.
- **BakedWpo, close view only**: the game thread is +2.6-3 ms over BakedOpaque/BakedMask in the hitting and settle
  phases (when shards are folded away), in both repeats (10.6/9.9 and 11.1/10.3 ms vs 7.5-8.1/7.1-7.8 ms), not while
  idle and not in the RTS view. **Not reproduced** in the clean matrix below (no ProfileGPU, 3 repeats): BakedWpo's
  game thread matches None there. Most likely outside CPU load or the profiling, not WPO.

### Clean close-view matrix (no ProfileGPU)

Close view, 25 tanks, 10 hits/s, 3 repeats per technique, `PerfCloseClean_*` (`matrix_20261104_170154.txt`). All 15
runs finished; no run or phase was over the GPU outlier rule. Whole-frame GPU time, median of the repeats (ms):

| phase | None | Niagara (instanced) | BakedOpaque | BakedMask | BakedWpo |
|---|---|---|---|---|---|
| idle | 7.77 | 8.25 | 7.73 | 7.95 | 7.89 |
| hitting | 7.92 | 8.29 | 8.04 | 8.09 | 8.01 |
| settle | 7.88 | 8.13 | 7.89 | 7.98 | 7.87 |

Game thread / render thread, best repeat (ms):

| phase | None | Niagara (instanced) | BakedOpaque | BakedMask | BakedWpo |
|---|---|---|---|---|---|
| idle | 7.05 / 12.33 | 8.64 / 12.86 | 7.89 / 13.30 | 7.01 / 12.23 | 7.39 / 13.29 |
| hitting | 6.53 / 12.08 | 8.73 / 13.63 | 7.73 / 14.24 | 7.03 / 12.50 | 7.53 / 13.92 |
| settle | 6.31 / 11.97 | 8.53 / 13.46 | 7.24 / 13.93 | 7.17 / 13.60 | 6.50 / 12.22 |

- GPU: mask +0.05-0.22 ms over BakedOpaque, WPO -0.03-+0.16 ms; Niagara's instanced intact skirts +0.25-0.48 ms
  over None. Shader cost stays below the noise.
- The frame is render-thread bound here (render thread close to the frame time). Runs of the same technique differ by
  up to 3 ms on the game and render thread together (None r1 vs r3), so CPU differences below that between techniques
  are not established. Niagara's +1.5-2 ms game thread is above None in every repeat.
- **Render-thread hitches (100-411 ms) with the baked hull**: at 25 tanks over all three matrices, 10 of 14 baked
  runs have frames of 100-450 ms, mostly in the hitting phase, against 1 of 8 None, 2 of 8 Niagara and 0 of 3 geometry
  collection runs. In the clean matrix: BakedMask r2 411 ms, BakedOpaque r2 329 ms (two runs back to back), BakedWpo r1
  132 ms in idle. The game thread is normal in these frames; the log has nothing at those times. BakedOpaque has them
  too, so it is not the hiding material. A hit only writes one Custom Primitive Data float (a primitive uniform
  buffer update on the render thread). See the trace and quiet-machine runs below: these were outside load.

### Render-thread stalls: trace and quiet-machine check

- Unreal trace (`TraceClose_*`, 4 baked runs, `trace_hitches.py`): one stall caught, BakedMask r2, 313 ms render
  thread in the first second of the hitting phase. Inside it the render thread did the same work as in every other
  frame (Lumen surface cache: 37 card captures per frame, median 0.9 ms, 51 ms in that frame), and leaf scopes that
  take 0.01 ms took 10-14 ms once each (view uniform buffer setup, a buffer creation, the fog of war post process).
  That is the thread losing the CPU in scheduler-sized slices, not extra work.
- Other sessions run headless Unreal processes on this machine, including `UnrealEditor-Win64-DebugGame-Cmd`, which
  the matrix's wait did not catch. `run_matrix.sh` now waits for any `UnrealEditor*-Cmd` and writes a `CONCURRENT`
  line when one is still running after a run ends.
- Quiet-machine matrix (`PerfCloseQuiet_*`, `matrix_20261104_180534.txt`: None, BakedOpaque, BakedMask, 4 repeats,
  editor closed): no frame over 250 ms in any of the 12 runs. Frames of 80-170 ms: BakedMask r2 in the idle phase
  (before any hit, game thread, the whole phase 5 ms slower: load), BakedOpaque r1 92 ms (flagged CONCURRENT: another
  session's run started as it ended). None of them happens on the hit path.
- Means, quiet machine, hitting phase (median of 4): frame None 14.28 / BakedOpaque 15.17 / BakedMask 14.23 ms, GPU
  7.95 / 8.17 / 7.93 ms, game thread 7.33 / 8.07 / 7.52 ms. BakedMask equals None.

### Measurement problems in this matrix

- **Outside load**: whole runs ran 2-2.5x slower on the GPU and/or game thread (for example close view BakedMask r1 at
  19.9 ms GPU for the whole run, r2 at 8.0 ms). Runs and dumps are left out by objective rules, both listed by the
  report scripts: a ProfileGPU dump when a pass the skirt material cannot affect (PostProcessing, Nanite::VisBuffer,
  RenderDeferredLighting) is over 1.5x its median; a run's phase when its median GPU time is over 1.5x the view
  median. Outside CPU load has no such control, so CPU figures above are from the best repeat. In the close view
  None's game thread is 1-1.5 ms above BakedOpaque in both repeats, so None is not a clean zero there; BakedOpaque
  is the reference for shader cost.
- ProfileGPU stalls its frame and the next one for 0.5-1.4 s; `frames_report.py` drops frames over 250 ms and
  counts them. The averages in `matrix_report.py` include those frames: do not use them for profiled runs.
- **One GPU crash**: BakedWpo RTS view r2 ended in `DXGI_ERROR_DEVICE_REMOVED` (1 of 25 runs). The active GPU
  breadcrumb was BuildHZB in the main viewport, not a pass that draws the skirts; Windows logged no driver reset.
  Cause not established. The slow-GPU periods started about six minutes later.
- Matrix files: `Saved/AddOnArmorBreak/matrix_20261104_132208.txt` (RTS view, 3 repeats),
  `matrix_20261104_135303.txt` (close view, 2 repeats).

## Conclusion so far

Niagara debris is the better base: deterministic, cheap per hit, no lag on moving tanks, no tunnelling (analytic
ground plane), and the mesh-per-armour-kind user parameters work with pooling. Its measured cost is the instanced
intact skirts, which a production version would replace (for example one static mesh per skirt side, swapped to a
per-panel damaged state on a hit). The geometry collection looks as good up close but lags behind moving hulls,
loses thin pieces through the ground and leaves a permanent game-thread cost on every damaged tank.

Baked skirts with a masked material remove the intact-skirt cost: same CPU as no skirts, GPU cost below what this
test resolves, one Custom Primitive Data write per hit. Mask is preferred over WPO: same pictures, no WPO vertex
cost. The render-thread stalls seen in the shared-machine matrices did not recur on a quiet machine (see above).
Open points: the lighting artefact under the skirt edge and LODs for the merged mesh.

## Running it

```
# isolated copy when the shared DLLs are locked: PROJECT=D:/UE5Projects/RTS_Survival_AddOnTest/RTS_Survival.uproject
TECHNIQUE=GeometryCollection Tools/AddOnArmorBreak/run_test.sh Spike
Tools/AddOnArmorBreak/run_test.sh Capture && python Tools/AddOnArmorBreak/contact_sheet.py Saved/AddOnArmorBreak/Capture
REPEATS=3 SCALES="25:10 100:30" Tools/AddOnArmorBreak/run_matrix.sh && python Tools/AddOnArmorBreak/matrix_report.py <matrix file>
# baked shader cost: GPU pass dumps, close view; then per-pass and per-frame reports
TECHNIQUE_LIST="None Niagara BakedOpaque BakedMask BakedWpo" REPEATS=2 SCALES="25:10" VIEW_TAG=Close \
  Tools/AddOnArmorBreak/run_matrix.sh -AddOnArmorBreakProfileGpu -AddOnArmorBreakViewDistance=2500 -AddOnArmorBreakViewPitch=-25
python Tools/AddOnArmorBreak/profilegpu_report.py --view Close && python Tools/AddOnArmorBreak/frames_report.py PerfClose_
# clean close view, no profiling (every dropped frame is a real hitch)
TECHNIQUE_LIST="None Niagara BakedOpaque BakedMask BakedWpo" REPEATS=3 SCALES="25:10" VIEW_TAG=CloseClean \
  Tools/AddOnArmorBreak/run_matrix.sh -AddOnArmorBreakViewDistance=2500 -AddOnArmorBreakViewPitch=-25
python Tools/AddOnArmorBreak/frames_report.py PerfCloseClean_ --no-profile
# what holds the long frames of a traced run (TRACE=1 run_test.sh ...); exports all events, takes ~2 min per trace
python Tools/AddOnArmorBreak/trace_hitches.py Saved/AddOnArmorBreak/<tag>.utrace
```
