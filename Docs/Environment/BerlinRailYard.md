# Ger_Berlin rail yard (south lane): redesign options

Status: **D1 "Sperrbahnhof - Kompakt" picked and BUILT on Ger_Berlin, 2026-11-22** (section "Built: D1" right
below). The concept rounds v01 (six versions) and v02 (three D versions) follow it.

- Sheet: `Docs/Concepts/BerlinRailYard/v01/BerlinRailYard_v01_sheet.png` (main repo; `_sheet_small.jpg` is half
  size). It shows all six versions, each with a top-down plan and a game-camera view.
- Detail pages: `BerlinRailYard_v01_A.png` to `_F.png` in the same folder. Each has a full-size plan with every label
  and the full text.
- Scripts: `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinRailYard/Concepts/`.
  - `bry_versions.py` holds all six layouts as data.
  - `bry_prep.py` prepares the map data and runs the detection-zone checks into `work/check.txt`.
  - `bry_blender.py` renders the scenes (Blender 5, headless).
  - `bry_sheet.py` composes the sheet.
  - The scripts reuse the extra-Radixite and start-resource concept tools read-only.

Background: [BerlinMissionFlow.md](../Campaign/BerlinMissionFlow.md) section 5.3, [BerlinMission.md](../Campaign/BerlinMission.md)
"South lane: the rail yard", and [BerlinMap.md](../Campaign/BerlinMap.md).

## Built: D1 "Sperrbahnhof - Kompakt" (2026-11-22)

The owner picked D1 from the v02 sheet. It is built as a **set piece only**. The mission logic is **not** built: no
bonus plate, no sabotage delay, no response train, no detection-zone trigger. The flow doc 5.3 still owns those.

- Script: `Source/Tools/BerlinMap/poi/rail_yard_d1.py`, run in a throwaway editor (`ry_editor_server.py` +
  `ry_cmd.py`, command folder `Saved/BerlinRailYard/cmd`). Functions:
  - `build_target_bps()`
  - `place()`
  - `nav_report()`
  - `pie_check()`
  - `capture_shots()`
  - `save_when_nav_ready()`
- Read-only probe of the asset sizes and the yard: `rail_yard_probe.py` (commandlet, `Saved/BerlinRailYard/probe.json`).
- Pre-edit copy of the map: `_MapBackups/Ger_Berlin_2026-11-22_before_railyard.umap` (byte-identical to the 16:27
  save that holds the forge station).
- Pictures (editor-world captures, no units): `Docs/Concepts/BerlinRailYard/build/railyard_d1_*.png`.

### Deviations from the v02 sheet

All of them come from the real module sizes.

