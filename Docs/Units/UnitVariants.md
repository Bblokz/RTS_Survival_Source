# Unit variants

Design for the player's army customisation layer that sits below unit cards. A card swaps a whole unit; a
variant is a small modification a unit applies to itself when it begins play (a different gun, extra armour
plates, a different squad weapon mix, a different turret on a pit). Every German unit has at most three
variants, of which the first is always the base unit. The player picks one variant per unit type for the
whole army before a mission; every unit of that type the player receives during the mission then starts as
that variant.

This document is the implementation brief. The C++ side (sections 3 to 6 and the automation tests of 9.1) is
implemented; where the code deviates from the brief, section 12 lists it and the affected sections say so.
The data asset `DA_UnitVariants` and the Blueprint switches (section 8 step 10) are not made yet. The variant
*selection UI* is out of scope; everything that lets the selection reach the units, and everything the UI will
need to know which units have variants to choose from, is in scope.

Decisions taken (the questions that led to them are in the session history; the proposed defaults were accepted,
with one deviation: nomadic trucks were proposed as in scope by inheritance and are excluded here because the
request names tanks, squads and BXPs; section 10 shows the three-line extension):

| Topic | Decision |
|---|---|
| Choice model | One exclusive choice per unit type, from a shared enum `EUnitVariant { Base, VariantA, VariantB }`. Not stackable. |
| Supported unit kinds | Tanks (`ATankMaster` with `UnitType == UNType_Tank`), squads (`ASquadController`, including team weapon controllers since they are squads), building expansions (`ABuildingExpansion`). Nomadic trucks, harvesters (`UNType_Harvester`) and aircraft get `Base` always; extension points are listed in section 10. |
| Who gets variants | Units whose `URTSComponent::GetOwningPlayer()` equals `UPlayerTechManager::PlayerTechOwnerIndex` (1). Everyone else gets `Base`. |
| Direction | Units **pull** their variant when their subtype is known, from a manager on `ACPPController`. The spawner pushes nothing. |
| Persistence | The selection is saved next to the cards in the player profile. |
| Scope of a selection change | Only units that begin play after the change see it. Pre-mission selection makes this moot. |
| Variant definition store | A `UDataAsset` picked in Project Settings, holding per subtype how many variants exist and their names and icons for the later UI. |
| Cost / training time | Variants never change cost or training time. They are stat, mesh and loadout tweaks done in the unit's Blueprint. |
| Per doctrine | No. The implemented count is per subtype, not per `ERTSFaction`. |
| Stale saves | A saved variant the asset no longer implements clamps to `Base` and reports an error once. |
| Testing before the UI exists | Cheat manager commands plus a developer-settings forced-variant list. |

---

## 1. Existing code this builds on

Read these before implementing. They define the ordering constraints.

| Fact | Where | Why it matters |
|---|---|---|
| Player units are spawned with `World->SpawnActor` (not deferred) and then handed to a callback. | `Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.cpp` `AttemptSpawnAsset`, `SquadController.cpp` ~4016, `Squads/Reinforcement/SquadReinforcementComponent.cpp` ~651 | `BeginPlay` runs before any post-spawn callback. Nothing can be pushed into the unit before BeginPlay without converting every spawn site to deferred spawning, which is why the unit pulls. |
| Several player-unit spawn paths never touch the async spawner. | `MissionBase::AsyncSpawnActor*`, `VTOLDropshipAircraft.cpp` ~479, `NomadicVehicle.cpp` ~2338 (conversion), `SquadController.cpp` ~1156 (team weapon controller), reinforcements | A push from the spawner would miss all of these. Pull covers all of them. |
| `URTSComponent` holds `OwningPlayer` (BP default, 222 when unset), `UnitType` and the subtype. The subtype is set by BP via `SetUnitSubtype`, which broadcasts `OnSubTypeInitialized`. | `RTSComponents/RTSComponent.h/.cpp` | The component is the natural place to store the resolved variant and the single lookup key (owner + unit type + subtype). |
| Each supported class already has a "my subtype is known, go look for upgrades" hook that calls into the player tech manager and early-outs for non-player owners. | `ATankMaster::BeginPlay` → `CheckForUpgrades()`; `ASquadController::InitSquadData` → `CheckForUpgrades()`; `ABuildingExpansion::InitBuildingExpansion` → `CheckForUpgrades()`; `URTSComponent::On*InitializedLookingForUpgrades` → `UPlayerTechManager::CheckTechsToApplyTo*` | The variant hook goes in the **same places, immediately before** `CheckForUpgrades()`, so techs apply on top of the variant. |
| Squad units are loaded asynchronously after the controller's BeginPlay and register through `ASquadUnit::SetSquadController`, which queues work on `SquadDataCallbacks.CallbackOnSquadDataLoaded`. That callback runs immediately when the data is already loaded. Reinforcements use the same `SetSquadController`. | `SquadUnit.cpp` ~2795, `SquadControllerDataCallback.cpp` | Squad units receive their variant through this callback, which also covers reinforcements and the re-crewed team weapon squad without extra code. |
| `UPlayerTechManager` is a `UActorComponent` created in the `ACPPController` constructor, exposed via `GetPlayerTechManager()` and `FRTS_Statics::GetPlayerTechManager(WorldContext)`. | `Player/PlayerTechManager/PlayerTechManager.h`, `Player/CPPController.cpp` ~267, `Utils/RTS_Statics/RTS_Statics.cpp` ~158 | The variant manager copies this shape exactly. |
| The profile is loaded in `UPlayerProfileLoader::LoadProfile`, which applies resources and techs and then spawns the starting units (`LoadInUnits`). | `Player/StartGameProfileManager/PlayerProfileLoader.cpp` ~39 | The variant manager must be initialised from the profile **before** `LoadInUnits`, otherwise the starting units pull an empty selection. |
| Project-settings-picked data assets follow `UTrainingLayoutSettings` (`UDeveloperSettings`, `TSoftObjectPtr<Asset>`, `LoadSynchronous`, class default fallback). | `GameUI/TrainingUI/TrainingLayout/TrainingLayoutSettings.h/.cpp` | Copy that pattern for the variant asset. |
| Save data lives in `USavePlayerProfile` (legacy slot `PlayerProfile`, read by the in-game loader) and in `FPlayerCardSaveData` (world campaign, `FPlayerData::CardData`, written by `W_CardMenu`). | `CardSystem/PlayerProfile/...`, `WorldCampaign/PlayerProfile/PlayerCardData/PlayerCardData.h` | The selection array is added to both so the later UI can save wherever cards are saved. |
| `Player/AGENTS.md` wants player state routed through `ACPPController`. | `RTS_Survival/Player/AGENTS.md` | The manager lives on the controller, not in the game instance or a subsystem. |

