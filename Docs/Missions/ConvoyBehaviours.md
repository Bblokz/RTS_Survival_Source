# Convoy behaviours (timed stop, branch, ambush zones, hunters, cargo outcome)

Built 2026-11-23 for the Ore Mountains mission (convoys 2, 3 and 4). It compiles (DebugGame); it has not been
play-tested.

Everything is set on the placed `AEnemyConvoyRoute` (details panel, or Blueprint, since the settings are
`BlueprintReadWrite`). The route turns the settings into an `FConvoyBehaviourPlan` for each convoy it starts and hands
it to `AConvoy::SetBehaviourPlan`. The convoy runs the plan in its own tick, so there are no timers, and the route
turns the convoy's events into typed Blueprint events. There is no mission flow in C++: the mission Blueprints start
the routes and react to the events.

Code: `Enemy/Convoy/ConvoyBehaviourTypes.h`, `Enemy/Convoy/ConvoyBehaviours.cpp` (the convoy side), and
`Enemy/Convoy/ConvoyRouteActor/EnemyConvoyRouteBehaviours.cpp` (the route side).

## Convoy index

Every route event carries a `ConvoyIndex`. It counts the convoys **this route** has started, **across every
`StartRoute` run**, from 0. A route that the mission starts three times gives the indices 0, 1 and 2. Each behaviour has
`OnlyConvoyIndices`: leave it empty to apply the behaviour to every convoy of the route, or list the indices it applies
to. With one route actor per convoy, leave it empty. `GetConvoyIndexOf(Convoy)` returns the index of a running convoy.
`SetConvoyDefinition` lets one route run differently loaded convoys.

## a) Timed stop (`Stop` + the `StopPoint` diamond)

The whole column (trucks and escorts) halts at the stop point for `StopSeconds`, then drives on. Each truck brakes
towards the slot it has when the front truck stands at the stop.

If the convoy is attacked on the way, the stop is skipped (`SkippedAttacked`). If it is attacked during the stop, the
stop ends (`Attacked`). Either way the convoy floors it: the trucks' pace is multiplied by `FloorItSpeedMultiplier`
for `FloorItSeconds`. "Attacked" means the convoy is in `UnderAttack` (a hostile unit is in `ResponseRadius`) or a
member is in combat, for example from long-range fire. A player unit merely parked within `ResponseRadius` therefore
also counts.

| Setting | Default | Notes |
|---|---|---|
| `bEnabled` | false | |
| `OnlyConvoyIndices` | empty | Empty = every convoy. |
| `StopActor` | none | Optional; replaces the `StopPoint` diamond. Projected onto the drive path. |
| `StopSeconds` | 60 | |
| `ArriveTolerance` | 400 cm | The stop starts when the front truck is this close (along the road). |
| `FloorItSpeedMultiplier` | 1.4 | Capped at the trucks' top speed (restriction fraction 1). |
| `FloorItSeconds` | 30 | |
| `bFloorItIgnoresHalt` | true | While flooring it, `HaltBehindEscorts` does not stop the trucks. |

- **Events:** `OnConvoyStopStarted(Route, Convoy, ConvoyIndex)` and
  `OnConvoyStopEnded(Route, Convoy, ConvoyIndex, Reason)`. `Reason` is one of `TimedOut`, `Attacked`,
  `SkippedAttacked` or `Cancelled`.
- **Calls:** `AConvoy::CancelStop()` ends the stop without flooring it (reason `Cancelled`). The stop is also
  cancelled when the column loses all its trucks.
- **Queries:** `AConvoy::GetStopPhase()` (`None`, `Approaching`, `Halted` or `Done`) and `GetIsFloorItActive()`.
- **Never stalls short:** a truck short of its stop slot always rolls at least 150 cm/s. The stop also starts 20 s
  after the front truck came within 25 m of the stop point, even if it never got within `ArriveTolerance`.
- **Limits:**
  - A stop point at the exit is reported and ignored.
  - A stop point behind the column's start makes the convoy stop where it formed.
  - The convoy state stays Cruise/UnderAttack during a stop, so `ASurrenderingCargoTruck` capture still works.

## b) Branch split (`Branch` + the `BranchPoints` diamonds)

- **Splitting:** trucks whose cargo is `Branch.CargoType` leave the column when they reach the fork, which is the
  first `BranchPoints` diamond projected onto the road. They then drive the branch path to its last point, or to
  `DestinationActor` when one is set. The branch path is built once per run with `BuildNavigableRoute`.
- **Mixed columns:** a column gets more than one cargo type through `FConvoyTruckSpec::bOverrideCargoType` and
  `CargoTypeOverride` in the convoy definition.
