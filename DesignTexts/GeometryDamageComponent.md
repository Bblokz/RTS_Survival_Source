# GeometryDamageComponent — Design Document

Author: design pass, 2026-07-17
Status: **Approved for implementation** — all open questions resolved (§13)
Target: `Source/RTS_Survival/RTSComponents/GeometryDamageComponent/`

---

## 0. Scope and implementation boundary — READ FIRST

**This work implements exactly one thing: `UGeometryDamageComponent` and its settings structs. Nothing else.**

The component must be **standalone and self-sufficient**: it compiles, initialises, and can be driven entirely from
Blueprint or from a test call, with **zero changes to any existing class**. Integration into the damage pipeline is a
separate, later refactor that is explicitly **not** part of this task.

### 0.1 Files that must NOT be modified

| File | Why it is off-limits |
|---|---|
| `RTSComponents/HealthComponent.h/.cpp` | **Do not touch.** The component only *reads* it through existing public const getters (`GetMaxHealth`, `GetCurrentHealth`, `GetHealthPercentage`). Reading is not refactoring. Do not add hooks, delegates, notifies, or a component pointer to it. |
| `MasterObjects/HealthBase/HPActorObjectsMaster.*` | Do not add the call to `OnDamageTaken` here yet. |
| `MasterObjects/HealthBase/HpPawnMaster.*`, `HpCharacterObjectsMaster.*` | Same. |
| `Weapons/Projectile/Projectile.*` | The `ShotDirection`/`ImpactNormal` gap (§11.2) is **real but deferred**. Do not fix it as part of this task; the component's fallback chain (§11.2) makes v1 work without it. |
| `Collapse/FRTS_Collapse.*` | The death handoff (§11.3) is future integration work. |
| `RTS_Survival.Build.cs` | Already links `Chaos`, `FieldSystemEngine`, `GeometryCollectionEngine`, `PhysicsCore`, `Niagara`. **No build changes needed.** |

If implementing this component appears to *require* editing any file above, that is a signal the component is not
self-sufficient — fix the component, do not widen the change.

### 0.2 What "done" looks like for this task

- The component class + settings structs exist and compile.
- `InitGeometryDamage` binds a Geometry Collection, validates it, and reports precisely why it failed if it did.
- `OnDamageTaken` produces visible physics on a suitably configured Geometry Collection when called manually
  (from a Blueprint test harness, a debug key, or a `UFUNCTION(CallInEditor)`).
- The simulation window opens, resets on new impacts, and quiesces.
- **No existing call site invokes the component.** The designer opts a unit in by adding the component and calling
  `InitGeometryDamage` in Blueprint; wiring it to real damage comes later.

Because nothing calls `OnDamageTaken` yet, ship a debug entry point so the work is testable in isolation:

```cpp
/** @brief Editor/debug only: fires a synthetic impact at a world location to tune forces without a damage source. */
UFUNCTION(BlueprintCallable, CallInEditor, Category = "GeometryDamage|Debug")
void Debug_FireTestImpact(const FVector WorldLocation, const float DamageAmount);
```

---

## 1. Purpose

A reusable `UActorComponent` that turns **damage events with point data** into **Chaos physics fields** on a
`UGeometryCollectionComponent` owned by the same actor, so that a unit visibly *reacts where it was hit*: plates
jump, panels rattle loose, and at low health the structure starts shedding pieces.

Three hard requirements drive the whole design:

1. **Damage must scale against max health**, not against raw damage numbers. A 200-damage hit on a 200 HP scout
   must feel catastrophic; the same hit on a 4000 HP heavy tank must be a chip.
2. **The designer must be able to overrule the math.** The physics gives us a defensible default curve; art
   direction gets a curve asset and a set of multipliers that win.
3. **We must not simulate when nothing is happening.** Impacts open a bounded simulation window; new impacts
   reset it; when it expires the collection is put to sleep.

### 1.1 Non-goals

- This component **does not kill the unit**, does not touch health, and does not decide death. It is a pure
  reaction/FX system driven *by* the damage pipeline. It reads health; it never writes it (§5.1.1).
- It **does not replace `FRTS_Collapse`**. Collapse is the death/destruction event. This component covers the
  *alive-and-being-shot* window. The handoff is described in §11.3.
- It does not fracture the mesh itself — the Geometry Collection asset and its damage thresholds are authored
  content.
- **It does not integrate itself.** Wiring it into the damage pipeline is a later, separate refactor (§0, §11).
- **No squad support** (§13.3). **No replication** (§13.5). **No damage-type-driven behaviour** (§13.4).

---

## 2. Engine ground truth (researched, load-bearing)

Everything in this section was read out of the engine source at
`D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Source` rather than taken from the public docs, which are
vague on the exact semantics. **These facts constrain the design and several of them are counter-intuitive.**

### 2.1 `ApplyPhysicsField` is a one-shot transient command

`UGeometryCollectionComponent::ApplyPhysicsField` (`GeometryCollectionComponent.cpp:5707`) does exactly this:

```cpp
void UGeometryCollectionComponent::ApplyPhysicsField(bool Enabled, EGeometryCollectionPhysicsTypeEnum Target,
                                                     UFieldSystemMetaData* MetaData, UFieldNodeBase* Field)
{
    if (Enabled && Field)
    {
        FFieldSystemCommand Command = FFieldObjectCommands::CreateFieldCommand(
            GetGeometryCollectionPhysicsType(Target), Field, MetaData);
        DispatchFieldCommand(Command);   // buffers to the physics thread for the *next* step
    }
}
```

There is no persistent field. Each call affects **one physics step**. A "sustained" force means *re-issuing the
command every tick*. This is the single most important fact for the architecture — it is why the component needs a
tick at all, and therefore why the tick window matters.

`DispatchFieldCommand` early-outs when `PhysicsProxy` is null. **If the Geometry Collection is not simulating, every
field call silently does nothing.** This is the #1 predicted "why isn't it working" support question, so `Init` must
validate it loudly (§7.1).

### 2.2 Field physics types — exact solver behaviour

From `EGeometryCollectionPhysicsTypeEnum` (`GeometryCollectionSimulationTypes.h:46`) and the solver code in
`PhysicsProxy/FieldSystemProxyHelper.h`. The differences below are **not documented publicly** and directly decide
which field we use:

| Physics type | Solver does | Mass-scaled? | Affects sleeping particles? |
|---|---|---|---|
| `Chaos_LinearImpulse` | `SetLinearImpulseVelocity(Current + Result * InvM())` | **Yes** (`* InvM()`) | **No — `Dynamic` only** |
| `Chaos_LinearVelocity` | `SetV(GetV() + Result)` | **No** (raw velocity add) | No — `Dynamic` only |
| `Chaos_LinearForce` | `AddForce(Result)` | Yes (`a = F/m`) | **Yes — and it wakes them** |
| `Chaos_AngularTorque` | `AddTorque(Result)` | Yes | Yes (same wake path) |
| `Chaos_ExternalClusterStrain` | `SetExternalStrain(Max(Current, Result))` | n/a | n/a (operates on clusters) |
| `Chaos_SleepingThreshold` | sets per-particle material sleeping threshold | n/a | n/a |
| `Chaos_DisableThreshold` | sets per-particle disable threshold | n/a | `Dynamic` only |
| `Chaos_DynamicState` | sets object state (Static/Kinematic/Dynamic) | n/a | yes |

**The critical asymmetry:** `Chaos_LinearForce` contains this, and `Chaos_LinearImpulse` does not:

```cpp
if (RigidHandle->Sleeping())
{
    RigidHandle->SetObjectStateLowLevel(Chaos::EObjectStateType::Dynamic);   // wakes it
}
RigidHandle->AddForce(ResultsView[Index.Result]);
```

Since our performance design *deliberately puts the collection to sleep* between impacts (§8), a design built purely
on `LinearImpulse` would **work on the first hit and silently do nothing on every hit after the collection sleeps**.
This would be a nightmare to debug from the symptom. It is the reason the primary force channel below is
`LinearForce`, not `LinearImpulse`.

Also note every velocity/impulse/force type filters on `ObjectState() == Dynamic`: **pieces still welded into a
cluster do not respond**. Strain must break them loose first, which drives the ordering in §6.4.

### 2.3 Field node vocabulary

From `Field/FieldSystemObjects.h`. These are the nodes we compose (all `NewObject`-ed, all UObjects):

- `URadialFalloff` (`UFieldNodeFloat`) — `Magnitude, MinRange, MaxRange, Default, Radius, Position, Falloff`.
  Scalar sphere. This is the **spatial locality** node: it is what makes a hit local instead of global.
- `URadialVector` (`UFieldNodeVector`) — `Magnitude, Position`. Per the header: *"The direction is the normalized
  vector from the field position to the sample one."* So **positive magnitude points outward** (explosion),
  negative points inward (implosion/sag).
- `UUniformVector` — `Magnitude, Direction`. Output is `Magnitude * Direction`, position-independent. This is our
  directional punch along the shot vector.
