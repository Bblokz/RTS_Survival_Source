# CrewRepair refactor into TankCrewRepair

Implementation plan, reviewed against the current working tree on 1 October 2026. This document does not implement the refactor. Existing local C++ changes are part of the reviewed baseline and must be preserved.

Read [VehicleModuleSystemDesign.md](VehicleModuleSystemDesign.md), especially sections 2, 7, 8, 12 and 13, and [Abilities.md](Abilities.md) before implementation. The vehicle-module document describes the original design; the source already implements CrewRepair and is the authority for current symbols. This is an extraction of an existing ability, not registration of a new ability.

## 1. Goal and boundaries

Create `UTankCrewRepair`, a native `UActorComponent`, in:

- `RTS_Survival/RTSComponents/AbilityComponents/TankCrewRepair/TankCrewRepair.h`
- `RTS_Survival/RTSComponents/AbilityComponents/TankCrewRepair/TankCrewRepair.cpp`

Use the inherited default-subobject name `TankCrewRepair`. Every `ATankMaster` owns exactly one component. The component owns all crew-specific session, timer, command-card and restriction-acquisition/release logic. `ATankMaster` retains small forwarding methods for its existing command interface and Blueprint queries.

Keep the following responsibilities in their current owners:

| Owner | Responsibilities that remain |
| --- | --- |
| `UArmorCalculation` | Module storage, stable module IDs, snapshots, priority selection, HP/state mutation, counts and native batch dispatch |
| `ATankMaster` | Module finalization/turret registration, ordinary healing gate and finishing work, module behaviours, module icons/announcements/BP events, shared mounted-weapon locks, shared mobility restrictions and actual locomotion adapters |
| `UCommandData` | Queue execution/serials, reserved slot storage, ability suppression backing entries, cooldowns and action UI |
| `ICommands` / controller | Public activation, exact-subtype validation, replacement/Shift queue semantics, immediate Disable and generic termination |
| `UTankCrewRepair` | Crew runtime state, current target/time, timer, card state, acquired suppression handle and crew-owned weapon-lock source |

Do not move `FVehicleModuleRepairState`, `M_ModuleRepairState`, `M_ModuleArmor`, `bM_AreVehicleModulesInitialized`, `M_MountedWeaponLockSources`, `M_MobilityRestrictions`, deferred Blueprint batches, or module-binding delegate handles into this component. Do not duplicate the tank's cached counts or ordinary healing progress. The component reads authoritative red counts from armor.

## 2. Current behavior that must survive

### Activation and card

The existing path is:

`ACPPController::ActivateActionButton` -> `DirectActionButtonCrewRepair` -> `ICommands::CrewRepair` -> queued Enable/direct Disable -> `ATankMaster::ExecuteCrewRepairCommand`.

- `EAbilityID::IdCrewRepair` and `ECrewRepairAbilityType::{EnableRepair, DisableRepair}` retain their values, names and `CustomType` encoding.
- The action is free, with `CrewRepairCooldownSeconds == 0`. Card insertion alone never starts repair.
- The reserved slot is `VehicleModuleBalance::CrewRepairAbilitySlotIndex`, currently the last of 15 slots (index 14). Use the constant, never the literal index.
- Living tank with red modules: Enable while inactive, Disable while repairing. No red modules: empty entry. Yellow modules alone do not offer CrewRepair.
- `ReserveCrewRepairAbilitySlot` sizes the card once and rejects an occupied final slot with an error. Do not relocate loadout abilities to make room.
- `SetCrewRepairAbilityEntry` writes only an empty/CrewRepair entry and reports whether ID/subtype actually changed. Generic additions skip the slot; runtime `SetAbilities` preserves its reservation and entry.
- Enable follows existing replacement/Shift semantics and is revalidated on dispatch. Duplicate queued/running Enable is rejected. Disable executes immediately, including with Shift; it never clears the rest of the queue first or queues behind Enable.
- Keep exact-subtype card validation. CrewRepair is not a command-card exception and has no right-click decoder or targeting step.

### Work, timing and restrictions

