# RTS_SurvivalShippingEnemyAITests

`RTS_SurvivalShippingEnemyAITests` is a separate **Game** target that validates the **Enemy AI system**
end-to-end on the dedicated map `UT_EnemyAI`, in a build that is as close to Shipping as possible while
still compiling the test-only harness. It is the enemy-AI counterpart to
[`RTS_SurvivalShippingCampaignMapTests`](RTS_SurvivalShippingCampaignMapTests.md) and
[`RTS_SurvivalShippingMapTests`](RTS_SurvivalShippingMapTests.md).

Running against a real Shipping build (not the editor) is deliberate: garbage collection, async task
scheduling and object lifetimes behave like the real game, so GC-safety and threading bugs surface here
that PIE would hide.

## Target and macro

The target is defined in:

```text
Source/RTS_SurvivalShippingEnemyAITests.Target.cs
```

It sets a single global definition and an isolated build environment:

```csharp
GlobalDefinitions.Add("RTS_WITH_ENEMY_AI_SHIPPING_TESTS=1");
BuildEnvironment = TargetBuildEnvironment.Unique;
```

`RTS_WITH_ENEMY_AI_SHIPPING_TESTS` is **only** defined by this target. In every other target
(`RTS_Survival`, `RTS_SurvivalEditor`, the other test targets) it is undefined, so all of the harness and
its in-engine observability hooks are removed by the preprocessor. Do **not** define this macro in
`DeveloperSettings.h`.

### Zero impact on the shipping game

- The harness itself lives entirely in `Source/RTS_Survival/UnitTests/EnemyAIShippingTestRunner.cpp`,
  wrapped top-to-bottom in `#if RTS_WITH_ENEMY_AI_SHIPPING_TESTS ... #endif`.
- The small observability hooks added to production AI classes (e.g.
  `AEnemyController::ShippingTest_GetEnemyWaveController`,
  `UEnemyDirectControlComponent::ShippingTest_GetSuccessfulSquadRegistrationCount`,
  `UEnemyStrategicAIComponent::ShippingTest_EnablePressureOnlyTrainingMode`, the formation/wave progress
  records) are each guarded by the same macro. In any other target they compile to nothing.

The editor target is built as part of the workflow below precisely to confirm the guarded hooks do not
change the normal game.

## Build

Both targets should build cleanly. The editor build is the regression check (macro undefined); the
shipping build is what runs the tests (macro defined, Unique environment).

```powershell
$bat  = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Build\BatchFiles\Build.bat'
$proj = '-Project=D:\UE5Projects\RTS_Survival\RTS_Survival.uproject'
& $bat RTS_SurvivalEditor               Win64 Development $proj -WaitMutex   # regression check
& $bat RTS_SurvivalShippingEnemyAITests Win64 Shipping    $proj -WaitMutex   # test target
```

Because the shipping configuration uses a **Unique** build environment, the first build recompiles engine
modules for that environment; later code-only rebuilds are fast.

## Package / stage

