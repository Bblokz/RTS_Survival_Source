# Four-player refactor: plan

Goal: the game supports up to four players. Player 1 is the human; players 2-4 are AI. Until now the code knew two
sides: "player 1" and "everything else". This plan replaces that split where it decides **who fights whom**, and
leaves it alone where it only asks **is this the human**.

Status (2026-11-18): implemented, built (DebugGame editor) and playtested; uncommitted. Results in section 10.

## 1. Rules everything follows

1. **Relations, not player numbers, decide combat.** `FRTSPlayers::AreHostile(A, B)` is the only source of truth.
   Relations come from teams (`SetPlayerTeam`); the default is free for all. They may change at runtime.
2. **Two kinds of `OwningPlayer == 1` sites.** Classify each one before touching it:
   - *"Is the human"* (UI, fog of war behaviour, resources, tech, XP, voice lines, building placement preview,
     healthbar colour): stays player 1. Write it as `FRTSPlayers::IsHumanPlayer(Owner)` when you touch the file.
   - *"Which side"* (weapon channel, target lists, overlap logic, enemy-of lookups, `? 2 : 1`, `PlayerSearch != 1`,
     paired `*Player` / `*Enemy` members): becomes relation- or player-index-based.
3. **Odd owners keep their old behaviour.** Owners outside 1..4 (unset 222, 0) used to fall into the "enemy" bucket.
   `FRTSPlayers::NormalizePlayer` maps them to player 2. `UWeaponState`'s own `0 -> 1` rule stays.
4. **Blueprint-facing signatures and property types do not change.** `OwningPlayer` stays a `uint8`
   `BlueprintReadWrite`; `BlueprintCallable` functions keep their parameters. Add overloads or new functions and let
   the old ones forward.
5. **No behaviour change for a two-player game.** A map with only players 1 and 2 must play exactly as before.

## 2. Foundation (written, frozen API)

| File | What it gives |
| --- | --- |
| `Game/Players/RTSPlayers.h/.cpp` | `RTSPlayers::HumanPlayer / FirstAIPlayer / MaxPlayers / PlayerArraySize / AllPlayersMask / AIPlayersMask`; `FRTSPlayers::IsValidPlayer, IsHumanPlayer, IsAIPlayer, NormalizePlayer, GetPlayerBit, GetHostileMask, GetFriendlyMask, AreHostile, AreAllied, IsHostileToHuman, HasHostileAIPlayer, SetPlayerTeam, GetPlayerTeam, ResetToFreeForAll, GetOwningPlayerOfActor, IsActorHostileToPlayer, IsActorAlliedToPlayer` |
| `Game/Players/RTSPlayerRelationsSubsystem.h/.cpp` | World subsystem that resets teams to free for all per game world; `URTSPlayerRelationsLibrary` exposes the same questions to Blueprints |
| `Utils/CollisionSetup/RTSPlayerCollision.h/.cpp` | `FRTSPlayerCollision::GetWeaponTraceChannel, GetWeaponIgnoreMask, ApplyWeaponIgnoreMask, GetOwnerObjectChannel, SetWeaponHitResponses, SetWeaponHitResponsesForAllPlayers, SetOwnerMaskFilter, ClearOwnerMaskFilter, GetOverlapLogicForHostilesOf, GetOverlapLogicForAlliesOf, AddObjectTypesForOverlapLogic` |
| `RTSCollisionTraceChannels.h` | Aliases `COLLISION_TRACE_HUMAN_WEAPONS` (= `COLLISION_TRACE_ENEMY`) and `COLLISION_TRACE_AI_WEAPONS` (= `COLLISION_TRACE_PLAYER`) |

A player mask is a `uint8` with bit `1 << (Player - 1)`. The same bit layout is used for hostile masks, friendly
masks, visible-to masks and the collision mask filter.

## 3. Collision design: no new channels

Collision channels keep meaning *human side* vs *AI side*. Adding channels per player would need every map asset and
collision profile that blocks weapon traces to be re-authored; the engine's per-body **mask filter** (6 spare bits,
checked by every scene query, ignored by physics contacts) does the per-player part instead.

- **Weapon query** of player K: channel `GetWeaponTraceChannel(K)` (human: `COLLISION_TRACE_ENEMY`, any AI:
  `COLLISION_TRACE_PLAYER`), plus `QueryParams.IgnoreMask = GetWeaponIgnoreMask(K)` (K and its allies).
- **Unit body** owned by K that weapons may hit: `SetWeaponHitResponses(Component, K)`. Human-owned bodies block the
  AI weapon channel. AI-owned bodies block **both** weapon channels, so AI players can shoot each other. The body
  gets K's bit as mask filter, so friendly queries skip it.