- `URandomVector` — `Magnitude`, random direction per sample. The rattle/jitter channel.
- `UNoiseField`, `UBoxFalloff`, `UPlaneFalloff`, `UWaveScalar` — available, not used by the default profiles.
- `UOperatorField` — combines two fields with `EFieldOperationType`: `Field_Multiply / Divide / Add / Substract`.
  This is how a vector direction gets multiplied by a scalar falloff.
- `UCullingField`, `UToFloatField`, `UToIntegerField` — available for future filtering.

`EFieldFalloffType` (`Chaos/Public/Field/FieldSystemTypes.h:131`): `Field_FallOff_None`, `Field_Falloff_Linear`,
`Field_Falloff_Inverse`, `Field_Falloff_Squared`, `Field_Falloff_Logarithmic`.

`EObjectStateTypeEnum` (`GeometryCollectionSimulationTypes.h:33`): `Sleeping=1, Kinematic=2, Static=3, Dynamic=4` —
these are the integer values a `Chaos_DynamicState` field carries.

### 2.4 What the project already does

`FRTS_Collapse.cpp:260` already uses precisely this API, and the new component should stay stylistically consistent
with it:

```cpp
URadialFalloff* StrainFalloff = NewObject<URadialFalloff>(GeoComp);
StrainFalloff->Magnitude = Context->CollapseForce.Force;
StrainFalloff->Radius    = Context->CollapseForce.Radius;
StrainFalloff->Position  = ForceLocation;
StrainFalloff->Falloff   = EFieldFalloffType::Field_Falloff_Linear;
StrainFalloff->MinRange  = 0.0f;
StrainFalloff->MaxRange  = 1.0f;
StrainFalloff->Default   = 0.0f;
GeoComp->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain, nullptr, StrainFalloff);
```

`RTS_Survival.Build.cs` already links everything needed — `Chaos`, `FieldSystemEngine`, `GeometryCollectionEngine`,
`PhysicsCore` (public) and `Niagara` (private). **No build changes are required.**

### 2.5 External sources

Epic's [Chaos Fields User Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/chaos-fields-user-guide-in-unreal-engine)
confirms two things worth keeping: strain magnitude must **exceed the Geometry Collection's authored Damage
Threshold** to break bones (so `MaxStrain` is asset-coupled, §10.2), and Epic explicitly recommends *"avoiding using
falloff with external strain"* for performance. We knowingly deviate on the second point — without a falloff the
strain is uniform and would shatter the entire unit on any hit, losing the whole point of point-local damage. The
mitigation is that our strain radii are small (≤ ~150cm) so the sample set is tiny. This is called out as a
deliberate trade in §8.5.

