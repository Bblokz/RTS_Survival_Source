# Shield Feature Implementation Design

## Status

This design is ready for implementation. The open behavior questions have been resolved.

The shield is a tank-owned defensive layer implemented by a dedicated actor component. It has its own capacity, damage reduction, runtime sphere mesh, collision, lifetime/recharge state, command-card ability, and screen-space widget. It must not become part of armor or health, and it must not reuse `FResistanceAndDamageReductionData`.

## Goals

- Support shields on every `ATankMaster` subclass, including nomadic vehicles and trains.
- Exclude aircraft and squads from the initial system.
- Add `EAbilityID::IdActivateShield` as a direct action-button ability with normal command-queue semantics.
- Encapsulate shield state, capacity, damage reduction, timers, mesh, material, and widget behavior in `UShieldComponent`.
- Intercept projectiles, trace weapons, AOE, shrapnel, and flamethrowers.
- Let mines bypass shields completely.
- Let APCR and railgun shots damage the shield but always continue toward the tank's armor.
- Mirror the health bar's actual runtime visibility, including hover, selection, damage, user settings, and hide-all-UI.
- Provide virtual extension points for a later caster/support shield implementation without implementing that subclass now.

## Non-goals

- Do not add shields to aircraft or squads.
- Do not implement the future caster/support system or a concrete derived caster component.
- Do not place armor plates, armor resistance data, or `FResistanceAndDamageReductionData` inside the shield component.
- Do not make absorbed projectile impacts execute explosion AOE, shrapnel, or gameplay damage.
- Do not make shields a generic interception layer around every `AActor::TakeDamage` call. Each supported damage family must opt into the shield contract so mines can bypass it explicitly.

## Existing Project Hooks

- `EAbilityID` and `Global_GetAbilityIDAsString` live in `Player/Abilities.h`.
- `ACPPController::ActivateActionButton` routes immediate action-button abilities.
- `ICommands` and `UCommandData` provide queueing, Shift behavior, command-card validation, execution dispatch, and cooldown start-on-execution.
- Ability components such as `UAttachedWeaponAbilityComponent` defer ability registration to the next tick so tank initialization does not overwrite the entry.
- `AProjectile::OnAsyncTraceComplete` resolves `UArmorCalculation` before performing armor calculations.
- `UWeaponStateTrace` uses a single async line trace and currently proceeds directly to armor/damage processing.
- `FRTS_AOE::DealDamageInRadiusAsync` and `DealDamageVsRearArmorInRadiusAsync` are shared by explosions, shrapnel, grenades, bombs, ICBMs, and mines.
- `UWeaponStateFlameThrower` aggregates hit actors and applies each contact in `OnHitValidActors`.
- `UHealthComponent` already evaluates every required health-bar visibility policy, but currently does not broadcast its final widget visibility.
- `FDamageReductionSettings` in `RTSComponents/DamageReduction/DamageReduction.h` already provides the simple flat-plus-multiplier reduction required by shields.

## Ownership and Scope

### `IShieldOwner`

Add a native, non-Blueprint interface with one lookup-free accessor:

```cpp
virtual UShieldComponent* GetShield() const = 0;
```

`ATankMaster` implements `IShieldOwner`. The base tank caches its optional component once during `PostInitializeComponents`:

```cpp
UPROPERTY()
TObjectPtr<UShieldComponent> M_ShieldComponent = nullptr;
```

All `ATankMaster` subclasses consequently support the interface. A tank without a shield component returns `nullptr`; this is a valid optional state and should not report an initialization error.

Damage code must use `IShieldOwner::GetShield()` or the armor component's cached pointer. It must never call `FindComponentByClass<UShieldComponent>()` at impact time.

When `KillOnDestroyed` destroys the component, `ATankMaster` must clear `M_ShieldComponent` before the component is released. A private cache-clear function callable by the shield component, or an equivalent destruction callback, prevents a strong `TObjectPtr` from retaining the destroyed component.

### Base-component owner validation

The base `UShieldComponent` supports only an `ATankMaster` owner. It should report an invalid setup and disable itself when placed on another actor. The future caster implementation can override an owner-validation extension point if its ownership model differs.

## Proposed File Layout

New files:

- `RTS_Survival/RTSComponents/ShieldComponent/ShieldTypes.h`
- `RTS_Survival/RTSComponents/ShieldComponent/ShieldComponent.h`
- `RTS_Survival/RTSComponents/ShieldComponent/ShieldComponent.cpp`
- `RTS_Survival/RTSComponents/ShieldComponent/ShieldOwner/ShieldOwner.h`
- `RTS_Survival/GameUI/ShieldBar/W_ShieldBar.h`
- `RTS_Survival/GameUI/ShieldBar/W_ShieldBar.cpp`