- Repair only `Destroyed` -> `Damaged`, using `UArmorCalculation::RestoreDestroyedModuleToDamaged(ModuleId)`. That function applies `MaxHp * GetRecoveredHealth01(Type)` with cause `EModuleChangeCause::CrewRepair` and dispatches the normal module batch.
- Never call `Heal`, write hull health, check the ordinary 75% hull gate, or grant/reset ordinary full-service work as part of crew repair.
- Keep `VehicleModuleBalance.h` as the tuning authority. Priority is Engine, Tracks, Wheels, Weapon, Turret, Ammo, AddOnArmor, then fixed slot order within a type.
- Current durations are Engine 20s, Tracks 12s per side, Wheels 10s per side, Weapon 12s, Turret 15s per turret, Ammo 18s and AddOnArmor 8s per zone. Engine plus two tracks takes 44s without interruption.
- One UObject-bound repeating timer per active component: interval and first delay are both `CrewRepairTickSeconds` (currently 1s). Disable `PrimaryComponentTick`; there is no per-module timer, heap repair queue or idle polling.
- Use elapsed world game time, preserving pause/time-dilation behavior. Retain the existing `ElapsedToleranceSeconds = 0.01` jitter allowance as a named implementation constant in the new `.cpp`; it is numerical tolerance, not a new repair balance setting.
- Keep the current target while it stays red. Newly destroyed modules do not reset its progress or preempt it. Further damage to the same red target creates no warmup/quiet period.
- If the target is removed/recovered elsewhere, discard its unfinished time and select a fresh target with its full duration. Disable/re-enable also loses unfinished time; previously completed yellow repairs persist. A module destroyed again needs its full duration again.
- Acquire the shared mounted-weapon lock and physically stop movement/rotation. Suppress exactly Attack, Move, ReverseMove and RotateTowards through `BeginAbilitySuppression` with `CrewRepairSuppressedAbilities`.
- Suppression must preserve full backing entries, slot indices, subtypes, costs and live cooldowns. Grants/revocations and overlapping stun/module suppression remain owned by `UCommandData`; never cache/replay ability arrays in the component.
- Releasing repair restores only its restrictions. Other lock sources and behaviour-composed weapon/mobility restrictions remain effective. Newly installed turrets/hull weapons inherit the tank's active shared weapon lock.

## 3. Component shape, state and ownership

Declare `UCLASS(ClassGroup=(Custom)) class RTS_SURVIVAL_API UTankCrewRepair : public UActorComponent`. It is created by C++, so do not require `BlueprintSpawnableComponent` or designer setup. Add class Doxygen explaining that the tank forwards commands and finalized-module notifications to it. Preserve the required Blueprint finalization note on `ATankMaster`; do not invent a new required Blueprint call.

### Owned references

| Member | Representation / initialization |
| --- | --- |
| Tank `M_TankCrewRepair` | `UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true")) TObjectPtr<UTankCrewRepair>`; `CreateDefaultSubobject` in `ATankMaster` constructor |
| Component `M_TankOwner` | `UPROPERTY(Transient) TWeakObjectPtr<ATankMaster>`; supplied after tank `Super::PostInitializeComponents` |
| Component `M_CommandData` | `UPROPERTY(Transient) TWeakObjectPtr<UCommandData>`; tank-owned external object supplied at the same boundary |
| Component `M_ModuleArmor` | `UPROPERTY(Transient) TWeakObjectPtr<UArmorCalculation>`; supplied only after successful vehicle-module finalization |
| Component `M_CrewRepairState` | `UPROPERTY(Transient) FTankCrewRepairState`; contains the migrated runtime session |

The component must not retain the owner, command data or armor with strong pointers: all belong to the tank. Do not cache raw `ICommands*`, controller pointers, module-array element pointers or command-card element pointers across calls. Call the tank's public command APIs using its validated weak reference.

Move `ECrewRepairStatus` and `ECrewRepairStopReason` from `TankMaster.h` into `TankCrewRepair.h`. They are component implementation types; the reflected command enum stays in its existing `CrewRepairAbilityTypes.h` so controller/queue includes remain stable.

Move `FCrewRepairState` into the component header and rename it `FTankCrewRepairState` to make its ownership clear. Preserve these fields and initial values:

| Field | Initial value / contract |
| --- | --- |
| `Status` | `Inactive`; retain `Inactive`, `Repairing`, `Stopping` and the re-entrant cleanup guard |
| `CurrentModuleId` | `INDEX_NONE`; stable ID, not an array address |
| `ModuleWorkStartGameTime` / `RequiredModuleSeconds` | `0.0` / `0.f`; elapsed-time timing of the current target |
| `TimerHandle` | Unset; the component's only repair timer |
| `SessionGeneration` | `0`; increment at each start/stop, never reset to zero on ordinary cleanup |
| `ActiveCommandToken` | `0`; exact queued Enable execution serial, zero for a session without queue ownership |
| `AbilitySuppressionHandle` | Default; retain its `UPROPERTY()` and existing handle type |
| `bHoldsWeaponAndMovementLock` | `false`; tracks acquisition of crew restrictions |

Add a component-specific pending-module-reconciliation boolean for the safe-boundary handoff described below, and a shutdown boolean to prevent a component ending play from starting another session. Keep related session fields together. This refactor does not replace existing booleans with new enums.

