# Team Weapon Crew Animations and Crew Position Integration — Implementation Plan

Author: design pass, 2026-09-14
Status: **Approved for implementation** — all decisions resolved in chat (§0.2)
Targets:
- `Source/RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/` (anim instance + new montage structs)
- `Source/RTS_Survival/Units/TeamWeapons/` (controller crew tracking)
- `Source/RTS_Survival/Weapons/Turret/` and `Weapons/WeaponData/` (reload-start signal)

---

## 0. Scope

### 0.1 What this task builds

1. A designer-facing struct on `USquadUnitAnimInstance` that holds one full-body montage per crew role
   (`Gunner`, `Loader`, `Spotter`, `AdditionalLoader`), each with a play rate, a "react to weapon fire" flag,
   and an array of **squad-subtype overrides** where one override entry serves any number of subtypes.
2. Runtime playback in the anim instance: looping montages for "in position" roles, and one-shot reaction montages
   whose play rate is computed so they end exactly when weapon 0 of the team weapon finishes reloading.
3. Role tracking and "settled at crew position" detection in `ATeamWeaponController`, so a montage starts only
   when the team weapon is `Ready_Deployed` **and** the operator stands on its `UCrewPosition`, and stops the moment
   the weapon leaves `Ready_Deployed` or the unit stops being an operator.
4. A reload-start signal from the turret to its owner (`ITurretOwner`), plus passing the **flux-adjusted** reload
   time so the sync is exact.

### 0.2 Decisions taken (from the design chat)

| Topic | Decision |
|---|---|
| Signal for react montages | **Reload-start hook** of weapon index 0, new `ITurretOwner::OnTurretWeaponReloadStart`. Duration = the real (flux-adjusted) reload time. |
| Override key | The **team weapon squad's `ESquadSubtype`** (e.g. `Squad_Ger_GrW42_80mm`), read from the controller's RTS component, with the team weapon actor's RTS component as fallback. |
| Between reactions | Optional **`IdleLoopMontage`** per entry (base and override). Null means the unit shows its normal idle. |
| Unresolved montage / no crew position | **Play nothing, no error report.** Normal animations keep running. |

### 0.3 Non-goals

- Guards never play crew montages.
- No AnimGraph changes. The existing `FullBody` slot after the movement blend (see the AnimGraph screenshot) is
  reused, which is what makes these montages override the aim offset.
- No new crew position types, no per-unit-subtype overrides, no multi-weapon support (weapon index 0 only).
- No crew animation while packed, packing, deploying, moving, towed, or abandoned.
- The fire event (`ITurretOwner::OnFireWeapon`) is left untouched and is **not** used for crew montages in this task.

---

## 1. Ground truth from the codebase (load-bearing)