A one-time full package (via the editor's `BuildCookRun` / `Package Project`) produces a staged Windows
build. Because the harness is **code only** (no cooked content changes), later iterations only need the
freshly built child executable copied over the staged one:

```text
<Package>/Windows/RTS_SurvivalShippingEnemyAITests.exe                                   (launcher)
<Package>/Windows/RTS_Survival/Binaries/Win64/RTS_SurvivalShippingEnemyAITests-Win64-Shipping.exe  (game)
```

This repo's working package lives under `E:\RR_test\EnemyAIShippingTests\Windows`.

## Run

The harness **navigates to `UT_EnemyAI` itself** — you do not pass a map URL. It boots to the front-end,
primes a valid faction/commander/difficulty, force-opens
`/Game/RTS_Survival/Maps/UnitTests/UT_EnemyAI`, and — importantly — **drives the game's start-game
un-pause** (`ACPPController::PauseGame(ForceUnpause)`) so the game-thread timers actually run. The RTS
boots paused behind a "click to start" widget; without this the strategic AI timers never tick and the
blackboard never populates.

Run with a **real RHI** (the front-end menu is UMG-heavy and crashes under `-nullrhi`):

```powershell
E:\RR_test\EnemyAIShippingTests\Windows\RTS_SurvivalShippingEnemyAITests.exe `
  -RTSRunEnemyAITests -RTSTestExitOnComplete -unattended -windowed -ResX=640 -ResY=360 -log
```

Command-line options:

```text
-RTSRunEnemyAITests           Trigger the harness (also -RTSRunMapTest=EnemyAI or =All).
-RTSTestExitOnComplete        Exit when the sweep finishes (exit code 0 = all passed, 1 = failures).
-RTSEnemyAITestGCCycles=N     Forced GC cycles in the GC-stress phase (default 3).
-ABSLOG=<path>                Write the game log to an explicit path.
```

> The staged **launcher** exe often returns 0 regardless of the child's result. Treat the **summary file**
> and the `RTS_ENEMY_AI_TEST_RESULT` log line as ground truth, not the process exit code.

You can also run it from the console once already on `UT_EnemyAI`: `RTS.UnitTests.EnemyAI.Run`.

## What it validates (staged, tick-driven phases)

1. **Startup** — enemy squads/tanks registered and idle in the blackboard; the game is started/unpaused;
   the decision tree layout matches the authored tree (top-level actions, sub-action membership, native
   requirement classes and amounts); the two global abilities are configured and loaded.
2. **Ability cooldowns** — Carpet (10s) and T34 (8s) cooldowns tick down ~1/sec off a dedicated timer.
3. **Strategic blackboard** — road splines, player HQ / resource buildings / unit bulks / combat counts,
   enemy base clusters, and generated construction / mine / base-defense locations.
4. **Training buckets** — every registered sub-action routes its focus/specialty training pressure to the
   correct bucket, contributions match the authored decision tree, and the per-minute point income is
   elapsed-time based.
5. **Missions** — the repeating attack-move wave and the single random-patrol-with-attack-move wave spawn,
   complete, and hand off to real formations; no failed spawns.
6. **GC stress** — forces GC several times and re-verifies the controller, components, abilities and
   active formations all survive (GC safety).
7. **Stochastic execution** — the decision tree actually executes at least one sub-action, and no
   training units are spawned (training buckets are wired but no training components exist yet — expected).

### Output

```text
%LOCALAPPDATA%\RTS_Survival\Saved\EnemyAIShippingTests\Run_<YYYYMMDD_HHMMSS>.txt
```

with `RESULT PASS|FAIL`, pass/fail counts, and one `PASS`/`FAIL` line per check. The live log carries the
same information as `RTS_ENEMY_AI_TEST_PASS` / `RTS_ENEMY_AI_TEST_FAIL` / `RTS_ENEMY_AI_TEST_RESULT` lines,
plus `RTS_ENEMY_AI_DIAG` world-state lines (road-spline count, applied difficulty, player-HQ validity,
mission-manager difficulty, and a registered-actor histogram) that make map-setup problems obvious.

## `UT_EnemyAI` map / config requirements

The AI code is validated; a handful of checks depend on the **map and enemy-controller data** being set
up. For the full suite to pass, `UT_EnemyAI` must provide:

- **A player HQ nomadic** of subtype `Nomadic_GerHq` (or `Nomadic_RusHq`). The strategic AI identifies the
  HQ purely by that nomadic subtype (`FStrategicAIHelpers::GetIsNomadicHQ`); without it `PlayerHQLocation`
  stays zero and `AttackMoveToPlayerHQ` has no target. (Resource-building nomadics are detected
  separately and already work.)
- **`FindEnemyBase_TimerRequest.CoreBuildingTypes`** configured on the enemy AI controller to the
  `EBuildingExpansionType` values that anchor a base. `BuildEnemyBaseClustersResult` returns no clusters
  when this list is empty, and the base-defense / construction / mine / defense-arc locations all derive
  from those clusters.
- **The mission's random-patrol wave** must be given a non-zero spawn location and at least two patrol
  points. The difficulty gate is satisfied (the harness runs at Ironman), so a missing patrol wave means
  its spawn/points are unset (a zero spawn location makes `GetIsValidWave` reject it).

## Bugs found and fixed by this harness

- **T34 global ability never loaded.** `UGlobalAbility::M_AbilityType` was taken only from the data-asset
  template; a mis-authored template left `FindLoadedAbilityByType(GA_SovietT34Drop)` returning null, so the
  enemy's T34 cooldown override never applied. `LoadGlobalAbilities` now stamps `M_AbilityType` from the
  enum key the ability is loaded under.
- **Enemy sub-components held the class-default object as their controller (systemic).**
  `AEnemyController` initialised its formation, wave, navigation, retreat and field-construction components
  with `Init...(this)` from the **constructor**; for a placed controller that owner reference deserialises
  back to the class default object, so at runtime those components pointed at the CDO rather than the live
  controller (only the strategic component, re-inited in `BeginPlay`, was correct). Concretely this meant:
    - **Every enemy wave formation** (attack-move and random-patrol) was created on the CDO's formation
      controller instead of the live one, so wave units never formed up/advanced/patrolled in the world.
    - **Road splines** were propagated to the CDO's strategic blackboard, so road-based AI logic (e.g.
      road-mine placement) silently never received them.

  Fixed properly (matching the direct-control component, which already did this): each affected component
  re-binds `M_EnemyController = Cast<AEnemyController>(GetOwner())` at the start of its own `BeginPlay`
  (runtime, after deserialization, before any use), and every `EnsureEnemyControllerIsValid()` now guards
  against a stale reference — the non-const ones (wave/formation/field-construction) re-bind to the owner
  each call, and the const ones (navigation/retreat/strategic) require `M_EnemyController == GetOwner()`.
  The decision tree, strategic and direct-control components were already runtime-bound and unaffected.
- Plus a batch of GC-safety / lifetime fixes in the wave, formation, navigation, blackboard-query,
  strategic and global-ability systems (dangling `FAttackWave*` across async spawn callbacks, off-game-thread
  Recast access, stale weak-pointer dereferences, per-unit flank order-queue reset, elapsed-time training
  income, a dedicated global-ability cooldown timer, and a missing early-return in
  `AddTrainingComponentToAIBlackboard`).
