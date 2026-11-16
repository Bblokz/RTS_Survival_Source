# Ger_IntroMission: the rear areas behind the Soviet base

Built on 2026-11-16 at the user's request. The ground between the base's rear wall (x 4139) and the map edge used to be
flat and empty. It now holds three curved two-track roads, a train station on its own ridge, a large concrete depot
platform behind a second ridge, two ponds, a dry wash, and dressing.

Coordinates are world cm. X runs from the base to the map edge, and the playable edge is the `TopBlocker` at x 24775.

## Layout

| Area | Where | What |
|---|---|---|
| Hub | (6500, -700) | Where the roads start, just past the fuel-truck parking spot by the long camo bunker. The truck lane (x 3900..5250, y -6200..-1000) and the two mounds behind the bunker are left untouched (keep-out boxes in the generator). |
| Road D (`REAR_Road_Depot`) | hub to (16700, -13600) | Curves south-east and climbs through a cut saddle (12550, -9500) in the depot ridge to the depot. |
| Road E (`REAR_Road_EastFade`) | from road D to (24300, -1300) | Curves east between the two ponds and runs out near the map edge, where an abandoned KV-1 wreck sits. |
| Road S (`REAR_Road_Station`) | from road E to the station forecourt (19750, 10100) | Curves north-east around pond W1 and ramps up the station ridge's west flank. The ramp is 17.7 degrees at its steepest. |
| Station ridge | spine x ~21000, y 3600 to the north map edge | An 8 m spine with a flat top about 35 m wide. The rail line (`REAR_Rail_Main`) comes from the north map edge and ends at a buffer stop at y 4950. A siding (`REAR_Rail_Siding`) runs beside the platform. |
| Station | top of the ridge, y 5500..17500 | Two `SM_Berlin_RailRamp` loading platforms, an `SM_Berlin_SwitchTower` as the station office, an `SM_Warehouse_03` goods shed, a lattice water tower and a water tank, two houses, and freight stacks. The parked trains are static `SM_SovietTrain_*` meshes (locomotive, fuel and goods cars), not train Blueprints. Telegraph poles run along the line. North of the station are sleeper piles and a lineman's hut. |
| Depot ridge | arc from the south map edge (7000, -25500) to (15100, -7600) | About 9.5 m high and steep, with a cut saddle for road D. Hedgehogs and two sandbag arcs guard the cutting. It sits between the depot and the base. |
| Depot | centre (18300, -15700), yaw 18 | A 72 x 48 m slab of 6 `BP_Airport_Ground1` tiles on a painted concrete apron. On it: a Soviet crane, oil tanks, containers, a camo net with crates, a Quonset hut, a pipe stack, barrels, flood lights on the corners, a barrier line with a gap for the road, and a T-34 wreck by the entrance. |
| Ponds | W1 (15200, 2600), W2 (17600, -5700) | Organic shorelines with cracked-mud margins. The water planes use `MI_Water_Shore` with no collision. |
| Dry wash | from W1 north-west to (7300, 19500) | A shallow meandering cracked-mud channel. Road S crosses it. |
| Map edges | east x 24775, north y 25130, south y -24387 (rear part) | The map's own edge blockers only reach z 200, and both ridges rise over them. Three `REAR_Edge_*` copies of `TopBlocker` (z -200..900 east, -200..1500 north/south) close the rear edges. They stay low so aircraft coming in over the edges never touch them. |
| Map extension | x 24900 to 30900 | 136 new landscape components (4 x 34). They rise into low hills, so the camera at the playable edge looks onto ground instead of the end of the landscape. Units stay behind `TopBlocker`. Both RVT volumes were stretched to x 30900. The fog-of-war extent (camera bounds) is unchanged. |

## How it is built (re-runnable)

The tools are in `Source/Tools/IntroMission/rear/`.