`FAbilitySuppressionHandle` currently lives in `Interfaces/Commands.h`. Include that header in the component header because the state contains the handle by value. Forward-declare `ATankMaster` and `UArmorCalculation`, include their definitions in the `.cpp`, and keep `TankCrewRepair.generated.h` last. Forward-declare `UTankCrewRepair` in `TankMaster.h`; include its header only in actor implementation files/tests that call it. This avoids a component/tank include cycle. Existing Engine/Core dependencies suffice; no new `Build.cs` dependency is expected.

### Proposed public C++ API

These are the only integration entry points; internal timer/card/session helpers remain private:

| Method | Contract |
| --- | --- |
| `void InitializeForTank(ATankMaster* TankOwner, UCommandData* CommandData)` | Cache validated owner/data once; no card reservation, timer, module search or repair start. Allow identical repeated setup; reject conflicting owner/data. |
| `void OnVehicleModulesInitialized(UArmorCalculation* ModuleArmor)` | Cache finalized armor, reserve the slot and synchronize the initial card. Idempotent for the same armor; reject rebinding a live session. |
| `void ExecuteCrewRepairCommand(ECrewRepairAbilityType Subtype)` | Own Enable/Disable dispatch, failed-start rollback and exact queued-command completion. |
| `void TerminateCrewRepairCommand(ECrewRepairAbilityType Subtype)` | Stop with `QueueTermination`; never advance the queue. |
| `bool GetIsCrewRepairActive() const` | True only for `Repairing`, preserving the existing predicate. |
| `float GetRemainingCrewRepairSeconds() const` | Sum full durations of installed reds, subtract clamped current-target elapsed time when active, clamp final result to zero. |
| `int32 GetCrewRepairTargetModuleId() const` | Current ID, `INDEX_NONE` after cleanup. |
| `bool GetShouldIgnoreCommandCompletion(EAbilityID AbilityFinished) const` | Ignore non-CrewRepair completions while repairing and the active queued command is CrewRepair. |
| `void DeferModuleStateReconciliation()` | Mark a batch pending before committing behaviour changes; no callbacks or state mutation. |
| `void OnModuleStateBatchCommitted()` | At a safe behaviour boundary, reconcile target/completion and card from current armor state; also usable after import. Clear pending intent before callbacks. |
| `void OnModuleBehavioursApplied()` | Drain pending reconciliation only; repeated notifications without pending work do nothing. |
| `void Shutdown()` | Idempotent terminal cleanup, no weapon restoration or command progression; block subsequent starts. |

Keep `FinishCrewRepair(ECrewRepairStopReason)` private. Loading uses the public terminate path, not a public arbitrary-stop-reason API.

## 4. Exact migration inventory

The current private CrewRepair block in `TankMaster.h` contains twelve helpers. Move their implementations out of `TankMasterCrewRepair.cpp` according to this table:

| Existing helper | Destination |
| --- | --- |
| `InitializeCrewRepairAbilitySlot` | Component private helper called from `OnVehicleModulesInitialized` |
| `RefreshCrewRepairAbilityFromModuleState` | Component private helper |
| `MakeCrewRepairAbilityEntry` | Component private const helper |
| `BeginCrewRepair` | Component private helper |
| `AcquireCrewRepairRestrictions` | Component private helper; use the component as source |
| `StopVehicleForCrewRepair` | Retain on the tank as the narrow public physical-stop adapter described below |
| `StartCrewRepairTimer` | Component private helper; bind `UTankCrewRepair::CrewRepairTick` |
| `CrewRepairTick(uint32 SessionGeneration)` | Component private helper |
| `SelectNextRedModuleForCrewRepair` | Component private helper; call armor's existing selector |
| `UpdateCrewRepairAfterModuleBatch` | Component private helper invoked by its safe batch reconciliation |
| `FinishCrewRepair` | Component private helper |
| `RestoreCrewRepairState` | Component private helper |

Move the implementations of remaining-time calculation and completion filtering into the component as well. Keep the actor's five public compatibility methods (`Execute`, `Terminate`, `GetIsCrewRepairActive`, `GetRemainingCrewRepairSeconds`, `GetCrewRepairTargetModuleId`) and protected `GetShouldIgnoreCommandCompletion` override as validated forwarding wrappers. Preserve all existing `UFUNCTION` names, signatures and categories on the actor; the formerly inline target getter becomes an out-of-line forwarder.

After migration, remove the private CrewRepair helper block, enums, session struct and `M_CrewRepairState` from `ATankMaster`. The actor needs only its component member, `GetIsValidTankCrewRepair() const`, lifecycle forwarding and the movement adapter. Delete `TankMasterCrewRepair.cpp` after relocating the small compatibility wrappers to `TankMaster.cpp`; do not leave a second implementation/state behind.

