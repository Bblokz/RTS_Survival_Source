# 10 — Level design: SM_Desert_01

Map: `/Game/RTS_Survival/SurvivalMode/Maps/SM_Desert_01`. Owner: the level worker. State: review round 6
(2026-11-01). Everything below is produced by the scripts in `LevelTools/` and is reproducible from them; the review
history with images is in `Saved/AgentNotes/level_review/round_N/`.

## 0. Round 8: scaled to one third of the area (current map)

The owner found the map too large, so the round 7 design was scaled down with `LevelTools/survival_scale.py` (run on the
generator output): XY x0.577 (sqrt 1/3), heights x0.75 around Z 600, landscape 14 x 63 quads (88 200 cm), playable
78 288 cm, start ring r 24 541, build discs flat to r 5 700 (islands grow only by what the bigger disc needs and blend
back to the plain scale 110-140 m out), every ramp re-stamped at >= 620 cm half width with gates regraded (p95 5.9-14.8
deg). The oasis stays full size at its centre; the landforms give way round it (S3 and S4 lost their oasis-side gates and
got new ones, north_road and east_road). Positions in the sections below are the round 7 full-size values; the current
ones are in `LevelTools/SM_Desert_01_layout.json` and `Saved/AgentNotes/level_review/round_8/README.md`.

Round 9: the oasis is 75 % of its size about its centre (heights x0.75 about the water, Z 372; palms and plants moved in
and thinned, 93 palms remain), with drivable banks (<= 16 deg) graded into the non-pad ground round it. Red Mesa has its
bench_road back (north_road removed); Dry Wells keeps east_road in place of wash_ford, whose head is still inside the oasis.

## 1. The map in one paragraph