`ShieldTypes.h` is separate because its damage request/result types are shared by the component and several weapon systems.

## Shared Types

### `EShieldSpawnMode`

```cpp
enum class EShieldSpawnMode : uint8
{
	Immediately,
	OnActivation
};
```

- `Immediately`: activate the default shield during component `BeginPlay`.
- `OnActivation`: do not create the runtime sphere until the first successful public activation.

### `EShieldBehaviour`

```cpp
enum class EShieldBehaviour : uint8
{
	KillOnDestroyed,
	Recharge,
	OnlyRechargeOnActivation
};
```

- `KillOnDestroyed`: zero capacity destroys the shield component.
- `Recharge`: zero capacity hides and disables the sphere, waits `RechargeDelay`, refills to maximum, and automatically activates. If timed life is enabled, its timer starts when recharge completes.
- `OnlyRechargeOnActivation`: zero capacity hides and disables the sphere indefinitely. The next successful external call to `ActivateShield` refills and activates it immediately; it does not wait `RechargeDelay`.

Behavior applies when capacity reaches zero. Timed expiry is a separate transition and does not invoke these depletion policies.

### `EShieldState`

Use an enum instead of multiple overlapping booleans:

```cpp
enum class EShieldState : uint8
{
	Inactive,
	Active,
	Recharging,
	Destroying
};
```

### `EShieldDeactivationReason`

```cpp
enum class EShieldDeactivationReason : uint8
{
	TimedOut,
	OwnerDestroyed
};
```

Depletion uses `OnShieldDestroyed`; it is not a deactivation reason. `OwnerDestroyed` is available for derived cleanup behavior, although normal owner teardown must not start any new timers or effects.

### `EShieldDamageSource`

```cpp
enum class EShieldDamageSource : uint8
{
	Projectile,
	Hitscan,
	AreaOfEffect,
	Shrapnel,
	Flamethrower,
	Mine
};
```

This must be an explicit source rather than a `bBypassShield` boolean. It makes the mine exception reviewable at every shared AOE call site and allows later source-specific shield effects without changing the function contract.

### `EShieldDamageResult`

```cpp
enum class EShieldDamageResult : uint8
{
	NotHandled,
	Absorbed,
	PassThrough
};
```

- `NotHandled`: no active shield exists, or the source is a mine.
- `Absorbed`: stop this damage path at the shield.
- `PassThrough`: shield state was updated, but the original damage path must continue unchanged.

### `FShieldDamageRequest`

The request should contain:

- `float BaseDamage`
- `float RangeAdjustedArmorPenetration`
- `EWeaponShellType ShellType`
- `EShieldDamageSource DamageSource`
- `FVector ImpactLocation`

Non-projectile sources use zero penetration and `Shell_None`. `ImpactLocation` supports future material hit effects without changing every weapon integration again.

### `FShieldActivationOverrides`

Make this `BlueprintType`. Every override has an explicit flag so zero, `nullptr`, and `FVector::ZeroVector` remain valid intentional values:

- `bOverrideShieldMaterial` and `ShieldMaterial`
- `bOverrideRadius` and `Radius`
- `bOverrideTimedLife` and `TimedLife`
- `bOverrideCurrentShields` and `CurrentShields`
- `bOverrideMaxShields` and `MaxShields`
- `bOverrideShieldOffset` and `ShieldOffset`
- `bOverrideShieldBehaviour` and `ShieldBehaviour`

Overrides apply to that activation and its resulting recharge cycle. They must not mutate the authored component defaults. A later activation without overrides starts from the component defaults again.

If maximum alone is overridden, current defaults to the effective maximum. If current is overridden, clamp it to `[0, EffectiveMaxShields]`. A successful activation requires a positive effective maximum and current value.

## Shield Settings

Group the related authored settings in `FShieldComponentSettings`:

| Setting | Type | Semantics |
| --- | --- | --- |
| `SpawnMode` | `EShieldSpawnMode` | Activate immediately or wait for activation. |
| `ShieldBehaviour` | `EShieldBehaviour` | Depletion policy. |
| `MaxShields` | `float` | Default maximum and activation refill amount. |
| `DamageReductionSettings` | `FDamageReductionSettings` | Simple multiplier and flat shield reduction. |
| `ShieldSphereMesh` | `TObjectPtr<UStaticMesh>` | Editable mesh; defaults to `/Engine/BasicShapes/Sphere.Sphere`. |
| `ShieldMaterial` | `TObjectPtr<UMaterialInterface>` | Default material for the sphere. |
| `ShieldBarWidgetClass` | `TSubclassOf<UW_ShieldBar>` | Derived shield widget class. Null disables the bar. |
| `Radius` | `float` | Desired world-space radius passed through the virtual radius application function. |
| `TimedLife` | `float` | `<= 0` is permanent; positive values deactivate on expiry. |
| `RechargeDelay` | `float` | Delay used only by automatic `Recharge`. There is no shields-per-second regeneration. |
| `ShieldOffset` | `FVector` | Sphere offset relative to the tank root. |
| `ShieldBarOffset` | `FVector` | Widget offset relative to the tank root; default `(0, 0, 600)`. |
| `CooldownDuration` | `float` | Cooldown placed on the `IdActivateShield` ability entry. |
| `PreferredAbilityIndex` | `int32` | Requested command-card position. |

The shield must use `FDamageReductionSettings`; it must not include or depend upon `UnitData/ArmorAndResistanceData.h`.

Runtime capacity is stored separately from authored defaults:

```cpp
UPROPERTY()
float M_CurrentShields = 0.0f;

UPROPERTY()
float M_MaxShields = 0.0f;
```

`M_MaxShields` is the effective maximum for the current activation/recharge cycle. Group the other resolved per-activation values—material, radius, lifetime, offset, and behavior—in a shield-specific runtime struct so overrides can be reset atomically on the next default activation.

## `UShieldComponent`

### Public API

Recommended public surface:

```cpp
virtual bool ActivateShield(const FShieldActivationOverrides& ActivationOverrides);
bool CanActivateShield() const;
EShieldDamageResult ApplyShieldDamage(const FShieldDamageRequest& DamageRequest);

float GetCurrentShields() const;
float GetMaxShields() const;
float GetShieldPercentage() const;
EShieldState GetShieldState() const;
bool GetIsShieldActive() const;
UStaticMeshComponent* GetShieldMeshComponent() const;
```

Expose `ActivateShield` to Blueprint. An all-default `FShieldActivationOverrides` value is the no-override call; use `AutoCreateRefTerm` or a no-argument C++ wrapper if required by UHT.

`ActivateShield` is idempotent:

- Return `false` without applying overrides, restarting timers, or changing state when already active.
- Return `false` during automatic recharge; that recharge must finish normally.
- From an inactive zero-capacity `OnlyRechargeOnActivation` shield, refill and activate immediately.
- From an inactive timed-out shield, activate its already reset full capacity.
- Return `true` only when an activation actually occurs.

### Protected virtual extension points

The base component should provide:

```cpp
virtual void BeginPlay_AddAbility();
virtual bool GetIsSupportedShieldOwner() const;
virtual void ApplyShieldRadius(UStaticMeshComponent* ShieldMesh, float Radius) const;
virtual void OnShieldActive();
virtual void OnShieldDamaged(float AppliedDamage, const FShieldDamageRequest& DamageRequest);
virtual void OnShieldDestroyed();
virtual void OnShieldDeactivated(EShieldDeactivationReason Reason);
```

`BeginPlay_AddAbility` is virtual specifically so a later caster-derived component can skip adding `IdActivateShield` or register a different ability. `ApplyShieldRadius` is virtual so the future component can reinterpret radius without rewriting activation.

`OnShieldDestroyed` means shield capacity reached zero, regardless of whether the UObject component is subsequently destroyed. `OnComponentDestroyed` remains the Unreal component-lifecycle callback.

### Default radius implementation

Do not assume the engine sphere has a hard-coded radius. Read the assigned mesh bounds and apply a uniform relative scale:

```text
UniformScale = DesiredRadius / MeshBounds.SphereRadius
```

Reject a non-positive desired radius or a mesh with invalid bounds. This makes the editable sphere mesh setting safe and keeps radius behavior overridable.

## Runtime Mesh and Widget Creation

The shield sphere is a dynamically created `UStaticMeshComponent`:

1. Use the owning tank actor as the `NewObject` outer so `FHitResult::GetActor()` returns the tank.
2. Call `AddInstanceComponent` on the tank.
3. Attach to the tank root with `KeepRelativeTransform`.
4. Apply `ShieldOffset`, mesh, material, and the virtual radius function.
5. Register the component.
6. Apply shield collision.
7. Store it as a component-owned `UPROPERTY() TObjectPtr<UStaticMeshComponent>`.

