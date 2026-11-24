# Soviet AI harvest economy: decisions (2026-11-24)

Goal: the Soviet AI (player 2) harvests with its own T-34 "Breaker" harvesters, delivers to its own refinery, and turns
the delivered resources into **training points** and **building points**. This is Phase 4 of
[EnemyBaseBuilding_Plan.md](EnemyBaseBuilding_Plan.md). No code yet.

---

## 1. Decisions (picked by the owner)

| # | Question | Pick |
|---|---|---|
| H1 | How does harvest plug into AI spending? | **Two pools, both fed.** Training points and building points stay separate (matches plan D1). Harvest income is converted into both. |
| H2 | Which resource feeds which pool? | **Radixite → training points, Metal → building points.** Each resource has one job, so the player can read the AI's plans from what it harvests. |
| H3 | Relation to today's timer income? | **Timer unchanged; harvest is a bonus on top.** `TrainingPointsPerMinute` (and survival's curve) stay as they are. Maps without fields work exactly as today. |
| H4 | When do deliveries become points? | **Refinery stockpile.** Delivered loads go into the refinery's stock and convert to points at a fixed rate, like the player's converter. The stock is visible; destroying the refinery loses it. |
| H5 | How does the AI get and keep harvesters? | **Each refinery trains its own, free.** Every refinery keeps up to **N** harvesters; N is set on the enemy AI controller (per mission, so some missions get more). A refinery starts with none (unless harvesters are hand-placed on the map) and trains until it has N, replacing losses at no cost. |
| H6 | Which building is the drop-off? | **A new dedicated Soviet refinery building**, with an unload animation setup for the harvester's cargo (like the German HQ crane / refinery knuckle boom / depot elevator). Design starts with **7 concept drawings** to refine. |

## 2. What follows from the picks

- **Pools:** training points already exist (`FEnemyStrategicTrainingState::TrainingPoints`). Building points need the
  `FEnemyBuildingEconomy` from the base-building plan §3, or at least its `AddBuildingPoints(Amount, Source)` entry point,
  before metal income has anywhere to go.
- **One credit API for both pools:** the refinery calls something like
  `UEnemyStrategicAIComponent::AddTrainingPoints(Amount, EEnemyPointSource::Harvest)` and
  `AddBuildingPoints(Amount, EEnemyPointSource::Harvest)`. The timer keeps using its own path.
- **Free harvesters (H5) do not go through the strategic AI's training budget.** The refinery owns a trainer-like
  component (one harvester at a time, an exit, a rally point to the nearest field), modelled on
  `UEnemyBuildingTrainerComponent` but without spending points. Hand-placed harvesters near a refinery count toward its N.
- **Harvesters stay out of combat pools:** the strategic AI's idle / attack / defend selection must skip harvester
  subtypes, otherwise a free harvester becomes a free tank.
- **Stockpile (H4):** per refinery, one stock per resource with a cap, converted at a fixed rate per tick. Values are a
  balance call for later (see the economy-sim rule: no numbers without discussing them).

## 3. Code gaps found in the survey (2026-11-24)

The harvest loop is player-1-only in four places:
1. `UGameResourceManager::RegisterResourceDropOff` rejects any owner but 1 ("Enemy ResourceDropOffs not supported yet!").
2. `UResourceDropOff::BeginPlay` takes the resource manager of player controller 0.
3. `UHarvester::SetupPlayerResourceManagerReference` does the same.
4. `UHarvester::HarvestAIAction_AsyncGetDropOff` asks for drop-offs with a literal owner `1`.

Also:
- `ABuildingExpansion` does not implement `IResourceStorageOwner`, so a drop-off on a Soviet building needs a small C++
  change (or the refinery is its own actor class).
- The T-34 Breaker (`Rus_Harvester_T34Breaker`, `ETankSubtype::Tank_T34_Harvester`) is not spawnable: no
  `RTS_BP_RTSAsyncSpawner` row, and the BP still carries `Tank_PzI_Harvester`. Not play-tested.
- Unchecked: whether the Breaker's harvester component is set up for metal as well as radixite (H2 needs both).
- Nothing in `Enemy/` orders a harvester to harvest.
- Resource fields have no owner filter, so both sides harvest the same fields (intended).

## 4. Open items (to decide later)

- Default N per refinery, harvester train time, and the refinery's stock caps and conversion rates (balance; run the
  economy sim first).
- Refinery cost/build order inside the base builder, and whether the base builder prefers sites near fields (plan §5.1).
- What the player gets for killing a full refinery (stock lost only, or dropped as salvage).
- The refinery building itself: concept drawings in progress (H6).
