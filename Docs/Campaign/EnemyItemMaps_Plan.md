# Enemy item maps: handmade maps + spawned enemy start (plan, 2026-11-24)

Direction from the owner (2026-11-24): attacking an enemy item on the world map (airfield, factory, ...) no longer
opens a procedurally generated map. Instead:

- a set of **handmade maps** is used for enemy item operations;
- the list is **shuffled once per campaign** (campaign seed) and an operation picks the map at an index;
- each map has its own **hand-placed enemy base**; at load the **signature buildings of the item type** (for an
  airfield: hangars, strip, aircraft) are spawned into it at the enemy player's start location;
- the enemy is played by the **skirmish AI** (still to be built).

The procedural map system stays in the project untouched; it is only no longer the default source for enemy items.

Related: [WorldMapCampaignDesign.md](WorldMapCampaignDesign.md) (section 10 launch path),
[EnemyBaseBuilding_Plan.md](../EnemyAI/EnemyBaseBuilding_Plan.md), [EnemyHarvestEconomy.md](../EnemyAI/EnemyHarvestEconomy.md),
[50_SovietBases.md](../../RTS_Survival/SurvivalMode/Docs/50_SovietBases.md).

## 1. What already exists (read from code 2026-11-24, nothing run)

**The shuffled map pool is already built.** Only its contents and its meaning change.

| Part | Where | State |
|---|---|---|
| Map pool | `FWorldOperationMapSettings::EnemyObjectProceduralMaps` (`WorldPlayerController.h:58`), Blueprint default on the world controller | Exists. Contents not checked (editor bridge was down); the only map in `Maps/Campaign/EnemyObjProcedural/` is `EnemyObjProcedural_0`. |
| Shuffle | `AWorldPlayerController::InitializeOperationMapsForCampaignSeed` (`WorldPlayerController.cpp:1038`) | Exists: `DeterministicShuffle` with `FRandomStream(seed + 23017)`, recomputed on every world map load. |
| Index | `FWorldCampaignState::EnemyObjectProceduralMapIndex` (`FWorldCampaignState.h:154`), read in `TryGetEnemyMap` (`.cpp:1251`) | Exists, saved. **One global cursor**: it advances on every enemy launch (items and divisions) and wraps. |
| Item type to the level | `FWorldMissionContext::EnemyObjectType`, `OperationSeed`, `StrengthContributions` | Carried. Only the PCG node `PCGWorldMissionContextData` reads the type and the summed strength. No C++ reads `OperationSeed` or the per-entry fortifications. |
| Divisions and convoys | `LaunchOperationAgainstDivision` (`.cpp:918`) | Same pool and cursor; a proxy item type (convoy = SupplyDepo, infantry = Barracks, rest = Factory). |
| Result back to the map | `AMissionManager::TriggerVictory`, `UW_MissionEnd` WITHDRAW | Exists. `EnemyObjProcedural_0` holds a `BP_MissionManager`, so its objective is a Blueprint (not checked which). |
| Player start | `ACPPController::InitPlayerController(SpawnCenter)` | Works on any map at a fixed spawn centre. Start picking (`UPlayerStartLocationManager`) needs a `ARTSLandscapeDivider`, so it is procedural-only. |
| Enemy start | `AEnemyController::OverrideEnemyStartPosition` (aircraft only), `PresetRegion_EnemyStart` (procedural only) | **Missing** for handmade maps: no marker actor, no spawner. |
| Base presets | `USurvivalSovietBaseComponent::GenerateSovietBaseAt` + 7 archetypes (`SurvivalSovietBaseTypes.h:65`) | Exists, builds finished garrisoned bases as player 2, but is welded to the survival director and `ASurvivalStartLocation`. Layout input `FSurvivalSovietLayoutInput` is pure. |
| Enemy AI | `AEnemyController`, strategic AI (6 s think, Attack/Defend tree), training at Soviet buildings, waves, trenches, retreat, global abilities | Plays a **prebuilt** base: trains from standing buildings and attacks or defends. Without a decision tree it trains nothing. |
| Skirmish AI | `EnemyBaseBuilding_Plan.md` phases 0-3, `EnemyHarvestEconomy.md` | **Designed, no code.** Building points are collected but never spent; enemy drop-offs are not supported. |

