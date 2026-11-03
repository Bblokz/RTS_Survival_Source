# Abilities via `ICommands` (Controller → Queue → Execute)

This document explains how player-issued abilities flow through `ACPPController`, `UPlayerCommandTypeDecoder`, and the `ICommands`/`UCommandData` queue system.

---

## 1) Ability Lifetime Overview (real symbol flow)

Typical right-click flow:

1. `ACPPController::SecondaryClick()` gathers click hit data and calls:
   - `ACPPController::ExecuteCommandsForEachUnitSelected(AActor* ClickedActor, FVector& ClickedLocation)`.
2. `ExecuteCommandsForEachUnitSelected` asks
   - `UPlayerCommandTypeDecoder::DecodeTargetedActor(ClickedActor, TargetUnion)`
   to infer `ECommandType` from the clicked actor.
3. `ACPPController::IssueCommandToSelectedUnits(...)` maps `ECommandType` to an `EAbilityID` and routes to unit-facing `ICommands` APIs (for example: `MoveToLocation`, `AttackActor`, `CaptureActor`, `ThrowGrenade`, etc.).
4. Each selected unit implementing `ICommands` receives that public call. Most wrappers then call
   - `UCommandData::AddAbilityToTCommands(...)`.
5. `UCommandData::AddAbilityToTCommands` pushes an `FQueueCommand` into `M_TCommands` and immediately calls
   - `UCommandData::ExecuteCommand(false)`
   when this is the first queued command.
6. `UCommandData::ExecuteCommand(...)` validates queue/index/cooldown/ability ownership and dispatches by `EAbilityID` into the matching `ICommands::Execute...` virtual (or helper path).
7. The concrete unit class (squad/pawn/selectable actor) performs the actual gameplay logic in its overrides.
8. When execution finishes, code calls
   - `ICommands::DoneExecutingCommand(EAbilityID AbilityFinished)`.
9. `DoneExecutingCommand` calls `TerminateCommand(AbilityFinished)` and then `CommandData->ExecuteCommand(false)` to continue the queue.
10. If queue is exhausted, `UCommandData::ExecuteCommand(false)` clears queue and calls
    - `ICommands::OnUnitIdleAndNoNewCommands()`.

Action-button flow is similar, but starts at `ACPPController::ActivateActionButton(...)` / `ExecuteActionButtonSecondClick(...)` and then issues the same public `ICommands` calls.

---

## 2) When an ability must exist on the command card

Most abilities are validated through:
- `ICommands::GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID)`.

That means the ability must be present in `UCommandData::M_Abilities` (unit command data) and not on cooldown.

### Important exception list
`UCommandData::IsAbilityRequiredOnCommandCard(...)` explicitly returns `false` for:
- `IdCreateBuilding`
- `IdConvertToVehicle`
- `IdAttackGround`
- `IdReturnCargo`
- `IdEnterCargo`
- `IdManAbandonedTeamWeapon`
- `IdCapture`
- `IdTowActor`
- `IdRegisterUnitAsBlackboardIdle`
- `IdMarch`

These are allowed even when no direct command-card entry exists.

### March (`IdMarch`)

- Squads only. Mostly scripted: `ICommands::MarchToLocation` is BlueprintCallable and mission spawn orders accept it.
  A card entry is optional; with one, the second click goes through `ACPPController::ActionButtonPatrol`.
- A patrol with a different gait: `ASquadUnit::ExecuteMarch` reuses the patrol back-and-forth, multiplies the walk
  speed by `MarchSpeedMultiplier` (default 0.6) and loops the anim BP's `MarchMontage`, its play rate following
  the ground speed. `TerminateMarch` restores both.
- Like patrol it never completes by itself and blocks the queue (`IsQueueActiveAndNoPatrol`).
- Patrol and march end when the squad enters combat: `ASquadController::OnSquadUnitInCombat` (a soldier fires or
  is hit) calls `StopCommand`, so the whole squad goes idle and engages.