| Sheet | Built | Why |
|---|---|---|
| Court 45 x 56 m | **45.2 x 45.2 m** = 2 x 2 `Rus_Wall_Concrete_Long` (22.6 m, 5 m high), X -4862..-342, Y 12100..16620 | Whole modules only. The `_Corner` piece is an L (11.4 + 11.6 m), and overlapping plates would z-fight. |
| Gate A beside the track, gate C on it | Both on the east wall: `BP_RusWallGateA` X -342..-1292 (opening 6.9 m), `BP_RusWallGateC` X -1292..-2392 on the track (opening about 10.3 m) | The frames are 9.5 m and 11 m wide. |
| Drain culvert under the south wall | A **2.1 m broken gap** at the south-east corner (between the east wall's long module and the south wall) | Infantry pass; a light tank needs 2.4 m. `BP_Airport_Dest_DrainPipeStack` lies outside as dressing (no nav effect). |
| Court flush with the yard | The court sits 1.6 m south of the old fence line; `BER_YardFence_00` removed | The south lane runs through the court's footprint. Traffic now goes round the south side (path checks below). |
| One siding | No siding inside the court | The court is 45 m; the siding would hit gate C's tower. |

### What is on the map (labels `RY_`, folder `Berlin/RailYard`)

- **Track:** `RY_Rail_Main` (`BP_RoadSpline`, `RoadMesh = SM_RustedRefinery_RailRoad`, no `RoadMaterial`, the same as
  Ger_Trains' main lines `TrainTrack_0..2`; that map's base siding uses `M_YellowRail` instead; collision and decals
  off).
  - It is 405 m long: along the whole rail_east line, through gate C, to the ramp's buffer stop.
  - **Even spacing, at the owner's request:** 14 points, 13 segments of 30.8-31.2 m over the whole line.
    `ARoadSplineActor` stretches one 25.5 m panel per segment, so this gives the Ger_Trains look (its tracks use
    30-45 m segments) and no short, vertex-heavy segments.
  - Height: sunk 85 cm (sleeper tops about 10 cm above the ground, rail tops about 28 cm).
    - Each point's height and vertical tangent are fitted to the ground over its two neighbouring segments
      (`lay_track_even_tangents`, sinking capped at 8 cm).
    - 95 % of the line sits within 15 cm of the target.
    - Over the 62 cm step down onto the yard pad (just east of gate C) and a 44 cm dip at about (-4975, 36344), the
      track stands up to 52 cm / 39 cm proud. The mesh's stringers and cross pieces reach 1.2 m under the sleeper
      tops, so it still reads as carried there (pictures `railyard_d1_track_step.png`, `_dip.png`).
  - History: the first build put points 6 m apart, which squeezed each panel into 6 m (0.14 m sleeper pitch). A
    terrain-split version followed with uneven 13-27 m segments. Both were replaced by the even layout.
  - The 40 `BERW_Rails` panels of that line are removed; the other scenery lines keep the old track.
  - `TRACK_EXTENT = "rail_east"` is the default; `"yard"` would replace only the last 75 m. Which one to keep is the
    owner's call.
- **Targets:**
  - `RY_Target_Ramp` = `BP_Berlin_RailRamp_Sabotage`, at the old ramp transform (-2102, 13389, yaw 180).
  - `RY_Target_SwitchTower` = `BP_Berlin_SwitchTower_Sabotage`, at (-2900, 23300, yaw 180) on the east throat.
  - Both Blueprints are new children of `BP_ScavengeableObject` in `Environment/Berlin/RailYardTarget`, with the yard
    meshes. Scavenge time 40 s / 30 s, no resource reward, health 2400 / 1800. Scavenging or destroying either one
    counts (mission tracking of scavengeable objects).
- **Walls:** `RY_Wall_00..08`: 7 long modules and the two gates, all owned by player 2.
- **Guard towers:** `RY_Tower_NW` (-800, 12750) and `RY_Tower_SE` (-4300, 16000), `Rus_BXP_GuardTowerMG_Soviet`,
  owner 2.
- **Strongpoint:** `RY_Strongpoint` (id `RailYard`), posts:
  - `WestKV`: dug-in KV-1 facing the west wall;
  - `GateT34`: dug-in T-34/76 facing the gates;
  - `CourtSquad` (-3200, 14000) and `GateSquad` (-300, 19500): Mosin squads, both more than 25 m from the gates and
    with `TakeNearbyCover` off. A Soviet unit within a gate's `OpenRadius` (14 m) holds that gate open; the first
    posts did exactly that.

  The units are set on the posts (`SetPostUnitOverrides` from a future mission can replace them); it spawns on
  BeginPlay and the release is Never.
- **Removed** (logged with transforms in `Saved/BerlinRailYard/removed_<time>.json`):
  - `POI_RailYard_UnloadingRamp` and `POI_RailYard_SwitchTower` (the plain meshes);
  - `BER_YardContainer_*` (8), `BER_YardCrane`, `BER_YardShed`, `BER_YardBarrels_*` (3);
  - `BER_YardFence_00`.

  The other 12 yard fence panels stay.

### Checks

Run in the throwaway editor before the save. Results: `Saved/BerlinRailYard/nav_report.json` and `pie_check.json`.

- **Editor world:**
  - All four agents path from the HQ to the lane at the yard (252 m), on along the lane to its south-west exit
    (145-148 m), across the E5 gap, and from the lane to the switch tower and to the outside of the west wall.
  - 12 of 12 scavenge spots round each target are on the infantry navmesh.
- **Play in Editor, 150 s, using the game's planner filter for queries without a unit
  (`URTSClosedBreachableWallsQueryFilter`).** The walls build their nav at BeginPlay (`RuntimeGeneration =
  DynamicModifiersOnly`), so only PIE can test them. Ger_Berlin starts **paused** in PIE (game time stays at 0)
  behind the "Start game" widget (`UPlayerStartGameControl::BeginPlay_InitStartGameFlow`, every map with that
  widget, until `PlayerStartedGame()`); `pie_check()` unpauses it.
  - Leaving the court through the west wall detours 116-133 m after 25 s (a first, too-short PIE run still saw a
    straight 17 m: the walls' nav was not built yet).
  - From outside the south-east gap to the ramp: infantry 38 m, vehicles 55-57 m (the long way).
  - Lane, E5 gap and switch-tower routes are unchanged.
  - The garrison spawned on its posts: 2 Mosin squads (12 soldiers), a KV-1 and a T-34/76.
  - **Both tanks dug in:** `BP_DiginWall_RusHeavy` round the KV-1 and `BP_DiginWall_GerLight` round the T-34. The T-34's
    own Blueprint picks that dig-in wall.
  - **Both gates stayed CLOSED** with the garrison in place (`GetGateState`). Through the east wall, the planner filter
    still finds a 30 m path through gate C: a closed gate counts as passable without a unit.
  - **Both towers were at full health (600/600),** so they spawn built. Their MG is the tower's own hull weapon; its
    firing was not tested.
  - **Both targets** are scavengeable objects with their scavenge times (40 s / 30 s). A scavenger squad doing the
    sabotage was **not** run.
  - Not tested here: what a closed gate means for a player unit's own per-side filter (the gates open only for
    Soviets, as `WallGates.md` describes).
- **The saved file** (`rail_yard_verify.py` in a commandlet, `Saved/BerlinRailYard/verify.json`):
  - `RY_` 16, `FORGE_` 16, `RES_` 47; 25 `BERW_Rails` panels left on the scenery lines.
  - Walls, gates and towers owned by player 2.
  - The Forge mission stage `Mission_Berlin_ForgeStation` kept in `BP_MissionManager`.
  - 65.8 MB.
  - Saves: 17:40 (build), the forge session's 17:48 (its mission stage, my build kept), 18:31 (posts + track), and
    18:37 (even track). The scripting session saved at 18:42 with the four new mission stages; it kept all 16 `RY_`
    actors (checked after a reload). Start any later edit from that save.
- **Not run:** the floating-gaps detector (`Tools/BerlinMap/gaps`) over the new pieces, and a lit in-game look check.

### Open

- The mission side (BerlinMissionFlow 5.3): bonus plate, sabotage delay, response train (`BP_SovietCargoTrain` on
  `RY_Rail_Main`, stopping at the ramp), and the gate squad's role in the alarm.
- The editor-world captures are dark (no game exposure); a lit in-game look check is still to do.
- Two existing puddle decals lie across the track just east of gate C (`DECAL_puddle_3531` and its neighbour).
- The generators would undo this (see BerlinMap.md, "Rail yard D1"); do not rerun `step_water_and_rails`,
  `step_custom_poi` or `set_pieces` without excluding the yard.

## Round v02: three improved versions of D "Sperrbahnhof" (2026-11-22)

The owner asked for three improved versions of D:

- the wall must be the destructible Soviet concrete wall;
- add guard towers;
- add dug-in tanks.

Sheet: `Docs/Concepts/BerlinRailYard/v02/BerlinRailYard_v02_sheet.png` (plus `_sheet_small.jpg` and detail pages
`_D1.png` to `_D3.png`). Data: `bry_versions_d.py`; sheet: `bry_sheet_v02.py`. The renders and checks use the same
tools as v01.

### Pieces used (all exist)

| Piece | Asset | Notes |
|---|---|---|
| Wall | `Rus_Wall_Concrete_Long` (22.6 m module), `_Corner` (11.3 m), `_LongSandBags`, `_Long_Holes` (pre-holed), `_Soviet` / flag variants | `AWallActor`: shell holes on a field texture. Player medium and heavy tanks breach it; Soviet units never damage it, and light tanks go round. Docs: `WallChunkDamage.md`, "Field holes and tank breaches". |
| Gates | `BP_RusWallGateA` (twin swing leaves, about 9 m), `BP_RusWallGateC` (lift slab between 11 m towers, about 11 m; used on the track) | `AWallGate`: opens by itself for Soviet units, closed to the player, can be shot down (health A 1400, C 2200). Doc: `WallGates.md`. |
| Guard towers | `Rus_BXP_GuardTowerMG_Soviet` (5.5 x 6.6 x 10.7 m lattice + cabin), `Rus_BXP_LargeGuardTower` (7.7 x 12.7 x 15.7 m: plinth, steel frame, cabin), `Rus_BXP_GuardTowerBT-7` (about 7 x 7 m base with a BT-7 turret) | Sizes from the source FBX. The BT-7 tower's size is taken from the guard-tower base. |
| Dug-in tanks | `AEnemyStrongpoint` posts with `bDigIn` (Soviet T-34 / KV-1), release Never | Dug-in tanks are anchored (`tank-dig-in` work): they cannot be shoved and never leave the pit to chase. |

The gate opening widths are read from the concept sheets, not measured; confirm them in the editor after the pick.

### D1 Sperrbahnhof - Kompakt (v01 D refined)

- **Layout:** a 45 x 56 m court round the ramp (2 x 2.5 wall modules). Trains enter through gate C on the track;
  tanks leave through gate A beside it.
- **Guard towers:** MG towers in two opposite corners.
- **Dug-in tanks:** a T-34 behind gate A and a KV-1 covering the west wall.
- **Garrison:** 2 squads.
- **Switch tower:** outside at the east throat, as in A.
- **Ways in:**
  - Quiet: infantry through the E5 gap and a drain culvert under the south wall.
  - Loud: medium and heavy tanks breach the west wall from the south lane.
  - Gate A stands in `dz_yard`, so a light raid on it is loud.
- **Cost:** the smallest build of the three; only the culvert nav gap is new.

### D2 Sperrbahnhof - Zwei Höfe (two courts)

- **Layout:** Soviet walls replace the yard fence on the south, and a cross wall splits the yard into two courts.
  - East court: the throat, the garrison camp, the switch tower, and a dug-in KV-1 at the east gate C.
  - West court: the ramp, with two dug-in T-34s on the west wall.
- **Guard towers:** the large guard tower stands at the cross gate and overlooks both courts; MG towers stand on the
  west corners.
- **Quiet ways in,** both from the E5 side:
  - a holed wall section (`_Long_Holes`) into the east court, right beside the switch tower;
  - a culvert into the west court.
- **Loud ways in:**
  - Medium tanks breach the east court's south wall.
  - Heavy tanks breach the west wall.
  - Light vehicles have to shoot down gate A. It stands outside the DZ, so this is quiet as far as the DZ goes.
- **Cost:** the biggest garrison and the most wall to build. The north half of both courts lies in `dz_yard`.

### D3 Festungsgleis (walled rail corridor)

- **Layout:** the unloading siding runs in a 20 m walled corridor along the south side of the yard; the walls
  replace the fence. The corridor's east end is all gates: C for the train, A for the tanks.
- **Guard towers:** a BT-7 tower and the large guard tower on the north wall, and an MG tower at the SW gate.
- **Dug-in tanks:** outside the walls. Two T-34s stand on the E5 side, which makes the E5 harvest a fight. A KV-1
  covers the west end, where the switch tower stands 60 m west of the yard pad.
- **Ways in:**
  - Quiet: infantry through a holed wall section beside the ramp, or straight to the switch tower from the lane exit.
  - Light vehicles reach the switch tower from the central lane.
  - Loud: medium and heavy tanks breach the corridor's south wall or its west end wall, each under a dug-in gun.
- **Cost:** no new assets; the hardest of the three for vehicles.

### Checks (`work/check.txt`, as in v01)

- In all three, both targets and their scavenge rings sit outside `dz_yard`.
- No drawn route is blocked by a wall, gate pillar, guard tower, dug-in pit, wagon or the switch tower. Soviet gates
  count as open for the response force and as closed for the player.
- The only loud player route is D1's light raid on gate A, which stands inside the DZ.

## What the yard has to do

1. **Bonus "Sabotage the rail yard".** Destroy the **switch tower** and the **unloading ramp** before the alarm. This
   delays the rail part of the response force. Scavengers must be able to do it.
2. **After the alarm**, the rail reinforcements unload here and head east toward the player's base.
3. **A small staging garrison** guards the yard. The yard must be reachable before the alarm.
4. The **E5 Radixite field** lies just south of the yard fence, and its **fence gap stays open**.
5. The track becomes the **wooden-sleeper track of the Trains mission**.

How the six versions meet this:

- The two targets are the **existing meshes**, `SM_Berlin_RailRamp` (end and side loading ramp) and
  `SM_Berlin_SwitchTower`, in every version.
- Each target sits **outside `dz_yard`** (centre (542, 19444), radius 40 m), and so does the ring of scavenge spots
  round it (half its size plus 1.5 m). At least one infantry approach to each target also stays outside the zone.
  `bry_prep.py` checks this and writes the result to `work/check.txt`.
- The same script checks **every drawn route**, per unit class and widened by that class's nav agent radius, against
  the version's own pieces (tower, ramp, platform, walls, wagons, crater, roundhouse), the fence panels between the
  gaps, and F's bank. No route is blocked. Each player route is marked **quiet** (stays outside the zone) or **loud**
  on the sheet. The loud ones are deliberate: D's light raid through the wired gate, and F's heavy route onto the
  island.
- E's targets also sit outside today's zone; its smaller zone is optional (see E).

## Facts found (2026-11-22)

### The Trains-mission track

- `Ger_Trains.umap` references **neither** candidate from the brief. `SM_RTS_Railway_Track_01` (the mesh Berlin uses
  today) and the LowPolyForest `SM_Railway_Track_01` do not appear among its 324 asset imports.
- The track mesh it does reference is `/Game/RTS_Survival/Meshes/Buildings/Industrial/RailRoad/SM_RustedRefinery_RailRoad`.
  The map also references `M_YellowRail`, a material with `Wood026` normal and roughness maps, which gives the
  wooden sleepers. The map's track objects are `BP_RoadSpline` actors named `TrainTrack_0` to `_2` (`ARoadSplineActor`:
  a spline of `RoadMesh` with an optional `RoadMaterial`). The trains ride them through `FTrainMissionRoute::TrackSpline`.