The [Chaos Destruction Optimization](https://dev.epicgames.com/documentation/en-us/unreal-engine/chaos-destruction-optimization)
page is the reference for the sleep/disable strategy in §8.

---

### 2.6 The substrate: what the collection must be for any of this to work

Confirmed by the design owner: **the Geometry Collection is the unit's visible mesh for the whole life of the unit —
there is no death-time mesh swap** for the units that will adopt this component. That removes the risk that there is
nothing on screen to push.

But visibility was the wrong thing to worry about. **The load-bearing property is the *simulation state*, and it is
a separate axis from visibility.** Three facts collide here:

1. `ApplyPhysicsField` → `DispatchFieldCommand` early-outs when `PhysicsProxy` is null (§2.1). A collection with
   `SetSimulatePhysics(false)` **renders its rest state perfectly and silently swallows every field**.
2. The project's existing convention is that collections are **not** simulating while alive:
   `BuildingExpansion.cpp:350` does `SetSimulatePhysics(false)`, and `FRTS_Collapse.cpp:255` flips it to `true` only
   at collapse.
3. `UGeometryCollectionComponent::ObjectType` (`GeometryCollectionComponent.h:962`) is an `EObjectStateTypeEnum`
   that *"defines how to initialize the rigid objects state, Kinematic, Sleeping, Dynamic"*.

So an adopting unit needs a configuration that is **simulating but does not fall over**:

| Setting | Required value | Consequence if wrong |
|---|---|---|
| `bSimulatePhysics` | **`true`** | `false` → `PhysicsProxy` is null → **every field is a silent no-op**. The #1 predicted support question. |
| `ObjectType` | **`Chaos_Object_Kinematic`** | `Dynamic` → the hull is dynamic from `BeginPlay` and **the unit collapses under gravity while alive**. `Sleeping` → the first `LinearForce` wakes the whole hull (§2.2) and it collapses on the first hit. |

`InitGeometryDamage` therefore: calls `SetSimulatePhysics(true)` if it is off (this is our component's charge — it is
not a refactor of anything), and **validates `ObjectType`, reporting an error if it is not `Kinematic`** rather than
silently forcing it, since `ObjectType` is an `EditAnywhere` property the designer deliberately sets.

#### 2.6.1 The consequence that shapes the whole design: reaction is *break-and-push*, not *wobble*

This follows directly from §2.2 and is the single most important behavioural fact in this document:

> With `ObjectType = Kinematic`, every particle starts **Kinematic**. Force, impulse, velocity and torque fields all
> filter on `Dynamic` (and `LinearForce` additionally on `Sleeping`). **Kinematic particles are completely deaf to
> every force channel.** The *only* thing that can affect them is `Chaos_ExternalClusterStrain`, which breaks them
> loose — and only once freed do they become `Dynamic` and start responding to force.

**Therefore, on a living unit, a hit that does not break a piece loose produces no physics whatsoever.** There is no
"the hull flexes and rattles but stays intact" — Chaos geometry collections are rigid clusters, not soft bodies. A
field cannot deform an intact kinematic cluster, and no amount of force tuning will change that.

This is not a limitation to work around; it is the correct mental model, and the design leans into it:

- **Strain is the gate.** `StrainMagnitude` must exceed the collection asset's authored **Damage Threshold** for
  anything at all to happen. The severity at which that crossing occurs *is* the unit's "damage that tears metal"
  breakpoint, and it is the single most important number a designer tunes.
- **Force is the shaper.** It decides where the freed pieces go, and only affects pieces strain has already freed.
- **Sub-threshold hits are FX-only, by design.** Small-arms fire on a heavy tank plays a spatial impact sound and
  optional Niagara and does nothing physical — which is exactly right, and free.
- **Damage is visibly cumulative and permanent.** A unit progressively sheds plates as it is worn down, and freed
  pieces are left behind by a moving vehicle. This is a genuinely good RTS read, and it composes naturally with the
  health thresholds in §7: the low-health stages are where the unit starts visibly coming apart.
- **Repair does not restore geometry.** Shed pieces are gone. This is a known, accepted asymmetry, and it is a second
  reason `bRearmOnHeal` defaults to `false` (§7.2).

**Static buildings have a second option** worth knowing about but *not* in scope for v1: `ObjectType = Dynamic` plus
`SetAnchoredByBox` to pin the foundation gives real flex and wobble, because the unanchored particles are `Dynamic`
and therefore *can* hear force fields while still held together by internal strain. That is unavailable to vehicles —
anchored dynamic particles do not follow a moving actor's transform. Vehicles get `Kinematic` + break-and-push.

---

## 3. Architecture overview

```
Projectile / AOE / Bomb
        │  (FPointDamageEvent carries HitInfo + ShotDirection)
        ▼
AActor::TakeDamage  ──►  UHealthComponent::TakeDamage   (health truth — untouched)
        │
        └─► UGeometryDamageComponent::OnDamageTaken(FGeometryDamageHit)   ◄── public API
                    │
                    ├── 1. Severity solve      (damage ÷ max health → 0..1 → curve)   §5
                    ├── 2. Threshold check     (health % crossings → extra force + FX) §7
                    ├── 3. Impact enqueue      (coalesced per frame)                   §8.3
                    └── 4. Open/reset sim window → SetComponentTickEnabled(true)       §8
                                │
                       TickComponent (only while window is open)
                                │
                                ├── re-issue LinearForce fields (decaying)  §6.4
                                └── window expired → quiesce + tick off     §8.2
```

The component is **self-contained and owner-agnostic**: it knows about a Geometry Collection and a health source,
nothing else. Any actor with a Geometry Collection can adopt it by calling `InitGeometryDamage` and forwarding
`OnDamageTaken`.

---

## 4. Public API

Per the requirement: a Blueprint-callable init that receives the geo component, and a public damage entry point.

```cpp
/**
 * @brief Binds this component to the Geometry Collection it should drive and to the owner's health source.
 * @param InGeometryCollection The geometry collection that will receive the physics fields.
 * @param InHealthComponent Optional explicit health source; when null the owner is searched for one.
 * @return Whether the component is fully initialised and will respond to damage.
 */
UFUNCTION(BlueprintCallable, Category = "GeometryDamage")
bool InitGeometryDamage(
    UGeometryCollectionComponent* InGeometryCollection,
    UHealthComponent* InHealthComponent = nullptr);

/**
 * @brief Entry point from the owning actor's damage path; converts a point-damage hit into physics fields and FX.
 * @param Hit The impact description; requires a world-space hit location to do anything meaningful.
 */
UFUNCTION(BlueprintCallable, Category = "GeometryDamage")
void OnDamageTaken(const FGeometryDamageHit& Hit);

/** @brief Convenience overload for call sites that only have the engine damage event. */
UFUNCTION(BlueprintCallable, Category = "GeometryDamage")
void OnDamageTakenFromEvent(float DamageAmount, const FPointDamageEvent& DamageEvent, AActor* DamageCauser);

/** @brief Stops all simulation immediately and closes the window; used on death handoff to FRTS_Collapse. */
UFUNCTION(BlueprintCallable, Category = "GeometryDamage")
void StopGeometryDamage();
```

`InitGeometryDamage` returning `bool` rather than `void` is deliberate: the most common failure (the collection not
simulating, §2.1) is silent otherwise, and a Blueprint author gets a return pin they can branch on.

### 4.1 The hit payload

```cpp
/** @brief One impact, in world space, normalised away from the engine damage types. */
USTRUCT(BlueprintType)
struct FGeometryDamageHit
{
    GENERATED_BODY()

    // Raw damage dealt, before any health-relative normalisation.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DamageAmount = 0.f;

    // World-space impact point; the centre of every field this hit produces.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector WorldHitLocation = FVector::ZeroVector;

    // Surface normal at the impact. Zero is tolerated and falls back to the radial direction.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector ImpactNormal = FVector::ZeroVector;

    // Normalised travel direction of the projectile. Zero is tolerated; see the fallback chain in §11.2.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector ShotDirection = FVector::ZeroVector;

    // Health context. Negative means "read live from the health component" — the normal path. See §5.1.1.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxHealthOverride = -1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HealthPercentAfterOverride = -1.f;
};
```

Keeping our own struct rather than passing `FPointDamageEvent` around means AOE, bombs, ICBM and future
melee/collision sources can all feed the component without inventing a fake `FPointDamageEvent`.

**`ERTSDamageType` is deliberately absent.** Damage type does not select the force archetype (decided; §13.4), and
carrying an unused field would invite exactly that scope creep during implementation. If a future version wants
type-driven profiles, it adds the field then.

---

## 5. Damage → severity: the scaling model

This is the heart of the "feels right" requirement. Everything downstream (force magnitude, radius, strain, FX
selection) is driven by **one normalised scalar**, `Severity ∈ [0,1]`. One scalar means one thing to tune, and it
means force/radius/strain/FX can never disagree with each other.

### 5.1 The solve

```
                      DamageAmount
    DamageFraction = ───────────────                     // health-relative, per requirement
                        MaxHealth

                     DamageFraction − DeadzoneFraction
    Normalised   = ──────────────────────────────────── , clamped to [0,1]
                    FullResponseFraction − DeadzoneFraction

    Shaped       = ResponseCurve ? ResponseCurve(Normalised) : pow(Normalised, ResponseExponent)

    Severity     = clamp(Shaped * GlobalSeverityMultiplier, 0, 1)
```

**Why each term exists** — every one of these is answering a specific way this can feel wrong:

- **`DamageFraction`** is the requirement: *damage relative to max health*. Read `MaxHealth` **live on every hit,
  never cached at Init** — `SetMaxHealth`, `AddHealth` and veterancy all mutate it at runtime, and a cached value
  would make veteran units under-react by exactly their veterancy bonus. See §5.1.1 for where the value comes from
  without touching `UHealthComponent`.
- **`DeadzoneFraction`** (default `0.02`) kills the death-by-a-thousand-cuts problem. Without it, a machine-gun
  squad putting 40 chip hits/second on a heavy tank makes it shimmer constantly like it is made of jelly. Below the
  deadzone, `Severity = 0` and **we early-out before touching the physics thread at all** — this is a performance
  gate as much as an aesthetic one.
- **`FullResponseFraction`** (default `0.25`) is "the damage fraction that earns the maximum reaction". Clamping
  here stops overkill from being absurd: a 5000-damage ICBM hit on a 300 HP truck should not launch the wreck into
  orbit, it should do the same thing a 75-damage hit does. Overkill is expressed by the *death* system, not this one.
- **`ResponseExponent`** (default `0.6`) is the taste knob. **Below 1.0 the curve is generous at the low end** —
  small hits still read clearly — which is almost always what you want for game feel, because a linear mapping makes
  everything under a quarter of the max look like nothing happened. Above 1.0 means "only big hits matter".
- **`ResponseCurve`** (`UCurveFloat`, optional) — when set it **replaces** the exponent entirely. This is the
  artistic-control escape hatch: when the exponent cannot express the desired shape (e.g. a deliberate step at 50%),
  the curve wins and the math gets out of the way.

Note `pow(x, 0.6)` and the curve both map `[0,1] → [0,1]`, so `Severity` is always in range and downstream code
never needs to re-clamp.

### 5.1.1 Where health comes from — read-only, no integration

`UHealthComponent` is **read, never modified, never hooked** (§0.1). Everything the component needs already exists as
public `const` Blueprint getters on it today:

```cpp
UFUNCTION(BlueprintCallable) inline float GetMaxHealth() const;        // HealthComponent.h
UFUNCTION(BlueprintCallable) inline float GetCurrentHealth() const;
UFUNCTION(BlueprintCallable, NotBlueprintable) inline float GetHealthPercentage() const;
```

Calling these is not a refactor and requires no changes to that class. `InitGeometryDamage` finds the health
component via `GetOwner()->FindComponentByClass<UHealthComponent>()` when one is not passed explicitly.

**The API contract, stated once and unambiguously:**

> `OnDamageTaken` must be called **after** the damage has been applied to health. At that moment
> `GetCurrentHealth()` already reflects the hit, so the post-hit percentage is simply
> `GetCurrentHealth() / GetMaxHealth()` with no arithmetic and no double-counting.

This is why §11.1's `GotHit` hook is documented as the *non*-recommended integration: it fires **before**
`HealthComponent->TakeDamage`, so health there is still pre-hit and every threshold would fire exactly one hit late.

**Escape hatch for sources with no health component**, so the component never hard-depends on one:

```cpp
// On FGeometryDamageHit. Negative means "read live from the health component" (the normal path).
// A future integration at a pre-health hook, or an actor with no UHealthComponent, sets these explicitly.
UPROPERTY(EditAnywhere, BlueprintReadWrite)
float MaxHealthOverride = -1.f;

UPROPERTY(EditAnywhere, BlueprintReadWrite)
float HealthPercentAfterOverride = -1.f;
```

Resolution order for both: **override if non-negative → live health component → skip the hit entirely** and report
once via `RTSFunctionLibrary::ReportErrorVariableNotInitialised`. Guessing a max health would silently produce
wrong severities forever; refusing to act is louder and cheaper to diagnose. `Debug_FireTestImpact` (§0.2) uses the
overrides, which is what makes the component tunable before any integration exists.

### 5.2 Severity → physical quantities

```
    ForceMagnitude = Lerp(MinForce,  MaxForce,  Severity) * GlobalForceMultiplier
    FieldRadius    = Lerp(MinRadius, MaxRadius, Severity)
    StrainMagnitude= Lerp(MinStrain, MaxStrain, Severity) * GlobalStrainMultiplier
```

**Scaling the radius alongside the magnitude is what sells it**, and it is the thing most implementations miss. If
radius is constant, a small hit and a huge hit disturb *the same number of pieces* and only differ in speed — which
reads as "the same event, faster". Scaling radius with severity means a rifle round nudges one plate while a
howitzer shell heaves a whole quarter of the hull. The event changes *shape*, not just intensity.

### 5.3 The tweakable struct

This is the struct the designer lives in. It is deliberately small — five numbers and an optional curve — because
every knob added here multiplies the tuning space:

```cpp
/** @brief Designer controls for turning raw damage into the 0..1 severity that drives every reaction (§5.1). */
USTRUCT(BlueprintType)
struct FGeometryDamageScalingSettings
{
    GENERATED_BODY()

    // Damage below this fraction of max health is ignored entirely: no fields, no FX, no simulation window.
    // Raise this if sustained small-arms fire makes units shimmer.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DeadzoneFraction = 0.02f;

    // The fraction of max health that earns the maximum reaction. Damage beyond this is clamped, so overkill
    // does not produce absurd forces. Lower it to make the unit read as more fragile.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float FullResponseFraction = 0.25f;

    // Shapes the response between the deadzone and full response. Below 1 is generous to small hits (recommended);
    // above 1 means only heavy hits register. Ignored entirely when ResponseCurve is set.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.05", EditCondition = "ResponseCurve == nullptr"))
    float ResponseExponent = 0.6f;

    // Full artistic override of the response shape. Expects normalised damage on X (0..1) and severity on Y (0..1).
    // When set, this replaces ResponseExponent completely.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    TObjectPtr<UCurveFloat> ResponseCurve = nullptr;

    // Final trim on severity, for per-unit tuning without touching the curve shape.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float GlobalSeverityMultiplier = 1.0f;
};
```

The `EditCondition` on `ResponseExponent` greys it out the moment a curve is assigned. That is a small thing that
prevents a genuinely common confusion — tuning an exponent that is being ignored.

### 5.4 Worked example

Heavy tank, `MaxHealth = 4000`, defaults (`Deadzone 0.02`, `FullResponse 0.25`, `Exponent 0.6`):

| Weapon | Damage | Fraction | Normalised | Severity | Reads as |
|---|---|---|---|---|---|
| MG burst | 15 | 0.004 | 0 (deadzone) | **0.00** | ignored, zero physics cost |
| Autocannon | 90 | 0.023 | 0.011 | **0.07** | faint local tick |
| AP shell | 400 | 0.100 | 0.348 | **0.53** | solid plate jump |
| HEAT shell | 900 | 0.225 | 0.891 | **0.93** | violent, sheds pieces |
| ICBM | 5000 | 1.250 | 1 (clamped) | **1.00** | maximum, not absurd |

Now the same autocannon against a **300 HP scout car**: `fraction = 0.30`, which is already past
`FullResponseFraction`, so it **saturates at `Severity = 1.00`**. The identical weapon that a heavy tank shrugs off
at 0.07 delivers the maximum possible reaction to the scout. That ~14× spread between the two units, from one
unchanged weapon, is the health-relative requirement doing its job.

It also shows the deadzone and the reference clamp are not merely safety rails — they are what *creates* the
contrast between weight classes. The exponent alone would compress it.

---

## 6. Force archetypes — strong field definitions

The requirement asked for *"strong definitions of maybe chaos fields or spherical forces"*. Rather than one hard-coded
behaviour, the designer picks an archetype per profile. Each archetype is a **named, exactly-specified field graph**.

```cpp
UENUM(BlueprintType)
enum class EGeometryDamageForceArchetype : uint8
{
    // Outward sphere from the hit point. The generic "something exploded here".
    RadialBurst      UMETA(DisplayName = "Radial Burst"),
    // Along the shot direction. Reads as a projectile shoving metal inward. Best default for direct fire.
    DirectionalPunch UMETA(DisplayName = "Directional Punch"),
    // Inward pull; structural sag. For low-health thresholds.
    Implosion        UMETA(DisplayName = "Implosion"),
    // Random per-piece jitter. Rattle/groan for a dying unit.
    ChaoticJitter    UMETA(DisplayName = "Chaotic Jitter"),
    // Spin about the impact normal. Panels peel rather than pop.
    TorqueTwist      UMETA(DisplayName = "Torque Twist")
};
```

### 6.0 The force profile struct

One profile fully describes "what a hit does physically". The component holds one as its default, and each threshold
stage (§7.2) holds another for its crossing burst — so the same vocabulary covers both ordinary hits and escalation:

```cpp
/** @brief A complete physical reaction: which field graph, how strong, how wide, and how much it breaks. */
USTRUCT(BlueprintType)
struct FGeometryDamageForceProfile
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    EGeometryDamageForceArchetype Archetype = EGeometryDamageForceArchetype::DirectionalPunch;

    // 0 = pure archetype, 1 = pure radial burst. Blends the two graphs (§6.3). The primary "feel" knob.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RadialVsDirectionalBlend = 0.3f;

    // Force magnitude at Severity 0 and 1. Chaos force units; mass-dependent and asset-coupled (§10.2).
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float MinForce = 25000.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float MaxForce = 750000.f;

    // Field radius at Severity 0 and 1, in cm. Scaling this with severity is what makes big hits read as a
    // different event rather than a faster small one (§5.2).
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float MinRadius = 25.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float MaxRadius = 220.f;

    // Cluster-breaking strain at Severity 0 and 1. MinStrain = 0 keeps ordinary hits non-destructive (§10.2).
    // MaxStrain must exceed the Geometry Collection asset's authored Damage Threshold to break any bones.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float MinStrain = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float MaxStrain = 400000.f;

    // Strain radius is kept separate from and smaller than the force radius: a hit should push a wide area but
    // only break metal near the entry point. Also bounds the falloff-on-strain cost (§8.5).
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float MaxStrainRadius = 150.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    TEnumAsByte<EFieldFalloffType> FalloffType = EFieldFalloffType::Field_Falloff_Squared;

    // Optional shape for the force decay across ImpulseWindowSeconds (§6.5). Linear ramp-down when null;
    // a front-loaded curve gives a sharper crack, a flat one gives a shove.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    TObjectPtr<UCurveFloat> ForceDecayCurve = nullptr;
};
```

**Separating `MaxStrainRadius` from `MaxRadius` is the detail that makes hits look right.** A shell should shove a
wide area of hull but only tear metal loose right where it went in. One shared radius forces a bad choice between
"the push is too local to feel weighty" and "the whole flank shatters".

### 6.1 `RadialBurst`

```
URadialVector  (Magnitude = +ForceMagnitude, Position = HitLocation)      // outward direction
       × (UOperatorField, Field_Multiply)
URadialFalloff (Magnitude = 1, Radius = FieldRadius, Position = HitLocation,
                Falloff = Linear, MinRange = 0, MaxRange = 1, Default = 0)
       → Chaos_LinearForce
```

The `RadialFalloff` here carries `Magnitude = 1` and acts as a pure **0..1 spatial weight**; all the strength lives
in the `RadialVector`. Keeping the magnitude in exactly one node is what makes the graph debuggable — otherwise the
effective strength is a product of two numbers in two places and nobody can tell which one is wrong.

### 6.2 `DirectionalPunch`

```
UUniformVector (Magnitude = ForceMagnitude, Direction = ResolvedShotDirection)
       × (UOperatorField, Field_Multiply)
URadialFalloff (Radius = FieldRadius, Position = HitLocation, Falloff = Squared)
       → Chaos_LinearForce
```

**This is the recommended default for projectile hits, and the reason is worth stating.** A pure radial burst
centred on an off-centre hit throws pieces *away from the hit point in all directions* — it reads as a bomb going
off inside the armour. A real shell drives metal *the way it was travelling*. `Squared` falloff (rather than
`Linear`) concentrates the response tightly around the entry point, which is what makes it read as penetration
rather than as an explosion.

### 6.3 The blend knob — the single best "feel" control

Neither pure archetype is right. Pure directional is a flat shove with no splash; pure radial is a bomb. Real
impacts are both: a dominant push along the shot vector plus a smaller outward spray at the entry.

`RadialVsDirectionalBlend ∈ [0,1]` (default **0.3**) issues both graphs with complementary magnitudes:

```
DirectionalMagnitude = ForceMagnitude * (1 - Blend)
RadialMagnitude      = ForceMagnitude * Blend
```

Two field dispatches instead of one. That cost is real but small and bounded (§8.4), and this knob is where most of
the tuning time will be spent — it is worth the dispatch.

### 6.4 Per-impact command sequence and its ordering hazard

On the frame an impact lands, the component issues, **in this order**:

1. **`Chaos_ExternalClusterStrain`** — `URadialFalloff(Magnitude = StrainMagnitude, Radius = StrainRadius)`.
   Breaks pieces loose. Must come first: §2.2 established that force/impulse/velocity fields only touch `Dynamic`
   particles, so anything still clustered is deaf until strain frees it.
2. **`Chaos_LinearForce`** — the archetype graph(s) from §6.1–6.3. Chosen over `LinearImpulse` specifically because
   **it is the only channel that wakes sleeping particles** (§2.2), and our own sleep strategy guarantees they will
   be asleep by the second hit.
3. **`Chaos_AngularTorque`** *(optional, `TorqueTwist` only)* — same falloff, `URadialVector` about the normal.

**The known ordering hazard, stated plainly:** the strain break in step 1 and the force in step 2 are buffered as
two commands consumed within the same physics step. Pieces that step 1 frees may not yet be `Dynamic` when step 2
evaluates, so **the freshly-broken pieces can miss the first frame of force**. They pick it up on the next tick
because the force is re-issued across the window (§6.5). This is why the sustained window is not merely a "nice
extra" — *it is what makes newly-broken debris move at all*. A single-frame implementation would visibly drop the
first frame of every hit that breaks something. This should be verified in-editor early (§12).

### 6.5 Sustained re-issue and frame-rate independence

`ApplyPhysicsField` being one-shot (§2.1) has a subtle and dangerous consequence: a single `LinearForce` command
produces `Δv = (F/m) · dt` — **it is frame-rate dependent**. Shipping that means the punch is literally twice as
strong at 30fps as at 60fps.

The fix is to re-issue the force every tick for `ImpulseWindowSeconds` (default `0.08`), with a decay:

```
Elapsed01   = (Now - ImpactTime) / ImpulseWindowSeconds
DecayWeight = 1 - Elapsed01                      // linear; ForceDecayCurve may override
IssuedForce = ForceMagnitude * DecayWeight
```

Total delivered `Δv ≈ (F/m) · Σdt ≈ (F/m) · ImpulseWindowSeconds · 0.5`, which **converges to the same value at any
frame rate** because `Σdt` over the window is the window duration regardless of how it is subdivided. The 0.08s
window is short enough to read as an impact rather than a push, and long enough to survive a frame hitch.

`ForceDecayCurve` (optional `UCurveFloat`) replaces the linear ramp — a front-loaded curve gives a sharper crack,
a flat one gives a shove.

---

## 7. Health thresholds — extra forces, sound, and VFX

### 7.1 Reuse the existing health-level notify system

`UHealthComponent` **already has this mechanism** and it should not be rebuilt:

- `EHealthLevel` (`HealthInterface/HealthLevels/HealthLevels.h`): `Level_100Percent, Level_75Percent,
  Level_66Percent, Level_50Percent, Level_33Percent, Level_25Percent`.
- `HealthLevelsToNotifyOn` (`TArray<EHealthLevel>`) already drives `NotifyHealthLevelChange` → `IHealthBarOwner::OnHealthChanged(Level, bIsHealing)`,
  and it already handles **overshoot** (a hit that skips 75% and 66% in one go still reports correctly) via
  `FindOvershotNotifyLevel`.

The component should therefore express thresholds as `EHealthLevel`, not as raw floats. This keeps one vocabulary
across UI, Blueprint, and physics, and inherits the overshoot handling for free — which matters, because a big shell
routinely crosses two thresholds at once and a naive float comparison would fire only the last one, or worse, all of
them at full strength in the same frame.

**Recommendation:** the component computes crossings itself from the health percentage it reads on each hit (so it
stays owner-agnostic and needs no Blueprint wiring), but reuses `EHealthLevel` as the vocabulary. If the owner
already implements `IHealthBarOwner`, forwarding is a valid alternative — but that requires per-actor Blueprint
work, and the point of this component is to be droppable.

### 7.2 Threshold stage definition

```cpp
/** @brief One health crossing: the extra reaction fired the first time the unit drops past a health level. */
USTRUCT(BlueprintType)
struct FGeometryDamageThresholdStage
{
    GENERATED_BODY()

    // The level that arms this stage; fires when health first drops to or below it.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    EHealthLevel TriggerLevel = EHealthLevel::Level_50Percent;

    // Multiplies force magnitude for every hit taken while at or below this level.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "1.0"))
    float SustainedForceMultiplier = 1.0f;

    // One-shot extra field fired at the hit location on the crossing itself.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    bool bFireCrossingBurst = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (EditCondition = "bFireCrossingBurst"))
    FGeometryDamageForceProfile CrossingBurst;

    // Spatial SFX + optional Niagara, played at the hit location.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    FGeometryDamageFX CrossingFX;

    // Whether the stage may re-fire if the unit is repaired back above and damaged down again.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    bool bRearmOnHeal = false;
};
```

`bRearmOnHeal` defaults to **false** and this is a real gameplay concern, not a hypothetical: the codebase has a
repair ability (`DesignTexts/RepairAbility.txt`) and `UHealthComponent::Heal`/`AddHealth`. A tank parked on a repair
pad oscillating around 50% would otherwise re-fire its "structural failure" boom and VFX every few seconds forever.
Default off; opt in per stage.

### 7.3 FX payload — spatial sound is the point

Modelled directly on the existing `FCollapseFX` (`Collapse/CollapseFXParameters.h`), which already carries exactly
the `USoundCue` + `USoundAttenuation` + `USoundConcurrency` + `UNiagaraSystem` set the requirement asks for. Same
shape, but **positioned at the hit location** rather than at a fixed component offset:

```cpp
/** @brief Spatialised SFX/VFX fired at an impact point when a damage threshold is crossed. */
USTRUCT(BlueprintType)
struct FGeometryDamageFX
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    TObjectPtr<USoundCue> ImpactSound = nullptr;

    // Attenuation drives the distance falloff; required for the effect to localise on the hull.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    TObjectPtr<USoundAttenuation> SoundAttenuation = nullptr;

    // Concurrency caps how many of these can overlap; mandatory in RTS crowds. See §8.6.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    TObjectPtr<USoundConcurrency> SoundConcurrency = nullptr;

    // Volume/pitch are scaled by Severity between these and 1.0 so a heavy hit sounds heavier.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float MinVolumeMultiplier = 0.6f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    FVector2D PitchRange = FVector2D(0.95f, 1.05f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    TObjectPtr<UNiagaraSystem> ImpactVfx = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    FVector VfxScale = FVector::OneVector;

    // When true the VFX orients to the impact normal instead of world up.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    bool bAlignVfxToImpactNormal = true;
};
```

Playback via `UGameplayStatics::PlaySoundAtLocation(World, Cue, HitLocation, Rotation, Volume, Pitch, 0.f,
Attenuation, Concurrency)` — the same call the codebase already uses in `FRTS_Collapse.cpp:481` and
`ExplosionManager.cpp:170`, so it inherits the project's existing audio routing.

**On spatial sound selling the destruction:** the reason this matters more here than elsewhere is that the *physics*
is already local to the hit point, so the audio must be too. A non-attenuated 2D cue on a hull impact reads as a UI
sound and actively fights the visual. `SoundAttenuation` is therefore effectively **required**, not optional, and
`Init` should warn when a configured stage has a cue but no attenuation — that combination is nearly always a
mistake rather than a choice.

---

## 8. Performance

The stated requirement: *simulate for a bounded time after an impact, reset on new impacts, and do nothing when
idle*. That is the core, but there are five other places this can go wrong in an RTS with hundreds of units.

### 8.1 The simulation window

```cpp
/** @brief Global cost controls shared by every force type on this component. */
USTRUCT(BlueprintType)
struct FGeometryDamageSimulationBudget
{
    GENERATED_BODY()

    // How long we keep ticking after the last impact. The core idle-cost control.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.05"))
    float SimulateAfterImpactSeconds = 2.5f;

    // Force re-issue window per impact; see §6.5. Must be <= SimulateAfterImpactSeconds.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float ImpulseWindowSeconds = 0.08f;

    // Ceiling on field dispatches per tick; excess impacts coalesce (§8.3).
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "1"))
    int32 MaxFieldDispatchesPerTick = 4;

    // Beyond this distance from the view, skip fields entirely and play FX only (§8.4).
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float MaxSimulationDistanceFromView = 12000.f;

    // Sleeping threshold pushed onto the pieces when the window closes (§8.2).
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    float QuiesceSleepThreshold = 1000.f;

    // Minimum seconds between threshold FX on this component, regardless of stage (§8.6).
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float MinSecondsBetweenThresholdFX = 0.25f;
};
```

Lifecycle:

- Constructor: `PrimaryComponentTick.bStartWithTickEnabled = false;` — matching the convention already used by
  `BehaviourComp.cpp:25`, `LandscapeDeformComponent.cpp:14`, `SpatialVoiceLinePlayer.cpp:17`.
- `OnDamageTaken` with `Severity > 0`: stamp `M_WindowExpiryTime = Now + SimulateAfterImpactSeconds` and
  `SetComponentTickEnabled(true)`. **A new impact just re-stamps the expiry — it never stacks or restarts anything.**
- `TickComponent`: re-issue decaying forces for any impacts still inside their `ImpulseWindowSeconds`; when
  `Now > M_WindowExpiryTime`, quiesce (§8.2) and `SetComponentTickEnabled(false)`.

**Use tick, not a timer.** The force re-issue in §6.5 must land once per *physics-relevant frame* to be
frame-rate-independent; a `FTimerHandle` at a fixed interval would double-issue or skip depending on frame rate and
reintroduce exactly the dt-dependence §6.5 exists to remove. The timer manager is the right tool for the *FX
cooldown*, not for the force loop.

### 8.2 Quiescing — the actual CPU win

Turning off our tick stops *us* issuing commands, but the Chaos solver keeps integrating every loose piece until it
sleeps on its own. **Disabling the tick alone does not save the frame time we care about.** On window expiry:

```cpp
// Raise the sleep threshold so anything still drifting sleeps almost immediately.
UUniformScalar* SleepField = NewObject<UUniformScalar>(GeoComp);
SleepField->Magnitude = Budget.QuiesceSleepThreshold;
GeoComp->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_SleepingThreshold, nullptr, SleepField);
```

Per §2.2 this routes to `UpdateMaterialSleepingThreshold`, setting the per-particle sleeping linear/angular
threshold. A high threshold means "sleep unless you are moving fast", so drifting debris parks itself and leaves the
solver's active set. This mirrors the Sleep/Disable field pattern in Epic's own `FS_MasterField`.

`Chaos_DisableThreshold` is the heavier hammer (removes particles from simulation entirely) and is the right call
for post-death debris — but it is **wrong for a living unit**, because a disabled piece cannot react to the *next*
hit, and this component's whole job is reacting to the next hit. Sleep, don't disable. Sleeping pieces are woken
again for free by the next `LinearForce` (§2.2) — which is the second reason the primary channel is force.

### 8.3 Burst coalescing

An RTS unit under focused fire takes many hits per frame — a squad volley, a burst-fire autocannon, AOE splash
touching several actors. Naively that is N strain + 2N force dispatches in one frame, each allocating field UObjects.

Impacts within a frame are accumulated into a small `TArray<FGeometryDamageActiveImpact>` and resolved at tick:

```cpp
/** @brief A resolved impact being re-issued across its force window; internal bookkeeping, not designer-facing. */
USTRUCT()
struct FGeometryDamageActiveImpact
{
    GENERATED_BODY()

    FVector WorldLocation = FVector::ZeroVector;

    // Already resolved through the fallback chain (§11.2), so tick-time dispatch never re-derives it.
    FVector ResolvedShotDirection = FVector::ZeroVector;

    FVector ImpactNormal = FVector::ZeroVector;

    // Solved once on receipt (§5.1); the force window re-uses it rather than recomputing per tick.
    float Severity = 0.f;

    // World time of the impact; drives the decay weight and window expiry (§6.5).
    float ImpactTime = 0.f;

    // Set when the fallback chain found no usable direction, forcing RadialBurst regardless of profile (§11.2).
    bool bForceRadialFallback = false;
};
```

Resolving the shot direction and severity **once, at receipt** rather than per tick matters: the tick path runs every
frame for the window's duration and should be pure dispatch, and `DamageCauser` may already be destroyed (projectiles
go dormant immediately after `OnHitActor`) by the time the window ticks.


