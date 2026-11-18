# Soviet cargo train (`ASovietCargoTrain`)

Set-dressing train for missions. A locomotive, flatcars carrying static cargo meshes (tanks) and goods wagons roll in
along a rail path, stop at the station, and roll back out the way they came. The train is a plain `AActor`, not a
unit: nothing can select or target it, it has no collision and no navigation, and it is not counted as a unit. Built
for the Ger_IntroMission defence waves: on arrival the mission hides the cargo and spawns real tanks next to the
platform.

Code: `Source/RTS_Survival/Missions/MissionProps/SovietCargoTrain/SovietCargoTrain.h/.cpp`.
Blueprint: `/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/BP_SovietCargoTrain`. Its class defaults:
Locomotive / CargoCar / FuelCar meshes, `FlatcarDeckHeight` 175, `bHideCarsBeyondPathStart` true. It is built by
`Source/Tools/IntroMission/counterattack/cargo_train_bp.py`.

Cargo tanks: `.../IntroMission/CargoTrain/SM_CargoTank_T3476`, `SM_CargoTank_KV1E` and `SM_CargoTank_IS2`. Each is
baked by `cargo_tank_bake.py` from the real `Rus_Vehicle_*_Soviet` Blueprints: tracks, hull, turret base and gun, in
reference pose, merged with Geometry Script. They keep the real materials and have 3 LODs and no collision.

Check: `cargo_train_visual_test.py` runs a PIE check through `train_editor_server.py` / `train_cmd.py`. Its pictures
go to `Saved/CargoTrainTest/`. The unattended PIE window edge-scrolls the camera pawn, so the test re-pins the pawn
every tick.

## API

| Member | What it does |
| --- | --- |
| `StartRun(WorldPathPoints, StopFrontLocation, FlatcarCargoMeshes, NumGoodsWagons)` | Builds the consist in this order: the locomotive, one flatcar per cargo entry (a null entry is an empty flatcar), then the goods wagons. The front starts at the path start, and the cars behind it trail straight and level along the start direction, off-map. The front then rolls to the path distance closest to `StopFrontLocation` and brakes over `EaseDistance`, and `OnArrived` fires. You can call it only once per train. |
| `HideCargo()` | Hides every cargo mesh. |
| `Depart()` | Only after arrival. The train backs out along the path, easing in over `EaseDistance`. Once the locomotive front is back at the path start, `OnDeparted` fires and the train calls `Destroy()`. |
| `GetHasArrived()` | True while the train is standing at the stop. |
| `GetCargoWorldLocations()` | Returns the world bounds centres of the cargo meshes, front flatcar first. Hidden cargo is included. |
| `OnArrived`, `OnDeparted` | Native multicast delegates (`FOnSovietCargoTrainEvent`). Each is broadcast as the last statement of the tick, so a listener may call `Depart` or `Destroy` from inside it. |

`StartRun`, `HideCargo`, `Depart` and the getters are also UFUNCTIONs, so Python and Blueprints can call them (for
headless tests).

## How it moves

- **Path.** The world points become a polyline with cumulative distances. A distance below 0 or past the end
  extrapolates straight and level (z of the end point) along the end direction.
- **Cars.** Each car is a runtime `UStaticMeshComponent` that `StartRun` creates and attaches to the root. It is
  movable, has no collision, no overlaps, no nav effect and receives no decals.
- **Placement.** Every tick, car *k* is centred at `front - offset_k - len_k / 2`. It is oriented along the chord
  between its two bogie points (centre ± `BogieFraction` × length), which gives both yaw and pitch. The chord
  smooths over polyline kinks and junction steps.
- **Mesh alignment.** This is computed from the mesh bounds at runtime, so any mesh works. The long bounds axis is
  turned onto the track: an X-long mesh keeps its +X end in front, and a Y-long mesh gets yaw -90 so its +Y end is
  in front. The bounds bottom centre sits on the rail. This is the same convention `rear_build._train` uses for the
  parked trains. Use `*YawOffset` (for example 180) for a mesh whose front is the other end.