- **Inferred, not seen in an editor:** that the `TrainTrack_*` actors carry `SM_RustedRefinery_RailRoad` and
  `M_YellowRail`. Confirm it in a throwaway editor after the pick.
- **Scripted precedent:** the Intro rear station (`Source/Tools/IntroMission/rear/rear_build.py`, `_rail()`) builds
  `BP_RoadSpline` with `RoadMesh = SM_RustedRefinery_RailRoad`, `bUseCollision = False` and `bAllowDecals = False`.
  It puts the existing `SM_Berlin_RailRamp` (twice) and `SM_Berlin_SwitchTower` next to that track, and
  `ASovietCargoTrain` unloads on it.
- **Mesh size**, measured from the source FBX
  (`AA_LaptopWork/RustedRefinery/RailRoadBridge/SM_RustedRefinery_RailRoad.fbx`), not from the uasset:

  | Part | Size |
  |---|---|
  | Rails | 25.1 m long, 1.5 m gauge (rail centres at ±0.75 m) |
  | Sleepers | 0.15 x 4.2 m, every 0.6 m |
  | Beams under the sleepers | 2, plus cross pieces |
  | Height above the pivot | sleeper tops 0.95 m, rail tops 1.13 m |

  So the track has to be sunk about 0.85-0.9 m for the sleeper tops to sit at ground level and the rail tops about
  0.24 m above it, which is what the ramp deck assumes. The Intro rear station sets its spline points 2 cm above the
  ground; check how high its sleepers really stand before copying that. Confirm the uasset bounds too.
