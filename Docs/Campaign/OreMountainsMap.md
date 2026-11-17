# Ger_2ndOreMountains: the map (first pass, no mission scripting)

`/Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains`, started 2026-11-13 from the v5 layout sketch
(`MapDesigner/OreMountains/ore_01.layout.json`, see [OreMountainsMission.md](OreMountainsMission.md)). It is a copy of
`Maps/ToCopy/RTS_ToCopyMap` with a new landscape. Status: **large sections only** (terrain, paint, lighting, resources,
stand-in trees, placeholder blocks). No units, enemy buildings, triggers or objectives.

## Size and axes

- Play area **570 x 570 m**, centred on the origin. +X is north. The ridge and the Czech border are on the south edge.
  The owner found the sketch's 850 m too large on 2026-11-13 ("reduce it by 33 %"), so the generator scales the whole
  sketch by 0.67 about the centre (`SCALE` in `ore_terrain_gen.py`; it writes the scaled spec to
  `Saved/OreMountains/terrain/ore_01_impl.layout.json`). The sketch file itself is still the 850 m one.
- Heights, the 10 m road, tracks, gates and the viaduct deck keep their real sizes; ramps keep at least 12 m of width.
  Every ramp is therefore 1.5 times steeper than drawn: main 12 %, west 14 %, east 16 %, knoll west 19 %, prow sally 34 %.
- Landscape 12 x 12 components of 63 quads at 100 cm (756 m square, centred): 93 m of camera margin a side.
- Heights: valley road 0 m, foothills 4-10 m, battery pad 9.5 m, Base B knoll 16 m, ridge bench 18 m, crest 35-38 m
  inside the play area and up to 97 m in the camera margin south of it.
- Material `MI_RTS_Landscape_DryCastle_OreMountains`, a child of `MI_RTS_Landscape_DryCastle_Inst` with
  `LS_RadixiteBlight` on. The shared master and its other instances are untouched.

## Rebuilding

Scripts are in `Source/Tools/OreMountainsMap`.

```bash
python Source/Tools/OreMountainsMap/ore_terrain_gen.py   # heights, 8 layer weights, layout, masks, check.txt
python Source/Tools/OreMountainsMap/ore_dress_gen.py     # resources, stand-in trees, placeholders -> Ore_dress.json
```

Both write to `Saved/OreMountains/terrain`. Then, in a throwaway full editor started with
`-ExecCmds="py .../ore_editor_server.py"` (commands are sent with `ore_cmd.py <file>`; its command folder is
`Saved/OreMountains/cmd`, separate from the Berlin one):

```python
import ore_map_build as omb
omb.load_layout()
omb.step_duplicate(); omb.step_strip()            # only for a new map file
omb.step_landscape(); omb.step_fit_level_actors(); omb.step_lighting()
omb.step_resources(); omb.step_trees(); omb.step_placeholders()
omb.save_level()                                  # after the navmesh has finished building
import ore_nav_check; ore_nav_check.run()
```

`step_landscape` destroys and recreates the landscape, so run `step_fit_level_actors` after it (the RVT volumes align
to the landscape). Each dressing step clears its own actors first (`ORE_Res_`, `ORE_Trees_`, `ORE_PH_`, `ORE_Note_`).

## How the terrain is made

- Heights come from the sketch's `elevation` block (contours, pads, graded ramps), not from region `z`.
- All four nav agents have `agent_max_slope` 44 degrees, so MapDesigner's per-class grades do not exist in the game.
  The escarpment, the knoll sides and the crest face are sharpened into rock faces of about 60 degrees; the six graded
  ramps are left as drawn (4-13 degrees along the centre line).
- `check.txt` holds a flood-fill check on the height field; `ore_nav_check.run()` repeats it on the built navmesh
  (2026-11-13: 0 problems; every ramp passes for all four agents, and no path crosses a rock face).
- How the DryCastle layers look here: Desert = khaki with orange flecks (valley), MountainTop = pale grey-tan (bench,
  high ground, tracks), RTSSand = dark earth (peat bogs, gullies, under dead woods), GraySand = orange dust (thin
  drifts), RockDesert = faces and spoil heaps, Asphalt = the valley road and yard patches, GrassCastle = scrub.

## What differs from the sketch

- **Prow sally ramp:** the sketch closes it to heavy vehicles by its 23 % grade. In the game heavy tanks can use it
  (same slope limit for every agent). Closing it needs a `BP_NavBlocker_*` volume; not placed.
- **Skybox mesh:** the template's sunset sky dome was removed with the test actors; the map has a SkyAtmosphere and
  daylight lighting instead (`step_lighting`).

## Placeholders