- Guard squads stop their own guarding on a march order, as on a patrol order.
- The anim BP's `MarchMontageNaturalSpeed` (190 cm/s) is the clip's own stride speed; at the default 0.6 × 500 cm/s
  the montage plays at about 1.6× (quicker steps, no sliding). A `MarchSpeedMultiplier` near 0.38 keeps the
  authored cadence.
- Map test: `-SquadMarchValidate` on TestCover (`Units/Squads/Tests/SquadMarchTestScenario`) logs
  `RTS_MARCH_TEST RESULT PASS|FAIL` after checking march speed, montage and its rate, that a hurt soldier drops
  march and patrol to idle with speed and locomotion restored, and saves pictures to `Saved/SquadMarchTest`.
  `-SquadMarchSquad=<name part>` picks the squad; riflemen are preferred otherwise.

### Attack move

- Not an `EAbilityID` and not on any card. It is an `IdMove` queue entry with `FQueueCommand::bIsAttackMove` set, so
  `GetActiveCommandID()` reads `IdMove` and arrival, move chaining and the final rotation need no cases of their own.
  Use `UCommandData::GetIsActiveCommandAttackMove` to tell it from a plain move.
- Player: arm the attack button and click the ground. `ACPPController::CheckAttackCommandForAttackMove` turns that
  click (`ECommandType::Movement`) into `ECommandType::AttackMove`; clicks on allies keep their repair or move
  behaviour. `IssueAttackMoveToSelectedUnits` reports `IdAttack`, which is where the voice line comes from.
- Script: `ICommands::AttackMoveToLocation`. A unit without `IdAttack` on its card makes a regular move, so mixed
  selections stay together.
- `UCommandData::StartAttackMoveScan` runs a 0.8 s timer that stops itself once neither the attack move nor the
  attack it inserted is running. Each tick asks `URTSTargetAcquisition::RequestAttackMoveTarget`, which ignores
  stance and idle state and searches at weapon range; a unit in combat adds the aggro range to reach whoever is
  shooting it.
- A found target goes through `AttackActorBeforeActiveCommand`, the same insertion as a grenade before an attack.
  The move is terminated and stays in the queue; when the attack is done the move executes again from where the
  fight ended, and the next scan picks up the next enemy. `DropExecutedCommandsBeforeActive` keeps those insertions
  from filling the 16 queue entries.
- While the inserted attack runs, the scan ends it if the target stops being engageable: vehicles otherwise only finish
  an attack when the target dies.
- Only units that override `GetCanStartAttackMoveEngagement` fight: squads (not inside cargo), team weapons (not
  while packing or deploying) and armed tracked vehicles. Aircraft and trains make a regular move.
- Team weapons: the inserted attack stops the gun mid-move, so `bM_IsStoppingMoveForAttackMoveEngagement` keeps
  `HandlePushedMoverFailed` from finishing the move and takes a crew-led gun out of `Moving` so it can deploy.
- The attack does not leash: a unit chases a visible target that runs. A patrol or march queued behind an attack move
  blocks the insertion (`QueueHasPatrol`), so the unit walks without fighting until the queue reaches the patrol.
- Tests: `RTS.Commands.AttackMove` (`Interfaces/Tests/AttackMoveQueueTests.cpp`) covers the queue, including more
  fights than the queue has entries. `-AttackMoveValidate -AttackMoveAt=16796,-15808` on `RTS_ToCopySmall`
  (`Interfaces/Tests/AttackMoveMapTest.cpp`) walks a tank, a squad and seven German guns past two disarmed T-34s and
  logs `RTS_ATTACKMOVE_TEST RESULT PASS|FAIL`. It lives in one file behind `RTS_WITH_MAP_TESTS` with no hook in game
  code. It needs a real RHI for the fog of war; `-RenderOffscreen -ResX=320 -ResY=180` with `r.Streaming.PoolSize 256`
  and the `sg.*` settings at 0 keeps its VRAM use low.
