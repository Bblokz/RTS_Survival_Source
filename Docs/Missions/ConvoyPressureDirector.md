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

## Ore Mountains setup

`Ger_2ndOreMountains` has one director, `ORE_PressureDirector`, on the Base B knoll. It watches `ORE_Convoy1..6` at
1 / 3 / 5 stopped convoys. Its responses are filled by `Source/Tools/OreMountainsMission/place_director_responses.py`
(built 2026-11-23, editor Python, re-runnable).

**Running it:**
- **In an open editor:** `import place_director_responses as pdr; pdr.run(); pdr.check()`. This does not save.
- **As a throwaway full-editor job:** `run_director_responses.py`, launched from PowerShell like `run_ore_placement.py`.
  It rebuilds navigation, runs the script and its check, and saves only when the check passes. It then logs the map size
  before and after.
- **Order:** run it **after** `place_convoy_content.py`. That script resets `TierResponses` to empty TODO entries on
  every run, so a re-run of `run_ore_placement.py` wipes the tiers and this script must run again.

**How a re-run behaves.** Every actor is labelled `ORE_Director_*`, with tag `ORE_Director` and folder
`OreMountains/Director`. On a re-run the script:
- updates the actors it finds by label, so the director's references survive;
- spawns the ones that are missing;
- deletes `ORE_Director_*` actors that are no longer in the plan;
- leaves the Ammo and Concrete cargo entries (from `place_convoy_content.py`) untouched.

It aborts when the director or a heap station is missing. Every placement actor has its start on BeginPlay off.

**Units per difficulty.** Wave spawners add their extra units cumulatively:
- Base: every difficulty.
- N: Normal and up.
- H: Hard and up.
- B: Brutal and Ironman.

| Tier / cargo | What | Units (starter values) |
|---|---|---|
| 1, patrols | `ORE_Director_T1_Patrol_East`: patrol from the Base B rear-ramp foot (18200, -1800) along the east flank lane to the radio outpost. `_North`: patrol from south-west of the battery (15200, 9000) through the dead pines between the outpost and the battery. 4 waypoints each; first wave 10 s / 40 s after the tier; once. | Base: Mosin + BA-12/BA-14. N: + Mosin. H: + PTRS (Okhotnik). B: + T-70/BT-7/T-26 |
| 2, retake squads | The director's own `TargetedAttack` on `ORE_Explore_Station_HN1` and `_HB3`. Spawn points: the east foot of the knoll (14900, -800) and north of the checkpoint (1600, 9800). Retries every 20 s until a station is held. | 2 Mosin + 1 PTRS spread over both points, and 1 BA-12/BA-14 at Base B. **Same on every difficulty** (see below) |
| 3, ridge raids | `ORE_Director_T3_Raid_Sally`: from the valley floor below Base B (5300, -6800), through the viaduct gap and up the prow sally ramp to the bench and `ORE_HQ_AttackMarker`; 2 waves 300 s apart. `_MainRamp`: from the foothill toe east of the road (7700, 13300), across the Radixite field, up the main ramp and past the refinery to the marker; once, 90 s after the tier. Both show on the minimap. | Sally: Base: 2 Mosin + light tank. N: + PTRS. H: + T-34/76. B: + Maxim + T-34/76. Main ramp: Base: 2 Mosin + armoured car. N: + PTRS + light tank. H: + Mosin. B: + T-34/76 |
| Fuel cargo | `ORE_Director_Fuel_1..4`: strongpoints (`bSpawnOnBeginPlay` off, no dig-in) with 2 light tank posts each, plus a 3rd post on Brutal. In list order: valley side east of the knoll, the east flank lane, the west ramp foot, behind the knoll. `PlacementActorsPerEscapedConvoy` = 1. | T-70 / BT-7 / T-26 per post |

**Why there are 4 fuel strongpoints.** Fuel can escape with convoys 1, 2, 3 and 6. Convoys 2 and 3 are mixed, and their
fuel trucks count through `ByCargoType`.

**Stations and `bOnlyHostileTargets` (left on).** `AOreHeapStation` changes the owner of its RTS component:
- It starts at owner 0, which is neutral. Neutral is not hostile to player 2, so the station is skipped.
- On capture it becomes the capturer (1, hostile), so it is attacked.
- When knocked out it becomes `KnockedOutOwner` (2), so it is skipped again.

The script checks owner 0 and `KnockedOutOwner` == 2 on every run. The test also stepped the station through these
states and read `IsActorHostileToPlayer(Station, 2)`: false, true, false, true.

**Limits:**
- `FConvoyPressureTargetedAttack.Units` has one list and no per-difficulty lists. The tier 2 squads are therefore the
  same on every difficulty. Scaling them needs C++.
- The attack launches once, at the stations held at that moment. A station captured later is not attacked by this tier.
- **Wave supply.** Every wave unit takes 1 of the enemy controller's wave supply (default 20). It comes back only
  when the unit dies, including after hand-over to the AI. With everything alive at once, these responses hold:

  | Difficulty | Living wave units |
  |---|---|
  | New To RTS | 17 |
  | Normal | 23 |
  | Hard | 28 |
  | Brutal | 35 |

  The counts include both Sally waves. The counterattack stages draw on the same supply. At 0 supply, elements are
  skipped, and a wave iteration with no unit is spent. A `Once` spawner the director started then never comes back.
  To raise the supply, call `SetEnemyWaveSupply` (BlueprintCallable on `AEnemyController`) from the Opening stage.

**Test (2026-11-23).** Run headless in the DebugGame commandlet on a file copy of the map in `Maps/Tmp`, with
`test_director_responses.py`. The copy was deleted afterwards.
- Two runs kept the same 8 actors.
- `check()` passed: thresholds, tier lists, the attack's targets, units and spawn points, the Fuel list, start flags
  off, and no editor label problems.
- Spawn points and posts are flat to 1.5 m over 8 m, and the sphere overlap check found no colliding actor at any of
  them.
- All 24 nav legs found full paths on the default agent: spawn to every patrol point, every raid leg, both tier 2 spawns
  to both stations' drop-off points.

**Not verified:**
- A full-editor save of the map and a reload. The test copy was not saved because a second full editor was not started
  (VRAM was about 10 of 12 GB in use).
- Vehicle nav agents. The legs were checked on the default agent only. `OreMountainsMap.md` says every ramp passes for
  all four agents with the same slope limit, so heavy tanks can also use the prow sally ramp.
- Any behaviour in play.

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