- **Hit by everyone** (neutral objects, walls that stop friendly fire): `SetWeaponHitResponsesForAllPlayers`,
  mask filter 0. Environment assets are untouched: they have no owner bit and block as authored.
- **Consequence that must not be missed:** every AI-fired trace, sweep or overlap query now needs the ignore mask.
  One without it hits the shooter's own units. After the refactor no gameplay code may pass a raw weapon channel
  macro to a query; a grep for the macros must only find response setup and the helper.
- **Object channels** (`COLLISION_OBJ_PLAYER` human, `COLLISION_OBJ_ENEMY` all AI) stay as they are. Triggers and
  area queries take their channels from `GetOverlapLogicForHostilesOf / AlliesOf` and **always** filter the result
  with `FRTSPlayers::IsActorHostileToPlayer / IsActorAlliedToPlayer`, because hostile and allied AI share a channel.
  Object-type queries also get the ignore mask.
- **Physics blocking** (movement meshes, hull blockers) stays human side vs AI side. Vehicles of two hostile AI
  players therefore meet like two vehicles of one AI player do today (hull boxes block). Accepted for now.

## 4. Visibility rule

The fog of war stays human-centred: human units are tagged `IsInEnemyVision` when any AI unit sees them; AI units
are hidden when the human does not see them.

- Human looking at anything: target must not be hidden.
- AI looking at a human unit: target must carry `IsInEnemyVision` (shared by all AI players) and not be hidden.
- AI looking at an AI unit: always visible while alive; a unit that is hidden **only by the fog** still counts.
  `UFowComp` marks that state (tag `IsHiddenByFog`) so "hidden in a transport" stays untargetable.

The async snapshot stores this per unit as a visible-to mask, so the thread does two bit tests per candidate.

## 5. Work packages (four sub-agents, disjoint files)

Nobody builds or launches an editor except the lead; the lead does one build and the test runs.

### A. Collision, weapons, area damage, triggers
`Utils/CollisionSetup/FRTS_CollisionSetup.*`, `Weapons/**`, `Utils/AOE/**`, grenades, bombs, ICBM, flares, fire
manager, field mines, auras, reinforcement points, medic, every other user of the weapon channel macros or
`ETriggerOverlapLogic`.
- Collision setup functions take the owner and use `FRTSPlayerCollision`.
- Every weapon query gets channel and ignore mask from the helper (hitscan, projectiles, lasers, flame, multi-trace,
  strafe gun, bombs, rockets).
- Every overlap handler and area query filters by relation.
- Owner change at runtime (capture, garrison, `ChangeOwningPlayer`) re-applies responses and mask.

### B. Unit storage, async targeting, visibility
`Game/GameState/GameUnitManager/**` (except the strategic AI helpers), `RTSComponents/RTSTargetAcquisition/**`,
`RTSComponents/RTSComponent.*`, `Utils/FunctionLibary.cpp` / `HFunctionLibary.h` (visibility), `FOWSystem/**`.
- Per-player unit arrays and counters, indexed by player number; old "Player"/"Enemy" accessors stay as wrappers.
- Async thread: one flat snapshot array `{ActorId, Location, Type, OwnerBit, VisibleToMask}`; a request carries the
  requester's bit and hostile mask by value; squared distances; one id-to-actor map on the game thread.
- Synchronous closest-target searches walk the arrays of all hostile players.
- Viewer-aware visibility as in section 4.

### C. Enemy AI controller
`Enemy/**`, `Utils/RTS_Statics/**` (controller access), `Game/GameState/GameUnitManager/` strategic AI request
handling shared with B (agree the seam first), tactical AI components, `SurvivalMode/**` call sites.
- `AEnemyController` gets an editable `ControlledPlayer` (default 2). Everything it spawns, counts, orders or asks
  the async thread about uses that player, never a literal 2.
- `UEnemyControllerSubsystem` keeps a controller per player. `GetEnemyController()` without arguments keeps
  returning player 2's, so the existing call sites and Blueprints stay valid; new
  `GetEnemyControllerForPlayer(Player)`.
- Strategic AI requests and the base-cluster cache carry the requesting player and its hostile mask;
  "player units" in those helpers become "units hostile to the requester".
- Units find the controller of their own owner.

### D. Blueprints and the four-player playtest
- Audit Blueprint weapons (turrets, infantry weapons, hull weapons, aircraft weapons, building expansions with
  guns): graphs that compare an owning player with a literal, pick a channel, or trace on the weapon channels.
  Report each one with the fix (C++ now handles it / node must change).