---

## 2. Runtime flow

```
Pre-mission (later UI)      Mission start                              Unit spawn (any path)
┌─────────────────┐   ┌───────────────────────────────────┐   ┌─────────────────────────────────────┐
│ player picks     │   │ UPlayerProfileLoader::LoadProfile  │   │ BP sets owner + subtype             │
│ variant per unit │──►│  1. ApplyResourceCards             │   │ C++ hook (section 5):               │
│ → saved in       │   │  2. ApplyTechnologies              │   │   URTSComponent::ApplyUnitVariant() │
│   profile        │   │  3. VariantManager->InitFromProfile│   │     owner != 1 → Base               │
└─────────────────┘   │  4. LoadInUnits (starting units)   │   │     else manager->GetSelectedVariant│
                      └───────────────────────────────────┘   │     store on RTSComponent           │
                                                              │     Execute_BP_ApplyUnitVariant     │
                                                              │   CheckForUpgrades() (techs)        │
                                                              └─────────────────────────────────────┘
```

The unit's Blueprint implements one interface event, `BP_ApplyUnitVariant(EUnitVariant Variant)`, and switches
on the enum. That is the only Blueprint work per unit.

---

## 3. New types

All new C++ lives in `RTS_Survival/Units/Variants/` unless stated otherwise. Follow `AGENTS.md` (early
returns, `not`, `M_` prefix, validator functions, Doxygen on classes, braces on their own lines).

### 3.1 `EUnitVariant` — `Units/Variants/UnitVariantTypes.h`

```cpp
UENUM(BlueprintType)
enum class EUnitVariant : uint8
{
	Base     UMETA(DisplayName = "Base"),
	VariantA UMETA(DisplayName = "Variant A"),
	VariantB UMETA(DisplayName = "Variant B"),
};

namespace UnitVariants
{
	// Base plus two alternatives; the enum, the data asset validation and the UI all rely on this cap.
	constexpr int32 MaxVariantsPerUnit = 3;
}

static inline FString Global_GetUnitVariantAsString(const EUnitVariant Variant);  // define inline in the header, like Global_GetCardTypeAsString
```

The enum is shared by every unit on purpose: a Blueprint `Switch on EUnitVariant` node gives the designer
exactly three pins, and the cap is enforced by the type. The *meaning* of VariantA for a Panzer IV and for a
PaK 40 is unrelated; the display name per unit comes from the data asset (3.3).

### 3.2 `FUnitVariantSelection` — same header

The saved choice for one unit type.

```cpp
USTRUCT(BlueprintType)
struct FUnitVariantSelection
{
	GENERATED_BODY()

	// Which unit this choice is for. Only Tank, Squad and BuildingExpansion unit types are valid keys.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FTrainingOption Unit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EUnitVariant Variant = EUnitVariant::Base;
};
```

`FTrainingOption` (`GameUI/TrainingUI/TrainingOptions/TrainingOptions.h`) is `EAllUnitType` + `uint8`
subtype and already hashes, so the runtime map is `TMap<FTrainingOption, EUnitVariant>`. Building expansions
use `EAllUnitType::UNType_BuildingExpansion` 
with `static_cast<uint8>(EBuildingExpansionType)` as the subtype, which is also what `URTSComponent` stores
for them.

### 3.3 `UUnitVariantDataAsset` — `Units/Variants/UnitVariantDataAsset.h/.cpp`

The designer-edited list of which variants exist per German unit. Keyed by the typed subtype enums so the
asset is readable in the editor; converted to `FTrainingOption` at runtime.

```cpp
/** One selectable variant as the later UI will show it. */
USTRUCT(BlueprintType)
struct FUnitVariantDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;
};

/** The variants one unit type has. Index 0 is always the base unit; the array length is the implemented count. */
USTRUCT(BlueprintType)
struct FUnitVariantEntry
{
	GENERATED_BODY()

	// Element 0 describes the base unit, 1 is VariantA, 2 is VariantB. Length 1 means "no choice to offer".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (TitleProperty = "DisplayName"))
	TArray<FUnitVariantDefinition> Variants;
};

/**
 * @brief Make one asset of this class and pick it in Project Settings > Unit Variants. It lists, per German unit
 * subtype, which variants are implemented in that unit's Blueprint; the variant UI only offers a choice for
 * entries with more than one variant and the runtime clamps saved choices to what is listed here.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UUnitVariantDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** @return 1 when the unit has no entry (base only), otherwise the entry's array length clamped to 1..3. */
	int32 GetImplementedVariantCount(const FTrainingOption& Unit) const;

	/** @return True when Variant's index is below the implemented count of Unit. Base is always implemented. */
	bool GetIsVariantImplemented(const FTrainingOption& Unit, const EUnitVariant Variant) const;

	/** @return Null when the unit has no entry or the variant is not implemented. */
	const FUnitVariantDefinition* FindVariantDefinition(const FTrainingOption& Unit, const EUnitVariant Variant) const;

	/** @return Every unit with more than one implemented variant, for the later UI. */
	TArray<FTrainingOption> GetUnitsWithChoice() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	const FUnitVariantEntry* FindEntry(const FTrainingOption& Unit) const;

	UPROPERTY(EditAnywhere, Category = "Unit Variants|Tanks", meta = (AllowPrivateAccess = "true"))
	TMap<ETankSubtype, FUnitVariantEntry> M_TankVariants;

	UPROPERTY(EditAnywhere, Category = "Unit Variants|Squads", meta = (AllowPrivateAccess = "true"))
	TMap<ESquadSubtype, FUnitVariantEntry> M_SquadVariants;

	UPROPERTY(EditAnywhere, Category = "Unit Variants|Building Expansions", meta = (AllowPrivateAccess = "true"))
	TMap<EBuildingExpansionType, FUnitVariantEntry> M_BuildingExpansionVariants;
};
```

