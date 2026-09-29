# Vehicle modules — technical design

**Design only.** Proposed APIs/pseudocode; no C++ files are added or changed. Reviewed against source on 27–29 September 2026. Numeric defaults require playtesting.

**Owner:** `ATankMaster` and its `UArmorCalculation`. Ordinary healing uses the tank's actual `UHealthComponent` health percentage for recovery. CrewRepair instead spends time to recover red modules without changing hull HP.

## 1. Files and responsibilities

| Proposed file / existing class | Responsibility |
| --- | --- |
| `ArmorCalculationComponent/VehicleModules/VehicleModuleTypes.h` | Enums, plain rule structs, reflected setup/state and event payloads |
| `ArmorCalculationComponent/VehicleModules/VehicleModuleBalance.h` | **All system balance constants and constexpr rule tables** |
| `UArmorCalculation` | Fixed module storage; plate → module lookup; damage; state transitions; batch repair mutations |
| `ATankMaster` | Tank health repair gate, crew action, shared finishing work, behaviour bindings and capabilities |
| `UBehaviourComp` / module behaviours | Source-owned damage effects, e.g. engine speed penalty |
| `UHealthComponent` / `UW_HealthBar` | Central healing receipts/completion; tank-only icon changes, `ModuleBox` lookup and widget lifetime |
| `UVehicleModuleSettings` / `UVehicleModuleDataAsset` | Shared damaged/destroyed textures, optional behaviours and image dimensions |
| `UVehicleModuleSubsystem` | One retained module asset and enum-indexed cache per game instance |
| Repair components / auras / healing behaviours | Call normal `UHealthComponent::Heal`; module repair follows automatically |

Both headers live under `RTS_Survival/RTSComponents/`. Types must not include the balance header; balance includes the plain rule types. Define the fixed-array `FPlateModuleRuleSet` in the balance header after its capacity constant, and `FModuleDamageBatch` in `ArmorCalculation.h` after including balance. This avoids a types/settings include cycle.

## 2. One balance file

`VehicleModuleBalance.h` is the only definition site for module-system tuning: probabilities, damage multipliers, HP thresholds, repair settings, performance penalties, source gates, anti-spam limits and UI timing. Use `inline constexpr` values/POD tables. No duplicate values in `DeveloperSettings.h`, behaviours, Blueprint defaults or `.cpp` functions.

Vehicle setup selects a constexpr class profile, running gear, component bindings and add-on coverage. **Module HP is calculated from the tank health component's MaxHealth**, with no manually authored `ModuleHp`. Class-specific HP/chance multipliers live here. Textures, image dimensions and optional behaviour classes live in the shared module asset (section 11); behaviours read numeric tuning from this header.

### Core declarations

```cpp
namespace VehicleModuleBalance
{
    inline constexpr int32 MaxModulesPerPlate = 3;
    inline constexpr int32 AddOnArmorCandidateIndex = 0;
    inline constexpr int32 MaxRunningGearModules = 2;
    inline constexpr int32 MaxEngineModules = 1;
    inline constexpr int32 MaxAmmoModules = 1;
    inline constexpr int32 MaxTurretModules = 4;
    inline constexpr int32 MaxWeaponModules = 1;
    inline constexpr int32 MaxAddOnArmorModules = 8;
    inline constexpr int32 MaxModuleInstances = MaxRunningGearModules + MaxEngineModules
        + MaxAmmoModules + MaxTurretModules + MaxWeaponModules + MaxAddOnArmorModules;
    inline constexpr int32 ArmorPlateRuleCount = 17;
    inline constexpr int32 ModuleTypeCount = 8; // Includes None and Wheels.
    inline constexpr int32 VehicleProfileCount = 5;
    inline constexpr int32 RunningGearTypeCount = 2;

    // The single tank-health gate for restoring red modules to yellow.
    inline constexpr float TankHealthRequiredForModuleRecovery01 = 0.75f;

    // Percentage points above the module type's destruction threshold.
    inline constexpr float RepairedModuleHealthMargin01 = 0.05f;

    namespace DestroyedHealth01
    {
        inline constexpr float AddOnArmor = 0.05f;
        inline constexpr float Tracks = 0.20f;
        inline constexpr float Wheels = 0.20f;
        inline constexpr float Engine = 0.15f;
        inline constexpr float Ammo = 0.10f;
        inline constexpr float Turret = 0.25f;
        inline constexpr float Weapon = 0.20f;
    }

    constexpr float GetDestroyedHealthThreshold01(EVehicleModuleTypes Type);
    constexpr float GetRecoveredHealth01(EVehicleModuleTypes Type);

    inline constexpr float CrewRepairTickSeconds = 1.0f;
    namespace CrewRepairSeconds
    {
        inline constexpr float AddOnArmor = 8.0f;
        inline constexpr float Tracks = 12.0f;
        inline constexpr float Wheels = 10.0f;
        inline constexpr float Engine = 20.0f;
        inline constexpr float Ammo = 18.0f;
        inline constexpr float Turret = 15.0f;
        inline constexpr float Weapon = 12.0f;
    }
    constexpr float GetCrewRepairSeconds(EVehicleModuleTypes Type);
}
```

`GetRecoveredHealth01(Type) = GetDestroyedHealthThreshold01(Type) + RepairedModuleHealthMargin01`.

Compile-time validation: destruction thresholds lie in `[0,1)`; recovered percentages are **strictly above their threshold and below 1**. `None` cannot be installed. Fractions use `01`, calibre uses millimetres, time uses seconds.

### Other definitions in the same header

| Symbol | Initial value / contents |
| --- | --- |
| `BasePlateRules` / `ProfilePlateRules` | Section 4 topology; constexpr class/gear variants, three entries per row |
| `ModuleProfileRules` | Per-class, per-type HP ratios and damage-chance multipliers; section 3 |
| `MaxRegisteredArmorMeshes` / `MaxPlateBindings` | 3 / mesh count × existing `MaxArmorPlatesPerMesh`; fixed routing capacity |
| `ModuleTypeRules` | Yellow/red effects; section 8 |
| `ShellModuleRules` | Source probability/energy factors; section 5 |
| `MineModuleRule` / `SplashModuleRules` | Universal source-specific candidates, multipliers and attenuation; never local constants in the adapters |
| `MinBallisticCalibreMmByModuleType` | Independent constexpr values per module type; initial values all 20 mm, section 5 |
| `PenetratingModuleDamageCap01` / `NonPenModuleDamageCap01` | 0.60 / 0.20 of module MaxHP per impact |
| `MaxNewDestroyedModulesPerImpact` | 1, excluding AddOnArmor; limits state transitions regardless of assigned behaviours |
| `SurvivingModuleThresholdMargin01` | 0.01; floor for a second would-be failure |
| `PenDamageFloorFromBase` / `PenDamageCeilingFromBase` | 0.20 / 1.25 |
| `NonPenWindowSeconds` / `NonPenWindowDamageCap01` | 1.0 / 0.20 of module MaxHP across all attackers |
| `NonPenWindowBucketCount` | 20 fixed buckets; conservative oldest-bucket expiry |
| `VehicleRepairTickSeconds` / `BaseWorkerRepairHpPerSecond` | 0.5 / 7.5, matching the current worker baseline |
| `CrewRepairPriority` | Engine, Tracks, Wheels, Weapon, Turret, Ammo, AddOnArmor; fixed module slot breaks ties |
| `CrewRepairSuppressedAbilities` | `IdAttack`, `IdMove`, `IdReverseMove`, `IdRotateTowards` |
| `CrewRepairAbilitySlotIndex` | Existing `MaxAbilitiesForActionUI - 1`; final command-card slot |
| `CrewRepairCooldownSeconds` | 0 for EnableRepair and DisableRepair; no resource cost |
| `FullModuleServiceWork` | 60 healing-work units; shared finishing pass at full tank health, including surplus from the same heal |
| `CompletedHealth01` / `HealthCompletionTolerance01` | 1.0 / 0.0001 |
| `CrewShockMaxSeconds` / `CrewShockImmunitySeconds` | 4 / 8 |
| `ModuleAnnouncementCooldownSeconds` | 5; announcements only, never delays icon changes |
| `DefaultModuleIconWidth` / `DefaultModuleIconHeight` | 225.0 / 225.0; defaults for the presentation asset's editable dimensions |

`FullModuleServiceWork` controls yellow → Healthy completion, **not** red → yellow eligibility. Ordinary healing has one shared tank-health recovery gate. **CrewRepair is the explicit exception:** timed red → yellow recovery independent of hull health, with no hull healing or full-service credit. Crew durations are positive whole multiples of the one-second interval; validate the constexpr table at compile time. There is no extra crew quiet period or warmup.

Reuse the existing repair technology's multiplier as an input. Vehicle repair paths consume this header rather than maintaining a second vehicle tuning source in `DeveloperSettings.h`.

## 3. Types, setup and fixed storage

```cpp
enum class EVehicleModuleTypes : uint8
{
    None,
    AddOnArmor,
    Tracks,
    Engine,
    Ammo,
    Turret,
    Weapon,
    Wheels
};

enum class EVehicleModuleState : uint8
{
    Healthy,
    Damaged,   // Yellow.
    Destroyed  // Red; recoverable while the tank survives.
};

struct FPlateModuleDamage
{
    EVehicleModuleTypes TypeToDamage;
    float DamageMultiplier;
    float DamageProbability;
    EModuleTargetSelector TargetSelector;
    EModuleNonPenPolicy NonPenPolicy;
};
```

`EModuleTargetSelector`: singleton engine/ammo, struck running-gear side, plate-bound turret/weapon, or covering add-on zone. `EModuleNonPenPolicy`: Never, External, or MantletOnly. Rules contain numeric data, without UObjects or dynamic containers.

| Struct | Fields |
| --- | --- |
| `FVehicleModuleSetup` | `Type`, `Binding`; HP derived on initialization, ID assigned from the fixed module slot, behaviour classes from shared data asset |
| `FVehicleModule` | `Type`, `CurrentHp`, `MaxHp`, `State`, `Revision`, installed flag |
| `FModuleBinding` | Stable mesh/plate registration ID, mount/weapon role, weak component references |
| `FPlateModuleRuleSet` | `Entries[MaxModulesPerPlate]`; unused entries have `TypeToDamage = None` |
| `FModuleDamageBatch` | Three `{ModuleId, Damage}` entries plus count; returned by value |
| `FModuleChangeBatch` | `MaxModuleInstances` copied state-change payloads plus count; fixed storage for damage and full repair |

```text
UArmorCalculation::SetupModule(const FVehicleModuleSetup& Setup, int32& OutModuleId) -> bool
UArmorCalculation::ValidateModuleSetup() const -> bool
UArmorCalculation::GetModuleSnapshot(FVehicleModuleId Id) const -> FModuleSnapshot

M_Modules[VehicleModuleBalance::MaxModuleInstances]
M_Bindings[VehicleModuleBalance::MaxModuleInstances]
M_PlateRoutes[VehicleModuleBalance::MaxPlateBindings][VehicleModuleBalance::MaxModulesPerPlate]

UArmorCalculation::RebuildPlateModuleBindings()
    -> precompute route slots after setup / armor registration / mount change
```

`SetupModule` is Blueprint-callable. The designer supplies no numeric ID; successful setup returns the assigned fixed slot ID through `OutModuleId` for coverage, rebind and query calls. Reject `None`, duplicate side or mount, exhausted slots, the wrong running-gear type and missing required bindings. Repeating setup never heals an existing module. Multiple turrets and add-on zones must be installed in the same order when reconstructing a saved tank so each instance keeps its slot. Finalization requires valid positive tank MaxHealth and profile HP ratios.