The screen-space `UWidgetComponent` is also attached to the tank root, at `ShieldBarOffset`. Its widget class is `ShieldBarWidgetClass`, and its actual widget is a separate `UW_ShieldBar`, not a `UW_HealthBar` instance.

For `OnActivation`, defer sphere and shield-bar creation until the first successful activation. Once created, retain both components while inactive or recharging. Hiding and disabling collision avoids repeated creation and GC churn.

Every later activation must reapply its effective material, radius, lifetime, offset, behavior, maximum, and current capacity to the retained sphere.

## Collision Contract

Add a dedicated function to `FRTS_CollisionSetup`:

```cpp
static void SetupShieldCollision(UStaticMeshComponent* ShieldMesh, int32 OwningPlayer);
```

Active collision setup:

- `QueryOnly`.
- Ignore every channel first.
- Block only the weapon trace channel used to hit that owning side:
  - player-one shield: `COLLISION_TRACE_PLAYER`
  - non-player-one shield: `COLLISION_TRACE_ENEMY`
- Do not block or overlap visibility, camera, movement, navigation, building placement, selection, or other object/trace channels.
- Disable overlap generation.
- Disable navigation influence.
- Disable physics simulation and decals where applicable.
- Use `ECC_WorldDynamic` as the object type, not either player/enemy target object type, so the current AOE target queries do not return the tank twice through its shield mesh.

Inactive, recharging, and timed-out shields use `NoCollision` and are hidden. Activation reapplies the library setup rather than relying on stale responses.

Both projectile sweeps and hitscan line traces use these weapon channels, so the sphere intercepts both systems before the armor mesh.

## State Machine

| Current state | Event | Result |
| --- | --- | --- |
| `Inactive` | Successful activation | Resolve effective settings, create/reconfigure sphere, show mesh, enable collision, set `Active`, start lifetime timer if positive, call `OnShieldActive`. |
| `Active` | Activation requested | Reject with no state change and no new cooldown. |
| `Recharging` | Activation requested | Reject; automatic recharge remains authoritative. |
| `Active` | Damage leaves capacity above zero | Update widget and call `OnShieldDamaged`; remain active. |
| `Active` | Capacity reaches zero with `KillOnDestroyed` | Hide/disable, call `OnShieldDestroyed`, clear timers, remove ability, clear owner cache, destroy runtime components, then destroy `UShieldComponent`. |
| `Active` | Capacity reaches zero with `Recharge` | Hide/disable, call `OnShieldDestroyed`, set `Recharging`, start `RechargeDelay`. |
| `Recharging` | Recharge timer completes | Refill to effective maximum, activate automatically, and start a new timed-life timer if applicable. No command cooldown is started. |
| `Active` | Capacity reaches zero with `OnlyRechargeOnActivation` | Hide/disable, call `OnShieldDestroyed`, set `Inactive`, and wait indefinitely. |
| `Inactive` at zero with `OnlyRechargeOnActivation` | External activation | Refill to effective maximum and activate immediately; do not wait `RechargeDelay`. |
| `Active` | Positive `TimedLife` expires | Immediately hide/disable, reset current to effective maximum, set `Inactive`, and call `OnShieldDeactivated(TimedOut)`. Do not enter recharge and do not auto-reactivate. |

Timed-life expiry always deactivates even when capacity remains. Depletion behavior is not consulted for this transition.

## Timers and Destruction Safety

Maintain separate handles for:

- active timed life;
- automatic recharge;
- deferred ability registration if the next-tick API provides a handle.

Rules:

- Starting active life clears any previous lifetime handle before setting a new one.
- Starting recharge clears the lifetime handle.
- Timed expiry clears its own lifetime state before deactivation.
- `KillOnDestroyed` clears every handle before `DestroyComponent`.
- `OnComponentDestroyed` clears every handle, unbinds the health visibility delegate, destroys the runtime mesh/widget components, and clears the tank cache.
- Deferred delegates and timers capture `TWeakObjectPtr<UShieldComponent>` or use an equivalent weak UObject binding.
- Timer callbacks re-check state so stale callbacks cannot reactivate a destroyed, timed-out, or manually changed shield.

## Shield Damage Calculation

### Simple damage reduction

For ordinary sources:

```text
AdjustedShieldDamage = max(0, BaseDamage * DamageReductionMlt - DamageReduction)
```

For APCR and railgun:

```text
ShieldImpactCost = BaseDamage + RangeAdjustedArmorPenetration
AdjustedShieldDamage = max(0, ShieldImpactCost * DamageReductionMlt - DamageReduction)
```

Use the projectile's already range-adjusted penetration value. Do not recalculate range falloff inside the shield component.

### Result rules

| Damage type | Adjusted damage relative to current shields | Shield result | Original damage path |
| --- | --- | --- | --- |
| Ordinary | Less than current | `Absorbed` | Stop. |
| Ordinary | Equal to current | `Absorbed` | Stop, then run zero-capacity behavior. |
| Ordinary | Greater than current | `PassThrough` | Set shields to zero, run zero-capacity behavior, then continue with the full original damage. |
| APCR/Railgun | Any value | `PassThrough` | Subtract/clamp shields and always continue toward armor. |
| Mine | Any value | `NotHandled` | Continue unchanged; shields take no damage. |

There is no overflow-only damage. When a hit passes through, health/armor receives the same original damage it would have received without a shield. Health and armor then apply their existing calculations normally.

An adjusted value of zero is still an absorbed contact for ordinary damage, but it does not reduce capacity.

## Projectile Integration

### Armor component link

Add an optional non-owning pointer to `UArmorCalculation`:

```cpp
UPROPERTY()
TWeakObjectPtr<UShieldComponent> M_ShieldComponent;
```

Provide a setter and a nullable getter that uses `M_ShieldComponent.Get()`. Because absence is valid, do not report the optional pointer as an initialization failure.

`ATankMaster::PostInitializeComponents` caches its shield and links it to its armor calculation component once. If `KillOnDestroyed` destroys the shield component, the weak armor pointer naturally resolves to null.

### Impact order

In `AProjectile::OnAsyncTraceComplete`, after resolving `UArmorCalculation` but before any armor calculation:

1. Ask the armor component for its optional shield.
2. Verify `HitResult.GetComponent()` is exactly the shield's runtime sphere component.
3. If it is not the shield mesh, continue existing armor handling. This prevents a pass-through projectile from charging the shield again when it subsequently hits armor.
4. Build `FShieldDamageRequest` with base damage, `GetArmorPenAtRange()`, shell type, projectile source, and impact location.
5. Apply shield damage and handle the returned disposition.

Capture the hit shield component as a weak component pointer before applying damage. `KillOnDestroyed` may destroy the shield and its runtime mesh inside `ApplyShieldDamage`; pass-through handling must not dereference the component through a now-destroyed shield component afterward.

### Absorbed projectile

When the result is `Absorbed`:

- Stop the projectile and clear its movement/trace timer through the normal pooled shutdown path.
- Propagate the successful hit notification once.
- Create an air-impact VFX/audio event at the shield surface.
- Do not call `SpawnExplosionHandleAOE` or `HandleAoe`.
- Do not generate shrapnel, gameplay AOE, health damage, armor damage, or explosion camera shake.

Add a dedicated shield-impact feedback helper rather than overloading an existing explosion helper that also performs gameplay effects.

### Pass-through projectile

The projectile must ignore only the specific shield sphere component after its first contact. Ignoring the tank actor would also bypass its armor and is forbidden.

Add a per-launch collection such as:

```cpp
UPROPERTY()
TArray<TWeakObjectPtr<UPrimitiveComponent>> M_ComponentsToIgnore;
```

Before each async trace, add valid entries to `FCollisionQueryParams` with `AddIgnoredComponent`. On shield `PassThrough`, add that sphere component and let the projectile continue. Reset this collection on every pooled projectile initialization/restart so a reused projectile never inherits ignored shields from its previous launch.

This component ignore is required both when an ordinary hit overwhelms the shield and whenever an APCR/railgun hit occurs.

## Hitscan Integration

`UWeaponStateTrace` and `UWeaponStateMultiTrace` use `EAsyncTraceType::Single`. A pass-through cannot merely return from the first shield hit because the trace already ended at the sphere.

On a valid trace hit:

1. Cast the hit actor to `IShieldOwner` and obtain the cached shield.
2. Confirm the hit component is the active shield sphere.
3. Apply a hitscan `FShieldDamageRequest` before `DidTracePen` performs armor work.
4. On `Absorbed`, end the trace at the sphere and create air-impact VFX/audio only.
5. On `PassThrough`, issue a continuation trace toward the original `TraceEnd`, ignoring only that shield component.

As with projectiles, retain a weak reference to the hit sphere before applying damage because a `KillOnDestroyed` result can destroy the component during the call.