### Tank physical-stop adapter

Keep `ATankMaster::StopVehicleForCrewRepair()` as a short C++ method the component can call. This is an owner capability, not session ownership. It must retain this sequence:

1. `EndTurretRangeMovement(ETurretRangeMovementEndMode::FullStop)`.
2. `StopBehaviourTree()`.
3. Validate/repair the AI controller through `GetIsValidAIController`, then `AITankController->StopMovement()`.
4. `StopRotating()`.
5. `ResetRotateTowardsFinalMovementRotation()`.
6. `StopMovement()` through the existing tank movement implementation.

Do not replace it with `ExecuteStopCommand`, which only updates auto-engage. Do not give the component friendship/access to private pursuit or movement state. Retain the tracked adapter's `GetIsCrewRepairActive()` brake guard in `ATrackedTankMaster::UpdateVehicle_Implementation`; it now receives the same answer through the actor wrapper. Verify wheeled/vehicle path-following braking through the existing `StopMovement` path too.

### Restriction source identity

Change both `AcquireMountedWeaponLock(this)` and `BeginAbilitySuppression(this, ...)` to use `this` in the **component**, i.e. `UTankCrewRepair` is the source. Release with that same component and its exact suppression handle. Never release using the tank pointer or clear all sources. Shared actor lock APIs and automatic lock inheritance remain in place. The component does not need a new mobility scalar restriction: movement is stopped by the adapter and existing command/drive guards.

## 5. Every actor call site and lifecycle handoff

| Location | Required change and ordering |
| --- | --- |
| `ATankMaster` constructor | Create `M_TankCrewRepair` with stable name `TankCrewRepair`; do not touch command/module state in constructors. |
| `ATankMaster::PostInitializeComponents` | After `Super` creates/initializes `UnitCommandData`, call a short `PostInitializeComponents_InitTankCrewRepair` helper to initialize owner/data. Components may initialize before this actor boundary; do not rely on component `BeginPlay` order. |
| `ATankMaster::BeginPlay_SetupData` | Keep both existing `InitAbilityArray` paths. Reserve only after abilities exist and modules finalized, matching current behavior; do not add an earlier reservation that can drop an occupied loadout entry. |
| `FinalizeVehicleModulesAfterTurretRegistration` | Replace `InitializeCrewRepairAbilitySlot` with `OnVehicleModulesInitialized(M_ModuleArmor.Get())` only after `FinalizeVehicleModuleSetup` succeeds, the initialized flag is set, behaviours/health ownership are established, counts refreshed and initial icons synchronized. |
| `OnModuleStateBatchCommitted` | Keep tank count mirroring first, then dead-unit guard, behaviour synchronization, crew handoff, icons/announcements and BP event pipeline. Apply the safe-boundary details in section 7. |
| `OnModuleBehavioursApplied` | Forward pending crew reconciliation before the existing early return for an empty deferred BP batch; pending crew cleanup must not depend on there being a Blueprint event. Preserve copied/reset deferred event dispatch and dead-unit guards. |
| `ImportVehicleModuleSaveData` | Preserve all validation/deferred-load decisions. Once import is actually attempted, call `SetUnitToIdle` if CrewRepair owns the active command, then the component's terminate path before `ImportModuleState`. After import/counts/icon snapshot, reconcile the inactive card. Failed import still leaves crew work stopped, as today. |
| `ApplyDeferredVehicleModuleSaveData` / initial turret registration | Keep existing flow through `ImportVehicleModuleSaveData`; initialize the component before a deferred import runs. No new independent import or timer resumption. |
| `UnitDies` | Replace crew Finish with component `Shutdown` at the existing point before death callbacks. Ensure terminal release never restores weapons even though `SetUnitDying` occurs later. |
| Tank `EndPlay` | Call component `Shutdown` before module binding cleanup/shared-source reset/`Super::EndPlay`. |
| Component `EndPlay` | Call `Shutdown` before `Super::EndPlay` as a fallback; repeated actor/component cleanup is harmless. |
| Tracked movement update | Preserve the existing actor query and braking path; no direct dependency on component internals. |

Tanks without finalized modules have an inert component: no slot reservation, missing-armor spam or timer. Queries return `false`, `0.f` and `INDEX_NONE` before readiness. Distinguish this expected state from a required weak reference expiring after setup; the latter uses its validator and safe cleanup. Readiness may be represented by explicit setup flags, not by bypassing member validators.