Fixed slot budget: two running-gear instances, one engine, one ammo, four turrets, **one Weapon module maximum**, eight add-on zones: **17 module slots**. Derive capacities and type-to-slot ranges constexpr from the constants above. Reject a second Weapon module even if bound to a different gun. This does not increase the existing **three armor meshes × sixteen plates** limit; validate mesh coverage separately.

`FSetupPlateModuleDmg` is unnecessary: C++ defines probabilities and damage multipliers. Blueprint installs modules and configures **only add-on armor coverage** through the separate function below. Do not construct per-vehicle `TMap<ModuleType, TArray<Rule>>` storage.

### Class profiles, tracks/wheels and automatic module HP

```text
EVehicleModuleProfile : uint8 { ArmoredCar, LightTank, MediumTank, HeavyTank, SuperHeavyTank }
EVehicleRunningGear : uint8 { Tracks, Wheels }

UArmorCalculation::SetVehicleModuleProfile(EVehicleModuleProfile Profile) -> bool // BP
UArmorCalculation::SetRunningGearType(EVehicleRunningGear RunningGear) -> bool  // BP
UArmorCalculation::FinalizeVehicleModuleSetup(const UHealthComponent& TankHealth) -> bool
UArmorCalculation::RecalculateModuleMaxHealth(float TankMaxHealth)

FVehicleModuleProfileRule:
    ModuleHealthMultiplier[ModuleTypeCount]
    DamageChanceMultiplier[ModuleTypeCount]
    DefaultRunningGear

constexpr ModuleProfileRules[VehicleProfileCount]
constexpr BuildProfilePlateRules(BasePlateRules, ModuleProfileRules)
    -> ProfilePlateRules[VehicleProfileCount][RunningGearTypeCount][ArmorPlateRuleCount]
```

Default component profile = MediumTank, running gear = Tracks. Selecting ArmoredCar defaults to Wheels; other profiles default to Tracks. `SetRunningGearType(Wheels)` overrides the selected profile's default. Call these before installing modules; reject changes once installation starts. No runtime profile switching or conversion of damaged tracks into fresh wheels.

There is one mutually exclusive running-gear choice for the entire tank. Two left/right instances use **either** `Tracks` **or** `Wheels`; mixed installs fail validation. Both use the same plate selectors and mobility capability, with distinct enum entries, textures, thresholds and tunable profile fields. The constexpr table builder substitutes Wheels for Tracks before applying the chosen type's chance multiplier. It never adds a second running-gear candidate.

Initial profile values below: **module MaxHP / tank MaxHealth ; chance multiplier**. Tracks and Wheels have separate fields initialized to the same values shown in the running-gear column. None has zero HP/chance; all other ratios must be finite and positive.

| Profile | Tracks / Wheels (each side) | Engine | Ammo | Turret | Weapon | AddOnArmor (per zone) |
| --- | --- | --- | --- | --- | --- | --- |
| ArmoredCar | 0.18 ; 1.15 | 0.25 ; 1.20 | 0.22 ; 1.10 | 0.22 ; 1.10 | 0.20 ; 1.10 | 0.15 ; 1.00 |
| LightTank | 0.22 ; 1.05 | 0.30 ; 1.10 | 0.26 ; 1.05 | 0.26 ; 1.05 | 0.23 ; 1.05 | 0.18 ; 1.00 |
| MediumTank | 0.25 ; 1.00 | 0.35 ; 1.00 | 0.30 ; 1.00 | 0.30 ; 1.00 | 0.25 ; 1.00 | 0.20 ; 1.00 |
| HeavyTank | 0.30 ; 0.90 | 0.45 ; 0.75 | 0.36 ; 0.90 | 0.36 ; 0.90 | 0.30 ; 0.95 | 0.25 ; 1.00 |
| SuperHeavyTank | 0.35 ; 0.85 | 0.55 ; 0.60 | 0.42 ; 0.80 | 0.42 ; 0.85 | 0.35 ; 0.90 | 0.30 ; 1.00 |

```text
ModuleMaxHp = TankHealth.GetMaxHealth() * Profile.ModuleHealthMultiplier[ModuleType]
InitialModuleHp = ModuleMaxHp                         // new spawn only
NewModuleHp = NewModuleMaxHp * OldModuleHealth01      // MaxHealth upgrade

CompiledChance = Clamp(BaseRule.DamageProbability
                       * Profile.DamageChanceMultiplier[ResolvedModuleType], 0, 1)
```

For equal 1,000 tank MaxHealth, a medium engine has 350 module HP; a heavy engine has 450 HP. A rear-plate engine chance of 0.65 becomes 0.4875 for HeavyTank. This controls both how often damage occurs and how much punishment the module survives.

`ATankMaster::BeginPlay_SetupData_Resistances` currently calls `InitHealthAndResistance`. Finalize module HP **after** this establishes tank MaxHealth, with setup/bindings ready; do not use the health component's constructor default. Initialization is an explicit readiness step, independent of component BeginPlay ordering. On load, apply saved module health fractions after finalization.

Add `UHealthComponent::OnMaxHealthChanged(OldMaxHealth, NewMaxHealth)` at the end of an actual `SetMaxHealth` change. Tank binds once → `RecalculateModuleMaxHealth`; preserve each module's percentage and state, including exact red-threshold boundaries. Reject invalid tank MaxHealth before division; leave setup disabled or existing modules unchanged and report the error. Current hull damage/healing never resizes modules; accepted healing can trigger section 7's repair milestones. All post-initialization MaxHealth writes must use this setter.

Runtime work is selecting an immutable constexpr table once and multiplying by actual tank MaxHealth on initialization/upgrades. Blueprint-selected vehicle data cannot itself be constexpr. No table construction, profile switch or health-component lookup per impact. Do not infer module profiles from navigation agents or `EResistancePresetType`; those existing classifications serve different purposes.

### Blueprint add-on armor coverage

Modules are fixed data records. Expose this node on their owning `UArmorCalculation` component:

```cpp
/**
 * @brief Allows vehicle-specific armor coverage while retaining shared damage rules.
 * @param ModuleId Installed AddOnArmor module whose complete coverage is replaced.
 * @param CoveredPlates Registered mesh/plate pairs; empty clears this module's coverage.
 * @return True if all bindings were valid and the replacement was committed.
 */
UFUNCTION(BlueprintCallable, Category = "ArmorSettings|VehicleModules")
bool SetAddOnArmorPlateCoverage(
    FVehicleModuleId ModuleId,
    const TArray<FAddOnArmorPlateBinding>& CoveredPlates);
```

`FAddOnArmorPlateBinding` is `USTRUCT(BlueprintType)` with `EditAnywhere, BlueprintReadWrite` fields: `TObjectPtr<UMeshComponent> MeshWithArmor` and `EArmorPlate PlateType`. It is a transient setup input; resolve it into stable registration IDs and keep only weak component references in stored bindings. A mesh/type pair covers all matching registered armor boxes on that mesh, matching the scope of existing `SetArmorOfPlateType`.

```text
Blueprint setup:
    InitArmorCalculation(HullMesh, ...)
    SetupModule(SideSkirtSetup, SideSkirtId)            // Type = AddOnArmor; ID is an output
    SetAddOnArmorPlateCoverage(SideSkirtId,
        [{HullMesh, Plate_SideLeft}, {HullMesh, Plate_SideLowerLeft}])

SetAddOnArmorPlateCoverage(ModuleId, CoveredPlates):
    validate installed AddOnArmor ID and every registered mesh/type pair
    resolve/deduplicate pairs into fixed scratch plate IDs
    reject coverage owned by another AddOnArmor module
    replace this module's coverage atomically; leave old coverage on failure
    RebuildPlateModuleBindings()
    RebindActiveAddOnArmorBehaviour(ModuleId)           // if assigned; no implicit damage effect
    RefreshArmorContributionsAndRearCache()
    return true
```

- New add-on modules cover **no plates** until configured. One zone may cover several plates/meshes; each physical plate has at most one covering zone.
- Repeated calls replace coverage without resetting HP/state. Empty input removes coverage/contribution without uninstalling or healing the module. Calls during a damage batch are rejected; apply configuration between impacts.
- The same coverage identifies damage targets, installed add-on armor and the plates an assigned armour behaviour may modify. Uncovered plates neither damage that add-on nor receive its armor bonus. Module HP/state alone never reduces protection; only a configured behaviour changes its contribution. Hits use pre-impact armor; behaviour changes affect subsequent hits.
- BP arrays exist only during setup; compile into the reserved slot in fixed `M_PlateRoutes`. No per-hit arrays, searches or extra candidate pass. Other modules' routes/probabilities are unchanged.
- Save authored coverage by stable mesh/mount role and plate type. Rebind on load or mesh replacement; unresolved coverage is disabled and reported until registration succeeds.

## 4. Compile-time plate → module mapping

In `VehicleModuleBalance.h`:

```text
constexpr MakeRule(Type, Probability, DamageMultiplier, Selector, NonPenPolicy)
    -> FPlateModuleDamage
constexpr MakeRuleSet(First, Second = NoModuleRule, Third = NoModuleRule)
    -> FPlateModuleRuleSet

inline constexpr FPlateModuleRuleSet BasePlateRules[ArmorPlateRuleCount] = ...
constexpr TryGetPlateRuleIndex(EArmorPlate Plate) -> checked optional index
constexpr ValidatePlateRules() -> bool
static_assert(ValidatePlateRules())
```

Use explicit enum-to-index conversion and validate exhaustive coverage. Every row reserves `AddOnArmorCandidateIndex` for optional add-on coverage, leaving at most two other modules. `ValidatePlateRules()` checks that layout as well as probability/multiplier ranges. Lookup is O(1), followed by at most three entries. Tuple = **module: penetrating probability / damage multiplier**; values live directly in the constexpr table.

For example, the mantlet row is constructed in that header as:

```cpp
MakeRuleSet(
    MakeRule(EVehicleModuleTypes::AddOnArmor, 1.00f, 0.25f,
             EModuleTargetSelector::CoveringArmorZone, EModuleNonPenPolicy::External),
    MakeRule(EVehicleModuleTypes::Weapon, 0.60f, 0.65f,
             EModuleTargetSelector::BoundWeapon, EModuleNonPenPolicy::MantletOnly),
    MakeRule(EVehicleModuleTypes::Turret, 0.30f, 0.45f,
             EModuleTargetSelector::BoundTurret, EModuleNonPenPolicy::Never))
```

