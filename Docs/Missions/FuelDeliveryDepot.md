# Fuel delivery depot (`UFuelDeliveryDepotComponent`)

A self-contained supply line you can drop on any map to add life to a Soviet base. A fuel truck drives up to a
building, a crane lifts its fuel off, the building's AI player gets training points, and the empty truck drives back
and disappears. After a while it comes back full. If the player kills the truck, a new one comes later.

- C++: `Source/RTS_Survival/Missions/MissionProps/FuelDelivery/`
  - `FuelDeliveryDepotComponent.h/.cpp`: the loop and its settings
  - `FuelDeliveryNodeComponent.h/.cpp`: the placeable points (editor-only coloured arrow + label)
- Shared truck dressing: `Missions/MissionProps/FuelTruck/MissionFuelTruckSetup.h/.cpp` (hull hit box + wreck sink;
  `USecureBaseMission` uses the same helper)
- Crane: `AMissionFuelCrane::StartUnloadingTruckOnce` + `OnTruckCargoEmptied` (see [MissionFuelCrane.md](MissionFuelCrane.md))
- AI: `UEnemyStrategicAIComponent::AddBonusTrainingPoints`
- Blueprint: `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_LongCamoBunker_FuelDepot`
  (child of `Rus_BXP_LongCamoBunker`; built by `Source/Tools/FuelDelivery/build_fuel_depot_bp.py`)
- Test: `-FuelDepotValidate` (`FuelDelivery/Tests/FuelDepotTestSubsystem.cpp`), run with
  `Source/Tools/FuelDelivery/run_fuel_depot_test.ps1`

## Placing it on a map

1. Drag `Rus_BXP_LongCamoBunker_FuelDepot` into the level. Its owner is player 2 (the Soviet AI), as on every Rus BXP.
2. Select the placed bunker, then pick one of its nodes in the Details panel's component list and move it in the
   viewport. Each placed depot keeps its own node positions. The nodes are drawn only in the editor:

| Node | Colour | What it marks |
|---|---|---|
| `FuelTruckSpawnNode` | orange | Where the full truck appears, and where it drives back to and disappears. Put it out of sight (map edge, behind a ridge). The arrow is the spawn heading. |
| `FuelTruckParkNode` | green | Where the truck stands while the crane unloads it. The arrow is the truck's heading. |
| `FuelCraneNode` | cyan | Where the crane's mast stands. The arrow is the crane's forward axis. |

3. Keep the navmesh connected from the spawn node to the park node.

The defaults copy the intro mission's layout around the long bunker: crane just west of the bunker, truck parked
parallel to the bunker on its east side (heading +Y), spawn 35 m behind the park node.

### Park rules (the crane's reach)

- The crane only reaches a truck parked **parallel** to its jib's working side.
- The bed must be within about 11 m of the mast; ZIL pallets ~9.4–10.5 m away worked on the intro map.
- The truck first drives to an approach point `ApproachDistance` behind the park node (against the green arrow), then
  rolls in straight. The park order forces the final heading.
- If the crane can't reach any cargo, the truck lines up again twice, then drives home without unloading, and an
  error names the park node.

## The loop

`WaitingForTruck` → `SpawningTruck` → `DrivingToApproach` → `DrivingToPark` → `Unloading` → `DrivingHome` → removed →
`WaitingForTruck` (after `RefillSeconds`).

- **Spawn.** Through the async spawner, at the spawn node, for the building's owner. Each new truck is full: the six
  `Cargo_N` pallets of `Rus_ArmoredCar_ZilFuel`.
- **Unload.** The crane's one-shot mode lifts every reachable pallet onto its stack. It never refills the truck.
- **Points.** When the last pallet lands, the building's AI player gets `TrainingPointsPerDelivery`. They go to the
  strategic AI's training pool, the same pool its per-minute income fills. An unload that moved nothing grants nothing.
- **Going home.** At the spawn node the truck is removed with `Destroy()`. That leaves no wreck and plays no death.
  This is the "invisible while it refills" phase; the next truck spawns full after `RefillSeconds`.
- **Truck killed.** A new truck comes after `RespawnAfterDestroyedSeconds`. The wreck smokes, sinks and leaves a
  scorch mark (`TruckWreckDeath`), so no scavengable wreck farm builds up. Turn this off with `bSinkTruckWreck`.
