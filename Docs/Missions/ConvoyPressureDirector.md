# Convoy pressure director (`AConvoyPressureDirector`)

The director turns the results of convoy routes into enemy pressure. Each stopped convoy raises an alert tier. Each
escaped convoy makes the enemy stronger according to its cargo. It was written for the Ore Mountains mission
(campaign mission 2) but has no mission names in it.

C++: `Source/RTS_Survival/Enemy/ConvoyPressure/`
- `ConvoyPressureDirector.h/.cpp`: the director.
- `ConvoyPressureTypes.h`: setting structs.
- `DormantDefence/DormantDefenceSlot.h/.cpp`: dormant defences.

Both actors are placed enemy actors (`AEnemyPlacementActor`). They have an editor label (yellow when the setup has a
problem) and shapes that show only in the editor. Nothing of them shows in game.

## Behaviour

**Watching routes.** On BeginPlay the director binds `OnConvoyCargoOutcome` of every route in `WatchedRoutes`. A
route listed twice is bound once. The route sends that event for every finished convoy, with its totals already
counted. On EndPlay the director unbinds and clears the retry timer.

For each finished convoy (`FConvoyCargoOutcome.Outcome`):

| Route outcome | Director |
|---|---|
| `TrucksStopped` (no truck reached the exit, at least one captured) | +1 stopped convoy |
| `TrucksDestroyed` (all trucks destroyed) | +1 stopped convoy while `bCountDestroyedConvoysAsStopped` is on (default) |
| `Escaped` (at least one truck reached the exit) | +1 escaped convoy, cargo responses |
| `Aborted` (ended from code or the world ended) | nothing |

The route's own `GetStoppedConvoyCount` counts only convoys with a captured truck. The director keeps its own count.

**Convoy index.** `OnEscapedCargo` passes on the route's convoy index from the outcome: 0 is the first convoy that
route started. With one route that runs every convoy of the mission, it is the convoy number minus one.

**Escaped trucks by cargo.** The director uses `FConvoyCargoOutcome.ByCargoType`, so a mixed column applies each cargo
type that got trucks away. For example, convoy 3's ammo trucks that branch off to the battery count as
branch-delivered, not escaped. If that tally is empty, the director falls back to the outcome's `CargoType` and
`EscapedTrucks`. A mixed convoy counts once in `GetEscapedConvoyCount` but sends one `OnEscapedCargo` per cargo type.

**Alert tiers.** The tier is the number of entries in `StoppedConvoysPerTier` that the stopped count has reached. The
default is 1 / 3 / 5, so the highest tier is 3.

The alert climbs one tier at a time. Each step runs that tier's response and then broadcasts
`OnAlertTierChanged(NewTier, OldTier)`. A jump, from `ForceAlertTier` or from re-entrant Blueprint calls, therefore
still runs every tier in between, in order.

Each tier's response runs only once. If `ForceAlertTier` lowers the tier, later stopped convoys raise it again and send
the events again, but they do not repeat the responses.

**Tier response** (`TierResponses[Tier - 1]`):
- **`PlacementActorsToStart`** start once each:
  - wave spawner: `StartSpawner`
  - convoy route: `StartRoute`
  - strongpoint: `SpawnGarrison`
  - air drop: `StartAirDrop`
  - dormant defence slot: `Activate`

  The director never starts the same placement actor twice, even when it sits in several lists. Turn off the
  actor's own start on BeginPlay.
- **`TargetedAttack`** (for retake squads) is an attack-move wave that the director builds itself on the enemy
  controller of its `EnemyPlayer`:
  - The units are `Units`, spread over `SpawnPoints` (relative to the director; drag the diamonds).
  - The waypoints are the living `Targets`, nearest first.
  - With `bOnlyHostileTargets` (the default), only targets whose RTS component belongs to a player hostile to the
    enemy count. A station the player has not captured is skipped.
  - When no target counts yet, the attack is tried again every `TargetedAttackRetrySeconds` (20 s; 0 = never) until
    one does.
  - The movement and formation settings are the wave spawner's attack settings. At the last waypoint the units go to
    the enemy AI's direct control, as wave spawner units do.
  - The units use enemy wave supply like any wave.

**Cargo responses** (`CargoResponses`). Every entry whose `CargoType` matches is applied for each escaped convoy of
that type:
- `PlacementActorsToStart`: the next `PlacementActorsPerEscapedConvoy` not-yet-started actors in list order.
- `DormantDefences`: the next `DormantDefencesPerEscapedTruck x escaped trucks` slots that have not been activated, in
  list order. If there are fewer, it activates what is left and logs that the list is used up; it never spawns more
  than the list holds. A slot whose spawn fails is skipped, and the next one takes its place.
- `AmmoSupplySecondsPerEscapedTruck x escaped trucks` goes out through `OnEscapedAmmoSeconds` when it is above 0.

The sum of all of this is "Base B gets stronger for every escaped convoy".

## Settings and defaults

| Setting | Default | Notes |
|---|---|---|
| `WatchedRoutes` | empty | the convoy routes (`AEnemyConvoyRoute`) |
| `StoppedConvoysPerTier` | 1, 3, 5 | must rise; one entry per tier |
| `TierResponses` | 3 empty entries | index 0 = tier 1 |
| `TargetedAttackRetrySeconds` | 20 | 0 = do not wait for a target |
| `bCountDestroyedConvoysAsStopped` | on | |
| `CargoResponses` | Fuel: 1 placement actor per convoy; Ammo: 60 s per truck; Concrete: 1 dormant defence per truck | VehicleParts has no entry: only `OnEscapedCargo` |
| `EnemyPlayer` | 2 | the AI player the attacks and slots spawn for |