- The track has **no ballast and no turnout piece**. A junction is two splines that overlap, as at the Intro station.
  The 4.2 m sleepers need **5 m between parallel track centres**.

### What is on the map today

- **Track:** `step_water_and_rails()` in `berlin_map_build.py` lays the `BERW_Rails` instances of
  `SM_RTS_Railway_Track_01`, sunk 35 cm. It covers the rail_east line, the yard spur (`YARD_SPUR`, X -1842,
  Y 18056 to 12700) and three scenery lines (Anhalter, Potsdamer, Lehrter).
- **Targets:** `step_custom_poi()` places the ramp (pivot (-2102, 13389), yaw 180) and the tower (-3602, 14889) as plain
  StaticMeshActors.
- **Generic dressing:** `berlin_dress_gen.py` `set_pieces()` places the `BER_Yard*` containers, crane, shed, barrels
  and fence panels.
- The ramp was built for the old track: rail centres 1.85 m apart, rail top 24 cm. The new 1.5 m gauge keeps the same
  centreline. The 4.2 m sleepers reach 2.1 m from it, while the platform face is 1.9 m from it, so 20 cm of each
  sleeper ends under the deck edge. That is fine as long as the track is sunk.

### Scavenger sabotage

- `AScavengeableObject` is a child of `ADestructableEnvActor`. When a scavenge finishes, it broadcasts
  `OnScavengedOrDestroyed` and calls `TriggerDestroyActor(ERTSDeathType::Scavenging)`, so the object dies.