The shipped header adds the private editor-only helpers `ValidateEntry` / `GetIsRussianKey` and a test
`friend`; see section 12.

Rules the implementation must follow:

- A subtype with **no entry** has exactly one variant (Base). Designers only add entries for units that have
  a real alternative, so the asset stays short.
- `FindEntry` switches on `Unit.UnitType`: Tank → `M_TankVariants`, Squad → `M_SquadVariants`,
  BuildingExpansion → `M_BuildingExpansionVariants`, anything else → null. Cast `Unit.SubtypeValue` to the
  typed enum before the `Find`.
- `GetImplementedVariantCount` clamps to `[1, UnitVariants::MaxVariantsPerUnit]`. An entry with an empty
  array still counts as 1.
- `IsDataValid` reports an error (not a warning) for: an entry with 0 or more than 3 elements; a non-base
  element (index ≥ 1) with an empty `DisplayName`; a key that is the `_None` / `BXT_Invalid` enumerator; a
  Russian subtype as key (enumerator name contains `Rus`, *or* display name starts with `Rus`: Russian tanks
  such as `Tank_T34_85` only carry it in the display name "Rus Medium T-34/85"). Follow
  `UTrainingLayoutDataAsset::IsDataValid` for the style.
- Nothing in the asset is loaded at runtime except through the settings class below.

### 3.4 `UUnitVariantSettings` — `Units/Variants/UnitVariantSettings.h/.cpp`

Copy of `UTrainingLayoutSettings`.

```cpp
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Unit Variants"))
class RTS_SURVIVAL_API UUnitVariantSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UUnitVariantSettings* Get() { return GetDefault<UUnitVariantSettings>(); }

	/** @return The picked asset, or the class defaults when none is picked, so every unit resolves to Base. */
	const UUnitVariantDataAsset* LoadVariantDataAsset() const;

	UPROPERTY(Config, EditAnywhere, Category = "Unit Variants")
	TSoftObjectPtr<UUnitVariantDataAsset> VariantDataAsset;

	// Development aid until the selection UI exists: applied over the saved selection when the manager initialises.
	// Ignored in Shipping builds. Clear it before committing.
	UPROPERTY(Config, EditAnywhere, Category = "Unit Variants|Debug", meta = (TitleProperty = "Unit"))
	TArray<FUnitVariantSelection> DebugForcedVariants;
};
```

### 3.5 `IUnitVariantReceiver` — `Interfaces/UnitVariantReceiver.h`

```cpp
UINTERFACE(BlueprintType)
class UUnitVariantReceiver : public UInterface
{
	GENERATED_BODY()
};

/**
 * @brief Implemented in C++ by every unit base class that supports variants; the unit's Blueprint implements
 * the event and switches on the variant to apply its modifications. Fired once, after the subtype is known.
 */
class RTS_SURVIVAL_API IUnitVariantReceiver
{
	GENERATED_BODY()

public:
	/**
	 * @brief Apply the modifications of this variant. Base is also delivered so a Blueprint can run shared setup.
	 * @param Variant The variant the player selected for this unit type; Base for non-player units.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Unit Variant")
	void BP_ApplyUnitVariant(EUnitVariant Variant);
};
```

Because the C++ classes inherit `IUnitVariantReceiver`, their Blueprint children see
`Event BP_ApplyUnitVariant` under *Interfaces* in the graph. C++ fires it with
`IUnitVariantReceiver::Execute_BP_ApplyUnitVariant(OwnerActor, Variant)`, never by calling the function
directly. The interface deliberately has no getter; the variant is read from `URTSComponent` (3.7).

### 3.6 `UPlayerUnitVariantManager` — `Player/PlayerUnitVariantManager/PlayerUnitVariantManager.h/.cpp`

```cpp
/**
 * @brief Owned by the player controller; holds the variant the player selected per unit type for this mission and
 * answers units that pull their variant when they begin play. Initialised from the profile before starting units spawn.
 */
UCLASS()
class RTS_SURVIVAL_API UPlayerUnitVariantManager : public UActorComponent
{
	GENERATED_BODY()

public:
	/**
	 * @brief Replaces the selection with the saved one, drops entries for unsupported unit types and clamps every
	 * variant the data asset does not implement to Base, reporting each clamp once.
	 * @param SavedSelections The selections stored in the player profile.
	 */
	void InitFromProfile(const TArray<FUnitVariantSelection>& SavedSelections);

	/** @return The selected variant for the unit type, Base when nothing was selected or the asset is missing. */
	EUnitVariant GetSelectedVariant(const FTrainingOption& Unit) const;

	/**
	 * @brief The one entry point units use. Returns Base for every owner except the player and for unit types
	 * without variant support, so callers need no owner check of their own.
	 * @param RTSComponent The unit's RTS component with owner, unit type and subtype already set.
	 */
	EUnitVariant ResolveVariantForUnit(const URTSComponent* RTSComponent) const;

	/**
	 * @brief Changes one selection at runtime (cheat manager, future UI). Clamps like InitFromProfile.
	 * @return False when the unit type is unsupported or the variant is not implemented.
	 */
	bool SetSelectedVariant(const FTrainingOption& Unit, const EUnitVariant Variant);

	TArray<FUnitVariantSelection> GetSelectionsForSave() const;

	const UUnitVariantDataAsset* GetVariantDataAsset() const;

	static bool GetIsUnitTypeSupported(const EAllUnitType UnitType);

protected:
	virtual void BeginPlay() override;

private:
	void BeginPlay_LoadVariantDataAsset();
	void ApplyDebugForcedVariants();
	bool GetIsValidVariantDataAsset() const;

	// Same declaration shape as UTrainingLayoutSubsystem::M_TrainingLayout.
	UPROPERTY()
	TObjectPtr<const UUnitVariantDataAsset> M_VariantDataAsset;

	// Only units with a non-Base choice are stored; a missing key means Base.
	TMap<FTrainingOption, EUnitVariant> M_SelectedVariants;
};
```