| Fact | Where |
|---|---|
| Squad unit montages are grouped in `EditDefaultsOnly` structs (`FWeaponMontages`, `FAimPositionMontages`); the anim instance plays them through `StartMontage` and one shared `FOnMontageEnded` delegate. | `SquadUnitAnimInstance.h:100-235`, `SquadUnitAnimInstance.cpp:566-600` |
| `StartMontage` derives play rate as `MontageLength / PlayTime` when a target time is given. | `SquadUnitAnimInstance.cpp:566-581` |
| `SetMovementStateWithSpeed` auto-plays crouch/stand transition montages (same `FullBody` slot) on idle-aim and on start-walking. | `SquadUnitAnimInstance.cpp:614-690` |
| `StopAllMontages()` is called by grenade, repair, field construction, scavenging and death paths. | `RepairComponent.cpp:172`, `GrenadeComponent.cpp:562`, `FieldConstructionAbilityComponent.cpp:824`, `SquadUnit.cpp:101,1669` |
| Operators are matched to crew positions **by index** after sorting `UCrewPosition` components by type. Nothing stores which operator holds which `ECrewPositionType`. | `TeamWeaponController.cpp:1860-1966` (`TryGetCrewPositionsSorted`, `IssueMoveCrewToPositions`), `:2534-2576` (`SnapOperatorsToCrewPositions`) |
| `UCrewPosition::M_AcceptanceRadius` (default 75 cm) exists but has no getter and is never read. | `CrewPosition.h:24-29` |
| Crew moves use `ExecuteMoveToSelfPathFinding(..., IdMove, true)`; completion reaches the controller as `OnSquadUnitCommandComplete(EAbilityID)` with **no unit identity**. If the unit is already at the goal, **no completion is reported at all**. | `TeamWeaponController.cpp:28-41`, `SquadUnit.cpp:887-925` (AlreadyAtGoal early return), `SquadUnit.cpp:2055-2062` |
| `HandleDeployingTimerFinished` sets `Ready_Deployed` and then **re-issues** crew position moves. | `TeamWeaponController.cpp:1238-1255` |
| `SetTeamWeaponState` is the single choke point for every state transition (23 call sites). | `TeamWeaponController.cpp:2101-2121` |
| Rotation-in-place teleports operators to their crew positions every tick; state stays `Ready_Deployed`. | `TeamWeaponController.cpp:2406-2453` |
| Operators already have their infantry weapon search disabled and the weapon hidden. | `TeamWeaponController.cpp:1602-1626` |
| Turret fires → `ACPPTurretsMaster::PlayWeaponAnimation` → `TurretOwner->OnFireWeapon(this)`. Once per single shot, once per burst start. | `CPPTurretsMaster.cpp:460-476`, `WeaponData.cpp:1161-1230` |
| Reload → `UWeaponState::Reload()` → `WeaponOwner->OnReloadStart(WeaponIndex, WeaponData.ReloadSpeed)`. The timer uses `GetTimeWithFlux(ReloadSpeed, CooldownFlux)` but the **reported** time has no flux. | `WeaponData.cpp:1344-1357`, `:1495-1501` |
| `ACPPTurretsMaster::OnReloadStart` is `override final` and only forwards to the BP event `ReloadWeapon`. `ITurretOwner` has no reload hook. `OnHullWeaponOutOfRange` shows the default-empty-hook pattern. | `CPPTurretsMaster.h:280,355`, `CPPTurretsMaster.cpp:517-520`, `TurretOwner.h:52-62` |
| For `MagCapacity == 1` single-fire weapons the sequence is: shot → `CoolDown()` → next fire tick with empty mag → `Reload()`. So reload start trails the shot by `BaseCooldown` plus one fire tick. | `WeaponData.cpp:1161-1190`, `:1359-1372` |
| For `SingleBurst` weapons the reload starts when the mag cannot fill a burst (`FireSingleBurst` else-branch, `OnCooldownShutDown`). | `WeaponData.cpp:1192-1220`, `:1626-1636` |
| Squad subtype: `ASquadController::RTSComponent->GetSubtypeAsSquadSubtype()`; the team weapon actor has the same via `ATeamWeapon::GetSquadSubtypeFromRTSComponent()`. Adoption of an abandoned weapon spawns a replacement controller with the team weapon's subtype. | `SquadController.h:389,485`, `TeamWeapon.cpp:234-242`, `SquadController.h:738-769` |
| `ATeamWeaponController` is a friend of `ASquadUnit`; `GetAnimBP_SquadUnit()` is public anyway. | `SquadUnit.h:186,207` |
| Debug flag pattern: `if constexpr (DeveloperSettings::Debugging::G..._Compile_DebugSymbols)`. | `DeveloperSettings.h:1864-1888`, `SquadUnitAnimInstance.cpp:635` |

---

## 2. Designer-facing data model

New header + cpp (used by the anim instance and read by the controller → own file per AGENTS rule 21):

`Source/RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/TeamWeaponCrewMontages/TeamWeaponCrewMontages.h/.cpp`

