# Enemy base building: plan (brainstorm, 2026-11-16)

Goal: the enemy AI (player 2, Soviet) builds its own base from nothing, so a near-empty skirmish map works. It picks a site, lays out a base that looks designed, buys buildings with **building points**, and raises them with smoke and a rise-from-the-ground animation. What it has built unlocks its training tiers. Field constructions (unit-built) are out of scope and stay as they are.

No code yet. This doc maps what exists, where building plugs in, a phased plan, and the decisions that are yours.

---

## 1. What we already have, and where building plugs in

| System | What it does today | How the builder uses it |
|---|---|---|
| `AEnemyController` components | Formation, Wave, FieldConstruction, NavigationAI, StrategicAI, Retreat, DirectControl, GlobalAbilities, UnitAbilityAI, BaseTrench. No tick; each has its own timer. | Add a sibling: **`UEnemyBaseBuilderComponent`**, on its own timer, modelled on `UEnemyBaseTrenchComponent`. |
| Strategic loop | One 6 s timer (`StrategicAiThinkingLoop`) polls sub-timers, runs the tree, runs `Training_ThinkStep`, flushes async requests. | The builder starts from `StartStrategicAIThinkingTimer`, next to `StartBaseTrenchDigging` (`EnemyStrategicAIComponent.cpp:1471`). It is gated on `GetIsAllowedBuilding()`. |
| Async worker (`FGetAsyncTarget`) | Off-thread. It only sees unit snapshots (`FAsyncDetailedUnitState`) and the base-cluster cache. Results land on the blackboard. | It gets no new work. Traces and overlaps need the world, so they stay on the game thread, spread over frames (see §4). The builder **reads** the async results already on the blackboard. |
| Blackboard | `EnemyBasePoints`, `CurrentBaseDefensePositions`, `CurrentPlayerUnitCounts` (incl. `PlayerHQLocation`), `CurrentLocationsUnderPlayerAttack`, `CurrentPlayerUnitBulkLocations`, `CurrentEnemyBasesInCombat`, … | Threat side = player HQ, then player unit bulks, then locations under attack. These order the defence slots. |
| Base clusters (`FFindEnemyBaseClusters`) | Finds the AI's bases from its buildings. It needs configured core types and **≥ 2 production buildings**, and refreshes about every 12 s plus snapshot lag. | It is empty when the AI starts from nothing. The **builder owns its base record** and publishes it (see §5, "Bootstrap"). Cluster search keeps working once buildings stand. |
| Stochastic decision tree | Attack/Defend actions → sub-actions that spend idle units. Only runs with direct control on. | Building is **not** a tree action (it needs no units and must run without direct control). The tree hooks in two ways: training pressure drives build pressure, and a new `HasBuiltBuilding` requirement lets designers gate actions (§6). |
| Training | Points over time (`TrainingPointsStart`, `TrainingPointsPerMinute`) → pressure buckets → batch → async spawn at a trainer or at a random base point. | **Building points** copy this exactly (§3). Trained units spawn **at the building that makes them** (§6). |
| Tech (`TrainingRequirements_ThinkStep`) | Counts the AI's buildings by type: Barracks → BasicInfantry; Factory / CoolingTowers → LightTanks; ResearchCenter → Tier2. | It is the natural goal of the build order. Two fixes are needed (§2). |
| Trench component | Free, capped trenches out of pit anchors and on an arc toward the player HQ. It has `AddFortifiedBase(Centre, Radius, ThreatDir)`. | The builder calls `AddFortifiedBase` with its **plan's perimeter**, so trenches fit the final base rather than the first two buildings. |
| Survival Soviet bases | `FSurvivalSovietSiteMap` (terrain grid, flatness, slope, drivable flood fill, approaches, overlooks), `FSurvivalSovietBaseLayoutRun` / `BuildPlan` (archetype master plan → streets, plots, perimeter, gates, emplacements, pads, dressing) on `FEnemyBaseSetupBuilder` + `FEnemyOccupancyGrid`. Spawns everything **finished** at match start. | This is the "looks designed" engine, and it already runs at runtime. The builder **draws the whole base once** with it, then **builds it slot by slot over time** (§5). |
| BXP construction | Every Soviet building is an `ABuildingExpansion` BP. `StartExpansionConstructionAtLocation` works with no owner (every owner call is null-guarded). It plays the construction material swap plus random `NS_ConstructionSmokeRound`, or a VAT deploy if `DeployAnimationData` is set. | Add a third construction look, **Rise**, as a reusable component (§7). |
| Player placement checks | `ACPPConstructionPreview`: slope (corner traces, 5°), plane fit (5×5 landscape traces, ≤ 5°, ≤ 75 cm off the plane), grid overlap on `COLLISION_OBJ_BUILDING_PLACEMENT`. Private to the cursor-driven preview. | Pull the plane fit and the placement-channel box overlap into a **shared static helper** (§4). The preview, the trench component and the builder all use it. |