The shipped header adds a constructor, the private `StoreSelectionClamped` and `CacheVariantDataAsset`, and a
test `friend`; see section 12.

Behaviour details:

- `BeginPlay` loads the asset via `UUnitVariantSettings::Get()->LoadVariantDataAsset()` and stores it. The
  asset is `const` and owned by the asset registry; the `UPROPERTY` keeps it alive. `InitFromProfile` and
  `SetSelectedVariant` cache it too (`CacheVariantDataAsset`) and the const `GetVariantDataAsset()` loads it
  without caching when the pointer is still null, so `InitFromProfile`, the cheats and `ResolveVariantForUnit`
  work whatever order they run in relative to the component's `BeginPlay`.
- `InitFromProfile` is called from `UPlayerProfileLoader::LoadProfile` right after `ApplyTechnologies` and
  before the `bDoNotLoadPlayerUnits` early return that precedes `LoadInUnits`, through the private
  `UPlayerProfileLoader::ApplyUnitVariantSelections`. It clears the map, inserts valid entries, then calls
  `ApplyDebugForcedVariants()` (guarded by `#if !UE_BUILD_SHIPPING`).
- Clamping: for every selection, if `not M_VariantDataAsset->GetIsVariantImplemented(Unit, Variant)` then
  `RTSFunctionLibrary::ReportError` with the unit's `GetTrainingName()` and the variant string, and store
  nothing (Base). Do this once per selection, in `InitFromProfile`/`SetSelectedVariant` (both go through the
  private `StoreSelectionClamped`), never in `GetSelectedVariant`, which is called per spawned unit. Selecting
  `Base` removes the key.
- `ApplyDebugForcedVariants` does **not** clamp: it only checks `GetIsUnitTypeSupported`, so a Blueprint variant
  that has no asset entry yet can be tested (section 7 step 3).
- `ResolveVariantForUnit`: `not IsValid(RTSComponent)` → Base; owner `!= UPlayerTechManager::PlayerTechOwnerIndex`
  → Base; `not GetIsUnitTypeSupported(UnitType)` → Base; otherwise
  `GetSelectedVariant(FTrainingOption(UnitType, RTSComponent->GetUnitSubType()))`.
- `GetIsUnitTypeSupported`: `UNType_Tank`, `UNType_Squad`, `UNType_BuildingExpansion` → true. `UNType_Harvester`
  (the Pz I harvester is an `ATankMaster` with this unit type), `UNType_Nomadic`, `UNType_Aircraft`,
  `UNType_StandaloneTurret`, `UNType_GarrisonBuilding` → false.

Wiring on `ACPPController` (mirror `M_PlayerTechManager` exactly):

- `UPROPERTY() UPlayerUnitVariantManager* M_PlayerUnitVariantManager;` created in the constructor with
  `CreateDefaultSubobject<UPlayerUnitVariantManager>(TEXT("PlayerUnitVariantManager"))`.
- `inline UPlayerUnitVariantManager* GetPlayerUnitVariantManager() const`.
- `bool GetIsValidPlayerUnitVariantManager()` validator using `ReportErrorVariableNotInitialised`.
- `FRTS_Statics::GetPlayerUnitVariantManager(const UObject* WorldContextObject)` next to
  `GetPlayerTechManager`, same null-returning shape.

### 3.7 Changes to `URTSComponent`

Add to `RTSComponents/RTSComponent.h/.cpp`:

```cpp
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUnitVariantApplied, EUnitVariant);

public:
	// Fired once when the variant is resolved; components (e.g. UI, portraits) that care can bind here.
	FOnUnitVariantApplied OnUnitVariantApplied;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unit Variant")
	EUnitVariant GetUnitVariant() const { return M_UnitVariant; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unit Variant")
	bool GetHasAppliedUnitVariant() const { return bM_HasAppliedUnitVariant; }

	/**
	 * @brief Resolves the player's selection for this unit, stores it and fires BP_ApplyUnitVariant on the owner.
	 * Call once, after owner and subtype are set and before techs are applied. Later calls are ignored.
	 */
	void ApplyUnitVariant();

	/**
	 * @brief Used by squad units, which copy the variant of their squad controller instead of resolving one.
	 * @param Variant The variant resolved by the squad controller.
	 */
	void ApplyUnitVariantFromParent(const EUnitVariant Variant);

private:
	void ApplyUnitVariant_Internal(const EUnitVariant Variant);

	EUnitVariant M_UnitVariant = EUnitVariant::Base;
	bool bM_HasAppliedUnitVariant = false;
```

`ApplyUnitVariant()`:

1. If `bM_HasAppliedUnitVariant` → return (idempotent; the hooks below are reached once but this guards
   against future callers).
2. `UPlayerUnitVariantManager* Manager = FRTS_Statics::GetPlayerUnitVariantManager(this)`. If null (enemy-only
   test maps, editor utility worlds) → `ApplyUnitVariant_Internal(EUnitVariant::Base)` and return. Do **not**
   report an error; units legitimately exist in worlds without a player controller.
3. `ApplyUnitVariant_Internal(Manager->ResolveVariantForUnit(this))`.

`ApplyUnitVariantFromParent(Variant)` has the same `bM_HasAppliedUnitVariant` guard, so a soldier fires the event
once even if it is later handed to another squad controller.

`ApplyUnitVariant_Internal(Variant)`: store, set the flag, then if the owner actor
`Implements<UUnitVariantReceiver>()` call `IUnitVariantReceiver::Execute_BP_ApplyUnitVariant(Owner, Variant)`;
finally `OnUnitVariantApplied.Broadcast(Variant)`. Fire the event for `Base` too; a Blueprint that does
nothing for Base simply leaves that pin unconnected.

Owner changes: `ChangeOwningPlayer` / `SetOwningPlayerRuntime` do not re-resolve. A captured enemy unit keeps
its Base variant and a player unit given to a mission keeps its variant. Document this in the function comment.

---

## 4. Save data

Add `TArray<FUnitVariantSelection>` with `SaveGame` to both places cards are saved. Default empty means "all
Base", so old saves keep working with no migration.