| `EArmorPlate` | Entry 0 | Entry 1 | Entry 2 |
| --- | --- | --- | --- |
| `Plate_Front` | AddOnArmor: 1.00 / 0.30 | Tracks: 0.12 / 0.30 | Ammo: 0.08 / 0.30 |
| `Plate_FrontUpperGlacis` | AddOnArmor: 1.00 / 0.35 | Ammo: 0.10 / 0.30 | None |
| `Plate_FrontLowerGlacis` | AddOnArmor: 1.00 / 0.25 | Tracks: 0.35 / 0.55 | Engine: 0.10 / 0.30 |
| `Plate_SideLeft` | AddOnArmor: 1.00 / 0.30 | Tracks: 0.25 / 0.40 | Ammo: 0.30 / 0.50 |
| `Plate_SideRight` | AddOnArmor: 1.00 / 0.30 | Tracks: 0.25 / 0.40 | Ammo: 0.30 / 0.50 |
| `Plate_SideLowerLeft` | AddOnArmor: 1.00 / 0.20 | Tracks: 0.65 / 0.70 | Ammo: 0.15 / 0.30 |
| `Plate_SideLowerRight` | AddOnArmor: 1.00 / 0.20 | Tracks: 0.65 / 0.70 | Ammo: 0.15 / 0.30 |
| `Plate_Rear` | AddOnArmor: 1.00 / 0.25 | Engine: 0.65 / 0.70 | Ammo: 0.20 / 0.40 |
| `Plate_RearLowerGlacis` | AddOnArmor: 1.00 / 0.20 | Engine: 0.60 / 0.65 | Tracks: 0.30 / 0.40 |
| `Plate_RearUpperGlacis` | AddOnArmor: 1.00 / 0.25 | Engine: 0.65 / 0.70 | Ammo: 0.25 / 0.45 |
| `Turret_Front` | AddOnArmor: 1.00 / 0.25 | Turret: 0.30 / 0.45 | Weapon: 0.20 / 0.40 |
| `Turret_SideLeft` | AddOnArmor: 1.00 / 0.25 | Turret: 0.40 / 0.50 | Ammo: 0.25 / 0.45 |
| `Turret_SideRight` | AddOnArmor: 1.00 / 0.25 | Turret: 0.40 / 0.50 | Ammo: 0.25 / 0.45 |
| `Turret_Rear` | AddOnArmor: 1.00 / 0.25 | Turret: 0.35 / 0.50 | Ammo: 0.40 / 0.55 |
| `Turret_SidesAndRear` | AddOnArmor: 1.00 / 0.25 | Turret: 0.40 / 0.50 | Ammo: 0.30 / 0.50 |
| `Turret_Cupola` | AddOnArmor: 1.00 / 0.25 | Turret: 0.15 / 0.25 | Ammo: 0.10 / 0.25 |
| `Turret_Mantlet` | AddOnArmor: 1.00 / 0.25 | Weapon: 0.60 / 0.65 | Turret: 0.30 / 0.45 |

`ResolveModuleId(Rule, HitContext)` resolves one installed instance per entry:

- Tracks → struck side; front/rear hits use hull-local lateral position with a stable centerline tie-break.
- Turret → plate-bound mount. Weapon → the sole installed Weapon module, only for its bound gun's plates; secondary guns do not acquire damage bindings automatically.
- AddOnArmor → zone assigned by `SetAddOnArmorPlateCoverage`; an unconfigured slot is skipped. Engine/Ammo → singleton.
- Absent instance → skip, without reallocating its probability/damage.

Each fixed route contains a primary module slot and, for side-dependent running gear, an alternate slot. Resolve directly from stable plate registration and candidate index; no per-hit module search. Validate registration generations after mesh replacement. Sorting armor boxes cannot change their stable routing slots.

Casemate gun shields may use `Turret_Mantlet` with a Weapon binding and no Turret instance. This version models at most one damaged weapon per tank; behaviour bindings specify which gun is affected. Plate-to-module topology stays shared; constexpr class profiles change probabilities and substitute the selected running gear. Only add-on coverage enables/disables its reserved candidate per vehicle. It never becomes a fourth candidate or replaces another module. Validate every generated profile/gear table at compile time.

## 5. `CalculateModuleDamage` and damage application

### Required public entry point

```cpp
void CalculateModuleDamage(
    EArmorPlate PlateHit,
    float EffectiveArmor,
    bool bPen,
    float DamageDealt,
    float ProjectileBaseDamage,
    float ProjectileCalibre);
```

Keep these six values explicit. `ProjectileCalibre` is millimetres; `DamageDealt` is actual applied tank-health damage. The function does **not** recalculate penetration.

The six-argument overload supports unambiguous single-instance bindings and the default kinetic source profile. Production weapon paths use an overload with a final `const FVehicleModuleHitContext& HitContext` for source-specific rules and multiple instances:

```text
FVehicleModuleHitContext:
    StablePlateRegistrationId, HullLocalHitPosition, BoundMountId
    ShellType, DamageType, DeliveryType, bOverpenetrating
    ShotActivationId, ImpactOrdinal, VictimId, RuleVersion

CalculateModuleDamage(PlateHit, EffectiveArmor, bPen, DamageDealt,
                      ProjectileBaseDamage, ProjectileCalibre, HitContext)
```

Do not cache “last hit context” on the component: re-entrant impacts can overwrite it. The six-argument overload rejects ambiguous bindings. Shield absorption, invalid plate results and rejected damage never call either overload. Legacy/default-source callers must supply an explicit context whenever the omitted information would change the result.

`TryMakeDefaultModuleHitContext(PlateHit)` uses a prevalidated unambiguous binding, a tank-local impact serial and stable victim ID. Reject routes requiring unavailable side/mount information; never fabricate a hit position. Serialize the serial if this path is used across saves. Actual weapons supply shot/impact identity through the production overload.

### Resolver pseudocode

```text
CalculateModuleDamage(...):
    Changes = ResolveModuleDamage(...)                 // fixed return value
    DispatchModuleChangesAfterResolver(Changes)         // behaviours/BP/UI may allocate

ResolveModuleDamage(...) -> FModuleChangeBatch:
    ValidateImpactInputsAndLivingOwner(...) or return
    RuleIndex = TryGetPlateRuleIndex(PlateHit) or return
    Rules = M_SelectedProfilePlateRules[RuleIndex]      // const constexpr-backed reference
    Batch = MakeEmptyModuleDamageBatch()                // fixed storage
    Input = MakeModuleDamageInput(the six arguments, HitContext) // stack value
    Energy = CalculateModuleDamageEnergy(Input)

    for CandidateIndex in [0, MaxModulesPerPlate):
        TryAppendCandidateDamage(Rules.Entries[CandidateIndex],
                                 CandidateIndex, Energy, Input, Batch)

    LimitBatchToEnergyBudget(Energy, Batch)
    ApplyPerModuleDamageCaps(bPen, Batch)
    ApplyNonPenRateBudgetIfNeeded(Input, Batch)
    LimitNewDestroyedModules(Batch)
    return CommitModuleDamageBatch(Batch)

TryAppendCandidateDamage(...):
    ResolveModuleId(...) or skip
    GetIsCandidateEligibleForSource(...) or skip
    Probability = CalculateCandidateProbability(...)
    Roll01 = GetDeterministicModuleRoll(ShotKey, CandidateIndex)
    if Roll01 < Probability:
        Batch.Append(ModuleId, Energy * Rule.DamageMultiplier)

CommitModuleDamageBatch(Batch):
    DamageModule(Id, Damage, Cause) for each accepted entry
    return CommitModuleCountsAndChangeBatch()           // fixed records only
```

`DamageModule(FVehicleModuleId, float Damage, EModuleChangeCause)` is the mutation path. A type-only overload is valid only when exactly one instance exists. Module damage never calls tank `TakeDamage` or subtracts hull health again.

### Formula and source rules

```text
PenEnergy = min(Base * PenDamageCeilingFromBase,
                max(DamageDealt, Base * PenDamageFloorFromBase))
            * ShellRule.PenEnergyMultiplier
NonPenEnergy = Base * ShellRule.NonPenEnergyMultiplier
Probability = Rule.DamageProbability
              * (bPen ? 1 : ShellRule.NonPenProbabilityMultiplier)
ProposedDamage = Energy * Rule.DamageMultiplier
BudgetScale = min(1, Energy / SumOfSuccessfulProposedDamage)  // zero-safe
```

`Rule.DamageProbability` already includes the class/type chance multiplier; do not multiply it again. Clamp final probability to `[0,1]`. Mine/splash adapters use their source candidates with the same profile/type factor once. The penetration floor applies only to accepted hits; explicit immunity/friendly-fire rejection blocks modules too. `EffectiveArmor` remains a validated hit/diagnostic value; do not divide damage HP by armor millimetres or override the supplied penetration result.

| `ShellModuleRules` row | Pen energy multiplier | Non-pen probability multiplier | Non-pen energy multiplier |
| --- | ---: | ---: | ---: |
| AP / APCR | 1.00 | 0.35 | 0.15 |
| APHE / APHEBC | 1.15 | 0.35 | 0.15 |
| HE | 1.10 | 0.70 | 0.35 |
| HEAT | 1.00 | 0.70 | 0.35 |
| Railgun | 1.00; 0.70 for overpenetration | 0.35 | 0.15 |

Non-penetration can damage Tracks/Wheels, AddOnArmor and mantlet-bound Weapon only. Engine, Ammo and traverse require penetration.

### Calibre gate per module type

`VehicleModuleBalance::MinBallisticCalibreMmByModuleType[ModuleTypeCount]` holds separate constexpr values for AddOnArmor, Tracks, Wheels, Engine, Ammo, Turret and Weapon. Initial values are **20.0 mm for each**; None is never eligible. Check this inside candidate evaluation, not as a global early return:

```text
GetIsCandidateEligibleForSource(Rule, Input):
    if Input is a ballistic impact and
       Input.ProjectileCalibre < MinBallisticCalibreMmByModuleType[Rule.TypeToDamage]:
        return false
    return existing penetration/non-penetration and source eligibility
```

Exactly 20 mm is eligible with the defaults; smaller ballistic rounds cannot damage any module. This does not alter their hull damage. Lowering only Wheels to 7.62 mm would allow those rounds to roll for Wheels while other modules retain their own cutoffs. Passing the cutoff still requires the correct plate, installed module, permitted hit type and successful chance roll. The gate applies to penetrating and non-penetrating ballistic hits; mine/splash adapters use their own source rules. Validate finite, nonnegative cutoffs at compile time. Calibre is never inferred from base damage.

`LimitNewDestroyedModules` compares proposed HP with **each type's destruction threshold**, not zero. Allow the greatest normalized loss of remaining HP above that threshold to become red; stable table order breaks ties. Clamp additional would-be failures to `min(OldHP, MaxHP × (DestroyedThreshold + SurvivingModuleThresholdMargin01))`, so the cap cannot heal. AddOnArmor destruction does not consume this allowance. Count state transitions, not actual stat effects; never inspect behaviour assets in this calculation. Discard prevented damage.

Validate `1 - PenetratingModuleDamageCap01 > MaxDestroyedThreshold01` to prevent one-shot red failures from full module health. RNG keys reserve one position per candidate; skipped candidates never shift other rolls. Unsupported source profiles fail closed.

## 6. Exact yellow/red transitions

```text
GetModuleState(Type, CurrentHp, MaxHp):
    Health01 = Clamp(CurrentHp / MaxHp, 0, 1)
    if Health01 <= GetDestroyedHealthThreshold01(Type): return Destroyed
    if Health01 < CompletedHealth01:                    return Damaged
    return Healthy

DamageModule(Id, Damage, Cause):
    CurrentHp = Clamp(CurrentHp - ValidatedDamage, 0, MaxHp)
    UpdateModuleState(Id, Cause)

RestoreDestroyedModulesToDamaged():
    for each installed module with State == Destroyed:
        CurrentHp = MaxHp * GetRecoveredHealth01(Type)
        UpdateModuleState(Id, Repair)
    CommitModuleCountsAndChangeBatch()
    PublishCoalescedModuleChanges()

RestoreAllModulesToHealthy():
    set installed module HP to MaxHp
    update states and publish once
```

Example: engine MaxHP = 200, red threshold = 15%. `HP <= 30` is red. Permitted recovery sets HP to `200 × (0.15 + 0.05) = 40`, producing yellow. A track recovers to 25%, because its threshold is 20%. **There is no universal 35% restored module HP.**

