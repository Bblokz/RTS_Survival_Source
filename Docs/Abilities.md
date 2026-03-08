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

These are allowed even when no direct command-card entry exists.

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

Subtype-sensitive queue validation is centralized in `UCommandData`:
- `GetDoesQueuedCommandRequireSubtypeEntry(...)`
- `GetAbilityEntryForQueuedCommandSubtype(...)`
- `GetIsQueuedCommandStillAllowed(...)`

So queued subtype abilities are revalidated against the *exact* `(AbilityId, CustomType)` entry, including cooldown.

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

