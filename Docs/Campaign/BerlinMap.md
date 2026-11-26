# Ger_Berlin: the map (no mission scripting yet)

`/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin`, built 2026-11-12 from the v6 layout sketch
(`MapDesigner/Berlin/berlin_01.layout.json`, see [BerlinMission.md](BerlinMission.md)). It is a copy of
`Maps/ToCopy/RTS_ToCopyMap` with a new landscape, ruins and the landmark objects. It holds no units, triggers, waypoints
or objectives.

## Size and axes

- Play area 520 m (X, north-south) by 1000 m (Y, east-west), centred on the origin, as in the sketch. +X is north.
- Landscape 19 x 19 components of 63 quads at 100 cm (1197 m square, centred), as the sketch's `bounds.landscape` gives it.
- Fog of war `map_extent` 50 000 (a square over the long side), nav bounds and the four boundary blockers fit the play
  area, and the two RVT volumes (`RVT_Environment`, `RVT_Height`) cover the landscape.

## Rebuilding

Everything is generated; the scripts are in `Source/Tools/BerlinMap`.

```bash
python Source/Tools/BerlinMap/berlin_terrain_gen.py   # heights, six layer weights, layout json -> Saved/Berlin/terrain
python Source/Tools/BerlinMap/berlin_dress_gen.py     # ruins, trees, wrecks -> Ger_Berlin_dress.json
```

Then, in a throwaway full editor (a commandlet crashes saving a landscape map). `berlin_editor_server.py` keeps one
open and runs python files sent with `berlin_cmd.py`:

```python
import berlin_map_build as bmb
bmb.load_layout()
bmb.step_duplicate(); bmb.step_strip()            # only for a new map file
bmb.step_landscape(); bmb.step_fit_level_actors(); bmb.step_lighting()
bmb.step_poi(); bmb.step_water_and_rails(); bmb.step_dress()
bmb.save_level()                                  # after the navmesh has finished building
```

`step_dress` clears every `BER_` actor first, `step_poi` every `POI_` actor, so both can be rerun on their own.
Textures: `berlin_textures_gen.py` then `berlin_textures_import.run()`. Landscape material: `Docs/Landscape/LandscapeBerlin.md`.

## What differs from the sketch

The landmark objects are far larger than the sketch's pads, so the generator overrides the sketch (the list is at the top
of `berlin_terrain_gen.py`; the sketch file itself is unchanged):

- **Reichstag ruin** (`BP_ReichstagRuin_B2`, 117 x 94 m plus the pit forecourt): centred on (8100, -15500), about 70 m
  west of the sketch, turned with the portico to the south. Only the ruin is placed, not the Soviet base buildings.
- **Brandenburg Gate ruin** (65 x 26 m): at (0, -8300), 14 m west, so it clears the crater rim.
- **Flak towers:** not in the project yet. Flat 60 x 64 m pads and grey 40 x 34 x 27 m stand-in blocks labelled
  `POI_PLACEHOLDER_FlakTower_*` at the sketch positions.
- **North lane:** runs round the north side of the Reichstag, along the south bank of the Spree.
- Forge station, Demeter pod, sabotage target, radio bunker: flat ground and a `POI_Note_*` actor each.

The owner confirmed these positions on 2026-11-12 ("keep moved"). The mission's detection zones and patrol routes near the Reichstag and the Gate have to follow these positions.

## Passability (checked on the built navmesh, `Saved/Berlin/logs/nav_report.json`)

| Barrier | How it blocks |
|---|---|
| Wall cores of the dense ruin fields | 2.6 m rubble mound in the terrain with two lines of standing DBK walls on top |
| Infantry-only gaps (4 m) | open gap plus `BP_NavBlocker_VolumeBlockTanks` (`BER_TankBlock_*`) |
| Small-vehicle gaps (6 m), Moltke bridge, Hbf gate | a row of concrete posts leaves one 3.45 m lane: the light agent (radius 120) fits, medium and heavy do not |
| Sinkholes | 8 m pits with near-vertical sides; the 6 m causeway has posts, the 10 m one is open to all |
| Spree | quay-walled trench 4.8 m deep, water plane at -190; the bridge is a rubble causeway |

All lanes route for all four agents except where the sketch wants a class kept out. Long routes (over about 600 m)
fail in one editor path query because of the search node limit, so the probe tests lane by lane.

## Shader checks on the map (2026-11-12, repeated on the 19 x 19 landscape)