Full repair means tank health and every installed module are at maximum. Tank MaxHealth upgrades recalculate module MaxHP while preserving each module's health percentage. Accepted healing restores modules only at the recovery/completion milestones in section 7, without per-tick module HP increments. Recovery applies the ordinary yellow behaviour.

## 7. Ordinary healing and the CrewRepair exception

### One entry point for every healing source

```text
Scavenger RepairTick / TickingHealBehaviour / SingleHealBehaviour / future healing
    -> UHealthComponent::Heal(HealAmount)
    -> ApplyHealingInternal(HealAmount)
    -> ATankMaster::OnHealthHealingApplied(Receipt)
    -> UArmorCalculation repair milestone, only when due

UHealthComponent::Heal(float HealAmount) -> bool
UHealthComponent::ApplyHealingInternal(float HealAmount) -> bool
UHealthComponent::GetHasDamageToRepair() const -> bool
UHealthComponent::InitializeTankRepairOwner(ATankMaster* Tank)

FHealthHealingReceipt:
    AcceptedHealingWork
    AppliedHullHealing
    HealthAfter
    MaxHealth
```

**Every positive accepted `Heal()` call can repair modules**, regardless of who called it. Commander auras, one-shot heals, passive regeneration and new healing abilities need no module-specific payload, provider registration or repair-source enum. CrewRepair is a separate module-only action and never calls `Heal`. Source rules such as range, ownership and ability cost remain with the source.

The health component caches a weak tank owner once after tank/module initialization. It sends a stack receipt to that tank only while module repair is pending; no owner casts, component searches, delegates to every tank or module iteration per healing pulse. It calls the tank before returning completion to the healer. Non-tank healing retains its existing path.

### Central healing transaction and completion

```text
UHealthComponent::ApplyHealingInternal(Amount):
    reject dead owner, nonfinite/nonpositive Amount or invalid MaxHealth
    AcceptedWork = Amount                         // after shared healing modifiers
    HullWork = min(AcceptedWork, max(0, MaxHealth - CurrentHealth))
    CurrentHealth += HullWork
    Receipt = {AcceptedWork, HullWork, CurrentHealth, MaxHealth}
    if tank has pending module repairs:
        Tank.OnHealthHealingApplied(Receipt)       // even when HullWork == 0
    update hull UI only if hull HP changed
    return living owner and not GetHasDamageToRepair()

UHealthComponent::Heal(Amount):
    return ApplyHealingInternal(Amount)

UHealthComponent::GetHasDamageToRepair():
    return owner is alive and
           (CurrentHealth < MaxHealth or cached tank NonHealthyModuleCount > 0)
```

For tanks, `Heal` returning true means **hull and modules are fully repaired**. This deliberately broadens its current hull-only completion contract: `URepairComponent::RepairTick` already uses that bool to stop repairs. Update its API comment and audit callers/overrides; squad-specific healing stays unchanged. A full-health tank with damaged modules must not early-return before producing the receipt.

Clamp tank hull healing to MaxHealth instead of using the current 99% snap, so work cannot be counted both as hull healing and finishing service. `AcceptedHealingWork` is the accepted request, including unused healing at full hull HP; immunity/rejected/zero/negative healing produces no receipt. Apply healing modifiers once. Direct initialization/load setters (`SetCurrentHealth`, `SetMaxHealth`) are not healing operations and emit no receipt; future ordinary healing uses `Heal`. The crew action has no health-cap overload or healing receipt.

### Tank-owned gate and finishing work

```text
ATankMaster::OnHealthHealingApplied(const FHealthHealingReceipt& Receipt)
ATankMaster::TryRecoverDestroyedModulesAfterHealing() -> bool
ATankMaster::AccumulateFullModuleService(float Work) -> bool // completion reached; no armor mutation
ATankMaster::GetIsVehicleFullyRepaired() const -> bool
ATankMaster::OnModuleConditionChanged(const FModuleStateChange& Change)

FVehicleModuleRepairState:
    DestroyedModuleCount
    NonHealthyModuleCount
    ModuleDamageRevision
    FullServiceAccumulatedWork

OnHealthHealingApplied(Receipt):
    reject dead/uninitialized tank or nonpositive accepted work
    if NonHealthyModuleCount == 0: return
    SurplusWork = max(0, Receipt.AcceptedHealingWork - Receipt.AppliedHullHealing)
    if HealthComponent is full and AccumulateFullModuleService(SurplusWork):
        clear finishing work
        ArmorCalculation.RestoreAllModulesToHealthy()   // one final-state batch
        return
    TryRecoverDestroyedModulesAfterHealing()            // recovery if full service was not reached

TryRecoverDestroyedModulesAfterHealing():
    if DestroyedModuleCount == 0: return false
    if HealthComponent.GetHealthPercentage()
       < TankHealthRequiredForModuleRecovery01: return false
    ArmorCalculation.RestoreDestroyedModulesToDamaged() // one batch call
    return true

AccumulateFullModuleService(Work):
    if Work <= 0 or NonHealthyModuleCount == 0: return false
    RemainingWork = max(0, FullModuleServiceWork - FullServiceAccumulatedWork)
    FullServiceAccumulatedWork += min(Work, RemainingWork)
    return FullServiceAccumulatedWork >= FullModuleServiceWork
```

Cache counts from native module events and mirror the pending/nonhealthy count into the health component before mutations return. Reset finishing progress on new module damage; ordinary pauses preserve it. Rebuild counts once on setup/load. The armor component receives only recovery/completion milestones; healers never call it.

Damaged modules require extra healing work beyond the hull repair. One work unit equals one accepted HP of healing. **Surplus from the same healing pulse counts immediately**, including a pulse that first makes red modules eligible for recovery. Reaching full tank health plus the remaining finishing work restores every damaged/destroyed module to Healthy immediately. There is no mandatory second pulse, recovery surcharge or minimum finishing duration.

Finishing work is not rate-limited: a strong one-shot heal must deliver its full accepted healing power immediately. Only cap accumulation to the remaining requirement; discard surplus once hull and modules are all healthy. Concurrent healing sources contribute to the same counter, with no timer or refill budget.

Example: a 1,000-MaxHP tank at 700 HP has red/yellow modules and zero finishing progress. `Heal(360)` spends 300 on hull HP and 60 on finishing; **all module icons disappear in that heal's update**. `Heal(300)` fills the hull and recovers reds to yellow but supplies no finishing work; a further 60 completes it. At full hull health, `Heal(60)` immediately clears red or yellow modules and their icons.

Choose the final repair milestone before mutating modules. Full service commits Destroyed/Damaged → Healthy once, removes existing module behaviours and clears type icons, without briefly creating yellow behaviours/icons. The BP state event reports one transition to Healthy with MaxHp for each changed module. When less work is available, ordinary red → yellow recovery applies. Hidden healthbars cache the final Healthy state; they show no stale icon when revealed. Behaviour iteration may defer effect/UI dispatch to its safe boundary, but never to another healing pulse or repair timer.

### Gate edge cases

| Situation | Required behavior |
| --- | --- |
| Any heal raises tank health to at/above 75% | Recover reds; if it also fills the hull and completes finishing work, restore all directly to Healthy |
| Module becomes red while tank has 90% HP | Next positive accepted heal restores it; no new upward crossing required |
| Tank has 100% HP and red modules | Positive healing counts entirely as finishing work; restore Healthy if sufficient, otherwise recover red → yellow |
| Tank has 100% HP and yellow modules | Positive healing completes or advances finishing work immediately |
| Further pulses after all modules recover | No armor calls; cached count is zero |
| New red failure during healing | State event re-arms pending recovery |
| Zero/rejected heal, MaxHealth upgrade or load | No recovery or finishing credit |
| Death/re-entrant healing during callbacks | No resurrection or duplicate milestone |

Re-entrant healing is deferred until the current health/module transaction and its notifications finish. Each accepted transaction produces one receipt; do not also trigger repair from health-percent delegates or `UpdateHealthBar`. Distinct heal calls are distinct work contributions; an ability must not submit the same heal through two APIs.

### CrewRepair: timed module-only recovery

CrewRepair **never changes tank health**, checks no hull-health threshold and grants no full-service work. It restores one red module at a time to `MaxHp * GetRecoveredHealth01(Type)`. Yellow/healthy modules are skipped. A tank at 10% hull HP remains at 10% after crew repair unless another source heals it.

```cpp
// Append IdCrewRepair to EAbilityID; preserve existing enum values.
UENUM(BlueprintType)
enum class ECrewRepairAbilityType : uint8
{
    EnableRepair,
    DisableRepair
};
```

Ability display name = **CrewRepair**; C++ ID = `EAbilityID::IdCrewRepair`, following existing ID naming. Store subtype in `FUnitAbilityEntry::CustomType` and `FQueueCommand::CustomType`. Both subtypes are free and have zero cooldown. EnableRepair starts only on player command; automatic card insertion does not start the timer.

| Module type | Red → yellow crew time |
| --- | ---: |
| Engine | 20 seconds |
| Tracks | 12 seconds per installed side |
| Wheels | 10 seconds per installed side |
| Weapon | 12 seconds |
| Turret | 15 seconds per installed turret |
| Ammo | 18 seconds |
| AddOnArmor | 8 seconds per installed zone |

All times come from the individual constexpr constants in section 2. Repairs are **sequential**, so total time is the sum for red instances: engine + two tracks = **44 seconds**. Use `CrewRepairPriority` for the next module, then fixed slot order. Keep the current target until it is repaired, removed or repaired externally; newly red modules join the remaining work without resetting that target.

### Ability registration and final command-card slot

```text
ATankMaster::InitializeCrewRepairAbilitySlot()
ATankMaster::RefreshCrewRepairAbilityFromModuleState()
ATankMaster::MakeCrewRepairAbilityEntry(ECrewRepairAbilityType Subtype) const

DestroyedModuleCount > 0, inactive -> final slot = CrewRepair / EnableRepair
DestroyedModuleCount > 0, active   -> same slot = CrewRepair / DisableRepair
DestroyedModuleCount == 0          -> stop if active; clear CrewRepair slot
```

Initialize after `InitAbilityArray`; synchronize again after module setup/load. Reserve `CrewRepairAbilitySlotIndex = MaxAbilitiesForActionUI - 1` at tank ability initialization. The current maximum is 15, so this is index 14. **The final slot is guaranteed unused by tank loadouts.** Size the tank's array once to that capacity; initially the reserved entry is `IdNoAbility`. No ability relocation or full-card fallback is needed. Generic additions skip this reserved slot.

**Codebase constraint:** `UCommandData::AddAbility` neither appends nor grows the array. After setup sizing, write CrewRepair only to the reserved final slot. Removal clears the entry to `FUnitAbilityEntry()`, preserving indices. Swap EnableRepair ↔ DisableRepair in place with the existing exact-subtype `SwapAbility` semantics. Preserve this reservation when abilities are rebuilt at runtime.

Call `RefreshCrewRepairAbilityFromModuleState` after committed native state batches, only when red count crosses zero or active state changes. It returns without writing when the desired entry already matches. Additional red modules do not add duplicates or reset DisableRepair to EnableRepair. Update the command card once after each mutation batch through `UCommandData::UpdateActionUI`.

### Controller → ICommands → TankMaster implementation

Follow [AGENTS.md](../AGENTS.md), [Player/AGENTS.md](../RTS_Survival/Player/AGENTS.md) and [Docs/Abilities.md](Abilities.md). UI callbacks must route through the controller.