- The mission tracker (`UMissionBase::Tracking_RegisterScavengableCallbacks`) counts "scavenged **or** destroyed" as
  done. Infantry scavenging is therefore the quiet sabotage, and shooting the target with guns is the loud one.
- For a sabotage target: set `bGivePlayerResourceRewards = false` and choose a `ScavengeTime` per target. The squad is
  consumed only if `bConsumeSquadWhenFinished` is set.
- `BP_Hint_UseScavengers` shows the text "Use Scavenger Squads to investigate". The brief calls it the factory missions'
  hint, but that does not match the files:
  - It is stored in `Hints/DestroyFactories`, but the factory hints `BP_Hint_DestroyMediumFactory` and
    `_PlatformFactory` do not reference it.
  - The only asset that uses it is `BP_Mission1_Nuclear` (ClearRoadMission1). That mission puts the hint over
    `BP_ScavObj_*` objects, airdrops `Squad_Ger_Scavengers` (`BP_GA_Airdrop_3xScavs`), and removes the hint when its
    object is scavenged (`FindHintForScavengedObj_AndDestroy`).

  That pattern fits the yard: a hint over each target, and the bonus plate tracking both.

### Trains and units

- `ASovietCargoTrain` (doc `Source/Docs/Missions/SovietCargoTrain.md`) drives the Intro counterattack trains:
  1. The train rolls in along `PathPoints` and stops at `StopFrontLocation`.
  2. The mission hides the cargo tanks (`SM_CargoTank_T3476` / `_KV1E` / `_IS2`) and spawns real tanks next to the
     platform.
  3. The train backs out.

  The locomotive is always at the front, so an end ramp is drawn here as unloading over the side platform.