- **RVT:** an unlit test sheet sampling `RVT_Environment` reproduced the glass/ash boundary of the ground under it, and
  one sampling `RVT_Height` showed the crater bowl (`Saved/Berlin/shots/rvt_color.png`, `rvt_height.png`). The landscape
  writes both, as on the DryCastle maps.
- **Tank trails:** `MAP=.../Ger_Berlin run_bench.sh berlin -TankTrailBenchAt=13500,44400` passed; the log names
  `MI_RTS_Landscape_Berlin` with strength 1 and the capture shows ruts.

## Update 2026-11-13: custom POIs, destructible ruin kit, electrical station

- **POIs now real objects:** both flak towers (`Environment/Berlin/FlakTower`, plain Breakthrough concrete import), the
  destructible `BP_BrandenburgGateRuin`, the Forge survey station (concept C, Director of Forge style), the Demeter pod,
  the rail-yard unloading ramp + switch tower (with a siding to the buffer stop; track panels sunk 35 cm) and the Zoo
  radio bunker (at the tower's north-east foot: the sketch point lies inside the tower). `step_poi()` and
  `step_custom_poi()` place them; import scripts are in `Source/Tools/BerlinMap/poi`.
- **Ruins:** the instanced DBK walls are gone. `step_ruins()` spawns 778 `BP_BerlinRuin_*` actors (owner's pick: kit E about
  two thirds of the ruins, kit B the rest; here 334 E and 181 B ruins), each one mesh with the shell-hole component and the
  cheap `M_BerlinRuin` material. Kit docs: `Docs/Environment/BerlinRuinKit.md`, `BerlinRuinKitMaterial.md`; contract and
  concept sheets in `CustomMapItems/Environment/BerlinRuinKit`. Loose rubble and street litter stay instanced (not
  destructible) on `MI_BerlinRuin_Rubble`.
- **Navigation must be rebuilt and the map saved after `step_ruins()`**: the ruins' nav boxes are cut in at build time.
- **Electrical station** (owner request) at (13800, 35000), about 95 m from the start: ruined power-station pieces, pylons,
  fence, and a wreck yard with four `ScavObj_Berlin_*_Parts` tank wrecks (children of the German scavenge wrecks that add
  40-80 vehicle parts), four vehicle-parts piles and two stripped cars.
- **Open:** ruin death hides the mesh and bursts pieces (no collapse/sink graph on `BP_BerlinRuin_Base` yet); AI tanks
  ordered to attack `E1`/`E3` barely engage (attack-ground works); the Hauptbahnhof gate needed a wider bollard row with
  the slab walls.

## Update 2026-11-13 (later): critic rounds and footprint skirts

An adversarial critic judged 12 fixed camera views (`Saved/Berlin/critic/r0..r3`, script `Saved/Berlin/critic_shots.sh`)
three times; its reports are `Saved/Berlin/critic/critique_r1..r3.md`. Grades out of 10 for r0 / r1 / r2: streets
2 / 3.5 / 5, area round the ruins 2 / 4 / 5. Round 3 was the last and was not graded again.

What the rounds changed (all in the two generators):
- **Streets:** facade runs with return walls and a second row behind them, ruins turned to the nearest lane, street
  props as events (pile-ups, pole runs) instead of even spacing, sunken cobble carriageway with a rubble berm. Round 3
  made the berm ragged (2-9 m wide, breaks every 30-60 m, tapered ends, none in the blast glass), painted it darker than
  the carriageway, added wheel-rut decals along the lanes, varied the return walls (6, 9 or 12 m, sometimes one,
  sometimes a broken stub) and put corner tenements (E1) at some lane/side-street corners.
- **Round the ruins:** every kit piece with a footprint stands on a modelled rubble skirt (the owner's pick A
  "Schuttschuerze"; 13 meshes `SM_BerlinFoot_*` in `RuinKit/Footprints`, table `ruinkit/footprint_table.json`, placed by
  `step_feet()` as instanced meshes with no collision and no nav effect, labels `FOOT_*`). Collapsed lots use the
  skirt's collapse heaps. Loose rubble masses lie only against lone facade slabs; they carry cool tints per component
  (custom primitive data), whose visible effect is small: the brick colour comes from the rubble texture. The skirts use
  `MI_BerlinRuin_Rubble_Foot` (darker, thinner dust; with the kit's pale dust they read as light rings from above).
  The tracks decal texture runs across the decal yaw, so ruts use lane heading + 90. The landscape is painted a second time after dressing: ash is cleared and a little wet dirt added where
  ruins stand, and scrub and mud patches appear only near ruins. Ground decals of dirt, debris, puddles and cracks are
  placed only beside ruins; crater sticks have 3-5 craters of clearly different size.
- **Kept on purpose in round 3** (read back from `Saved/Berlin/Ger_Berlin_dress_r2.json`; without that file the generator
  rolls them again): the hero ruins, car pile-ups, street wrecks, pole runs and the decals round them.
- **Open:** the table's `dead` collapse heaps for a ruin that dies in play are not wired (runtime behaviour); the E
  share is about 65 % counting the carried heroes (the generator prints 61 % without them).
- **Not done from the critic's list:** terrain dips under large craters, sunken lots in the south, a terrain mound under
  heroes (the skirt does that job), per-instance skirt tint (four tint buckets per mesh instead).

`berlin_dress_gen.py` now also writes the landscape paint (second pass), so run it instead of the terrain generator
alone. Applying in the shared editor through the bridge crashed it once during an autosave; use the throwaway editor
(`Saved/Berlin/cmd_r3_apply.py`, `cmd_r3_nav.py`) while the shared editor is closed.

## Update 2026-11-13 (evening): heap cover, seated ruins, fallen buildings

- **Rubble heaps are natural cover:** 211 kit heaps and 175 collapse heaps are destructible `BP_BerlinCoverHeap_*`
  actors with prone sockets (see `Docs/Environment/BerlinRubbleCover.md`).
- **Ground seating (`Dress.seat`):** the landscape is levelled to each piece's height under every ruin, skirt and lone
  wall (1080 footprints, blended back over 3.5 m) and everything placed moves with the ground under it. Carriageways,
  wall cores and the hard keep-outs are left alone. This fixed bases floating on slopes and berms.
- **Fallen buildings (`Dress.fallen_building`):** a lone standing wall gets its building's rubble behind it (plot
  skirt, a collapse heap on about half, chunks): 83 walls. The other lone walls already have another ruin's footprint
  on that ground.

## Update 2026-11-13 (night): crushable cover, furniture, furniture debris decals

- **Crushable cover (vertical death):** breastwork, toppled slab, facade panels, wall run and corner L are crushed by
  medium and heavy tanks; heap chunk, cellar lip, masonry block, fallen chimney and doorway by heavy tanks only. The
  rubble heaps stay uncrushable. Setup and tests: `Docs/Environment/BerlinRubbleCover.md`, section "Crushing".
- **Furniture:** 16 pieces `BP_BerlinFurn_<NN>_<Name>` on one atlas and one material, shell holes, crushed by any tank
  with vertical death; infantry walk round them (`Docs/Environment/BerlinFurniture.md`). `Dress.furniture()` places
  182: 113 on collapsed plots (tall pieces only there) and 69 in groups of two to four along lane kerbs, never within
  6.5 m of a cover piece or 15 m of an opening. `step_furniture()` spawns them (labels `FURN_*`). Own random stream.
- **Furniture debris decals:** `M_BerlinFurnitureDebris_Decal` takes colour, normal and roughness from the furniture
  atlas; `T_BerlinFurniture_DebrisMap` (RGBA16, built by `furniture/debris_mask_gen.py` after `debris_uv_coverage.py`)
  says which atlas texel each decal texel shows. 16 variants, one per piece, chosen through the decal colour
  (R = (variant + 0.5) / 16). 377 on the map: beside about 70 % of the furniture, the rest loose on ruin footprints.
  Build: `furniture/debris_decal_build.py` (headless).

## Update 2026-11-13 (late): lighting, crater erosion

- **Lighting:** the sun has its own light function (soft drifting cloud shadows, a pall per district) and the map has
  no engine height fog: `Docs/Lighting/BerlinLighting.md`, `step_sun_light_function()`.
- **Crater erosion (`Dress.erode_craters`):** every crater and puddle decal of 2.5 m or more has a real bowl under it
  (a 4 m crater about 18 cm deep, a 14 m one about 55 cm, puddles 60 % of that, ragged outline, low rim; overlapping
  bowls add up to at most 95 cm), painted mud. 1185 decals, 5.9 % of the play area lowered by 5 cm or more. Skipped
  within 13 m of openings and at wall cores, gaps, sinkholes, the blast crater, landmark pads, the bridge, the quay
  and under ruin footprints. It runs before `seat()`, and everything placed follows the ground.

## Update 2026-11-13 (night): material pass, ambient motion, crater erosion on the map

Applied in the shared editor and saved; nav check 0 problems after the landscape rebuild.

- **Ruin material pass** (`Docs/Environment/BerlinRuinKitMaterial.md`): rubble desaturated (texture and a per-component
  control in custom primitive data 4), wall shader 367 -> 292 pixel-shader instructions. `RUBBLE_TINTS` and `FOOT_TINTS`
  now hold (r, g, b, soot, desaturation); `step_dress` and `step_feet` were rerun with them.
- **Ambient motion** (`Docs/Environment/BerlinAmbientMotion.md`, `step_ambient()`, labels `AMB_*`): 85 street-litter
  stretches, 736 curtains, wires, cables and lamps on 383 ruins, 151 perched crows in 34 groups, 3 circling flocks.
  No collision, no nav effect. Rerun `ambient/ambient_gen.py` and `step_ambient()` after any change of the ruin list
  and after every `step_ruins()` (the dressing is destroyed with the ruins). The crows' take-off reaction is a design
  only (needs C++).
- **Crater erosion** is on the map (see the previous section).

## Update 2026-11-14: ruin death, atmosphere, 32 furniture types, litter fixes

- **Ruin death** (`Docs/Environment/BerlinRuinKit.md`, "Death"): every ruin ends as a lowered, turned, dead remnant;
  dust and debris sized per class; collapse heaps spawn on the larger classes. Class defaults only: no map step.
- **Atmosphere** (`Docs/Lighting/BerlinLighting.md`, `step_atmosphere()`, labels `ATM_*`): 70 smouldering ruins, Mitte
  smoke and haze sheets, crater glow and shimmer cards, five fires in the east. No engine fog.
- **Furniture:** 32 types now (17-32 mapped onto the unchanged atlas). `Dress.furniture()` reads the `tall` flag of
  `furniture/furniture_table.json`; 196 pieces on the map (124 on plots, 72 along kerbs), 413 debris decals. A piece
  numbered above 16 uses the debris variant of (number - 1) mod 16.
- **Street litter:** `NS_BerlinStreetLitter_B` (soft wisps, pause instead of deactivate); `step_ambient()` rerun.
- **Bridge without the MCP tools:** `Source/Tools/BerlinMap/uecp_call.py <script.py> "description"` sends a script to
  the editor's `execute_python` over a raw socket (the tool connection of a long session can drop).

## Update 2026-11-17: horizon skirt

End-of-map planes `SKIRT_Plane_*` (folder MapSkirt, `MI_Berlin_HorizonSkirt`) run from the landscape rim out to 1.5 km, and the landscape's outer 30 m (70 m at the two Spree mouths, where the trench now silts up before the rim) is eased to their height and painted pure rubble: `Docs/Environment/MapHorizonSkirt.md`, section Ger_Berlin; tools `Source/Tools/BerlinMap/skirt`. This was applied to the map as it stood on disk, not through the generators: `step_landscape()` or a re-import of the generator heights wipes the eased rim, so run `berlin_skirt_plan.py` and `berlin_skirt_apply.py` again after one.

## Known gaps

- The track-smoke surface map is not baked, and the Berlin layer names are not in `URTSGroundSurfaceSettings`, so a bake
  would count every layer as dirt. Adding them is a C++ change for the owner to decide (suggested: rubble and mud = dirt,
  ash = rock, scrub = grass, cobble and glass = concrete).
- Alexanderplatz and the Hauptbahnhof shell are generic dressing, not designed set pieces. (The rail yard is designed
  now: see "Rail yard D1" below.)
- No landscape grass output; the Tiergarten is dead trees and logs on the scrub layer.

## Floating gaps under ruins and objects (2026-11-17)

Tools in `Tools/BerlinMap/gaps/` (throwaway DebugGame editor through `gaps_editor_server.py` / `gaps_cmd.py`):
`gaps_dump.py` (landscape heights, every static mesh instance's transform, LOD0 geometry), `gaps_scan.py` (offline
detector and sculpt plan), `gaps_apply.py` (height sub-rectangle import + piece moves), `gaps_shots.py` (pictures).

- Detector: lowest geometry of anything per 50 cm cell against the landscape; a cell floats when that underside is
  3-260 cm above ground, belongs to the base of its piece and lies on the outline of what covers the ground.
- Fixed pieces (ruins, footprint plates, cover heaps, landmarks, power station, rail yard): the landscape is raised to
  the floating edge and blended out over 3 m. It never rises above a footprint floor or another flat fixed piece.
- Loose pieces (rubble instances, furniture, cover, wrecks, trees, yard clutter): lowered onto the ground, or sunk in up
  to 30 cm when one side floats; they ride on any ground that rose under them. Pieces resting on another piece and
  pieces more than 60 cm in the air (the slabs hanging in the sinkholes) are left alone.
- Result on the saved map: visible floating area under fixed pieces 1,330 m2 -> 56 m2; 24,000 vertices raised (1.7 % of
  the map, mean 13 cm), 552 loose pieces moved. Nav check 0 problems. Left: plate `FOOT_StubHeap_3` overhanging the
  sinkhole at (-17450, 1820), about 5 m2 around the Brandenburg Gate (up to 12 cm), the fallen power mast.
- Rerun after placing new pieces: dump, `gaps_scan.py plan <height file> nosink`, apply, rescan.

`BP_Berlin_BreweryVaults` placed at (-11959, 34847), yaw 20, on the hall's -X side (`vaults_clear_plan.py`,
`vaults_apply.py`): 5 footprint instances and 9 rubble instances removed from their instanced actors, 3 loose actors
and 12 decals inside the footprint deleted.