| File / symbol | Required addition |
| --- | --- |
| `Player/Abilities.h` | Append `IdCrewRepair`; add `Global_GetAbilityIDAsString` case |
| New `CrewRepairAbilityTypes.h` beside vehicle module types | `ECrewRepairAbilityType` reflected enum |
| `FQueueCommand` in `Interfaces/Commands.h` | `GetCrewRepairAbilitySubtype()`; validate CustomType is EnableRepair/DisableRepair |
| `ACPPController::ActivateActionButton` | Dispatch to `DirectActionButtonCrewRepair(Subtype)`; iterate eligible selected tanks and report activation success |
| `ICommands::CrewRepair(Subtype, bSetUnitToIdle)` | Validate exact card subtype; enqueue EnableRepair through `AddAbilityToTCommands` |
| `UCommandData::ExecuteCommand` | Dispatch `IdCrewRepair` to `ExecuteCrewRepairCommand(Subtype)` |
| Subtype validators | Add ID to `GetDoesQueuedCommandRequireSubtypeEntry`, `GetAbilityEntryForQueuedCommandSubtype` and exact-entry checks |
| `ICommands::TerminateCommand` | Route current queued subtype to `TerminateCrewRepairCommand(Subtype)` |
| `ATankMaster` | Override execute/terminate; own timer, repair progress, stop/restore and card lifecycle |
| Ability UI data | Author EnableRepair/DisableRepair labels/icons/tooltips using `(AbilityId, CustomType)`; no targeting cursor or second click |

This is action-button-only: no new right-click `ECommandType` or target decoder branch. Require the exact card entry; do not add CrewRepair to `IsAbilityRequiredOnCommandCard` exceptions. Unsupported owners reject safely.

**Queue policy:** EnableRepair follows normal replacement/shift-queue semantics. A queued Enable is revalidated when dispatched and skipped if no red modules remain. Once started, its asynchronous command stays active until cancelled/completed. Swapping the card to DisableRepair must not invalidate the already-running Enable command.

DisableRepair is an **immediate control action** through the same controller/`ICommands` API: validate the active tank and DisableRepair entry, then call its execute override directly, without enqueueing or first clearing the queue. This works even with Shift held or while the queue is otherwise busy. Never queue Disable behind the long-running Enable. Reject duplicate Enable commands while active; stale Disable after automatic completion is a harmless no-op.

### Execute, timer and cleanup functions

```text
ATankMaster::ExecuteCrewRepairCommand(ECrewRepairAbilityType Subtype)
ATankMaster::TerminateCrewRepairCommand(ECrewRepairAbilityType Subtype)
ATankMaster::BeginCrewRepair() -> bool
ATankMaster::CrewRepairTick()
ATankMaster::SelectNextRedModuleForCrewRepair() -> bool
ATankMaster::FinishCrewRepair(ECrewRepairStopReason Reason)
ATankMaster::RestoreCrewRepairState()
UArmorCalculation::RestoreDestroyedModuleToDamaged(FVehicleModuleId Id) -> bool

FCrewRepairState:
    Status                                    // Inactive / Repairing / Stopping
    CurrentModuleId, ModuleWorkStartGameTime, RequiredModuleSeconds
    TimerHandle, SessionGeneration, ActiveCommandToken
    AbilitySuppressionHandle, WeaponAndMovementLockHandle

ExecuteCrewRepairCommand(Subtype):
    if Subtype == DisableRepair: FinishCrewRepair(PlayerDisabled); return
    if not BeginCrewRepair():
        complete this queued Enable command once after rollback

BeginCrewRepair():
    validate living tank, red count > 0, required components and final card slot
    if already active: return false
    begin action-state transaction
    acquire crew movement/weapon locks
    SetTurretsDisabled()
    StopVehicleForCrewRepair()
    suppress Attack, Move, ReverseMove, RotateTowards entries
    select next red module; record start game time and GetCrewRepairSeconds(Type)
    set Status = Repairing; swap card subtype to DisableRepair
    start UObject-bound repeating timer: interval = 1 second, first delay = 1 second
    commit action/card refresh once
    // Any start failure rolls back locks, abilities, card and timer.

CrewRepairTick():
    validate living owner and current session; ignore stale timer callbacks
    if not Repairing: return
    if DestroyedModuleCount == 0: FinishCrewRepair(AllModulesRecovered); return
    if current target is no longer red: select next target; record a fresh start time
    if GameTimeNow - ModuleWorkStartGameTime < RequiredModuleSeconds: return
    ArmorCalculation.RestoreDestroyedModuleToDamaged(CurrentModuleId)
    // Committed state event advances the target or auto-finishes. Return immediately.

RestoreDestroyedModuleToDamaged(Id):
    if Id is invalid or module is not Destroyed: return false
    set HP = MaxHp * GetRecoveredHealth01(Type)
    commit native state/counts; dispatch ordinary behaviour/icon/BP changes
    return true
```

One timer per active tank; no timer per module and no heap-allocated repair queue. Selection uses the fixed module array and constexpr priority. Compare elapsed game time on each one-second tick; pause freezes progress, and a target selected between ticks never completes early. Select failure with no red modules completes normally; invalid bindings/owner abort through cleanup. Unfinished time resets on manual disable/re-enable; completed yellow repairs remain. Damage does not add a quiet period or restart ongoing red-module work. A repaired module destroyed again later requires its full duration.

`StopVehicleForCrewRepair` must stop actual movement/rotation: use `EndTurretRangeMovement`, `StopBehaviourTree`, `AITankController::StopMovement`, `StopRotating`, reset pending final rotation and clear/brake movement requests for the tracked/wheeled adapter. Prevent stale movement-completion callbacks from completing the CrewRepair command. Existing `ExecuteStopCommand()` only calls `SetTurretsToAutoEngage(false)`; it is insufficient.

Existing `SetTurretsDisabled()` disables all turrets **and hull weapons**, which is appropriate during crew repair. Hold the action lock so other commands, auto-engage, patrol/attack-ground exceptions and behaviour refresh cannot restart movement/fire. These restrictions come from the explicitly activated repair action; red module state alone still imposes no effect. Other action requests that need movement/fire must first cancel repair or remain queued. Stop/cancellation uses the same cleanup.

### Restore abilities and weapons without stale state

Remove the four named abilities from effective `M_Abilities` while active, preserving their full entries, original indices, CustomType, costs and live cooldowns. Restore only entries that existed and remain granted. Do not rebuild defaults, resurrect revoked abilities, overwrite newly granted entries or restore unrelated old commands.

Use new `UCommandData::BeginAbilitySuppression(Source, AbilityIds)` / `EndAbilitySuppression(Handle)` helpers with fixed per-card-slot backing entries and suppression ownership. The effective card contains `IdNoAbility` while suppressed; normal add/remove/swap operations update the backing entries. `HasAbilityOnCooldown` and `AbilityCoolDownTick` include backing entries so hidden cooldowns continue in the existing scheduler. Releasing CrewRepair reveals an entry only when no other suppression source holds it. Route overlapping `UBehWeaponRemoveAbilities` removals/restorations through these helpers as well; its current cached-entry replay would otherwise conflict with crew cancellation or module recovery. The small card backing storage is prepared during setup; no per-second card rebuilding.

```text
FinishCrewRepair(Reason):
    if Inactive or Stopping: return
    set Status = Stopping; invalidate session; clear timer
    RestoreCrewRepairState()                       // release only crew-owned suppression/locks
    clear unfinished target/time; set Status = Inactive
    if living and red count > 0: final slot = EnableRepair
    else: clear CrewRepair entry
    refresh command card once
    if this session still owns the active queued Enable command
       and Reason is not external queue termination/death:
        DoneExecutingCommand(IdCrewRepair)         // exactly once, after cleanup
```

`RestoreCrewRepairState` reenables turrets/weapons through current behaviour-composed eligibility; call normal auto-engage only where no other active restriction remains. Never blindly enable a weapon still disabled by a module behaviour or stun. Newly spawned/swapped turrets inherit an active crew lock and release it on cleanup. Resume the remaining command queue after completion; immediate Enable intentionally cancelled the prior command through normal command semantics.

`TerminateCrewRepairCommand` calls the same idempotent cleanup but **does not** call `DoneExecutingCommand` again; its caller already owns queue termination/progression. Both the tank death handler and EndPlay invoke cleanup with the destruction reason, clearing timer, entries, handles and callbacks without enabling weapons or advancing commands.

### Automatic stop when another healer recovers modules

The native module-state batch, not the timer or UI, detects completion:

```text
ATankMaster::OnModuleStateBatchCommitted():
    refresh cached red/nonhealthy counts
    if crew is active and DestroyedModuleCount == 0:
        FinishCrewRepair(AllModulesRecovered)
    else if crew is active and its target is no longer red:
        discard its unfinished time; select next red target
    RefreshCrewRepairAbilityFromModuleState()
```

Run this once after the whole batch and its behaviour changes are committed; defer cleanup until the mutation loop ends. Ordinary heals keep their usual hull threshold/full-service rules. If an aura or scavenger restores every red module first, clear the crew timer, restore removed abilities, release weapon/movement locks and remove CrewRepair from the card immediately at that batch boundary. **Undo the crew action's restrictions, never the other source's healing or completed module repairs.**

Partial external recovery skips repaired targets and continues on remaining red modules. No red modules means remove the ability even if some modules are already Healthy rather than Yellow. Later red damage adds a fresh EnableRepair entry in the final slot, without auto-starting. A simultaneous timer/external repair cannot restore twice: the armor function checks current state, and session generation prevents stale callbacks.

On load, rebuild ordinary ability grants and module state, clear transient crew suppression/locks, and expose EnableRepair if reds remain. Do not deserialize a live timer or resume crew work automatically. This avoids restoring a card with DisableRepair but no active repair session.

### Existing healers and future callers

| Existing integration | Required change |
| --- | --- |
| `UTickingHealBehaviour::OnTick` | Keep calling `HealthComponent->Heal(Amount)`; gains module repair automatically |
| `USingleHealBehaviour::OnAdded` | Same; no tank cast or module branch |
| `URepairComponent::RepairTick` | Keep existing `Heal` call; stop when its revised full-repair bool is true |
| `FRTSRepairHelpers::GetIsUnitValidForRepairs` | For tanks, use `GetHasDamageToRepair()` instead of the current 99%-hull check |
| `URadiusRepairAura::IsValidTarget` | Existing helper now includes full-health module-only targets; keep its heal behaviours |
| Controller/right-click/completion checks | Use the same health-component eligibility/completion contract |

Keep unrelated unit eligibility restrictions intact. Future healing abilities use `Heal` and, if filtering targets, `GetHasDamageToRepair`; no knowledge of module thresholds, repair stages, classes or the armor component is required. Healing streams sum naturally: four 7.5 HP/s workers supply 30 finishing work/s, and an equivalent aura supplies the same work.

## 8. Behaviour hooks and effects

```text
UArmorCalculation::OnModuleConditionChanged(Change) -> ATankMaster
ATankMaster::SyncModuleBehaviour(const FModuleStateChange& Change)

Healthy   -> remove module-owned behaviour
Damaged   -> Asset.ModulesByType[Type].DamagedBehaviourClass
Destroyed -> Asset.ModulesByType[Type].DestroyedBehaviourClass

UBehaviourComp::SetModuleBehaviour(ModuleId, DesiredClass, Context)
UBehaviourComp::RemoveModuleBehaviour(ModuleId)
UVehicleModuleBehaviour::OnModuleContextUpdated(Context)
```