## 2. Decisions (owner, 2026-11-24)

- **D1. Which map an item gets: next map per launch, same map on a retry.** The global cursor stays: attacking a
  new item takes the next map in the shuffled list. The map an item got is remembered, so attacking that same item
  again (after WITHDRAW or a defeat) opens the same map and does not move the cursor. (Restarting a lost mission
  from the end screen already reloads the same level.)
- **D2. Enemy start: hand-placed base per map.** Each handmade map has its own hand-placed enemy base. The item type
  only adds its **signature buildings** into that base (airfield: hangars, strip, aircraft; factory: halls).
- **D3. Divisions and convoys: separate field pool, army at the enemy start.** They get their own, smaller list of
  field-battle maps with no base. The division or convoy spawns as an army at the enemy start marker and the AI plays
  it.
- **D4. Victory: destroy the item's signature buildings.** The rest of the base may still stand.
- **D5. EnemyHQ and EnemyWall get their own story maps (picked 2026-11-24).** They never use the handmade pool. The
  world map routes them to fixed levels (built, section 5c); those levels (wall mission, Ural finale) do not exist
  yet.

## 3. What is missing

1. **Two pool switches.** On `FWorldOperationMapSettings`: a handmade **item pool** and a handmade **field pool**
   (D3), next to the procedural pool, plus a setting that says which source enemy operations use. The procedural pool,
   `EnemyObjectProceduralMapIndex` and its save field stay as they are (no rename). Each new pool gets its own
   shuffle offset and saved cursor.
2. **Remembered map per item (D1).** A saved map `anchor key -> map path` in the campaign state. The pick first
   looks up the item's anchor key; only when it has no entry does it take the cursor and advance it. The same
   applies to divisions with their own key in the field pool. (Built, section 5.)
3. **Enemy start marker.** Built as part of the site director (5b): the director's location is the enemy start. On
   field maps it will be where the division army spawns.
4. **Signature slots (D2).** Slot actors placed inside each hand-placed base. Each slot holds, per `EMapEnemyItem`,
   what to spawn there (a hangar, a strip section, a parked aircraft, a factory hall), or nothing. At load the item
   type fills the slots; slots that are not used stay empty or get a filler. Every item map must have slots that fit
   every item type in the pool (the strip is the long one), or a map is tagged with the item types it supports.
5. **Item site director.** One actor on each handmade map that, at load:
   - reads `FWorldMissionContext` (item type, `OperationSeed`, strength contributions, difficulty);
   - fills the signature slots (item maps) or spawns the division army (field maps);
   - turns the strength contributions into extras: Bunkers, Minefields and AABattery switch on optional hand-placed
     groups in the base; field divisions add garrison; AirSupport gives the enemy air abilities;
   - configures and starts the `AEnemyController` (decision tree, `FEnemyAIMissionSettings`, global abilities);
   - registers the spawned signature buildings with the objective (D4);
   - has a debug override (item type, division type, seed) so a map runs in PIE without the world map.
6. **Per item profile data.** Built as one `UEnemyItemSiteProfiles` asset (5b) with, per item type, a decision tree
   and training; slot content lives on the slots and content sets. Still to add: enemy global abilities (airfield:
   the Pe-2/Il-2 strafes while the hangars stand) and briefing text. EnemyHQ and EnemyWall are meant as story places,
   but the world map still sends them to the item pool.
7. **Division army data.** Per division type (tank, infantry, convoy): which units spawn, scaled by the division's
   strength, and the AI orders it starts with (defend, attack, or drive a route for a convoy).
8. **Generic objective (D4).** A mission on every item map that wins when every registered signature building is
   destroyed and reports through `AMissionManager::TriggerVictory`. Field maps win when the division army is
   destroyed. Both lose through the existing defeat and WITHDRAW path.
9. **Map authoring checklist.** Player spawn centre, hand-placed base with an `AEnemyItemSiteDirector` at its centre,
   `AEnemyItemSignatureSlot`s and optional `AEnemyItemStrengthSlot`s, navmesh, `BP_MissionManager` with the generic
   objective, an `AEnemyController` with a decision tree. A placement check (editor utility or map test) that every
   slot fits its content.
