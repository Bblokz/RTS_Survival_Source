# 40 — Soviet waves and the survival enemy AI

Owner: waves/AI agent. Code: `Source/RTS_Survival/SurvivalMode/Waves/`. Data: `Content/RTS_Survival/SurvivalMode/Data/Waves/`.
Default data: `Docs/wave_defaults.json` (identical to the `USurvivalWaveSettings` C++ constructor — a freshly created
asset already holds every value below).

## 1. Files

| File | What |
|---|---|
| `SurvivalWaveTypes.h/.cpp` | Enums (`ESurvivalWaveKind`, `ESurvivalWaveUnitRole`), pool entry, tech stage, wave-kind definition, per-difficulty profile, `FSurvivalWaveAnnouncement`, the two HUD delegates |
| `SurvivalWaveSettings.h/.cpp` | `USurvivalWaveSettings` data asset (all tuning + defaults), `FSurvivalStrategicAISettings`, `RTS_SURVIVAL_WAVES_WITH_TREE_OVERRIDE` switch |
| `SurvivalWaveComposer.h/.cpp` | Stateless budget → units logic (tech stage, weights, fill, upgrade pass) |
| `SurvivalEdgeSpawnZone.h/.cpp` | `ASurvivalEdgeSpawnZone`: box + inward arrow + label, weight, "only after minute N", navmesh spawn points |
| `SurvivalWaveDirectorComponent.h/.cpp` | Scheduling, kind roll, composition, alive cap, edge selection, launch through `AEnemyController`, formation tracking and retargeting, HUD broadcast |
| `SurvivalStrategicAICoordinator.h/.cpp` | Configures the map's strategic AI for survival (difficulty, start, direct control, pick limits, training income, decision tree) |

## 2. Ownership rule (waves vs strategic AI)

There is exactly one owner per unit at any time, and the hand-over already exists in the engine:

| Unit state | Owner | Why it cannot be grabbed by the other |
|---|---|---|
| Spawning / in a wave formation (edge → base) | **Wave director** (`UEnemyFormationController` formation it created) | The strategic AI only picks *idle blackboard* units; formation units are never on the blackboard. |
| Wave formation reached its final waypoint | **Strategic AI** | `UEnemyFormationController::OnFormationReachedFinalDestination → RegisterValidFormationUnitsAsIdle → UEnemyDirectControlComponent` puts the survivors on the blackboard. The wave director stops tracking a group the moment its formation disappears and never commands it again. |
| Soviet base garrisons, units trained at Soviet bases, global-ability drops | **Strategic AI** | Trained units go through `IssueOrdersToSpawnedBlackboardUnit`; drops through `RegisterDroppedUnitWithBlackboardWhenReady`. |

Consequences / rules:
- Retargeting touches only formations the director can identify as its own (exact last-waypoint match, see §8).
- Nothing on a survival map may call `AEnemyController::RetreatAllFormations` (it would hijack every wave formation).
  Only test code and Blueprints call it today; keep it that way on survival maps.
- The direct-control damaged-vehicle retreat only rosters *registered* (post-arrival) units, so it does not pull units
  out of en-route wave formations.

## 3. Growth model

`t` = survival minutes (since the first wave, `ASurvivalModeDirector::GetElapsedSurvivalSeconds()/60`).
Every quantity is a `FRuntimeFloatCurve` keyed by `t` inside `FSurvivalWaveDifficultyProfile` (one per
`ERTSGameDifficulty`; inline keys or an external `UCurveFloat`). The defaults are one **base (Normal) curve** per
quantity, turned into a per-difficulty curve by compressing time with `GrowthPace` and scaling the value:

`Value_d(t) = Scale_d × Base(t × GrowthPace_d)` (keys stored at `minute / GrowthPace_d`).

| Base curve (Normal) | keys `(minute, value)` |
|---|---|
| WaveIntervalSeconds | (0,100) (5,90) (10,80) (20,70) (40,60) (60,55) |
| WaveBudget (training points, 10 = light tank) | (0,30) (5,60) (10,100) (20,190) (30,300) (40,420) (60,680) (90,1100) (120,1500) |
| EdgesPerWave (fraction = chance of one more) | (0,1) (6,1) (12,1.5) (20,2) (30,2.5) (45,3) (60,3.5) |
| SpecialWaveChance | (0,0) (6,0.15) (15,0.25) (30,0.35) (60,0.45) |
| MaxUnitsPerWave | (0,8) (10,14) (20,18) (40,24) (60,28) |
| StrategicTrainingPointsPerMinute (Soviet bases) | (0,4) (10,8) (20,12) (40,18) (60,24) |