Classes are optional `TSubclassOf<UVehicleModuleBehaviour>` values in the shared asset (section 11). `UVehicleModuleBehaviour : UBehaviour` supplies module context before `OnAdded`: instance ID, type, state, current/max HP and weak bindings. **Null means no behaviour for that state; no error, fallback behaviour or null `AddBehaviour` call.** Healthy always requests null.

### Cached instance per module

```text
UBehaviourComp::M_ModuleBehaviourSlots[MaxModuleInstances]
    -> reflected slots: owned behaviour pointer, desired class/context, generation

SetModuleBehaviour(ModuleId, DesiredClass, Context):
    validate installed module ID; obtain its fixed slot directly
    if processing behaviours/callbacks: queue latest desired slot state; return
    if existing instance has DesiredClass:
        update context only; no remove/add or allocation
        return
    RemoveModuleBehaviour(ModuleId)                 // exact instance, if present
    if DesiredClass is null: return                 // valid empty assignment
    Instance = CreateBehaviourInstance(DesiredClass)
    set module source identity and context before OnAdded
    store GC-visible pointer; AddInitialisedBehaviour(Instance)
```

The new module API uses `SourceKey = (VehicleModule, ModuleId)` and O(1) fixed-slot lookup. `RemoveModuleBehaviour` calls existing `RemoveBehaviourInstance` for that pointer, then clears the slot; unrelated same-class instances remain active. Keep the slot synchronized with generic removal/cleanup. The component owns instances through `UPROPERTY` references; the tank caches a weak behaviour-component reference once.

Existing `AddBehaviour`, `RemoveBehaviour`, `SwapBehaviour` and `TryHandleExistingBehaviour` use class/stack matching. Module instances must bypass that cross-source matching; generic matching must likewise exclude module-owned instances. Each module gets at most one active behaviour, even when several modules select the same class. Default module behaviours are persistent and non-ticking; custom behaviours can use existing tick support when their effect requires it. State transitions own attachment/removal; no polling or repeated additions are required.

| Transition | Operation |
| --- | --- |
| Healthy → yellow/red | Add the configured class, if any |
| Yellow → red | Remove yellow instance; add red class if assigned |
| Red → yellow | Remove red instance; add yellow class if assigned |
| Yellow/red → Healthy | Remove this module's instance |
| Same class configured for yellow/red | Keep instance; call `OnModuleContextUpdated` with the new state |
| New state has no class | Remove previous instance; leave slot empty, without error |

`SyncModuleBehaviour` runs only on state transitions, plus reconciliation on load, binding changes or behaviour refresh. Repeated HP loss within a state does not contact `UBehaviourComp`. Default add-on penalties depend only on state; there is no automatic HP-proportional armor loss. Use a fixed pending-slot bitmask while the behaviour component is iterating: the latest desired state per module wins, and drain at its existing safe processing boundary. Do not enqueue stale remove/add-by-class pairs. Batch behaviour-derived stat/UI refresh once after replacements; avoid exposing a temporary restored-speed state between removal and addition.

Example: `Engine Damaged → BP_EngineDamaged::OnAdded → RegisterVehicleSpeedLimit(SourceKey, EngineYellowSpeedMultiplier)`. Red removes that source's yellow effect and optionally adds the configured red behaviour. Recovery restores the yellow class; full service removes it. All removal callbacks unregister their own modifiers without restoring stale base values.

### TankMaster Blueprint state event

```cpp
/**
 * @brief Lets tank Blueprints react once to committed module state transitions.
 * @param ModuleType Type of the instance whose state changed.
 * @param NewState Committed Healthy, Damaged or Destroyed state.
 * @param RemainingModuleHp Absolute remaining module HP, clamped to [0, MaxHp].
 */
UFUNCTION(BlueprintImplementableEvent, Category = "Vehicle Modules")
void OnVehicleModuleStateChanged(
    EVehicleModuleTypes ModuleType,
    EVehicleModuleState NewState,
    float RemainingModuleHp);
```

`ATankMaster::OnModuleConditionChanged(Change)` updates repair counts and requests behaviour synchronization. After the batch's module state and behaviour-derived effects are committed, invoke the BP event once per changed module with a copied transition payload. If behaviour work is deferred, publish after that safe drain. Direct Healthy → Destroyed and strong-heal Destroyed → Healthy each emit one final-state event. Recovery-only red → yellow emits its usual event. HP loss within an unchanged state, initial setup, load reconciliation and refresh do not emit transition events.

Remaining HP is an **absolute value**, e.g. `40.0` for a 200-HP engine recovered to 20%. Native payloads retain `ModuleId`; the requested BP signature is type-based, so two instances of the same type can produce two events. This event is separate from the healthbar's worst-state-per-type aggregation. Re-entrant BP mutations wait until notification dispatch ends; stop dispatch on tank death/EndPlay. No implementation of the BP event is required, and BP must not reapply the native behaviour/icon work.

### `ModuleTypeRules` defaults

| Type | Default asset's yellow behaviour | Default asset's red behaviour |
| --- | --- | --- |
| Tracks / Wheels | Travel ×0.65; hull turning ×0.70 | No powered travel/turning |
| Engine | Travel ×0.70; acceleration ×0.60 | No powered travel/turning; weapons remain available |
| Ammo | Reload duration ×1.50 | No new reload; loaded rounds may fire |
| Turret | Traverse ×0.50 | Traverse locked; aligned gun may fire |
| Weapon | Dispersion ×1.25; firing-cycle duration ×1.20 | Only the bound weapon stops firing |
| AddOnArmor | Covered add-on contribution ×0.50 | Covered add-on contribution ×0; structural armor remains |

These are default behaviour implementations, **not mandatory effects of module states**. All values, including `AddOnArmorYellowContributionMultiplier = 0.50` and `AddOnArmorDestroyedContributionMultiplier = 0.0`, are constexpr fields in `VehicleModuleBalance.h`. HP ratios are defined only in the profile table. Wheels use their own enum/icon entry. Default behaviours combine engine/running-gear speed limits using the minimum, not their product, and restrict only their bound weapon/mount.

```text
ATankMaster::RebuildVehicleCapabilities()
    -> current base values + research/veterancy
    -> active behaviour modifiers/restrictions, including assigned module behaviours
    -> existing non-module restrictions: stun, dig-in, service, death
    -> tracked / wheeled movement and mounted weapon adapters
```

**No assigned behaviour means no module-caused stat or capability effect**, including at Destroyed. The module still has HP/state, an icon, a BP transition event and repair eligibility. Native damage/state code never disables movement, firing, reload, traverse or armor. Removing yellow behaviour when red has no class removes its penalty and restores the normal behaviour-composed result. There is no hidden fallback.

The default asset can assign both yellow and red classes from the table. Behaviours own all module restrictions and armor modifiers through source-specific registration/removal functions; shared stat-composition code only applies their registered effects. Stun expiry, turret swap and `SetTurretsToAutoEngage` preserve active behaviour restrictions without inspecting module state.

`RefreshAllBehaviours` must retain module source identity/context and rebuild module slots through `SetModuleBehaviour`, not the current class-only re-add path. Commit behaviour-derived effects before publishing Blueprint/UI callbacks. Defer re-entrant mutations until the batch ends; death takes priority.

## 9. Hit integration and allocation guarantees

```text
AProjectile::ArmorCalc_KineticProjectile / UWeaponStateTrace hit handling:
    HandleShieldHit() -> absorbed: stop
    GetEffectiveArmorOnHit(...) -> plate, effective armor, adjusted penetration
    ResolveExistingPenetrationPolicy(...) -> bPen
    ApplyExistingHullDamage(...) -> FVehicleDamageReceipt
    if tank survived and plate/source are eligible:
        CalculateModuleDamage(PlateHit, EffectiveArmor, bPen,
                              Receipt.AppliedDamage, BaseDamage, Calibre, Context)
    finish bounce / overpenetration / impact feedback without repeated hull damage
```

`FVehicleDamageReceipt` carries applied HP damage and death status. `AHpPawnMaster::TakeDamage` currently returns **0 on death / 1 on survival**. Capture actual damage after `UHealthComponent::TakeDamage(float& InOutDamage, ...)` updates the amount.

| Source | Adapter rule |
| --- | --- |
| Projectile / hitscan | Common resolver; preserve existing penetration behavior initially |
| HE/HEAT bounce | External modules; existing hull chip once; remove duplicate legacy “module damage”/stun handling and UI-pool gating |
| Railgun overpenetration | One receipt per victim; global overpenetration factor |
| Shield absorbed | No underlying module call |
| Direct mine | `CalculateMineModuleDamage(Context)`; selected running-gear candidate, base probability 1 × class factor, normal caps |
| Splash | `CalculateSplashModuleDamage(Context)`; external modules, attenuation/occlusion, direct victim excluded |
| ICBM direct hit / ICBM splash | Skip module damage entirely; preserve existing hull damage/armor calculations, including surviving tanks |
| Fire / laser / radiation / DOT | Existing hull effects until an explicit module adapter is designed |

Mine/splash rules and attenuation settings also live in the balance header. An add-on candidate requires an affected plate with configured coverage; these adapters cannot bypass the coverage map. Rear-armor AOE estimates cannot become fictitious engine hits. Pool reuse needs activation/generation IDs for deduplication. Invalid plate output must not silently become `Plate_Front`.

`AICBMActor::ApplyDirectDamage` already calculates armor and calls `TakeDamage`; leave module resolution out of it. `ApplyAOEDamage` uses `FRTS_AOE::DealDamageVsRearArmorInRadiusAsync`. Keep module splash opt-in at weapon adapters; if the shared AOE helper gains module support, default its module policy to Ignore so ICBMs retain the current path. Do not add tank-wide module damage inside generic `TakeDamage`.

### Fixed storage and constexpr implementation contract

| Work | Implementation |
| --- | --- |
| Type capacities/slot offsets | constexpr prefix sums; `MaxWeaponModules == 1`, total 17; checked slot lookup |
| Profile/gear/plate rules | Shared `inline constexpr` tables: 5 × 2 × 17 rows, each with three entries; store one const table pointer per tank |
| Enum/source lookup | constexpr checked enum-to-index tables; no enum reflection, strings or hash maps per hit |
| Chance calculation | Bake profile chance factors and gear substitution at compile time; retain source multiplier and random roll at runtime |
| Type thresholds, HP ratios, calibre gates | constexpr enum-indexed arrays; prevalidate ranges, row layout and enum coverage with `static_assert` |
| Module HP, bindings, routes | Zero-initialized embedded fixed arrays; no separate UObject per module or per-tank rule-table copy |
| Impact damage batch | Stack POD array of three entries plus count; normalize/cap with bounded loops, no sort or temporary container |
| State/repair change batch | Fixed array of `MaxModuleInstances` value payloads; enough for restoring every module in one call |
| Icon deltas/counts | Fixed enum-sized counters/state arrays; one changed-type bitmask |
| Non-pen history | Existing constant-sized bucket ring per module; no sample list, allocation or pruning container |
| Healing work | Stack receipt, cached counts and scalar finishing counter; select one final-state milestone per heal |
| Crew work | One timer, scalar current-target time and bounded fixed-array selection at target changes; no tick allocations or card rebuilds |
| Binding/coverage rebuild | Fixed scratch routes/plate IDs; validate then commit; Blueprint input arrays are setup-only |

Use fixed C++ arrays/`std::array` for POD storage, and the project's reflected fixed-array pattern for GC-visible UObject references. Do not use `TArray::Reserve` as an allocation-free guarantee, or inline allocators that silently spill beyond capacity. Capacity violations reject setup before play; a hit can never grow storage. Pass rule tables by const reference and change batches by bounded view; never retain a view after its owner returns.