```cpp
// One playable configuration. Used both as the base entry of a role and as the payload of an override.
USTRUCT(BlueprintType)
struct FTeamWeaponCrewMontageEntry
{
	GENERATED_BODY()

	// Full body montage (FullBody slot). Null means: play nothing for this role (no error).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	// Looping roles: straight play-rate multiplier.
	// Reacting roles: multiplied with the reload-synced rate (see §4.2).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	// False: Montage loops while the operator is in position.
	// True: Montage plays once per reload start of weapon 0 and is stretched to the reload time.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	bool bReactToWeaponFire = false;

	// Only used when bReactToWeaponFire. Loops between reactions; null = normal idle between reactions.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew",
		meta = (EditCondition = "bReactToWeaponFire"))
	TObjectPtr<UAnimMontage> IdleLoopMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew",
		meta = (EditCondition = "bReactToWeaponFire", ClampMin = "0.01"))
	float IdleLoopPlayRate = 1.0f;
};

// One override serves every subtype listed in SquadSubtypes (e.g. 4 mortar squads sharing a crouched spotter).
USTRUCT(BlueprintType)
struct FTeamWeaponCrewMontageOverride
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	TArray<ESquadSubtype> SquadSubtypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	FTeamWeaponCrewMontageEntry Entry;
};

USTRUCT(BlueprintType)
struct FTeamWeaponCrewMontage
{
	GENERATED_BODY()

	// Used by every team weapon squad that has no matching override.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	FTeamWeaponCrewMontageEntry Base;

	// First override whose SquadSubtypes contains the team weapon squad subtype wins.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	TArray<FTeamWeaponCrewMontageOverride> Overrides;
};

USTRUCT(BlueprintType)
struct FTeamWeaponCrewMontages
{
	GENERATED_BODY()

	FTeamWeaponCrewMontages(); // Loader and AdditionalLoader default to bReactToWeaponFire = true.

	/**
	 * @brief Picks the entry a crew member must play so overrides stay a pure data lookup.
	 * @param CrewRole Crew position type the operator was assigned to.
	 * @param TeamWeaponSquadSubtype Subtype of the team weapon squad used as override key.
	 * @return Matching override entry, else the base entry; nullptr for ECrewPositionType::None.
	 */
	const FTeamWeaponCrewMontageEntry* ResolveEntry(const ECrewPositionType CrewRole,
	                                                const ESquadSubtype TeamWeaponSquadSubtype) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	FTeamWeaponCrewMontage Gunner;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	FTeamWeaponCrewMontage Loader;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	FTeamWeaponCrewMontage Spotter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Weapon Crew")
	FTeamWeaponCrewMontage AdditionalLoader;

private:
	const FTeamWeaponCrewMontage* GetMontageForRole(const ECrewPositionType CrewRole) const;
};
```

Resolution rules (implement exactly):
1. `None` → `nullptr`.
2. Overrides are scanned in array order; the first whose `SquadSubtypes` contains the key returns its `Entry`.
3. Otherwise the role's `Base` is returned.
4. The caller treats `Entry->Montage == nullptr` as "play nothing". This makes an override with a null montage a
   deliberate way to **disable** a role for specific subtypes.

Public member naming stays plain (no `M_`) to match the sibling structs `FWeaponMontages` / `FAimPositionMontages`.

On `USquadUnitAnimInstance` add, next to `AimPositionMontages`:

```cpp
UPROPERTY(EditDefaultsOnly, Category = "Team Weapon Crew")
FTeamWeaponCrewMontages TeamWeaponCrewMontages;
```

---

## 3. Anim instance runtime (`USquadUnitAnimInstance`)

### 3.1 Public API (called by `ATeamWeaponController` only)

```cpp
/**
 * @brief Arms the crew animation so the operator animates only while settled on a deployed weapon.
 * Idempotent for the same role and subtype.
 * @param CrewRole Crew position type assigned to this operator.
 * @param TeamWeaponSquadSubtype Override key of the team weapon squad.
 */
void StartTeamWeaponCrewAnimation(const ECrewPositionType CrewRole, const ESquadSubtype TeamWeaponSquadSubtype);

/** @brief Stops loop and reaction montages and returns the unit to regular animations. */
void StopTeamWeaponCrewAnimation();

/**
 * @brief Plays the reaction montage stretched over the reload so it ends when the weapon is ready again.
 * @param ReloadTime Flux-adjusted reload duration of weapon 0 in seconds.
 */
void OnTeamWeaponReloadStarted(const float ReloadTime);

/** @return True while armed, even when the resolved entry has no montage (keeps the controller from re-arming). */
bool GetIsTeamWeaponCrewAnimationActive() const;
```

### 3.2 Private runtime state (rule 20: 4+ related fields → struct, same header)

```cpp
USTRUCT()
struct FSquadUnitTeamWeaponCrewAnimRuntime
{
	GENERATED_BODY()

	void Reset();

	// Copy of the resolved entry; montages are also referenced by the EditDefaultsOnly struct so GC is safe.
	UPROPERTY()
	FTeamWeaponCrewMontageEntry M_ActiveEntry;

	ECrewPositionType M_CrewRole = ECrewPositionType::None;
	ESquadSubtype M_TeamWeaponSquadSubtype = ESquadSubtype::Squad_None;

	// Armed by the controller; cleared by Stop, StopAllMontages, and UnitDies.
	bool bM_IsActive = false;
	// True while a loop montage (Montage or IdleLoopMontage) is expected to be playing.
	bool bM_IsLoopPlaying = false;
	// True while a reaction montage is in flight; suppresses the loop's interrupted-end handling.
	bool bM_IsReactPlaying = false;
};
```