- Playtest harness under `RTS_WITH_MAP_TESTS`, modelled on `VehicleDuelBenchSubsystem`: spawns a mixed group (tanks,
  squads, a Blueprint turret) per player with `FRTSUnitOwnerSpawning::SpawnUnitForPlayer`, records an
  owner -> owner damage matrix from real hits, writes `summary.md`.
- Remaining "is the human" sites outside packages A-C get the named helper where cheap.

## 6. Playtest pass criteria

Run headless with `-RenderOffscreen` (the fog of war needs a renderer) on a unit-test map.

1. Free for all, four groups placed so that each group has at least two other groups in weapon range:
   every player damages **at least two** other players; **zero** damage between units of one player.
2. Teams {1, 3} vs {2, 4}: damage only crosses teams; **zero** damage between allies.
3. Regression: one existing two-player suite (vehicle duel bench) still passes.

## 7. Known limits after this refactor

- The fog of war is not per player: all AI players share one view of the human's units and see each other always.
- Physics blocking between vehicles is still human side vs AI side.
- Human-only systems (resources, tech tree, UI, global abilities UI) stay human-only.
- One `AEnemyController` per AI player is supported by the registry; authoring strategic behaviour for players 3
  and 4 (decision trees, wave settings per map) is content work that follows.

## 8. Decisions after the survey round (these override the sections above where they differ)

### Foundation changes
- **Neutral owner 0** is real (supply points, command decoder): never registered, hostile to nobody, allied to
  nobody, no player bit. `NormalizePlayer(0) == 0`; only other odd values (unset 222) map to player 2. Neutral
  things keep their old collision: only the human's weapons reach them (`FRTSPlayers::CanPlayerDamageOwner`).
- **Active players.** `FRTSPlayers::GetActivePlayersMask()` starts as players 1 and 2; the unit manager marks a
  player active when its first unit registers. `GetOverlapLogicForHostilesOf / AlliesOf` only count active players,
  so a two-player map sweeps exactly the channels it swept before.
- **Object-type queries** read their ignore mask from `FCollisionObjectQueryParams`, not from the query params:
  there is an `ApplyWeaponIgnoreMask` overload for each.
