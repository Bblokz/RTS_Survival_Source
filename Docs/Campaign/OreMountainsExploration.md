# Ore Mountains: exploration objects and start base (placement)

Status 2026-11-23: script and station Blueprint built, tested headless on a throwaway copy of the map (nothing saved
there). **Not placed on `Ger_2ndOreMountains` yet**: the lead runs the script. No play test. All spots are
*provisional* (the owner has not picked from `Saved/OreMountains/spots/ore_spot_candidates.png`).
Design: [OreMountainsMission.md](OreMountainsMission.md) ("Wismut spoil heaps", "The player's base", "Fire lookout
towers", "Buried 1945 armour", "Open questions").

## Running it

- Script: `Source/Tools/OreMountainsMission/place_exploration.py`. With the map open in an editor:
  `py "D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMission/place_exploration.py"`, then save the map.
- It is rerunnable: each run deletes every actor labelled `ORE_Explore_*` and places them again (outliner folder
  `OreMountains/Exploration`, actor tags `ORE_Explore` and the label without the prefix, e.g. `Station_HN1`).
- To move an item to another candidate, change its label in `PICKS` at the top of the script and run it again.
- Report: `Saved/OreMountains/exploration/place_report.json`.
- `BP_OreHeapStation` needs a DLL with `AOreHeapStation`: **DebugGame only until Development is rebuilt.** In a
  Development editor the script skips the two stations with a warning and places the rest.
- Test: `test_place_exploration.py` (DebugGame commandlet) loads a file copy of the map at
  `/Game/RTS_Survival/Maps/Tmp/Ger_2ndOreMountains_ExploreTest`, runs the script twice, checks and saves nothing.
  Delete the copy afterwards.

## What is placed where (headless test result, 2026-11-23)

| Label | Item | Spot | Placed at (cm) | Yaw |
|---|---|---|---|---|
| `Tower_T1` | `BP_OreLookoutTower` | T1 (+8 m, off a tree) | (8770, 25866, 703) | 0 |
| `Tower_T2` | `BP_OreLookoutTower` | T2 | (15770, 3066, 1060) | 0 |
| `Buried_StuG_S1` | `BP_BuriedArmour_StuGIII` | S1 | (-10700, 6700, 1800) | 90 |
| `Buried_PzIV_P1` | `BP_BuriedArmour_PzIV_F1_Assault` | P1 (+4 m) | (19100, 900, 1012) | -45 |
| `Buried_Sdkfz251_V1` | `BP_BuriedArmour_Sdkfz251_MortarTW` | V1 | (-2800, -2500, -4) | 180 |
| `Buried_FlakLuchs_L3` | `BP_BuriedArmour_FlakLuchs` | L3 (+2 m) | (13500, 4400, 980) | -45 |
| `Buried_Wespe_W1` | `BP_BuriedArmour_Wespe` | W1 (+2 m) | (24100, 17300, 912) | 90 |
| `Station_HN1` | `BP_OreHeapStation` | HN1, north toe of spoil heap 1 | (5200, 6400, 493) | -163 |
| `HN1_Metal_1`, `_2` | `Resource_Metal_Ore_Glaskopf`, `_Katzengold` | on spoil heap 1 | (3216, 6365), (3766, 5412) | |
| `Station_HB3` | `BP_OreHeapStation` | HB3, beside the valley-west metal | (-1200, -13100, -26) | 125 |
| `Base_Gamma` | `Ger_GammaFacility_Truck`, pre-built | layout `b_gamma` (+2 m) | (-20905, 5695, 1810) | 0 |
| `Base_Refinery` | `Ger_Refinery_Truck`, pre-built | layout `b_refinery` (+10 m) | (-14235, 9519, 1809) | 0 |
| `Base_Barracks` | `Ger_Barracks_Truck`, pre-built | layout `b_barracks` | (-17755, 3350, 1810) | 0 |

How each is set:
- **Towers:** yaw 0 as the tower doc asks (door south). The site is off-centre on its pivot
  (X -360..1020, Y -240..1907), so the pivot is offset to centre the footprint on the checked spot. The site and the
  `entrance_01` door point must be clear of colliding actors (nudged up to 10 m; T1 moved 8 m off a tree). Ground
  under the yaw-0 site varies 1.0 m (T1) and 1.2 m (T2).
- **Buried armour:** the spot's footprint axis, with the nose (+X) on the end whose 10 m strip ahead is flatter and has
  no colliding actor (ties face the HQ). Nudged up to 10 m when the footprint touched something (P1, L3: trees;
  W1: a fallen trunk). Units (pawns) are not obstacles: a Soviet T-34 placed near P1 is ignored.
- **Stations:** +X (the drop-off point, 8 m out) faces the centre of the metal within 40 m. HN1 is sunk to the lowest
  ground under the bin (the heap toe varies 1.8 m).
- **HN1 metal:** spoil heap 1 had no metal at all, so the script puts two Ore metal resources on the heap's flattest
  ground (only when no metal is within 40 m; `PLACE_HN1_METAL`).
- **Ground:** a line trace onto the landscape (it hit in the commandlet too), else `Ore_height.raw`, else the JSON z.
- Base buildings keep 8 m off the straight lorry line `ramp_main` -> `CapturedTruckDropOff`, 18 m off the drop-off
  pad and off the reserved HQ spot, and need ground flat to 1.2 m. The mine railway has no collision and is not
  checked.
- **Base position (lead's call):** the buildings use the layout's base points (`Ore_layout.json`: `hq`, `b_gamma`,
  `b_refinery`, `b_barracks`), not "west of the drop-off" literally. With +Y east, the drop-off is at Y 8196: the Gamma
  (Y 5695) and the barracks (Y 3350) are west of it, the refinery (Y 9519) is east, and the HQ spot (-17085, 8040) is
  49 m north of it on the bench. To move a building, change its layout point id in `BASE` or edit the coordinates.

## Start base and the HQ

- Pre-placed player buildings work: a placed `ANomadicVehicle` with `bStartAsConvertedToBuilding` (instance-editable)
  converts in `BeginPlay`. The Showcase map does the same. Default owner is player 1. At game start each one plays its
  normal deploy (truck montage and build animation, `GetTotalConstructionTime`), so the base unfolds during the
  opening.
- **The HQ is not pre-placed by default (`HQ_MODE = 'profile'`).** The profile loader always spawns an HQ truck at the
  player start (`UPlayerProfileLoader::RequestToSpawnHQ`), and `RTS_PLayerStart` already stands on the HQ spot
  (-17085, 8040). A pre-placed HQ next to it would give two HQs, and `ACPPController::SetupPlayerHQReference` takes
  whichever it finds first. The script keeps the HQ footprint free, so the truck spawns there and the player (or the
  Opening) deploys it.
- **A built HQ at the start, with existing systems:** set `HQ_MODE = 'preplaced'`. The script then:
  - places `Ger_HQ_Truck` as a built HQ on the HQ spot (`ORE_Explore_Base_HQ`; tested headless);
  - turns on the player start's Blueprint flag `DoNotLoadProfileUnits`, so no second truck spawns.

  The Opening stage must then call `ACPPController::MissionOnHqSpawned` once, as the Intro mission does after its
  glider HQ. With the flag on, `LoadProfile` returns before the units and the load never completes on its own;
  `MissionOnHqSpawned` runs `OnPlayerProfileLoadComplete`, which finds the HQ and applies the game-setting resources
  and HQ resource cards. Note that it also adds the bonus passed to it.

  This is the lead's call. It touches the Opening stage, and the faction start force must then come from the mission
  (the campaign deck holds no unit cards anyway). Switching back to `'profile'` leaves the flag on; the script warns,
  and the flag must then be turned off by hand.

## The heap station (`BP_OreHeapStation`)

`/Game/RTS_Survival/Blueprints/Environment/OreMountains/HeapStations/BP_OreHeapStation`, a child of C++
`AOreHeapStation` (`Source/RTS_Survival/Environment/OreHeapStation/`, child of `ACaptureActor`). **No graph.** It is
built by `Source/Tools/OreMountainsMission/build_heap_station_bp.py` (DebugGame commandlet).

Why C++ and not a Blueprint-only child (checked in code 2026-11-23):
- Enemy weapons only shoot what is in the unit manager. `URTSComponent` registers a fixed class list, and
  `ACaptureActor` is not on it. The pressure director's tier-2 attack is an attack-move to the targets, with no
  attack order, so a Blueprint-only station would never be shot at.
- `UResourceDropOff` reports an error on every delivery unless its owner implements the C++-only
  `IResourceStorageOwner`.
- It reads its owner from a `URTSComponent` and refuses any owner but player 1 at registration. Registration happens
  in `InitResourceDropOff`, which is protected and has no public deregister.
- The base capture actor can be captured again while held, which costs the squad 2 more units.

How it works:
- **Start:** neutral. The RTS component has owner 0, `UNType_GarrisonBuilding` and is not in the game state. The
  drop-off is off and holds no storage. Damage is ignored.
- **Capture:** 2 infantry, 20 s, "Taking the loading station...". The squad walks to the drop-off point in front of
  the bin (`GetCaptureLocationClosestTo`), since the bin itself carves the navmesh. While held or being captured it
  asks for 999 units, so further orders are refused. On completion (`OnStationCaptured`):
  - the owner becomes the capturer;
  - on the first capture the drop-off registers, with 0 storage;
  - the drop-off gets `StorageCapacity` (600 metal, through `AddExpansionStorage`) and is switched on, and
    harvesters choose it when it is nearer;
  - the station enters the unit manager as a building target;
  - an active fog component gives 18 m of vision and the enemy-vision tag that AI weapons need;
  - the `HeapStationHeld` parts show.
- **Knock-out:** while held, damage takes `HeldHealth` (2500). At zero the station is knocked out instead of
  destroyed:
  - the drop-off switches off;
  - its storage is removed with `RemoveExpansionStorage`, so the metal stored there is lost;
  - it leaves the unit manager and the fog;
  - the owner becomes `KnockedOutOwner` (2, the Soviets have retaken it), so the pressure director's
    `bOnlyHostileTargets` skips it;
  - the `HeapStationKnockedOut` rubble shows.
- **Retake:** the player captures it again. Health is full again and storage comes back. "Repair" is this recapture;
  there is no repair command for environment actors.
- **Events and getters:**
  - `OnHeapStationStateChanged(Station, bIsHeld, Player)` (BlueprintAssignable) and `BP_OnStationStateChanged`, for the
    station capture voice line or an objective;
  - `GetAllHeapStations`, `GetIsHeld`, `GetHoldingPlayer`, `GetStationHealth`;
  - `KnockOutStation()` for tests.
- **Harvesting the far heaps:** not locked (open question 9). The station only shortens the trip.
- **Settings** (`StationSettings`): `DropOffResource`, `StorageCapacity`, `HeldHealth`, `VisionRadius`,
  `KnockedOutOwner`, and the three look tags. Starter values for the owner to tune.
- **Placeholder look** until the owner picks a concept (`sheet_4_heapstation`):
  - QuarryEnv `SM_Silo_2` ore bin (10.7 x 10.7 x 7.7 m, centred, BlockAll, affects navigation);
  - the lookout's orange `SM_OreLookout_HeldPanel`, shown while held;
  - an A4 rubble fan, shown once knocked out.
- **Mission wiring:** for tier 2, add both `ORE_Explore_Station_*` actors to the pressure director's
  `TargetedAttack.Targets`.

## Tests run (2026-11-23)

- `build_heap_station_bp.py`: built and saved the Blueprint; the defaults read back (mesh, BlockAll, owner 0).
- `test_place_exploration.py` (DebugGame commandlet, file copy of the map, deleted afterwards):
  - 14 actors, the same 14 on the second run, 0 warnings;
  - the ground trace hit at every spot;
  - the station mesh override survives a fresh load;
  - the base buildings are owner 1 with `bStartAsConvertedToBuilding` on;
  - the stations are owner 0, with the bin BlockAll.
  - **State machine** on `Station_HB3` (editor world: no BeginPlay, no game state):
    - capture -> held, owner 1, 2500 health;
    - 1000 damage -> 1500;
    - 2000 more -> knocked out, owner 2, health reset;
    - damage while knocked out -> ignored;
    - recapture -> held, owner 1;
    - `KnockOutStation` -> knocked out.
  - `HQ_MODE = 'preplaced'` -> the HQ is placed built and `DoNotLoadProfileUnits` is on.

## Not verified

- **No PIE or game run.** None of the runtime behaviour has been seen working, including:
  - capture, drop-off, harvester choice and storage changes;
  - target registration: AI fire on the held station, and whether units also shoot it when attack-moving;
  - the fog component starting, stopping and starting again across owner changes (it reacts to owner changes itself);
  - the knock-out and the look swap;
  - the buildings' deploy at start, and whether the Barracks and Gamma deploy without the HQ standing.
- **Spend order:** spending takes resources from drop-offs in registration order, so the HQ is emptied first. A full
  station then sends harvesters home until the base storage is spent. Tune `StorageCapacity`.
- **Navigation:** the headless run checked footprints against colliding actors and the ground, not the navmesh. The
  editor rebuilds navigation after the placement; check that the stations and tower sites stay reachable. The lorry
  route check is a straight line, not the real path.
- **HN1 ground:** the HN1 station sits on a heap toe that varies 1.8 m; the stand-in bin is sunk on the high side.
- **Development DLL:** it has no `AOreHeapStation` until Development is rebuilt.
- **Hover title:** the station's RTS component uses `UNType_GarrisonBuilding` (every other type needs a subtype or
  reports an error), so `URTSComponent::GetDisplayName` makes the held station's drop-off hover read "Apartment
  Building". Before capture the hover shows "Capturable: 2 units required". A one-line name case in shared code fixes
  it.