If the required component is unexpectedly missing, the execute wrapper must safely complete the exact active queued Enable through `TryDoneExecutingCommand`, without a queue stall. Terminate remains a safe no-op; queries return their neutral values. Capture the execution serial before any operation that can re-enter the queue.

## 6. Session and command-completion contract

### Start

1. Reject shutdown, non-Inactive state, dead owner, missing owner/data, uninitialized armor, unreserved slot or zero red count.
2. Select a red module with the existing armor selector; record duration and game-time start. Validate world before acquiring restrictions.
3. Increment session generation, set `Repairing`, and capture the current execution serial only if the active command is `IdCrewRepair`.
4. Acquire the component-source mounted-weapon lock, stop the vehicle and acquire ability suppression. Preserve the existing order so stale movement completion is ignored during the stop. Revalidate generation/status, shutdown and owner liveness after operations that can invoke callbacks; if termination/death intervened, do not continue startup or install a timer for the cancelled session.
5. Start the UObject-bound timer with the generation payload. A failure rolls back all acquired state.
6. Synchronize Disable and the action UI after suppression/card state is ready. UI must refresh when suppression changes even if the CrewRepair entry did not change; avoid duplicate updates within a single start/stop operation.

Use public tank command APIs for queue progression; the component does not implement `ICommands` or become `UCommandData::M_Owner`. Capture no raw owner in timer lambdas. `FTimerDelegate::CreateUObject(this, &UTankCrewRepair::CrewRepairTick, SessionGeneration)` is sufficient.

### Tick

Check generation/status first. Validate the living owner, armor and world. No reds finishes normally. If the target snapshot is no longer Destroyed, select a new target with fresh start time and return. Compare elapsed game time with the required duration plus existing tolerance. Once due, call `RestoreDestroyedModuleToDamaged(CurrentModuleId)` and return immediately: native batch reconciliation owns retargeting/automatic completion.

The armor call may synchronously stop the session, advance the queue, start another command or destroy the tank. Do not access saved module/card references or assume the old generation remains current after it. Snapshot APIs return values; keep them as values. If selection/mutation fails because of invalid dependencies, clean up safely; do not spin forever or report all modules recovered when reds still exist.

### Stop reasons

| Reason | Restore crew suppression/lock ownership | Update living card | Queue completion |
| --- | --- | --- | --- |
| `PlayerDisabled` | Yes, normal restoration through current composed eligibility | Enable if reds remain, otherwise empty | Try exact captured token once |
| `AllModulesRecovered` | Yes, at the safe behaviour boundary | Empty | Try exact captured token once |
| `QueueTermination` | Yes | Enable if reds remain, otherwise empty | Never; caller owns progression |
| `StartFailed` | Roll back any acquired restrictions | Reconcile current state | Exactly one owner completes the captured failed Enable token |
| `OwnerDestroyed` | Remove owned suppression/lock; never re-enable weapons | Clear reserved entry if data survives; skip UI callbacks | Never |

Cleanup sets `Stopping` first, increments generation and clears the timer before releasing anything. Capture/clear the active token before re-entrant callbacks can use it. Release only the stored suppression handle and component-source weapon lock. Reset target/work/lock-held fields and set `Inactive`. Complete only after cleanup/card restoration through `TankOwner->TryDoneExecutingCommand(IdCrewRepair, CapturedToken)`.

Shutdown takes precedence over a normal stop already in progress. If a release/UI callback ends play or destroys the owner, the outer cleanup must recheck shutdown/liveness and skip later UI, weapon restoration and queue progression. Revalidate weak references between such callbacks; a previously validated local raw pointer is not a guarantee that the object survived a re-entrant call.

Do not reset the entire session struct during ordinary stop: losing the monotonic generation can make old timer payloads match a later session. Repeated stops in Inactive/Stopping must not advance commands or release another source's handles. Shutdown also clears a residual timer/card/pending reconciliation when already inactive and permanently blocks starts.

**Failed-start correction:** Current `FinishCrewRepair(StartFailed)` can try token completion and `ExecuteCrewRepairCommand` then calls unguarded `DoneExecutingCommand` after `BeginCrewRepair` returns false. A re-entrant next CrewRepair command can therefore be completed accidentally. In the component, choose one owner: failed-start cleanup never progresses the queue, and Execute completes only its originally captured token with `TryDoneExecutingCommand`. Failures before session acquisition use the same captured-token path. Do not test only the current ability ID after rollback.

The actor's completion filter must retain the existing predicate: active repair + active queued CrewRepair + completion of another ID -> ignore. Returning true for all completions would block the repair's own completion. A stale same-ID completion needs the execution serial check, not only this filter.

## 7. Module batches, behaviour boundaries and terminal safety