- **Branch trucks stay convoy members** (member state `Branching`):
  - A destroyed branch truck counts as destroyed and drops its cargo cache.
  - A branch truck can still surrender.
  - It keeps the convoy pace times `SpeedMultiplier`. It does not stop for the column's halts and has its own
    stuck re-path.
- **Arrival:** a branch truck within `ArrivalRadius` of the destination has delivered. `OnBranchTruckArrived` fires,
  then the truck is removed (`bDespawnOnArrival`; when that is off, the truck is released alive). Deliveries are
  counted apart from escaped trucks: see `GetBranchDeliveredTruckCount` on the route and the convoy.
- **Finishing:** the convoy finishes only when both parts are done. If the column reaches the exit first, it stands
  there and waits for the branch trucks, at most `MaxWaitAtExitSeconds`; branch trucks still driving after that count
  as escaped. If the column is wiped out while branch trucks still drive, its escorts wait where the column was.
  When the last branch truck is done, the usual "trucks lost" behaviour (guard, then withdraw) takes over.
- **No matching trucks:** a convoy with no truck of the branch cargo simply never splits.

| Setting | Default | Notes |
|---|---|---|
| `bEnabled` | false | |
| `OnlyConvoyIndices` | empty | |
| `CargoType` | Ammo | |
| `DestinationActor` | none | Appended to the `BranchPoints`. Two points, or one point plus this actor, are needed. |
| `ArrivalRadius` | 1500 cm | |
| `SpeedMultiplier` | 1.0 | |
| `bDespawnOnArrival` | true | |
| `MaxWaitAtExitSeconds` | 120 | |

**Events:** `OnBranchTruckLeftColumn(Route, Truck, Cargo, ConvoyIndex)` and
`OnBranchTruckArrived(Route, Truck, Cargo, ConvoyIndex)`. The truck is only valid during the arrival event.

**Outcome rules:** when the column itself was stopped, the convoy keeps its destroyed or stopped outcome, even if some
branch trucks delivered. A convoy whose every truck delivered on the branch ends as `Escaped`.

## c) Cargo outcome (for the pressure director)

`OnConvoyCargoOutcome(Route, Convoy, Outcome)` fires once per finished convoy, before `OnConvoyFinished`. The route
totals already include that convoy when it fires. `FConvoyCargoOutcome` contains:

- `ConvoyIndex`, `ConvoyId`, the definition's `CargoType` and the convoy `Outcome`;
- `TruckCount` (escaped + destroyed + captured + branch-delivered; trucks that never spawned are left out);
- `EscapedTrucks`, `DestroyedTrucks`, `CapturedTrucks` and `BranchDeliveredTrucks`;
- `ByCargoType`: the same counts for each cargo type in the column.

You can also read it at any time with `AConvoy::GetCargoOutcome()`. `GetEscapedTruckCount`, `GetDestroyedTruckCount`
and `GetBranchDeliveredTruckCount` on the convoy give the single counts.

The route's `GetEscapedTruckCount` now uses the convoy's own escaped count. Branch-delivered and captured trucks are
not in it, and trucks that failed to spawn are no longer counted as escaped. Existing events and signatures are
unchanged.

## d) Ambush zones and the radio post (`Ambush` + the `AmbushZonePoints` diamonds)

- **Zones:** the `AmbushZonePoints` diamonds, plus every actor in `ZoneActors`, plus every actor tagged
  `ZoneActorTag` (set it to `ORE_AmbushZone` for the Ore map; the default `None` is off). Each zone has the radius
  `ZoneRadius`.