Five start islands (raised benches) stand on a pentagon of radius 40 000 around a central eroded table. Between them
lies a low basin with an oasis (east of the table) and a salt flat (Salt Flats' outer pan). Each island has its own
designed stamp: a flat build disc, a content band, 2-3 gate ramps from the desert, ridge platforms that become gun pits,
and rock-faced scarps. The islands are separated by five natural chokes (the gaps, 23-51 m of drivable ground). Waves
enter from 10 spawn zones along the edge of a 1.36 km playable square; outside it a dune rim and a flat sand skirt hide
the end of the world. Owner decisions: "slide a little" closer (R 42 000 → 40 000, round 5), crop to about 136 000 cm,
keep the oasis exactly as it is.

## 2. Landscape and playable area

| Item | Value |
|---|---|
| Landscape | `SM_Landscape`, 151 200 cm square centred on (0, 0), 24 × 24 components of 63 quads, 1513² vertices, 100 cm quads, no edit layers |
| Material | `MI_RTS_Landscape_DesertPaintable`; layers Sand, DesertGravel, DesertRockDark, ScorchConcrete, ScorchSandyStone, Weedy |
| Playable square | x -69 400..66 200, y -64 400..71 200 (135 600 cm a side, centre (-1 600, 3 400)) |
| FOW / camera bounds | `BP_FogOfWar_Manager` at (-1 600, 3 400, 600), `MapExtent` 67 800; the minimap capture follows it |
| Boundaries | `BoundaryTop/Bottom` at x 66 800 / -70 000, `BoundaryRight/Left` at y 71 800 / -65 000, 600 thick |
| Nav | `RTS_NavMeshVolume` at (-1 600, 3 400, 1 500), extent 68 000 × 68 000 × 3 000; 4 agents; rebuild before every save |
| Outside the square | the landscape rises in a dune rim (humps 2-8 m on a wobbling crest, rounded 80 m corners, tallest behind Dune Ridge), its outer 9 m fall to Z 470 where four flat `SMD_HorizonSkirt_*` planes (landscape sand, no collision, no nav, hidden from the minimap capture) run to 150 000 cm. 16 edge/corner camera shots show no void. |
| Saved size | about 128 MB (most of it navmesh). A save while navigation builds drops tiles (25 MB): always wait for `IsNavigationBeingBuilt` to be false. |

## 3. Starts

| # | Name | Deploy XY | Z | Gates (main first) | Identity |
|---|---|---|---|---|---|
| S0 | Dune Ridge | (0, 40000) | 1420.3 | front_pass, back_road, flank_road | long spine across the front with dunes in its lee, rear shelf |
| S1 | Salt Flats | (-38042, 12361) | 1120.3 | pan_grade, north_track, south_track | low bluff on a salt flat, hogback behind |
| S2 | Scorpion Basin | (-23511, -32361) | 1560.2 | mouth, rear_gap | basin with two claws round a forecourt, tail ridge |
| S3 | Red Mesa | (23511, -32361) | 2260.2 | bench_road, cliff_road | highest table, bench on the oasis side, long cliff road |
| S4 | Dry Wells | (38042, 12361) | 1679.7 | wash_ford, long_road, back_gate | high table, low terrace and wells on the wash side |

- Build disc: `BuildAreaRadius` 8 000, exactly flat to r 8 400 (generator check: z range 0 on all five).
- Content band: fields at r 10 400-14 600, wrecks 13 000-17 500. Everything beyond r 14 600 of a start may be trimmed; in
  round 5 a few arms were pulled in (`TRIMS` in `survival_landforms_desert01.py`).
- Gate ramps: slope p95 3.9-16.3° (all under the 25° drivable target). S1 south_track and S4 wash_ford are graded the
  whole way (`full=True`); the road paint fades over its last ~28 m on the pad.
- `DesignerDefensePoints` (first 3 become pits): S0 pass_west, pass_east, rear_knoll_top; S1 shore_knoll_top,
  south_bluff_top, north_bluff_top; S2 claw_right_top, claw_left_top, pincer_right; S3 rim_south_top, road_guard_top,
  rim_north_top; S4 wall_south, nose_top, road_knoll_top. Positions: `LevelTools/SM_Desert_01_layout.json`.
- `ReservedContentSpots`: circles along every ramp (0.55 of its width) and, on S1, the salt flat outside the gate, so no
  field lands there.

## 4. Between the starts

- **Gaps** (chokes between neighbours; widest drivable passage, slope < 25°): S1-S2 ≥ 51 m, S2-S3 23 m, S3-S4 29 m,
  S4-S0 27 m, S0-S1 ≥ 51 m. Three were widened in round 6 by corridors along the existing passage (`GAP_CORRIDORS`,
  `survival_frontier.apply_corridors`: floor set within 15 m, only lowered beyond; ramps, platforms and the oasis keep
  their ground). Every gap passes a single HeavyTank path query.
- **Oasis** at (15 500, -3 600), water Z 372: frozen by the owner. Its basin and shore heights and paint are pasted from
  round 4 (`SM_Desert_01_oasis_r4.npz`); the water planes, minimap stand-in, NavModifierVolumes and plants are the round 4
  actors and are never cleared by the scripts (`SMD_Oasis*`).
  - Round 7 (owner exception for the palms): the 128 palms are crushable Blueprints in
    `Content/RTS_Survival/SurvivalMode/Maps/Foliage/`: `BP_SMD_Palm_01..03` (`SM_palm_tree_01..03`) and
    `BP_SMD_PalmSmall_01..02` (`SM_small_palm_tree*`). Each is a copy of `BP_WpoLeafTree_Large` (child of `BP_WpoTree`:
    crush overlap, vertical collapse, DT row `Dest_TallTree`, `TankDestructionType = MediumOrHeavyTank`) with the palm mesh
    swapped for a copy (`SM_SMD_Palm*`) whose only collision is a trunk capsule (palms r ~100 cm, 5.2 m high; small palms
    r 45 cm, 1.8 m). `bDisableRandomScale` is on and the parent construction call (random tree yaw/tilt) is removed, so
    every palm keeps its placed XY, yaw, scale and look. All palms sit on the landscape (trunk-base ring, or the lowest
    LOD0 vertex for the small palms, against ground-only traces); 42 had been stacked on other crowns up to 56 m up.
    100 grass and reed clumps that floated more than 1 m above ground or water were dropped onto the ground the same way.
  - `-ConvoyTest=PalmCrush` (map-test builds) drives a Pz II Luchs, then a Pz IV H, through one palm and writes frames.
- **Central table** with its tail, shard, two ramps and six remnants: unchanged since round 3.
- **Salt flat**: a decal (`M_SM_SaltPanDecal`, generated `T_SM_SaltPanDecal`): since round 7 a plain sand crust about
  10 % paler than the sand round it (colour 0.30, 0.12, 0.058 times the texture), no cells, no green.
- **Routes**: 10 worn two-track paths (`SMD_Route_*`, `BP_GroundPathSpline_TwoTrack`), one outer and one inner per start:
  outer from the nearest edge zone to an outward gate foot, inner from the outer lane through a gap to an inward gate
  foot. Polylines in the layout json (`routes`). Round 7: the ruts use `MI_SM_RouteTwoTrack` (child of
  `MI_GroundPath_TwoTrack` with `PathSurfaceAware` off, sand tint; the map has no runtime virtual texture, which is what
  made the surface-aware ruts green), PathWidth 560. Under every route a worn band of `SMD_RouteDust_*` decals
  (`M_SM_RouteBand`, ~10 m wide with soft ragged edges, about 10 % paler than the sand, distance fade off) makes the routes
  readable from the top-down camera and the minimap. S3_inner and S4_inner stop 25 m short of the oasis palms.
- **Ramp heads**: `SMD_RampSpill_*` decals (`M_SM_RampSpill`, pad-coloured, ragged) over the last 14 m of every ramp,
  never inside r 8 200 of a start.
- **Power lines**: trunk, eastern branch and highline, replanned on the new ground (pole2 at bends, pole3 on tangents,
  pole1 highline); lowest wire 400 cm; the ends run over the rim off the map.

## 5. Wave spawn zones (`ASurvivalEdgeSpawnZone`)

| Zone | Centre | Edge | Active after minute | Weight |
|---|---|---|---|---|
| SurvivalEdge_0 | (-53000, 68000) | +Y | 0 | 1.0 |
| SurvivalEdge_1 | (40000, 68000) | +Y | 0 | 1.0 |
| SurvivalEdge_2 | (63000, 44000) | +X | 0 | 1.0 |
| SurvivalEdge_3 | (63000, -20000) | +X | 12 | 0.5 |
| SurvivalEdge_4 | (55000, -61200) | -Y | 0 | 1.0 |
| SurvivalEdge_5 | (0, -61200) | -Y | 0 | 1.0 |
| SurvivalEdge_6 | (-31000, -61200) | -Y | 15 | 0.6 |
| SurvivalEdge_7 | (-66200, -29000) | -X | 0 | 1.0 |
| SurvivalEdge_8 | (-66200, 34000) | -X | 0 | 1.0 |
| SurvivalEdge_9 | (28000, 68000) | +Y | 4 | 0.8 |

All 3 200 inside the playable edge and at least 21 000 from any build disc edge. Zones 3 and 6 are late because they sit
close to outward gates (S3/S4 and S2); zone 9 gives Dune Ridge a nearer threat after minute 4. HeavyTank path lengths from
every zone to every deploy centre are in the round 6 review folder (`game/nav_paths_s<N>.json`).

## 6. Dressing (all `SMD_`, outliner `SurvivalDressing/`)

Rock faces along the scarps (Megascans cliff pieces in chains, 1 900), foot boulders, top stones, crater decals, open-ground
props and cactus cover, wells at Dry Wells, shrub foliage (`FT_SM_DeadShrub_*`, 110 patches), the salt decal, the horizon
skirt, routes and power lines. `survival_map_build.clear_dressing` removes it; the round 5+ rebuild keeps `SMD_Oasis*`.

## 7. Rebuild pipeline (LevelTools)

1. `python survival_terrain_gen.py <dir> SM_Desert_01 --fast` — heights, paint, layout with dressing data; report shows disc
   flatness, ramp slopes, leak check with gates closed, platform coverage, power lines. About 7 minutes.
2. In an own DebugGame editor on the map (both agent locks): `smb.load_layout(<dir>/SM_Desert_01_layout.json)`, clear the
   dressing except `SMD_Oasis*`, `step_replace_landscape`, `step_place_start_locations`, `step_fit_level_actors`, zone
   moves; then `survival_map_dress`: `step_power_lines`, `step_rock_faces`, `step_rocks`, `step_craters`, `step_props`,
   `step_identity`; `survival_map_decals`: salt texture and material, `step_salt_decal`, `ensure_skirt_material`;
   `smd.step_horizon_skirt`; shrubs through UECP `execute_uecp_script` (`shrub_patches`, skipping patches within 95 m of
   the oasis); `ReservedContentSpots`; routes, route dust decals and ramp spill decals (round 7 scripts in the review
   notes, `level_review/round_7/README.md`).
3. `RebuildNavigation`, wait until navigation is idle, save, check the `.umap` size.
4. Checks: the planner audit (`ASurvivalStartLocation.LogPlansForMapTest`, 60 seeds × 5 difficulties), `-game` runs per
   picked start at Epic (`scalability 3`; the editor's `GameUserSettings.ini` is Low) with
   `PressStartForMapTest` so the world runs and Soviet bases build.

## 8. Known limits

- The ramp-head tabs still show their shape from above: the decals remove the colour edge, not the cut's geometry.
- Long single path queries go partial at `DefaultMaxSearchNodes` 2048; chained queries reach every gate in at most 3 hops.
- The `-game` capture reads `Saved/Config/WindowsEditor/GameUserSettings.ini` (Low); always run `scalability 3` for views.
- Editor and Development builds older than 2026-10-31 lack `ReservedContentSpots`; save the map only from a current DLL.