The component must not subscribe directly to a separate armor delegate or independently publish module icons/BP events. `UArmorCalculation::DispatchModuleChangesAfterMutation` already routes committed batches to the tank after the mutation guard ends and stages re-entrant changes. Preserve that single route.

Once it is safe to reconcile:

1. Read the current armor red count; do not cache a second copy of `DestroyedModuleCount`.
2. While repairing, zero reds stops automatically, even if remaining modules are Healthy rather than Damaged.
3. If the current target remains red, keep its clock; otherwise choose the next red target with full time.
4. Refresh the card from current live state. Later new red damage offers Enable, never auto-starts.

**Existing ordering gap:** Tank batch handling currently calls `UpdateCrewRepairAfterModuleBatch` even when `CommitModuleBehaviourChanges()` returned false. Deferred behaviour application may still be iterating. Moving that call unchanged does not meet the vehicle-module design's requirement to restore after effects commit.

Use the existing tank `GetOnModuleBehavioursApplied` subscription, without adding a component subscription:

- After count refresh/dead guard and queuing desired module behaviour changes, call component `DeferModuleStateReconciliation()` **before** calling `CommitModuleBehaviourChanges`. That commit can broadcast synchronously.
- If commit returns true, call component `OnModuleStateBatchCommitted()`. A synchronous notification may already have drained the pending request; make the reconciliation idempotent.
- If commit returns false, leave reconciliation pending. `ATankMaster::OnModuleBehavioursApplied` forwards to the component before publishing the existing deferred BP events.
- Verify that behaviour work really finished before draining: `ApplyPendingModuleBehaviourSlots` currently broadcasts after a bounded drain, even if pending slots remain. Add a narrow const `UBehaviourComp::GetHasPendingModuleBehaviourChanges()` query (cover pending slots/refresh/application) and use it in the actor's forwarding guard. Do not equate every broadcast with a successful full commit or recursively call Commit from its own notification.
- Clear the pending flag before reconciliation can complete a command. Multiple batches coalesce; read current snapshots/counts instead of storing an old batch in the component. Recheck owner liveness after re-entrant callbacks.
- Retain immediate tank count mirroring and existing icon pipeline; cleanup waits for the existing safe processing boundary, not a later crew timer/healing pulse. A missing required behaviour component is a logged dependency failure, not an endless wait for a notification that cannot arrive.

Test both immediate and deferred commit paths, including a heal issued during behaviour iteration and re-entrant module changes during apply. Manual cancellation keeps the generic source-owned release APIs; it must not overwrite current effects. Terminal shutdown cancels pending reconciliation immediately.

**Terminal restoration gap:** Existing `FinishCrewRepair(OwnerDestroyed)` calls `RestoreCrewRepairState`, which invokes ordinary `ReleaseMountedWeaponLock`. During `UnitDies`, `SetUnitDying` has not happened yet; during EndPlay an actor may still report alive. The existing dead-unit guard alone therefore cannot guarantee no weapon restart.

Add `ATankMaster::ReleaseMountedWeaponLockWithoutRestoringWeapons(UObject* Source)` as a narrow C++ capability. Share source removal/pruning with normal release in a short helper; only normal release runs auto-engage. Shutdown uses the non-restoring path, removes its suppression if command data survives, and explicitly clears the reserved card entry without calling `UpdateActionUI` or queue APIs. Keep normal `ReleaseMountedWeaponLock` behavior unchanged for other callers. Do not reorder the complete death handler merely to make a moved crew helper safe.

If a required weak reference expires, clear the component timer/local state first, release any remaining independently valid handles/owner restrictions where possible, and leave no callback that can access an expired object. Invalid pointers log rather than crash; teardown must not depend on all three references still being valid.

## 8. Compatibility and files to inspect/update

### Implementation changes

- New `TankCrewRepair.h/.cpp`: component, component-local enums/state, validators, timer, card helpers and session operations.
- `Units/Tanks/TankMaster.h`: forward declaration/owned member/validator, movement and terminal-release capabilities, remove migrated state/helpers; preserve public Blueprint/interface signatures.
- `Units/Tanks/TankMaster.cpp`: constructor/post-initialization, forwarding methods, death/EndPlay handoffs and physical-stop adapter.
- `Units/Tanks/TankMasterCrewRepair.cpp`: remove after migration; no duplicate definitions.
- `Units/Tanks/TankMasterModules.cpp`: finalized-armor initialization, safe batch/behaviour forwarding, load termination/reconciliation and non-restoring shared lock release helper.
- `Behaviours/BehaviourComp.h` and module implementation if needed: the small const pending-work query used to guard safe-boundary forwarding.
- `VehicleModules/Tests/VehicleModuleTests.cpp`: fixture/component setup and private test access, plus behavioral coverage below.