| Difficulty | GrowthPace | Budget × | Interval × | MaxUnits × | TechPace | Alive load cap | Strategic income × | AI picks min/max |
|---|---|---|---|---|---|---|---|---|
| NewToRTS | 0.75 | 0.6 | 1.25 | 0.8 | 0.7 | 50 | 0.6 | 3 / 6 |
| Normal | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 | 65 | 1.0 | 3 / 8 |
| Hard | 1.15 | 1.25 | 0.92 | 1.1 | 1.2 | 75 | 1.25 | 2 / 10 |
| Brutal | 1.3 | 1.5 | 0.85 | 1.2 | 1.4 | 85 | 1.5 | 2 / 12 |
| Ironman | 1.45 | 1.75 | 0.8 | 1.3 | 1.6 | 90 | 1.75 | 2 / 14 |

Edges and special chance use time compression only. `DifficultyPercentage` (DefaultGame.ini: 100/100/150/250/250) is
**not** used by default; `bScaleBudgetByDifficultyPercentage` multiplies budgets by `Percentage/100` clamped to [0.5, 2].
Each interval and budget is randomised ±10 %.

### What it looks like (regular waves, simulated with the exact composer, 400 rolls)

"Avg units (tanks)" = mean wave size and mean vehicles in it; the example is one roll. Special % is the chance a
non-milestone wave is special. Simulation script: same algorithm as `FSurvivalWaveComposer`. Long example
rows are abbreviated ("infantry ×6", "…").

#### NewToRTS
| Minute | Interval (s) | Budget | Edges | Max units | Tech stage | Special % | Avg units (tanks) | Example regular wave |
|---|---|---|---|---|---|---|---|---|
| 0 | 125 | 18 | 1.00 | 6 | BasicInfantry | 0 | 3.4 (0.5) | 1x Dyakonov, 1x Mosin, 1x Ba14 |
| 5 | 116 | 31 | 1.00 | 8 | LightTanks | 9 | 4.6 (1.7) | 1x KorovinAssault, 1x Mosin, 1x T70, 1x T70F |
| 10 | 106 | 48 | 1.12 | 10 | MediumTanks | 17 | 5.8 (2.9) | 2x Mosin, 1x Ba12, 1x T34_76, 1x T28 |
| 20 | 94 | 87 | 1.69 | 13 | AdvancedInfantry | 25 | 7.1 (3.7) | 2x Mosin, 1x T70, 1x RedHammer, 1x RedHamerPTRS, 1x T28, 1x T34_85, 1x T70F |
| 40 | 81 | 180 | 2.50 | 17 | Tier3 | 35 | 9.2 (5.2) | 2x Mosin, 2x LargePTRS, 1x T34_76_L, 1x KV_1_Arc, 1x IS_1, 1x ToxicGuard, 1x ROKS3_PPSH, 1x Kvarc77, 1x KorovinAssault |
| 60 | 73 | 291 | 3.00 | 20 | Experimental | 40 | 12.3 (7.6) | 2x SU_100, 1x SU_85, 1x IS_1, 1x IS_2, 1x T34_85, 1x T34_76, infantry ×6 |

#### Normal
| Minute | Interval (s) | Budget | Edges | Max units | Tech stage | Special % | Avg units (tanks) | Example regular wave |
|---|---|---|---|---|---|---|---|---|
| 0 | 100 | 30 | 1.00 | 8 | BasicInfantry | 0 | 5.3 (1.0) | 2x Ba14, 1x Dyakonov, 1x Mosin, 1x Okhotnik |
| 5 | 90 | 60 | 1.00 | 11 | LightTanks | 12 | 8.0 (3.8) | 3x Mosin, 2x T70F, 1x KorovinAssault, 1x Ba14, 1x BT7, 1x ROKS3_PPSH |
| 10 | 80 | 100 | 1.33 | 14 | Tier2 | 19 | 9.7 (5.5) | 4x Mosin, 1x BT7, 1x T34_76_L, 1x SU_122, 1x BT7_4, 1x SU_76, 1x T70 |
| 20 | 70 | 190 | 2.00 | 18 | Tier3 | 28 | 12.0 (6.5) | 2x T34_76_L, 1x T34_100, 1x SU_122, 1x T28, 1x T26, 1x SU_76, 2x RedHammer, 1x Tucha12T, infantry ×4 |
| 40 | 60 | 420 | 2.83 | 24 | Experimental | 38 | 17.1 (11.1) | 2x IS_2, 2x SU_100, 1x IS_1, 1x KV_1, 1x SU_85, 1x T34_76, 1x T34_76_L, light ×3, infantry ×7 |
| 60 | 55 | 680 | 3.50 | 28 | Experimental | 45 | 25.7 (18.0) | 3x KV_1, 2x IS_1, 2x SU_100, 1x IS_2, 1x IS_3, 2x T34_76_L, 1x T34_85, … infantry ×9 |