The continuation trace must preserve the original launch location/time used by the visual tracer and must create impact/damage feedback only once at the final result. Use a named small continuation offset along the trace direction if needed to prevent a boundary re-hit; do not use a magic literal.

Factor the continuation behavior into the base `UWeaponStateTrace` path so `UWeaponStateMultiTrace`, which calls the same completion logic for each ray/socket, receives identical shield behavior.

## AOE, Shrapnel, and Mine Integration

Append a required `EShieldDamageSource` argument to the shared `FRTS_AOE` damage functions and capture it by value in their async completion callbacks. Do not give this argument a default: every call site must deliberately classify itself, and compilation should expose any missed mine/shrapnel path.

For every damaged actor:

1. Calculate the actor's existing falloff damage.
2. Before rear-armor adjustment or `TakeDamage`, use `IShieldOwner` to apply that post-falloff value to an active shield.
3. On `Absorbed`, skip armor and actor damage.
4. On `PassThrough` or `NotHandled`, continue the existing path with the full post-falloff damage.

Shield interception occurs before rear-armor calculation because the shield is outside the tank's armor.

Call-site policy:

| Call site | Source |
| --- | --- |
| Standard explosion AOE | `AreaOfEffect` |
| Projectile and grenade shrapnel | `Shrapnel` |
| Bomb and ICBM explosion damage | `AreaOfEffect` |
| `AFieldMine` explosion | `Mine` |

`Mine` returns `NotHandled`, so the existing mine damage path is unchanged and the shield loses no capacity.

The shield sphere should not use the player/enemy target object types queried by AOE. The AOE query should find the tank actor once, then use `IShieldOwner`; it should not independently damage both tank and sphere components.

## Flamethrower Integration

`UWeaponStateFlameThrower::OnIterationAllRaysComplete` intentionally preserves duplicate actor contacts, and that behavior should remain.

In `OnHitValidActors`, before `FluxDamageHitActor_DidActorDie`:

1. Resolve `IShieldOwner` on each hit actor.
2. Apply `DamageWithFlux` with source `Flamethrower` and zero penetration.
3. On `Absorbed`, skip health damage for that contact.
4. On `PassThrough` or `NotHandled`, apply the full `DamageWithFlux` through the existing function.

Multiple flame contacts may therefore damage the shield multiple times, matching the current flamethrower contact semantics.

## Ability and Command Integration

Follow the full flow in `Docs/Abilities.md`.

### Ability definition and registration

- Add `EAbilityID::IdActivateShield` to `Player/Abilities.h`.
- Add `Global_GetAbilityIDAsString` mapping for `"Activate Shield"`.
- The base shield component creates an `FUnitAbilityEntry` using `CooldownDuration` and `PreferredAbilityIndex`.
- Defer registration to the next tick, following the attached-weapon/aim component pattern, so tank command-data initialization cannot overwrite it.
- Keep `BeginPlay_AddAbility` virtual. A future derived caster component may override it with a no-op or different registration.
- `IdActivateShield` remains subject to normal command-card and cooldown validation; do not add it to `IsAbilityRequiredOnCommandCard` exceptions.
- On `KillOnDestroyed`, remove the ability entry before destroying the component so the command card cannot retain a dead action.

### Public command API

Add:

```cpp
virtual ECommandQueueError ActivateShield(bool bSetUnitToIdle);
virtual void ExecuteActivateShieldCommand();
virtual void TerminateActivateShieldCommand();
```

The enqueue function must:

1. Validate command data and the `IdActivateShield` command-card/cooldown entry.
2. Resolve `IShieldOwner` and its cached shield.
3. Reject active or recharging shields with `AbilityNotAllowed` before calling `SetUnitToIdle` or enqueueing. This guarantees an already-active button press neither interrupts the current command nor starts cooldown.
4. Reject a duplicate queued `IdActivateShield` command.
5. If accepted and `bSetUnitToIdle` is true, clear the current command.
6. Add `IdActivateShield` to `UCommandData`.

Add a shield-state check to queued-command revalidation before `StartCooldownForCommand`. This handles a queued activation that becomes active or starts recharging through an external source before its turn arrives; it is cancelled without cooldown.

`UCommandData::ExecuteCommand` dispatches `IdActivateShield` to `ExecuteActivateShieldCommand`. The default `ICommands` implementation can use `IShieldOwner`, call the component's no-override activation, and immediately call `DoneExecutingCommand(IdActivateShield)`. Termination has no gameplay work because activation itself is instantaneous.

