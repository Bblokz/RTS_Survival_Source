# Enemy base garrison: marching columns, roving patrols, call-up (design v2, 2026-11-16)

Status: design only, no code. Companion to [EnemyBaseBuilding_Plan.md](EnemyBaseBuilding_Plan.md).
v2 folds in an adversarial review against the code; see "Review log" at the end for what changed and why.

## Goal

The enemy controller keeps a **garrison**: units that make its bases look manned and answer when a base is hit.
- **Marching columns.** Infantry squads march between the AI's bases, in step, along a route.
- **Roving patrols.** Vehicles drive wide loops around the bases, outside the walls.
- **A separate direct-control group.** The decision tree's idle pool, the retreat systems and the unit-ability AI never take garrison units over. The garrison drives them itself.
- **Call-up.** When a base is attacked, the nearest garrison groups break off and respond, then return to duty.

Non-goals: no change to waves, to the decision tree's own units, or to field constructions.

---

## 1. What we build on (checked against the code)

| Existing piece | Where | What it gives us, and the catch |
|---|---|---|
| **Convoy** (the march precedent) | `Enemy/Convoy/Convoy.h`, `ConvoyColumn.cpp`, `ConvoyRoute.h`, `ConvoyCombat.cpp` | `AConvoy` already marches squads along a polyline (`FConvoyRoute`, built by `UConvoyRouteLibrary::BuildNavigableRoute`). It gives:<ul><li>**Slots:** each member has a slot (`SlotAlongOffset`, `SlotLateralOffset`) behind a moving column reference.</li><li>**Look-ahead target:** the order target is the slot plus `LookAheadDistance` (`ConvoyColumn.cpp:271-276`), so a march end is never reached and never turns round.</li><li>**March upkeep:** redirect with fallback to a fresh march, pacing with `SetMarchSpeedOverride`, stuck/partial-path handling.</li><li>**Fight guard:** a squad that dropped its march to fight is left alone until calm (`:310-317`).</li><li>**Leash and return:** `EConvoyMemberState` (Station / Engaging / Returning, `LeashDistance`).</li><li>**Hostile scan:** a cheap radius check over the hostile player's units (`ConvoyCombat.cpp:31-71`).</li></ul>The garrison column reuses this machinery rather than reinventing legs. |
| March command | `ICommands::MarchToLocation` (the scripted entry, sets `IdMarch`; `Interfaces/Commands.cpp:2253`), `ASquadController::RedirectMarch`, `SetMarchSpeedOverride` | **The catches:**<ul><li>A fresh march sends every soldier to one point; only a redirect keeps their spread.</li><li>At the march end a soldier turns back toward the start (`SquadUnit.cpp:4030`).</li><li>Two completions within 1 s abort the march.</li></ul>A look-ahead target avoids all three. |
| **March drops on contact** | `ASquadController::OnSquadUnitInCombat` (`SquadController.cpp:635-647`) | Any shot fired or hit taken calls `StopCommand()` on a marching or patrolling squad. It then stands and fights only what is in range. The design must treat this as a state (§4.1), not a stuck squad. |
| Random patrol formation | `UEnemyFormationController::MoveRandomPatrolWithAttackMoveFormation` | Not used for vehicle loops (§4.2). Survival base infantry already use it (`SurvivalSovietBaseComponent.cpp:2007`). |
| Idle pool entry | `UEnemyDirectControlComponent::RegisterDirectControlUnit` (`EnemyDirectControlComponent.cpp:514-575`) | The **single** place units enter `IdleDirectControlUnits` (line 559). The guard goes there (§3). |
| Bases | Blackboard `EnemyBasePoints` (centroid + `CoreBuildingActors`/`Locations`, refreshed ~12 s); builder `FEnemyBuiltBase` (not built yet); `AddFortifiedBase` | Garrison homes. Base points carry **no radius and no gates**; §4.0 derives them. |
| Training spawn | `CreateTrainingBatch` / async spawn callback (`EnemyStrategicAIComponent.cpp:938-1163`) | The callback captures only location and index; the batch record is gone before the spawn completes. A garrison tag must be **captured in the lambda** (§5). |

---

## 2. Architecture