- **Cargo.** Each cargo mesh is attached to its flatcar. Its bounds bottom centre sits at `FlatcarDeckHeight` above
  the rail (0 means the top of the flatcar bounds), turned by its own long-axis fix plus `CargoYawOffset`. Then
  `CargoOffset` is applied around that bottom centre, in the flatcar frame.
- **Ticking.** The actor ticks only while it is arriving or departing.

## Class defaults (category "Cargo Train")

| Property | Default | Notes |
| --- | --- | --- |
| `LocomotiveMesh` / `FlatcarMesh` / `GoodsWagonMesh` | none | Set in the BP. `StartRun` reports an error and does nothing when one it needs is missing. |
| `LocomotiveYawOffset` / `FlatcarYawOffset` / `GoodsWagonYawOffset` / `CargoYawOffset` | 0 | Extra yaw on top of the long-axis fix. |
| `CargoOffset` | identity | A transform for each cargo mesh, applied around its bottom centre in the flatcar frame. |
| `FlatcarDeckHeight` | 0 (bounds top) | Height of the deck above the rail. BP_SovietCargoTrain sets 175, because `SM_SovietTrain_CargoCar` is a container well car: its well floor is 48 cm and its side sills are 171-180 cm above the rail, and the well is only about 2.6 m wide. The tanks straddle the well with their tracks on the sills. |
| `RailHeightOffset` | 0 | Lifts all cars above the path z. |
| `CruiseSpeed` | 900 cm/s | |
| `EaseDistance` | 2500 cm | Braking distance at the stop and run-up distance on departure (constant deceleration). |
| `MinEaseSpeed` | 40 cm/s | Speed floor during the ease, so the train never stalls. |
| `GapBetweenCars` | 120 cm | The same gap as the parked trains. |
| `BogieFraction` | 0.35 | |
| `bHideCarsBeyondPathStart` | false | Hides a car while its centre is behind the path start (past the end of the rails). |
| `EngineLoopSound` / `EngineSoundAttenuation` / `EngineSoundFadeSeconds` | none / none / 1.5 s | The sound asset must loop. It is attached to the locomotive, fades out at the stop and fades back in on `Depart`. |

## Ger_IntroMission path

- **Path file.** `Source/Tools/IntroMission/counterattack/train_path.json` (`points`, `junction_index`,
  `siding_only`) holds the rail path in world space, sampled every ~3 m.
- **Probe.** It comes from `train_path_probe.py`, which reads the map in a throwaway Development editor. The probe
  never saves the map. Its full dump goes to `train_probe.json`: rail control points, parked cars, edge blockers and
  skirts, and candidate mesh bounds.
- **Siding.** Since the map save of 2026-11-18 14:04, `REAR_Rail_Siding` is its own line and no longer branches off
  the main line. It runs from y 27448 (16 m beyond the landscape edge, z 104 over the skirt at z 97) straight
  down x 20875, S-curves to x 21000 at y ~17500, and ends at y 7600 in front of `REAR_Station_Buffer_Siding`. The
  path is therefore the siding alone: 68 points, 199.8 m. The probe falls back to "main line, then siding" if the
  siding branches off the main line again (a gap of 3 m or less).
- **Stop.** Suggested `StopFrontLocation` is (21000, 8500, 945), about 10 m north of the siding buffer. The
  `REAR_Station_TrainSiding_*` parked cars are no longer in the map.
- **Tail at the stop.** The locomotive and 6 flatcars are 148 m long, so the tail ends at about (20875, 23260),
  still on the map. Each goods wagon (FuelCar) adds 14.4 m. With 2 the tail is at y ~26070, past the landscape edge
  but still on the rails. With 3 it runs past the end of the rails.
- **Spawn.** The mission spawns units at (19953, 15593, 943), beside flatcar 2.