## Start-base resource fields (2026-11-18)

The owner picked concept A "Ostrand" (`Docs/Concepts/BerlinStartResources/v01`, main repo) and asked for it to be built
from the existing resource Blueprints only. `Tools/BerlinMap/poi/start_resources.py` (`check_sites()` read-only,
`place()` reruns cleanly: it clears its own `RES_Start_` prefix, folder `Berlin/StartResources`) placed 14 actors,
seated on the landscape by a pivot trace. Both fields sit on the east edge behind the start (13542, 44444):

- **Radixite, glass crater** round (17300, 47300): `Resource_Radixite_Crater0` (the old bomb crater) ringed by one
  `Crystal_Large`, one `Crystal_LargePoint`, three `Crystal` and two `Crystal_SmallDecal`. No Growth variants (Berlin
  Radixite does not root or regrow). Placed total 4,635.
- **Metal, wrecked tram depot** in X 10700-14800, Y 47000-49100: `Resource_Metal_Ruin1` (depot shed), three
  `Resource_Metal_Rails` (tracks), `Resource_Metal_BrokenWaterTank`, `Resource_Metal_Barrels`. Placed total 8,975. It is
  generic scrap, not trams: there is no tram Blueprint.

Checks: a 1 m trace grid and every instanced mesh found nothing in either footprint before placing; navmesh rebuilt
before the save, every agent paths from the assumed HQ spot (13900, 43300) to both fields (35-64 m). Pictures in
`Saved/Berlin/start_resources/`. The save made the umap 65 MB (it was 122 MB); a commandlet load of both versions found
the same actors, components and 3,487 instances apart from the 14 new actors, and nav covers the play area, so the
drop is re-serialised editor data, not lost content. Pre-edit copy: `_MapBackups/Ger_Berlin_2026-11-17.umap` (same size).