The numbers are starter values for the owner to tune.

## Blueprint API

| Node | Kind |
|---|---|
| `GetAlertTier()` | pure, 0 = no alert |
| `GetStoppedConvoyCount()`, `GetEscapedConvoyCount()`, `GetEscapedTruckCount()` | pure |
| `ForceAlertTier(NewTier)` | for tests; going up runs the skipped responses |
| `AddStoppedConvoy()` | counts a convoy the mission stopped itself |
| `ApplyEscapedCargo(Cargo, Trucks, ConvoyIndex = -1)` | an escaped convoy by hand (tests, scripted arrivals) |
| `OnAlertTierChanged(NewTier, OldTier)` | event, once per step |
| `OnEscapedCargo(Cargo, Trucks, ConvoyIndex)` | event, every escaped cargo type |
| `OnEscapedAmmoSeconds(Seconds)` | event, forward it to the battery |
| `OnDormantDefenceActivated(Defence)` | event, the spawned defence actor |

Slot: `Activate()` returns the spawned actor. `GetIsActivated()`, `GetSpawnedDefence()` and `GetSlotId()` read its
state. `OnActivated(Slot, SpawnedDefence)` fires when it spawns.

## Setting it up in a map

1. Place one `AConvoyPressureDirector`. Add the convoy routes to `WatchedRoutes`.
2. **Tier 1 (patrols):** place patrol wave spawners in the foothills with `bStartOnBeginPlay` off. Add them to
   `TierResponses[0].PlacementActorsToStart`.
3. **Tier 2 (retake squads):** turn on `TierResponses[1].TargetedAttack`, then:
   1. Add the 2 spoil-heap stations to `Targets`.
   2. Set `Units`.
   3. Drag the `SpawnPoints` to where the squads come from (for example behind Base B).
4. **Tier 3 (ridge raids):** use attack wave spawners whose waypoints end on the player's ridge, or a targeted attack
   on ridge buildings. Add them to `TierResponses[2]`.
5. **Fuel:** use placement actors near Base B, for example a strongpoint with `bSpawnOnBeginPlay` off, or a wave
   spawner whose path ends at the base. Put them in the Fuel entry. Or place dormant defence slots with vehicle classes
   and set `DormantDefencesPerEscapedTruck`.
6. **Concrete:** place `ADormantDefenceSlot` actors at Base B, one per bunker or AT pit that may appear:
   1. Set `DefenceClass` to the Blueprint.
   2. Rotate the slot to face the threat.
   3. Set `FootprintHalfExtent` (editor drawing only).
   4. Add the slots to the Concrete entry's `DormantDefences`, in the order they should appear.

## Wiring from the mission Blueprint

- Find the director with `Get Actor Of Class`, or keep a reference on the mission.
- **Ammo to the battery:**
  1. Bind `OnEscapedAmmoSeconds`.
  2. In the handler, call `AddSuppliedSeconds(Seconds)` on the placed `AEnemyArtilleryBattery`.

  The director does not know the battery, by design.
- **Kill missions:** a defence that appears later should count for `DestroyBaseB`. Bind `OnDormantDefenceActivated`
  and add the actor to that `UKillActorsMission`'s `TargetActors` (the array is BlueprintReadWrite). **Never put a
  dormant defence slot itself in `TargetActors`:** the slot never dies, so the mission would never complete.
- **Voice lines and warnings:** use `OnAlertTierChanged` (play the warning for `NewTier` when `NewTier > OldTier`) and
  `OnEscapedCargo` (for example "the fuel got through").
- **Light counterattack (one unit stronger when convoy 2 escaped):** read `GetEscapedConvoyCount()`, or remember the
  `ConvoyIndex` from `OnEscapedCargo`.

## Dormant defences: why slots and not hidden actors

The brief asked for a component that keeps placed defences hidden and inactive until switched on. A placed unit or
building cannot be made fully inert from outside:
- It registers itself with the game unit manager, so turrets and AI target it.
- The fog component shows it when it is in vision.
- Its weapons search for targets on timers.
- Squads are several actors.

Re-adding a unit to the game unit manager is not public on `URTSComponent`. Hiding the actor would leave an invisible
bunker that can shoot and be shot.

So a dormant defence does not exist until it is activated. `ADormantDefenceSlot` spawns its `DefenceClass` with
`FRTSUnitOwnerSpawning::SpawnUnitForPlayer`, owned by the slot's `EnemyPlayer`, at the slot's transform. The defence
then comes up exactly like a freshly spawned unit or building. Two consequences:
- Per-instance edits made on a placed actor do not apply; set them on the Blueprint.
- The navmesh is `DynamicModifiersOnly`, so a spawned building's nav modifiers should carve the navmesh (unverified).

## Unverified (no play test yet)

- Whether attack-moving units fire on a captured station. Whether a station has an RTS component whose owner changes
  on capture; if not, turn off `bOnlyHostileTargets`.
- Whether the bunker and AT pit Blueprints that will be used work when spawned at runtime, with no init call and no
  owning building.
- The route's `OnConvoyCargoOutcome` and the per-type tally were being added by another change while this was
  written.