Grey blocks labelled `ORE_PH_*` with a red text above them mark landmarks that have no model in the project yet: the
three mine adit portals, the Wismut headframe and hoist house (Base B), the Wismut ore plant, conveyor and sheds
(Base A), the weighbridge, booth and boom barrier, the four A4 viaduct pieces, the Forge survey post items, the five
fossil-root spots and the two smouldering-peat spots. Yellow `ORE_Note_*` texts mark sites that use existing content
later (player buildings, the Soviet bases, the battery's eight pits). Blocks have no collision and no nav effect.
The dead trees are stand-ins too (`SM_RTS_DeadTree_*`, instanced, no collision).

## Objects the map needs (2026-11-13)

Positions are metres from the map centre (X north, Y east).

**Not in the project (new models):**

| Object | Where | Size and seating |
|---|---|---|
| Mine adit portal (3, one "main") | X -240 to -244, Y 40 / 84 / 127 | opening about 12 m wide, 8 m high; buried in a rock face of about 60 degrees that rises from the 18 m bench to 35 m; flat apron in front |
| Wismut headframe + hoist house | Base B knoll top, X 127, Y -60 | flat top about 60 m across at 16 m; headframe about 12 x 12 x 25 m, hoist house about 10 x 16 m |
| Wismut ore plant, conveyor + loading bunker, sheds | Base A yard, X 10, Y 238 | yard 57 x 67 m, flat at 0 m, the convoy road runs through its south part |
| Weighbridge deck + guard booth | X 1, Y 81 (deck in the roadway), booth X -6, Y 75 | deck about 4 x 18 m flush with the 10 m road; booth about 4 x 5 m |
| Forge survey post props | X -56, Y 117 | sampling stakes with flags (4 on the Radixite field), cryo canister rack about 1.5 x 3 m; Director of Forge style as in Berlin |
| Dead spruce kit | 9 woods, about 440 trees | standing snags and broken trunks 8-14 m, crushable like the Wpo trees, instanced; fallen spruce logs |
| Fossil roots | 5 gully cuts | blackened root mass about 3 x 6 m lying in a 1.3 m deep gully |
| Border markers | south edge, X -271 | posts or boundary stones; dressing only |

**Close fits exist, owner decides whether they do:** broken A4 viaduct (132 m line from X -70, Y -56 to X 59, Y -30, deck
12 m wide, 10 m road gap at X -5, Y -43; option A picked, see the detail pass; `BP_Dest_PostApo_RoadOnPillars*`), boom barrier (`SM_GateBarrier_1`,
`SM_Boom_Barrier_1`), generator and tent (`BP_Generator_A/B`, Rus tents), smouldering peat (`NS_BerlinAtmo_Smoulder_*`),
adit stand-in (`BP_Tunnel_01`, `SM_Tunnel_End_01`), yard dressing (QuarryEnv, PostApo silos and cranes), rock faces
(`SM_RTS_RockMoss_*`, Brushify cliffs; no granite set), watchtowers (`Rus_BXP_GuardTowerMG_Soviet`).

**Not objects:** a peat or forest-floor landscape layer (the bogs use the dark RTSSand layer for now).

## Checked in a play session (2026-11-13)

The map starts: the camera pawn sits over the start on the bench (camera 40 m above it; repeated on the 570 m build), the default
start units spawn, fog of war works and the ground reads pale grey-tan at normal brightness. Editor scene captures of
this map come out dark; the game view does not.

## Known gaps

- At 570 m the sketch's weapon ranges no longer fit: the battery's 380 m "normal" range now reaches the bench (the
  battery is about 360 m from the HQ). Ranges and the Base B overwatch circle need retuning with the mission design.
- The bench is about 160 m deep and 570 m wide; the knoll top is about 60 m across (tight for a headframe plus AT guns).

- The viaduct blocks and the checkpoint have no collision and no nav effect: the 10 m choke does not exist yet.
- The stand-in trees neither slow nor block.
- The placeholder blocks are visible in game (only their labels are editor-only).
- The sun comes from the north-north-east so the escarpment is lit; that is not where the real sun stands.

## Detail pass (2026-11-13)

- **Terrain:** the straight creases the sketch's contour polygons left in the foothills are rounded off; the east and
  west camera margins got their own noise.
- **Roads (`step_roads`, labels `ORE_Road_*`):** the valley road is `BP_GroundPathSpline_Road` with
  `MI_GroundPath_Road_WarTorn`, 9 m wide, running out through both margins (368 tiles). The ammo track and the Base B
  depot track are `BP_GroundPathSpline_TwoTrack`. The landscape under the road only carries a faint dark stain.
- **Checkpoint (`step_checkpoint`, labels `ORE_CP_*`):** concrete fence panels either side of the 12 m gate, barbed
  wire and hedgehogs carrying the line on, two sandbag walls, a stop sign and a raised boom barrier (posts and bars
  have no collision). The booth, the weighbridge deck and the two towers are still placeholders.
- **Rock faces (`step_rocks`, labels `ORE_Rocks_*`):** 401 `SM_RTS_RockMoss_*` chunks along the lip and foot of the
  faces. **The owner rejected these on sight ("these look ridiculous").** They were removed from the map the same day; do not run
  `step_rocks` again; a proper cliff kit followed
  (`CustomMapItems/Environment/OreCliffKit`, sheets in `Saved/OreMountains/cliffs`). The owner picked options
  **C (blocky crag) and E (quarried benches)** on 2026-11-13; read as C on the escarpment and knoll, E on the crest
  face with the adits. The kit is being modelled and imported under
  `/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit`; placement data comes as `Ore_cliffs.json`.
- **Viaduct:** concept sheets in `CustomMapItems/Environment/A4Viaduct` (copies in `Saved/OreMountains/viaduct`).
  The owner picked **option A (stone arch viaduct)** on 2026-11-13 and wants it to look clearly 1940s and heavily
  damaged; a refinement round (v02) gave two variants and he picked **A1 "broken teeth"** (arches caved in, piers
  standing); it is being modelled and imported under `/Game/RTS_Survival/Blueprints/Environment/OreMountains/A4Viaduct`
  with a placement file `Saved/OreMountains/viaduct/A4Viaduct_pieces.json`. The four placeholders now sit where the road really
  crosses the viaduct line: 10 m gap centred on X -5, Y -43, south end X -70, Y -56, north end X 59, Y -30
  (parts south to north 22.5 | 17 | 22.5 | 10 | 24 | 17 | 19 m). The sketch's own deck regions left a 6.9 m gap
  7 m north of the road after scaling.
- Play session at 570 m: the map starts, start units spawn, fog of war works. An unfocused throwaway editor runs the
  play session at about 2 fps and screenshots come out black until `t.IdleWhenNotForeground 0` is set.

## Detail pass 2 (2026-11-13, `ore_detail_gen.py` + `step_props`)

`ore_detail_gen.py` writes `Ore_detail.json`; `step_props` places it (labels `ORE_Prop_*`, `ORE_Scatter_*`) and
removes the placeholders it replaces. Everything is existing project content.

- **Base A yard:** `BP_Industrial_Dest_Hangar` (in place of the sheds placeholder), chimney, water tower, a Rus fuel
  store, crate and barrel stacks, two containers, and 37 concrete fence panels round the perimeter with stretches
  missing and openings where the road and the ammo track pass. The ore plant and conveyor are still placeholders.
- **Battery:** Rus supply storage and crate stacks at the rear, a camo net, three sandbag arcs on the rim that faces
  the valley. The eight pits are still notes.
- **Survey post:** a (holed) Rus small tent and crates; the cryo rack, generator and stakes are still placeholders
  (`BP_Generator_A` is a 16 x 21 m building, not a field generator).
- **Smouldering peat:** six `NS_BerlinAtmo_Smoulder_Wisp` effects at the two sketch spots.
- **Valley road:** crushable power poles on the north verge about every 42 m (none at the checkpoint, the viaduct
  and the yard), four 1945 tank wrecks and four old cars on the verges near the viaduct and the checkpoint.
- **Bench:** a narrow mine railway (track panels at 0.55 scale, no collision) from the west adit to the metal by
  the bench edge, with three mine carts.
- **Dead woods:** 26 fallen logs (`BP_Cover_DeadTreeLog_*`, natural cover), instanced stumps, dead shrubs and litter.
- **Ground detail (instanced, no collision):** scree at the feet of the faces, stones in the gully beds and on the
  spoil heaps (`SM_GS_*`), dead shrubs and dry tufts in the bogs and scrub pockets.
- **Camera margin:** about 620 dead trees and 680 dead shrubs on the ground past the play edge (north, east, west).
- **Fixes:** the valley road spline uses every third sketch point (the full set kinked the tiles) and the viaduct
  rubble swell keeps 10 m off the road (it lifted the road bed through the tiles).
- **Base B and the adits (added later the same day):** sandbag arcs at the three gun positions on the knoll rim, a
  camo net and stores by the shaft; crate and barrel stacks at two adit mouths. `BP_Cover_RusSandbags_LongArc`
  bulges about 80 degrees clockwise of its yaw (`SANDBAG_FACE`).
- Nav check after the pass: 0 problems, including a new leg along the road through the yard fence.
- Play session after the pass: starts, start units and fog of war present, no errors from the new actors.
  A play session in an unfocused throwaway editor stays paused at time 0 (black screenshots): call
  `GameplayStatics.set_game_paused(world, False)` and wait on game time, as in `Saved/OreMountains/cmd_pie_debug.py`.
- Editor scene captures of this map have no sky light (pitch-black shadows, orange ground); the game view is fine.
  Use them for layout, not for judging colour.

## Cliff kit in the map (2026-11-13, `step_cliffs`)

- Assets: `/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit` (23 meshes `SM_OreCliff_*`, one material
  `M_OreCliff` / `MI_OreCliff`, world-aligned gneiss). Source, brief and placement guide:
  `CustomMapItems/Environment/OreCliffKit` in the Blender project.
- Layout: `Saved/OreMountains/terrain/Ore_cliffs.json`, written by the kit's own `ock_place.py`
  (`escarpment=C knoll=C crest=E`): 705 instances (escarpment 224, knoll 80, crest 401). `step_cliffs` places them as
  29 instanced components, labels `ORE_Cliff_<face>_<piece>`, no collision and no nav effect (the landscape blocks).
- If the terrain generator changes a face, rerun `ock_place.py` before `step_cliffs`.
- `MI_OreCliff` was retuned after the first in-game look (the rock read near white and bluish): `RockTint`
  (0.74, 0.67, 0.58), `DustAmount` 0.75, `GroundFromRVT` 0.45.
- Checked in game with the fog of war off (`ore_ingame_shots.py`): faces read as continuous rock, the crest reads as
  cut benches round the adit openings. Nav check 0 problems after placing.
- Open: the three adit openings still hold placeholder blocks; lip pieces stand up to 2.3 m above the bench within
  about 4 m of the edge (units there can clip); outcrop pieces `Crag9` / `Crag5` are not placed.
- **In-game screenshots anywhere on the map:** `ore_ingame_shots.py` starts a play session, unpauses it, switches the
  fog of war off with `ShowFlag.PostProcessMaterial 0`, moves the camera pawn to each spot and writes a HighResShot.

## Viaduct in the map (2026-11-13, `step_viaduct`)

- Assets: `/Game/RTS_Survival/Blueprints/Environment/OreMountains/A4Viaduct` (nine static meshes, 38.6k triangles,
  instances `MI_A4_Granite`, `MI_A4_Rubble`, `MI_A4_Concrete`, `MI_A4_RebarRust`; the instances hang off the
  Brandenburg Gate masters `M_BG_Sandstone_Procedural` and `M_BG_BronzePatina`, so a change there changes the viaduct).
  Source and README: `CustomMapItems/Environment/A4Viaduct/Model` in the Blender project.
- Placement: `Saved/OreMountains/viaduct/A4Viaduct_pieces.json`; `step_viaduct` places 17 actors (labels
  `ORE_Viaduct_*`: four standing pieces with their UCX hulls, two fallen spans and eleven rubble fans without collision
  or nav effect) and removes the four viaduct placeholders. One yaw for every piece: 11.43 degrees.
- Checked on the built navmesh: the road passes the 10 m gap, both fallen spans are crossed in a straight line by
  infantry, light and heavy tanks, and a path across a standing stub goes round its end.
- Checked in game with the fog of war off: reads as a heavily damaged stone-arch viaduct, the road clear in the gap.
- Open: stone coursing was only judged from above; LODs and shot collision are untested; the viaduct has no
  Blueprint and is not destructible; the wreck-seal idea for the gap is mission scripting.

## Ambient sand and dust (2026-11-13, `ore_fx_gen.py` + `step_fx`)

The owner asked to go through the Fab pack `Content/DustAndSandStorm` (87 Niagara systems), improve what is usable
and add some to this map.

- **Survey.** All 72 looping systems were shot in game on this map from the RTS camera (`ore_fx_survey.py`; sheets in
  `Saved/OreMountains/fx/sheet_*.jpg`). Out of the box almost none reads from 40 m up: they are sized for a walking
  camera and coloured dark grey-brown. The pack's systems expose user parameters (colours, size, spawn rates), so the
  improvement is done as **tuned presets**: a pack system plus parameter overrides and a scale, stored on each placed
  component. The pack assets themselves are unchanged.
- **Presets** (in `ore_fx_gen.py`, tested with `ore_fx_presets.py`):
  - `SandStreak` = `NS_SandPush`: wind-driven sand hugging the ground. Longer (18 m), recoloured to the map's pale
    dust, brightened (`AddWhiteColor` 0.38, `Emessive` 0.42; below about 0.4 the sand sprites stay a dark smudge) and
    cut from 4000 to 1500 GPU particles a second. Yaw follows the project wind (35 degrees) with a little scatter.
  - `DustCloud` = `NS_StormSmoke_02`: a wide soft haze, 2.5 times the size, dust-coloured.
- **On the map** (`step_fx`, labels `ORE_Fx_*`): 11 sand streaks (five on the valley floor, three on the bench,
  three on the foothills) and 4 dust clouds (both valley mouths, the foothill slope, the valley centre).
- **Dropped:** the sand falls (`NS_SandFall01/02/04/05`) on the rock lips and `NS_FallingDust_01` under the viaduct
  stubs. Scaled and recoloured they still do not show from the RTS camera at any time after spawning, so they would
  only cost simulation time. The big set pieces (`NS_Tornado`, `NS_SandStorm_B`) fill the screen and were not used.
- **Checks:** the overrides survive a save and a fresh load (reload shot `fx/reload_streak_bench.png`). Frame time with
  the 15 effects on against off, camera on a streak: 11.65 ms against 11.20 ms (unfocused editor play session, about
  500 frames each: a rough figure, not a profile).
- **Open:** the placed effects ignore the player's Ambient Effects setting (`rts.AmbientFx`); from above the streaks
  read slightly cool grey on the warm bench ground; they are subtle and static in place.
- **Second round, same day (owner: "Yes, stronger").** `SandStreak` is denser and larger (`Length` 2400, `SandScale`
  1.8, `ParticleSmokeSizeScale` 2.6, spawn rates 110 / 35 / 35, 2000 GPU particles a second) and warmer in colour;
  `AddWhiteColor` stays 0.38 because 0.2-0.3 with more `Emessive` came out whiter and cooler. `DustCloud` is larger
  and brighter (`Smoke_Size_Scale` 3.2, spawn 1.3, `Emessive_Scale` 1.6, alpha 0.6 / 0.7). The map now holds **21
  streaks and 6 clouds**. In-game sheet: `Saved/OreMountains/fx/sheet_final2.jpg`. The frame-time comparison for this
  round was unusable (the machine was busy: 20.1 ms with the effects on, 22.8 ms with them off), so the cost of the
  stronger set is not measured.
- **Dust devils.** The project's own dust devils (`ULandscapeAmbientFxSubsystem`) already ran on this map through the
  landscape's sand physical material (820 flat cells). `SandMask_Ger_2ndOreMountains` is now baked
  (`RTS.AmbientFx.BakeSandMask`): 583 flat sand cells, so devils no longer form on rock, scrub or asphalt paint.
  Rebake after repainting the landscape.

## SandStreak masked rework (2026-11-16)

Owner: the 21 `ORE_Fx_SandStreak_Streak_nn` use translucency with "crazy overdraw that is too expensive"; make them
masked (no dither AA), design textures for a similar look, and optimise `NS_SandPush`.

- **What made it expensive.** `NS_SandPush` (Fab pack, used only by this map and the pack's demo `BP_Sand`) has four
  GPU emitters, all lit translucent and depth-sorted: smoke A/B sprites of 13-26 m (500-1000 cm x
  `ParticleSmokeSizeScale` 2.6) at 35/s each (about 90 + 50 alive per streak), 1-3 m sand sheets on `SM_Sand`/`SM_Sand2`
  at 110/s (about 275 alive), and 3-5 cm grains at 2000/s. Shader complexity over a streak was solid white.
- **Technique: stochastic coverage, not blending.** New master `M_SandStreak_Masked` (masked, clip 0.5, two-sided,
  default lit with a world-up normal so the sand is lit like the ground, decal response none, no DitherTemporalAA).
  It computes the pack's own opacity, `c = pow(ParticleAlpha * D, Dyn.A) * pow(|N.V|, DotPower)`, boosts it for the
  lower particle count, `c' = 1 - (1 - c)^Boost`, and draws the pixel only where `c'` beats a clumped grain noise. The
  drawn fraction of an area is then `c'`, and overlapping sprites cover `1 - prod(1 - c'_i)`: the same expected value
  translucent alpha accumulates to, but every pixel is shaded once and nothing is sorted. Fades thin the grains
  instead of popping. The dynamic parameters keep the pack's meaning (R phase, G add-white, B emissive, A power).
- **Textures** (`ore_sandstreak_textures.py`, linear, mips with alpha-coverage scaling on the noise channel):
  `T_SandStreak_Wave` 512x256 (R/G = the two pack sheet flipbooks' alpha, B = noise), `T_SandStreak_Puff` 2048x2048
  BC7 (R/G = alpha of the HDR smoke flipbooks `T_C_Cam09`/`T_C_Cam05`, B = noise), `T_SandStreak_Grain` (disc),
  `T_SandStreak_GrainNoise` 256x256 tiling grain noise (round 2). They replace 2 x 4096x2048 DXT5 + 2 x 4096x4096 HDR.
- **System edits** (UECP, on `NS_SandPush` itself): renderer materials -> `MI_SandStreak_WaveA` (mesh override slot 0;
  both sand meshes have one material slot, so the stale slot-1 entry `MI_SandPush_Inst2` is never drawn, as in the pack),
  `MI_SandStreak_PuffA/PuffB`, `MI_SandStreak_Grain`; SortMode None on all four; SpawnRate inputs unbound from the user
  parameters and set to literals: sheets 40/s, smoke A 10/s, smoke B 6/s, grains 400/s. The actors' four `*SpawnRate`
  overrides are now dead values (noted in `ore_fx_gen.py`). The pack's own materials and MIs are untouched
  (`MI_Ribon_Inst` feeds 17 systems).
- **Build tools:** `ore_sandstreak_survey.py` / `_survey2.py` (read-only exports), `ore_sandstreak_textures.py`
  (`--preview` composes the old and new look over an in-game shot), `ore_sandstreak_build.py` (headless import + master +
  MIs; its DynamicParameter links need the UECP step in its docstring), `ore_sandstreak_round2.py` (grain-noise branch +
  MI retune, run in the main editor), `ore_sandstreak_ab.py` (A/B run, private command folder
  `Saved/OreMountains/sandstreak/cmd` via `sand_server.py`/`sand_cmd.py` there). Shots in `Saved/OreMountains/sandstreak/ab`.
- **Round 1 result:** shader complexity over a streak went from solid white (many translucent layers) to one green
  masked layer. ProfileGPU dumps from the throwaway editor were not usable (some profile the level editor viewport).
  The look was wrong: grains were 10-30 px blobs (noise baked per flipbook texel on 20 m sprites) that boiled frame to
  frame, the core was ~0.49 covered against ~0.23 effective for the old haze, and the sand rendered near white over the
  tanks. Round 2: tiling grain noise at `NoiseTiling` 3 (puffs) / 0.3 (sheets), Boost 2.5 / 3.5 / 2.0, `ColourGain`
  0.7, `EmissiveScale` 0.3.
- **Round 2 result** (`Saved/OreMountains/sandstreak/sandstreak_before_after.jpg`): warm, fine-grained sand plumes
  along the wind, shader complexity still one green layer. Against the old haze it reads as a narrower band of
  blowing sand rather than a soft wide veil (the faint outer haze becomes sparse grains), and grain patches still
  shift from frame to frame where many sprites overlap. Tuning knobs, all on the MIs: `Boost` (density),
  `NoiseTiling` (grain size), `ColourGain` / `EmissiveScale` (brightness), `Erode` (global thinning).
- **Open:** the scratch A/B copy was deleted after the run; the mesh renderer still lists the unused pack MI in
  override slot 1 (the bridge tool can only write slot 0); the BEFORE/AFTER GPU timing is not measured; nothing is
  committed.

### Round 3: few large drift sheets (2026-11-16, evening)

Owner after round 2: "too many small pieces, which makes it look very flaky and fake ... larger particles with more
detail ... efficiently using more of the UV space". Rounds 1-2 are superseded on the map (their MIs stay assigned to
the now silent emitters).
- **System (`NS_SandPush`):** only Smoke_Emitter_A draws: 1.2 sprites/s, lifetime 5-7.5 s (about 7 alive per streak),
  lying flat (own Set Parameters module `Particles.SpriteFacing` (0,0,1); the pack's facing module was removed because
  the bridge cannot change its value) and aligned with the 35 degree wind (Set Parameters `Particles.SpriteAlignment`
  (0.819, 0.574, 0), world space), FacingMode CustomFacingVector, fixed bounds +-30 m. Smoke B, the sheets and the
  grains spawn 0. Renderer material `MI_SandDrift_A`.
- **Atlas (`ore_sanddrift_atlas.py`):** `T_SandDrift_Atlas` 2048 2x2: R = density of large sand tongues whose rows end at
  random lengths downwind (frayed fingers), G = body shade; `T_SandDrift_Detail` 1024 tileable: normal XY + lineation
  shade, scrolled downwind (`PanSpeed`). Coverage is a plain clip of the smooth density (no grain noise).
- **Material (`M_SandDrift_Masked`, `ore_sanddrift_build.py` + main-editor edits listed in its docstring):** cell picked
  by Particle Random; fade from particle age (`FadeIn` / `FadeOut`), not particle alpha (the pack's alpha is tiny);
  colour = `SandTint` x `ColourGain` (Smoke A's particle colour renders white); WPO lifts the sheet (`LiftCm` 70) and
  scales each quad about its particle (`SizeMult` 3: the emitter's size comes from a nested random range the bridge
  cannot reach). Two-sided, masked, decal response none.
- **Look check** (`ore_sanddrift_look.py`, MI brackets in memory, shots in `Saved/OreMountains/sandstreak/drift`,
  sheet `Saved/OreMountains/sandstreak/sanddrift_before_after.jpg`): large wind-aligned sheets with frayed fingers,
  tanks stand above them. Traps found on the way: sprites first stood vertical (facing value never written), then
  vanished (alignment module added with a zero vector), then were white (particle colour). After the last shots the
  tint was warmed to (0.66, 0.48, 0.29) and ShadeContrast 0.6 / NormalStrength 1.0 set.
- **Polish (owner: "Keep, polish it" - the ground-sheet look stays):** look check with brackets ColourGain 0.42 / 0.5 /
  0.6 and a detail bracket (shots `Saved/OreMountains/sandstreak/polish`, sheet `sanddrift_polished.jpg`). The warm
  tint matches the ground hue at ColourGain 0.5 (0.6 reads as pasted paper, 0.42 slightly dull); stronger inner detail
  (ShadeContrast 0.95, NormalStrength 1.7) turns the flat fill into wind-brushed sand lineation, so those were saved
  on MI_SandDrift_A. Sheet heads are rounded (atlas cells are empty at their borders: no quad-edge cuts). Motion
  frames 0.75 s apart show sheets growing in and thinning out over their life, no popping.
- **Dither (owner: "use some dither ... right now it is too opaque"):** new `DriftDither` Custom between `DriftMask` and
  OpacityMask: coverage ramps from the density threshold over `EdgeSoft` up to `Opacity` and is screen-doored against
  interleaved gradient noise, re-offset every frame (`TemporalDither` 1) so TSR resolves it into see-through sand.
  Brackets (shots `Saved/OreMountains/sandstreak/dither`, taken with `HighResShot 1` so the TSR history stays valid):
  Opacity 0.45 / 0.6 nearly vanish; 0.8 with EdgeSoft 0.5 reads as streaky sand with the ground showing through and
  feathered edges - saved on MI_SandDrift_A. A static dither looked about the same in stills; temporal kept for motion.
  Still masked (no translucency, no DitherTemporalAA node); a fully covered pixel costs the same as before.
- **Open:** a sheet crossing a terrain rise is cut with a hard line (masked cannot depth-fade; raise `LiftCm` if it
  shows). Dither crawl not judged in motion; GPU cost not measured. Nothing committed.

## Mid-map relief and the dead forest (2026-11-13, evening)

Owner: "work on the dead foliage for this map, use what we already have ... make sure there are some interesting
height changes in the middle of the map; still traversible with vehicles ... adjust the landscape blending for that".

**Relief (`ore_terrain_gen.py`, block "2b").** Swells, nine low ridges (to about 4.8 m), four hollows (to about 3 m)
and a dry wash (1.9 m deep, 14 m wide, between the road and the foothills) on the valley floor and the foothills;
`RELIEF_GAIN` scales all of it. Slopes in the changed ground: median 4, 99th percentile 25 degrees (nav limit 44).
Nothing changes on the bench, within 20 m of a rock face, 25 m of the viaduct line, 10-19 m of the road, on ramps or on
the level pads: the cliff kit and the viaduct sit at absolute heights, and a check after generating confirmed 0.0 cm of
change under every cliff and viaduct piece (`Ore_height_before_relief.raw` is the reference). Nav check: 0 problems.

**Paint for the relief.** From the RTS camera (pitch -52 to -70) a 4 m swell barely shades, so the paint carries it:
slopes facing the map's sun are pale (MountainTop), lee slopes darker (RTSSand), crests pale, hollows darker with a
little scrub, drifted orange sand (GraySand) in the wash, bare rock where a swell is steeper than about 11 degrees.
Instanced stones lie on the crests and in the wash, dead scrub and dry tufts in the hollows
(`ore_detail_gen.py`, masks `relief_crest`, `relief_hollow`, `wash`). The sand mask was rebaked (582 cells).

**Dead forest from existing content (`step_trees`, labels `ORE_Tree_*`).** The instanced stand-ins are gone; every
tree in the play area is a real actor:
- 298 `BP_Scorch_Dest_DeadTree` (stout spiky trunks) and 205 `BP_Scorch_Dest_DeadWillow` (thin bare crowns): tanks
  crush them, their `BlockChar` child keeps infantry out of the trunk. The mesh is varied per tree (six dead-tree
  meshes, five willow meshes) by setting the Blueprint's `StaticMesh` component; the destroyed state is still the
  Blueprint's own.
- 17 survivors: `BP_WpoSpruce_SmallDark`, `BP_WpoSpruce_Small`, `BP_WpoFir_Small` in the scrub pockets and at a few
  wood edges.
- Dense woods mix 70 % stout / 30 % willow, thin woods about half and half; about 80 lone dead trees are dotted over
  the valley and foothills (none on the bench, at sites, on the Radixite field or the knoll).
- 38 fallen logs as cover (was 26). The camera margin keeps its instanced trees (no units go there).
- Not usable: `Fab/Dead_Trees/dead_trees` is one 600 km wide mesh of a whole tree set (wrong import scale);
  `SM_RTS_AshTree_Log_*` have no material (world grid); the Megascans dwarf birch has 63k triangles at LOD0.

A concept round for a proper dead-spruce kit (`CustomMapItems/Environment/OreDeadForest`, sheets in
`Saved/OreMountains/foliage`) gave five options; the owner picked **B "Gerippe"** (whorled grey spruce skeletons) on
2026-11-13. It is being modelled with the fossil roots R2 (ribs) and R1 (root plate) and imported under
`/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest`, to plug into the crushable Wpo tree setup;
placement data comes as `Saved/OreMountains/foliage/OreDeadForest_pieces.json`.

## Dead spruce kit in the map (2026-11-13, night, `step_trees`)

The owner picked concept B "Gerippe" (whorled grey spruce skeletons). Assets:
`/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest` (20 meshes `SM_OreDead_*` on one 2048 atlas, four
material instances of the existing Wpo masters, nine crushable `BP_OreDeadSpruce_<Piece>`; source, brief and README in
`CustomMapItems/Environment/OreDeadForest`). Placement data: `Saved/OreMountains/foliage/OreDeadForest_pieces.json`.

- **Trees (`ORE_Tree_*`):** 659 actors in the play area, mixed by the kit's shares for dense woods, thin woods and lone
  trees: 444 skeletons in four sizes, 85 silver snags, 75 snapped, 33 leaning, 15 dying survivors, plus 7
  `BP_Scorch_Dest_DeadWillow` among the lone trees. They are `AWpoTree` (medium and heavy tanks crush them; light
  tanks do not, inherited from the Blueprint they were copied from; the owner confirmed "Keep medium+heavy" on
  2026-11-14, so the dead woods stay an obstacle for light vehicles). Dense woods are about 2.4 trees per 100 m2.
  The old stout dead trees and the living Wpo spruces are gone from the play area.
- **Windthrown (`ORE_Timber_*`):** 41 `Windthrow_12m` as one instanced component with its collision.
- **Fossil roots (`ORE_Roots_*`):** at the five gully spots: `Roots_R2_Ribs` across the cut (pitched to the floor
  slope from the kit file), `Roots_R1_Plate` on the bank, one `BP_OreDeadSpruce_Black_8m` as a marker. The five
  placeholder blocks are gone. The ribs were fitted to a level test gully; spots 1 and 2 have about 1 m of floor
  relief under them and may need hand seating.
- **Woods floor and margin (`ore_detail_gen.py`):** the kit's stumps, fallen tops, brash, bilberry mats, dead fern
  and tussocks replace the old oak stumps; the trees past the play edge are instanced kit meshes.
- **Fix made after the first in-game look:** from the game camera the trees read as bare poles, because the thin twig
  alpha averages out of the lower mips of the masked cards. `Opacity_Instensity` is now 3.0 on
  `MI_OreDeadForest_Cards` and `_Cards_Still` (1.0 as delivered); with that the whorls and the fishbone shadows read.
  Setting the atlas's alpha-coverage thresholds from Python was attempted and not saved.
- Tall thin trunks seen from a 28 m high camera project as lines radiating from the picture centre: that is
  perspective, not fallen trees.
- Nav check after placing: 0 problems.
- **Crush test (`ore_crush_test.py`, play session):** a Panzer III M slid through a `BP_OreDeadSpruce_Skeleton_9m`: the
  tree was destroyed within 9 s and the tank left ruts in the ground. A Panzer 38(t) through another one: the tree
  stood unchanged (the trees take medium and heavy tanks only). The test moves the tank kinematically, so it shows the
  crush overlap works, not how a tank paths through a wood; the fall itself was not captured, only before and after
  (`Saved/OreMountains/shots/crush_sheet.jpg`).
- **Not verified:** wind motion and the cost of 659 tree actors (no profile taken).

## Not done yet

Real enemy sites, border dressing, gully and bog
detail, grass output, surface-map bake, map margins east and west (they repeat the edge profile).

## Landscape critic rounds and realism round (2026-11-14)

Owner request: an adversarial agent reviews the landscaping and layer blending, five reviews and five improvements, then
one more round aimed at realism. Every round: `ore_ls_round.sh <tag> paint|full` rebuilds, runs the nav check, saves and
takes the fixed in-game views into `Saved/OreMountains/critic/<tag>/` (sheets + top view). Critic grades went from
shapes 4 / blending 3 (out of 10) to 6 / 6 after round 5; the realism review of that state gave 4 / 10 and led to round 6.

What was learned about the material (holds for any map on the DryCastle landscape master):

- **No scalar removes the cloudy look, one removes the orange blotches.** The blotches are the Desert layer's variation
  texture let through by a noise mask: `Noise_Mask_Power` 0.35 and `Texture_Scale_Noise_Near` 8 on this map's own
  instance (`LANDSCAPE_SCALARS` in `ore_map_build.py`) thin them to small flecks.
- **Paint edges cannot be sharper than about 2 m.** Weights are one per metre vertex and interpolated; a hard 0/1 step
  in the weight file still shows as a 1.5-2.5 m ramp at game zoom (a view is about 32 m wide). A crisper edge has to
  come from heights, stones or decals.
- **There is no dark layer.** RTSSand reads salmon red in sunlight, Asphalt at full weight is mid grey and below about
  0.85 barely darkens, RockDesert at high weight on a slope reads charcoal. Dark needle litter or peat needs a new layer.
- `MaterialEditingLibrary.set_material_instance_scalar_parameter_value` in a loop over every parameter turns them all
  into overrides. `tune_landscape_material` resets the instance to the two intended ones.

What the rounds built (all in `ore_terrain_gen.py`, `ore_detail_gen.py`, `ore_map_build.py`):

- Paint: all former RTSSand "dark" moved to partial Asphalt; bench pale against gravel; valley half pale with long
  GraySand drifts along the wind; foothills with a rock base tone; thresholded, noise-broken edges.
- Shapes: ribs as cuestas (steep shaded face, long sunny back); bogs as flat pans 1.4 m down behind a cut bank with peat
  hags; the wash with scalloped banks, gravel bars and a bed that runs edge to edge; deflation pans; a continuous road
  ditch on the north side (0.8 m). A gully meander was added but only applies mid-length, away from the rock faces and
  fossil-root spots: it moved about 0.1 % of the ground.
- Scatter (`stone_scatter`): stones in clumps with a size mix, sunk a third into the ground, only where something put
  them (faces, beds, crests, heaps, fans, headwalls, crater rims); tussock belts on bog rims; stump and brash belts
  round the woods. 21.9k instances (5.3k before the rounds); cost not measured.
- Realism round: a wear network of 14 two-track paths (3 km) routed by least cost over the built ground from the start
  to every ramp, the road, the adits, the overlook, the spoil heaps and the knoll ramps (`build_wear`, exported as
  `wear_tracks`, placed by `step_roads`); 28 shell craters (9 sunk, the rest paint and rim stones: beside the viaduct,
  round the battery and checkpoint, along the bench lip); every gully and bog drains to the wash by a rill with a fan.
- Fixed ground never moved: `Saved/OreMountains/check_fixed_ground.py` compares the heights under the cliff kit and the
  viaduct with the reference raw after every regeneration (0.0 cm every round).

Not done, with reasons:

- Crater decals: the project's dirt crater decals render brick red on this ground; craters are heights and paint only.
- Rides through the woods, straight compartment edges, heaps moved to the adit mouths, flat-topped tips: these change
  the sketch or vehicle pathing and are the owner's call.
- The ditch reads weakly; the yard slabs were never judged (always in building shadow in the fixed view).
- The wood floor still reads pale and the shaded wash bank still reads as a dark band (no dark layer, see above).
- From the realism review, not built: a yard turning loop, checkpoint pull-offs, braided tracks at ramp feet, talus
  aprons beyond the cliff kit's own scree, seep lines to the bogs, road headwalls only as stone clumps.
- 3 of the 17 track legs found no route and were dropped (the generator prints the count, not which).
- Grades: 6 / 6 was given on the round-4 state; improvements 5 and 6 were not graded again, and the 4 / 10 realism
  grade was given before round 6.
- Cost not measured: 21.9k scatter instances and about 1 600 two-track spline tiles (separate components) were added.
- Tool trap: a patch written through a shell heredoc left a literal backslash-n in the generator, and two rebuilds were
  saved with stale paint because the generator's exit code was not checked. Edit the generators with the editor tools
  and check the exit code before a rebuild.

## Dark earth layer and ground decals (2026-11-14)

**DarkEarth layer.** The DryCastle master had no dark ground, so it got a tenth weight-blended layer, `DarkEarth`
(forest-floor soil in world space at two scales, desaturated and tinted). Builder: `Source/Docs/Landscape/ls_darkearth_build.py`
(`run()` builds and checks, `save()` saves); layer info `DATA/DarkEarth_LayerInfo` from `ls_darkearth_layerinfo.py`
(headless run). The layer's weight is also added to the tank trails' SoftGround; `ls_master_build.py` does the same
from `SOFT_ONLY_LAYERS` on its next run.

- Shader stats of the master, `_Inst`, `_Trains` and this map's instance were the same before and after (the preview
  weight is 0). A landscape that does not paint the layer compiles it out.
- This map overrides `DarkEarth_Tint` (0.31, 0.26, 0.22) and `DarkEarth_Desaturation` 0.3 on its own instance
  (`LANDSCAPE_VECTORS` / `LANDSCAPE_SCALARS` in `ore_map_build.py`).
- Paint (`ore_terrain_gen.py`, `earth`): dense wood floor 0.78-0.96 with stone gaps, thin woods in patches, a fading
  skirt past the last trunks, peat bog bodies and hags, crater bowls, gully bank feet, the road ditch, lee slopes and
  hollows (faint). Asphalt soot is left for oil patches and paving only.
- Wash bank: the RockDesert weight that rendered as a charcoal band is more than halved; a narrow patchy damp-earth
  band (0.3) sits at the bank foot instead.
- The sand mask was rebaked: 269 of 900 cells (582 before the critic rounds), so fewer dust devils.

**Decals** (`ore_map_build.step_decals()`, labels `ORE_Decal_*`, plan in `decal_plan()`; it runs after every rebuild
in `ore_ls_shots.py`). Candidates were judged in game first (`Saved/OreMountains/critic/decals/sheet.jpg`).

- Every shell crater: a scorch star, thrown debris, and a crater decal where the ground could not be sunk.
- Soot under the four tank wrecks, the smoulder spots and the yard fuel store.
- An erosion channel where each rill starts and half-way down it.
- Left out: `MI_Decal_Dirt_TracksSandy_*` (brick red on this ground), the Zone `MI_Decals_*` (paint markings), embers.

**Not done / not checked**

- `DarkEarth` is not in the ground-surface settings (track smoke tints, surface map): tanks on it get the default.
- Tank ruts on the dark earth are wired but were not watched in a play session.
- Dark earth on open sunlit ground still reads a little like a cloud shadow (ditch, lee slopes): it is kept faint there.
- Decal cost is not measured; no decal was checked for overlap with the wear-track tiles one by one.
- Views: `Saved/OreMountains/critic/dark2`, comparison `critic/dark_before_after.jpg` (top r6d, bottom now; the camera
  sat higher in the second set).

## Base B road (2026-11-14)

User request: a second road of the same macadam spline from the south-west corner to the Soviet base, with curves, and
an intersection piece where it crosses the valley road. My reading: "the Soviet base" is Base B on the knoll (the one
in the screenshot); the road starts on the bench in the west margin, comes down the west shoulder (not down the
player's ramps), crosses the valley road at about (2760, -20500) at a right angle and climbs the knoll's west ramp.

- Route: `ore_road_route.py` (least-cost over the built ground with a grade cost, inside the play area once off the
  shoulder, away from cliffs, bogs, pads, craters, heaps) -> `ore_base_b_road.json`, then smoothed and cut back to the
  ramp top by hand after the first in-game look. About 450 m in the play area against about 400 m straight.
- Terrain (`ore_terrain_gen.py`, step 7c): graded bed (long profile smoothed over about 60 m, up to 1.4 m of cut and
  fill, side slopes blended over 10 m), pinned on ramps, pads and rock faces. Steepest stretch off the ramp 17.5 %.
  The valley road's ditch stops short of the crossing. Cliff kit and viaduct ground unchanged (0.0 cm).
- Paint, scatter, trees, craters and rills keep off it (shared `road` mask, `wear` mask); wear tracks are cut where they
  would run along it. `step_roads()` places it as `ORE_Road_BaseB`.
- Nav check has two new legs (west shoulder -> crossing, crossing -> Base B): all agents, 0 problems.
- Junction: `ore_map_build.step_junction()` places `BP_GroundPath_RoadJunction` (subagent-built overlay, spec in
  `Source/Docs/Landscape/path_road_junction_build.py`) as `ORE_Junction_BaseB`, with pivot, yaw, height and skew read
  from the two placed splines (skew -4.2 degrees here). It runs after `step_roads()` in a full rebuild. In-game view:
  `Saved/OreMountains/critic/junction/junction.png`.
- Open: the road links the
  player's bench to Base B, which the mission design has not seen; the sketch JSON and mission doc do not have it.
  Views: `Saved/OreMountains/critic/road3` (33-39).

## Intact viaduct spur at Base B (2026-11-14)

User request: a largely intact stretch of the viaduct with an arch between the old north end (P4) and the Base B
knoll, drivable from the base, not connected to the other pieces. Four greybox concepts
(`Saved/OreMountains/viaduct/v04`); the user picked **C Kurve** (three arches on a curve, deck falling 4 % from the
plateau, broken end 9.2 m short of P4).

- Asset `SM_A4Viaduct_Spur_Kurve` (14k triangles, 17 UCX hulls, kit materials), built by a subagent; source
  `CustomMapItems/Environment/A4Viaduct/Model/Spur`, placement file `Saved/OreMountains/viaduct/A4ViaductSpur_pieces.json`.
- `ore_map_build.step_spur()` places it at (9750, -4380, 1603) yaw 134.5 as `ORE_Spur_Kurve` and clears its landing:
  cliff-kit instances 662, 663, 664, 701 are left out by `step_cliffs()`, trees inside the footprint, scatter on the
  approach slab and props within 6 m of the slab are removed (one Base B sandbag arc, `BaseB_Sandbags_2`, stood on the
  slab and shut medium and heavy tanks out). It runs last in a full rebuild.
- Nav check: every agent drives from Base B to the deck near the broken end; the deck is not reachable from the ground
  beside it. 0 problems. Bay 1 under the arches is closed, bays 2 and 3 are open (6.5 m between piers).
- Open: the layout's Base B AT gun point (10211, -5113) is 2.5 m behind the landing (a layout point only, nothing
  placed there yet); no arch face shows from the default camera (the plan shape does); units under the open bays and
  on the deck were not watched in play. Views: `Saved/OreMountains/critic/spur2`.

## Cliff weathering decals (2026-11-14)

User request: decals for the cliff kit that fit its palette; the first round was "barely visible", the second
(stronger) round is the one placed. Built by a subagent: 19 decals (seep and soot curtains, weather and leach bands,
calcite, rust field, radixite seep, blast fan, drill half-casts, three painted marks; for the top-down view damp
patch, lichen crust, soot fall-out, rust wash, ore fines, rock-flour fan). Source and spec:
`CustomMapItems/Environment/OreCliffKit/Decals/v02` (`decals.json`); Unreal assets under `CliffKit/Decals`
(`M_OreCliffDecal` with an angle fade, `MI_OreCliffDecal_*`).

- `ore_map_build.step_cliff_decals()` places them from `Ore_cliffs.json` (labels `ORE_CliffDecal_*`, 316 decals):
  stains and bands on the face pieces by a rate per 20 m, patches on a quarter of the lip tops, flour fans at a tenth
  of the talus pieces, and at each adit a blast fan, a painted mark (STROSSE 2, Cyrillic warning, German warning), a
  soot curtain, a rust field with its wash, soot fall-out and ore fines; one radixite seep by adit 2. It runs last in
  a full rebuild.
- Found on the first placement: a decal maps image U along its local Z, so every decal is turned a quarter with width
  and height swapped; the adit decals are anchored to the real face pieces (the adit point lies in front of the rock);
  in the engine the instances read far weaker than in the Blender previews, so `tune_cliff_decals()` raises opacity and
  darkens the stains (values in `CLIFF_DECAL_TUNE`, saved into the instances); the first full set (486) striped every
  block, so the rates were halved.
- The adit placeholder boxes no longer receive decals; the marks sit on the portal lintel behind them and show once
  the placeholders are replaced.
- Open: from the default top-down camera only the fans, fines and lip patches show; the wall stains need a rotated
  camera. Cost of 316 decals is not measured. Editor views: `Saved/OreMountains/cliffs/decals/ue/all2_sheet.jpg`.

## Crushable rocks (2026-11-14)

- Kit: `Environment/OreMountains/CrushableRocks/` (16 rocks, one atlas; see `Docs/Environment/OreCrushableRocks.md`).
- Plan: `ore_crush_rocks_gen.py` (own seed, own file `Ore_crush_rocks.json`, preview `Saved/OreMountains/rocks/placement_plan.png`).
  284 rocks (37 medium, 247 small) in 96 clumps: rockfall at cliff feet, ramp fans, the wash, scoured crests, crater
  rims, road shoulders, a few strays. Keep-out: roads, tracks, wear tracks, ramps, pads, gullies, bogs, spur, viaduct,
  bases, resources, adits, 32 m round the HQ. Gaps: 3 m round a medium rock, 1 m round a small one.
- Build: `ore_map_build.step_crush_rocks()` places single Blueprint actors `ORE_CrushRock_*` (instances would have no
  crush), sunk by the plan's amount for the slope. In the full rebuild order after `step_spur()`.
- Checked: `NAV PROBLEMS 0`; editor views in `Saved/OreMountains/rocks/map/`. The nav check does not use the per-agent
  area filters, so the gaps in the plan are what keeps infantry and light vehicles from being walled in.
- Open: no crush driven on this map; puff and sound of the crush are still the building-sized ones.

## Large rockworks (2026-11-17)

- Kit: `Environment/OreMountains/Rockworks/` (8 formations 5-10 m, not crushable, shell holes, `NavArea_Null`; see
  `Docs/Environment/OreRockworks.md`). Holes show flat back faces inside the solid rock; the user kept that as is.
- Plan: `ore_rockworks_gen.py` (reuses the keep-out part of `ore_crush_rocks_gen.py`; file `Ore_rockworks.json`,
  preview `Saved/OreMountains/rockworks/placement_plan.png`). 15 rockworks: 6 at cliff feet, 4 on crests, 5 on open
  ground. 6.5 m free all round, 60 m apart, none inside the HQ build radius, scale 1 only.
- Build: `ore_map_build.step_rockworks()` (labels `ORE_Rockwork_*`; skips a spot when an actor this tool did not
  place stands within 12 m). In the full rebuild order after `step_crush_rocks()`.
- Checked: `NAV PROBLEMS 0`; editor views in `Saved/OreMountains/rockworks/map/`. Not fired at on this map.
