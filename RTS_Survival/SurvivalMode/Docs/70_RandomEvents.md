# 70 — Random events (grace-period events) and the reusable convoy

Owner: events worker. Review rounds: `Saved/AgentNotes/events_review/round_N/`.

## 1. What it is

Between survival rounds a random event can pull the player out of the base. The first (and so far only) event is an
escorted Soviet supply convoy: cargo trucks with an escort of vehicles and marching infantry drive from one map edge to
another, past the player's base. Destroyed trucks leave a scavengeable cargo cache next to their wreck; the cargo pays
only when a player squad scavenges it. A convoy that reaches its exit is lost.

Two layers:

| Layer | Folder | Knows survival? |
|---|---|---|
| Convoy core (reusable: campaign missions, other modes) | `Source/RTS_Survival/Enemy/Convoy/`, content `Content/RTS_Survival/Blueprints/Convoy/` | no (no `SurvivalMode/` include) |
| Survival scheduler | `Source/RTS_Survival/SurvivalMode/Events/`, data `Content/RTS_Survival/SurvivalMode/Data/Events/` | yes |

## 2. Convoy core API

```cpp
AConvoy* AConvoy::StartConvoy(WorldContext, UConvoyDefinition* Definition, const FConvoyRoute& Route,
                              const FConvoyStartParams& Params);   // BlueprintCallable
void AConvoy::EndConvoy(bool bDespawnUnits);
FOnConvoyEventNative OnConvoyEventNative;      // every event (C++)
FOnConvoyEventDynamic OnConvoyEvent;           // every event (Blueprint)
FOnConvoyFinishedDynamic OnConvoyFinished;     // outcome Escaped / TrucksDestroyed / Aborted
FConvoyRoute UConvoyRouteLibrary::BuildNavigableRoute(WorldContext, ControlPoints, Spacing, RouteId, bOutReachedEnd);
```

Events (`EConvoyEventType`): Started, StateChanged, UnitSpawned, SpawnFailed, RegroupStart/End, UnderAttack,
EscortEngage, EscortReturn, TruckDestroyed, EscortDestroyed, CargoSpawned, CargoScavenged (detail `scavenged=1` only
for a finished scavenge), CargoExpired, Stuck, Unstuck, Repath, SlotReassigned, ReachedExit, Despawned,
RestrictionCleared, Finished.

`UConvoyDefinition` (data asset): trucks (exact class, training option, calibrated cruise speed), guard tiers (escort
unit, count, station Lead / LeftFlank / RightFlank / Rear, extra trucks), cargo (cache class, resources per guard
training point, lifetime, scavenge time), spacing, speed (TruckSpeedFraction 0.5, catch-up, slot gain, look-ahead,
regroup lag), combat (response radius, leash, calm seconds, truck behaviour, what escorts do once the trucks are gone),
robustness (stuck window, nudge, max wait for a stuck truck), minimap icons.
Assets: `DA_Convoy_Fuel` (Soviet tanker + ZiS fuel truck, pays Radixite + Metal, cache `BP_ScavObj_RadixiteDump`),
`DA_Convoy_VehicleParts` (2 ZiS ammo/crate trucks, pays Vehicle Parts + Metal, cache `ScavObj_VehicleParts1`).
`Resource_Fuel` has no HUD slot or storage in the player economy, so the fuel convoy pays Radixite.

## 3. How the column moves

- One *column reference* distance along the route: the most lagging truck's distance minus its slot offset. Every slot
  (trucks every `TruckGap` behind slot 0, lead / rear rows, flanks spread along the trucks) hangs off it, so the column
  can never run away from a slow truck.
- Trucks: wanted speed = `TruckSpeedFraction` × calibrated cruise speed (+ a small catch-up when behind their slot,
  slower when ahead). Applied with `ATankMaster::SetMobilityRestriction(this, …)` keyed to the convoy; a closed-loop trim
  learns how much restriction gives that ground speed (a 0.5 restriction measures about 0.4 of top speed).
- Escort vehicles: wanted speed = the trucks' *measured* mean ground speed + slot gain × along-route slot error,
  capped at catch-up × pace, through the same source-keyed restriction. Cleared on release.