- Car lengths from the FBX: `SM_SovietTrain_Locomotive` 19.5 m, `_CargoCar` 20.4 m, `_FuelCar` 13.1 m. Destroyed
  variants of all three exist. A train of locomotive, 3 flatcars and a goods wagon is about 95 m long, and the yard is
  111 m. The comment in `berlin_dress_gen.py` that says "wagons are not in the project" is out of date.
- Nav agent radii (`DefaultEngine.ini`): infantry 35 cm, light 120, medium 160, heavy 200. The clear widths used on the
  sheet are 2 r + 40 cm: 1.1, 2.8, 3.6 and 4.4 m.
- `dz_yard` covers the north-east two-thirds of the yard. Outside it are the strip along the south fence (X below
  -3458) and the west end (Y below about 15500).

## The six versions

| | Name | Idea | Ramp / tower | Garrison | New assets | Cost |
|---|---|---|---|---|---|---|
| A | **Kopframpe** | Three stub tracks; the targets at opposite ends of the yard: two small raids | Ramp stays where it is now (west); tower moves to the east throat | squad at a guard hut, AT gun at the east gate, squad at the ramp | none | lowest |
| B | **Militärrampe** | Through line with a 70 m side platform; the line runs on west to a bomb-crater cut; the back door is the dead line | Ramp = the platform's west head; tower 60 m west of the yard on the dead line | AT gun + squad on the platform facing east, squad on the north side | platform segment (or BF slabs), crater | medium |
| C | **Lokschuppen** | E5 is the glazed turntable pit; a half-ring roundhouse ruin round it; the sabotage and the E5 harvest share one approach | Ramp on a south siding; tower at the turntable lead | squad north, MG nest on the roundhouse (also covers E5), sentries | roundhouse front (or ruin kit), clears a little outside the pad | medium-high |
| D | **Sperrbahnhof** | Shield-walled compound over the ramp; quiet way in = a drain culvert (infantry); loud way = medium/heavy breach the west wall | Ramp inside the walls; tower outside at the east throat | MG bunker at the gate, squad inside, squad at the gate | culvert nav gap | medium |
| E | **Wagenburg** | Burnt wagons on 4 sidings form a maze: infantry slip between, light vehicles use 2 cross lanes, medium/heavy only the yard roads | Ramp where it is now; tower against the south fence | camp on the north road, squad in the maze, sentries at the ramp | none (optional DZ change) | low-medium |
| F | **Wendeschleife** | Teardrop turning loop on a 1 m bank; tower on the island; vehicles cross the bank only at a narrow crossing (3.2 m, light only, quiet side) and a wide one (10 m, inside the DZ) | Ramp on the ground-level stub west of the loop; tower inside the loop | squad + AT gun on the island, sentries at the ramp | landscape bank, crossing decks | highest |

Per version, routes are drawn for infantry/scavengers, light, medium and heavy vehicles, plus the response force's
way out east (see the sheet).

### A Kopframpe (stub yard)

- **Layout:** the main line fans into three stub tracks at the east throat, each ending at a buffer stop. Two burnt
  wagons stand on the short tracks.
- **Targets:** the end and side ramp stays at its current spot on the main track. The tower moves 84 m east to the
  throat (-2900, 23300), south of the tracks, where it sits 12 m outside the DZ edge.
- **Garrison:** a squad and a guard hut in the yard centre (inside the DZ), an AT gun covering the east gate, a squad
  by the ramp slope (outside the DZ, so killing it quietly is possible).