Cooldown starts through the existing `UCommandData::StartCooldownForCommand` path only when an accepted command begins execution. Automatic recharge and external direct component calls do not start the owner's command-card cooldown.

### Controller direct action

Add `IdActivateShield` handling to `ACPPController::ActivateActionButton` and implement `DirectActionButtonActivateShield`.

The helper iterates selected pawn masters, limits execution to actors implementing both `ICommands` and `IShieldOwner`, and calls:

```cpp
Commands->ActivateShield(not bIsHoldingShift);
```

- No Shift: accepted activation interrupts the current command and executes immediately.
- Shift: activation appends to the command queue.
- Aircraft and squads do nothing because they do not implement `IShieldOwner` and never receive the ability entry.

## Shield Bar and Health Visibility Mirroring

### `UW_ShieldBar`

Create a dedicated widget class with an update function that receives current and maximum shields or a normalized percentage. The widget is visually independent of `UW_HealthBar` and is supplied through `ShieldBarWidgetClass`.

The bar updates after:

- activation/refill;
- every applied shield-damage change;
- depletion;
- timed expiry;
- automatic recharge completion.

The bar is always hidden while the shield state is not `Active`.

### Health visibility delegate

Add a native multicast delegate to `UHealthComponent`, for example:

```cpp
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHealthBarVisibilityChanged, ESlateVisibility);
```

Centralize every direct `M_HealthBarWidget->SetVisibility(...)` call behind one health-component helper. That helper sets the actual widget visibility and broadcasts the resulting `ESlateVisibility`.

This centralization must include:

- explicit hide/show functions;
- hover and unhover callbacks;
- selection state;
- damage visibility;
- player/enemy user visibility strategy changes;
- restoring unit defaults;
- hide-all-game-UI and its restoration;
- initial widget setup.

`UShieldComponent` binds once using a weak UObject-safe delegate binding. Its effective widget visibility is:

```text
ShieldBarVisibility = Active ? CurrentHealthBarVisibility : Hidden
```

Copy the exact `ESlateVisibility` value rather than re-evaluating health settings. This guarantees exact mirroring and prevents the shield bar from drifting out of sync with hover or global UI state. On shield activation, immediately sample the current health-bar visibility so the shield bar is correct even when visibility has not just changed.

Unbind during component destruction.

## Lifecycle Hook Ordering

Use consistent hook ordering so later material effects can be added safely:

### Successful activation

1. Resolve effective settings.
2. Create or reconfigure the runtime components.
3. Set state to `Active`.
4. Enable visibility and collision.
5. Update the bar and start timed life.
6. Call `OnShieldActive`.

### Damage without depletion

1. Subtract adjusted damage.
2. Update the bar.
3. Call `OnShieldDamaged`.

### Damage with depletion

1. Set current shields to zero.
2. Update and hide the bar; hide/disable the sphere.
3. Call `OnShieldDamaged`.
4. Call `OnShieldDestroyed` exactly once.
5. Execute the configured depletion policy.

### Timed expiry

1. Clear lifetime state.
2. Hide/disable sphere and bar.
3. Reset current shields to effective maximum.
4. Set state to `Inactive`.
5. Call `OnShieldDeactivated(TimedOut)`.

## Implementation Sequence

1. Add shared shield enums/structs and `IShieldOwner`.
2. Implement `UShieldComponent` state, overrides, runtime sphere creation, collision, timers, hooks, and cleanup.
3. Implement `UW_ShieldBar` and the `UHealthComponent` visibility delegate/central setter.
4. Add `IShieldOwner` and the cached component pointer to `ATankMaster`; link the optional shield to `UArmorCalculation`.
5. Add `FRTS_CollisionSetup::SetupShieldCollision`.
6. Add `IdActivateShield`, component-driven ability registration, controller routing, ICommands queueing, dispatch, termination, and pre-cooldown revalidation.
7. Integrate physical projectiles and component-specific pass-through ignores.
8. Integrate base and multi-trace hitscan continuation.
9. Integrate AOE/shrapnel with explicit source data and the mine bypass.
10. Integrate flamethrower contacts.
11. Add automated tests and perform Blueprint asset setup for material, widget, size, offsets, ability index, cooldown, and behavior.

## Verification Matrix

### Activation and state

- `Immediately` produces a visible, colliding, full shield at game start.
- `OnActivation` creates no sphere until first successful activation.
- Repeated activation while active is a side-effect-free rejection.
- Activation while `Recharge` is waiting is rejected.
- `Recharge` waits exactly `RechargeDelay`, refills, activates, and restarts positive timed life.
- `OnlyRechargeOnActivation` remains down indefinitely and refills immediately when externally activated.
- Timed life `<= 0` never expires.
- Positive timed life immediately deactivates, resets full, and does not auto-reactivate.
- Activation overrides do not modify defaults used by a later default activation.
- Current/max overrides clamp correctly.