### Preserve/audit, rather than rebuild

- `Interfaces/CommandsCrewRepair.cpp`, `Commands.h`, `Commands.cpp`: reservation, suppression sources/generations, hidden cooldowns, exact card subtype, execution serials, immediate Disable, generic execute/terminate dispatch and `SetAbilities` preservation remain intact.
- `Player/CPPController.h/.cpp`, `Player/Abilities.h`, voice helpers and existing ability UI data: no new ID, enum renumbering, controller branch, decoder or UI authoring.
- `TrackedTank/TrackedTankMaster.cpp`, wheeled/vehicle movement adapters: retain movement stop and late-drive guards through actor query wrappers.
- `TankMaster.h` turret installation, `TankMaster.cpp::SetupHullWeapon` and turret swaps: retain inheritance of shared lock sources and current behaviour eligibility.
- `TankMasterTurretModules.cpp`: retain initial discovery/finalization/deferred import ordering and module identities.
- `ArmorCalculationModules.cpp`, `VehicleModuleBalance.h`, health component and module behaviours: retain repair mutation/priority/tuning/healing/presentation semantics.

Update [Abilities.md](Abilities.md) to say the tank forwards CrewRepair and `UTankCrewRepair` writes its reserved slot through CommandData. Update the relevant owner/responsibility, function/state and lifecycle descriptions in [VehicleModuleSystemDesign.md](VehicleModuleSystemDesign.md), distinguishing the completed extraction from its historical proposed APIs. Do not leave documentation claiming the actor still owns the crew timer/session.

Blueprint assets keep the existing tank query nodes and module finalization call. The component is inherited automatically; designers should not manually add a second one. Its transient state is not save data. `FVehicleModuleSaveData` remains unchanged; save version, finishing work and stable module IDs stay with the existing module system. No active timer, generation, suppression, command token or elapsed crew work is serialized. No new replication/RPC design is part of this extraction.

## 9. Coding and lifetime requirements

Follow [AGENTS.md](../AGENTS.md) for all changed C++:

- Add `GetIsValidTankCrewRepair()` on the actor with actor error reporting. Component member checks go through const boolean validators `GetIsValidTankOwner`, `GetIsValidCrewCommandData` and `GetIsValidModuleArmor`. The command-data validator deliberately has a distinct name from the tank's existing pointer-returning `GetIsValidCommandData` API.
- Component validators use `RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, MemberName, ValidatorName, ContextObject)` because `UActorComponent` is not an actor. Centralize member-pointer errors there; do not repeat them at call sites. Expected pre-setup readiness is handled before requesting required validation.
- Owning component pointer is reflected `TObjectPtr`; externally owned references are reflected weak pointers. Reflect/transient-mark the new state containing the suppression handle. Keep generated-header ordering correct.
- Use `not`, early returns, braces on their own lines, descriptive CamelCase names and private `M_` / `bM_` prefixes. Keep nesting at most two levels and extract logically separate functions longer than 60 lines/long loop bodies. Extract nontrivial switch cases.
- Add class Doxygen and Doxygen for complex multi-parameter setup functions, including each parameter and meaningful return values. Preserve the user's comments when relocating code, updating only those made inaccurate by ownership changes.
- Queries and validators are const. Use named constants for timing and shared tables from balance. Keep mutable UObject work on the game thread.
- Never retain array/map entry addresses across mutations/callbacks. Use value snapshots and exact serial/ID handles. Respect existing armor mutation/dispatch guards and source-generation validation. Clear timers at shutdown and reject stale generations.
- Do not let `FinishCrewRepair` grow into a large nested function; extract timer invalidation, restriction release, session reset and final card/command handling with clear contracts.

## 10. Implementation order

1. Add the native component/types/validators with ticking disabled, and add its owned default subobject plus post-initialization owner/data wiring.
2. Move session/card/timer helpers and queries; use armor snapshots/public tank APIs. Introduce the short owner movement adapter and component source identity.
3. Convert existing actor interface/BP methods to forwarding wrappers and add finalized-armor initialization. Do not rely on component BeginPlay timing.
4. Replace module-batch, behaviour-safe-boundary and import call sites. Apply exact-token failed-start handling and terminal non-restoring release.
5. Wire actor/component shutdown before external lifecycle callbacks; remove actor session/types/helper declarations and the old implementation file.
6. Adapt tests, compile UHT/C++, run relevant automation and PIE checks, then update the two existing documentation files.
7. Search for residual actor state/helpers and old actor-source crew locks. Review the diff against existing local changes; do not revert unrelated work.

## 11. Verification and acceptance criteria

### Existing tests and fixture migration