---

## 2. Fixes needed before anything else (small, needed by any design)

**Status 2026-11-16:** 1, 2 (the mechanism; the D4 mapping is still open) and 3 are done in code. 4 is checked; see below.

- **1, done.** `ABuildingExpansion::GetIsStanding()` is false while a BXP is placing, being built, unpacking, packed up or looking to unpack. `GetPlayerBxpCountsByType` counts only standing BXPs. Expansions placed in a level or spawned finished (survival) never pass through an owner, so they keep their initial status (`BXS_NotUnlocked`). They count as standing, so existing maps unlock as before.
- **2, done.** `TypesUnlockingThisLevel` is `EditAnywhere`, and there is a new `bRequireAllTypesUnlockingThisLevel` (all of / any of). An empty list still never unlocks. The C++ defaults are unchanged; the upper tiers stay empty until D4 is decided.
- **3, done.** `URTSComponent::ChangeOwningPlayer` now moves a BXP that has begun play between the player and enemy lists (`AddBxpForPlayer` already de-duplicates). Aircraft are still not moved there; that is a separate gap.
- **4, checked.** No `.umap` and not `RTS_BP_EnemyController` contain `bAllowBuilding`, and the survival coordinator does not set it. So **base trenches never dig outside their test**, including survival, where the trench doc says they should start in the grace period.