#### Hard
| Minute | Interval (s) | Budget | Edges | Max units | Tech stage | Special % | Avg units (tanks) |
|---|---|---|---|---|---|---|---|
| 0 | 92 | 38 | 1.00 | 9 | BasicInfantry | 0 | 6.6 (1.3) |
| 5 | 81 | 82 | 1.00 | 13 | MediumTanks | 14 | 10.2 (5.3) |
| 10 | 72 | 142 | 1.46 | 16 | Tier2 | 21 | 11.8 (6.9) |
| 20 | 63 | 279 | 2.15 | 21 | Tier3 | 30 | 13.8 (8.4) |
| 40 | 54 | 622 | 3.03 | 28 | Experimental | 40 | 24.5 (16.5) |
| 60 | 51 | 1007 | 3.50 | 31 | Experimental | 45 | 30.9 (25.8) |

#### Brutal
| Minute | Interval (s) | Budget | Edges | Max units | Tech stage | Special % | Avg units (tanks) |
|---|---|---|---|---|---|---|---|
| 0 | 85 | 45 | 1.00 | 10 | BasicInfantry | 0 | 7.8 (1.6) |
| 5 | 74 | 108 | 1.04 | 14 | MediumTanks | 16 | 12.4 (7.0) |
| 10 | 65 | 191 | 1.56 | 18 | AdvancedInfantry | 23 | 14.3 (8.4) |
| 20 | 57 | 384 | 2.30 | 24 | Tier3 | 32 | 18.0 (11.0) |
| 40 | 48 | 864 | 3.23 | 32 | Experimental | 42 | 30.9 (22.8) |
| 60 | 47 | 1398 | 3.50 | 34 | Experimental | 45 | 34.0 (30.9) |

#### Ironman
| Minute | Interval (s) | Budget | Edges | Max units | Tech stage | Special % | Avg units (tanks) |
|---|---|---|---|---|---|---|---|
| 0 | 80 | 52 | 1.00 | 10 | BasicInfantry | 0 | 8.9 (1.9) |
| 5 | 68 | 136 | 1.10 | 16 | MediumTanks | 16 | 14.9 (9.0) |
| 10 | 60 | 246 | 1.66 | 21 | AdvancedInfantry | 24 | 17.7 (9.9) |
| 20 | 52 | 506 | 2.45 | 27 | Experimental | 34 | 21.7 (14.6) |
| 40 | 44 | 1145 | 3.43 | 36 | Experimental | 44 | 35.9 (29.2) |
| 60 | 44 | 1852 | 3.50 | 36 | Experimental | 45 | 36.0 (32.5) |

From roughly minute 25 (Normal) the alive-load cap (§6), not the budget, limits how many Soviets are on the map;
the budget then keeps buying *better* units (upgrade pass) rather than more.

## 4. Tech stages and unit tiers