- The map test starts units directly, so the attack button click is not covered, and all seven guns are pushed
  (`PushedWeaponLeads`): a crew-led gun has not run this path yet.
- Team weapons do not enter the in-combat state by firing (`ATeamWeaponController::OnFireWeapon`), only by being hit,
  so test their shots through the magazine in `UWeaponState`.

---

## 3) Ability subtypes: where and why they are used

Subtype-driven abilities store subtype in `FQueueCommand::CustomType` and cast via helpers:
- `GetBehaviourAbilitySubtype()`
- `GetModeAbilitySubtype()`
- `GetFieldConstructionSubtype()`
- `GetGrenadeAbilitySubtype()`
- `GetAimAbilitySubtype()`
- `GetAttachedWeaponAbilitySubtype()`
- `GetTurretSwapAbilitySubtype()`
- `GetCrewRepairAbilitySubtype()`

`IdHeal` keeps its subtype (`EHealAbilityType`) on the card entry only; its queue command carries the target actor.

Subtype-sensitive queue validation is centralized in `UCommandData`:
- `GetDoesQueuedCommandRequireSubtypeEntry(...)`
- `GetAbilityEntryForQueuedCommandSubtype(...)`
- `GetIsQueuedCommandStillAllowed(...)`

So queued subtype abilities are revalidated against the *exact* `(AbilityId, CustomType)` entry, including cooldown.

### CrewRepair (`IdCrewRepair`, `ECrewRepairAbilityType`)

- Action-button only (`ACPPController::DirectActionButtonCrewRepair` → `ICommands::CrewRepair`); no right-click decoder path.
- Lives in the **reserved final card slot** (`VehicleModuleBalance::CrewRepairAbilitySlotIndex`); only the tank writes it
  through `UCommandData::SetCrewRepairAbilityEntry`. Generic `AddAbility` never uses that slot.
- `EnableRepair` follows normal replacement/Shift queue semantics and is revalidated on dispatch (skipped quietly when
  no damaged or destroyed modules remain). `DisableRepair` is an **immediate** control action: it calls
  `ExecuteCrewRepairCommand(DisableRepair)` directly and is never queued behind the running repair.
- The running command completes itself exactly once through `TryDoneExecutingCommand`; `TerminateCrewRepairCommand`
  only cleans up because the queue owns progression there.

### Heal (`IdHeal`, `EHealAbilityType`)

- Squads with medics only (`ASquadUnit_Medic`, or any squad unit with a `UMedicComponent`). The medic squad
  subtypes carry it on card index 5.
- `HealingActive` is a second-click button: `ACPPController::ActivateActionButtonHeal` waits for a click on an allied
  squad unit and `ActionButtonHeal` issues `ICommands::HealTarget`. No right-click decoder path.
- `ASquadController::ExecuteHealCommand` hands the order to the squad's medic components, which heal the injured
  soldiers of the clicked unit's squad, the clicked one first. While it runs the squad swaps its card entry to
  `HealingDisabled`; `TerminateHealCommand` swaps it back.
- `HealingDisabled` is an **immediate** control action (`ICommands::CancelHeal` → `ExecuteCancelHealCommand`), never
  queued. It ends the order and keeps the medics from healing by themselves until the squad's next command.
- The queue only checks that the card holds an `IdHeal` entry of either subtype, so a heal order for another squad
  can replace the running one or be Shift-queued behind it.
- The order completes itself: each medic reports through `ASquadController::OnMedicFinishedOrderedHeal`, and the
  squad calls `DoneExecutingCommand(IdHeal)` once no medic has a patient of that squad left.
- Medics also heal without a command while their squad is idle; `OnCommandExecutionStarting` takes them off that
  before any command starts. `-MedicValidateHeal` runs the map test (`FMedicHealTestScenario`).

### EnterCoverAndHold (`IdEnterCoverAndHold`)

- Squads only, action-button only (`ACPPController::DirectActionButtonEnterCoverAndHold` →
  `ICommands::EnterCoverAndHold`); no subtype, no right-click decoder path. The card must hold the entry.