**`UEnemyBaseGarrisonComponent`** is a sibling component on `AEnemyController`.
- It thinks on its own timer (`ThinkIntervalSeconds`, 2 s), gated by `FEnemyGarrisonSettings::bEnabled`.
- It does **not** depend on `bAllowDirectControlStochasticDecisionTree`.
- **Start:** at the controller's BeginPlay when enabled, **not** from `StartStrategicAIThinkingTimer`. On `StartAfterMissionTrigger` maps the strategic AI may never start before the trigger, but a base should look manned from the first second. A setting `bStartWithStrategicAI` keeps the old behaviour for maps that want a quiet base until the trigger.

**Group** (the unit of control):

```
FEnemyGarrisonGroup
  int32 GroupID
  EEnemyGarrisonGroupKind Kind      // MarchColumn | RovingPatrol
  TArray<FEnemyGarrisonMember> Members   // weak actor, slot offsets, member state (as FConvoyMember)
  int32 HomeBaseID
  FEnemyGarrisonRoute Route         // FConvoyRoute polyline + column reference distance + direction
  EEnemyGarrisonGroupState State
  int32 AlarmID                     // INDEX_NONE when none
  double StateEnterTime, LastHostileSeenTime
  int32 TargetMembers               // squads (columns) or vehicles (patrols); never soldiers
```

**Group states:**

| State | Meaning | Leaves to |
|---|---|---|
| `Forming` | Members spawned or adopted, moving to their first slots | `OnDuty` when all are within slot error, or after 60 s with whoever arrived |
| `OnDuty` | Marching the route / driving the loop | `Contact`, `Responding` |
| `Contact` | **The group itself was engaged on duty** (march dropped on contact, or a hostile within `ContactRadius`) | `OnDuty` after `CalmSeconds` with no hostile near; `Withdrawing` if strength falls below 50 % |
| `Responding` | Assigned to an alarm, moving to it | `Engaging` on contact or arrival; `Returning` if the alarm clears first |
| `Engaging` | Fighting at the alarm, leashed (§6.3) | `Returning` after `StandDownSeconds` calm; `Withdrawing` below 40 % |
| `Returning` | Moving back to the nearest route point | `OnDuty` on arrival; `Responding` if a **High** alarm takes it |
| `Withdrawing` | Falling back to the nearest base's parade point | `Holding` |
| `Holding` | Depleted, at a parade point | `Forming` once topped up; released to the idle pool after `HoldTimeoutSeconds` (default 180 s) if nothing tops it up |

**One alarm per group at a time.** A group in `Responding`, `Engaging` or `Withdrawing` is never reassigned. A Low alarm never pulls a `Returning` group; a High one may. A group whose home base vanishes keeps its state until it would return. It then rehomes to the nearest base, or is released to the idle pool if there is none (§4.0).

---

## 3. A separate group: keeping other systems' hands off

Phase 1 needs **one `TSet<TWeakObjectPtr<AActor>>` of garrison units** on the garrison component and a few guards. A full ownership-registry refactor can come later if more systems need it (§10).

Where enemy units get taken over today, and the guard for each:

| System | Where | Guard |
|---|---|---|
| Idle pool (decision tree picks) | `RegisterDirectControlUnit` (`EnemyDirectControlComponent.cpp:514-575`), the single entry for all `RegisterAsBlackboardIdle` callers. Those callers are: training spawn, air drops, formation end, strongpoints, the BP library, the end of a damaged retreat, survival airlift and survival bases. A queued `RegisterAsBlackboardIdle` registers when it executes (`Commands.cpp:4080-4116`). | Early-out in `RegisterDirectControlUnit` when the unit is a garrison unit (next to the existing damaged-retreat guard at :550-557). This covers queued registrations too. |
| Async retreat groups | Scan **every** player-2 tank below the health threshold (`StrategicAIHelpers.cpp:1752-1776`); excluded only by `FillRetreatRequestExcludedUnits` (`EnemyStrategicAIComponent.cpp:1787-1800`) | Append garrison units to that exclusion list. |
| Damaged-vehicle retreat | Its roster is filled only on idle registration (`_DamagedVehicleRetreat.cpp:105-118`), so garrison vehicles never enter it | Nothing to guard. The garrison handles its own damaged vehicles (§6.4). |
| Unit ability AI | Rosters **every** player-2 squad and tank (`EnemyUnitAbilityAIComponent.cpp:184-238`); grenades and behaviours replace current orders on idle, attacking or fighting units | **Allow** abilities for garrison units in `Contact`/`Engaging` (they should throw grenades), **skip** them while `OnDuty`/`Forming`/`Returning` (a grenade order would end the march). The garrison exposes `GetIsGarrisonUnitOnDuty(Actor)`. |
| Tank tactical AI | `UEnemyTankTacticalAI` rotates idle tanks in combat toward threats (`EnemyTankTacticalAI.cpp:18-50`) | Harmless (it only fires when idle and fighting); leave it. |
| Training move-to-defence | The spawn callback registers idle, then moves to a defence position (`EnemyStrategicAIComponent.cpp:1137-1162`) | Garrison batches take their own callback branch (§5); never reached. |
| `RetreatAllFormations`, waves | Formations only (`EnemyController.cpp:598-664`); garrison units are never in a formation | Nothing. Adopting units that are in a formation calls `RemoveUnitsFromAnyFormation` first. |