10. **Skirmish AI.** Not needed for the first playable version: the existing strategic AI already trains from the
    hand-placed Soviet buildings and attacks or defends. The full skirmish AI (base building, economy, expansion) is
    `EnemyBaseBuilding_Plan.md` phases 0-3 and `EnemyHarvestEconomy.md`, not re-planned here.
11. **Doc update.** `WorldMapCampaignDesign.md` says "a division or convoy is fought on the next enemy procedural
    map"; that changes to the field pool once step 1 lands. The division proxy item type
    (`GetEnemyItemProxyForDivision`) is then only used by the procedural source.

Not needed any more because of D2: extracting `GenerateSovietBaseAt` out of survival mode (it stays survival-only).

## 4. Build order

Each step is testable alone.

1. **Pools and remembered maps** (C++, small). **Built 2026-11-24, see section 5.** Item pool, field pool, source setting, per-anchor map memory, cursor
   per pool. Automation tests next to the existing `RTS.WorldCampaign` ones: same item twice gives the same map, a
   new item moves the cursor, divisions use the field pool.
2. **Enemy start marker, signature slots, site director, item profiles** (C++ + data assets), with the debug override.
   **C++ built 2026-11-24, see section 5b.** The director doubles as the enemy start marker.
3. **Generic objective** for item maps and field maps. **Built 2026-11-24, see sections 5c and 5d.**
4. **Division army spawning** on field maps. **C++ built 2026-11-24, see section 5d.** Supply convoys drive a
   placed route: **C++ built 2026-11-25, see section 5e.**
5. **First maps** (owner): two or three item maps and one or two field maps; each item map checked with every item
   type through the debug override.
6. **Airfield content**: parked aircraft, enemy air abilities.
7. **Skirmish AI** per `EnemyBaseBuilding_Plan.md` and `EnemyHarvestEconomy.md`.

## 5. Step 1 as built (2026-11-24)

- `WorldCampaign/WorldPlayer/OperationMaps/WorldOperationMapPool.h/.cpp`: `EWorldEnemyOperationMapSource`
  (ProceduralMaps, HandmadeMaps), `FWorldOperationMapPoolState` (saved cursor + `MapByTargetKey`, a map path per
  anchor or division key) and the pure pick `WorldOperationMapPool::TryPickMapForTarget`.
- `FWorldOperationMapSettings` (world controller Blueprint defaults, category World Campaign|Operations):
  `EnemyOperationMapSource` (default **ProceduralMaps**, so nothing changes until it is switched),
  `HandmadeEnemyItemMaps`, `HandmadeFieldMaps`.
- `FWorldCampaignState`: `HandmadeEnemyItemMapPool`, `HandmadeFieldMapPool`. A new campaign resets both. The
  procedural cursor `EnemyObjectProceduralMapIndex` is unchanged.
- `AWorldPlayerController`: each pool is shuffled from the campaign seed with its own offset.
  `LaunchEnemyMap` and `LaunchOperationAgainstDivision` both pick through `TryPickEnemyOperationMap`; a launch
  that fails after the pick restores every cursor. Each handmade pick logs `RTS_WORLDCAMPAIGN_OPERATION_MAP` with
  the pool, target, map and whether it was a retry.
- The memory stores the map path, not the index, so adding maps to a pool during a campaign keeps each attacked
  item on its map. A target whose map is taken out of the pool gets the next map.
- With HandmadeMaps selected and the field pool empty, a division attack reports an error and does not launch;
  there is no fallback to the item pool.
- Tests: `RTS.WorldCampaign.OperationMaps.*` (retry keeps the map, cursor wraps, unset and removed maps, a failed
  launch restores the cursors, the pools survive the save game serializer).
- Not run: a real launch from the world map, WITHDRAW and re-attack (no test switch launches an enemy item).
- Only the DebugGame editor target was built; the new Blueprint fields show in the Development editor after a
  rebuild.
- The mission context still carries the proxy item type for divisions (the procedural source uses it); field maps
  ignore it and read the roster instead (section 5d).