Valid `ResolveModuleDamage` calls and native HP/state/count mutations perform **zero heap allocations**. Rejected inputs return an empty fixed result; format any diagnostic only outside this region. The resolver produces fixed change records and finishes before dispatching behaviour/BP/UI work. Runtime hit geometry, module HP, object pointers, health-component values and random draws cannot be constexpr. Cache what setup determines; do not duplicate all source combinations just to avoid a cheap runtime multiply.

`CalculateModuleDamage` may call the dispatcher after resolution, so the complete outer call may allocate on a transition. Measure the resolver/commit region separately. Behaviour callbacks may re-enter through the existing safe deferred-operation path; those callback queues are outside this allocation-free region. Ordinary hits that do not change state allocate nothing in the added module path.

Existing `UBehaviourComp` allocates UObjects/dynamic storage. Create/remove module behaviours only on state transitions outside the resolver; unchanged states do no behaviour work. This minimal version does not preallocate behaviour objects or promise allocation-free UMG/behaviour transitions.

## 10. Existing code changes required later

| Existing source / symbol | Required integration |
| --- | --- |
| [ArmorCalculation.cpp](../RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.cpp): `NoArmorHitGetClosest` | Fill all outputs; fix squared-distance threshold and adjusted-penetration reset before routing fallback hits |
| Same: `EvaluateArmorPlatesForHit`, `GetArmorAtAngle` | Preserve registered zero-armor plate identity; handle near-zero cosine safely |
| Same: armor setters/multipliers, rear cache | Separate structural/permanent/add-on armor; BP coverage routes hits and behaviour modifiers; only assigned behaviours alter protection on module damage; refresh rear cache when effects change |
| [Projectile.cpp](../RTS_Survival/Weapons/Projectile/Projectile.cpp): kinetic/bounce/overpenetration paths | Common resolved-hit module call exactly once |
| [WeaponData.cpp](../RTS_Survival/Weapons/WeaponData/WeaponData.cpp): `DidTracePenArmorCalcComponent` | Return plate context and penetration; retain sampled damage flux |
| [HpPawnMaster.cpp](../RTS_Survival/MasterObjects/HealthBase/HpPawnMaster.cpp), [HealthComponent.cpp](../RTS_Survival/RTSComponents/HealthComponent.cpp) | Expose real damage receipt; actor return value is not damage |
| Same health component: `Heal`, `GetHasDamageToRepair`, `SetMaxHealth` | Central healing transaction/receipt; include modules in tank eligibility/completion; notify actual MaxHealth changes |
| Same: `OnWidgetInitialized`, `SetHealthBarVisibility` | Initialize/rebind `ModuleBox`; flush pending icon state on visibility changes |
| [W_HealthBar.h](../RTS_Survival/GameUI/Healthbar/W_HealthBar.h), [BehaviourButtonSettings.h](../RTS_Survival/Behaviours/ProjectSettings/BehaviourButtonSettings.h) | Existing widget/settings reference pattern; shared module asset with textures and optional classes; widget icon cache |
| [ICBMActor.cpp](../RTS_Survival/Weapons/ICBM/ICBMActor/ICBMActor.cpp): `ApplyDirectDamage`, `ApplyAOEDamage` | Retain hull-only damage; no module adapter |
| [TankMaster.h](../RTS_Survival/Units/Tanks/TankMaster.h) | CrewRepair execute/terminate/timer and automatic cleanup, native counts, behaviour synchronization and BP state event |
| [RepairComponent.cpp](../RTS_Survival/RTSComponents/RepairComponent/RepairComponent.cpp), [RepairHelpers.h](../RTS_Survival/RTSComponents/RepairComponent/RepairHelpers/RepairHelpers.h), [RadiusRepairAura.cpp](../RTS_Survival/RTSComponents/AOEBehaviourComponent/RadiusAOEBehaviourComponent/RepairAura/RadiusRepairAura.cpp) | Keep normal healing calls; tank-aware eligibility/completion includes full-health module-only targets |
| [TickingHealBehaviour.cpp](../RTS_Survival/Behaviours/Derived/Heal/TickingHealBehaviour.cpp), [SingleHealBehaviour.cpp](../RTS_Survival/Behaviours/Derived/Heal/SingleHealBehaviour.cpp) | Existing `Heal` calls automatically recover modules; no custom tank payload |
| [BehVehicleStunned.cpp](../RTS_Survival/Behaviours/Derived/BehaviourVehicleStunned/BehVehicleStunned.cpp), [BehaviourComp.h](../RTS_Survival/Behaviours/BehaviourComp.h) | Fixed module behaviour slots; exact-instance replacement; preserve identity through refresh/deferred operations; no stale speed restore |
| [TurretSwapComp.h](../RTS_Survival/RTSComponents/AbilityComponents/TurretSwapComponent/TurretSwapComp.h) | Preserve condition by mount role; rebind without healing/reloading |
| [TrackPathFollowingComponent.h](../RTS_Survival/Units/Tanks/TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h), [TrackPhysicsMovement.cpp](../RTS_Survival/Units/Tanks/TrackedTank/TrackPhysicsMovementComp/TrackPhysicsMovement.cpp) | Apply restrictions to navigation/movement; copy scalar limits into physics snapshots |
| [Abilities.md](Abilities.md), [Commands.cpp](../RTS_Survival/Interfaces/Commands.cpp), [CPPController.cpp](../RTS_Survival/Player/CPPController.cpp) | CrewRepair enum/subtypes, final reserved slot, queued Enable, immediate Disable and single completion/termination |
| [BehWeaponRemoveAbilities.h](../RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehWeaponRemoveAbilities.h) | Share suppression ownership with CrewRepair so overlapping removals restore correctly |

## 11. Shared module asset and tank healthbar icons

### Settings and module asset

Place settings, asset and cache beside the module types under `ArmorCalculationComponent/VehicleModules/`; widget helpers stay under `GameUI/Healthbar/VehicleModules/`. Follow the existing `UBehaviourButtonSettings` config-reference pattern:

```text
UVehicleModuleSettings : UDeveloperSettings
    UCLASS(Config=Game, DefaultConfig)
    UPROPERTY(EditAnywhere, Config)
    TSoftObjectPtr<UVehicleModuleDataAsset> ModuleDataAsset

FVehicleModuleStateAssets : reflected struct
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UTexture2D> DamagedTexture
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UTexture2D> DestroyedTexture
    UPROPERTY(EditDefaultsOnly) TSubclassOf<UVehicleModuleBehaviour> DamagedBehaviourClass
    UPROPERTY(EditDefaultsOnly) TSubclassOf<UVehicleModuleBehaviour> DestroyedBehaviourClass

UVehicleModuleDataAsset : UDataAsset
    UPROPERTY(EditDefaultsOnly)
    TMap<EVehicleModuleTypes, FVehicleModuleStateAssets> ModulesByType
    UPROPERTY(EditDefaultsOnly)
    FVector2D ImageSize = FVector2D(DefaultModuleIconWidth, DefaultModuleIconHeight)

UVehicleModuleSubsystem : UGameInstanceSubsystem
    InitializeModuleAssets()
    GetModuleBehaviourClass(Type, State) const -> cached optional class
    GetModuleIconStyle(Type, State) const -> cached texture + ImageSize
    OnModuleAssetsReady
```

**Default image size: X = 225.0, Y = 225.0** (UMG layout units before the existing healthbar render scale). The asset supplies yellow damaged and red destroyed textures for every installed enum type, including separate Tracks and Wheels entries. Healthy/None need no texture. Validate missing entries/textures and nonpositive/nonfinite dimensions once on load; report configuration errors and skip unavailable icons without affecting gameplay.

Both behaviour fields default to null and are independently optional: **no validation error for an unassigned damaged or destroyed behaviour**. Missing textures must not block valid behaviour classes. Healthy resolves to null without an asset lookup.

Load the soft asset once during game initialization, retain it with `UPROPERTY() TObjectPtr<>` in the subsystem, and compile its map into a fixed enum-indexed cache of textures/classes. Retain class/texture references through the asset and include them in cooking. Gameplay asset readiness precedes tank module finalization, including on dedicated servers; only UMG work is client-only. A missing asset reports a configuration error once; module state/repair logic continues, but absent behaviours produce no stat effects.

Tanks cache a weak subsystem reference; no per-tank loads, per-hit map lookups or visibility-dependent behaviour activation. If late readiness is supported, reconcile each surviving tank's current states once without replaying historic BP events, then let health components flush pending icons. Disconnect readiness callbacks at EndPlay.

### Required `ModuleBox` lookup

All module UI entry points live on the tank's existing `UHealthComponent`. Enable them once through `InitializeTankModulePresentation(ATankMaster* Tank)`; reject non-tank owners. Do not require `ModuleBox` on squad/building healthbars.

```text
UHealthComponent::InitializeTankModulePresentation(ATankMaster* Tank)
UHealthComponent::Widget_FindAndCacheModuleBox() -> bool
UHealthComponent::Widget_GetIsValidModuleBox() const -> bool
UHealthComponent::Widget_OnModulePresentationReady()
UHealthComponent::Widget_ReleaseModuleIcons()

Widget_FindAndCacheModuleBox():
    Widget_GetIsValidHealthBarWidget() or return false
    M_ModuleBox = Cast<UHorizontalBox>(HealthBar.GetWidgetFromName(TEXT("ModuleBox")))
    return Widget_GetIsValidModuleBox()
```

`Widget_GetIsValidModuleBox()` reports through `RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object`, naming **ModuleBox**, the tank and widget class if absent or the wrong type. Centralize error reporting in this validator; log once per widget generation and suspend icon writes until reinitialization. Keep `M_ModuleBox` as `UPROPERTY() TWeakObjectPtr<UHorizontalBox>`. Look it up once per widget instance, never on damage/repair ticks. The tank Blueprint healthbar must contain an initially empty `UHorizontalBox` named exactly `ModuleBox`, reserved for these icons.

The health component's current `Widget_CreateHealthBar` broadcasts readiness before `OnWidgetInitialized` finishes. Use `OnWidgetInitialized` after successful base setup for the initial lookup; if tank presentation is enabled later, initialize immediately when the widget is already ready. Widget reconstruction/replacement invalidates cached children and calls the same hook once again. Missing or intentionally disabled widgets retain desired state without repeated lookup/errors. Dedicated servers skip presentation entirely.

### State changes → health component → widget operations

Use one icon per module **type**, showing the worst installed state: Destroyed > Damaged > Healthy. Two damaged tracks produce one Tracks icon; repairing one red track leaves the icon red while another remains red. At most six type icons can be active because Tracks/Wheels are exclusive. Every damaged type remains visible when the healthbar is shown.

```text
UArmorCalculation::CommitModuleDamageBatch / Restore...():
    update module state and per-type damaged/destroyed counts
    build fixed FModuleIconDeltaBatch only for changed aggregate type states
    after module state and behaviour-effect commit:
        ATankMaster::OnModuleIconStatesChanged(DeltaBatch) // only if nonempty
            -> HealthComponent.ApplyModuleIconStateChanges(DeltaBatch)

UHealthComponent::ApplyModuleIconStateChanges(const FModuleIconDeltaBatch& Changes)
UHealthComponent::SynchronizeModuleIconSnapshot(const FModuleIconSnapshot& Snapshot)
UHealthComponent::Widget_FlushModuleIconChanges()
UHealthComponent::Widget_AddModuleIcon(Type, State)
UHealthComponent::Widget_ChangeModuleIcon(Type, State)
UHealthComponent::Widget_RemoveModuleIcon(Type)

ApplyModuleIconStateChanges(Changes):
    update M_DesiredModuleIconStates[Type] only if different; mark dirty type bits
    if widget/assets ready and healthbar visible: Widget_FlushModuleIconChanges()

Widget_FlushModuleIconChanges():
    for each dirty type:
        Desired = M_DesiredModuleIconStates[Type]
        if Desired == M_DisplayedModuleIconStates[Type]: clear dirty bit; continue
        Healthy -> Damaged/Destroyed: Widget_AddModuleIcon(Type, Desired)
        Damaged <-> Destroyed:        Widget_ChangeModuleIcon(Type, Desired)
        Damaged/Destroyed -> Healthy: Widget_RemoveModuleIcon(Type)
        record displayed state and clear dirty bit only after successful update
```