- **Stuck.** A truck that makes no headway for `StuckSecondsBeforeRecycle` is removed with an error. The next one
  comes after `RefillSeconds`.
- **Crane stalls.** A parked truck still not empty after `MaxUnloadSeconds` drives home without points.

### When the building or the crane dies

- Deliveries stop. A truck on its way turns back and is removed at the spawn node. A truck being unloaded stops being
  unloaded and drives home.
- `bStopWhenCraneDestroyed` (default on) also stops the line when only the crane is destroyed.
- The crane is its own destructible prop. It stays when the bunker dies.
- When the bunker actor itself is removed with a truck still out, a timer bound to the truck keeps driving it home and removes it there (it gives up after `MaxLegSeconds` and leaves the truck as an ordinary enemy unit).

## Settings (all EditAnywhere on the `FuelDeliveryDepot` component, per placed instance)

| Setting | Default | Notes |
|---|---|---|
| `bEnabled` | true | Off: no crane, no trucks |
| `TrainingPointsPerDelivery` | 3 | Points the AI player gets per fully unloaded truck. The intro AI earns 1.5/min; a Mosin squad costs about 3. |
| `FirstTruckDelaySeconds` | 10 | Wait after BeginPlay before the first truck |
| `RefillSeconds` | 120 | Time the truck is "away" after it disappeared |
| `RespawnAfterDestroyedSeconds` | 180 | Time before a destroyed truck is replaced |
| `MaxUnloadSeconds` | 240 | Unload watchdog (a full unload takes about 85 s) |
| `StuckSecondsBeforeRecycle` | 90 | Removes a truck that can't move |
| `MaxLegSeconds` | 300 | Removes a truck whose drive takes too long, even if it keeps moving (e.g. the AI's damaged-tank retreat ordered it elsewhere) |
| `TruckClass` | `Rus_ArmoredCar_ZilFuel` (set on the Blueprint) | Must carry `Cargo*` static mesh components for the crane. No native default: only values saved on a Blueprint are followed by the cooker. |
| `ApproachDistance` | 1500 | Straight run-in behind the park node; 0 drives straight to the park node |
| `ParkStopShortDistance` | 230 | The park order aims this far past the node (vehicles stop ~2 m short) |
| `ParkTolerance` / `LooseParkTolerance` | 300 / 700 | Parked check (the loose one after three orders) |
| `HomeArrivalTolerance` | 600 | Distance from the spawn node at which the truck is removed |
| `TruckHullHitboxMesh` | `SM_Hull_Zil_Hitbox` | Padded hull collision so shells hit the open bed |
| `bSinkTruckWreck`, `TruckWreckDeath` | on, ZIL defaults | Wreck smoke, sink and decals |
| `CraneClass` | `BP_MissionFuelCrane` (set on the Blueprint) | The crane's rig, stack and speed settings live on that Blueprint (make a child to change them) |
| `bStopWhenCraneDestroyed` | true | See above |

Blueprint events on the component: `OnTruckSpawned`, `OnTruckUnloaded(Truck, TrainingPointsGranted)`,
`OnTruckDespawned`, `OnTruckDestroyed`. Getters: `GetDeliveryState`, `GetTruck`, `GetCrane`, `GetCompletedDeliveries`,
`GetTotalTrainingPointsGranted`.

## Using it on another building

Add a `FuelDeliveryDepot` component and three `FuelDeliveryNode` components to any AI-owned actor Blueprint whose
health dies through `IsUnitAlive` (any BXP). Set each node's `NodeRole`, and set `NodeColor` / `Label` if you like.
The component finds the nodes by role.

## Test

```powershell
powershell -File Source/Tools/FuelDelivery/run_fuel_depot_test.ps1 [-At "X,Y"] [-Yaw 0] [-Capture]
```

The test spawns the Blueprint for player 2 on `RTS_ToCopySmall` and shortens the timings to 2 / 6 / 6 s. It checks:

- the crane is at its node;
- truck 1 arrives full, parks (pose against the park node), and has 6 pallets lifted;
- the AI points rise by `TrainingPointsPerDelivery`;
- the truck is removed at home without counting as destroyed;
- truck 2 comes after the refill, full; it is killed and truck 3 comes after the respawn delay;
- once the bunker is killed, truck 3 is removed and no truck comes after it.

`-Capture` saves pictures while truck 1 is parked and unloading to `Saved/FuelDepotTest/<time>/`.