1. **A building still under construction unlocks its tier.** `UGameUnitManager::GetPlayerBxpCountsByType` (`GameUnitManager.cpp:1581`) counts every alive BXP with no `BXS_Built` filter, and BXPs register in `URTSComponent::BeginPlay`. Add an "only built" filter (default on for the AI's tech step). Otherwise an AI barracks unlocks infantry the second it starts rising.
2. **The upper tiers can never unlock.** MediumTanks, AdvancedInfantry, Tier3 and Experimental have empty `TypesUnlockingThisLevel`, and the field is `VisibleAnywhere`. Make the ladder data-driven (a data asset or `EditAnywhere` on the controller), and fill it. See decision D4.
3. **Ownership at spawn.** `ChangeOwningPlayer` does not move a BXP between the player/enemy lists. The builder spawns **deferred** with owning player 2 set before `FinishSpawning` (or relies on BP-default owner 2, which the Rus BPs have).
4. **`bAllowBuilding`** (`FEnemyAIMissionSettings`) defaults to false, and the survival coordinator does not set it. Trenches may be off in survival today; worth checking on its own. The builder gets its own flag (D9).

---

## 3. Building points (economy)

A twin of training points, in its own small struct (`FEnemyBuildingEconomy`) on the builder:

- `BuildingPointsStart`, `BuildingPointsPerMinute` (with an optional curve over match time, like survival's training curve), and an API `AddBuildingPoints(Amount, EBuildingPointSource)`. Sources today are `Time` and `Mission` (scripted grants). Later `Harvest` goes through the same call, so harvesting changes nothing else.
- **Costs** per `EBuildingExpansionType` in a data asset (not hardcoded like `EnemyTrainingHelpers`). Defences can start from survival's existing `ThreatPoints` (guard tower 6 … B-4 45) so balance is shared. Production and support get their own numbers.
- **Reservation**, exactly like the training reservation: once the next build is picked it is held until it can be paid for, so the AI saves up for a research centre instead of spamming sandbag towers.
- **Concurrency cap:** at most N buildings rising at once (1 early, 2 later). This paces the base visually and gives the player a window to raid.
- **Build time** per type in the same table, times a difficulty factor.

Separate pools for training and building are recommended (D1): you can tune "how fast the base grows" and "how big the army is" apart, and maps without resources still work.

---

## 4. Footprint checking: finding flat ground

Two levels, because a plan is drawn once and a building is placed minutes later.

**Plan time (once per base): `FSurvivalSovietSiteMap`.** It traces terrain over frames on a 400 cm grid to ~5000 cm past the build area. `GetIsFootprintBuildable` takes 3×3 samples per footprint and requires slope ≤ tan 4° and step ≤ 45 cm. The layout already uses it as the occupancy block predicate. Footprints come from **colliding components only** (not actor bounds: Rus BPs carry big non-colliding spheres; see the actor-bounds seating trap).

**Build time (every slot, just before spawning): `FRTSFootprintGroundCheck`,** a new shared static helper extracted from the player preview:
- **Plane fit:** 5×5 landscape traces under the oriented footprint, least-squares plane, slope ≤ limit, every sample within tolerance of the plane. This is the preview's `IsFieldDefenseInclineValid`, made a free function. It also returns the plane height, which is used to seat the building.
- **Blocking:** an oriented box overlap on `COLLISION_OBJ_BUILDING_PLACEMENT` (and WorldStatic/WorldDynamic). It catches player buildings, wrecks, props and trenches that appeared since the plan.
- **Units on the spot:**
  - The AI's own units are ordered off (the `FMoveUnitsFromConstruction` idea) and the slot retries next step.
  - Player units block the slot; it retries later.
- **Navigation:** `GetLocationProjected_WithNavSystem` on the door/apron point. A building whose door is not reachable is skipped.
- The player preview and `UEnemyBaseTrenchComponent` switch to the same helper, so one check rules all three.

**When a slot fails:**
- It is a temporary block (units) → retry with backoff.
- It is a permanent block (a wreck, a player building, the ground changed) → ask the plan's occupancy for a replacement spot in the same zone (`PlaceCentered` with that footprint), then any zone, then drop the slot. This mirrors what survival does at layout time ("moves to any plot, then any free ground").

**Cost:** about 25 traces plus one overlap per building attempt, a few times a minute. It runs on the game thread and needs no async work. The site map's traces are already spread over frames.

---

## 5. Base layout that looks designed: plan once, build over time

The key idea: **an incremental builder should not choose spots one at a time.** Greedy placement gives clutter and walled-in buildings. Instead:

1. **Pick a site** (§5.1) and an **archetype** (Firebase, Cantonment, Tank Park, Supply Depot, Hill Stronghold, Command Fortress, Airstrip). Use the survival preset whose `MinProductionTier` and strength fit the difficulty, or a per-map override.
2. **Draw the final base** with `FSurvivalSovietBaseLayoutRun` at the difficulty's *maximum* size. It returns streets, plots per zone, perimeter line + gates, emplacement spots (facing the threat side), pads, yards and lanterns. **Nothing spawns.** The output becomes a list of **slots** (type, transform, footprint, zone, stage, roads it needs).
3. **Build slots over time** in an order that reads as a camp growing into a base:
   - **Core first:** the barracks (or a large tent) at the hub. **Not the Admin Bunker:** per `50_SovietBases.md` it counts as `BTX_RusResearchCenter` and would unlock Tier2 in minute one. It belongs later, as a Tier2 building.
   - **Streets with their buildings:** a road piece to a plot is laid when the first building on that plot starts (road decals are cheap and look like tracks being worn in).
   - **Production by tech need** (§6), in its zoned plots.
   - **Support fill** (storage, fuel, tents, megaphone): cheap and frequent, so the base looks lived-in between big builds.
   - **Defences on the threatened side first:** emplacement slots sorted by angle to the threat (player HQ, then player bulks, then the last locations under attack).
   - **Perimeter last, in segments:** wall/fence runs bought per span, threat side first, gates left open until their span is done.
   - **Pads and dressing appear with each building** (survival's `ASurvivalSovietBasePad` and foundation dressing per class), not all up front.
4. **Unbuilt slots stay empty ground.** Because the plan reserved streets first, the base stays drivable at every stage.
5. **Ladder slots the archetype lacks are added.** The survival preset library only knows Barracks, LargeTent, CoolingTowers, Factory, ResearchCenter and AdminBunker as production (`SovietBasePresetLibrary.cpp:222-228`), and the min-tech-0 archetypes (Firebase, Cantonment, Airstrip) may carry no factory or research at all. After drawing the plan, the builder checks the tech ladder. For each ladder building with no slot (e.g. `Rus_Building_PlatformFactory`, `Rus_Building_AssemblyYard`), it places one in the matching zone through the plan's own occupancy (`PlaceCentered`), before anything is built.

This reuses everything that already makes survival bases look designed: one wall family, zoning, gate on the open approach, bunkers only outside, pads that merge into yards. You only author the **growth order**.

### 5.1 Site selection

- **Phase 1:** a designer gives the site. `FEnemyAIMissionSettings::StartBuildLocations` already exists and is unused; use it (location + optional archetype + radius). A placed `AEnemyBaseSiteMarker` actor is nicer to edit; either works.
- **Phase 3:** automatic search for a near-empty skirmish map:
  - **Candidates:** points on a coarse grid over the map, filtered to the enemy's half (far from the player HQ, near the AI's start or existing units).
  - **Cheap pass:** a coarse site map per candidate (large cells, few traces), spread over frames. Score buildable-area fraction, distance band from the player HQ, approach openness (defendable: few open approaches), and later distance to resource fields.
  - **Best candidate:** gets the full-resolution site map and the plan.

### 5.2 Bootstrap: the AI has no "base" until it has buildings

Cluster search needs ≥ 2 production buildings, so with zero buildings the blackboard has no base. Training would then have no spawn point and trenches no base. Fix: **the builder owns `FEnemyBuiltBase` records** (centre, perimeter radius, threat direction, slots, built buildings) and publishes them:
- It calls trenches' `AddFortifiedBase(Centre, PlanPerimeterRadius, ThreatDir)`. Trenches then fit the final perimeter rather than the first buildings, and the trench arc no longer freezes too small.
  - Double-base risk: a cluster base later found within 3000 cm of a designer base is ignored (`UpdateBasesFromStrategicAI` → `FindBaseNear`), but a big plan's building centroid can drift further than that from the plan centre. So the trench component should skip any cluster base whose centre lies inside a builder base's perimeter.
- It provides a training spawn transform (§6), so `GetValidTrainingSpawnTransform` stops relying on clusters.
- **Its records are merged into the blackboard, not written beside it.** `ProcessEnemyBaseClusterResults` overwrites `EnemyBasePoints` and `CurrentBaseDefensePositions` wholesale every ~12 s, so a separate write would be wiped. That handler appends the builder's bases and its emplacement and gate positions after each cluster result. Retreat, mines, bases-in-combat and "new unit moves to a defence position" then work from the first building on, not only after the cluster search accepts the base.
- It still adds its building types to `FindEnemyBase_TimerRequest.CoreBuildingTypes`, so a base the builder did not plan (a mission's pre-placed one) is still found.

### 5.3 Roads: the AI's own procedural spline network

The AI keeps a **road graph** per base (nodes and edges in world space) and lays it as spline roads as the base grows. It is its own system (`FEnemyRoadNetwork` + a small spawner on the builder), not survival's one-shot road pass. It reuses survival's pieces where they fit.

**The graph**
- **Trunk:** the plan's streets (`FSurvivalSovietPlannedRoad`: Main / Lane / Track point chains, already reserved in the occupancy so no building lands on them) become the first edges. They are not laid yet; they are only *planned*.
- **Spurs:** each building slot gets a spur from its door/apron point (the plan knows `DoorDirection` and `ApronLength`) to the nearest trunk node.
  - It is routed with `FSurvivalSovietSiteMap::FindDrivablePath` (drivable grid, avoids slopes and steps).
  - The occupancy marks it `Reserved`, so later buildings keep clear of it.
- **Links between bases and to the front (phase 3):**
  - an edge from each base's main gate to the next base;
  - an edge toward the player side, which waves and convoys can follow.
- **Shaping:** each edge is a grid path. The pipeline is: Douglas-Peucker simplify (survival's `SimplifyRoadPoints`), then a Catmull-Rom resample, then a minimum turn radius per road kind. This keeps a track from zig-zagging along grid cells.

**Laying it, and growing it**
- An edge is **laid when the first building that needs it starts rising**, never all up front: the trunk piece to that plot, then the building's spur. The road appears with the building.
- **Surface by age and tier:**
  - New edges start as `BP_GroundPathSpline_TwoTrack` (worn dirt ruts).
  - When the base reaches a tier (or an edge carries N buildings), its spline actor is swapped for the next class: `BP_GroundPathSpline_Road` (macadam) for main streets, a gravel lane class for lanes.
  - Swap means: same points, new class, old actor destroyed under a dust burst.
- **Joins:**
  - A road junction overlay (`BP_GroundPath_RoadJunction` / `_Tee`, skew in custom primitive data 0 for 70-110° crossings) goes where an edge meets another at a node.
  - The path blend decals (`MI_PathDecal_RutJunction`, `RutFade`, `FootpathToTwoTrack`) go at spur ends, surface changes and dead ends.
- **Splines:** spawned like survival's `SpawnRoad` → `ApplyRoadToSpline`, one actor per edge, points projected on the landscape.
  - Gotcha (from the ground-path work): the BP snaps tile ends by tracing down from the spline, so points must sit on the ground, not below it, or the road dives under a hill.
  - Gotcha: a spline set from code must be flagged as edited, or construction resets it to the default spline.

**Gameplay**
- Roads have no collision and do not block placement or navigation.
- Optional (phase 2+): a cheaper nav area on laid roads, so the AI's trucks and tanks prefer them. This is the visual tell that the base is used.
- Destroyed buildings leave their spur; it is reused when the slot is rebuilt.

**Cost:** one spline actor per edge, tiles of ~2.2 m (one draw per tile per pass, per the road's measurements). A base of 20-30 edges stays well inside what the Ore and Trains maps already carry. Spawning is batched per think step like everything else in the builder.

### 5.4 Destroyed buildings

Each slot keeps its state: `Planned → Reserved → Rising → Built → Destroyed`. A destroyed slot goes back into the queue after a cooldown, at full cost, at most N rebuilds (D5). A building destroyed while rising refunds nothing, so raiding construction pays off for the player. Losing the last building of a type removes its tier (that already works), which raises its build pressure at once.

---

## 6. Choosing what to build: priority scoring

Each think step (about 5 s) scores every buildable slot and takes the best one into the reservation. The score is a sum of a few signals, so it stays tunable:

| Signal | Source | Effect |
|---|---|---|
| **Opening** | Data: an ordered list per archetype/difficulty (e.g. Barracks, Storage, Factory, Fuel, ResearchCenter) | Forces the first few builds. Done when the list is built. |
| **Tech need** | `FEnemyStrategicTrainingState` buckets: when the picked focus has no unlocked options, its tier is locked | Big bonus to the building that unlocks that tier (through the tech-ladder data). This is the link between the decision tree and building: sub-actions push training pressure, training pressure pushes buildings. |
| **Threat** | Blackboard: `PlayerHQLocation`, `CurrentPlayerUnitBulkLocations`, `CurrentLocationsUnderPlayerAttack`, `CurrentEnemyBasesInCombat` | Defence slots facing the threat get a bonus; a base in combat raises all defence slots and pauses support fill. |
| **Balance** | Counts of built buildings per category vs. a target mix per archetype | Keeps the base from becoming all bunkers. |
| **Rebuild** | Destroyed slots | Bonus after the cooldown. |
| **Cost/time** | Points needed vs. income | A tie-breaker that prefers what can be afforded soon. |

Picks use a weighted random over the top few (as the tree and training do, seeded), so two matches don't grow identically.

**Hooks into the existing AI:**
- **Training spawns from buildings.**
  - `GetValidTrainingSpawnTransform` asks the builder first: infantry at a built barracks door, vehicles at a factory apron (the plan already knows door direction and apron), aircraft at the airstrip.
  - It falls back to today's behaviour.
  - Optionally a building plays its Blueprint "unit out" event.
- **Decision tree requirement:** `UStrategicAIRequirement_HasBuiltBuilding` (type, count), so a designer can gate e.g. "Heavy push" behind a research centre being up.
- **Defend base:** the plan's emplacement and gate positions are also fed into `CurrentBaseDefensePositions`, so `DefendBase` parks tanks where the base was designed to be defended.
- **Waves owned by buildings** (`Wave_OwningBuilding*`) can point at AI-built buildings later, once missions want it.

---

## 7. Construction look: "rise + smoke" as one reusable component

**`UBuildingRiseConstructionComponent`** (on `ABuildingExpansion`, opt-in; works for any building or BXP):
- **At start:**
  - Collision, nav modifier and the placement-channel blocker sit at the **final pose** at once, so units don't walk into the plot and the player can't build on it.
  - Health starts low and scales up with progress (D6).
- **The animation itself:**
  - Only the visuals move: the building mesh and visual children start sunk by their height plus a margin and rise with an ease-out curve. A small jitter and tilt settle at the end.
  - It works on any mesh with no material edits.
  - The ground hides the sunk part from the RTS camera.
  - Optional, for the Rus buildings that have them: swap to the staged meshes (`SM_Rus_WarFactoryAt33/66`, `SM_Rus_Barrack_At50`, `SM_Rus_IndustrialCoolingAt33/66`, `SM_Rus_ResearchCenter_At33/66`, `SM_Rus_FactoryPlatformAt33/66`) at 33%/66% for a scaffold look.
- **Smoke:**
  - It reuses existing assets: the "Smoke Screen" bursts from the nomadic deploy at start and end, a dust ring at the footprint corners (`NS_ConstructionSmokeRound`), and low continuous dust while rising.
  - Burst points are data on the component, like `NomadicDeployAnimationData`.
  - Known issue to avoid: game-wide `NS_DestructionSmoke` sprites clip into the ground (open item from the nomadic deploy work). Pick or tune the asset with that in mind.
- **Finish:** it calls the BXP's normal finish (`OnFinishedExpansionConstruction`): turrets spawn and auto-engage, attachments appear, `OnBxpConstructed` fires, the tier unlocks (with fix §2.1).
- **Owner-less construction, verified so far:** every `M_Owner` call in `SetStatusAndPropagateToOwner` / `OnNewConstructionLocationPropagateStatus` is null-guarded. **Not yet read:**
  - the status of a freshly spawned BXP (anything but `BXS_LookingForPlacement` takes the *unpack* branch of `StartExpansionConstructionAtLocation`);
  - where `M_BuildingTime` is set for an owner-less BXP;
  - the side effects of `bM_IsFieldDefense` if the field-defence path is borrowed.
- **Driving progress:** the field-defence path (`StartFieldDefenseConstruction` / `Sync…` / `Finish…`) already lets a caller drive progress with no owner. A third mode, `Rise`, sits next to the material and VAT modes, chosen by the component's presence.
- **Things to check when building it:**
  - `OnFinishedExpansionConstruction` returns early when there is no progress bar widget. Confirm the Rus BPs have one, or let Rise mode finish without it.
  - `ApplyExtraResourceStorage` reports an error when there is no owner drop-off. Skip it for enemy-built buildings.
  - Custom primitive data 0–3 (deploy) and 4–7 (team colour) are taken. The Rise animation uses relative transforms, so it needs none.

Because it is a component on the BXP, the player can use it too (e.g. for buildings without a VAT deploy). That is a free side benefit, not a goal.

---

## 8. Phased plan

**Phase 0: foundations (small, testable alone)**
- §2 fixes: built-only tech count, data-driven tech ladder, deferred spawn with owner 2.
- `FRTSFootprintGroundCheck` extracted. The preview and the trench component switch to it, with no behaviour change; prove it with the existing trench test and placement map tests.
- Building cost and time data asset.
- The shared Soviet base helper pulled out of `USurvivalSovietBaseComponent` (site tracing, footprint measuring, class resolver, pads/roads/dressing spawn; see D3). Survival keeps passing its preview tests.

**Phase 1: one seeded base, tech ladder, the look**
- `UEnemyBaseBuilderComponent`: building points, reservation, concurrency cap, slot state machine.
- Site from `StartBuildLocations` (or a marker); plan through `FSurvivalSovietBaseLayoutRun` with a fixed archetype; core-first growth order; opening list.
- `UBuildingRiseConstructionComponent` on the Rus production buildings.
- Training spawns at the AI's buildings; base published to trenches and the blackboard.
- **Test:** a headless map test on a near-empty map (like the trench test on UT_Nomadics), log tag `RTS_ENEMY_BUILD_TEST`. Checks:
  - Barracks → infantry trained → Factory → light tanks → Research → Tier2, in order.
  - Every building sits within its slope and step limits.
  - No building overlaps another.
  - Every door is reachable on the navmesh.
  - Points are never negative.
  - Captures at 2, 5 and 10 minutes for the look.

**Phase 2: reactive building**
- Tech-need, threat and balance scoring; defences toward the threat; perimeter segments; rebuild with cooldown.
- The `HasBuiltBuilding` tree requirement; emplacements fed into defence positions.
- **Test:** the player HQ placed on one side → defences grow on that side first; destroy the barracks → the infantry tier drops, then is rebuilt.

**Phase 3: skirmish from scratch**
- Automatic site search, archetype choice from the site (hilly → Hill Stronghold, flat open → Tank Park or Cantonment, etc.).
- A second base or forward outposts later in the match.
- A skirmish test map: an almost empty landscape, AI start point only; a 20-minute watch run with periodic captures.

**Phase 4: harvesting (later)** (decisions picked 2026-11-24, see [EnemyHarvestEconomy.md](EnemyHarvestEconomy.md))
- AI harvesters feed `AddBuildingPoints(…, Harvest)` (and training).
- The site score gains "near resources"; refinery and storage slots become economy rather than dressing.

---

## 9. Decisions for you (with my recommendation)

| # | Question | Recommendation |
|---|---|---|
| D1 | Separate building-point and training-point pools, or one pool? | **Separate.** Two knobs, and maps without resources still work. Harvesting later feeds both through the same `Add…` calls. |
| D2 | Do buildings rise by themselves, or does a builder unit (truck/engineers) have to go there? | **By themselves** for now (like trenches). A builder unit could come later and is a nice raid target. |
| D3 | Base plans: reuse the survival archetypes and data asset, or a new set for the campaign/skirmish AI? | **Reuse.** They are the designed-looking part and already tested. The pure layout (`FSurvivalSovietLayoutInput` → `FSurvivalSovietBaseLayoutRun`) needs no director. What does need extracting in phase 1 lives in `USurvivalSovietBaseComponent` (17 director references there): the time-sliced terrain tracing into the site map, footprint measuring, the `ResolveEntry` class resolver, and pad/road/dressing spawning. Pull those into a shared helper both use; leave the rest where it is. |
| D4 | Tech ladder for the empty tiers? | Constraint: the Admin Bunker already unlocks Tier2 (it is a `BTX_RusResearchCenter`). Proposal: MediumTanks ← PlatformFactory; AdvancedInfantry ← a second Barracks-class building, or Barracks + AmmoStorage; Tier3 ← ResearchCenter **and** AssemblyYard; Experimental ← AssemblyYard + MegaBunker. The tier rule needs "all of" as well as "any of". This is your design call. |
| D5 | Rebuild destroyed buildings? | **Yes**, after a cooldown, at full cost, at most 2 rebuilds per slot. No refund for buildings lost while rising. |
| D6 | Health while rising? | **Scales from 25% to 100% with progress**, so raiding construction is rewarded but not trivial. |
| D7 | What does a skirmish AI start with? | **Nothing but a site and a starting points pot** that buys the first barracks in the first minute. A Soviet HQ truck does not exist (`Nomadic_RusHq` has no BP); it is a later option. |
| D8 | Walls and fences: buy them with points, or free? | **Bought in segments, late**, threat side first. Free walls make every base a fortress at minute 3. |
| D9 | Where is the builder on? | **Its own flag** (`bAllowBaseBuilding`), not `bAllowBuilding`: that one already gates trenches, so a campaign map with trenches would also start building. The builder also stays inert without a site. On for skirmish, off in the campaign unless a mission turns it on; survival is your call (survival bases spawn finished today). |

---

## 10. Risks

- **Navmesh is dynamic-modifiers-only.** Buildings rising mid-match must update nav. Survival does this already with nav modifiers; the test must path to every door after each build.
- **Snapshot lag:** a new building reaches the async snapshot after up to ~9 s and the cluster search after ~20 s. The builder's own base record covers this.
- **Rus BP quirks:** big non-colliding spheres and hidden components at the origin break bounds-based footprints. Use the survival footprint measurement (colliding components only).
- **Shipping:** runtime PCG is not needed. The builder calls the layout code directly (`BuildPlan`/`RunStage`), not the PCG graph, which avoids the unproven cooked runtime PCG path.