`SynchronizeModuleIconSnapshot` runs once on tank presentation initialization/load; widget recreation replays the cached desired snapshot. Ordinary HP changes within yellow/red never send an icon delta. Repair completion publishes one batch. A strong heal clearing modules removes their red/yellow icons in this update; no intermediate yellow flash or timer-delayed removal. Icon aggregation does not inspect whether the state has a behaviour assignment.

Widget operations, always entered through the health component:

- **Add:** reuse cached `UImage` for the type, or construct once with the healthbar's `WidgetTree`; `SetBrushFromTexture(Texture, false)`, then `SetBrushSize(ImageSize)`. Add to `ModuleBox` with an Auto-sized horizontal slot and uncollapse the box. Maintain enum order and avoid duplicates. A full child-order rebuild is permitted only when membership changes.
- **Change:** set the existing image's texture in place; keep its configured dimensions and slot. No remove/create cycle for yellow ↔ red.
- **Remove:** `ModuleBox.RemoveChild(Image)` and collapse the box if empty. Retain the detached image for reuse for this widget's lifetime.

`UW_HealthBar` owns a GC-visible fixed icon cache (`UPROPERTY` reflected slots holding `TObjectPtr<UImage>`); this keeps detached images alive. Health-component references to widget-owned images remain weak. Release caches on widget replacement/EndPlay. UMG construction/child changes are outside the allocation-free combat resolver.

### Performance and visibility contract

- No module UI Tick, polling timers or property bindings. `Heal()` can cause a module transition; only that transition updates icons. Ordinary healing/`UpdateHealthBar()` calls without a module state change do no module UI work.
- Keep fixed desired/displayed state arrays and a dirty bitmask. Suppress duplicate states at the producer and health component; one notification per changed type per committed batch.
- Hidden healthbars only update desired state. Flush accumulated changes once when `SetHealthBarVisibility` actually transitions to visible, or assets/widget become ready. The existing visibility delegate can fire repeatedly; compare visibility first.
- Asset errors and missing ModuleBox stop retries until a readiness/rebuild event, avoiding per-hit log spam. Retain pending state for recovery.
- A module-only hit at full hull health counts as damaged for an enabled tank healthbar's `bDisplayOnDamaged` policy. Re-evaluate that policy only when aggregate module visibility changes. Preserve selection/hover preferences, fog-of-war rules and hide-all/permanent-hide overrides.
- After a batch, skip presentation if the tank died. Late callbacks use weak targets/generation checks; never recreate icons on a dead tank.

## 12. UX, lifecycle and safety

- **Yellow:** damaged module. **Red:** destroyed module; tooltip says “Crew Repair available.” Show stat consequences only when its assigned behaviour applies them. Healthy is neutral.
- CrewRepair appears in the final slot as EnableRepair; while active it shows DisableRepair. Tooltip/progress shows current module and total remaining crew seconds, refreshed by the active one-second timer. Remove the entry when no reds remain. Ordinary repair UI continues to show hull healing and finishing work.
- `SuspendCommandForModuleFailure(Reason)` preserves destination/target and shift queue when a behaviour applies a restriction. CrewRepair uses its explicit command policy instead: immediate Enable replaces prior orders, queued Enable waits its turn, and Disable ends the active repair command.
- `OnVehicleCapabilitiesChanged()` lets AI react to restrictions actually applied by behaviours: blocked movement stops path retries, and a disabled gun cannot drag the tank into firing range. Red state alone never imposes a restriction.
- Add-on armor retains its base protection unless an assigned behaviour modifies it. Default yellow/red behaviours apply the multipliers in section 8 only to covered add-on contributions. Structural armor remains separate; cosmetic debris cannot control damage.
- Use GC-visible owning/weak pointers and member validators; component/UObject errors use the `_Object` reporting variant.
- Mutate modules on the game thread. Physics reads copied scalar limits. Never retain array-entry references across callbacks or mutate containers during iteration.
- Death/`EndPlay` cancels timers/healing callbacks and invalidates generations. Same-frame death beats healing; stale callbacks cannot restore the tank.
- Save profile/gear, assigned slot IDs/health fractions, coverage, rule version, finishing work and random generation state; reconstruct derived HP, bindings, behaviours and the inactive CrewRepair card state on load. Version 1 saves with designer IDs map to installed slots by their exported module array order; multi-instance setup order must remain the same. Deploy/pack and swaps preserve condition. Tactical persistence/replication wiring still needs verification.
- Announce transitions only, using the settings-file cooldown. Enemy UI respects visibility; no hidden module HP or repair timers through fog.

## 13. Implementation and verification

1. **Types/settings:** constexpr class/gear/plate/source tables, derived module HP, per-type thresholds, fixed storage and BP add-on coverage.
2. **Hit integration:** correct plate/damage receipts; projectile/trace parity; duplicate-hit protection; bounded resolver.
3. **Effects:** shared optional behaviour classes, exact-instance swaps, TankMaster BP state event and combined movement/fire restrictions.
4. **Healing:** central health-component receipts and tank-owned milestones; separate module-only CrewRepair toggle, sequential timer, command restrictions and cleanup.
5. **Presentation:** shared asset textures, tank-only `ModuleBox` validation, health-component state deltas and widget lifecycle.
6. **QA:** all five profiles, tracked, wheeled, casemate, multi-turret and add-on tanks, with at most one Weapon module; AI and persistence.

Required checks:

- All 17 plate rows valid; maximum three candidates; missing modules do not redirect damage.
- Two vehicles with the same profile/gear and different BP coverage route add-on hits/bonuses only to configured plates, including cupola. Other module candidates and deterministic rolls remain identical.
- All five profiles compile valid tables; Tracks/Wheels cannot coexist. Profile selection adds no candidate and class chance multipliers apply exactly once.
- A second Weapon module is rejected regardless of its gun binding; all derived storage uses the 17-slot capacity.
- Per-type ballistic cutoffs reject just below and allow exactly at the configured value. Lowering Wheels alone does not enable other modules; hull damage and mine/splash rules remain unchanged.
- Module MaxHP uses initialized tank MaxHealth × profile ratio. Current hull damage/healing does not resize modules; MaxHealth upgrades preserve condition, including exact red thresholds. No manual module HP setup remains.
- Missing/wrong-type ModuleBox reports once on tanks only. Missing assets/textures and late readiness recover without polling or per-tank asset loads; packaged builds include the configured asset.
- Healthy → yellow/red adds one image; yellow ↔ red swaps its texture; final recovery removes it. Multiple instances aggregate correctly; icon size defaults to 225 × 225.
- Repeated damage within the same displayed state produces zero health-component icon calls and zero UMG changes. Hidden damage batches flush final state once on reveal; widget rebuilds/death leave no stale children or callbacks.
- ICBM direct and splash damage never route to module damage, even when a tank survives.
- Coverage replacement/clearing preserves HP; overlapping zones and invalid bindings fail atomically. Identical plate enums on different meshes stay distinct; load/swap rebinds safely. Setup arrays introduce no hit-path allocations.
- Equality at each destruction threshold produces red; recovery sets exactly threshold plus shared margin and produces yellow.
- Fresh modules cannot become red from one ordinary hit; multiple-failure cap uses type thresholds, not zero.
- Tank health below/equal/above gate, overshoot, new red damage above gate, full-health red/yellow targets, no repeated recovery calls after red count clears.
- CrewRepair succeeds below/equal/above the ordinary hull gate without changing hull HP or granting finishing work. Only the current red module becomes yellow, at its type's duration; engine + two tracks takes 44 seconds without interruptions.
- First red module inserts EnableRepair only in the final array slot; more reds create no duplicates. Starting switches to DisableRepair, stops physical movement/rotation and disables all turrets/hull weapons. A second Weapon module remains prohibited.
- DisableRepair is immediate even with Shift or a busy queue. It clears the timer, restores the four removed abilities with metadata/cooldowns and releases only crew-owned locks; red modules keep an EnableRepair entry. Re-enable restarts unfinished work only.
- External healing of all red modules auto-disables repair and removes its card entry without waiting for the timer. Partial recovery selects the next red target with full duration; same-frame healing/timer callbacks cannot double-repair or double-complete the command.
- Queue interruption, Stop, failed startup, death and load leave no stuck DisableRepair entry or active timer. Verify reserved final-slot placement, overlapping behaviour ability removals, new/revoked grants, turret swaps, stale subtypes and command completion exactly once.
- Commander aura, ticking/single-heal behaviour, scavenger and a new caller using only `Heal` all trigger the same recovery rules. Full-health red/yellow targets remain eligible; `Heal` returns completion only after modules recover too.
- A 700/1,000-HP tank with reds/yellows receiving 360 healing becomes fully repaired immediately; all icons and module behaviours clear, and each BP event reports Healthy once. At full hull HP, 60 healing has the same result. No yellow intermediate state or additional pulse is required.
- Healing that exactly fills the hull with zero surplus only recovers reds to yellow; existing finishing progress reduces the additional work required. Concurrent healing adds accepted surplus once and completes once, with no rate budget. Zero/rejected healing and raw setters grant no module repair work.
- A strong heal during CrewRepair restores modules fully, automatically clears the crew timer/card entry, and restores crew-suppressed functionality in the same safe dispatch; it cannot recreate yellow icons or finish the command twice.
- One receipt per accepted healing transaction, even at full hull HP. Re-entrant heals are deferred; module restoration does not recursively call `Heal`. Health UI and health-percentage notifications cannot duplicate recovery.
- Stun expiry, research, dig-in, swaps and two instances sharing a behaviour class preserve restrictions.
- Null yellow/red classes are silent; yellow → null red removes the old behaviour, null yellow → assigned red adds it, and recovery reverses correctly. Same-class transitions update context without recreation.
- Empty behaviour slots leave speed, movement, firing, reload, traverse and add-on armor unchanged by module state. Removing an old behaviour removes its penalty; no native fallback remains. Icons, state events and repair still work.
- Module behaviour operations leave unrelated same-class instances intact; deferred swaps/refresh retain source identity. Unchanged states make no behaviour calls.
- BP state event fires once per actual instance transition with type, new state and absolute remaining HP, including repair. No events for unchanged HP state, setup/load or refresh; death/re-entrant callbacks remain safe.
- Shield, bounce, mine, splash, overpenetration and projectile pooling never double-apply module damage.
- Missing UI/component, death, cancellation and re-entrant callbacks fail safely.
- Instrument allocations around resolver/commit and milestone HP updates; expect zero for valid pen/non-pen hits and full-module repair batches. Measure behaviour/widget creation separately. Static validation covers capacities and all generated profile/gear rows; stress coverage rebinds and missing candidates without container growth.
- Balance values, behaviour assets and Blueprint mesh setup remain untested until the prototype is playable.
