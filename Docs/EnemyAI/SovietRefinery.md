# Soviet refinery (A1 + G2): build contract (2026-11-24)

The owner picked two refinery designs from the concept rounds (`_AssetSource/SovietRefinery/SovietRefinery_Brief.md`,
refinement `Concepts/v04/A1|G2/*_refinement.md`). Both are built as playable Blueprints. Economy decisions:
[EnemyHarvestEconomy.md](EnemyHarvestEconomy.md). This file is the shared contract between the modelling work, the
harvest-loop fix and the refinery C++/Blueprints. Change it only together with whoever depends on the part you change.

| Design | Base (unchanged) | Addition | Crane |
|---|---|---|---|
| **A1** | `Rus_Zone_Refinery1` / `SM_Steel_refinery_09` | `SM_SovRef_A1_StorageHall` on the 4.5 m deck, front receiving hopper | `SK_SovRef_Crane` on a forecourt pad |
| **G2** | `Rus_Zone_Refinery3` / `SM_Steel_refinery_07` | `SM_SovRef_G2_AnnexStorage`, three stepped bins on the annex roof | `SK_SovRef_Crane` on a 3 m platform right of the annex |

Owner's open questions from v04, answered with defaults (change later if the owner says so):
- A1: open ridge kept; crane drops into the front receiving hopper.
- G2: radixite into bin 1 front cell, metal straight into bin 1 back cell; casings stay as drawn; crane right of the annex.
- Hoist rig: a **station-specific copy** of SovietCrane_1 (`SK_SovRef_Crane`); the existing rig and its 6 cycles stay untouched.
- Stock: one scaled fill mesh per cell plus a separate level-bar mesh per gauge (C++ scales them in Z).

## 1. Frames and units

- **Building frame** = the Blueprint root = the building mesh component (both at identity in the zone BPs).
  UE axes and centimetres. Blender concept specs are metres with UE Y = -Blender Y; convert on export.
- Every mesh below is exported **already placed in the building frame** (pivot rules per mesh below), so a component at
  identity lands in the right place unless the rule says otherwise.

## 2. Content (all under `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/SovietRefinery/`)

```
Crane/   SK_SovRef_Crane (+ _Skeleton, _PhysicsAsset), materials reuse the SovietCrane_1 MIs
A1/      SM_SovRef_A1_StorageHall, SM_SovRef_A1_CranePad, SM_SovRef_A1_Tripper,
         SM_SovRef_A1_Fill_Radixite, SM_SovRef_A1_Fill_Metal, SM_SovRef_A1_Bar_Radixite, SM_SovRef_A1_Bar_Metal
G2/      SM_SovRef_G2_AnnexStorage, SM_SovRef_G2_CranePlatform,
         SM_SovRef_G2_Fill_Bin{1,2,3}_{Radixite,Metal}, SM_SovRef_G2_Bar_Bin{1,2,3}_{Radixite,Metal}
Materials/  MI_SovRef_* (children of existing project masters; radixite stock = existing radixite crystal look,
            metal stock = existing ore/ingot look; level bars yellow = radixite, blue = metal)
Blueprints (refinery side): Rus_Building_SovietRefinery_A1 (child of Rus_Zone_Refinery1),
                            Rus_Building_SovietRefinery_G2 (child of Rus_Zone_Refinery3),
                            BP_SovRefCraneStation_A1 / _G2 (children of the C++ crane station)
```