Plus two dedicated delegates (do **not** reuse `M_MontageEndedDelegate`, which `StartMontage` rebinds):
`FOnMontageEnded M_CrewLoopMontageEndedDelegate; FOnMontageEnded M_CrewReactMontageEndedDelegate;`

### 3.3 Behaviour

**Start**
1. If active with the same role and subtype → return.
2. If active with a different role/subtype → `StopTeamWeaponCrewAnimation()` first.
3. `ResolveEntry`; copy into `M_ActiveEntry`; set role/subtype; `bM_IsActive = true` **even if** `Montage` is null
   (decision: silent, and the controller must not retry every tick).
4. Reset aim position to standing (`AimPositionMontages.AimPosition = Standing`,
   `AimOffsets.UpdateAOForNewAimPosition(Standing)`) so no stray crouch→stand transition fires when the loop ends and
   the unit walks off.
5. If `bReactToWeaponFire == false` and `Montage` valid → `PlayCrewLoopMontage(Montage, PlayRate)`.
6. If `bReactToWeaponFire == true` and `IdleLoopMontage` valid → `PlayCrewLoopMontage(IdleLoopMontage, IdleLoopPlayRate)`.

**Loop** (`PlayCrewLoopMontage`): `Montage_Play(Montage, PlayRate)`, then chain every section of the montage
instance to the next one (last back to first) with `Montage_SetNextSection`, so the montage loops by itself with no
gap and without asset edits. Bind `M_CrewLoopMontageEndedDelegate` via `Montage_SetEndDelegate`,
`bM_IsLoopPlaying = true`.
`OnCrewLoopMontageEnded(Montage, bInterrupted)`:
- if `not bM_IsActive` → return;
- if `bInterrupted`: if `bM_IsReactPlaying` → return (the reaction interrupted the idle loop on purpose);
  otherwise something else took the slot → `Reset()` (the controller's tick fallback re-arms when appropriate);
- else (natural end, only possible if chaining could not keep the montage alive) → replay as a fallback.

**Movement backstop**: `SetMovementStateWithSpeed` drops the crew animation (`ClearTeamWeaponCrewAnimationRuntime(true)`)
as soon as the unit's speed exceeds a few cm/s while armed. This covers movement that bypasses the team weapon state
machine (retreat, enter cargo, evasion). The controller notices the inactive anim instance on its tick and re-arms the
operator once it is settled on its crew position again.

**React** (`OnTeamWeaponReloadStarted`):
- if `not bM_IsActive` or `not M_ActiveEntry.bReactToWeaponFire` or `Montage == nullptr` → return;
- compute `PlayRate` per §4.2; `bM_IsReactPlaying = true`; `Montage_Play(Montage, PlayRate)`; bind
  `M_CrewReactMontageEndedDelegate`.
- `OnCrewReactMontageEnded`: `bM_IsReactPlaying = false`; if still active and `IdleLoopMontage` valid and the end was
  not interrupted by a newer reaction → restart the idle loop.

**Stop**: unbind both delegates, `Montage_Stop(CrewMontageBlendOutTime, <montage that is playing>)` for loop and/or
reaction montage, `Reset()`. Constant `CrewMontageBlendOutTime = 0.1f` in an anonymous namespace of the cpp.

**Integration with existing anim code**
- `StopAllMontages()` → also `M_TeamWeaponCrewAnimRuntime.Reset()` and unbind the two delegates. External stops
  (grenade, repair…) therefore deactivate the crew animation; the controller re-arms via its tick fallback (§5.4).
- `UnitDies()` → `Reset()` for hygiene (ragdoll already stops animation).
- `SetMovementStateWithSpeed`: keep updating `MovementState`, but **skip** `OnStartAimingWhileIdle()` and
  `OnStartWalking()` while `bM_IsActive` (they would play a transition montage in the same `FullBody` slot).
- `StartMontage` is untouched; weapon montages use their own slot and cannot interrupt the crew loop. Operators do not
  aim or fire anyway (§1, weapon restrictions).

---

## 4. Reload-start signal and play-rate math

### 4.1 Signal path (weapon 0 → controller → operators)

1. `ITurretOwner` (`TurretOwner.h`): add a default-empty hook next to `OnHullWeaponOutOfRange`:
   ```cpp
   /** Called when a weapon of the turret starts reloading; ReloadTime is the actual timer duration. */
   virtual void OnTurretWeaponReloadStart(ACPPTurretsMaster* CallingTurret, const int32 WeaponIndex,
                                          const float ReloadTime)
   {
   }
   ```
   Existing implementers (`ATankMaster`, `ABuildingExpansion`, `ATestTurretOwner`) need no change.
2. `ACPPTurretsMaster::OnReloadStart` (`CPPTurretsMaster.cpp:517`): after `ReloadWeapon(...)`, add
   `if (TurretOwner) { TurretOwner->OnTurretWeaponReloadStart(this, WeaponIndex, ReloadTime); }`.
3. `UWeaponState::Reload` (`WeaponData.cpp:1344-1357`): pass the local flux-adjusted `ReloadSpeed` to
   `OnReloadStart` instead of `WeaponData.ReloadSpeed`. Side effects, all desirable:
   - `AInfantryWeaponMaster::OnReloadStart` → `PlayReloadAnim(ReloadTime)` now matches the real timer.
   - `AStandaloneTurret::BP_ReloadWeapon` and the turret BP event `ReloadWeapon` receive the real duration.
   - Hull and aircraft weapons ignore the value.
4. `ATeamWeaponController::OnTurretWeaponReloadStart` override (private, in the `ITurretOwner` block):
   - return unless `CallingTurret == M_TeamWeapon`, `WeaponIndex == TeamWeaponCrewAnimationStatics::PrimaryWeaponIndex`
     (constant `0`), `M_TeamWeaponState == Ready_Deployed`, and not abandoned;
   - for every armed slot (§5.2) with a valid operator and anim instance → `OnTeamWeaponReloadStarted(ReloadTime)`.

Timing consequence to document for designers: on `MagCapacity == 1` guns the loader starts one `BaseCooldown`
(plus one turret fire tick) after the shot and ends exactly when the gun can fire again. Keep `BaseCooldown` small on
team weapons that should react immediately. Burst weapons trigger the loader once per magazine, not per burst.

### 4.2 Play-rate math

Engine detail: `UAnimInstance::Montage_Play(Montage, InPlayRate)` sets the instance rate, and
`FAnimMontageInstance::Advance` multiplies it by the asset's `RateScale`. So:

```cpp
// React montage, must end when the reload timer fires.
const float SafeReloadTime = FMath::Max(ReloadTime, TeamWeaponCrewAnimationStatics::MinReloadTimeSeconds); // 0.01f
const float SafeRateScale = FMath::Max(Montage->RateScale, KINDA_SMALL_NUMBER);
const float SyncedPlayRate = Montage->GetPlayLength() / (SafeReloadTime * SafeRateScale);
const float FinalPlayRate = SyncedPlayRate * M_ActiveEntry.PlayRate;   // designer multiplier, no clamp
```

- Designer `PlayRate > 1` finishes early, then the idle loop (or normal idle) shows until the next reload.
- Designer `PlayRate < 1` is cut by the next reaction; intended, no clamp.
- Looping montages use `PlayRate` (or `IdleLoopPlayRate`) directly.
- Recommend keeping `RateScale = 1` on crew montage assets; the formula is exact either way.

---

## 5. Controller: roles, settled detection, arming and disarming

### 5.1 New structs (only used by the controller → `TeamWeaponController.h`)

```cpp
USTRUCT()
struct FTeamWeaponCrewAnimationSlot
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ASquadUnit> M_Operator;

	UPROPERTY()
	TWeakObjectPtr<UCrewPosition> M_CrewPosition;

	ECrewPositionType M_CrewRole = ECrewPositionType::None;

	// Set once the operator was settled on its crew position while Ready_Deployed and the anim instance was armed.
	bool bM_IsArmed = false;
};

USTRUCT()
struct FTeamWeaponCrewAnimationState
{
	GENERATED_BODY()

	void Reset();
	bool GetHasSlotsToArm() const; // any slot with a valid operator, role != None, not armed

	UPROPERTY()
	TArray<FTeamWeaponCrewAnimationSlot> M_Slots;

	ESquadSubtype M_TeamWeaponSquadSubtype = ESquadSubtype::Squad_None;
};
```

Member: `UPROPERTY() FTeamWeaponCrewAnimationState M_CrewAnimationState;`

### 5.2 New private functions

| Function | Responsibility |
|---|---|
| `void RebuildCrewAnimationSlots()` | Called at the end of `AssignCrewToTeamWeapon` (after operator lists exist). Disarms operators that are no longer operators or whose role changed, then rebuilds slots: operator index `i` ↔ `TryGetCrewPositionsSorted()[i]` (the exact mapping `IssueMoveCrewToPositions`/`SnapOperatorsToCrewPositions` use). Operators beyond the position count get `None`. Caches the subtype via `GetTeamWeaponSquadSubtypeForCrewAnimations()`. |
| `bool TryGetCrewPositionForOperatorIndex(int32, UCrewPosition*&) const` | Extracted shared helper; `IssueMoveCrewToPositions` and `SnapOperatorsToCrewPositions` should use it too so the three call sites can never disagree. |
| `ESquadSubtype GetTeamWeaponSquadSubtypeForCrewAnimations() const` | Controller RTS component subtype; if `Squad_None` fall back to `M_TeamWeapon->GetSquadSubtypeFromRTSComponent()`. |
| `bool GetIsOperatorSettledAtCrewPosition(const FTeamWeaponCrewAnimationSlot&) const` | Operator has **no active path following** (`ASquadUnit::GetIsPathFollowingActive`, the AI controller move status), planar speed ≤ `SettledSpeedThresholdCmPerSec` (30 cm/s, braking finished), and 2D distance ≤ `MaxSnapToCrewPositionDistanceCm` (600 cm, stuck guard). Crew moves use partial paths without goal projection, so operators legally stop short of spots near the weapon footprint; a tight radius or a near-zero speed gate is exactly what prevented re-arming after pack → move → deploy. |
| `void TryArmCrewAnimationsForSettledOperators()` | Only when `M_TeamWeaponState == Ready_Deployed`. For each un-armed slot with role ≠ `None`: if settled → `ArmCrewAnimationSlot`. |
| `void ArmCrewAnimationSlot(FTeamWeaponCrewAnimationSlot&)` | Snap the operator onto its crew position with the same landscape-trace teleport the rotation flow uses (`SnapOperatorToCrewPosition`; the full body pose is authored relative to the weapon). If no landscape is hit, only the yaw is aligned. Then `AnimInstance->StartTeamWeaponCrewAnimation(role, subtype)`, `bM_IsArmed = true`. |
| `void DisarmCrewAnimationSlot(FTeamWeaponCrewAnimationSlot&)` | If armed and operator + anim instance valid → `StopTeamWeaponCrewAnimation()`; `bM_IsArmed = false`. |
| `void DisarmAllCrewAnimations()` | Loop over slots. |
| `void TickCrewAnimationArming()` | Called from `Tick`. Runs only while `Ready_Deployed` and there are pending or armed slots. For armed slots: drops the arming when the anim instance reports inactive (external `StopAllMontages`, or the unit walked) and **disarms** when the operator has active path following (it was ordered to move by anything). Standing slightly off the spot never disarms. Then arms settled pending slots. Bounded by operator count. |
| `bool GetIsValidCrewAnimationSlotOperator(const FTeamWeaponCrewAnimationSlot&) const` | Validator per AGENTS rule 0.5 (uses `GetIsValidSquadUnit` + anim instance check). |

Add `float GetAcceptanceRadius() const` to `UCrewPosition`.

### 5.3 Why both event triggers and a tick fallback

- Arrival by move: `OnSquadUnitCommandComplete(IdMove)` fires, but without the unit → the handler simply calls
  `TryArmCrewAnimationsForSettledOperators()` and lets the distance check identify who arrived.
- Already at goal: `MoveToAndBindOnCompleted` returns early on `AlreadyAtGoal` and **never** reports completion.
  `HandleDeployingTimerFinished` re-issues crew moves right after `Ready_Deployed`, so this case is common. Only the
  tick fallback covers it.
- Teleported into place during rotation: `SnapOperatorsToCrewPositions` → call `TryArm…` at its end.
- Because `HandleDeployingTimerFinished` sets `Ready_Deployed` **before** re-issuing moves, arming must never happen
  synchronously inside the `Ready_Deployed` transition; the settled check (velocity ≈ 0) plus the triggers above make
  sure an operator that still has to walk a few centimetres is armed only once it stopped.

### 5.4 Hook points in existing code

| Existing function | Change |
|---|---|
| `SetTeamWeaponState` (`:2101`) | After assigning the state: if `NewState != Ready_Deployed` → `DisarmAllCrewAnimations()`. Nothing on entering `Ready_Deployed` (see §5.3). Covers `Packing`, `Moving`, `Ready_Packed`, `Deploying`, `Towed`, `Abandoned`, `Spawning`. |
| `Tick` (`:108`) | Add `TickCrewAnimationArming();` after `TickRotationRequest`. |
| `OnSquadUnitCommandComplete` (`:578`) | At the top: `if (CompletedAbilityID == EAbilityID::IdMove) { TryArmCrewAnimationsForSettledOperators(); }` then continue the existing flow unchanged. |
| `SnapOperatorsToCrewPositions` (`:2534`) | Call `TryArmCrewAnimationsForSettledOperators()` at the end. |
| `AssignCrewToTeamWeapon` (`:939`) | Call `RebuildCrewAnimationSlots()` right after the operator/guard split (before the abandon check is fine; the abandon path disarms through the state change). |
| `AbandonTeamWeapon` (`:2776`) | State change already disarms; add `M_CrewAnimationState.Reset()` after `M_TeamWeapon = nullptr`. |
| `EndPlay` (`:86`) | `DisarmAllCrewAnimations()` before `Super::EndPlay`. |
| `UnitInSquadDied` (`:566`) | No direct change: `Super` + `AssignCrewToTeamWeapon` → `RebuildCrewAnimationSlots` drops the dead unit (its anim is stopped by ragdoll). |
| `OnActorBeingTowed` / `ExecuteDetachTowCommand` | No change: they go through `SetTeamWeaponState(Towed / Ready_Packed)`. |
| Rotation request (`TickRotationRequest`, `FinishRotationRequest`) | No change: state stays `Ready_Deployed`, loops keep running, snapping keeps operators settled. |
| Dig-in (`OnStartDigIn`, `OnDigInCompleted`, `WallGotDestroyedForceBreakCover`) | No change: state stays `Ready_Deployed`, loops keep running. |

`ITurretOwner` block: add `virtual void OnTurretWeaponReloadStart(...) override;` (§4.1 step 4).

Constants (anonymous namespace `TeamWeaponCrewAnimationStatics` in `TeamWeaponController.cpp`):
`PrimaryWeaponIndex = 0`, `SettledSpeedThresholdCmPerSec = 5.f`.

Debug: add `constexpr bool GTeamWeapon_CrewAnimations_Compile_DebugSymbols = false;` to
`DeveloperSettings::Debugging` and use `if constexpr` prints for arm/disarm/react (role, subtype, reload time,
computed play rate, montage length) so the sync can be verified against the weapon's reload-finished log.

---

## 6. Lifecycle summary

```
AssignCrewToTeamWeapon ──► RebuildCrewAnimationSlots (role per operator, subtype cached)
        │
Deploy timer done ──► SetTeamWeaponState(Ready_Deployed) ──► crew moves re-issued
        │
operator settles (move complete | already there | snapped) ──► TryArm… ──► ArmCrewAnimationSlot
        │                                                                   │
        │                                          loop role: Montage loops (code re-play on end)
        │                                          react role: IdleLoopMontage loops (optional)
        │
weapon 0 Reload() ──► OnReloadStart(flux time) ──► Turret ──► Controller::OnTurretWeaponReloadStart
        │                                                      └► armed operators: react montage, rate = len/(reload*RateScale)*PlayRate
        │
any state ≠ Ready_Deployed | operator removed | unit dies | EndPlay ──► Disarm ──► StopTeamWeaponCrewAnimation
```

---

## 7. Files to touch

| File | Change |
|---|---|
| **NEW** `Units/Squads/SquadUnit/AnimSquadUnit/TeamWeaponCrewMontages/TeamWeaponCrewMontages.h/.cpp` | §2 structs + `ResolveEntry`. |
| `Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h/.cpp` | `TeamWeaponCrewMontages` property, runtime struct, §3 API, guards in `SetMovementStateWithSpeed`, `StopAllMontages`, `UnitDies`. |
| `Units/TeamWeapons/CrewPositions/CrewPosition.h/.cpp` | `GetAcceptanceRadius()`. |
| `Units/TeamWeapons/TeamWeaponController.h/.cpp` | §5 structs, functions, hook points, `OnTurretWeaponReloadStart` override. |
| `Weapons/Turret/TurretOwner/TurretOwner.h` | Default-empty `OnTurretWeaponReloadStart`. |
| `Weapons/Turret/CPPTurretsMaster.cpp` | Forward reload start to the owner. |
| `Weapons/WeaponData/WeaponData.cpp` | `Reload()` reports the flux-adjusted time. |
| `DeveloperSettings.h` | Debug flag. |
| `Units/TeamWeapons/AGENTS.md` | Add a short "Crew animations" section (state-gated, settled-gated, weapon 0 reload hook). |

Not touched: AnimGraph/ABP assets (designer work), `TeamWeaponAnimationInstance`, `SquadController`, `SquadUnit`.

---

## 8. Implementation order

1. `TeamWeaponCrewMontages.h/.cpp` + property on the anim instance. **Build.** Open the squad unit ABP defaults and
   confirm the new category renders (base entry, overrides array with subtype arrays).
2. Anim instance runtime + API (§3). **Build.**
3. `ITurretOwner` hook, turret forward, `WeaponData.cpp` flux change. **Build**, PIE-check infantry reload montages
   still play (duration now varies slightly with flux).
4. `UCrewPosition::GetAcceptanceRadius`, controller structs, `RebuildCrewAnimationSlots`,
   `TryGetCrewPositionForOperatorIndex` refactor of the two existing call sites. **Build.**
5. Arming/disarming + hook points (§5.4) + `OnTurretWeaponReloadStart` override. **Build.**
6. Debug flag and prints. Run the test plan (§9).
7. Update `Units/TeamWeapons/AGENTS.md`.

---

## 9. Test plan (PIE)

Setup: one AT gun squad (e.g. PaK 38) with `Gunner` loop + `Loader` react, and one mortar squad whose `Spotter`
has an override listing all mortar subtypes with a crouched montage. Montages assigned to the `FullBody` slot.
Crew position components on the weapon BPs typed `Gunner`/`Loader`/`Spotter`.

| # | Scenario | Expected |
|---|---|---|
| 1 | Spawn, auto-deploy on idle | Nothing plays during `Deploying`. Gunner loop starts only after the deploy timer **and** arrival; loader shows idle loop (if set) or normal idle. |
| 2 | Operator already standing on its position when deploy finishes | Loop starts within a tick (no move-complete event exists in this case). |
| 3 | Attack target, weapon fires | Loader react montage starts on reload start and ends when the weapon fires again (compare debug print timestamps with the next `OnFireWeapon`). |
| 4 | Loader `PlayRate = 2` | Reaction ends at half the reload; idle loop resumes. `PlayRate = 0.5` → cut by the next reaction. |
| 5 | Burst weapon (MG team) | Loader reacts once per magazine reload, not per burst. |
| 6 | Move order while deployed | Loops stop on `Packing` before units walk. No crouch/stand transition montage fires. |
| 7 | Rotate-towards / internal turret rotation | Loops continue while operators are snapped each tick. |
| 8 | Dig-in and break cover | Loops continue. |
| 9 | Tow attach / detach | Loops stop on `Towed`; after detach they resume only after re-deploy + settle. |
| 10 | Kill an operator (weapon still crewed) | Replacement operator walks in and starts the role's loop; dead unit ragdolls cleanly. |
| 11 | Kill operators until abandoned | All loops stop; adopting squad later plays montages keyed by the **team weapon's** subtype. |
| 12 | Mortar subtype override | Spotter uses the crouched montage on every listed mortar subtype; AT gun spotter (if any) uses base. |
| 13 | Role with null base and no override | Nothing plays, no error in the log. Override with null montage disables the role for those subtypes only. |
| 14 | Grenade/repair path calling `StopAllMontages` on an operator while deployed (force via debug) | Loop stops, then re-arms on the next tick because the unit is still settled and deployed. |
| 15 | Operator count > typed crew positions | Extra operators get `None` and keep normal animations. |

---

## 10. Risks and notes

- **Flux change** (§4.1 step 3) affects every weapon owner's reload animation duration per shot. This is the intended
  fix (animation now matches the timer) but should be called out in the commit message.
- **Blend seams on loops**: loops are produced by chaining the montage instance's sections back to the start, so the
  montage never blends out between iterations and the asset needs no loop sections. The only visible seam is the
  authored first-to-last frame difference of the animation itself.
- **Settled threshold vs. acceptance radius**: the `max()` rule in §5.2 guards against the nav acceptance radius
  (`SquadUnitAcceptanceRadius`, currently 30 cm at `DeveloperSettings.h:1593`) ever being tuned above a crew
  position's radius (75 cm default). Without it a unit could stop legally but never arm.
- **Yaw snap on arm** rotates the unit to the crew position yaw so authored poses face the weapon. This mirrors what
  `SnapOperatorsToCrewPositions` already does during rotation; location is not touched.
- **`Ready_Deployed` transition does not arm directly** (§5.3). If a future change removes the crew-move re-issue in
  `HandleDeployingTimerFinished`, the tick fallback still arms settled operators, so behaviour stays correct.
- **Performance**: the tick fallback is a handful of distance checks and only runs while deployed with pending or
  broken slots. No per-frame work once every operator is armed.