| Struct / class | Field | Plumbing |
|---|---|---|
| `USavePlayerProfile` (`CardSystem/PlayerProfile/SavePlayerProfile/USavePlayerProfile.h`) | `UPROPERTY(SaveGame) TArray<FUnitVariantSelection> M_UnitVariantSelections;` | Add a defaulted trailing parameter to `Initialize` and to `FRTS_Profile::SaveNewPlayerProfile`; `FRTS_Profile::ProfileValidator` removes duplicates per `Unit` (last one wins) and entries whose `UnitType` is unsupported. `FRTS_Profile::GetTestProfile` leaves it empty. `DebugPrintProfile` prints each entry as `TrainingName -> VariantString`. |
| `FPlayerCardSaveData` (`WorldCampaign/PlayerProfile/PlayerCardData/PlayerCardData.h`) | `UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, meta=(TitleProperty="Unit")) TArray<FUnitVariantSelection> UnitVariantSelections;` | Add a defaulted trailing parameter to `FRTS_Profile::BuildPlayerCardSaveData`. `W_CardMenu` passes an empty array until the UI exists. |

The in-game loader (`UPlayerProfileLoader::LoadProfile`) reads `USavePlayerProfile` today; it passes
`PlayerProfile->M_UnitVariantSelections` to `InitFromProfile`. How the world-campaign `FPlayerCardSaveData`
reaches `USavePlayerProfile` is the card system's existing concern and is not changed here; the later UI
writes to whichever of the two it edits, and both carry the field.

---

## 5. Hook points per unit kind

Every hook is the line **immediately before** the existing `CheckForUpgrades()` call, so the variant is in
place before techs are applied. Each class gets a small private helper named `ApplyUnitVariant()` that
validates the RTS component and forwards to `URTSComponent::ApplyUnitVariant()`; `CheckForUpgrades()` is the
template for that helper.

### 5.1 Tanks — `ATankMaster`

`Units/Tanks/TankMaster.h/.cpp`. Add `public IUnitVariantReceiver` to the class declaration.

In `ATankMaster::BeginPlay()` (currently ~line 495):

```cpp
	M_TankStartGameAction.OnBeginPlay();
	if (GetIsValidRTSComponent() && RTSComponent->GetUnitType() == EAllUnitType::UNType_Tank)
	{
		ApplyUnitVariant();   // new
		CheckForUpgrades();
	}
```

By the time this runs, the Blueprint's BeginPlay (which calls `SetUnitSubtype`) has already executed inside
`Super::BeginPlay()`; the comment "Wait for bp begin play to set the subtype" above `BeginPlay_SetupData()`
documents that. The `UNType_Tank` check already excludes nomadic trucks, which keeps `ANomadicVehicle`
(derived from `ATrackedTankMaster`) on Base without touching it.

### 5.2 Squads — `ASquadController` and `ASquadUnit`

`Units/SquadController.h/.cpp`. Add `public IUnitVariantReceiver`. `ATeamWeaponController` inherits it; PaK,
Flak, mortar and MG crews are squads with `ESquadSubtype` keys (`Squad_Ger_PaK38`, …) and need nothing extra.

In `ASquadController::InitSquadData()` (~line 3714), after `InitSquadData_InitExperienceComponent()` and
`InitSquadData_SetupSquadHealthAggregation()` and **before** `SquadDataCallbacks.SetDataLoaded(true)`:

```cpp
	DamageSquadGameStart.ApplyStartDamageToSquad(M_TSquadUnits);
	ApplyUnitVariant();                 // new: before SetDataLoaded so units' callbacks see the variant
	SquadDataCallbacks.SetDataLoaded(true);
	SquadDataCallbacks.OnSquadDataReady.Broadcast();
	M_SquadLoadingStatus.bM_HasInitializedData = true;
	CheckForUpgrades();
```

`InitSquadData` runs once all initial squad units are loaded and the subtype is set (either from
`OnAllSquadUnitsLoaded` or from `OnSquadSubtypeSet`, whichever is last), so at this point the controller's
Blueprint event can safely touch `M_TSquadUnits` (change weapons, swap meshes, adjust squad size).

The re-crew path reaches the same place: `TryRemanAbandonedTeamWeapon_SpawnReplacementController` spawns the
replacement controller with `SetShouldSkipInitialSquadUnitLoad(true)` (so its BeginPlay loads nothing), the
configure step copies the subtype onto its RTS component, then `SquadController.cpp` ~1231 sets
`bM_HasFinishedLoading = true` and calls `OnAllSquadUnitsLoaded()` → `InitSquadData()`. No second hook is
needed there.

`Units/Squads/SquadUnit/SquadUnit.h/.cpp`. Add `public IUnitVariantReceiver` so soldier Blueprints can also
react (e.g. a helmet mesh). In `ASquadUnit::SetSquadController` (~line 2795) extend the existing
`CallOnDataLoaded` lambda:

```cpp
	auto CallOnDataLoaded = [WeakThis]()-> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->OnDataLoaded_InitExperienceWorth();
		WeakThis->OnDataLoaded_ApplyUnitVariantFromSquad();   // new
	};
```

`OnDataLoaded_ApplyUnitVariantFromSquad()`: validate `M_SquadController` and both RTS components, then
`GetRTSComponent()->ApplyUnitVariantFromParent(M_SquadController->GetRTSComponent()->GetUnitVariant())`.
Because `CallbackOnSquadDataLoaded` invokes immediately when the data is already loaded, this one path covers:

- the initial units (callback fires from `OnSquadDataReady.Broadcast()` in `InitSquadData`, after the
  controller stored its variant);
- reinforcements (`SquadReinforcementComponent` calls `SetSquadController` on an already-loaded squad →
  immediate);
- the squad that re-crews an abandoned team weapon (it goes through `InitSquadData` like any squad).

Squad units never call `URTSComponent::ApplyUnitVariant()` themselves; their owner is set by the controller
later (`SetOwningPlayerAndStartFow`) and would be unreliable at BeginPlay.

#### 5.2.1 The gun actor of a team weapon — `ATeamWeaponController` and `ATeamWeapon`