- **Fill meshes:** pivot at the **cell floor centre**, modelled at the **full** stock height. C++ scales Z from 0.04
  (empty) to 1.0 (full) and hides at 0. Component placed at the pivot location (so: export with the pivot at the
  floor centre, and give the pivot's building-frame location in the layout JSON).
- **Bar meshes:** same rule, pivot at the bar bottom, full height modelled.
- **Tripper (A1):** pivot at its rail contact centre; the JSON gives the rail start/end; C++ moves it along X.
- Static meshes: simple collision on the storage/platform meshes (boxes are fine), none on fills/bars/tripper.

## 3. Layout JSON (one per design)

`_AssetSource/SovietRefinery/Build/A1/layout.json` and `.../G2/layout.json`, building frame, cm, degrees:

```json
{
  "crane":    { "location": [x, y, z], "yaw_deg": 0.0 },
  "dock":     { "pick_point": [x, y], "harvester_yaw_deg": 0.0, "approach_distance": 900.0 },
  "drop_points": { "Radixite": [x, y, z], "Metal": [x, y, z] },
  "overflow_point": [x, y, z],
  "harvester_exit": { "location": [x, y, z], "yaw_deg": 0.0 },
  "rally_point": [x, y, z],
  "fills": [ { "mesh": "SM_SovRef_G2_Fill_Bin1_Radixite", "resource": "Radixite", "order": 0, "pivot": [x, y, z] } ],
  "bars":  [ { "mesh": "...", "resource": "Radixite", "order": 0, "pivot": [x, y, z] } ],
  "tripper": { "rail_start": [x, y, z], "rail_end": [x, y, z] }
}
```

- `dock.pick_point` = where the Breaker's **hopper centre** must stand; `harvester_yaw_deg` = the Breaker's actor yaw
  while docked (the concept sheets: hopper on the camera side).
- `drop_points` = where the **grab** opens (jaw bottoms about 1 m above the receiving rim).
- `order` = fill order across several cells of one resource (G2: bin 1 fills first).
- `harvester_exit` = where newly trained Breakers appear (on navmesh, clear of the dock); `rally_point` = where they
  drive first.

## 4. Crane rig contract: `SK_SovRef_Crane`

A copy of SovietCrane_1 reworked for runtime posing from C++ (`UPoseableMeshComponent`). No animation needed.

| Bone | Parent | Driven how |
|---|---|---|
| `root` | - | static; origin = pedestal foot centre |
| `crane_yaw` | root | local rotation about Z (slew) |
| `boom_pitch` | crane_yaw | local rotation about its luff axis (sign in the JSON) |
| `boom_tip` | boom_pitch | not driven; its location is the cable top |
| `cable` | boom_tip | **only the cable geometry** is skinned to it; C++ scales it along its length axis |
| `grab_head` | boom_tip | C++ sets it in **component space**: location = tip minus hoist length, rotation = upright |
| `jaw_l`, `jaw_r` | grab_head | local rotation about the hinge axis (open/close) |
| socket `grab_load` | grab_head | where payload meshes hang in the closed grab |

- The **shrunk grab is baked into the geometry** (owner's call: closed jaws about 0.7 x 0.7 m to fit the Breaker's
  1.45 x 0.82 m hopper slot; head block about 1.1 m). All bone scales are 1 at rest. Nothing scales the jaws.
- Rig facts file `_AssetSource/SovietRefinery/Build/Crane/crane_rig.json`: bone names, axes and signs (yaw, luff
  up/down, jaw open), rest cable length (cm, along which bone axis), luff limits (deg), jaw open/closed angles, rest
  boom-tip location in component space.

## 5. Code ownership (who edits what, 2026-11-24)

- **Harvest-loop fix** (sub-agent): `Resources/Harvester/*`, `Resources/ResourceDropOff/*`,
  `Game/GameState/GameResourceManager/*`, the player resource manager, strategic-AI unit selection helpers
  (`IdleUnitSelectionPolicy`, `StrategicAIHelpers`, `BlackboardQueries`), its own test. Contract it provides:
  - drop-offs of any owner register and are found only by harvesters of the same owner;
  - a drop-off **not owned by player 1** stores what is delivered (its cargo slots are the stock) and never credits
    the human player; `GetDropOffAmountStored` / `RemoveResources` work on it without touching the player;
  - `UHarvester::StartAutonomousHarvest(ERTSResourceType)`: finds the nearest field of that type and keeps
    harvesting (re-targets when a field runs dry) with no player command;
  - harvesters are never picked into enemy attack / defend / idle pools.
- **Refinery** (main session): new `Enemy/SovietRefinery/*`, new `Resources/UnloadStation/SkeletalCraneUnloadStation.*`,
  `Enemy/StrategicAI/Component/EnemyStrategicAIComponent.*`, `Enemy/EnemyController/*`,
  `Enemy/TrainingAndUnitCreation/StrategicTrainingState/*`, the Breaker/spawner/refinery Blueprints.
- **Modelling** (sub-agents): Blender + `_AssetSource/SovietRefinery/Build/`, and the UE import of the assets in §2.

## 6. What is built (2026-11-24)

**Runtime**
- `USovietRefineryComponent` (`Enemy/SovietRefinery/`) on the refinery Blueprint. Once the building stands it:
  - creates a `USovietRefineryDropOff` named `ResourceDropOff`; its cargo slots are the stock (radixite 1500, metal 1500);
  - spawns the crane station (`UnloadStationClass`) on the building mesh;
  - converts 15 radixite and 15 metal every 10 s, at 0.02 points per unit, into `AddBonusTrainingPoints` /
    `AddBuildingPoints` of the owner's `UEnemyStrategicAIComponent`. **These numbers are placeholders, not balanced.**
  - scales the fills and bars tagged `SovRefFill_<Res>_<order>` / `SovRefBar_<Res>_<order>`; cells of a resource fill in
    order;
  - shuttles the A1 tripper (`SovRefTripper`) between `TripperRailStart/End` while there is stock;
  - trains free Breakers at the component tagged `SovRefHarvesterExit` (45 s each) up to
    `FEnemyAIMissionSettings::MaxHarvestersPerRefinery` (default 2), always spawned for the refinery's own player, and
    calls `UHarvester::StartAutonomousHarvest` (radixite first, then alternating). Same-owner harvesters within 60 m
    are adopted when it starts.
- `ASkeletalCraneUnloadStation` (`Resources/UnloadStation/`): one bay, SK_SovRef_Crane posed through a
  `UPoseableMeshComponent`. The slew is solved from the rig (tip bearing), the luff by bisection on the reach, and the
  hoist from the tip height. The grab turns to line its jaws up with the docked hull. Three trips per unload, about
  20 s each; every release credits one chunk through `DeliverBayChunk`. Rail = dock → drops (only so the base class
  registers the bins).
- Harvesters are kept out of the AI's unit pools: blackboard queries, picks, `RegisterUnitAsBlackboardIdle` and
  `UEnemyDirectControlComponent::RegisterDirectControlUnit`.

**Content** (`.../RusBuildings/SovietRefinery/`)
- `Rus_Building_SovietRefinery_A1` (child of `Rus_Zone_Refinery1`) and `_G2` (child of `Rus_Zone_Refinery3`).
- `BP_SovRefCraneStation_A1` / `_G2`.
- Both are built by `Source/Tools/SovietRefinery/ue_build_refinery_bps.py` from `Build/<D>/layout.json` and
  `Build/Crane/crane_rig.json`, and the script is idempotent.
- Breaker: subtype pins set to `Tank_T34_Harvester` (SetUnitSubtype and `BP_GetTankDataOfPlayer`), and it has a row in
  `RTS_BP_RTSAsyncSpawner` → SetupMediumRusVehicles.

**Test:** `Source/Tools/SovietRefinery/run_soviet_refinery_test.ps1` (`-SovietRefineryValidate`, RTS_ToCopySmall).
- It spawns both designs for player 2 with their own radixite and metal field.
- It checks the crane rig and drops, N harvesters trained and owned by the AI, a forced 300 radixite + 150 metal trip
  (crane trips, exact amounts, grab within 70 cm of the hopper), natural delivery, conversion, and that your
  resources stay untouched.
- Pictures are saved to `Saved/SovietRefineryTest/<time>/`. The test takes the AI's actors out of the fog for the
  pictures only.

**Gotchas**
- The Breaker Blueprint defaults to owner 2, like the other `Rus_` vehicles (fixed 2026-11-25; it had been copied
  from the Pz I with owner 1). Its RTS component is inherited from `RTS_BP_TrackedTankMaster`, so the value lives in
  the Breaker's own component override. For AI player 3 or 4, set the owner on the placed instance.
  `GetOwnerOverrideForAIPlayer(2)` returns "no override", so the refinery passes the owner explicitly.
- Harvest montages far from the camera: `URTSOptimizer` stops ticking the skeletal meshes of far units, which froze
  the harvest montage (it never reached OnCompleted). Since 2026-11-25 `UHarvester` calls
  `URTSOptimizer::SetKeepMontagesTicking` while the montage plays. This fixes every harvester, not only the Breaker.
  The harvester's watchdog (12 s / play rate + 6 s) stays as a safety net for an interrupted montage: the harvester
  Blueprints wire OnCompleted only. The test checks `harvest_montage_ends_on_its_own` (harvest animations seen, and
  `UHarvester::MapTest_GetWatchdogFinishCount` stays 0).
- RTS_ToCopySmall has no metal fields and no ground past X ≈ 25000.
- Enemy actors are fog-hidden from test cameras; the crane station is not a unit and always shows.