**Hand-offs:**
- `ReleaseGarrisonGroup(GroupID)` removes the units from the set and registers them idle. Used on `HoldTimeoutSeconds`, with no bases left, or by a mission.
- `AdoptUnitsIntoGarrison(Units, BaseID, Kind)` is for missions and survival. It calls `DeregisterDirectControlUnit` and `RemoveUnitsFromAnyFormation` first.

---

## 4. Routes

### 4.0 Garrison bases

The garrison merges these into one list:
- **Builder bases:** `FEnemyBuiltBase` records, when the builder exists.
- **Blackboard bases:** `EnemyBasePoints`.
- **Designer bases:** an optional placed `AEnemyGarrisonPost` (an `AEnemyPlacementActor`) that pins a base, its gates or a route.

**Derived data for bases without a builder plan:**
- **Radius:** the 90th percentile distance from the centroid to its `CoreBuildingLocations`, plus 25 m, clamped to 40–150 m.
- **Gates:** 2–4 points where nav paths from the centre to the other bases (or, with one base, to the four compass points 200 m out) cross that radius.
- **Parade point:** the centroid projected to nav.

**Stable IDs despite cluster churn:** centroids are rebuilt every ~12 s and can shift or merge.
- A garrison base keeps its ID while any *core building it was first seen with* is still alive: match by building actors, not centroid distance.
- Two bases that merge keep the older ID; groups of the other are rehomed.
- A base whose buildings are all gone is removed; its groups finish their current state, then rehome to the nearest base, or are released if there is none.

### 4.1 Marching columns (infantry)

**Route graph:**
- **Nodes:** the garrison bases.
- **Edges:** a minimum spanning tree plus one extra shortest edge.
- **Long edges:**
  - Edges up to `MaxMarchEdgeLength` (default 400 m) are marched.
  - Longer edges are marched only if a road spline or AI road covers at least 70 % of them.
  - Otherwise the pair is not linked, and each base gets a **perimeter loop** instead.
- **Logging:** dropped edges are logged once, so "my bases are too far apart for columns" is visible, not silent.

**Route for an edge:**
1. AI road graph (builder), when both gates are on it.
2. Road splines from the blackboard, when one covers the edge.
3. Otherwise control points gate → gate fed to `UConvoyRouteLibrary::BuildNavigableRoute`.

Routes are cached per edge.

**Perimeter loop (one base, or unlinked bases):** a closed polyline at radius −10 m (inside the wall) through the gates, built the same way. A column then circles its base.

**Column = a small convoy of squads.** This reuses the convoy's column model rather than leader/follower chains:
- **Movement:** a **column reference distance** moves along the polyline at `ColumnSpeed` (default 0.9 × march speed, so the slowest squad keeps up).
- **Slots:** each squad has a slot `SlotAlongOffset = −i × ColumnSpacing` (8 m) behind the reference.
- **Orders:** each think step, every member on station gets `RedirectMarch(slot + LookAheadDistance)`, falling back to `MarchToLocation` when it is not marching. The target is never reached, so there is no turn-round and no double-completion abort.
- **Pacing:**
  - The reference pauses while any member is more than `MaxSlotError` (25 m) behind.
  - It pauses for at most `MaxColumnWaitSeconds` (30 s); after that the laggard is dropped from the column and walks to the next gate on its own, and is re-slotted there.
