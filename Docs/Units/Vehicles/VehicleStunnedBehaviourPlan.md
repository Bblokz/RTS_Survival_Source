# UBehVehicleStunned implementation plan

## Existing systems you can directly reuse

### 1) Behaviour lifecycle, stacking, and timed expiry
- `UBehaviourComp` already provides behaviour add/remove/swap, deferred queueing while ticking, stack rule handling, refresh handling, and timed expiry (`AddBehaviour`, `AddBehaviourWithDuration`, `RemoveBehaviour`, `TickComponent`, `HandleTimedExpiry`).
- `UBehaviour` already has all the extension points you need: `OnAdded`, `OnRemoved`, `OnStack`, `SetLifetimeDuration`, and timed lifetime support via `EBehaviourLifeTime::Timed`.

**Impact for stunned:** You can make `UBehVehicleStunned` a normal `UBehaviour` (or `UBehaviourWeapon` if you also need weapon stat changes) and rely on BehaviourComp to auto-expire and call restoration logic in `OnRemoved`.

### 2) Turret rotation speed reduction patterns
You already have two strong reference implementations:

- `UTurretRotationBehaviour`:
  - Applies additive + multiplier rotation changes.
  - Caches per-turret applied delta and safely restores on removal.
  - Supports stacking.
- `ULowEnergyBehaviour`:
  - Caches base speed per turret and restores exact original speeds on removal.
  - Uses weak pointers and safe restore checks.

**Best fit for stunned:** Use the **cache-original-then-restore** model (like `ULowEnergyBehaviour`) for deterministic recovery when other effects can coexist.

### 3) Ability removal + restoration patterns
You have two established patterns:

- `UBehWeaponRemoveAbilities`:
  - Calls `SetUnitToIdle()` on add.
  - Caches removed ability entries with their original index.
  - Restores each ability at the original index on remove.
- `ATrackedTankMaster::OnStartDigIn/OnBreakCoverCompleted`:
  - Explicitly removes move/reverse/rotate abilities.
  - Re-adds move/reverse/rotate using canonical per-unit indices from `FRTS_Statics::GetIndexOfAbilityForBaseTank(...)`.

**Best fit for stunned:** For general behaviour robustness across variants, prefer the `UBehWeaponRemoveAbilities` entry+index cache approach. For strict tank-card canonical ordering, use the `FRTS_Statics` index method.

### 4) Stopping mounted weapon fire immediately
`ATankMaster` already exposes:

- `SetTurretsDisabled()` → disables turrets and hull weapons.
- `SetTurretsToAutoEngage(bool)` → re-enables autonomous targeting.

**Best fit for stunned:**
- On add: call `SetTurretsDisabled()` for immediate fire stop.
- On remove: call `SetTurretsToAutoEngage(true)`.

### 5) Abrupt movement stop hooks
Existing stop mechanisms:

- `ICommands::SetUnitToIdle()`:
  - Terminates current command.
  - Clears queue.
  - Calls unit-specific idle reset logic.
- Tank command termination paths already stop movement/BT in several flows (`ATankMaster`, `ATrackedTankMaster`), and tracked reverse termination explicitly stops controller movement.

**Best fit for stunned:**
- On add, call `ICommands::SetUnitToIdle()` first to clear active intent.
- Then do a hard movement stop via tank/AI stop path if available.

## Proposed `UBehVehicleStunned` design

## Class shape
- Base class: `UBehaviour`.
- Stack rule: `Exclusive` (recommended) unless you intentionally want duration refresh/stack semantics.
- Lifetime: `Timed` for auto-end stun windows.

## OnAdded flow
1. Resolve and cache owner interfaces/pointers (`ICommands`, `ATankMaster`).
2. `SetUnitToIdle()` to abort current command and queue.
3. Hard-stop movement using available tank/AI stop path.
4. Disable mounted weapons via `SetTurretsDisabled()`.
5. Reduce turret rotation speed by applying multiplier to each turret and caching original values.
6. Remove abilities from command card:
   - Required: `IdMove`, `IdReverseMove`.
   - Optional (recommended): `IdRotateTowards` to avoid rotate-only drift while stunned.
   - Cache removed entries + original indices for exact restore.

## OnRemoved flow
1. Restore turret rotation speed from cache.
2. Re-enable mounted weapons via `SetTurretsToAutoEngage(true)`.
3. Restore removed abilities at original indices.
4. Clear all cached state.

## Suggested private state for the behaviour
- `TWeakObjectPtr<ATankMaster> M_TankMaster`
- `TWeakInterfacePtr<ICommands> M_CommandsInterface` (or actor pointer + cast helper)
- `TMap<TWeakObjectPtr<ACPPTurretsMaster>, float> M_CachedTurretRotationSpeeds`
- `TArray<FBehWeaponRemovedAbility> M_RemovedAbilityEntries` (or equivalent stunned-specific struct)
- `TArray<EAbilityID> M_AbilitiesToRemove` defaulted to move/reverse (and optionally rotate).

## Edge cases to account for
- Owner is not an `ATankMaster` (if behaviour can be applied to non-tank actors).
- Stun applied while another behaviour also modifies turret rotation.
- Duplicate re-add attempts when ability already restored by another path.
- Behaviour removed after turret destruction: weak-pointer checks during restore.
- Re-entrant add/remove via stack refresh operations.

## Recommended integration path
1. Implement `UBehVehicleStunned` in `Behaviours/Derived/` using the patterns above.
2. Add to the behaviour class selection where stun effects are applied (ability/effect source).
3. If stun duration is source-driven, use `AddBehaviourWithDuration(...)`.
4. Validate with scenarios:
   - Moving forward, reverse, rotating, attacking.
   - Multiple mounted weapons/turrets.
   - Stun expiry and manual remove.
   - Save/load or actor destroy during stun.

## Key reference files
- `RTS_Survival/Behaviours/BehaviourComp.h/.cpp`
- `RTS_Survival/Behaviours/Behaviour.h/.cpp`
- `RTS_Survival/Behaviours/Derived/BehaviourTurretRotation/BehaviourTurretRotation.h/.cpp`
- `RTS_Survival/Behaviours/Derived/BehaviourWeapon/LowEnergyBehaviour.h/.cpp`
- `RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehWeaponRemoveAbilities.h/.cpp`
- `RTS_Survival/Units/Tanks/TankMaster.cpp`
- `RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.cpp`
- `RTS_Survival/Interfaces/Commands.cpp`