The gun is a separate `ATeamWeapon` actor (`Units/TeamWeapons/TeamWeapon.h`, an `AEmbeddedTurretsMaster`
without an RTS component). `ATeamWeaponController::OnAllSquadUnitsLoaded` first runs
`Super::OnAllSquadUnitsLoaded()` (which may run `InitSquadData` and so the controller's variant hook) and only
then `SpawnTeamWeapon()`. So when the controller's Blueprint event fires, `M_TeamWeapon` is still null, and a
gun-mesh variant cannot be done from the controller's event.

Do this instead:

- `ATeamWeapon` adds `public IUnitVariantReceiver`, so gun Blueprints get the same event.
- `ATeamWeaponController` gets a private `ApplyUnitVariantToTeamWeapon()` called at the end of
  `OnAllSquadUnitsLoaded`, after `SpawnTeamWeapon()`. It validates `M_TeamWeapon` (silently, with
  `GetIsValidTeamWeaponNoReport`, because `SpawnTeamWeapon` already reported a failed spawn) and the controller's RTS
  component, then (both branches go through the private `ForwardUnitVariantToTeamWeapon`; the delegate handle is
  kept in `M_UnitVariantAppliedHandle` so it is bound once):
  - if `GetHasAppliedUnitVariant()` is already true → `IUnitVariantReceiver::Execute_BP_ApplyUnitVariant(M_TeamWeapon, GetUnitVariant())` now;
  - otherwise bind a lambda with a `TWeakObjectPtr<ATeamWeaponController>` capture to the RTS component's
    `OnUnitVariantApplied` that does the same when the controller's variant resolves later (the case where
    `InitSquadData` waits for `OnSquadSubtypeSet`).
- `SpawnTeamWeapon` has three branches (map setup, adopting an abandoned gun, fresh spawn). The call sits after
  the whole function, so all three are covered. An adopted gun that already received a variant from its
  previous crew receives the event again with the new crew's variant; gun Blueprints must therefore make the
  event idempotent (set state, do not accumulate).

### 5.3 Building expansions — `ABuildingExpansion`

`Buildings/BuildingExpansion/BuildingExpansion.h/.cpp`. Add `public IUnitVariantReceiver`.

In `ABuildingExpansion::InitBuildingExpansion(...)` (~line 1000), before the existing `CheckForUpgrades()`:

```cpp
	M_BuildingExpansionType = NewBuildingExpansionType;
	M_BuildingAttachments = NewBuildingAttachments;
	bM_IsStandAlone = bIsStandAlone;
	if (bM_IsStandAlone)
	{
		OnBxpStandAlone();
	}
	ApplyUnitVariant();   // new
	CheckForUpgrades();
```

`InitBuildingExpansion` is the Blueprint-callable setup every BXP Blueprint calls with its type, after
`BP_PostInit_SetupRTSSubtype` ran in `PostInitializeComponents`. The event therefore fires while the BXP is
still a preview or under construction, so a variant that changes `BuildingMeshComponent`'s mesh is visible
from placement onwards. If a variant needs the constructed state (turret swap on a pit), the Blueprint stores
the variant from the event and acts in `BP_OnFinishedExpansionConstruction` (or binds `OnBxpConstructed`); `GetUnitVariant`
on the RTS component is available at any later time for that.

Enemy field constructions and instant-placement batches also reach `InitBuildingExpansion`; the owner check in
`ResolveVariantForUnit` gives them Base.

### 5.4 What is deliberately not hooked

- `ANomadicVehicle` and `AAircraftMaster`: `GetIsUnitTypeSupported` returns false. See section 10 to add them.
- `ACPPTurretsMaster` and embedded turrets: a tank's Blueprint changes its own turrets inside its event.
- `URTSComponent::BeginPlay`: the subtype is not known yet there.

---

## 6. Development aids

### 6.1 Cheat manager — `Player/RTSCheatManager/RTSCheatManager.h/.cpp`

```cpp
	/** RTSCheatSetUnitVariant <UnitType> <Subtype> <Variant>: numeric enum values; affects units that spawn afterwards. */
	UFUNCTION(Exec)
	void RTSCheatSetUnitVariant(const int32 UnitType, const int32 Subtype, const int32 Variant);

	/** Prints every stored selection and the implemented count per unit with a choice. */
	UFUNCTION(Exec)
	void RTSCheatPrintUnitVariants();
```

Both resolve the manager via `FRTS_Statics::GetPlayerUnitVariantManager(this)` and print with
`RTSFunctionLibrary::ReportError`-style on-screen messages or `UE_LOG`, matching the neighbouring cheats.
`RTSCheatSetUnitVariant` returns the `SetSelectedVariant` result in the message so a wrong subtype number is
visible.

### 6.2 Forced variants in Project Settings

`UUnitVariantSettings::DebugForcedVariants` (3.4) lets a designer test a Blueprint variant without typing enum
numbers: pick the unit and variant in Project Settings > Unit Variants > Debug, start the map, train the
unit. The manager applies the list in `InitFromProfile` and logs each forced entry with `UE_LOG(LogTemp,
Warning, ...)` so a forgotten override is visible in every run.

---

## 7. Blueprint work (per unit, by the designer)

1. Open the unit Blueprint (tank, squad controller, squad unit or BXP). The parent C++ class now implements
   `UnitVariantReceiver`, so *Event BP_ApplyUnitVariant* is available under *Add Event > Interfaces*.
2. Add the event, a `Switch on EUnitVariant`, and the modification per pin. Leave `Base` unconnected unless
   shared setup is wanted.
3. Add the unit's entry to the variant data asset with one `FUnitVariantDefinition` per implemented variant,
   index 0 being the base unit. Without this entry the runtime clamps every selection for that unit to Base,
   so a Blueprint variant with no asset entry is unreachable except through the debug list.
4. Do not change cost or training time in the event; those are read from the training data tables and the UI
   would show the wrong values.
5. Do not call `SetUnitSubtype` or `ChangeOwningPlayer` from the event.

Typical modifications and where they are safe:

| Modification | Tank | Squad controller | Squad unit | BXP |
|---|---|---|---|---|
| Swap a static/skeletal mesh or material | yes | – | yes | yes (building mesh; preview mesh is loaded separately, see `ABuildingExpansion` notes) |
| Change health / armour via the existing BP init functions | yes; the event fires at the same point techs are applied, after `BeginPlay_SetupData`. Anything a derived class sets after `Super::BeginPlay()` (`ATrackedTankMaster`: experience component, target acquisition) runs later | yes, units are loaded | yes | yes |
| Change weapon class of a turret | yes, through the turret's existing swap/init calls | team weapon gun: in the `ATeamWeapon` Blueprint's own event (5.2.1), not the controller's | – | yes |
| Change squad weapon mix / squad size | – | yes, `M_TSquadUnits` is complete at this point | – | – |
| Add an ability | through existing ability init functions only | same | – | same |

---

## 8. Implementation order

Each step compiles on its own. Build with the editor target after every step
(`Build.bat RTS_SurvivalEditor Win64 Development ...`, see the memory note "UE build command").

1. `Units/Variants/UnitVariantTypes.h`: `EUnitVariant`, `UnitVariants::MaxVariantsPerUnit`,
   `FUnitVariantSelection`, `Global_GetUnitVariantAsString`.
2. `Units/Variants/UnitVariantDataAsset.h/.cpp` with `IsDataValid`.
3. `Units/Variants/UnitVariantSettings.h/.cpp`. Confirm *Project Settings > Game > Unit Variants* appears.
4. `Interfaces/UnitVariantReceiver.h`.
5. `URTSComponent` additions (3.7). No callers yet; compile.
6. `UPlayerUnitVariantManager` + `ACPPController` wiring + `FRTS_Statics` getter.
7. Save data (section 4) and the `InitFromProfile` call in `UPlayerProfileLoader::LoadProfile` between
   `ApplyTechnologies` and `LoadInUnits`.
8. Hooks (section 5): tank, squad controller, squad unit, BXP. Add `IUnitVariantReceiver` to each class.
9. Cheat commands and debug forced list (section 6).
10. Tests (section 9). Create the data asset `DA_UnitVariants` next to the other settings assets, e.g.
    `/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/` holds `DA_TrainingLayout` and
    `/Game/RTS_Survival/Blueprints/GroundVehicles/ModuleSystem/` holds `DA_VehicleModules`; a sensible home is
    `/Game/RTS_Survival/Blueprints/Units/Variants/DA_UnitVariants`. Picking it writes a
    `[/Script/RTS_Survival.UnitVariantSettings]` block into `Config/DefaultGame.ini` like the
    `TrainingLayoutSettings` block at ~line 189. Both the asset and the Blueprint switch need the editor (or the
    UECP `create_asset` / Blueprint tools); they cannot be made from the filesystem. Add one test entry
    (e.g. `Tank_PzIV_H` with two variants) and a matching Blueprint switch that prints the variant.
11. Add the `Docs/README.md` row and the `Units/TeamWeapons/AGENTS.md` / `Player/AGENTS.md` notes if those
    files list owned components.

---

## 9. Tests

### 9.1 Automation tests (no world needed) — `Units/Variants/Tests/UnitVariantTests.cpp`

Use `IMPLEMENT_SIMPLE_AUTOMATION_TEST` with `EAutomationTestFlags::EditorContext | ProductFilter`; run with
`-DisablePlugins=Fab` (memory note "Automation tests need -DisablePlugins=Fab").

- Data asset: `NewObject<UUnitVariantDataAsset>` filled through a test-only `friend` or a `#if WITH_EDITOR`
  setter; assert `GetImplementedVariantCount` is 1 for a missing key, 2 for a two-element entry, 3 for a
  five-element entry (clamped), and `GetIsVariantImplemented(VariantB)` false for the two-element entry.
- Manager clamp: `NewObject<UPlayerUnitVariantManager>`, inject the asset through a test-only setter, call
  `InitFromProfile` with `{PzIV_H → VariantB}` where only two variants are implemented; assert
  `GetSelectedVariant` returns Base and the error reporter was hit (see `UnitTests/UT_ErrorReporting` for how existing tests capture
  `RTSFunctionLibrary::ReportError`).
- Owner gate: `ResolveVariantForUnit` with an `URTSComponent` whose owner is 2 returns Base even when the
  selection says VariantA.
- Supported types: Nomadic and Aircraft resolve to Base.

### 9.2 Map test (manual or `-UnitVariantValidate`)

A headless validate subsystem in the style of `Units/TeamWeapons/Tests/TeamWeaponQualityTestSubsystem` is
optional; the manual check is:

1. Set `DebugForcedVariants` to `{Tank PzIV_H → VariantA}`, `{Squad Ger_JagerTruppKar98k → VariantB}`,
   `{BXP <any Ger pit> → VariantA}`.
2. Run `RTS_ToCopySmall` (flat map, see memory note "Team weapon quality test"), train one of each, and confirm
   via the Blueprint's debug print that each unit received the forced variant exactly once.
3. Reinforce the squad; the new soldier prints VariantB.
4. Spawn the same tank for the enemy (enemy director / wave) and confirm it prints Base.
5. `RTSCheatSetUnitVariant` to Base at runtime; already-spawned units keep their print, newly trained ones
   print Base.

---

## 10. Extension points (not in this change)

- **Nomadic trucks**: add `TMap<ENomadicSubtype, FUnitVariantEntry> M_NomadicVariants` to the asset, handle
  `UNType_Nomadic` in `FindEntry` and `GetIsUnitTypeSupported`, and call `ApplyUnitVariant()` before
  `CheckForUpgrades()` at the end of `ANomadicVehicle::BeginPlay` (~line 362). Converted buildings re-pull on
  conversion automatically because the new actor begins play again.
- **Harvesters**: the Pz I harvester is an `ATankMaster` whose unit type is `UNType_Harvester` and whose subtype is an
  `ETankSubtype`. Add `UNType_Harvester` to `GetIsUnitTypeSupported` and to `FindEntry` (same tank map), and widen the
  `UNType_Tank` check in `ATankMaster::BeginPlay` to include it.
- **Aircraft**: same shape with `EAircraftSubtype`, hook before `CheckForUpgrades()` in
  `AAircraftMaster::BeginPlay` (~line 91).
- **Per-doctrine availability**: wrap the three maps in a struct keyed by `ERTSFaction` like
  `UTrainingLayoutDataAsset::FactionLayouts`, with a fallback to a shared entry.