- **Gate dwell:** at a route end the reference holds `GateDwellSeconds` (10–25 s, seeded), then the route reverses (or takes another edge out of that base).
- **Dead members:** they free their slot; the remaining members close up (offsets recomputed), so there is no leader to lose.

**Contact on the march (the code's real behaviour).**
- **Entering Contact:** when a member drops `IdMarch` while its RTS component is in combat (the convoy guard), or the hostile scan (§6.1) finds a hostile within `ContactRadius` (40 m), the **group** goes `Contact`. It also raises an alarm at its own position (Low; High if strength < 70 %).
- **In Contact:**
  - Members that see the hostile **attack-move to the hostile's position**, leashed 35 m from where the contact started. Standing still, as the default behaviour leaves them, loses to anything that outranges rifles.
  - If the hostile is out of reach (beyond the leash, or a vehicle out-ranging them), the group goes `Withdrawing` to the nearest base instead of being sniped.
- **Back to duty:** after `CalmSeconds` (12 s) with no hostile within `ContactRadius`, the group returns to its slots and resumes.
- **Stuck checks** only run in `OnDuty`/`Returning`/`Forming`. A member that is in combat is never re-marched (this is what kept the old design yo-yoing).

**Route memory against ambush farming:** each edge counts members lost on it. An edge that loses ≥ 2 squads within `EdgeDangerWindow` (5 min) is **suspended** for `EdgeSuspendSeconds` (5 min). Columns on it switch to perimeter loops; the base's patrol gets a probe point on that edge (phase 4).

**Composition:** basic infantry only (Mosin, Dyakonov, Korovin, Okhotnik). No hazmats, no elite squads. 1–3 squads per column.

### 4.2 Roving patrols (vehicles)

**Engine: the garrison's own loop with attack-move orders** (formerly Option A). One controller per unit, a loop that reads as intentional. The random-patrol formation's random next-point pick and combat linger fight the leash.

**Patrol ring per base:** 8–12 points between perimeter radius + 20 m and + 80 m, seeded angles with jitter.
- Each point is projected to the vehicle nav and reachable from a gate (one path query per point, cached).
- Points are dropped:
  - within `PlayerExclusionRadius` (120 m) of the player HQ or a player bulk;
  - inside another AI base.
- **Per-loop radius jitter** (±15 m) and an occasional reversed direction. Otherwise a fixed loop carves a permanent rut ring into the RT track-trail system. Survival keeps its vehicles still for exactly this reason (`SurvivalSovietBaseComponent.cpp:1985`).

**Driving:**
- Points are driven in loop order with attack-move.
- At 2 seeded points per loop the patrol holds 15–30 s facing out.
- Contact, leash and withdraw as for columns (§4.1), with vehicle numbers (`ContactRadius` 70 m, leash 60 m).

**Composition:** fast light vehicles (BA-12, BA-14, BT-7, T-70, BTR-152A), 1–3 per patrol.

---

## 5. Strength and funding: a garrison budget slice

**Targets per base:**
- Columns: `ColumnSquadsPerBase` (default 2, with 3 from 4 production buildings). Patrols: `PatrolVehiclesPerBase` (default 2).
- Scaled by difficulty and capped by `MaxGarrisonMembersTotal` (default 16).
- "Members" means squads or vehicles, never soldiers.

**Budget (separate, so the army is never starved and the garrison never blocks training):**
- `GarrisonPoints` accrue as a fixed share of training income (`GarrisonShareOfTrainingIncome`, default 0.2). That share is **taken off** the training income, so the total army size knob is unchanged. Optional `StartingGarrisonPoints` per map.
- The garrison buys from its own pool with its own reservation, so a reserved army batch never blocks a garrison squad and vice versa (one shared reservation blocks everything today, `EnemyStrategicAIComponent.cpp:666-686`).
- **Unit prices:** the same as training (`EnemyTrainingHelpers`). **Tech:** columns need `BasicInfantry`, patrols `LightTanks`.
- **When the garrison is full:** income keeps accruing up to a cap (one column's worth). Beyond that it **flows back** to training points, so a full garrison doesn't hoard.

**Spawning (the code fix needed):**
- A garrison purchase calls the async spawner itself (it does not go through `CreateTrainingBatch`). The spawn lambda **captures the garrison group ID and base ID**, because the batch record is gone before the spawn completes.
- **Location:** the trainer nearest the home base. Else the builder's door for that building type. Else the home base's parade point. Never "a random cluster".
- On spawn the unit joins the garrison set *before* anything else touches it, so the idle-pool guard always holds.

**Adoption:** `AdoptUnitsIntoGarrison` lets missions and survival hand over placed units at no cost.

**Merging:** a group below half its target merges into a same-kind group of the same base. A group alone in `Holding` past `HoldTimeoutSeconds` is released to the idle pool.

---

## 6. Alarm and call-up

### 6.1 Detecting an attack: polled, not event-driven

The review showed the combat flag is a poor "base attacked" signal:
- It re-fires about every 5 s under sustained fire (`RTSComponent.cpp:38-63`).
- It fires on the unit's own shots (`TankMaster.cpp:1390`, `BuildingExpansion.cpp:679`).
- It fires on any damage with no instigator.
- Base points expose only core buildings, while the player usually hits walls and bunkers first.

So the alarm is **polled per base every think step (2 s)**, cheaply, in two parts:

1. **Hostile scan:** like `AConvoy::FindNearestThreat`, over the player's tanks and squad units, against each garrison base's centre with radius = base radius + `AlarmMargin` (40 m), and against each group's members with `ContactRadius`.
   - It counts hostiles and sums a **threat score** with the training price table (cheap, already exists), keeping a per-kind count (infantry, light vehicle, heavy vehicle).
   - That is the attacker estimate. The async `LocationsUnderPlayerAttack` drops its score (`StrategicAIHelpers.cpp:1980-1994`) and only evaluates designer-set locations, so it is not relied on.
2. **Under fire:** any garrison-base building whose health dropped since the last poll (a health snapshot per building, polled; no delegate).

**Alarm rules:**

| Hostile in range? | Under fire? | Result |
|---|---|---|
| Yes | — | **Alarm** at the hostile centre. High if score ≥ `HighThreatScore`, else Low. |
| No | Yes | **Shelled.** No responders (do not run into artillery). The base's groups on duty there switch their loop away from that side. The strategic AI gets a "base shelled" note for counter-battery later. |
| No | No | Nothing. |

**Fog and stealth.** The hostile scan is omniscient, like the convoy's. It is limited to a short radius around the AI's own buildings and units, roughly what they could see. A player unit that is hidden or camouflaged by a game rule (if such a flag exists on the unit; check during phase 2) is skipped.

**Alarm record:**
- Fields: ID, base ID, location (the hostile centre, updated every poll), urgency, threat score, kind counts, first and last time.
- It **expires** after `AlarmExpireSeconds` (10 s, five polls) with no hostile in range. Polling keeps it fresh, so no flapping.

### 6.2 Who answers

**Candidates:** groups in `OnDuty` (any alarm) or `Returning` (High only) within `ResponseRadius` of the alarm. `ResponseRadius` = max(200 m, 1.5 × the distance to the nearest other base), so far-apart bases still help each other on High alarms.

**Sort:** by ETA (path-free estimate: straight distance / group speed).

**Strength:**
- Groups are added until the assigned strength (training price of members alive) ≥ `ResponseStrengthFactor` (1.5) × the threat score.
- Low alarms may send **at most** the base's own groups. High alarms may take from any base in range.
- **Never strip a base:** every other base keeps `MinOnDutyGroupsPerBase` (1).

**Kind match** (from the scan's counts): heavy vehicles present → vehicles and any squad with AT (Okhotnik/PTRS) first; plain rifle columns are sent only if nothing else is in range.

**Too strong to fight:** if the assigned strength cannot reach 0.75 × the threat, the garrison does not suicide piecemeal.
- The base's own groups go to the parade point and hold (`Withdrawing` → `Holding`).
- The alarm is forwarded to the strategic AI (§6.4).

**Orders:**
- Vehicles: attack-move to a nav-projected spread around the alarm.
- Columns: `TerminateMarchCommand`, then attack-move. Dropping the march animation makes the call-up readable.

### 6.3 Engagement, leash, stand-down

- A group in `Responding` switches to `Engaging` on contact or within 25 m of the alarm location.
- **Leash anchor:** the **base the alarm belongs to**, not the hit building: no member goes further than base radius + `LeashBeyondBase` (vehicles 80 m, infantry 50 m). This stops bait-and-kite: a scout that shoots and runs is chased only to just outside the walls.
- **Stand-down:** no hostile in the alarm's range for `StandDownSeconds` (12 s), then `Returning`.
- **Cooldown:** a group back from a Low alarm ignores Low alarms of **other bases** for 20 s. Its own base's alarms still pull it, so poking base A to pull its garrison and then hitting B fails, because B's own groups never left.
- **Losing:** below 40 % strength → `Withdrawing` to the parade point; the alarm is forwarded (§6.4).

### 6.4 Help from the strategic AI, and damaged vehicles

- A forwarded alarm (too strong, or the group losing) raises **defend pressure** on the training state. When direct control is on, it also calls the existing `DefendBase` sub-action path for that base with idle units.
- **Damaged garrison vehicles:** below the retreat threshold a patrol vehicle leaves its group for the base's parade point. Hazmat repair is borrowed only if an idle hazmat exists; the garrison never adds hazmats. It rejoins at full health (or is replaced if it dies).
- `RequestGarrisonCallUp(Location, Radius, Urgency)` (BlueprintCallable) is for missions. A decision-tree sub-action that calls the garrison up is phase 4.

---

## 7. How it fits the other systems

| System | Interaction |
|---|---|
| Decision tree / idle pool | Never sees garrison units (guard in `RegisterDirectControlUnit`). Gets units through release (§3). Gets forwarded alarms as defend pressure. |
| Training | Shares income through a fixed slice with its own reservation (§5); the total is unchanged. Its own spawn path. |
| Base builder | Later: bases, gates, parade point, roads and spawn doors from its plan. Until then everything is derived (§4.0). |
| Trenches | Patrol rings sit outside the trench arc. Trenches don't block nav, so a route crossing one is only a visual issue. |
| Unit ability AI | Allowed in Contact/Engaging, skipped on duty (§3). |
| Survival | Off by default. Its static base patrols can be adopted later with `AdoptUnitsIntoGarrison`. |
| Missions | `AEnemyGarrisonPost` pins bases, gates or routes; `RequestGarrisonCallUp`, `ReleaseGarrisonGroup`, `AdoptUnitsIntoGarrison`. |

---

## 8. Cost

Every 2 s:
- **Hostile scan:** player units × garrison bases (a dozen or so) plus groups. A few thousand 2D distance checks at most.
- **Health snapshot:** one per building. **Orders:** one redirect per member.

Route building uses `BuildNavigableRoute` once per edge, cached. No per-frame work, no new delegates.

---

## 9. Testing

`-EnemyGarrisonValidate` (map test subsystem, log `RTS_ENEMY_GARRISON_TEST`). The map has two Soviet bases 150 m apart, the player HQ 300 m away, and starting garrison points so tech/training timing does not gate the test. Checks:

1. **Columns (180 s window):** every column completes A→B, dwells, and starts back. Members stay within `MaxSlotError` of their slots outside pauses. While `OnDuty`, every member is in `IdMarch` and none ever reaches its march end (no turn-round).
2. **Patrols:** every vehicle visits ≥ 6 ring points per loop; none comes within `PlayerExclusionRadius` of the HQ or enters a perimeter; the radius jitter changes from loop to loop.
3. **Exclusivity:** with direct control on for 180 s, no garrison unit appears in `IdleDirectControlUnits`, a retreat group or a tree pick. The ability AI issues nothing to on-duty members.
4. **Call-up:**
   - Player tanks spawned at base A's wall raise an alarm within 4 s.
   - The responders match the kind (vehicles first); base B keeps ≥ 1 group on duty.
   - The tanks are removed and the responders are back on duty within 30 s.
5. **Leash:** a player scout **drives in, fires, and keeps driving away** (scripted). Responders chase but stop at base radius + leash and return.
6. **Contact on the march:**
   - A player sniper squad placed 60 m off the route fires on a column. The column goes `Contact` and attack-moves, or withdraws, and never re-marches while in combat.
   - After two lost squads the edge is suspended.
7. **Too strong:** a large player force at base A → A's groups hold at the parade point, the alarm is forwarded, and no group is fed in piecemeal.
8. **Funding:** the garrison budget never blocks a training reservation. A full garrison returns its overflow to training points.

Captures at the start, mid-march, on contact and during the call-up, for the look.

---

## 10. Phases

1. **Phase 1, columns that look right and stay ours:**
   - the garrison set plus the guards (§3);
   - base derivation with stable IDs (§4.0);
   - columns on convoy machinery with Contact handling;
   - perimeter loops;
   - `StartingGarrisonPoints` spawning;
   - tests 1, 3 and 6 (without suspension).
2. **Phase 2, patrols and call-up:** vehicle loops; polled alarms (hostile scan + health snapshot); response selection, leash, stand-down, cooldown, the too-strong rule, forwarding; tests 2, 4, 5 and 7.
3. **Phase 3, upkeep:** garrison budget slice with replenishment and overflow; merging, hold timeout and release; damaged vehicles; edge suspension; adoption; test 8.
4. **Phase 4, polish and integration:** builder bases, gates, roads and doors; `AEnemyGarrisonPost`; probes; a decision-tree call-up sub-action; survival adoption; and, if more systems need it, the full ownership registry.

---

## 11. Decisions for you

| # | Question | Recommendation |
|---|---|---|
| G1 | Garrison funding | **A fixed slice of training income with its own reservation**, overflow back to training. |
| G2 | May the decision tree take garrison units? | **Only through release** (hold timeout, no bases, mission). Never by idle picking. |
| G3 | On by default per mode? | Off in the campaign unless the map enables it; on in skirmish; off in survival for now. |
| G4 | Start time | At BeginPlay (bases look manned at once), with an opt-in to wait for the strategic AI's trigger. |
| G5 | Bases far apart | No columns between bases further than 400 m apart unless a road covers them; perimeter loops instead, logged. High alarms still pull help from up to 1.5 × the nearest-base distance. |
| G6 | Omniscient hostile scan near its own buildings | Accept for now (same as convoys); skip stealthed units if a flag exists. |

---

## Review log (v1 → v2)

An adversarial review against the code found these problems in v1. Each is now addressed:

| Finding | v1 | v2 |
|---|---|---|
| A squad drops its march on any shot (`SquadController.cpp:635`) | Not modelled; the stuck rule would re-march a fighting squad, so it yo-yoed and could be sniped | `Contact` state, convoy fight guard, attack-move-or-withdraw, no stuck checks in combat (§4.1) |
| `RedirectMarch` legs turn round at each end; double completion aborts | Legs with arrival polling | Convoy look-ahead target on a `FConvoyRoute`; the end is never reached (§4.1) |
| The convoy already solves column marching | Cited only for the leash | It is the column engine (§1, §4.1) |
| Registry guarded the wrong door; missed async retreat groups and unit ability AI; retreat "borrow" could never fire | Big registry refactor | `TSet` + guard in `RegisterDirectControlUnit`, retreat exclusion, ability AI rule; own damaged-vehicle handling (§3, §6.4) |
| "Rich" async alarm data doesn't exist (no score, no composition; designer-only locations; 60 m centroid test every 25 s) | Relied on it | Polled per-base hostile scan with price-based score and kind counts; health snapshot for "under fire" (§6.1) |
| The combat-edge event re-fires, fires on own shots and on any damage | New delegate | Dropped; polling (§6.1) |
| No training hook; the floor starved the army; one reservation blocks all | Tagged training batches with priority floor | Separate budget slice and reservation, own spawn path with captured IDs (§5) |
| Bait base A, hit B; leash anchored on the shelled building; same-route ambush farming; follower chains with no timeout; Depleted forever; base IDs churn; late start | Holes | Per-base cooldown rule, base-anchored leash, edge suspension, convoy slots with wait cap, hold timeout and release, building-matched IDs, start at BeginPlay (§2, §4, §6) |
| Distant bases silently get nothing | 250 m caps | 400 m / road-covered edges, perimeter loops, logged; response radius scales with base spacing (§4.1, §6.2) |
| Tests didn't test what they named (90 s too short; the scout never got chased) | — | 180 s window, scripted kiting scout, contact and too-strong tests (§9) |