Crater look pass (2026-11-18, `poi/start_crater_fix.py` + `poi/crater_paint.py`): BerlinGlass painted into the
landscape (full within ~9 m of the centre, ragged fade to ~17 m; heights untouched; patch vertices i 745-798, j
1045-1098 through `URTSShortsLandscapeLibrary` Export/ImportLandscapeRegion), the Crater0 rim gets the instance
material `MI_BerlinRuin_Rubble_Foot` (MM_Crater tints any diffuse yellow), the crystals' own decals (dark splats +
crystal-shard sprites) are hidden on the placed instances, and 7 `RES_Start_CraterDecal_*` ash / glass-shard rubble
decals sit on the rim. The crater dip is not done; a dedicated crater design is in concept
(`Docs/Concepts/BerlinStartCrater/v01`).

Start crater dip (2026-11-19, owner's pick: concept v02 option 2 "Breschenwall", look B "Mauerreste",
`Docs/Concepts/BerlinStartCrater/v02`). Built in an own throwaway editor (`poi/crater_editor_server.py` +
`crater_cmd.py`, command folder `Saved/BerlinCrater/cmd`), pre-edit copy `Saved/BerlinCrater/backup/`.
- Landscape: `poi/crater_probe.py` exports vertices i 742-801, j 1040-1091 (`Saved/BerlinCrater/land/`);
  `poi/crater_heights.py` adds the concept delta (`O2R` in the concept tools' `bsc_common.py`) to the real heights:
  floor 1.84 m down (bowl centre 1 m N / 5 m W of the field centre), 0.74 m earth lip, nothing east of Y 49100;
  repaints BerlinGlass on the pan floor and under the crystals, ash on the inner wall, rubble under the rim.
- Rim: `BP_BerlinStartCraterRim` (`Blueprints/Environment/Berlin/StartCrater`, label `CRATER_Rim`, folder
  Berlin/StartCrater) at (17300, 47300, 0): three meshes `SM_BerlinCrater_Rim_East/_South/_West` fitted to the new
  ground (Blender `CustomMapItems/Environment/BerlinStartCrater/bsc_kit.py`; ruin-kit slots, vertex colours, UCX),
  4 sooty brick wall stubs (3.4-5.6 m) on the far rim, low ridge on the camera side. The meshes block shells and sight
  but not nav; 43 hidden `NavArea_Null` boxes (one per heap, one per stub) keep vehicles to the three breaches.
- Re-seat (`poi/start_crater.py`, set_actor_location only): the 8 RES_Start_Radixite_* pieces 95-191 cm down, Crater0's
  MainMesh rim hidden (no collision, no nav), the 7 crater decals and 5 track decals in the breaches moved with the
  ground.
- Nav (`poi/crater_nav_test.py`, `Saved/BerlinCrater/nav_test.json`): all 64 harvest slots reachable from the HQ for
  every agent through the game's slot projection; breach clear widths for HeavyTank W-SW 11.75 m, N 13.75 m,
  S 14.5 m; HQ to the bowl floor 50 m. A PIE run with a real harvester was not done.

## Extra Radixite fields E1, E2, E5, E6 (2026-11-18)

Owner's pick of four from eight candidates (`Docs/Concepts/BerlinExtraRadixite/v01`). `poi/extra_radixite.py`
(`RES_Extra_<site>_*`, folder `Berlin/ExtraRadixite`; `check_sites()`, `place()`, `check_placed()`, `crater_z()` seats
Crater0 at mean rim height, `move_to_plan()` keeps the roll). Stock Radixite Blueprints, no Growth variants, nothing
deleted; same Berlin look as the start crater (rubble rim material, crystal decals hidden, no landscape paint).

| Site | Where | Pieces | Total on the last load |
|---|---|---|---|
| E1 Boxhagener Trichter | (1700, 46300), south-lane start | Crater0, 2 Crystal, 2 SmallDecal | 2,880 |
| E2 Rosenthaler Trichter | (19650, 16850), north lane | Crater0, Large, 2 Crystal, 2 SmallDecal | 4,910 |
| E5 Drehscheibe | (-6050, 20750), south of the rail-yard fence | Crater0, 2 Large, 2 Crystal, 2 NoDecal | 7,245 |
| E6 Kraterrand Sued | (-8000, -2900), blast-crater rim | 3 Large, 2 Crystal, LargePoint, 2 NoDecal | 9,325 |

- **The crystal Blueprints re-roll on every construction-script run** (map load, gizmo or subsystem transform moves):
  mesh, yaw, scale and `total_amount` (Large about 1,750-2,950). Crater0 is fixed at 1,600. The totals above are one roll
  (the start crater read 4,635 when placed and 5,875 on reload).
- Every agent paths from the assumed HQ (13900, 43300) to every site, edge and inside (126-618 m); the E5 fence gap stays
  open; the pieces do not change the navmesh. Pictures: `Saved/Berlin/extra_radixite/`.
- Saved 2026-11-18 21:40 (65.5 MB); verified after a reload: 47 `RES_` actors, rim materials and hidden decals kept,
  glass paint identical to what was written.

## Rail yard D1 (2026-11-22)

The owner's pick D1 "Sperrbahnhof - Kompakt" is built by `Tools/BerlinMap/poi/rail_yard_d1.py`
([BerlinRailYard.md](../Environment/BerlinRailYard.md), "Built: D1"). It adds:

- a 45 x 45 m court of destructible Soviet walls with gates A and C;
- two MG guard towers;
- a strongpoint with dug-in KV-1 / T-34 and two squads;
- scavengeable ramp and switch-tower targets;
- the wooden-sleeper Trains track (`BP_RoadSpline`) along the whole rail_east line.

**The generators now drift from the map:**
- `step_water_and_rails()` would bring back the old `BERW_Rails` panels on rail_east;
- `step_custom_poi()` would place the plain ramp and switch-tower meshes again;
- `berlin_dress_gen.set_pieces()` would put back the yard containers, crane, shed, barrels and fence panel 00.

Exclude the yard before rerunning any of them. Pre-edit copy: `_MapBackups/Ger_Berlin_2026-11-22_before_railyard.umap`.

## Detail pass (2026-11-26)

Owner goal: more detail at low performance cost (landscape blending, decals, more furniture, the Reichstag area much
denser), then five rounds of adversarial critique and fixes. Tools in `Tools/BerlinMap/detail/`:

| Script | Where | What |
|---|---|---|
| `detail_dump.py` (+ `detail_dump_land.py`) | -nullrhi commandlet (`load_map`, no GPU) | every actor, mesh box, instance, decal + the whole landscape (heights, 6 weights) -> `Saved/Berlin/detail/dump/` |
| `detail_analyse.py` | plain python | empty-ground clusters, layer purity, density preview |
| `detail_gen.py` | plain python | the plan: `Saved/Berlin/detail/plan/placements.json` (`detail_gen_r0..r4.py` are the earlier rounds) |
| `detail_paint.py` | plain python | landscape blend -> `Saved/Berlin/detail/paint/new_<layer>.r8` (+ before/after previews) |
| `detail_apply.py` / `detail_apply_paint.py` | throwaway editor (`berlin_editor_server.py`) | spawn the plan (clears only `DET_`), import the paint (ImportLandscapeRegion, heights untouched, read back) |
| `detail_shots.py`, `detail_route_check.py` | throwaway editor | 27 views (`Saved/Berlin/detail/shots/r0..r5wide`, `*close`), patrol / lane path legs per agent |

Everything added carries the `DET_` prefix (folders `Berlin/Detail/...`): 1,115 decals (`DET_D_<group>`), 370 Blueprints
(`DET_B_`: about 225 crushable furniture pieces in 26 dense facade scenes plus loose groups, hedgehogs with a dark material
override, sandbag posts, camp crates and drums, flags, burnt 1940s cars with a rust paint, unlit poles, dead trees) and
476 instances in 48 HISM actors (`DET_I_<mesh>`: sandstone heaps, concrete and brick rubble, burnt car / truck wrecks,
tents with rack, table and board, a pallet depot, plank fans, sandbag crescents, debris). Small instances cull at 130-160 m.

- Landscape: 19.6 % of the vertices re-blended (soot and mud patches at 6 / 18 / 30-60 m, feathered soot round the ruins,
  old cobble on the squares and the side-street grid in empty fields, frayed street edges, mud on the Spree bank, the
  Tiergarten mottled, the glass edge broken up, dark patches on the Reichstag apron). Rim, river, rails and pads untouched.
- Reichstag: wall-hugging spill decals, sandstone heaps in piles with dark contact decals, courtyard spill decals, a pallet
  depot and stores, a south-court hull, 4 guard posts (L sandbags, signs, pole, wire chicane), hedgehog lines flanking them,
  front-apron MG nests and water craters, track chain round the patrol ring (bends round the gun pits), camp tents with
  crates, drums and flags, a wrecked motor pool east, soot and spills at the big north-east heap.
- Blast crater: radial soot streaks through and round the glass pan, scrap / glass fields, tilted burnt wrecks and convoy
  trucks thrown outward, soot ring, glass shards, half-sunk concrete chunks.
- Changes to older content: the dress generator's 41 post-apocalyptic car instances (`BER_ISM_SM_PostApo_Car_1..5`) now
  use the burnt `SM_OldCar_*_Wreck` meshes (undo data: `Saved/Berlin/detail/postapo_cars_undo.json`); nine pale
  `RUBDECAL_` decals at the Reichstag front were deleted (`detail_apply.DELETED_RUBDECALS`).
- Cost: actors 8,492 -> 10,028 (mostly decals, fade screen size 0.004); about 470k instance triangles map-wide at LOD1,
  no mesh type above 40k; decals +18 %. Saved 01:53 (70.9 MB). Nav check 0 problems; every agent paths every Reichstag
  ring / gate leg except two legs to (15124, -20700) for tanks: that ring point lies off the tank navmesh by the quay and
  failed before this pass too.
- Pre-map backup: `_MapBackups/Ger_Berlin_2026-11-26_before_detail.umap`.
- Not touched, flagged by the critics: the original sawtooth `DECAL_rut` tracks, white card quads north of Potsdamer
  Platz and the pan, two white generator boxes west of the Gate, pink tiles on the Reichstag tower floors, the original
  hedgehogs' pink atlas, the brewery's mint panels.

## Building room round E2 (2026-11-26)

Owner request: clear the area round the E2 "Rosenthaler Trichter" Radixite field (19650, 16850, north lane), so a
player can place buildings there when expanding. `Tools/BerlinMap/poi/e2_clear.py` (`plan`, `apply`, `check`) ran in
the open editor as one undoable transaction.

- **Removed within 28 m of the field centre:**
  - 16 actors: 6 ruin shells (B1, B2, B6, E6), 4 cover heaps, 2 masonry covers, 3 furniture pieces and 1 smoulder
    wisp;
  - 25 instances: loose rubble, props and floor tiles, and 3 footprint plates.

  All of them are logged with transforms in `Saved/BerlinE2Clear/removed_20261126_133302.json`.
- **Kept:** the field (`RES_Extra_E2_*`), its crater rim `FCRATER_E2_Rim`, decals and ambient litter.
- **Result:** a free ring about 16 m wide round the crystals. On a 2 m grid only the field pieces and the edges of
  two ruins just past 28 m remain. Ground slope: median 0.4 %, 95th percentile 8.2 %; heights within -61 to +37 cm
  (old footprint plinths and the crater lip).
- **Pictures:** `Saved/BerlinE2Clear/e2_cleared_top.png`.
- **Backup:** `_MapBackups/Ger_Berlin_2026-11-26_before_e2_clear.umap`.
- **Generator drift:** `berlin_dress_gen` and `step_ruins` would put the pieces back.

## South POIs: flak position and Ju 52 crash (2026-11-26)

Owner request: the south-central ruin field (between the mud-crater chain at Y -9000 and the Demeter pod) had
"not a lot going for it to invite the player". Two exploration POIs, built by `Tools/BerlinMap/poi/south_pois.py`
(`plan`, `apply`, `check`; labels `SPOI_`, outliner `Mission/POI_FlakPosition` and `Mission/POI_Ju52Crash`; `apply`
removes earlier `SPOI_` actors first). Run in a throwaway editor (`berlin_editor_server.py`); map saved 15:03.

- **Flak position** at (-12500, -3000):
  - cleared within 22 m: 1 facade ruin (B1), 2 heaps, 2 furniture pieces, 1 smoulder wisp, 19 rubble instances (log
    `Saved/BerlinSouthPOIs/apply_20261126_145826.json`);
  - two crewless **Flak 88** team weapons `SPOI_Flak88_A/B` (owner pick B, no 20 mm team weapon exists), owner 1,
    `bStartAbandoned` with `StartAbandonedCrewClass = Ger_SquadTW_Flak88`: any player infantry squad with enough men
    re-crews one (`IdManAbandonedTeamWeapon`). Packed pose, pointing north at the Forge crater;
  - a loose sandbag arc in front of each gun and two half-size ammo boxes behind it;
  - three scavengeable wrecks (vehicle parts, the power-station children): `ScavObj_Berlin_PZIV_Parts`, `_Stug_Parts`,
    `_Pz38T_Parts`.
- **Ju 52 crash** in the oval mud crater at (-16800, 3900), an 8 m deep bowl:
  - the DC-3 crash mesh stands in for the Ju 52 (owner pick; no Ju 52 model exists), lying along the crater, plus six
    `SM_RTS_PlaneWreck_*` parts on the rim, seated on the lowest ground under their footprint;
  - **the crater floor is a navmesh island** (no unit reaches it; it was before the wreck too), so the cargo lies on
    the west rim: scavengeable `ScavObj_VehicleParts1-3` (metal + vehicle parts) and a crate stack;
  - the survivors (one `Squad_Ger_JagerTruppKar98k`) come from stage `Mission_Berlin_Ju52Crash` (BerlinMissionScripting.md).
  - Looting makes no noise (owner pick): no patrol is near.
- **Checked:** every gun, wreck and crate paths from the flak side for infantry and medium tanks. A short Simulate run
  turned both guns abandoned; `FTeamWeaponDecalManager` then reports "M_DecalMaterial not set": no team weapon in the
  game has an abandoned-marker decal material (open).
- **Pictures:** `Saved/Berlin/detail/shots/poi_south/` (`close_flak`, `close_ju52`, `wide_both`).