- Squads march (`MarchToLocation`, then `ASquadController::RedirectMarch` moves the far end ahead of them so the patrol
  never turns back). Their march speed is set to the convoy pace through `SetMarchSpeedOverride` (raised above their own
  march speed; the trucks' half speed is binding). Combat drops a squad out of the march (existing behaviour); the convoy
  re-issues the march when the squad is out of combat.
- Every unit is sent to its slot point `LookAheadDistance` ahead along the route, re-ordered when that point moved more
  than `ReissueDistance`; so path queries are short (no search-node limit) and units do not cut corners.
- Regroup: a truck more than `RegroupLagDistance` behind its slot puts the convoy in Regroup; the trucks ahead slow
  down through their slot error (gradual), not a hard stop.
- Stuck: less than `StuckProgressCm` in `StuckSeconds` while it should move → re-path to a point nudged sideways (growing,
  alternating); never a teleport. A truck stuck longer than `MaxWaitForStuckTruckSeconds` no longer holds the column.
- Attack: a hostile unit within `ResponseRadius` of any member (or a member in combat) → UnderAttack; escort vehicles in
  range attack-move to the threat, squads fight through the existing combat drop; nobody goes further than `LeashDistance`
  from the trucks' centre; `CalmSecondsToResume` after the last contact everyone re-forms. Trucks keep moving (default)
  or halt (data).
- Deaths re-assign slots; the last truck's death switches the escorts to guarding the caches (then withdraw).
- Exit: every column-holding truck within `ExitRadius` of the route end → every unit is despawned, restrictions cleared.

## 4. Survival scheduling (`USurvivalEventSubsystem`)

Hook: `ASurvivalModeDirector::GenerateStartLocationContent` calls `USurvivalEventSubsystem::StartForMatch(this)` once.
Timing (coordinator decision 2026-11-01): the event is rolled when a round starts **clearing**; it spawns at a far edge
(out of sight of player units, not an edge the last wave used) once `GetEnemiesRemaining() ≤ ClearingSpawnEnemiesThreshold`
(or at the intermission start at the latest), is **announced when the intermission starts**, and the intermission is
extended by up to `MaxIntermissionExtensionSeconds` (90) — shown on the HUD countdown — and ends early when the convoy
escapes or its trucks are destroyed and their cargo is taken or expired. A convoy still on the map when the next round
starts drives on to its exit (banner says so). Director additions: `ExtendIntermission(float)`,
`EndIntermissionExtension()`.

Roll (`RollForRound`, pure, shared with the sweep): from `FirstAllowedRound` (2), not within `CooldownRounds` (1) of the
last event, `EventChance` (0.45), forced after `ForceAfterQuietRounds` (4); convoy kind by weight; guard tier by round
(Light from 1, Medium from 5, Heavy from 10; Hard/Brutal/Ironman reach them 2/3/4 rounds earlier); cargo × difficulty.

Route (`FSurvivalConvoyRoutePlanner`): entry → via point near the base → exit, every enabled edge-zone pair and five
bearings × three distances per pair; straight-line pre-check, then the best few built on the vehicle navmesh with
chained path queries and checked again: closest approach to the picked start 15 000-26 000, ≥ 16 000 from every
unpicked start (Soviet bases), outside the oasis + 3 000, away from the last wave's edges, 50 000-150 000 long, length
budget = pace × (clearing lead + intermission + extension).

The wave director has no round-state delegate, so the subsystem reads `GetRoundState()` once a second (integration
request: an `OnRoundStateChanged` delegate on `USurvivalWaveDirectorComponent`).

## 5. Tests (`-ConvoyTest=`, `RTS_WITH_MAP_TESTS` only)

`SurvivalMode/Events/Tests/SurvivalConvoyTest.cpp`, inside a `-SurvivalAutoStart` match:
T0 (calibration lanes), T1 (one convoy, 4 Hz `units.csv`, `events.csv`, `route.json`, `cameras.json`, frames with
`-ConvoyShots`), Sweep (`-ConvoySweepSeeds=N`, decisions + routes, no spawning), Watch (unforced match).
Force with `-ConvoyDefinition=N -ConvoyTier=N -ConvoyEntry=<edge actor> -ConvoyExit=<edge actor>`, seed
`-SurvivalEventSeed=N`.

## 6. Shared-code hooks (additive)

- `ASquadController::SetMarchSpeedOverride`, `ASquadController::RedirectMarch`; `ASquadUnit::SetMarchSpeedOverride`,
  `ASquadUnit::RedirectMarch`, two members, one early return in `OnMoveCompleted` while redirecting, override branch in
  `ApplyMarchSpeed`.
- `AScavengeableObject::SetFixedScavengeRewards`, `GetExpectedScavengeRewards`.
- `ASurvivalModeDirector`: include + `StartForMatch` line, `ExtendIntermission`, `EndIntermissionExtension`,
  `RestartIntermissionTimer`, `M_IntermissionBaseEndTime`.

## 7. Known limits

- The march montage play rate is capped at 2.0 × 190 cm/s (`USquadUnitAnimInstance::UpdateMarchPlayRate`); squads
  raised to the truck pace (~500-630 cm/s) slide their feet. Needs a decision (raise the cap, a faster march montage, or
  a slower truck fraction).
- Escorts slower than the truck pace (T-34s 26 km/h, KVs 12 km/h) cannot keep station; tiers use BA, BT and T-70.
- Unit owner comes from the unit Blueprints (Soviet = player 2); `FConvoyStartParams::OwningPlayer` only selects who is
  hostile.