- **Routes:** a 6 m yard road runs along the south fence at X -4000. All of it is outside the DZ, even for a heavy
  hull.
  - Medium tanks come along that road to the ramp.
  - Heavy tanks come in through the 12 m SW lane gate.
  - Light vehicles use `gap_csw_small`.
  - Scavengers split into two teams.
- **Sabotage story:** with the tower down, the points are spiked and the train has to unload on the open line east of
  the yard. With the ramp down, tanks cannot leave the flatcars.
- **Build:**
  - main track, T0 and T1 as `BP_RoadSpline`; move `SM_Berlin_SwitchTower`;
  - 2 `SM_DestroyedSovietTrain_*` wrecks;
  - the existing fence Blueprint with two gates, plus a guard hut and `BP_Cover_RusSandbags1`.

  Nothing new.

### B Militärrampe (through line, long platform)

- **Layout:** the line runs straight through the yard along its south side, with a run-round loop 5 m north of it. It
  continues west past the yard and ends in a bomb crater with a burnt locomotive in it.
- **Platform:** a 70 m concrete military platform, 1.25 m high, runs along the south side of the line. It works as a
  wall for vehicles, leaving a 9 m strip south of it.
- **Targets:**
  - The ramp is the platform's west head: the side platform and a slope going down west, with no end deck. Because
    the platform is on the south side, `SM_Berlin_RailRamp` has to be **mirrored** (negative Y scale) or remodelled.
  - The tower stands on the dead line 60 m west of the yard.
- **Garrison:** an AT gun and a squad on the platform face east toward the expected approach; a second squad camps
  on the north side. Both targets lie at the quiet back door.
- **Response:** off the platform at its east slope and straight out of the east gate, the shortest exit of the six.
- **Build:** a new **platform segment** (12 x 6 x 1.25 m) or Breakthrough concrete slabs, and the crater at the line
  cut, which is landscape work west of the yard pad.

### C Lokschuppen (roundhouse round E5)

- **Story:** this uses the E5 lore that is already written: E5 "Drehscheibe" is the yard's glazed turntable pit.
- **Layout:**
  - A turntable lead runs from the throat through the **E5 fence gap** to the pit. The gap stays open: vehicles drive
    over the sunk track.
  - A brick curb rings the 24 m pit. `Crater0` stays the pit, so no turntable mesh is needed.
  - A burnt half-ring roundhouse with 7 stalls stands round the pit's south-east to south-west side, with one burnt
    locomotive in a stall.
- **Targets:** the ramp sits on a south siding and the tower at the lead junction. Both are on the yard's south side,
  outside the DZ.
- **Garrison:** an **MG nest on the roundhouse** overlooks E5. Harvesting E5 quietly means taking that nest out first.
- **Routes:** scavengers come through the roundhouse stalls. Light vehicles enter through the E5 gap. Medium and heavy
  tanks use the SW gate (the E5 gap, 11 m, also fits them).
- **Build:** roundhouse walls from the Berlin ruin kit (B facade slabs, E pieces). A roundhouse front piece is new if
  the kit does not read as a roundhouse.
- **Cost:** it clears about 12 ruin and rubble items south-west of E5 and crosses a side street.

### D Sperrbahnhof (walled compound)

- **Layout:** Shield's own walls enclose the west half of the yard (44 x 53 m). They are `Rus_Building_Wall` plus
  the connection pieces and `Rus_Building_GateWired`, the Ger_Trains set, which is tank-breachable since the Rus wall
  breach work.
- **Garrison:** an `Rus_BXP_MGBunkerOneCamoed` at the gate corner, a squad inside, and a squad at the gate. The
  compound interior is outside the DZ, but the wired gate is inside it: **the gate is the alarm trap**.
- **Ways in:**
  - Quiet: a collapsed drain culvert under the south wall, infantry only
    (`BP_Airport_Dest_DrainPipeStack` plus an infantry-only nav gap, as the Berlin ruin breaches do).
  - Loud: medium and heavy tanks breach the west wall from the south lane.
  - Light vehicles only fit through the gate, so a light raid is the loud one.
- **Tower:** outside the walls at the east throat, as in A.
- **Sabotage story:** the compound is a dead end, so without the ramp the tanks have to be craned off, which is slow.

### E Wagenburg (wagon maze)

- **Layout:** the main line plus 4 sidings, with 9 burnt wagons (`SM_DestroyedSovietTrain_CargoCar` / `_FuelCar` /
  `_Locomotive`) set as rows with gaps.