### Depletion behavior and cleanup

- `KillOnDestroyed` calls depletion hooks once, removes the ability, clears the tank cache, clears timers, destroys runtime components, and destroys `UShieldComponent`.
- `Recharge` and `OnlyRechargeOnActivation` retain but hide their runtime mesh/widget components.
- Destroying the tank during lifetime/recharge leaves no timer or delegate callback into dead objects.

### Damage boundaries

- Ordinary damage below current shields is absorbed.
- Ordinary damage equal to current shields is absorbed and then depletes.
- Ordinary damage above current shields depletes and passes the full original damage.
- Flat/multiplier reduction uses `FDamageReductionSettings` and never armor resistance data.
- APCR/railgun cost is `(BaseDamage + RangeAdjustedArmorPenetration)` before simple reduction.
- APCR/railgun always passes through, whether or not the shield survives.

### Weapon families

- Player-one and enemy shields block only their correct weapon trace channel.
- An absorbed projectile creates air VFX/audio but no AOE, shrapnel, health damage, or camera shake.
- An overwhelmed ordinary projectile ignores only the sphere and can hit tank armor afterward.
- APCR/railgun ignores only the sphere after first contact and then reaches armor.
- Reused pooled projectiles do not retain previously ignored sphere components.
- Base trace and multi-trace weapons stop at an absorbing shield.
- Trace pass-through continues to armor without duplicate impact/tracer feedback.
- Explosion AOE and shrapnel use the shield before rear armor.
- AOE overload sends the full post-falloff damage into the existing actor path.
- Flamethrower duplicate contacts damage shields with the same multiplicity as current health damage.
- Field mines bypass shields and do not lower shield capacity.

### Ability flow

- The component adds `IdActivateShield` at the configured index/cooldown.
- No Shift interrupts only after activation preflight succeeds.
- Shift queues the activation.
- An already-active press neither interrupts nor starts cooldown.
- A queued activation invalidated by external activation is cancelled before cooldown.
- Cooldown begins when an accepted command executes.
- Automatic recharge does not begin another cooldown.
- Aircraft and squads never receive or execute the ability.

### Widget visibility

- Active shield bar exactly mirrors health-bar `Visible`, `Hidden`, or `Collapsed` state.
- Hover, unhover, selection, deselection, health damage, user setting changes, unit-default restoration, and hide-all-UI update both bars together.
- Inactive/recharging/timed-out shields keep the shield bar hidden even when the health bar is visible.
- Reactivation immediately samples and applies the health bar's current visibility.

## Key Files to Modify

- `RTS_Survival/Player/Abilities.h`
- `RTS_Survival/Player/CPPController.h`
- `RTS_Survival/Player/CPPController.cpp`
- `RTS_Survival/Interfaces/Commands.h`
- `RTS_Survival/Interfaces/Commands.cpp`
- `RTS_Survival/Units/Tanks/TankMaster.h`
- `RTS_Survival/Units/Tanks/TankMaster.cpp`
- `RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h`
- `RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.cpp`
- `RTS_Survival/RTSComponents/HealthComponent.h`
- `RTS_Survival/RTSComponents/HealthComponent.cpp`
- `RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h`
- `RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.cpp`
- `RTS_Survival/Weapons/Projectile/Projectile.h`
- `RTS_Survival/Weapons/Projectile/Projectile.cpp`
- `RTS_Survival/Weapons/WeaponData/WeaponData.h`
- `RTS_Survival/Weapons/WeaponData/WeaponData.cpp`
- `RTS_Survival/Weapons/WeaponData/MultiTraceWeapon/WeaponStateMultiTrace.h`
- `RTS_Survival/Weapons/WeaponData/MultiTraceWeapon/WeaponStateMultiTrace.cpp`
- `RTS_Survival/Weapons/FlameThrowerWeapon/UWeaponStateFlameThrower.h`
- `RTS_Survival/Weapons/FlameThrowerWeapon/UWeaponStateFlameThrower.cpp`
- `RTS_Survival/Utils/AOE/FRTS_AOE.h`
- `RTS_Survival/Utils/AOE/FRTS_AOE.cpp`
- AOE call sites in projectile, grenade, bomb, ICBM, destructible-environment, and field-mine code.