- `ASquadController::ExecuteEnterCoverAndHoldCommand` asks every soldier for the best free cover nearby
  (`ASquadUnit::TryEnterNearbyCoverOnOrder`) and polls on a timer until nobody is on its way into cover any more,
  or a deadline passes. It then sets `ERTSAggroBehaviour::Stance_HoldPosition` and calls `DoneExecutingCommand`.
- Soldiers that find no cover hold where they stand. An order that is replaced before the squad settled leaves the
  stance as it was.
- A squad on hold position never trades its cover for better cover (`TryRepositionToBetterCombatCover`).

### Temporary ability removal: `BeginAbilitySuppression` / `EndAbilitySuppression`

Hide abilities with `UCommandData::BeginAbilitySuppression(Source, AbilityIds)` instead of removing and re-adding them.
Hidden entries keep their index, `CustomType`, costs and running cooldowns in per-slot backing storage; an entry
reappears only when every suppression source that hides it has ended. Revoking (`RemoveAbility`) a hidden entry drops it
for good. Used by CrewRepair, `UBehVehicleStunned` and `UBehWeaponRemoveAbilities`.

---

## 4) Abilities that do not need per-unit virtual overrides: `ExecuteBehaviourAbility`

`IdApplyBehaviour` is a key example where logic is shared via a component lookup instead of unit-specific `Execute...` override code.

`UCommandData::ExecuteBehaviourAbility(...)` does:

```cpp
const UApplyBehaviourAbilityComponent* Comp = FAbilityHelpers::GetBehaviourAbilityCompOfType(
    BehaviourAbility, M_Owner->GetOwnerActor());
```

Why this pattern is required:
1. Multiple behaviour subtypes can exist on one unit.
2. The subtype (`EBehaviourAbilityType`) must resolve to the matching component instance.
3. The queue system must remain generic and avoid subclass-specific branching for each behaviour subtype.
4. Missing component is safely handled: queued path calls `DoneExecutingCommand(IdApplyBehaviour)` so the queue cannot stall.

When `bByPassQueue` is true (immediate execution path from `ActivateBehaviourAbility` with `bSetUnitToIdle=true`), it executes component logic without queue progression semantics.

---

## 5) How `Execute[Ability]`, `Terminate[Ability]`, and `DoneExecutingCommand` cooperate

The contract is:

1. Queue dispatch calls an `Execute...` function (for example `ExecuteAttackCommand`, `ExecuteAimAbilityCommand`, etc.).
2. Execution logic eventually signals completion via `DoneExecutingCommand(AbilityId)`.
3. `DoneExecutingCommand` calls:
   - `TerminateCommand(AbilityFinished)` (which routes to `Terminate...` and subtype-aware terminate paths where needed), then
   - `ExecuteCommand(false)` to start the next queued command.

`TerminateCommand` handles subtype-based terminate functions by reading the current `FQueueCommand` subtype for abilities like grenade/aim/attached weapon/turret swap/field construction.

If you forget to call `DoneExecutingCommand` for a queued asynchronous ability, the queue will not advance.

---

## 6) Idle / no-ability behavior

- `EAbilityID::IdNoAbility` in queue execution is treated as invalid and skipped (`ExecuteCommand(false)` recursively advances).
- When no command is active, active command resolves to idle semantics (`IdIdle` from `GetCurrentlyActiveCommandType`).
- `ICommands::SetUnitToIdle()` force-terminates current command, clears queue, and calls `SetUnitToIdleSpecificLogic()`.
- `ICommands::OnUnitIdleAndNoNewCommands()` is called when queue finishes naturally; it broadcasts `OnUnitIdleAndNoNewCommandsDelegate` (with a guard for final-rotation completion bookkeeping).

---

## 7) Other important implementation notes