- **C++ (editor only).** `URTSShortsLandscapeLibrary` (`Shorts/RTSShortsLandscapeLibrary`) gained these functions:
  - `ExportLandscapeRegion` and `ImportLandscapeRegion`: read or write a rectangle of heights and paint weights as raw files.
  - `AddLandscapeComponents`: the landscape editor's Add Component tool, without a viewport.
  - `GetLandscapeGridInfo`.
- **`rear_terrain_gen.py`.** Plain python with numpy. It reads `Saved/IntroRear/terrain/orig_full_*` (the untouched export of the whole landscape, taken before any edit) and writes:
  - the import rectangle `rear_*` (vertex x 556..825);
  - `rear_full_height.r16`;
  - `Saved/IntroRear/rear_plan.json` (roads, ponds, shorelines, depot, station);
  - `rear_plan.png`.

  It prints how many vertices changed outside the rectangle and inside the keep-outs; both must be 0.
- **`rear_lib.py` and `rear_build.py`.** These are exec'd in a throwaway editor through `rear_editor_server.py` and the `rear_cmd.py` client (command folder `Saved/IntroRear/cmd`).
  - `extend_landscape()` runs once.
  - `import_region('rear', PLAN['region'])` writes the terrain.
  - `build_all()` runs the steps water, roads, rail, station, depot, detail and extras. Every step clears and respawns its own `REAR_<Step>_*` actors, which live in the outliner folder `IntroRear/...`.
  - `clear_prefix('REAR_')` removes everything.
- **`rear_nav_check.py`.** Rebuilds the navmesh. For every agent it finds paths from inside the base to the hub, the station forecourt, the north of the station, the depot and the road end, and across each pond. It samples every path every metre and fails if any sample lies inside a pond.
- **`rear_ingame_shots.py`.** Plays in the editor with the fog-of-war post process off, moves the camera pawn to each spot and takes HighResShots into `Saved/IntroRear/shots/ig_*`.

**Undo.** The original map is kept at `Saved/IntroRear/Ger_IntroMission_before_rear.umap`. The full pre-edit landscape export is in `Saved/IntroRear/terrain/orig_full_*`.

## Checks (2026-11-16)

- **Terrain round-trip.** Re-exporting matches the generator exactly. No height or paint vertex west of x 3999 changed. The new components have collision, and their paint weights sum to 255.
- **Navigation.** `rear_nav_check.py` reports `NAV FAILS 0` for all four agents, with these checks:
  - Every destination is reachable from inside the base and from outside the west walls on the player's side. The paths from inside the base cross the rear wall's player-only breach areas, so they prove the rear is connected, and the outside-start paths prove units can reach it.
  - Five crossings of the rear map edges, including over both ridges, find no path.
  - The pond banks drop at least 2.6 m at 60 degrees under the waterline, so no path enters the water.
- **Stray actors.** There are 727 non-`REAR_` actors, class for class the same as before the work.
- **Edge view.** At the camera's east limit, fully zoomed out, the frame shows only ground, with the fog of war on and off.
- **Mission chain.** `BP_MissionManager.Missions[1]` is still a `SecureBaseMission` with its trigger after the save.
- **Look.** Checked in game from the RTS camera: station, depot, saddle, ponds, wash and road end.
- **Regression.** `secure_line_headless_test.py` passes 28/28 on the saved map: tour, fuel truck and crane, targets, and the glider gate.

## Gotchas

- `SM_Ground_1` (`BP_Airport_Ground1`) has its pivot at the top (+X, +Y) corner. The slab spans -2400..0 in local X and Y and -200..0 in Z. Placed centred and flush, it hid under the landscape and poked out as grey quads.
- `BP_Scorch_Dest_DeadTree` attaches a `RTS_BP_NavBlocker_CharOnly` child, which goes when the tree is deleted, so re-runs leave no orphans.
- Editor-viewport and scene-capture pictures draw water and many decals black, and the existing pond does the same. Judge water and decals in game.