Stages reuse `EEnemyAITechLevel` (the strategic AI's enum) but unlock by **survival minute / TechPace**, not by
buildings. A newly unlocked stage enters at 25 % weight and ramps to full over 3 minutes
(`NewTechStageRampInMinutes`, `NewTechStageStartWeightFraction`). Early units fade to 35 % weight after their
`FadeStartMinute` (also divided by TechPace).

| Stage | Normal minute | NewToRTS | Normal | Hard | Brutal | Ironman |
|---|---|---|---|---|---|---|
| BasicInfantry | 0 | 0 | 0 | 0 | 0 | 0 |
| LightTanks | 2 | 2.9 | 2 | 1.7 | 1.4 | 1.2 |
| MediumTanks | 6 | 8.6 | 6 | 5.0 | 4.3 | 3.8 |
| Tier2 | 10 | 14.3 | 10 | 8.3 | 7.1 | 6.2 |
| AdvancedInfantry | 14 | 20.0 | 14 | 11.7 | 10.0 | 8.8 |
| Tier3 | 20 | 28.6 | 20 | 16.7 | 14.3 | 12.5 |
| Experimental | 32 | 45.7 | 32 | 26.7 | 22.9 | 20.0 |

Costs are `EnemyTrainingHelpers` training points (shared economy with the strategic AI); `CostOverride` replaces them.

| Stage | Units (role, weight, cost, fade minute) |
|---|---|
| BasicInfantry | Mosin (Inf 3, 3p, 20) · Okhotnik (AT-Inf 2, 4p, 25) · Dyakonov (Inf 2, 5p, 30) · ROKS3 PPSh (Inf 1.5, 6p, 30) · Korovin Assault (Inf 1.5, 7p) · BA-12 / BA-14 (ArmCar 1.5, 10p, 15) |
| LightTanks | BT-7 (2, 10p, 20) · T-26 (2, 10p, 18) · BT-7-4 (1.5, 10p, 22) · T-70 (1.5, 12p, 25) · T-70F (1, 12p, 25) · SU-76 (TD 1, 12p, 25) |
| MediumTanks | T-34/76 (3, 20p) · T-28 (1.5, 15p, 35) · T-28F (1, 15p, 35) · T-34-AA (AA 0.3, 20p) |
| Tier2 | T-34/85 (3, 28p) · T-34/76L (2, 28p) · T-34E (1.5, 25p) · SU-85 (TD 1.5, 28p) · SU-122 (Art 1, 25p) · BM-13 (Art 0.8, 25p) · KV-1 (Heavy 1.5, 28p) · Large PTRS (AT-Inf 1.5, 10p) · Red Hammer (Inf 1.5, 10p) · Red Hammer PTRS (AT-Inf 1.5, 12p) · Toxic Guard (Inf 1, 15p) |
| AdvancedInfantry | Ghosts of Stalingrad (Elite 1, 15p) · Kvarc-77 (Elite 1, 25p) · Tucha-12T (Elite 1, 25p) · Cortex Officer (Elite 0.6, 30p) |
| Tier3 | T-34/100 (2, 30p) · T-44/100L (1, 45p) · SU-100 (TD 1.5, 35p) · ISU-152 (TD 0.8, 55p) · KV-2 (Heavy 1, 40p) · KV-1E (1.5, 40p) · KV-1 Arc (1, 40p) · IS-1 (1.5, 50p) · IS-2 (1.5, 55p) · T-35 (0.4, 28p) |
| Experimental | IS-3 (SuperHeavy 1, 100p) · IS-3 Flame (0.7, 100p) · KV-5 (0.7, 100p) · IS-6 (0.7, 75p) · SMK (0.7, 70p) |

Pool verification: every entry spawned through the real async spawner in the unit roster test
(`E:\RR_test\results\units\roster_all.txt`, 147/147 training options). **Deliberately excluded:**
- Team weapons (Maxim, mortars, Zis-76, 53-K, BS-3, ML-20, M1938, B-4, KS-30): deploy logic fights attack-move
  formations and they report `SpatialVoiceLinePlayer` errors (roster WARN).
- Hazmat engineers: direct control pulls hazmats off for vehicle repair.
- Never spawned in any roster run, probably absent from the spawner map: SU-85L, KV-IS, SU-152, T-44/155, ISU-130,
  MS-1, Zis-57, Bofors, DShK, LargeMixBasic, LargeMixT2, Medic, HazmatBazooka, T-34 Harvester (several of these are in
  the `FEnemyLevelTraining` defaults — the survival map's AEnemyController should not list them either).
- Soviet aircraft: the wave controller destroys any spawned actor that is not a tank or squad, so air is done with
  global abilities (§5).

### Composition algorithm (`FSurvivalWaveComposer`)
1. Pool = entries whose stage is unlocked; weight = `Weight × ramp × fade × kind.RoleMultiplier(Role)`.
2. Fill: weighted pick among entries with `cost ≤ remaining budget` until no entry fits or `MaxUnitsPerWave` is hit
   (always at least one unit).
3. Upgrade pass (only when the unit cap was hit): up to 16 times, replace the cheapest unit by a weighted pick costing
   more but within `old cost + remaining budget`.
4. The alive-load cap trims the cheapest units (§6); the wave is shuffled and dealt round-robin over its edges.

## 5. Wave kinds

- **Regular** — artillery ×0.4, super-heavies ×0.5, AA ×0.5.
- **Milestone ("Major offensive")** — the first wave after every 10 survival minutes: budget ×1.5, max units ×1.3,
  +1 edge, heavy/super-heavy ×1.5. A milestone delayed by the cap keeps its milestone status.
- **Specials** roll with `SpecialWaveChance(t)`; never fewer than 2 waves apart (`MinWavesBetweenSpecials`), forced
  after 6 regular waves once specials are possible (`MaxWavesBetweenSpecials`).

| Special | Weight | Min minute (Normal / NewToRTS / Hard / Brutal / Ironman) | Effect |
|---|---|---|---|
| Blitz | 1.0 | 3 / 4.0 / 2.6 / 2.3 / 2.1 | +1 edge; armoured cars & light tanks ×4, mediums ×1.5, rest ×0.15 |
| InfantryRush | 1.0 | 4 / 5.3 / 3.5 / 3.1 / 2.8 | budget ×1.2, max units ×1.6; infantry ×4, AT-inf ×3, elite ×2, rest ×0.1 |
| Artillery | 0.8 | 10 / 13.3 / 8.7 / 7.7 / 6.9 | SPGs/rockets ×6, TD ×1.5, infantry screen; + 1 barrage (GA_SovietSmallBarrage / LargeBarrage / LargeBarrageVariation) on the base if ready |
| HeavyArmour | 0.8 | 14 / 18.7 / 12.2 / 10.8 / 9.7 | budget ×1.3; heavy ×4, super-heavy ×3, TD ×2, medium ×1.5, rest ×0.2 |
| AirRaid | 0.7 | 8 / 10.7 / 7.0 / 6.2 / 5.5 | 1 cast of GA_2xPE2Strafing / GA_4xPE2Strafing / GA_HeavyCarpet_PE8_500Gr / _1000Gr on the base + escort at budget ×0.6. **Only rolls when one of them is loaded and off cooldown.** |
| Airborne | 0.6 | 12 / 16.0 / 10.4 / 9.2 / 8.3 | 1 cast of GA_SovietParatroopers / InfantryDrop / T34Drop / HeavyTankDrop 3500 uu from the base + escort at budget ×0.7. Only rolls when ready. Dropped units belong to the strategic AI. |

Normal minute 20 simulation: Milestone 16.5 units (9.7 vehicles), Blitz 12.5 (11.1), InfantryRush 23.2 (0.9),
Artillery 11.7 (6.2), HeavyArmour 10.4 (8.6), AirRaid escort 7.6, Airborne escort 8.6.

Global abilities go through `AEnemyController::GetEnemyGlobalAbilitiesManager()->TryExecuteAbilityAtLocation` (cooldown
and requirement checks inside). They must be in the AEnemyController loadout (§10).

## 6. Alive cap (performance)

`AliveLoad = enemy tanks + enemy soldiers × 0.25 (+ planned load of launched groups that have not spawned yet)`, read
from `UGameUnitManager` for player 2 — i.e. **all** Soviet units, including base garrisons. Cap = profile
`AliveEnemyLoadCap × PerformanceLoadCapMultiplier` (50/65/75/85/90). A planned squad counts 1.25, a tank 1.
- A due wave is trimmed (cheapest first) to fit the free load.
- If it would launch fewer than `min(MinUnitsToLaunch = 3, untrimmed size)` units it waits: retried every 8 s,
  `GetIsWaitingForUnitCap()` is true, `GetSecondsUntilNextWave()` returns 0. The wave clock (budget, stage) keeps
  advancing, so the delayed wave is stronger when it comes.
- Why not wave supply: `AEnemyController` wave supply is consumed per wave unit and refunded on death, but the
  refund also fires for units the strategic AI later puts in its own formations (double refund through
  `OnFormationUnitDiedPostReachFinal` + `CleanupInvalidFormations`) and for strategic-AI units that never consumed
  supply, so it drifts upward and is no cap. The director only tops supply up (`SetEnemyWaveSupply`) so it never
  silently drops a planned unit.

## 7. Edges (`ASurvivalEdgeSpawnZone`)

Placement (level designer): a box per edge stretch; **Y extent = half the length along the edge, X = depth, forward
arrow points into the map**. Put it on/near the navmesh border (points that miss are retried up to 3 × 600 uu
inward; a zone with no navmesh at all is reported). Properties: `DisplayName` (HUD; empty = compass from the base,
+X north / +Y east), `bEnabled`, `SelectionWeight`, `ActiveAfterMinute` ("only after minute N"), `MaxUnitsPerWave`
(narrow passes; surplus goes to the other edges of the wave), `NavProjectionHalfHeight`. The label in the editor
shows name, weight and minute.

Selection per wave:
1. Count = `floor(EdgesPerWave(t))` + chance of one more (fraction) + kind's `ExtraEdges`, clamped to eligible zones.
2. Eligible = enabled, weight > 0, `t ≥ ActiveAfterMinute`, ≥ 8000 uu from the base. Rules relax in order (distance,
   then minute) rather than launching nothing.
3. Weighted pick of distinct zones; zones used by the last 2 waves ×0.35; every extra edge is weighted by its angular
   separation from already-picked edges (`EdgeAngularSpreadBias` 0.75: 0° → ×0.33, 180° → ×1), so multi-edge waves
   come from different sides.
4. Spawn points: a front centred at a random spot along the zone, 700 uu apart, extra rows step inward, projected to
   the navmesh. Groups of more than 12 units per edge are split into several formations.

## 8. Targeting and retargeting

- Target = `ASurvivalModeDirector::GetCurrentPlayerBaseLocation()` evaluated when the wave launches.
- Every formation gets a **unique final waypoint**: a random point within 900 uu of the base, projected to the
  navmesh, nudged until ≥ 50 uu from every other tracked group's. Waypoints = staging point (2500 uu inward of the
  zone) → nav path resampled every 8000 uu (`StochasticHelpers::BuildNavigablePathPoints`, the strategic AI's own
  path builder) → final waypoint (forced to be exactly last).
- Launch: `AEnemyController::CreateSingleAttackMoveWave(Wave_NoOwningBuilding, one element per unit with a single
  option, waypoints, …, TimeTillWave = WarningLeadSeconds 10 s, MaxAttackTimeBeforeAdvancing 40 s)`. A positive
  linger time matters: ≤ 0 makes an attack-move formation hold the first contested waypoint forever.
- Every 2 s the director reads `UEnemyFormationController::GetActiveFormationData` and finds its groups by exact
  last waypoint. If the base moved ≥ 3000 uu from the point a group was aimed at (12 s cooldown), the group is
  re-aimed: `RemoveActiveFormationsByID` (unbinds the units' idle delegates) immediately followed by
  `AEnemyController::MoveAttackMoveFormationToLocation` with the same units and a fresh path from their centroid.
  Back to back, so no unit is ever outside a formation (no supply leak, never on the blackboard).
- A group whose formation disappears after being seen has arrived (survivors → strategic AI) or died; a group never
  seen within 120 s is dropped from tracking.

## 9. Survival decision tree and strategic AI configuration

`USurvivalStrategicAICoordinator` (created by the wave director) applies, using only public enemy API:

| When | What |
|---|---|
| Grace period starts (director `OnPhaseChanged`) | once: blackboard `GameDifficulty` = director difficulty (no MissionManager on survival maps), training points ≥ 20, decision tree override (§11 request 1), mission settings; then, with `StartMoment = GracePeriodStart`, `StartStrategicAIThinkingTimer()` with direct control **off** (Soviet bases train and defend; the tree does not attack the freshly landed player) |
| `StartWaves` | direct control **on**, start thinking (no-op if already running; works for any start condition on the map's controller), profile updates every 10 s |
| Every 10 s | `TrainingPointsPerMinute` = profile curve (4→24 Normal), min/max picked units per difficulty |
| `StopWaves` (defeat) | direct control and unit training off |

Behaviour is `PreferAggressive`. The decision tree (author as `DecisionTreeTemplate` in the wave settings asset, or
directly on the survival map's `RTS_BP_EnemyController` instance until the override hook exists):

| Top-level | Score | Sub-action | Score | Requirements / notes |
|---|---|---|---|---|
| **Attack** | 2.0 | AttackMoveToPlayerHQ | 3.0 | native: player HQ known. Finishes the job with wave survivors and follows a relocating nomadic HQ. |
| | | AttackMoveToPlayerUnits | 2.0 | harass player forces/expansions away from the base |
| | | AttackMoveToPlayerResourceBuildings | 1.5 | + `GameTimePassed 480 s`; harass harvesting |
| | | HeavyTankPushPlayerBaseOrUnits | 2.5 | native: ≥ 2 idle heavies (late survivors) |
| | | LightTanksAttackPlayerUnits | 1.5 | native: ≥ 5 idle light tanks |
| | | FlankPlayerHeavies | 1.5 | native: flank positions + idle tanks |
| **Defend** | 1.0 | DefendBase | 3.0 | tanks to Soviet base defence points, hazmats build hedgehogs (3 per hazmat); regroups survivors when bases are attacked |

Picks per action: 3–6 / 3–8 / 2–10 / 2–12 / 2–14 by difficulty. Path settings as in `wave_defaults.json`.
Unit abilities are used automatically by `UEnemyUnitAbilityAIComponent`; global abilities by the controller's own
global-ability think step (shared cooldowns with the Artillery/AirRaid/Airborne waves).

Cross-agent notes:
- **Soviet-base agent:** the strategic AI's own tech still comes from building expansions: `BTX_RusBarracks` →
  BasicInfantry, `BTX_RusFactory` / `BTX_RusCoolingTowers` → LightTanks, `BTX_RusResearchCenter` → Tier2; bases need
  enemy trainer components (`InitTrainerAsEnemyTrainerComponent`) or the AI trains nothing. Base buildings are what
  `FindEnemyBase_TimerRequest` clusters into DefendBase points.
- **Mission-UI agent:** see §12.

## 10. Survival map setup (level designer)

- One `RTS_BP_EnemyController` (the director and coordinator find it via `FRTS_Statics::GetEnemyController`).
  `M_EnemyAIMissionSettings.StrategicAIThinkingStartCondition = StartAfterMissionTrigger`,
  `bAllowDirectControlStochasticDecisionTree = false` (the coordinator turns it on), `bAllowUnitTraining = true`.
  `M_EnemyLevelTraining`: remove the never-spawned subtypes listed in §4.
- Global ability loadout on that controller: `GA_SovietSmallBarrage, GA_SovietLargeBarrage,
  GA_SovietLargeBarrageVariation, GA_2xPE2Strafing, GA_4xPE2Strafing, GA_HeavyCarpet_PE8_500Gr,
  GA_HeavyCarpet_PE8_1000Gr, GA_SovietParatroopers, GA_SovietInfantryDrop, GA_SovietT34Drop, GA_SovietHeavyTankDrop`
  (without them AirRaid/Airborne never roll and Artillery waves have no barrage). Suggested
  `M_EnemyGlobalAbilityAISettings`: ThinkStepInterval 20, TimeTillAllowBarrages 480, TimeTillAllowHeavyCarpetBombing 1200.
- 6–10 `ASurvivalEdgeSpawnZone`s covering all four edges; one or two with `ActiveAfterMinute` 10–20 for late surprises.

## 11. Integration requests

### 1. Decision tree override (needed for `bOverrideDecisionTree`; optional otherwise)

`Enemy/EnemyController/EnemyController.h`, public, below `GetStrategicDecisionTree();`:
```cpp
	/**
	 * @brief Replaces the map-authored decision tree with a copy of Template, for game modes that share one tree
	 * across maps (survival). Call after BeginPlay; think timers keep running.
	 */
	void OverrideStrategicDecisionTree(const UStochasticDecisionTree* Template);
```
`Enemy/EnemyController/EnemyController.cpp`:
```cpp
void AEnemyController::OverrideStrategicDecisionTree(const UStochasticDecisionTree* Template)
{
	if (not IsValid(Template))
	{
		RTSFunctionLibrary::ReportError("OverrideStrategicDecisionTree called without a template tree.");
		return;
	}
	UStochasticDecisionTree* NewTree = DuplicateObject<UStochasticDecisionTree>(Template, this);
	if (not IsValid(NewTree))
	{
		RTSFunctionLibrary::ReportError("Failed to duplicate the strategic decision tree template.");
		return;
	}
	M_StochasticDecisionTree = NewTree;
	BeginPlay_InitStochasticDecisionTree();
	if (GetIsValidEnemyStrategicAIComponent())
	{
		M_EnemyStrategicAIComponent->SetStochasticDecisionTree(M_StochasticDecisionTree);
	}
}
```
`Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h`, public, below `StartStrategicAIThinkingTimer();`:
```cpp
	// Swaps the tree after init (AEnemyController::OverrideStrategicDecisionTree); think timers are untouched.
	void SetStochasticDecisionTree(UStochasticDecisionTree* NewTree) { M_StochasticDecisionTree = NewTree; }
```
Then set `#define RTS_SURVIVAL_WAVES_WITH_TREE_OVERRIDE 1` in `SurvivalMode/Waves/SurvivalWaveSettings.h` (or tell me
and I flip it). Calling `InitStrategicAIComponent` twice instead is unsafe (it re-adds think timers).

### 2. Director (integrator-owned, no new code expected)
- `InitWaves(this)` in BeginPlay before any phase change (the component subscribes to `OnPhaseChanged` there; it
  needs the `GracePeriod` transition), `StartWaves()` at grace end, `StopWaves()` on defeat.
- `GetEdgeSpawnZones()` must be gathered at BeginPlay; `GetDifficulty()` and `GetElapsedSurvivalSeconds()` must work
  from the Waves phase on (the director falls back to its own clock if the latter returns 0).
- `M_WaveDirector` must be created as a default subobject of the director (it is in the header already).

### 3. Data asset
After the build: create `DA_SurvivalWaveSettings` (class `SurvivalWaveSettings`) in
`Content/RTS_Survival/SurvivalMode/Data/Waves/` — it already holds the defaults — and assign it to the survival
mode settings' `WaveSettings`. I can create it (and author the decision tree template) once the class exists in the
editor; `wave_defaults.json` lists every value for review/import (curves as `[minute, value]`, enums by name,
`_comment` / `_cost` keys are informational and must be skipped by an import script).

## 12. HUD API (for the mission-UI agent)

On `USurvivalWaveDirectorComponent` (`Director->GetWaveDirector()`):
- `FOnSurvivalWaveIncoming OnWaveIncoming` — native multicast `void(const FSurvivalWaveAnnouncement&)`, broadcast when a
  wave launches, `SecondsUntilSpawn` (10 s) before its units appear. Fields: `WaveNumber`, `Kind`, `KindDisplayName`,
  `EdgeNames`, `EdgeLocations`, `EdgeDirectionsFromBase` (2D, for arrows), `TargetLocation`, `UnitCount`, `Budget`,
  `TechStage`, `bIsSpecial`, `GlobalAbilitiesCast`, `Message` (e.g. "Wave 7 (Heavy armour) incoming from the North and
  East"). Bind with `AddUObject`; the component clears it in EndPlay.
- `FOnSurvivalTechStageReached OnTechStageReached` — `void(EEnemyAITechLevel, const FText& AnnouncementText)`.
- Polling: `GetSecondsUntilNextWave()`, `GetNextWaveNumber()`, `GetWavesLaunched()`, `GetCurrentTechStage()`,
  `GetIsWaitingForUnitCap()`, `GetAliveEnemyLoad()` / `GetAliveEnemyLoadCap()`, `GetLastAnnouncement()`.

## 13. Risks

- Not compiled or PIE-tested yet (no build allowed for this agent).
- Formation identification relies on `FFormationData::FormationWaypoints.Last()` being the exact point passed in
  (verified: only `InitWaypointsAndDirections` writes it for attack-move formations; random-patrol formations rewrite
  it but are skipped). If the formation controller starts projecting or
  editing waypoints, retargeting silently stops (waves still attack the launch-time base and survivors are then
  handled by the strategic AI's AttackMoveToPlayerHQ). A cleaner hook would be a non-test
  `UEnemyWaveController` delegate `(WaveID, FormationID)`; not requested to keep shared changes minimal.
- `AttackMoveToPlayerHQ` needs the strategic AI's player-count request to find the nomadic HQ; verify on the
  survival map, otherwise post-arrival survivors only use AttackMoveToPlayerUnits.
- The alive cap counts all Soviet units; very large generated Soviet bases eat into the wave budget of units.
- Global-ability cooldowns are shared with the strategic AI's own ability think step.
- Units outside the verified pool (or removed from the async spawner later) fail to spawn and are refunded with an
  error per unit.
- Team-weapon squads remain excluded; if wanted in Artillery waves they need deploy-on-arrival handling first.