**To switch over in the editor:** on the world controller Blueprint, fill `HandmadeEnemyItemMaps` and
`HandmadeFieldMaps`, then set `EnemyOperationMapSource` to HandmadeMaps.

## 5b. Step 2 as built (2026-11-24)

C++ only, in `RTS_Survival/Enemy/ItemSites/`. No asset or map was made (the editor bridge was down).

| Class | Placed where | What it does |
|---|---|---|
| `AEnemyItemSiteDirector` | One per handmade item map, at the enemy base. **Its location is the enemy start**, so there is no separate start marker. | On the first tick it reads the world mission context (or its `TestContext` when the map is opened directly, default Airfield), fills every signature slot and switches on the strength slots. Once the enemy controller is up it applies the item's profile and starts the enemy AI (`bStartEnemyAIThinking`). For step 3: `GetIsSiteBuilt`, `GetSignatureTargets`, `GetSpawnedSignatureTargetCount`, `OnSiteBuilt`. |
| `AEnemyItemSignatureSlot` | On each spot of the hand-placed base where item content goes. | Per item type an actor class and "is a victory target". Order: the slot's own row, its `ContentSet`, the slot's `FillerContent`, the set's filler. Fillers are not victory targets by default. Spawned as the slot's enemy player. |
| `UEnemyItemSlotContentSet` | Data asset. | A reusable per-item table for one kind of spot (for example "large building spot"), so maps do not repeat classes. |
| `AEnemyItemStrengthSlot` | Optional extras in the base. | A dormant defence slot that switches on when its condition holds: a required fortification from the world map (bunkers, minefields, AA), a minimum total strength, and/or only for some item types. |
| `UEnemyItemSiteProfiles` | Data asset on the director (best on its Blueprint subclass). | Per item type: an optional decision tree and an optional training override. Without one the map's own enemy controller settings stay. |

- A division operation (the mission context has a division key) is recognised and spawns no base content; the
  division army is step 4.
- Errors reported in play: no victory target was spawned for the item type (the map has no signature slot with a
  target for it, which is also what happens for EnemyHQ or EnemyWall, see below), and an enemy controller with no
  decision tree (it would train nothing).
- Log line: `RTS_ENEMY_ITEM_SITE built item=... source=worldmap|test signatureTargets=... strengthSlots=...`.
- Tests: `RTS.EnemyItemSites.*` (slot content order, strength conditions, context source incl. division and story
  mission, a director filling slots in a test world).
- **EnemyHQ and EnemyWall can still be attacked as pool items.** `LaunchOperationForWorldObject` sends every enemy
  map object to the enemy item pool. If they should be story places, the world map must route them elsewhere.