- Impacts closer together than `ImpactMergeDistance` (default 60cm) **merge**: severities combine as
  `1 - (1 - A)(1 - B)` (saturating, never exceeds 1) and the position becomes the severity-weighted centroid.
  Saturating combination is the right rule because two 0.5 hits should read as one strong hit (0.75), not as a
  clamped 1.0 nor a weak 0.5.
- After merging, impacts are sorted by severity and only the top `MaxFieldDispatchesPerTick` dispatch fields. The
  remainder still contribute their FX and still hold the window open — dropping their *fields* is invisible because
  the top-severity impacts dominate the visual, but dropping their *sound* would be audible.

Per AGENTS.md §22, this array is mutated during resolution — iterate by index and stage removals, do not hold
element references across the loop.

### 8.4 Distance culling

This is an RTS: the camera is usually far away and there can be hundreds of units. Beyond
`MaxSimulationDistanceFromView`, **skip field dispatch entirely and play FX only**. Physics detail on a unit that is
twelve metres of screen space away is invisible and is pure cost. The check should use the existing player camera
(`ACameraPawn` / `PlayerCameraController`) rather than `GetFirstPlayerController` each call.

This single gate is likely worth more than every other optimisation here combined, because it scales with the thing
that actually blows up — unit count during a big engagement.