- **Movement:**
  - Infantry pass through the 1.5-2 m gaps between wagons.
  - Light vehicles use two planked cross lanes.
  - Medium and heavy tanks keep to the north and south yard roads.
- **Targets:** the ramp stays where it is now; the tower stands against the south fence, beside the south road
  (X -3300).
- **Garrison:** the north road holds the garrison camp (camouflage nets, sandbags).
- **DZ, optional change for the owner:** with today's 40 m `dz_yard`, both targets and their scavenge rings are
  still outside the zone (ramp ring +13 m, tower ring +4 m). Only the cross lane at Y 17300, and the infantry path
  through it, touch the zone, by 2-8 m. Shrinking the zone to the north camp (r 26 m, drawn on the sheet) would make
  the whole maze stealth ground.
- **Option:** the wagon wrecks could be `AScavengeableObject`s that give vehicle parts, for extra scavenger play.

### F Wendeschleife (turning loop on a bank)

- **Layout:** the line runs round a teardrop loop on a 1 m landscape embankment with 1:2 slopes. Three splines meet
  at the loop's foot. West of the foot the bank comes down, so the stub with the ramp is at ground level.
- **Targets:** the tower stands on the island inside the loop. The ramp is a side ramp on the stub, with its slope
  facing the SW gate; it has no end deck.
- **Crossings:** vehicles cross the bank only at two planked level crossings:
  - a **narrow** one (3.2 m) on the base leg, outside the DZ: light vehicles only, the quiet way onto the island;
  - a **wide** one (10 m) on the south leg, inside the DZ: medium and heavy tanks reach the island only through it,
    which is loud.

  Infantry climb the bank anywhere.
- **Garrison:** a squad and an AT gun on the island, sentries at the ramp.
- **Response:** off the ramp's slope and out by the SW gate, then outside the fence and back in through the E5 gap
  to the east gate. Inside the fence, the gap between the bank's toe and the fence near the loop's foot is too narrow
  for heavy tanks (3.9 m).
- **Cost:** the most ground and nav work: an embankment in the generator or a height sub-rectangle import as for the
  start crater, plus `NavArea_Null` along the slopes. The bank also cuts the south lane. The light-only filter at the
  narrow crossing has to be proven with the four agents.

## Building the pick (all versions)

1. **Track.**
   - Replace the yard part of `BERW_Rails` (`YARD_SPUR`, the last leg of rail_east) with `BP_RoadSpline` actors:
     `RoadMesh = SM_RustedRefinery_RailRoad`, `RoadMaterial = M_YellowRail` (as on Ger_Trains), `bUseCollision`
     off, `bAllowDecals` off.
   - Sink the track so the sleeper tops sit about at ground level.
   - Smooth the points with the Catmull pass of `rear_build.catmull_pts`.
   - **(open)** Whether the whole rail_east line, and the scenery lines, also switch to the wooden track. That gives
     one track look on the map, and the response train needs a continuous path anyway.
2. **Targets.**
   - Make two Blueprints, children of `AScavengeableObject`, for the ramp and the tower: health, `ScavengeTime`, no
     resource reward, death effects.
   - Replace the plain StaticMeshActors placed by `step_custom_poi()` with them.
   - Add a hint marker in the style of `BP_Hint_UseScavengers` over each.
3. **Dressing.** Drop the generic `BER_Yard*` pieces from `set_pieces()` and add the picked version's pieces as their
   own generator step (labels `RY_`, folder `Berlin/RailYard`).
4. **Response train.** `BP_SovietCargoTrain` with `PathPoints` along rail_east and the yard track, a
   `StopFrontLocation` at the unload point, and the mission spawning the tanks there. Whether to unload at the end ramp
   (the train has to be pushed in) or the side platform has to be decided.
5. **Nav.**
   - Rebuild and save.
   - Path checks for the four agents: the routes on the sheet, the E5 gap, and the response exit east.
   - Rerun the floating-gaps detector (`Tools/BerlinMap/gaps`) after placing.
6. **Mission.** The sabotage effect (fixed delay or smaller groups) stays the owner's open question (BerlinMissionFlow
   5.3). The physical story of each version is on the sheet.

## Open questions for the owner

- v02: which of D1, D2 or D3, or a mix (for example D1's compact court with D3's dug-in tanks outside the walls)?

- Which version, or a mix: for example A's layout with C's roundhouse, or D's walls round B's platform.
- Version E only: shrink `dz_yard` to the garrison camp (optional; E works without it)?
- Version B only: mirror the existing ramp mesh, or model a south-side variant?
- Change the whole rail_east line and the scenery lines to the wooden track, or only the yard?