- **Selection UI**: reads `UUnitVariantDataAsset::GetUnitsWithChoice()` for the list, `FindVariantDefinition`
  for name/icon/description, writes `FUnitVariantSelection` arrays into the profile (section 4). Portraits
  per variant can hook `URTSComponent::OnUnitVariantApplied`.
- **Per-unit choice at training time**: would replace `ResolveVariantForUnit`'s map lookup with a value the
  trainer passes; keep that in mind and do not spread direct `GetSelectedVariant` calls outside the manager.

---

## 11. Pitfalls for the implementer

- **Do not fire the event from Blueprint BeginPlay or from `URTSComponent::BeginPlay`.** The subtype is set
  by the Blueprint during BeginPlay; only the hook points in section 5 are after it.
- **Squad controller before `SetDataLoaded(true)`.** If `ApplyUnitVariant()` is placed after the broadcast,
  initial squad units read `Base` from the controller and only reinforcements get the variant.
- **Profile order.** `InitFromProfile` must precede `LoadInUnits` in `UPlayerProfileLoader::LoadProfile`, or
  the starting units get Base while trained units get the variant.
- **Map-placed player units** (actors placed in the level with owner 1) begin play before the profile loads
  and therefore resolve to Base. This matches how techs behave today; do not try to fix it here.
- **No manager is not an error.** Enemy-only test maps, the portrait capture tool and editor utility worlds
  have no `ACPPController`; `ApplyUnitVariant` falls back to Base silently.
- **Keep `FTrainingOption` as the runtime key.** Do not introduce a second key struct; the training UI, cards
  and spawner all speak `FTrainingOption`.
- **`TMap::operator[]` inserts.** Use `Find` in `GetSelectedVariant`; it runs for every unit that spawns.
- **Russian keys are an asset error**, not a runtime case. The owner check already prevents variants on enemy
  units; the validation exists so nobody authors variants the player can never use.
- **Idempotency.** `bM_HasAppliedUnitVariant` guards double application; never reset it on owner change.
- **Shipping builds** compile out `ApplyDebugForcedVariants`. Guard with `#if !UE_BUILD_SHIPPING`, not a
  runtime bool, so the config list cannot leak into a release.

---

## 12. Implementation notes and deviations from the brief

Implemented on 2026-10-19. Where the code differs from sections 3 to 9, this list is authoritative.

| Area | What the code does | Why |
|---|---|---|
| Russian key check (3.3) | Enumerator name contains `Rus` **or** display name starts with `Rus`. | Russian tanks (`Tank_T34_85`, `Tank_KV_1`, …) only carry `Rus` in the display name. |
| `IsDataValid` (3.3) | Shared rules live in the private static `ValidateEntry` / `GetIsRussianKey` (editor only). | One rule set for the three maps. |
| Asset caching (3.6) | `GetVariantDataAsset() const` does not write the `UPROPERTY`; the non-const paths call `CacheVariantDataAsset`. | No `mutable` UPROPERTY. |
| Clamp path (3.6) | `InitFromProfile` and `SetSelectedVariant` share the private `StoreSelectionClamped`; selecting `Base` removes the key. | The map only holds non-Base choices. |
| Debug forced list (3.6, 6.2) | Not clamped against the asset; only `GetIsUnitTypeSupported` is checked. | Section 7 step 3: an unlisted Blueprint variant must be testable through this list. |
| Profile order (4, 3.6) | `LoadProfile` calls the new private `UPlayerProfileLoader::ApplyUnitVariantSelections` after `ApplyTechnologies` and **before** the `bDoNotLoadPlayerUnits` early return. | Otherwise the no-units path never initialises the manager. |
| Save validation (4) | `FRTS_Profile::ValidateUnitVariantSelections` (private static) runs in `ProfileValidator`, and also in `BuildPlayerCardSaveData` and `SaveNewPlayerProfile`. | Both save paths get the same dedupe / unsupported-type cleanup. |
| `ApplyUnitVariantFromParent` (3.7) | Guarded by `bM_HasAppliedUnitVariant` like `ApplyUnitVariant`. | The interface promises the event fires once. |
| Helper access (5) | `ATankMaster::ApplyUnitVariant` is private; `ASquadController::ApplyUnitVariant` and `ABuildingExpansion::ApplyUnitVariant` are protected, next to their `CheckForUpgrades`. | `SquadController.h` has no private section; the BXP helper sits with `CheckForUpgrades`. |
| `ACPPController::GetIsValidPlayerUnitVariantManager` (3.6) | Declared and defined (const, reports via `ReportErrorVariableNotInitialised`), but nothing in the controller calls it yet. It does not recreate the component the way `GetIsValidPlayerTechManager` does. | Kept to the brief's wording; callers use `FRTS_Statics::GetPlayerUnitVariantManager`. |
| Unity build | `Units/TeamWeapons/Tests/TeamWeaponDigInTestSubsystem.cpp` got an explicit `InfantryWeaponMaster.h` include. | The new .cpp files reshuffled the unity groups and exposed a missing include there. |
| `Player/AGENTS.md` (8 step 11) | Not changed. | It lists orchestration rules, not owned components. `Units/TeamWeapons/AGENTS.md` got a short "Unit variants" section. |
| Tests (9.1) | `Units/Variants/Tests/UnitVariantTests.cpp`, tests under `RTS.UnitVariants.*`. Private state is reached through `friend struct FUnitVariantTestAccess` (declared in the asset and the manager). The clamp test wraps `InitFromProfile` in `RTSFunctionLibrary::Push/PopModalDialogSuppression` and expects the `LogRTS` error. | No editor-only setters in shipping headers. |

Run the tests headless (does not touch an open editor):

```
UnrealEditor-Cmd.exe D:\UE5Projects\RTS_Survival\RTS_Survival.uproject -ExecCmds="Automation RunTests RTS.UnitVariants; Quit" -unattended -nullrhi -nosplash -DisablePlugins=Fab -log
```

The manager clamp test also runs `ApplyDebugForcedVariants`, so a non-empty `DebugForcedVariants` list in
`Config/DefaultGame.ini` can make it fail; clear the list before running the tests.