### 8.5 Falloff-on-strain: a deliberate deviation

Epic recommends against falloff on external strain (§2.5). We use it anyway, because uniform strain would break the
entire collection on any hit and destroy the point-local premise. The trade is mitigated by small strain radii
(`MaxStrainRadius ≤ ~150cm`), which keeps the evaluated sample set tiny. **If profiling shows strain evaluation is
hot, the lever is to shrink the radius, not to remove the falloff.**

### 8.6 Audio concurrency

Threshold FX are the most likely thing to embarrass us in a big fight: twenty tanks crossing 50% within a second is
twenty overlapping booms and instant clipping. Three layers, all of which should be used together:

1. `USoundConcurrency` on the cue (the engine's own limiter, with a distance-based resolve rule so nearby units win).
2. `MinSecondsBetweenThresholdFX` per component (stops one unit machine-gunning its own FX).
3. `bRearmOnHeal = false` (§7.2) stops indefinite re-firing.

---

## 9. Proposed header

Conforming to AGENTS.md: `M_`/`bM_` private members, validator functions, Allman braces, `not`, Doxygen on the class
and on complex functions, structs for related state, no magic numbers.

```cpp
// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GeometryDamageSettings.h"
#include "GeometryDamageComponent.generated.h"

class UGeometryCollectionComponent;
class UHealthComponent;

namespace GeometryDamageConstants
{
    // Below this fraction of max health an impact is ignored entirely; no fields, no FX, no window.
    constexpr float DefaultDeadzoneFraction = 0.02f;
    // Impacts closer than this merge into one (§8.3).
    constexpr float ImpactMergeDistance = 60.f;
    // Guards the division in the severity solve.
    constexpr float MinValidMaxHealth = 1.f;
}

/**
 * @brief Drives Chaos fields on an actor's Geometry Collection from point-damage events, scaled by damage
 * relative to max health, with health-threshold escalation and a bounded post-impact simulation window.
 * The bound collection must be simulating with ObjectType Kinematic, so reactions are break-and-push, never flex.
 * @note InitGeometryDamage: call in blueprint to bind the geometry collection before any damage is taken.
 * @note OnDamageTaken: call in blueprint AFTER health has been applied, so the post-hit percentage reads correctly.
 * @note StopGeometryDamage: call in blueprint before handing the collection to FRTS_Collapse on death.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UGeometryDamageComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGeometryDamageComponent();

    UFUNCTION(BlueprintCallable, Category = "GeometryDamage")
    bool InitGeometryDamage(UGeometryCollectionComponent* InGeometryCollection,
                            UHealthComponent* InHealthComponent = nullptr);

    UFUNCTION(BlueprintCallable, Category = "GeometryDamage")
    void OnDamageTaken(const FGeometryDamageHit& Hit);

    UFUNCTION(BlueprintCallable, Category = "GeometryDamage")
    void OnDamageTakenFromEvent(float DamageAmount, const FPointDamageEvent& DamageEvent, AActor* DamageCauser);

    UFUNCTION(BlueprintCallable, Category = "GeometryDamage")
    void StopGeometryDamage();

    /**
     * @brief Fires a synthetic impact so forces can be tuned before any damage-pipeline integration exists (§0.2).
     * @param WorldLocation Where to centre the fields; normally a socket or a trace hit on the collection.
     * @param DamageAmount Raw damage; normalised against MaxHealthOverride when no health component is present.
     */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "GeometryDamage|Debug")
    void Debug_FireTestImpact(const FVector WorldLocation, const float DamageAmount);

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Stand-in max health used by Debug_FireTestImpact when the owner has no UHealthComponent (§5.1.1).
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GeometryDamage|Debug", meta = (ClampMin = "1.0"))
    float DebugMaxHealthOverride = 1000.f;

    // How raw damage becomes a 0..1 severity (§5).
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GeometryDamage|Scaling")
    FGeometryDamageScalingSettings ScalingSettings;

    // The force archetype and magnitudes used for ordinary hits (§6).
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GeometryDamage|Forces")
    FGeometryDamageForceProfile DefaultForceProfile;

    // Health crossings that add force and fire spatial FX (§7). Evaluated low-to-high.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GeometryDamage|Thresholds")
    TArray<FGeometryDamageThresholdStage> ThresholdStages;

    // Global cost controls for every force type on this component (§8).
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GeometryDamage|Performance")
    FGeometryDamageSimulationBudget SimulationBudget;

private:
    /** @return Severity in 0..1 for this hit, or 0 when the hit is inside the deadzone. */
    float CalculateSeverity(const float DamageAmount) const;

    /** @return Current health percentage read live from the health component; 1.0 when unavailable. */
    float GetCurrentHealthPercentage() const;

    void DispatchStrainField(const FGeometryDamageActiveImpact& Impact, const FGeometryDamageForceProfile& Profile);
    void DispatchForceFields(const FGeometryDamageActiveImpact& Impact, const FGeometryDamageForceProfile& Profile,
                             const float DecayWeight);
    void DispatchArchetypeGraph(const FGeometryDamageActiveImpact& Impact, const FGeometryDamageForceProfile& Profile,
                                const EGeometryDamageForceArchetype Archetype, const float Magnitude);

    void EvaluateThresholdCrossings(const FGeometryDamageHit& Hit, const float HealthPercentAfter);
    void PlayThresholdFX(const FGeometryDamageFX& FX, const FGeometryDamageHit& Hit, const float Severity);

    void OpenOrResetSimulationWindow();
    void QuiesceSimulation();

    void Tick_ResolvePendingImpacts();
    void Tick_ReissueSustainedForces(const float DeltaTime);

    // Set once by InitGeometryDamage; every field dispatch goes through this component.
    UPROPERTY()
    TObjectPtr<UGeometryCollectionComponent> M_GeometryCollection;
    bool GetIsValidGeometryCollection() const;

    // Health source; MaxHealth is read live per hit because veterancy/repairs mutate it at runtime (§5.1).
    UPROPERTY()
    TWeakObjectPtr<UHealthComponent> M_HealthComponent;
    bool GetIsValidHealthComponent() const;

    // Impacts still inside their ImpulseWindowSeconds, re-issued each tick (§6.5). Index-iterate only (§8.3).
    UPROPERTY()
    TArray<FGeometryDamageActiveImpact> M_ActiveImpacts;

    // Impacts received this frame, not yet coalesced into M_ActiveImpacts (§8.3).
    UPROPERTY()
    TArray<FGeometryDamageActiveImpact> M_PendingImpacts;

    // Stages that have already fired; index-aligned with ThresholdStages. Cleared per stage on heal if bRearmOnHeal.
    TBitArray<> M_FiredThresholdStages;

    // World time after which we quiesce and stop ticking; re-stamped by every qualifying impact.
    float M_WindowExpiryTime = 0.f;

    float M_LastThresholdFXTime = 0.f;

    bool bM_IsInitialised = false;
};
```

### 9.1 Notes on the sketch

- `M_GeometryCollection` is `TObjectPtr` (component on the owning actor) while `M_HealthComponent` is
  `TWeakObjectPtr` — per AGENTS.md §17, both live on the owner but the health component is not owned by us and may
  outlive/precede our teardown ordering. Both get validator functions per §0.5.
- `TBitArray<>` for fired stages is deliberately not a `UPROPERTY` — it holds no object references, and it is
  index-aligned with `ThresholdStages`, which is a `UPROPERTY` and therefore stable.
- No field-node UObjects are cached as members. They are created per dispatch with `NewObject<>(GeoComp)` and
  consumed by the command, exactly as `FRTS_Collapse` does. **Do not cache and mutate a shared field node** — the
  command is consumed asynchronously on the physics thread and mutating a node that an in-flight command still
  references is a data race.

---

## 10. Recommended defaults

### 10.1 Scaling

| Field | Default | Rationale |
|---|---|---|
| `DeadzoneFraction` | 0.02 | Below 2% of max HP nothing happens; kills MG shimmer and the physics cost with it |
| `FullResponseFraction` | 0.25 | A quarter of max HP in one hit earns the maximum reaction |
| `ResponseExponent` | 0.6 | Sub-linear: small hits stay legible |
| `ResponseCurve` | null | Opt-in; when set it fully replaces the exponent |
| `GlobalForceMultiplier` | 1.0 | Per-unit trim without touching the curve |

### 10.2 Forces (starting points — **expect to retune per Geometry Collection asset**)

| Field | Default | Rationale |
|---|---|---|
| `Archetype` | `DirectionalPunch` | Best read for direct-fire projectiles (§6.2) |
| `RadialVsDirectionalBlend` | 0.3 | Dominant shove, light outward spray |
| `MinForce` / `MaxForce` | 25 000 / 750 000 | Chaos force units; mass-dependent, asset-coupled |
| `MinRadius` / `MaxRadius` | 25 / 220 cm | Radius scaling is what changes the event's *shape* (§5.2) |
| `MinStrain` / `MaxStrain` | 0 / 400 000 | **Must exceed the asset's authored Damage Threshold to break bones** |
| `MaxStrainRadius` | 150 cm | Keeps the falloff-on-strain cost bounded (§8.5) |
| `FalloffType` | `Field_Falloff_Squared` | Tight, local; reads as penetration not detonation |

**`MinStrain = 0` is intentional, and §2.6.1 is why it is not the knob it first appears to be.** On a kinematic
collection, strain below the asset's Damage Threshold breaks nothing, and a hit that breaks nothing does nothing at
all — force fields cannot touch an intact kinematic cluster. So low-severity hits are **FX-only by design**, and
`MinStrain = 0` simply makes that explicit rather than pretending a sub-threshold strain value accomplishes
something.

**The number that actually matters is the severity at which `StrainMagnitude` crosses the asset's Damage
Threshold.** That crossing point *is* the unit's "damage that tears metal" breakpoint. Because
`StrainMagnitude = Lerp(MinStrain, MaxStrain, Severity)`, with `MinStrain = 0` the crossing sits at:

```
    BreakSeverity ≈ AssetDamageThreshold / MaxStrain
```

So with `MaxStrain = 400 000` against an asset threshold of `200 000`, metal starts tearing at `Severity ≈ 0.5` —
which, from the §5.4 table, is about an AP shell on a heavy tank. **Tune `MaxStrain` against the asset's authored
threshold to place that breakpoint where you want it**, and read the §5.4 table to see which weapons land above it.
If everything shatters, `MaxStrain` is too high; if nothing ever breaks, it is below the asset threshold and the
component will look completely dead — which is the failure mode most likely to be misread as "the code is broken".

The force and strain numbers are **coupled to each specific Geometry Collection asset's mass and authored Damage
Threshold** and are first-look values only. Do not treat the table as truth; treat it as a place to start bisecting.

### 10.3 Suggested threshold stages

| Level | `SustainedForceMultiplier` | Crossing burst | FX intent |
|---|---|---|---|
| `Level_50Percent` | 1.25 | `RadialBurst`, moderate strain | Metal groan, light sparks, small smoke |
| `Level_25Percent` | 1.6 | `ChaoticJitter` + strain above asset threshold | Structural failure boom, heavy sparks, shedding |

Two stages is the right starting point. More is tempting and usually reads as noise — each stage needs to be
*distinguishable* to earn its place.

---

## 11. Integration — **FUTURE WORK, NOT PART OF THIS TASK**

> **Implementer: do not act on this section.** It is recorded so the later integration refactor starts from
> research already done, not because any of it is in scope now. Per §0.1, `HealthComponent`, the `HpMaster`
> classes, `Projectile`, and `FRTS_Collapse` are **off-limits**. This section is reference material with a
> deliberately deferred trigger — nothing here is a to-do.
>
> The only thing this section changes about the current task is that §11.2's fallback chain **must** be implemented
> in the component, precisely so that v1 works correctly *without* any of the changes described below.

### 11.1 The hook already exists (for later)

`AHPActorObjectsMaster::TakeDamage` (`HPActorObjectsMaster.cpp:35`) already calls `GotHit(DamageEvent)` on every
hit, before health is applied:

```cpp
if (IsValid(HealthComponent))
{
    GotHit(DamageEvent);                                  // <-- existing hook
    const ERTSDamageType RtsDamageType = FRTSWeaponHelpers::GetRTSDamageType(DamageEvent);
    if (HealthComponent->TakeDamage(DamageAmount, RtsDamageType)) { /* death */ }
}
```

`GotHit` is a `BlueprintImplementableEvent`, so a first integration needs **no C++ changes to the master classes**:
a Blueprint can call `OnDamageTakenFromEvent` from `GotHit`. `HpPawnMaster` (`:82`) and `HpCharacterObjectsMaster`
(`:129`) have the identical hook.

**Ordering caveat:** `GotHit` fires *before* `HealthComponent->TakeDamage`, so at that moment the health component
still reports the **pre-hit** health. The threshold logic in §7 needs the post-hit percentage. The component must
therefore compute it itself:

```
HealthPercentAfter = (CurrentHealth - DamageAmount) / MaxHealth
```

rather than reading `GetHealthPercentage()` at hook time. Reading it directly would make every threshold fire exactly
one hit late — subtle, and easy to ship by accident.

For a C++ integration, call `OnDamageTaken` *after* `HealthComponent->TakeDamage` returns instead, and read the
percentage live. This is cleaner and is the recommended end state.

### 11.2 The projectile does not populate enough hit data — **deferred; the component must cope**

This is the most important integration finding, but it is **not fixed as part of this task**. The component's
fallback chain (below) is what makes v1 correct without it, and that chain **is** in scope.

`AProjectile::OnHitActor` (`Projectile.cpp:1641`) does:

```cpp
M_DamageEvent.HitInfo.Location = HitLocation;
HitActor->TakeDamage(M_FullDamage * DamageMlt, M_DamageEvent, nullptr, this);
```

`M_DamageEvent` is an `FPointDamageEvent` (`Projectile.h:356`), but **only `HitInfo.Location` is ever set**.
`HitInfo.ImpactNormal`, `HitInfo.ImpactPoint`, and `FPointDamageEvent::ShotDirection` are all left at their previous
or default values — and because `M_DamageEvent` is a *reused member*, stale values from an earlier hit can leak in,
which is worse than them being zero.

`ShotDirection` is exactly what `DirectionalPunch` (the recommended default archetype) needs. So:

**Deferred change (DO NOT MAKE NOW)** in `AProjectile::OnHitActor` and `OnOverPenetratingArmorHit` (`:2061`):

```cpp
M_DamageEvent.HitInfo.Location      = HitLocation;
M_DamageEvent.HitInfo.ImpactPoint   = HitLocation;
M_DamageEvent.HitInfo.ImpactNormal  = HitResult.ImpactNormal;
M_DamageEvent.ShotDirection         = GetVelocity().GetSafeNormal();
```

`OnHitActor` currently receives a `HitRotation` rather than the `FHitResult`, so it needs the normal threaded
through (the callers already have `HitResult` — `Projectile.cpp:1884` and the armor-calc paths at `:2028`/`:2073`
have it in hand).

**Defensive fallback — IN SCOPE, and load-bearing.** Because the projectile fix is deferred, `ShotDirection` will
be zero or stale in practice for v1. The component must therefore never depend on it, and must also tolerate
AOE/bomb sources that genuinely have no shot vector:

```
ResolveShotDirection():
  1. Hit.ShotDirection            if non-near-zero
  2. -Hit.ImpactNormal            if non-near-zero      (drive into the surface)
  3. (HitLocation - Causer->GetActorLocation()).GetSafeNormal()
  4. fall back to RadialBurst     (blend forced to 1.0)
```

Step 4 matters: with no direction at all, `DirectionalPunch` would degenerate to a zero-magnitude uniform vector and
the hit would produce **nothing**. Silently falling back to `RadialBurst` is strictly better than a silent no-op,
and AOE hits genuinely have no meaningful shot direction anyway — radial is the *correct* answer for them, not a
consolation prize.

### 11.3 Handoff to `FRTS_Collapse` on death (for later)

`FRTS_Collapse::CollapseMesh` takes over the same `UGeometryCollectionComponent` on death and applies its own
strain field and lifetime. If this component is still ticking and re-issuing forces into a collection that collapse
now owns, the two fight — collapse sets up its impulse, we quiesce it 100ms later.

The future integration must call `StopGeometryDamage()` in `UnitDies` **before** `FRTS_Collapse::CollapseMesh`.
`StopGeometryDamage` clears active impacts, disables tick, and — importantly — **does not quiesce**, because
sleeping the pieces is precisely what collapse does not want.

`StopGeometryDamage` **is in scope now** (it is component-internal); the *call site* in `UnitDies` is not. Document
the requirement as a `@note` on the class so the later integration cannot miss it.

### 11.4 Rollout

**Phase 1 is this task. Phases 2+ are explicitly not.**

1. **← THIS TASK.** `UGeometryDamageComponent` + settings structs. Prove the field graph, the strain breakpoint, and
   the simulation window on one opted-in unit, driven by `Debug_FireTestImpact` (§0.2). No thresholds needed to
   call it done, though they should be implemented. Nothing in the damage pipeline changes.
2. *(Later)* Integration refactor: call `OnDamageTaken` from the damage path, honouring the §5.1.1 contract.
3. *(Later)* Fix the projectile hit data (§11.2); verify `DirectionalPunch` now reads distinctly from `RadialBurst`.
4. *(Later)* Tune threshold FX concurrency in a real fight, not in isolation.
5. *(Later)* Profile with `stat chaos` at scale; tune `MaxSimulationDistanceFromView` and `MaxFieldDispatchesPerTick`.
6. *(Later)* Consider buildings (`BuildingExpansion`, `DestructableEnvActor`), which already own geometry
   collections — and which unlock the anchored-`Dynamic` flex option from §2.6.1 that vehicles cannot use.

---

## 12. Validation plan

Physics feel cannot be unit-tested; this needs eyes. But several failure modes here are *silent*, and those are the
ones worth deliberately checking, because none of them produce an error:

Everything below is driven through `Debug_FireTestImpact` (§0.2), since nothing calls `OnDamageTaken` yet.

| Check | Why it is a real risk |
|---|---|
| **A hit above the strain breakpoint actually breaks a piece loose** | §2.6.1 — if `MaxStrain` is below the asset's Damage Threshold, *nothing happens at all* and it reads as broken code rather than as mistuning. **Verify this first**; nothing else can be judged until it passes |
| **`ObjectType = Kinematic` + `bSimulatePhysics = true` on the test unit** | §2.6 — `Dynamic` collapses the unit under gravity on `BeginPlay`; not simulating swallows every field silently |
| Second hit after the collection sleeps still reacts | §2.2 — the `LinearImpulse`/`LinearForce` wake asymmetry. If someone "optimises" the force channel to impulse later, hits go dead after the first and nothing logs |
| Freshly-broken pieces move on the frame after a strain break | §6.4 — the strain/force ordering hazard; symptom is a dropped first frame |
| Identical feel at 30fps and 144fps (`t.MaxFPS`) | §6.5 — proves the re-issue window actually removed the dt-dependence |
| A repaired unit does not re-fire threshold FX | §7.2 `bRearmOnHeal` |
| Thresholds fire on the correct hit, not one late | §5.1.1 — the post-health call contract |
| A big shell crossing 50% and 25% at once fires both correctly | §7.1 overshoot handling |
| `Init` on a non-simulating collection warns rather than no-ops | §2.1 — `PhysicsProxy` null is completely silent |
| 20+ units under fire: no audio clipping, no field dispatch spike | §8.3 / §8.6 |
| **No existing file was modified** | §0.1 — `git status` should show only new files under `RTSComponents/GeometryDamageComponent/` |

`stat chaos`, `stat game`, and the `Chaos Visual Debugger` are the tools. The distance cull (§8.4) should be
measured, not assumed — it is the load-bearing optimisation and deserves a real before/after number.

---

## 13. Resolved decisions

All questions from the first design pass are **closed**. Recorded here with their consequences so the implementation
has no discretion left on any of them.

### 13.1 Geometry Collection assets — **they exist; opt-in is per-unit, chosen by the designer**

The fractured assets exist as content but are not wired to any C++ backend. Adoption is a **designer decision per
unit**: they add the component and call `InitGeometryDamage` in Blueprint.

*Consequences:* the component must never assume its owner has one — `InitGeometryDamage` returning `bool` (§4) and
the `ObjectType`/simulation validation (§2.6) carry that weight. The §10.2 numbers remain first-look values to
bisect from, per asset. There is no global registry and no auto-discovery; a unit without the component is simply
unaffected.

### 13.2 Substrate — **the collection is visible for the unit's whole life; no death swap**

*Consequence:* the "nothing on screen to push" risk is gone, and the approach is sound. **But the question was
aimed at the wrong axis.** Visibility is not what fields need — *simulation state* is. §2.6 now covers this in full:
the collection must be `bSimulatePhysics = true` with `ObjectType = Chaos_Object_Kinematic`, and §2.6.1 derives the
resulting break-and-push behaviour model, which reshaped §10.2. **§2.6 and §2.6.1 are the most important sections
in this document for the implementer.**

### 13.3 Squads — **not supported**

`USquadHealthComponent` / `SquadUnitHealthComponent` are **out of scope**. The max-health normalisation ambiguity
is therefore moot.

*Consequence:* the component targets `UHealthComponent` only. Do **not** add squad-health handling, and do not
generalise the health source to an interface to accommodate squads — that is speculative work for a case that is
explicitly excluded.

### 13.4 Damage type does not select the archetype — **decided, no**

*Consequence:* no `TMap<ERTSDamageType, FGeometryDamageForceProfile>`, and `ERTSDamageType` is **absent from
`FGeometryDamageHit`** entirely (§4.1) so the field cannot quietly grow a consumer. One `DefaultForceProfile` plus
per-threshold `CrossingBurst` profiles is the whole story.

### 13.5 Networking — **single-player; cosmetic only**

The game is single-player.

*Consequence:* **no replication of any kind.** No `UPROPERTY(Replicated)`, no RPCs, no `GetLifetimeReplicatedProps`,
no `HasAuthority()` guards, no client/server branching. Fields are dispatched locally and determinism across
machines is a non-issue. If any of that appears in the implementation, it is unrequested scope.

---

## 14. Summary of the key decisions

- **Scope: this component and nothing else** (§0). No integration, no `HealthComponent` changes, no `Projectile`
  changes, no replication, no squads. `git status` should show only new files.
- **The collection must be simulating and `Kinematic`** (§2.6), and the consequence is that a living unit's
  reaction is **break-and-push, never wobble** (§2.6.1): force fields are deaf to intact kinematic clusters, so
  **strain is the gate and force is only the shaper**. Sub-threshold hits are FX-only by design.
- **One scalar, `Severity`**, derived from `damage ÷ max health` with a deadzone, a reference clamp, and a
  curve-or-exponent shaper, drives force, radius, strain and FX together so they can never disagree.
- **`Chaos_LinearForce`, not `Chaos_LinearImpulse`**, as the primary channel — because it is the only force type
  that wakes sleeping particles, and our own sleep strategy guarantees they will be asleep by hit #2 (§2.2).
- **Re-issued each tick over a short window**, because `ApplyPhysicsField` is one-shot and a single force command is
  frame-rate dependent (§6.5).
- **Strain first, force second**, because non-`Dynamic` particles are deaf (§6.4).
- **Radius scales with severity, not just magnitude** — this is what makes big hits read as different *events*
  rather than as faster small ones (§5.2).
- **Sleep on window expiry, never disable**, because a disabled piece cannot react to the next hit (§8.2).
- **Reuse `EHealthLevel`** and its existing overshoot handling rather than reinventing thresholds (§7.1).
- **The projectile's missing `ShotDirection`/`ImpactNormal` is deferred, not fixed** (§11.2). The component's
  fallback chain — ending in a forced `RadialBurst` — is what makes v1 correct without touching `Projectile.cpp`,
  and it is in scope.
- **Health is read, never written or hooked** (§5.1.1), through getters that already exist. The contract is
  "`OnDamageTaken` is called after health is applied", with explicit overrides on the hit struct so the component
  is fully testable via `Debug_FireTestImpact` before any integration exists.
