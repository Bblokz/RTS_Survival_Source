# Soviet air supply pad

`BP_SovietAirSupplyPad` (`/Game/RTS_Survival/Blueprints/Environment/SovietAirSupply/`), C++ `ASovietAirSupplyPad`
(`RTS_Survival/Enemy/AirSupply/`). Drop it into any enemy area. It runs by itself; no mission scripting is needed.

## What it does

In a loop:

1. A cargo plane (`Rus_PE8_Aircraft_Transport`, owned by `EnemyPlayer`) starts `ApproachDistance` before the pad and
   flies in along the red landing arrow.
2. It lands vertically on the pad (the regular aircraft drop flow, `AAircraftMaster::OrderAircraftDrop` with no
   payload).
3. Fuel barrels and munition crates come out beside the plane one by one.
4. Two unarmed Mosin workers carry each item to its storage: barrels to the fuel shed, crates to the munition shed.
5. When the last item is stored, the AI player gets `TrainingPointsPerDelivery` training points and
   `BuildingPointsPerDelivery` building points.
6. The plane lifts off vertically and flies on along the arrow for `DepartureDistance`, then leaves the map.
7. `SecondsBetweenPlanes` after it left (or was shot down), the next plane comes.

The plane is a normal, targetable aircraft. **Shot down before the last item is stored = no points**: the cargo
vanishes and the workers walk back to their posts. Shot down after delivery (while leaving) keeps the points.

## Settings on the placed instance (`Settings`, "Air Supply")

| Field | Default | Meaning |
|---|---|---|
| bEnabled | true | Off = no planes until `SetDeliveriesEnabled(true)` |
| EnemyPlayer | 2 | AI player that owns the plane and gets the points |
| FirstPlaneDelaySeconds | 60 | BeginPlay (or enabling) to the first plane |
| SecondsBetweenPlanes | 240 | Plane gone (left or shot down) to the next plane |
| TrainingPointsPerDelivery | 3 | Training points per stored delivery |
| BuildingPointsPerDelivery | 3 | Building points per stored delivery |
| FuelBarrelsPerPlane / MunitionCratesPerPlane | 4 / 4 | Cargo per plane (more = longer unloading = longer raid window) |
| AircraftClass | Pe-8 transport | Set on the Blueprint |
| ApproachDistance / DepartureDistance | 200 m / 250 m | Flight legs along the landing arrow |
| CruiseHeightAbovePad | 25 m | Flight height in and out |
| TakeOffDelaySeconds | 3 | Wait on the pad after the last item is stored |
| MaxApproachSeconds / MaxUnloadSeconds | 300 / 240 | Safety timeouts (count as lost, no points) |

Point values are placeholders, not balanced. The economy sim rule applies: discuss rates before changing them for real
maps.

## Building the layout

Components on the Blueprint (move them per placed pad if needed): `PadMesh`, `RadioMastMesh`, `FuelStorageMesh`,
`MunitionStorageMesh`, `LandingPoint` (its X arrow is the flight direction), `FuelStoragePoint` /
`MunitionStoragePoint` (where workers put items down), `WorkerPostA/B` (where workers wait). Keep the flight line
along the arrow free of tall obstacles for `ApproachDistance`.

`WorkerVisuals` (Blueprint defaults) holds the worker anims, speeds, carry offsets and pile layout. The worker anims
are infantry-skeleton copies of the casual anims (`Source/Tools/SovietAirSupply/copy_worker_anims.py`). The Blueprint
is rebuilt by `Source/Tools/SovietAirSupply/build_air_supply_bp.py` (headless).

## Building points

`UEnemyStrategicAIComponent::AddBuildingPoints` / `GetBuildingPoints` is a bare pool for the planned base builder
(`EnemyBaseBuilding_Plan.md` section 3). Nothing spends it yet.

## Test

`-SovietAirSupplyValidate` on `UT_EnemyAI` (`Source/Tools/SovietAirSupply/run_air_supply_test.ps1`): full delivery
(owner, landing on the pad, workers carry, points, VTOL lift-off, plane leaves), shot down while unloading (no points,
pad cleared, next plane scheduled), shot down on approach (no points). Results and screenshots:
`Saved/SovietAirSupplyTest`.