- **Entering:** when the column centre (the column's trucks, not the branch trucks) enters a zone,
  `OnConvoyEnteredAmbushZone(Route, Convoy, ConvoyIndex, ZoneIndex, bRadioPostAlive)` fires. The mission plays the
  warning, for example only when `bRadioPostAlive` is true.
- **Speed-up:** while `RadioPostActor` stands, the trucks drive `SpeedMultiplier` faster inside any zone. When
  flooring it at the same time, the larger multiplier wins.
- **Zone index order:** diamonds, then `ZoneActors`, then tagged actors.

| Setting | Default |
|---|---|
| `RadioPostActor` | none (no speed-up) |
| `SpeedMultiplier` | 1.35 |
| `ZoneRadius` | 3000 cm |
| `ZoneActors` | empty |
| `ZoneActorTag` | None |
| `OnlyConvoyIndices` | empty |

`SetRadioPostActor(Actor)` changes the radio post for convoys started afterwards. `AConvoy::GetTruckSpeedMultiplier()`
shows the current multiplier.

## e) Hunters (`Hunters` + the `DismountPoint` and `HunterWaypoints` diamonds)

- **Trigger:** the trucks carry infantry. When the front truck comes within `DismountRadius` (along the road) of the
  dismount point, the column halts `DismountHaltSeconds`. If it is attacked before that point and
  `bDismountWhenAttacked` is on, it halts where it is.
- **Unloading:** every living truck unloads its squads `UnloadSideOffset` beside it, on the side facing the path.
  A truck's squads are `PerTruckSquads[truck column index]` when that entry is not empty, otherwise `SquadsPerTruck`.
  The squads use the placed-actor picker `FEnemyPlacementUnit`. Trucks destroyed earlier lose their squads.
- **Attack:** the squads attack-move through the `HunterWaypoints` and then to `AttackTarget`.
- **Reuse:** this is the same code path as `AEnemyWaveSpawner`: one single-iteration `AttackMove` wave through
  `AEnemyController::StartWaveFromRequest`, with formation and combat support from `Hunters.Attack`. The controller is
  the route's `EnemyPlayer` controller.
- **Event:** `OnHuntersDismounted(Route, Convoy, ConvoyIndex, Squads)` fires when the wave has spawned. `Squads` are
  the spawned unit actors. The wave uses enemy wave supply: with no supply left it spawns nobody, a warning is
  logged and the event fires with an empty `Squads`.

| Setting | Default |
|---|---|
| `bEnabled` | false |
| `SquadsPerTruck` | empty |
| `PerTruckSquads` | empty |
| `DismountActor` | none |
| `DismountRadius` | 2500 cm |
| `bDismountWhenAttacked` | true |
| `DismountHaltSeconds` | 8 |
| `AttackTarget` | none |
| `UnloadSideOffset` | 600 cm |
| `Attack` | wave-spawner attack defaults |

`SetHunterAttackTarget(Actor)` sets the target; it is read at the dismount. Without a target and without waypoints, an
error is reported and nobody gets out.

## Setting up Ore convoys 2, 3 and 4

- **Convoy 2, the weighbridge:**
  - Set `Stop.bEnabled` and place `StopPoint` (or set `StopActor`) at the weighbridge. Keep `StopSeconds = 60`.
  - Start the route from the stage Blueprint. Bind `OnConvoyStopStarted` / `OnConvoyStopEnded` for guard lines; on
    `Attacked` or `SkippedAttacked` play "it floors it".
- **Convoy 3, the ammo split:**
  - In the definition, set `bOverrideCargoType = true` with `CargoTypeOverride = Ammo` on the ammo truck spec(s); the
    rest keep the definition's cargo.
  - On the route, set `Branch.bEnabled` and `CargoType = Ammo`. Put the first `BranchPoints` diamond on the road at
    the fork, and set `DestinationActor` to the battery.
  - Bind `OnBranchTruckArrived` and call the battery's `NotifyAmmoTruckArrived(Truck)`.
  - The stage completes on `OnConvoyFinished`, which comes after both parts.
- **Convoy 4, the hunters:**
  - Set `Hunters.bEnabled`, place `DismountPoint` in the ambush zone, fill `SquadsPerTruck`, and set `AttackTarget` to
    the player's HQ or ridge (via `SetHunterAttackTarget` if the HQ is spawned at runtime).
  - Bind `OnHuntersDismounted` for the warning.
- **Base B radio, on every Ore route:** set `Ambush.RadioPostActor` to Base B's radio mast, then add
  `AmbushZonePoints` (or set `ZoneActorTag` to `ORE_AmbushZone`).

## Surrendering trucks

A truck that holds itself in place (`ASurrenderingCargoTruck::GetIsHeldInPlace`: a scuttle countdown with
`bStopDuringCountdown`, or just surrendered) gets no move orders and no stuck checks from the convoy, in the column
and on the branch. This stops the false Stuck and Repath logs.

## Known limits

- **Cache spot (fixed 2026-11-23):** a lost truck's cache now lands beside its own wreck (ConvoyCargo.cpp), so branch trucks drop it where they died; it falls back to the route spot when the wreck is already gone.
- **Cache cargo (fixed 2026-11-23):** a truck with an overridden cargo type pays its share in that type's resource, by the same mapping as a capture (`ASurrenderingCargoTruck::GetPayoutMapping`).
- **Halts and branch fights:** an attack on a branch truck counts as an attack on the convoy (`HaltBehindEscorts`
  halts the column).
- **Escort squads:** during any halt they may creep slowly towards their look-ahead point, because a squad never
  marches slower than 60 cm/s. This was already so before these behaviours.
- **Plan timing:** `SetBehaviourPlan` is meant for the convoy's start; calling it later resets the behaviour state.