- Enemy global abilities (the airfield's strafes) are not applied: the enemy controller only loads its ability
  loadout at BeginPlay. Left for step 6.

**To try it in the editor (after a Development rebuild):** on a test map, place one `AEnemyItemSiteDirector` at the
enemy base and set its `TestContext`. Place a few `AEnemyItemSignatureSlot`s with classes for that item. Then play
in PIE.

## 5c. Step 3 as built (2026-11-24)

- **`UDestroyEnemyItemSiteMission`** (`Missions/MissionClasses/DestroyEnemyItemSiteMission`): add it to the map's
  `BP_MissionManager` Missions. It finds the site director, waits until the site is built, shows the targets in the
  mission widget and on the minimap (`TrackActorsWithMiniMapProgress`, `TrackingPrefix`), and completes when every
  signature target is dead. Then, after `VictoryDelaySeconds` (3 s), it calls `TriggerVictory`, which reports the
  win to the world map in a campaign operation. Set `bTriggerVictoryOnComplete` off to call it from the Blueprint
  instead. Give it its title and description in `MissionWidgetState` as for any mission.
- **Dead means dying, not destroyed.** Targets count as dead through `RTSIsValid` (the engine's `bIsDying`), so a
  building that stays as a wreck counts at once. `SetUnitDying` is called by building expansions (the Soviet base
  buildings), tanks, aircraft, squads, team weapons, standalone turrets and destructible environment actors. Any
  other class only counts once its actor is destroyed, so a parked aircraft or strip piece must be one of these; a
  plain prop never counts.
- The minimap/widget count follows `OnDestroyed`, so it can lag a wreck by a moment; the win does not wait for it.
- A built site with no victory target never wins (error reported). Field maps: section 5d.
- **EnemyHQ / EnemyWall (D5):** `FWorldOperationMapSettings::StoryEnemyItemMaps` (world controller Blueprint) maps an
  item type to a fixed level. When set, the attack opens it and no pool cursor moves. With HandmadeMaps selected and
  no level set, attacking the HQ or wall is refused with an error. The procedural source keeps its old behaviour.
- Tests: `RTS.EnemyItemSites.ObjectiveState`, `RTS.WorldCampaign.OperationMaps.StoryItemRoute`. The mission class
  itself is not covered by a test (it needs a mission manager and a running level).

## 5d. Step 4 as built (2026-11-24)

The attacked division is spawned as it is on the world map: no data table, no scaling. A world division already
loses roster entries when it is damaged, and its strength is worked out from what is left, so scaling again would count
the losses twice.

- **Roster in the mission context.** `FWorldMissionEnemyArmy` (`Game/RTSGameInstance/WorldMissionContext/`): division
  type, one `ETankSubtype` per vehicle, one `ESquadSubtype` per squad, and a truck count for supply convoys.
  `PopulateWorldMissionContextForDivision` fills `FWorldMissionContext::EnemyArmy` from the division's save data. The
  stand-in `EnemyObjectType` stays (the procedural source and `GetIsValid` still use it).
- **Field maps use the same `AEnemyItemSiteDirector`**, placed where the army starts (the facing arrow is the army's
  yaw). With a division context it spawns the army through its `UEnemyDivisionArmySpawner` component, on rings around
  the director (`ArmySettings.UnitSpacingMultiplier`, default 2x the strongpoint spacing), projected to the navmesh,
  as the director's enemy player. Strength slots switch on as on item maps; signature slots stay empty.
- **`ArmySettings`** on the director: orders per division type (`OrdersByDivision`, else `DefaultOrders`):
  - **Enemy AI Control** (default): once every unit has spawned and the enemy AI is ready, the units are registered
    with the AI's direct control, so its decision tree attacks or defends with them.
  - **Hold At Start**: the units stay where they spawned and fight what comes in range. Default for supply
    convoys, so the AI does not send unarmed trucks into attacks.
  `ConvoyTruck` is what one convoy truck spawns as (default Rus Zil ammo truck).
- **Profiles:** `UEnemyItemSiteProfiles::ProfileByDivision` gives a decision tree and training per division type,
  as `ProfileByItem` does per item. The "no decision tree" error only shows when the army is on Enemy AI Control.
- **Victory:** `UDestroyEnemyItemSiteMission` works on both map kinds through `GetVictoryTargets` /
  `GetSpawnedVictoryTargetCount`: signature targets on item maps, the living army units on field maps. A unit counts
  as gone when it is dying or destroyed (a squad destroys itself with its last soldier), or when it is no longer
  hostile to the player (captured). Hostility, not an exact owner, because the first AI player's units keep the owner
  of their Blueprint. A won division operation removes the division on the world map
  (`RemoveEnemyPieceDefeatedByPlayer`).
- **Site built only when the whole army stands:** `GetIsSiteBuilt` stays false while spawn requests are open, so the
  first units cannot be the whole objective. A failed spawn counts down and is reported; requests still unanswered
  after 30 s are reported and the army goes on with what arrived. An army that spawned nothing is an error and never
  a win. An empty roster is reported.
- **PIE:** set `TestContext.bIsDivisionOperation` and fill `TestContext.Army` to run a field map without the world map.
- Log lines: `RTS_ENEMY_DIVISION_ARMY spawned owner=... units=... failed=... orders=...` and the site's
  `RTS_ENEMY_ITEM_SITE built ... victoryTargets=...`.
- Tests: `RTS.EnemyItemSites.ArmySpawnList` (roster to spawn list, None entries and unset truck skipped, orders),
  `RTS.EnemyItemSites.DivisionArmySite` (an empty army is built at once with no target; a pending army is not built),
  `SiteContextSource` (the roster is carried). Real spawning needs the async spawner of a running level and is not
  covered.
- **Not run in a level:** real spawning, the hand-off to direct control, what the decision tree does with an army on
  a map without a base, and the mission on a field map. Only the DebugGame editor was built; `ArmySettings` and
  `ProfileByDivision` show in the Development editor after a rebuild.

**Open (owner decisions):**
- **Convoys** park at the enemy start only when the director has no `ArmySettings.ConvoyRoute`.
  **Owner pick 2026-11-24: B, drive a placed route** (an `AEnemyConvoyRoute` on the field map, with the convoy
  system's escorts, stops and caches). **Escape rule (owner 2026-11-25): any escape = defeat** (one truck at the
  exit or one branch truck delivered loses the battle; win when every truck is destroyed or captured). C++ built
  2026-11-25, see 5e.
- **Supporting divisions** (other enemy divisions in range) only add strength percentage, which can switch on
  strength slots. Their units do not spawn.
- **A lost or withdrawn field battle** leaves the world division at full roster; the units the player killed are not
  taken off it.

## 5e. Convoy route battles as built (2026-11-25)

Owner picks: convoys drive a placed `AEnemyConvoyRoute` (B), and **any escape = defeat**.

- **Setup on a field map:** place an `AEnemyConvoyRoute` (route points, definition, escort tiers, optional stop,
  branch, ambush zones, hunters) with **Start On Begin Play off**, and set it as the director's
  `ArmySettings.ConvoyRoute`. Tank and infantry battles on the same map leave the route idle. Without a route a convoy
  battle is reported and the trucks park at the start (step 4 behaviour).
- **The convoy:** `AEnemyConvoyRoute::StartSingleConvoy(TruckCount)` runs exactly one convoy, whatever the route's
  repeat settings, after the route's `FirstConvoyDelay` and once the enemy AI is ready. The truck count is the world
  map convoy's remaining trucks: `FConvoyStartParams::TruckCountOverride` cuts the definition's column from the back
  or repeats it in order (`UConvoyDefinition::ResizeTruckColumn`). Escorts are the route's guard tier for the
  difficulty. Cargo per truck stays the definition's share. The convoy drives and fights by its definition; it needs
  no decision tree.
- **Win and loss** (`EnemyItemSiteRules::GetConvoyBattleObjectiveState`, from truck fates, not living units, because
  escaped trucks are removed too):
  - one truck escaped at the exit or delivered on a branch: `EnemyEscaped`, checked first, also while the convoy
    still drives;
  - the convoy finished as `TrucksDestroyed` or `TrucksStopped` (captured trucks count as stopped) with at least one
    spawned truck: win;
  - aborted, no truck spawned, or the route started no convoy (bad drive path, broken definition, route already
    running): never a win, reported;
  - before the column formed: waiting, so zero trucks during spawning is not a win.
- **Mission:** `UDestroyEnemyItemSiteMission` asks the director (`GetObjectiveState`). On `EnemyEscaped` it fails and
  calls `TriggerDefeat(LostMainObjective)` (turn `bTriggerDefeatOnEnemyEscape` off to react in Blueprint). The defeat
  screen offers retry (same map, same convoy) or withdraw, which reports the defeat to the world map. The victory
  targets shown are the living convoy trucks.
- **Code:** `UEnemyConvoyBattleTracker` (component on the director) starts the route and records the convoy's
  outcome; the route's `OnConvoyCargoOutcome` gives the final truck fates. Log line
  `RTS_ENEMY_CONVOY_BATTLE finished owner= outcome= trucks= escaped= branchDelivered=`.
- **Tests:** `RTS.EnemyItemSites.ConvoyBattleRules`, `RTS.EnemyItemSites.ConvoyTruckColumn`,
  `RTS.EnemyItemSites.ConvoyRouteSite` (no route: reported and parked; idle route: running and waiting; running
  route: failed, never a win).
- **Not run in a level:** a real convoy on a field map, the escape defeat screen, and a win by capture. No field map
  has a route yet.

## 6. Not checked

- The contents of `EnemyObjectProceduralMaps` in the world controller Blueprint.
- Which PCG graph reads the world mission context nodes, and which objective `BP_MissionManager` runs on
  `EnemyObjProcedural_0`.