Keep existing `RTS.VehicleModules.CrewRepair`, `AbilitySuppression`, `HealingAndService`, `ProfilesAndPersistence`, and initial turret finalization/readiness tests passing; run the `RTS.VehicleModules` group after the component move because its fixtures exercise shared module setup.

`FVehicleModuleTestAccess` currently initializes deferred-spawn tank components manually, calls the actor's private `InitializeCrewRepairAbilitySlot`, and changes `Tank.M_CrewRepairState.ModuleWorkStartGameTime` before invoking its tick. Those accesses must be replaced:

- Initialize `UTankCrewRepair` after the fixture assigns its replacement test `UnitCommandData`; the native constructor creates the component, but deferred-spawn fixtures bypass normal post-initialization.
- Call `OnVehicleModulesInitialized(&Armor)` after fixture armor finalization/count setup.
- Give a narrow `FTankCrewRepairTestAccess` friend access under `WITH_DEV_AUTOMATION_TESTS` on the component for advancing its clock/tick and capturing generations. Keep general module private access on the tank; production code gets no friendship.
- Test through public tank wrappers for activation/query compatibility. Do not expose session setters or manual tick functions in production APIs.
- Teardown shuts the component down before world destruction; verify the timer belongs to the component and is cleared even when EndPlay is invoked twice.

### Required behavioral checks

| Scenario | Expected result |
| --- | --- |
| Unfinalized/module-free tank, occupied final slot, no reds, invalid owner/data/armor/world | Inert queries or safely rejected start; useful validation errors; no leaked timer/lock/suppression or stuck queue |
| Engine + two tracks; multi-turret/add-on setup | Priority then fixed slot order; full per-instance durations, 44s example; exactly one repair per due target |
| Hull below/equal/above ordinary gate | Crew leaves hull HP and ordinary finishing work unchanged; only timed red -> yellow |
| Pause and target selected between ticks | Game-time pause freezes work; new target never inherits elapsed work from an old target |
| Damage/new red during active repair | Keep current target/time; new reds join remaining work; repaired-and-red-again target needs full duration |
| Disable then Enable | Immediate stop even with Shift; unfinished time resets, yellow repairs persist, timer generation changes |
| Partial/all external recovery, direct full-service heal | Fresh target or auto-stop at safe batch boundary; no extra heal/timer pulse, yellow intermediate state, duplicate repair or duplicate command completion |
| Shift Enable behind another command, reds disappear before dispatch | Queued Enable is quietly skipped; no session starts |
| Move completion arrives during start/repair; old same-ID token arrives later | Running repair/new execution remains active; stale completion cannot advance it |
| Failed timer start with another CrewRepair ready next | Rollback completes only the failed command's captured token; next command is not completed by the old caller |
| Stop/SetUnitToIdle/replacement/Terminate | Idempotent cleanup; caller owns queue progression exactly once |
| Stun/module suppression + crew; hidden cooldown/grant/revoke changes | Ending crew releases only its source; current entries/cooldowns remain correct; revoked entries never return |
| Turret/hull weapon added or swapped while repairing | New weapons inherit active lock; after stop only currently eligible weapons resume |
| Tracked and wheeled movement/rotation/pursuit | Physical braking and final-rotation reset; late drive/pursuit callbacks cannot restart crew movement |
| Behaviour-iteration heal, bounded deferred drain and re-entrant BP/queue callback | No unsafe release during iteration; pending reconciliation drains once after effects are ready; no cached container references survive callbacks |
| Death before SetUnitDying, tank EndPlay while alive, independent component EndPlay | No weapon enable or queue advancement; timer/card/suppression/source/pending work cleared; repeated shutdown harmless |
| Active-session import, failed import, deferred turret import | Cleanup before actual load, no timer/progress resume, inactive Enable/empty card rebuilt from loaded red state |
| Remaining-time/target actor BP queries | Same API/results: sum reds minus clamped current progress; target becomes INDEX_NONE after stop |

Use focused automation for command/lifetime behavior and PIE for actual locomotion, inherited Blueprint component presence, action UI and turret/hull firing. Compile an Editor target from a fresh editor restart (new reflected class/default subobject) and verify a non-editor target compiles with test access excluded. Do not treat Live Coding alone as sufficient for reflected/default-subobject changes.

Acceptance: the actor no longer owns a crew session/timer/card implementation; only thin wrappers, lifecycle handoffs and owner capabilities remain. One inherited component owns all CrewRepair transient work; command/card/Blueprint contracts, ordinary module healing and module presentation remain compatible. Report actual compilation/tests performed and any unavailable engine/PIE checks when implementing; this planning document itself does not claim those checks ran.