- **Area damage does not use the ignore mask** (it would also skip the shooter's own destructible walls). It sweeps
  the channels of `GetOverlapLogicForHostilesOf` plus destructibles as before, keeps destructible hits, and keeps a
  unit-channel hit only if `FRTSPlayerCollision::IsHitDamageableByPlayer` (hostile seeking) or
  `IsHitAlliedToPlayer` (ally seeking) says so.
- **Who was hit.** `FRTSPlayerCollision::GetOwningPlayerOfHit` reads the owner bit of the hit body first, then the
  RTS component of the actor or its parent actors (turrets are child actors), then the side of the object channel.
- **Owner bit on whole actors.** `FRTSPlayerCollision::ApplyOwnerMaskToActor` marks every colliding primitive of a
  unit and its child actors. `URTSComponent` calls it at BeginPlay and on a runtime owner change. Components set up
  with `SetWeaponHitResponsesForAllPlayers` carry the component tag `RTSHitByAllPlayers` and are skipped; the tag
  can also be put on a Blueprint component by hand to keep it hittable by friendly fire.
- **Held targets.** `FRTSPlayers::GetRelationsVersion()` changes when teams change or a unit changes owner; weapons
  re-check that their held target is still hostile only when it differs.

### Seams (frozen signatures)
| Provider | API |
| --- | --- |
| B | `URTSComponent::OnOwningPlayerChanged` (native multicast, `uint8 OldOwner, uint8 NewOwner`), broadcast after the write and re-registration, only once the component has begun play. On a runtime change B also calls `ApplyOwnerMaskToActor` and `FRTSPlayers::NotifyUnitOwnerChanged()` |
| B | `GetPlayerTanks(Player)`, `GetSquadUnitsOfPlayer(Player)` and every other `...OfPlayer(Player)` return exactly that player's units; new `GetTanksOfPlayers(uint8 PlayerMask)`, `GetSquadUnitsOfPlayers(uint8 PlayerMask)`; new `GetHas{Tank,Squad,Nomadic,Bxp,Aircraft}OfTypeForPlayer(uint8 Player, Subtype)`; `GetHasEnemy*OfType` means any AI player |
| B | `RequestClosestTargets` and `RTSIsVisibleTarget(Actor, ViewerPlayer)` keep their signatures; exactly one callback per request (an empty one too); results are already limited to units hostile to the requester when delivered |
| C | `AEnemyController::GetControlledPlayer()`; `UEnemyControllerSubsystem::GetEnemyControllerForPlayer(int32)`, `GetAllEnemyControllers()`; `FRTS_Statics::GetEnemyControllerForPlayer(const UObject* WorldContextObject, int32 Player)`. The argument-less getters return player 2's controller, or the lowest-numbered one when player 2 has none |
| C | `FStrategicAIRequester { int32 RequestingPlayer = 2; uint8 HostileMask = 0x01; uint8 FriendlyMask = 0x02; }` as member `Requester` of `FStrategicAIRequestBatch`, stamped by the strategic AI component on the game thread at send. The six `FStrategicAIHelpers::Build*` that take unit states get it as a third, last parameter; the two-argument overloads stay |
| B | calls those six with `Request.RequestBatch.Requester`; keeps one base-cluster cache per requesting player on the thread |
| C | `AsyncSpawnOptionAtLocation / AsyncSpawnClassAtLocation` get a trailing `const int32 OwningPlayerOverride = INDEX_NONE`; controller 2 passes none (and logs a mismatch), controllers 3 and 4 always pass theirs; squad soldiers are matched to their squad's owner |
| A | weapon owners (`ACPPTurretsMaster`, `UHullWeaponComponent`, `AInfantryWeaponMaster`, team weapons, squad units) forward an owner change to their weapon states and collision |

### Rulings
- Ally-seeking effects (medic, heal and repair auras, the Sonnengewehr spare check) work for allied players; the
  reinforcement point stays own-player only. Authored "Player / Enemy" enums on auras and the radixite nuke are read
  relative to the owner; mission triggers and destructible-actor wrappers stay literal ("Enemy" = any AI player).
- Strategic AI: units the controller then orders (retreat, hazmat) and its base buildings are own-player only;
  only the spacing list uses the friendly mask. Exact owner matching on the thread, no normalizing.
- `GetHasEnemy*OfType` and the "enemy" healthbar strategies mean all AI players.
- Survival mode and the mission wave creators stay on player 2.
- The caller-less synchronous `GetClosestTarget*` functions are ported, not deleted; dead stub files are left alone.
- Bugs spotted on the way that are not part of this refactor (nomadic healthbar strategy never matches) keep their
  behaviour and are reported, not fixed.

### File ownership
| Agent | Files |
| --- | --- |
| A | `Utils/CollisionSetup/**`, `Weapons/**`, `Utils/AOE/**`, `RTSComponents/AbilityComponents/**`, `RTSComponents/ArmorComponent/**`, `RTSComponents/ArmorCalculationComponent/**`, `RTSComponents/MedicComponent/**`, `RTSComponents/AOEBehaviourComponent/**`, `RTSComponents/CargoMechanic/**`, shield component, `Behaviours/**`, `Subsystems/FireSubsystem/**`, `GlobalAbilities/Sonnengewehr/**`, `GlobalAbilities/FireStrikes/IncendiaryCanister.cpp`, `GlobalAbilities/RadixiteNuke/**`, `Units/Squads/Reinforcement/ReinforcementPoint.*`, `Units/Squads/SquadUnit/SquadUnit.*`, `Units/Tanks/TankMaster.cpp`, `TrackedTankMaster.cpp`, `NomadicVehicle.cpp`, `TrainUnit.cpp`, `Units/TeamWeapons/**`, `Buildings/**`, `Environment/DestructableEnvActor/WallGate/**`, `Navigation/CoverFinder/**`, tests that use the channel macros |
| B | `Game/GameState/GameUnitManager/**`, `RTSComponents/RTSComponent.*`, `RTSComponents/RTSTargetAcquisition/**`, `Utils/HFunctionLibary.h`, `Utils/FunctionLibary.cpp`, `DeveloperSettings.h`, `FOWSystem/**`, `GlobalAbilities/SpiesInfiltration/GA_SpiesInfiltration.cpp` |
| C | `Enemy/**`, `Utils/RTS_Statics/**`, `Player/AsyncRTSAssetsSpawner/**`, `Utils/UnitSpawning/**`, `Units/SquadController.*`, `Units/Squads/Reinforcement/SquadReinforcementComponent.*`, `GlobalAbilitySystem/GlobalAbilitiesManager/**`, `Interfaces/Commands.cpp`, `GameUI/TrainingUI/TrainerComponent/TrainerComponent.cpp`, `Units/Aircraft/AircraftMaster/AAircraftMaster.cpp`, `GlobalAbilities/Paratroopers/GA_Paratroopers.cpp`, `Missions/**`, `Player/RTSCheatManager/**` |
| D | new playtest files, runner script, Blueprint fixes, leftover "is the human" sites in files nobody else owns |
| Lead | `Game/Players/**`, this plan, the build |

## 9. Blueprint audit result (724 Blueprints exported and read, 1,583 component defaults)

Blueprint graphs almost never decide a side. All 316 weapon Blueprints pass their owner on to C++ (from the turret
getter or the RTS component), all 151 armor setups pass the RTS owner, and no Blueprint traces on a weapon channel.
No Blueprint is edited in this refactor; the cases that matter are handled in C++:

| Finding | Handled by |
| --- | --- |
| `Ger_Aircraft_BF109`, `_BF110Airborne`, `_StukaJu87` pass owner literal 0 to their weapon setup | A: weapon owners resolve the unit's owner when a Blueprint passes 0 |
| Seven Soviet light tanks (`Rus_Vehicle_BT-7`, `_MS1`, `_T50` and their `_Soviet` variants) default a Blueprint int `OwningPlayer` to 2 that the construction script copies over the RTS owner | C: a forced owner is also written to that int before the construction script runs |
| `ExperienceComponent` rejected owners other than 1 and 2 | D |
| Seven field-construction Blueprints pass a literal side to `SetupCollision` | A: the actor's RTS owner wins when valid |
| Mission Blueprints: `ExecuteGlobalAbility(1)`, trigger areas with `OverlapPlayer`, one convoy with owner 2 | stay: human and player 2 by design |

Class-default owners across 655 RTS components: 338 player 1, 304 player 2, 13 neutral, none other.

Playtest attribution: `Utils/RTSMapTests/DamageHooks/DamageMapTestHooks.h` (map tests only). Damage sources name
their owner with `RTS_MAPTEST_DAMAGE_ATTACKER_SCOPE`; `UHealthComponent::TakeDamage` reports attacker, victim and
health lost. The four-player skirmish builds its owner-by-owner matrix from that, so it counts real damage.

## 10. Result (2026-11-18)

Built: `RTS_SurvivalEditor Win64 DebugGame`, no errors. Playtest: `Tools/FourPlayerSkirmish/run_four_player_skirmish.ps1
-Phase Both` (see `FourPlayerSkirmish.md`): per player a Pz IV H, a Mosin squad and a 50 mm ball turret on the
corners of a 26 m square on `RTS_ToCopySmall`. Damage below is health lost, attacker (row) to victim owner (column).

| Free for all | P1 | P2 | P3 | P4 |
| --- | --- | --- | --- | --- |
| P1 | 0 | 572 | 0 | 243 |
| P2 | 440 | 0 | 229 | 0 |
| P3 | 36 | 466 | 0 | 326 |
| P4 | 343 | 9 | 564 | 0 |

| Teams 1+3 vs 2+4 | P1 | P2 | P3 | P4 |
| --- | --- | --- | --- | --- |
| P1 | 0 | 233 | 0 | 442 |
| P2 | 543 | 0 | 282 | 0 |
| P3 | 0 | 169 | 0 | 504 |
| P4 | 342 | 0 | 281 | 0 |

Both phases PASS: every player damaged at least two other players, zero damage to own units or allies, zero unclaimed
damage, no owner mismatch on units, soldiers or weapon states, no weapon ever held a non-hostile target. Regression:
the vehicle duel bench (two players, 8 matchups, 1 repeat) ran to the end with decisive outcomes on both sides.

Changes after the survey round: owner bits go on every primitive of a unit (also ones with collision off) and on
instanced-mesh instance bodies; the physics filter rebuild only runs for welded bodies; the neutral owner has an empty
friendly mask; `UWeaponState::ChangeOwningPlayer` normalizes like Init; AI cover firing lanes use the ignore mask;
pit defenses and building expansions use `SetWeaponHitResponses`; team weapons take their crew's owner.

Open, reported and not fixed (latent today): tank, nomadic, train, aircraft hulls, pit colliders, expansion meshes and
dig-in walls re-apply only the owner bit, not the side's channel responses, on a runtime human<->AI owner change; a
squad controller's runtime owner change is not forwarded to its soldiers; `UCargo::IsOccupiedByEnemy` never sets its
out parameter (predates the refactor); closed wall gates of AllowedPlayer 3 or 4 keep human/AI nav areas (fixed
later, see section 12).

Second run (2026-11-18, after the reviewer fixes and the controller checks were added): both phases PASS again. The
skirmish now also spawns a quiet `AEnemyController` for players 3 and 4; both register under their player, the
argument-less getter still returns the map's player-2 controller, and a Pz IV H spawned through each controller's
`AsyncSpawnOptionAtLocation` with `GetSpawnOwnerOverride()` reports owner 3 / 4 on the unit and every weapon state.
Player-2 controller regression: `run_enemy_wave_spawner_test.ps1` 25/25 PASS. Duel bench: no same-side hit in any
matchup (the 3 v 3 included). A sweep for `== 2`, `!= 2` and `? 2 :` on owners outside tests and survival mode finds none;
the remaining `== 1` / `!= 1` sites split human from every AI player, which is correct for players 3 and 4.
Blueprint check: no asset overrides `ARadixiteNuke::AffectedUnits` (default OverlapBoth); the pulse aura was already
owner-relative. Not compiled: the packaged (Shipping) target.

Third run (2026-11-18, exact per-weapon attribution): the damage hook now also names the firing actor
(`RTS_MAPTEST_DAMAGE_ATTACKER_WEAPON_SCOPE` at the weapon-state trace and projectile damage sites, map tests only), and
the skirmish counts every round each weapon fires by the owner of its held target. 202 of 213 hits were credited to the
exact unit. Free for all, by Blueprint weapon kind:

| Kind | Fired at (shooter -> targets) | Hit (shooter -> victims) |
| --- | --- | --- |
| Tank turret + hull MG (Pz IV H Blueprint) | P1->P2,P4; P2->P1,P3; P3->P2,P4; P4->P1,P3 | P1->P2,P3,P4; P2->P1,P3; P3->P2,P4; P4->P1,P3 |
| Infantry weapons (Mosin squad Blueprint) | P1->P2,P4; P2->P1,P3; P3->P2,P4; P4->P1,P3 | P1->P2,P4; P2->P1,P3; P3->P2,P4; P4->P1,P3 |
| Standalone turret (50 mm ball turret Blueprint) | P1->P4; P2->P3; P3->P2; P4->P3 | P1->P4; P2->P3; P3->P2; P4->P3 |

Each tank and squad engaged two players; a turret keeps its first target until one of them dies, so each turret engaged
one, and the four turrets together three different players. Both phases PASS; free for all now also requires every
weapon kind's shots to reach three different players.

Fourth run (2026-11-18, controller-driven fighting): the enemy controllers of players 3 and 4 now each start one
attack-move wave themselves (`AEnemyController::StartWaveFromRequest`: the controller's own spawning with its owner
override, its formation and attack-move orders) of two Pz IV H from 25 m behind their group's corner to the arena
centre. Both phases PASS:

| Phase | Controller | Wave units (owner) | Fired at | Hit (damage) |
| --- | --- | --- | --- | --- |
| Free for all | P3 | 2 tanks (P3, weapons P3) | P2 26, P4 20 rounds | P2 248, P4 229 |
| Free for all | P4 | 2 tanks (P4, weapons P4) | P1 21, P3 30 rounds | P1 302, P3 509 |
| Teams 1+3 v 2+4 | P3 | 2 tanks (P3) | P2 33, P4 23 rounds | P2 406, P4 241 |
| Teams 1+3 v 2+4 | P4 | 2 tanks (P4) | P1 25, P3 31 rounds | P1 311, P3 422 |

Each controller's units engaged two different players and never fired at an ally.

Fifth run (2026-11-18): runtime owner change between the human and an AI player is now handled.
`FRTSPlayerCollision::ApplySideChangeToActor` (called by `URTSComponent::ChangeOwningPlayer` when the owner crosses
human <-> AI) swaps the human/AI object channel and the responses to them, and gives every body the weapon responses of
the new side; the owner bit follows as before. The skirmish now changes two tanks out of the fight (P1 -> P3, P2 -> P1)
and fires a real weapon trace of every player at them before and after: in both phases every shot hit or passed exactly
as the relations say, the turrets and weapon states report the new owner and no body stayed on the old object channel.
Both phases PASS. The Shipping game target (`RTS_Survival Win64 Shipping`) and the Development and DebugGame editors
compile. Open from section 10 that remains: the fog of war is shared by all AI players, the hull-blocker channel
(`COLLISION_OBJ_ENEMY_HULL`) is not swapped on an owner change, and strategic content (decision trees, wave settings) for
players 3 and 4 is content work.

Sixth run (2026-11-18): the hull blocker boxes now also change side on a human <-> AI owner change (`ECC_Vehicle` <->
`COLLISION_OBJ_ENEMY_HULL`, mirrored responses); the owner-change test found no body or hull box left on the old side.
The controllers of players 3 and 4 also run their own strategic AI think loop in the skirmish; a "count the enemy's
units" query (`FGetPlayerUnitCountsAndBase`) goes through it with the controller's requester to the async thread.
The counts each controller got back match the squads of exactly its hostile players:

| Phase | Controller | Reported enemy squads | Squads of hostile players | Human only | All other players |
| --- | --- | --- | --- | --- | --- |
| Free for all | P3 | 8 | 8 | 4 | 8 |
| Free for all | P4 | 8 | 8 | 4 | 8 |
| Teams 1+3 v 2+4 | P3 | 4 | 4 | 4 (ally) | 8 |
| Teams 1+3 v 2+4 | P4 | 5 | 5 | 4 | 8 |

(The test map's own units give players 1-4 four, three, one and one squads.) Both phases PASS; the Shipping game target
and the Development and DebugGame editors compile. Remaining by design (section 7): one fog of war shared by all AI
players, and strategy content (decision trees, wave settings per map) for players 3 and 4.

Why the two remaining items are content or a later feature, not missing groundwork:
- Strategy content is authored per map on each placed `AEnemyController` (its instanced decision tree and mission
  settings). On the skirmish's unit-test map even the player-2 controller has none ("No stochastic decision tree was
  added to the enemy controller!" at BeginPlay). A player 3 or 4 gets strategy the same way: place a controller, set
  `ControlledPlayer` and its tree. Everything that tree drives (spawning, waves, strategic queries, unit orders) is
  verified per player above.
- Per-player fog of war for AI players: the targeting snapshot already carries a per-unit visible-to mask
  (`RTSGetVisibleToMask`), so per-AI-player vision only changes how that mask is filled; the fog itself (render-target
  vision per AI player) is a separate feature the plan left out (section 7).

Seventh run (2026-11-18, strategy decided by the controllers): the player 3 and 4 controllers now carry a decision
tree built from the stock action `USubAction_AttackMoveToPlayerUnits` (the content a designer puts on a placed
controller) with direct control of idle units on. Each controller's probe tank waits idle 45 m behind its group and is
handed to the controller; only that tree can order it. Its strategic queries (bulks of hostile units) feed the tree:

| Phase | Controller | Tank moved | Fired at (rounds) | Hit (damage) |
| --- | --- | --- | --- | --- |
| Free for all | P3 | 67 m | P4 66 | P4 609 |
| Free for all | P4 | 81 m | P3 37 | P3 229 |
| Teams 1+3 v 2+4 | P3 | 67 m | P4 26 | P4 255 |
| Teams 1+3 v 2+4 | P4 | 59 m | P3 33 | P3 509 |

Both controllers' strategy chose another AI player as target, never an ally; both phases PASS.

Eighth run (2026-11-18, authored strategy): the player 3 and 4 controllers now get the designer-authored survival
decision tree (`DA_SurvivalWaveSettings`, 2 actions), applied with `AEnemyController::OverrideStrategicDecisionTree`
exactly as survival mode applies it to player 2. Each controller's tree ordered its idle tank into the fight against
another AI player: free for all P3 -> P4 (62 m, 340 damage) and P4 -> P3 (96 m, 396); teams P3 -> P4 (75 m, 260) and
P4 -> P3 (60 m, 255). Both phases PASS.

## 11. Two-player mission check (2026-11-18)

`Tools/MissionChainAudit/run_mission_chain_audit.ps1` (subsystem `Utils/RTSMapTests/MissionChainAudit`) runs each map
with the map smoke test and walks its whole Blueprint mission chain: every 25 s it fires a waiting mission's trigger
or completes the running one, so each mission's Blueprint start logic (waves, spawns, wave supply, AI start) runs. It
checks every unit's owner and its weapons' owners, the enemy controller's formation units, and who damaged whom.

| Map | Missions walked | Enemy formation units (all P2) | P1 -> P2 dmg | P2 -> P1 dmg | Own-unit dmg | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| Ger_FirstMission | 7 | 19 | 7351 | 3488 | 0 | PASS |
| Ger_MarderStugMission | 2 | 47 | 24041 | 17676 | 0 | PASS |
| Ger_Trains | 3 | 4 | 5833 | 18398 | 0 | PASS |
| RTS_GameStartTutorial | 12 | 43 | 13166 | 17773 | 0 | PASS |
| UT_EnemyAI | 1 | 211 | 25425 | 73088 | 0 | PASS |

No unit of player 3 or 4 appeared, no weapon reported another owner than its unit, no formation unit had another owner
than its controller's player. Error lines seen are not from the refactor: a repeated LogHAL line of the hidden test
console on every map, and on Ger_Trains a renderer ensure at frame 0 (distance-field update of a zero-scale primitive in
the map). Code review of the mission paths: player 2's controller passes no owner override, so its waves and spawns
take the unchanged spawner path; MissionBase/MissionManager changes only widen "enemy" to every AI player.

## 12. Wall gates (`Environment/DestructableEnvActor/WallGate/**`)

Rules (2026-11-18):
- **Who opens a gate.** A gate opens by itself only for tanks and squad units of its `AllowedPlayer` and that
  player's allies (`FRTSPlayers::GetFriendlyMask(AllowedPlayer)`), never for a hostile player. The relations are read
  on every detection scan (0.3 s), so a team change applies from the next scan; no relations version is needed. A
  neutral allowed player (0) opens for nobody. Unset owners (222) count as player 2.
- **Allowed player vs owner.** Decision: the allowed player is the gate's owner unless a designer set it apart.
  `AllowedPlayer` (unchanged `uint8` property, default 2) left at the class default takes the RTS owner the gate begins
  play with (a gate placed or spawned for player 3 opens for player 3); a value changed from the default is kept even
  when it differs from the owner, with a warning at BeginPlay (the owner alone decides whose shots pass the gate). On
  every placed gate both are 2 (no asset overrides either), so nothing placed changes. When the owner changes at
  runtime (`URTSComponent::OnOwningPlayerChanged`, a capture) and `AllowedPlayer` was the old owner, it follows the
  new owner. `AWallGate::SetAllowedPlayer` (BlueprintCallable) hands a gate over explicitly. A neutral-owned gate
  keeps its `AllowedPlayer`.
- **Path planning.** One closed-doorway nav area class per player: `UNavArea_WallGatePlayerSide` (player 1),
  `UNavArea_WallGateEnemySide` (player 2; old names kept so saved navmesh data and the two-player game are unchanged),
  `UNavArea_WallGatePlayer3`, `UNavArea_WallGatePlayer4` (`RTSWallGateNavAreas::GetClosedGateAreaOfPlayer`). A closed
  gate marks its doorway with its allowed player's area. `URTSWallGateNavQueryFilter` (parent of every
  `RTS_NavFilter_*`, already instantiated per querier) excludes the area of every player that is not allied to the
  querier's owner: four `SetExcludedArea` calls per path request, nothing per gate. So AI vs AI paths use the
  per-owner filter like human paths did, a gate changing owner needs no filter change (only its area class changes),
  and a 1v1 game builds exactly the old exclusions. Queriers without an RTS unit and neutral units treat every
  closed gate as closed (before, a neutral unit counted as the AI side). An open gate is plain ground for everyone;
  `NavMode BlockEveryone` and the fallback stay `NavArea_Null`.
- **Known limit.** A filter is built when a path is requested; repaths reuse it (`FPathFindingQuery(PathToRecalculate)`
  copies the old query data). A unit that planned through an ally's closed gate keeps that plan after the alliance
  breaks until its next move order, and is then stopped by the closed leaf.
- **Collision.** Unchanged and verified: the frame gets the owner's weapon responses and owner bit through
  `SetupWallCollision` -> `FRTSPlayerCollision::SetWeaponHitResponses`; the leaf copies the frame's responses and
  `CopyOwnerMaskFilter`; a runtime owner change re-marks both through `ApplyOwnerMaskToActor` (and
  `ApplySideChangeToActor` across human <-> AI). Allied shots pass the closed leaf, hostile shots stop on it; damage
  handling of the destructible gate is untouched.

Test: `Tools/WallGates/run_wall_gate_relations_test.ps1` (`UWallGateRelationsTestSubsystem`, `-WallGateRelationsTest`,
map tests only) on `RTS_ToCopySmall`: gate of player 2, one Pz IV H each for players 1, 2, 3 parked 60 m apart and
brought to the doorway one at a time. Checks: opens for P2; stays shut for P3 and P1; after `SetPlayerTeam(3, 2)`
opens for P3; path through the closed doorway (infantry filter, tank as querier) for P2 and not P3/P1, then for P3
under the team; weapon traces at the closed leaf per player; capture by P4 moves the allowed player and the doorway
area to P4 and keeps P2 out; a second gate spawned for P3 (`SpawnUnitForPlayer`) reports allowed player 3 and P3's
doorway area. Prints `RTS_WALLGATE RESULT PASS|FAIL reasons=...`. `Docs/Environment/WallGates.md` still describes the
older human/AI split and was not updated.