- Queue entries are fixed-size (`MAX_COMMANDS = 16`).
- `SetCommandQueueEnabled(false)` blocks new enqueue operations.
- Patrol-in-queue blocks new additions via `IsQueueActiveAndNoPatrol()`.
- Cooldown starts when command begins execution (`StartCooldownForCommand`), not when queued.
- `ActivateBehaviourAbility(..., bSetUnitToIdle=true)` is intentionally immediate and bypasses queue, but still tries to start cooldown.
- Capture is intentionally allowed without command-card ability gate (`CaptureActor` path).
- Ability components (`UApplyBehaviourAbilityComponent`, `UAimAbilityComponent`, `UAttachedRockets`,
  `UAttachedWeaponAbilityComponent`, `UModeAbilityComponent`, `UTurretSwapComp`) build their command-card entry in
  `MakeAbilityEntry()`. The unit variant screen calls it on Blueprint templates to list a variant's abilities without
  spawning it (`UnitVariantAbilityCards`), so a new ability component needs the same getter and a case in
  `TryMakeAbilityEntry`.
- A variant Blueprint that drops an ability component of its base lists the component's variable name in
  `M_RemovedInheritedComponents` (Class Defaults, Unit Variant), never `DestroyComponent` in BeginPlay: the unit
  destroys them right after BeginPlay and the variant screen leaves their abilities out.

---

## 8) Adding a new ability end-to-end (controller → decoder → queue → execute)

Use this checklist.

1. **Add enum id**
   - Add `EAbilityID` in `Player/Abilities.h`.
   - Add `Global_GetAbilityIDAsString` mapping.

2. **Decide command origin**
   - Right-click inferred command? Add/extend `ECommandType` and decoder paths in `UPlayerCommandTypeDecoder::DecodeTargetedActor` (+ helpers).
   - Action-button only? Wire in `ACPPController::ActivateActionButton` and, if second-click targeting is needed, `ExecuteActionButtonSecondClick` + `ActionButton...` helper.

3. **Route from controller to unit API**
   - Add switch handling in `ACPPController::IssueCommandToSelectedUnits(...)` (or direct action-button helper).
   - Set `OutAbilityActivated` correctly for voice/VFX/UI feedback.

4. **Add public `ICommands` enqueue API**
   - Add `virtual ECommandQueueError <AbilityName>(...)` in `Commands.h`.
   - Implement in `Commands.cpp` to validate command data, ability ownership/cooldown policy, optional `SetUnitToIdle()`, then call `AddAbilityToTCommands(...)`.

5. **Queue dispatch wiring**
   - In `UCommandData::ExecuteCommand(...)`, add `case EAbilityID::...` that calls `M_Owner->Execute...` (or helper like the behaviour/mode component pattern).

6. **Termination wiring**
   - Add terminate routing in `ICommands::TerminateCommand(...)`.
   - For subtype abilities, store subtype in `CustomType` and resolve subtype from current queued command during terminate.

7. **Command-card policy**
   - Decide whether ability must be present in `M_Abilities`.
   - If it is an exception, update `UCommandData::IsAbilityRequiredOnCommandCard(...)`.
   - If subtype-based, ensure `GetDoesQueuedCommandRequireSubtypeEntry(...)` includes it.

8. **Command data / unit setup**
   - Ensure units that should expose it get a `FUnitAbilityEntry` in their command data setup.
   - If subtype-based, ensure `CustomType` matches component/ability subtype enum values.

9. **Concrete execution**
   - Implement/override `Execute...` and `Terminate...` in concrete unit classes as needed.
   - Ensure async completion reliably calls `DoneExecutingCommand(NewAbilityId)`.

10. **UI / targeting polish**
    - If cursor or radius targeting is needed, integrate with controller aim-preview paths (for example `DetermineShowAimAbilityAtCursorProjection`).
    - Add placement effect mapping if command type needs distinct VFX (`DecodeCommandTypeIntoEffect`).

11. **Safety checks**
    - Validate null target behavior.
    - Ensure queue cannot deadlock (missing `DoneExecutingCommand`).
    - Verify shift-queue semantics (`bSetUnitToIdle` false means append).

