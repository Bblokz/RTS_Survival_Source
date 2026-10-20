# Unit variants

The player's army customisation layer below unit cards. A card swaps a whole unit; a variant is a small
modification of a unit (a different gun, extra armour plates, a different squad weapon mix, a different turret
on a pit). Every German unit has at most three variants, of which the first is always the base unit. The player
picks one variant per unit type for the whole army before a mission; every unit of that type the player
receives during the mission then **is** that variant.

**A variant is a Blueprint child class of the base unit Blueprint.** The player's selection replaces the class
the `ARTSAsyncSpawner` spawns for that unit type, using the same mapping-override mechanism the enemy director
already uses to swap unit classes for the Director of Forge and the Director of Harvest. There is no runtime
event, no interface, and no per-unit lookup: the unit that comes out of the spawner is the variant.

This document is the implementation brief, and it **replaces an earlier design** that was implemented and
committed (units pulled their variant at BeginPlay and fired a Blueprint event). Section 8 lists, symbol by
symbol, what of that implementation is kept, changed and removed. Read section 8 before touching code.

The variant *selection UI* is out of scope. Everything that lets the selection reach the spawner, and
everything the UI will need to know which units have variants to choose from, is in scope.

| Topic | Decision |
|---|---|
| What a variant is | A Blueprint child class of the base unit Blueprint (`BP_PzIV_H_VariantA` is a child of `BP_PzIV_H`). It overrides defaults: meshes, weapons, modules, abilities, squad unit classes, team weapon class, building mesh. |
| Choice model | One exclusive choice per unit type, from the shared enum `EUnitVariant { Base, VariantA, VariantB }`. Not stackable. |
| How the choice reaches units | The spawner's class maps are overridden at mission start, exactly like `ARTSAsyncSpawner::ApplyEnemyDirectorUnitOverrides`. |
| Supported unit kinds | Tanks (`M_TrainingOptionMap` keys with `UNType_Tank`), squads (`UNType_Squad`, which includes team weapon crews since they are squads), building expansions (`BuildingExpansionMap`). Nomadic trucks and aircraft are not supported yet; section 13 shows the extension. |
| Scope | Only units that come out of `ARTSAsyncSpawner`. Anything spawned another way gets whatever class spawns it; see section 6. |
| Who gets variants | The player, and only the player, without any owner check. A faction is always unique to one player, so the spawner never spawns a unit of one player's faction for another player: German subtypes (the only keys variants override) are only ever requested for the player. No spawn path checks the requester, the trainer component's owner or the spawned unit's owner; see section 6. |
| Persistence | The selection is saved next to the cards in the player profile (already in place). |
| Scope of a selection change | Units requested after the change. Already spawned units keep their class. |
| Variant definition store | `UUnitVariantDataAsset` picked in Project Settings > Unit Variants (already in place), extended with the variant class per entry. |
| Subtype | **All variants of a unit share that unit's one subtype.** Three Pz II F Blueprints are all `Tank_PzII_F`. No variant ever gets its own `ETankSubtype` / `ESquadSubtype` / `EBuildingExpansionType`; see "One subtype per unit" below. |
| Cost / training time / requirements | Identical for every variant, because they are keyed by the shared subtype (`FTrainingOption`, `DT_Training` row). This is intended. |
| Training UI presentation | While a variant is selected, the training menu can show that variant's **icon, title and description** instead of the base unit's. Each of the three is opt-in per variant with its own bool on the variant data asset entry; off means the base unit's table entry is shown. The values come from the variant data asset, never from `DT_Training` / `DT_TrainingOptions`. See 3.8. |
| Per doctrine | No. The implemented variants are per subtype, not per `ERTSFaction`. |
| Stale saves | A saved variant the asset no longer implements clamps to `Base` and reports an error once (already in place). |
| Precedence vs director overrides | Director overrides apply first, variants second. A variant wins on the same key; the collision is reported. |
| Testing before the UI exists | Cheat manager commands plus the developer-settings forced-variant list (already in place, adapted). |

### One subtype per unit, shared by all its variants

This is a hard rule. Read it before designing anything.

- A unit type is identified by its `FTrainingOption` (unit type + subtype). **Variants do not change that
  identity.** `BP_PzII_F`, `BP_PzII_F_VariantA` and `BP_PzII_F_VariantB` all call `SetUnitSubtype` with
  `Tank_PzII_F` (the children inherit it from the base Blueprint and must not override it).
- **Do not add enumerators** to `ETankSubtype`, `ESquadSubtype` or `EBuildingExpansionType` for variants (no
  `Tank_PzII_F_VariantA`), and do not add `DT_Training` rows, training layout slots, cards, spawner setup rows or
  `GameState` unit data per variant.
- What tells variants apart is **only** `EUnitVariant`: the player's selection (keyed by the shared
  `FTrainingOption`), the variant data asset entry for that subtype (class, name, description, icon), and the
  `UnitVariant` class default on the variant Blueprint's `URTSComponent` (3.7).
- Everything keyed by the subtype is therefore shared by all variants, and that is the intended behaviour:
  - cost, training time and training requirements (`DT_Training` row named by `GetTrainingOptionName`,
    `FTrainingOptionState::RTSRequirement` in `TrainingMenuManager.cpp` ~425);
  - the training menu slot and its position in the layout (`UTrainingLayoutDataAsset`);
  - unit data from the game state (`ACPPGameState::GetTankDataOfPlayer` / `GetSquadDataOfPlayer`: health,
    speed, vision, resistances, abilities), which the variant Blueprint can still adjust on top like any
    Blueprint;
  - technologies that target the subtype, unit cards (`FCardSaveData::UnitToCreate`), the game unit manager's
    per-subtype counts, enemy director keys and the saved selection itself.
- The **only** per-variant exception the player sees in the training menu is presentation: icon, title and
  description, each opt-in (3.8). They are read from the variant data asset by the training menu, keyed by the
  shared `FTrainingOption` plus the selected `EUnitVariant`. Cost, training time and requirements in that same
  menu stay the shared ones.
- Consequence for the spawner: there is exactly **one key per unit** in `M_TrainingOptionMap` /
  `BuildingExpansionMap`, and applying a variant **replaces the class under that key**. It never adds a key.
- If a future variant genuinely needs a different cost or different requirements, that is a different unit (a
  new subtype, a new training slot), not a variant.

## 1. The precedent: enemy director unit overrides

Read this path end to end before implementing; the variant path is a copy of it.

| Step | Where |
|---|---|
| The spawner's Blueprint holds `TArray<FTankTrainingOptionSetup> SetupLightGerVehicles` and friends (one row = subtype + `TSoftClassPtr<AActor> UnitClass`), plus `TMap<EBuildingExpansionType, TSoftClassPtr<ABuildingExpansion>> BuildingExpansionMap` and `TMap<EBuildingExpansionType, UStaticMesh*> BxpPreviewMeshMap`. | `Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h` ~160–210 |
| `PostInitializeComponents` rebuilds the private `TMap<FTrainingOption, TSoftClassPtr<AActor>> M_TrainingOptionMap` from those arrays (`SetupTrainingOptionsFromConfig`, `AddTrainingOptionMapping`). Tanks are keyed `FTrainingOption(UNType_Tank, TankSubtype)`, squads `UNType_Squad`, nomadics `UNType_Nomadic`, aircraft `UNType_Aircraft`. | `RTSAsyncSpawner.cpp` 86–130, 195–214 |
| Every spawn request looks the class up by `FTrainingOption` at request time: `AsyncSpawnOptionAtLocation` (~652) and `LoadNextTrainingOptionInQueue` (~737) read `M_TrainingOptionMap`; `AsyncSpawnBuildingExpansion` (~328), `AsyncBatchLoadInstantPlaceExpansions` (~928) and `AsyncSpawnFieldDefenseExpansion` (~1057) read `BuildingExpansionMap`. | `RTSAsyncSpawner.cpp` |
| `ACPPController::InitPlayerController` stores the spawner, calls `ApplyEnemyDirectorUnitOverrides()`, then `InitPlayerStartLocation`, which leads to `LoadPlayerProfile` → `UPlayerProfileLoader::LoadProfile` → `LoadInUnits` (starting units). | `Player/CPPController.cpp` ~2145–2150 |
| `ACPPController::ApplyEnemyDirectorUnitOverrides` asks the mission manager for the `EEnemyDirector` and calls `ARTSAsyncSpawner::ApplyEnemyDirectorUnitOverrides(Director)`. | `CPPController.cpp` ~2172 |
| The spawner reads `UEnemyDirectorUnitOverrideSettings` (a `UDeveloperSettings` with a `TSoftObjectPtr` to the asset) → `UEnemyDirectorUnitOverridesDataAsset::FindOverrides(Director)` → `FEnemyDirectorUnitOverrides` (four typed arrays) → `ApplyTrainingOptionOverride(FTrainingOption, TSoftClassPtr<AActor>)`, which **replaces only an existing key** and returns silently when the key is absent. Guarded by `bM_HasAppliedEnemyDirectorUnitOverrides`. | `Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner_EnemyDirectorOverrides.cpp`, `Enemy/EnemyDirector/EnemyDirectorUnitOverrideSettings.h`, `EnemyDirectorUnitOverridesDataAsset.h/.cpp` |

Three things the director path does **not** do, which variants need: it never overrides `BuildingExpansionMap`
or `BxpPreviewMeshMap`; it never remembers the class a key had before the override, so nothing can be
restored; and a missing base key is silently fine (the director asset is sparse by design). Section 3.4 adds
these.

---

## 2. Runtime flow

```
Pre-mission (later UI)            Mission start (ACPPController::InitPlayerController)
┌───────────────────────┐   ┌──────────────────────────────────────────────────────────────┐
│ player picks a variant │   │ 1. ApplyEnemyDirectorUnitOverrides()  (existing)             │
│ per unit type          │──►│ 2. InitPlayerStartLocation → LoadPlayerProfile →             │
│ → FUnitVariantSelection│   │    UPlayerProfileLoader::LoadProfile:                        │
│   saved in the profile │   │      ApplyResourceCards, ApplyTechnologies,                  │
└───────────────────────┘   │      ApplyUnitVariantSelections  → manager clamps selection   │
                            │        → ARTSAsyncSpawner::ApplyPlayerUnitVariants(...)       │
                            │           replaces M_TrainingOptionMap / BuildingExpansionMap  │
                            │           entries with the variant classes from the asset     │
                            │      LoadInUnits (starting units now spawn as variants)       │
                            └──────────────────────────────────────────────────────────────┘
Any later spawn request through the spawner (trainer, mission, dropship, field defense, instant BXP batch)
reads the overridden map and spawns the variant class. Nothing in the unit knows or cares.
```

---

## 3. Types

Paths are under `RTS_Survival/`. Follow `AGENTS.md` (early returns, `not`, `M_` prefix, validator functions,
Doxygen on classes, braces on their own lines).

### 3.1 `EUnitVariant`, `FUnitVariantSelection` — `Units/Variants/UnitVariantTypes.h` (exists, one struct added)

`EUnitVariant { Base, VariantA, VariantB }`, `UnitVariants::MaxVariantsPerUnit = 3`,
`Global_GetUnitVariantAsString`, and `FUnitVariantSelection { FTrainingOption Unit; EUnitVariant Variant; }`.
Keep as committed. Add the struct the training menu passes to its widgets (3.8); it lives here so
`TrainingWidgetState.h` can include it without pulling in the data asset:

```cpp
/** What the training UI shows instead of the base unit's table entries while a variant is selected. */
USTRUCT(BlueprintType)
struct FUnitVariantTrainingPresentation
{
	GENERATED_BODY()

	// The selected variant; Base when the player kept the base unit, and then every override below is off.
	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	EUnitVariant Variant = EUnitVariant::Base;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	bool bOverrideIcon = false;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	bool bOverrideTitle = false;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	bool bOverrideDescription = false;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	FText Description;
};
```

Keys for the three supported kinds:

| Kind | `FTrainingOption` key |
|---|---|
| Tank | `(UNType_Tank, static_cast<uint8>(ETankSubtype))`. The spawner's setup arrays key **every** tank subtype under `UNType_Tank` (`AddTrainingOptionsForTankArray`), and the field defense test spawns the harvester as `(UNType_Tank, Tank_PzI_Harvester)` (`FieldDefenseTestSubsystem.cpp:98`). The training menu, however, lists the harvester under `UNType_Harvester` (`TrainingMenuManager.cpp` ~506); where that is translated before the spawn request was not traced. The manager therefore normalises `UNType_Harvester` to `UNType_Tank` when storing a selection (3.5), and the implementer confirms once in the map test that a harvester variant resolves (a wrong key shows up as the spawner's "not found in the map" error). |
| Squad / team weapon crew | `(UNType_Squad, static_cast<uint8>(ESquadSubtype))` |
| Building expansion | `(UNType_BuildingExpansion, static_cast<uint8>(EBuildingExpansionType))`. The spawner translates this to a `BuildingExpansionMap` key. |

### 3.2 `UUnitVariantDataAsset` — `Units/Variants/UnitVariantDataAsset.h/.cpp` (exists, extended)

Keep the committed shape (three typed maps `M_TankVariants`, `M_SquadVariants`, `M_BuildingExpansionVariants`
of `FUnitVariantEntry { TArray<FUnitVariantDefinition> Variants; }`, index 0 = Base, `FindEntry`,
`GetImplementedVariantCount`, `GetIsVariantImplemented`, `FindVariantDefinition`, `GetUnitsWithChoice`,
`IsDataValid` with the shared `ValidateEntry` / `GetIsRussianKey`). Add to `FUnitVariantDefinition`:

```cpp
	// The Blueprint child class spawned for this variant. Must stay null on element 0 (Base): the base class is the
	// one the spawner's own setup holds, so the asset never duplicates it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftClassPtr<AActor> UnitClass;

	// Building expansions only: the placement preview mesh of this variant. Null keeps the base preview mesh.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UStaticMesh> BxpPreviewMesh;

	// Training UI (3.8). Each override is opt-in; off shows the base unit's DT_TrainingOptions / DT_Training entry.
	// DisplayName / Description / Icon above are for the later variant selection UI and are not used here, so the
	// selection UI can say "Long 75 mm gun" while the training menu says "Pz IV H (L/48)".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI")
	bool bOverrideTrainingIcon = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI", meta = (EditCondition = "bOverrideTrainingIcon"))
	TSoftObjectPtr<UTexture2D> TrainingIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI")
	bool bOverrideTrainingTitle = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI", meta = (EditCondition = "bOverrideTrainingTitle"))
	FText TrainingTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI")
	bool bOverrideTrainingDescription = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI",
		meta = (EditCondition = "bOverrideTrainingDescription", MultiLine = "true"))
	FText TrainingDescription;
```

New accessors:

```cpp
	/** @return The class of a non-base variant, or a null soft pointer for Base, a missing entry or a missing class. */
	TSoftClassPtr<AActor> FindVariantClass(const FTrainingOption& Unit, const EUnitVariant Variant) const;

	/** @return The preview mesh of a building expansion variant, null when the variant keeps the base preview. */
	TSoftObjectPtr<UStaticMesh> FindBxpVariantPreviewMesh(const EBuildingExpansionType BxpType, const EUnitVariant Variant) const;

	/**
	 * @brief What the training menu shows for this unit while Variant is selected (3.8).
	 * @return All overrides off for Base, a missing entry or an unimplemented variant.
	 */
	FUnitVariantTrainingPresentation GetTrainingPresentation(const FTrainingOption& Unit, const EUnitVariant Variant) const;
```

`GetTrainingPresentation` copies each override only when its bool is on (and, for the icon, the soft pointer is
not null); `Variant` in the result is always the requested variant so a Blueprint can show a variant badge even
when no override is on.

`IsDataValid` additions (editor only, so `LoadSynchronous` is fine there), all reported as errors:

- Element 0 has a non-null `UnitClass`.
- Element 1 or 2 has a null `UnitClass` (an entry without a class is not implemented; shorten the array instead).
- A class whose native parent is wrong for its map: tank map classes must `IsChildOf(ATankMaster::StaticClass())`,
  squad map classes `ASquadController`, building expansion map classes `ABuildingExpansion`.
- The same class used by two definitions anywhere in the asset.
- `BxpPreviewMesh` set on a tank or squad definition (warning, it is ignored there).
- Any `bOverrideTraining*` on element 0 (Base always shows the base unit's table entries).
- `bOverrideTrainingIcon` on with a null `TrainingIcon`, `bOverrideTrainingTitle` on with an empty
  `TrainingTitle`, `bOverrideTrainingDescription` on with an empty `TrainingDescription`.

`GetImplementedVariantCount` stays "array length clamped to 1..3": an entry with elements that fail validation
is a data error, not a runtime case.

### 3.3 `UUnitVariantSettings` — `Units/Variants/UnitVariantSettings.h/.cpp` (exists, unchanged)

`VariantDataAsset` (`TSoftObjectPtr<UUnitVariantDataAsset>`, falls back to class defaults) and
`DebugForcedVariants` (`TArray<FUnitVariantSelection>`, ignored in Shipping). Keep, but its semantics change:
the committed version bypasses the asset clamp "so unlisted Blueprint variants can be tested". Under class
swap an unlisted variant has no class to swap to, so bypassing only produces a "no class" error every mission
start. Forced entries are now clamped exactly like saved ones; rewrite the header comment accordingly.

### 3.4 `ARTSAsyncSpawner` additions — `Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h` and a new `RTSAsyncSpawner_UnitVariants.cpp`

Mirror the director override file. Public:

```cpp
	/**
	 * @brief Replaces the spawn class of every selected unit type with its variant class from the variant data asset.
	 * Runs after the director overrides so a variant wins on a shared key; the collision is reported.
	 * @param Selections Clamped selections (Base entries are skipped).
	 */
	void ApplyPlayerUnitVariants(const TArray<FUnitVariantSelection>& Selections);

	/**
	 * @brief Changes one unit type at runtime (cheat manager, future UI). Base restores the class the mapping held
	 * before the first variant override, so a mission can go back and forth.
	 * @return False when the unit type has no base mapping or the variant has no class in the asset.
	 */
	bool SetPlayerUnitVariant(const FTrainingOption& Unit, const EUnitVariant Variant);

	/** @return The class currently mapped for the training option (after overrides), or a null soft pointer. */
	TSoftClassPtr<AActor> GetTrainingOptionClass(const FTrainingOption& TrainingOption) const;
```

Private:

```cpp
	bool ApplyTrainingOptionVariant(const FTrainingOption& Unit, const EUnitVariant Variant);
	bool ApplyBuildingExpansionVariant(const EBuildingExpansionType BxpType, const EUnitVariant Variant);

	// The class (and BXP preview mesh) each key held before its first variant override; restored for Base.
	TMap<FTrainingOption, TSoftClassPtr<AActor>> M_PreVariantTrainingOptionClasses;
	TMap<EBuildingExpansionType, TSoftClassPtr<ABuildingExpansion>> M_PreVariantBuildingExpansionClasses;
	UPROPERTY()
	TMap<EBuildingExpansionType, TObjectPtr<UStaticMesh>> M_PreVariantBxpPreviewMeshes;

	// Keys the director overrides replaced; a variant on one of these is applied but reported as a collision.
	TSet<FTrainingOption> M_DirectorOverriddenOptions;
```

Rules:

- `ApplyPlayerUnitVariants` loops the selections, skips `Variant == Base`, and dispatches on `Unit.UnitType`:
  `UNType_Tank` / `UNType_Squad` → `ApplyTrainingOptionVariant`, `UNType_BuildingExpansion` →
  `ApplyBuildingExpansionVariant` with `static_cast<EBuildingExpansionType>(Unit.SubtypeValue)`, anything else →
  `ReportError` and continue. It is not guarded by a once-flag: the cheat path calls `SetPlayerUnitVariant`,
  which uses the same two functions, so repeated application must be safe (it is, because the pre-variant
  maps are only written when the key is absent from them).
- `ApplyTrainingOptionVariant`:
  1. `TSoftClassPtr<AActor>* Existing = M_TrainingOptionMap.Find(Unit)`. Null → `ReportError` "no base
     mapping for <GetTrainingName>" and return false. **Do not** reuse `ApplyTrainingOptionOverride`; it
     returns silently on a missing key and its error strings say "Enemy director".
  2. If `not M_PreVariantTrainingOptionClasses.Contains(Unit)`, add `*Existing` under that key. Never
     overwrite a present entry.
  3. `Variant == Base` → `*Existing = M_PreVariantTrainingOptionClasses.FindChecked(Unit)` (the key was added
     in step 2), return true.
  4. Otherwise `Class = Asset->FindVariantClass(Unit, Variant)`; null → `ReportError`, return false.
  5. If `M_DirectorOverriddenOptions.Contains(Unit)` → `ReportError` "variant <X> replaces a director override
     for <unit>" and continue anyway.
  6. `*Existing = Class`, return true.
  The pointer from `Find` is used in the same function and `M_TrainingOptionMap` is not mutated in between
  (rule 22 of `AGENTS.md`); the pre-variant map is a different container.
- `ApplyBuildingExpansionVariant`: same steps against `BuildingExpansionMap` with
  `TSoftClassPtr<ABuildingExpansion>` (build it from the asset's `TSoftClassPtr<AActor>` through
  `ToSoftObjectPath()`), plus the preview mesh: remember the `BxpPreviewMeshMap` value in
  `M_PreVariantBxpPreviewMeshes` the same way, then if `FindBxpVariantPreviewMesh` is non-null,
  `LoadSynchronous()` it and write it into `BxpPreviewMeshMap` (the map holds hard pointers and
  `SyncGetBuildingExpansionPreviewMesh` reads it synchronously at placement time); Base restores the remembered
  mesh.
- `ApplyTrainingOptionOverride` (director path) gets one extra line: `M_DirectorOverriddenOptions.Add(TrainingOption)`
  after a successful replacement.
- The variant data asset is read through `UUnitVariantSettings::Get()->LoadVariantDataAsset()` at the top of
  `ApplyPlayerUnitVariants` / `SetPlayerUnitVariant`; store nothing on the spawner.
- No runtime class-hierarchy check. The asset's `IsDataValid` is the gate (3.2).

### 3.5 `UPlayerUnitVariantManager` — `Player/PlayerUnitVariantManager/` (exists, repurposed)

It stays the **selection store** on `ACPPController` (created in the constructor next to the tech manager,
`GetPlayerUnitVariantManager()`, `FRTS_Statics::GetPlayerUnitVariantManager`). Keep: `InitFromProfile` (clamps
through `StoreSelectionClamped`, applies `DebugForcedVariants`), `GetSelectedVariant`, `SetSelectedVariant`,
`GetSelectionsForSave`, `GetVariantDataAsset`, `GetIsUnitTypeSupported` (Tank, Squad, BuildingExpansion), the
test friend access.

Change: `StoreSelectionClamped` normalises `UNType_Harvester` to `UNType_Tank` before anything else (the
spawner has no harvester keys, see 3.1), and `ApplyDebugForcedVariants` goes through the same clamp as saved
selections (3.3).

Remove: `ResolveVariantForUnit` and every mention of owners. Units no longer ask the manager anything.

Add:

```cpp
	/**
	 * @brief Pushes the stored selection into the spawner's class maps and remembers the spawner so later
	 * SetSelectedVariant calls (cheats, future UI) reach it too.
	 * @param AsyncSpawner The mission's spawner; must already hold its base mappings and the director overrides.
	 */
	void ApplySelectionsToSpawner(ARTSAsyncSpawner* AsyncSpawner);

private:
	bool GetIsValidAsyncSpawner() const;

	UPROPERTY()
	TWeakObjectPtr<ARTSAsyncSpawner> M_AsyncSpawner;
```

`SetSelectedVariant` keeps its clamp, then, when `M_AsyncSpawner` is valid, calls
`M_AsyncSpawner->SetPlayerUnitVariant(Unit, Variant)` (also for `Base`, which restores). `ApplySelectionsToSpawner`
builds the array from `M_SelectedVariants` and calls `ApplyPlayerUnitVariants`.

Also add `GetTrainingPresentation(const FTrainingOption& Unit) const` for the training menu (3.8).

### 3.6 `UPlayerProfileLoader::ApplyUnitVariantSelections` (exists, extended)

Already called in `LoadProfile` after `ApplyTechnologies` and before the `bDoNotLoadPlayerUnits` early return
(`PlayerProfileLoader.cpp` ~58). Its body calls `InitFromProfile`; add
`PlayerUnitVariantManager->ApplySelectionsToSpawner(AsyncSpawner)` right after it (pass the spawner through from
`LoadProfile`, which already has it as a parameter). Keep the call before the early return: a map without
starting units still trains units later and needs the overrides.

### 3.7 `URTSComponent` (exists, simplified)

Replace the runtime-applied state with a class default the variant Blueprint sets:

```cpp
protected:
	// Set in the class defaults of a variant child Blueprint; the base Blueprint leaves it on Base. Read by UI and
	// portraits; nothing in gameplay branches on it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RTSComponent")
	EUnitVariant UnitVariant = EUnitVariant::Base;

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unit Variant")
	EUnitVariant GetUnitVariant() const { return UnitVariant; }
```

Remove `M_UnitVariant`, `bM_HasAppliedUnitVariant`, `GetHasAppliedUnitVariant`, `OnUnitVariantApplied`,
`FOnUnitVariantApplied`, `ApplyUnitVariant`, `ApplyUnitVariantFromParent`, `ApplyUnitVariant_Internal`.

### 3.8 Training UI: variant icon, title and description (new)

Goal: while the player has a variant selected for a unit, its training menu slot, its hover description and its
requirement panel can show the variant's icon, title and description. Each is opt-in on the variant definition
(3.2: `bOverrideTrainingIcon`, `bOverrideTrainingTitle`, `bOverrideTrainingDescription`). Cost, training time and
requirements are **not** overridable; they stay the shared ones ("One subtype per unit").

How the training UI gets its data today (traced):

- `UTrainingMenuManager::CreateWidgetStateForOption` (`TrainingMenuManager.cpp` ~3020) builds one
  `FTrainingWidgetState` per shown slot, called from `UpdateWidgetsWithShownSubSection` (~3016) on every
  refresh. It is the **only** place a `FTrainingWidgetState` is built from a training option.
- That state reaches the three Blueprint events that draw a training option:
  `UW_TrainingItem::OnUpdateTrainingItem` (the slot), `UW_TrainingDescription::OnShowTrainingDescription` (the
  hover panel, via `OnTrainingItemHovered` → `UMainGameUI::SetBottomUIPanel`) and
  `UW_TrainingRequirement::OnUpdateRequirement`.
- `FTrainingWidgetState` (`GameUI/TrainingUI/TrainingUIWidget/TrainingItemState/TrainingWidgetState.h`) carries
  no icon or text: the Blueprints look them up by `ItemID` (`DT_TrainingOptions` for the icon, keyed by
  `GetTrainingOptionName`; the title and description lookup inside `W_TrainingItem` / `W_TrainingDescription`
  was not traced, it is in their graphs). The cost panel (`UW_TrainingDescription::CostDisplay`) is filled from
  C++ by `ItemID` and stays as is.

C++ changes:

1. `FTrainingWidgetState` gets one field, filled for every slot:
   ```cpp
   	// Variant overrides for this slot; all off when the player keeps the base unit.
   	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
   	FUnitVariantTrainingPresentation VariantPresentation;
   ```
2. `UPlayerUnitVariantManager` gets
   `FUnitVariantTrainingPresentation GetTrainingPresentation(const FTrainingOption& Unit) const`: normalise
   `UNType_Harvester` to `UNType_Tank` (the training menu lists the harvester as `UNType_Harvester`, the
   selection is stored as a tank, see 3.1), then `GetVariantDataAsset()->GetTrainingPresentation(Unit,
   GetSelectedVariant(Unit))`. It reports nothing; it runs on every menu refresh.
3. `CreateWidgetStateForOption` sets `WidgetState.VariantPresentation` through a private helper of
   `UTrainingMenuManager` that gets the manager with `FRTS_Statics::GetPlayerUnitVariantManager(this)`; no
   manager (menus outside a mission) leaves the default (Base, all off). The training menu is the player's
   own UI, so no owner check (section 6).
4. A runtime selection change (cheat, future UI) shows on the next menu refresh, since the state is rebuilt on
   every `UpdateWidgetsWithShownSubSection`. `RTSCheatSetUnitVariant` may force a refresh of the shown sub
   section; nothing else needs to.

Blueprint changes (editor work, section 9 step 8):

- `W_TrainingItem` `OnUpdateTrainingItem`: if `VariantPresentation.bOverrideIcon`, set the item image from
  `VariantPresentation.Icon` (soft texture; *Set Brush from Soft Texture*), else keep the `DT_TrainingOptions`
  lookup. If the slot shows a name, use `Title` when `bOverrideTitle`.
- `W_TrainingDescription` `OnShowTrainingDescription`: use `Title` / `Description` when their bools are on, the
  existing lookup otherwise; the icon too if the panel shows one.
- `W_TrainingRequirement` `OnUpdateRequirement`: if it shows the hovered unit's name or icon, apply the same
  rule. The names of *required* units (`RequiredUnit`, `SecondaryRequiredUnit`) stay the base names.

Not covered (base presentation stays): quick navigation buttons (they show buildings, which have no variants),
the control group image (`UW_ControlGroupImage::OnUpdateControlGroup`, keyed by `FTrainingOption`), the selected
unit info panel, cards. A spawned unit's own UI can read `URTSComponent::GetUnitVariant()` (3.7) and the asset
if that is wanted later.

---

## 4. Save data (exists, unchanged)

`USavePlayerProfile::M_UnitVariantSelections` and `FPlayerCardSaveData::UnitVariantSelections`
(`TArray<FUnitVariantSelection>`, `SaveGame`), the defaulted trailing parameters on `USavePlayerProfile::Initialize`,
`FRTS_Profile::BuildPlayerCardSaveData` and `FRTS_Profile::SaveNewPlayerProfile`, and
`FRTS_Profile::ValidateUnitVariantSelections` (dedupe per unit, drop unsupported types; runs in
`ProfileValidator` and both save paths). `W_CardMenu` passes an empty array until the UI exists. Empty means
"all Base", so old saves need no migration.

---

## 5. Ordering and precedence

1. `ARTSAsyncSpawner::PostInitializeComponents` builds the base maps from the Blueprint arrays.
2. `ACPPController::InitPlayerController` → `ApplyEnemyDirectorUnitOverrides()` (Forge / Harvest only).
3. `InitPlayerStartLocation` → … → `UPlayerProfileLoader::LoadProfile` → `ApplyUnitVariantSelections` →
   `InitFromProfile` (clamp, debug list) → `ApplySelectionsToSpawner` → `ApplyPlayerUnitVariants`.
4. `LoadInUnits` spawns the starting units: the first requests that see the variant classes.
5. Everything later (trainer queue, missions, dropships, field defenses, instant BXP batches, enemy waves)
   reads the same maps.

A variant on a key the director also overrode replaces the director's class and reports the collision. The
director asset is expected to hold only Russian keys; check it in the editor rather than relying on that, the
collision report exists for exactly this case.

`InitFromProfile` runs once per mission. Runtime changes go through `SetSelectedVariant`, which reaches the
spawner through `SetPlayerUnitVariant`; spawn requests already queued or loading keep the class they resolved
at request time (`AsyncSpawnOptionAtLocation` copies the soft class before loading).

---

## 6. What is covered, what is not

Covered because it goes through the spawner's maps (verified call sites):

- Trainer buildings: `UTrainerComponent` → `AsyncSpawnTrainingOption` (~1408) and
  `AsyncSpawnFieldDefenseExpansion` (~422).
- Starting units: `UPlayerProfileLoader::LoadInUnits` → `AsyncSpawnOptionAtLocation` (~142).
- Missions: `UMissionBase::AsyncSpawnActor*`, `MissionManager`, the cargo-with-vehicle and tow-team-weapon
  spawn states.
- Global abilities: `GA_AircraftDrop` → `AsyncSpawnOptionAtLocation`.
- Building expansions: `ACPPController::AsyncSpawnBuildingExpansion` calls (~1820, ~1910) and
  `IBuildingExpansionOwner` instant placement (`BuildingExpansionOwner.cpp` ~188).
- Squad soldiers and reinforcements: `ASquadController::LoadSquadUnitsAsync` and
  `USquadReinforcementComponent` (~133, ~651) spawn the classes listed in the controller's `SquadUnitClasses`.
  A variant squad controller Blueprint lists its own soldier classes, so soldier changes are "a different
  soldier Blueprint in that list", not a runtime swap.
- Team weapon guns: `ATeamWeaponController::SpawnTeamWeapon` spawns the controller's `TeamWeaponClass`
  (`TeamWeaponController.h` ~559), so a variant crew Blueprint sets its own gun class.
- Post-spawn casts still pass because a variant is a child class: `Cast<IRTSUnit>` in
  `OnTrainingOptionSpawned`, `Cast<ABuildingExpansion>` in `OnBuildingExpansionSpawned` (~469) and
  `BatchBxp_SpawnExpansionActor` (~1031).

No owner check anywhere, by design:

- **The enemy shares the spawner but never shares a faction.** `EnemyController` (~467), `EnemyWaveController`
  (~801), `EnemyStrategicAIComponent` (~954), enemy `UTrainerComponent`s (registered on the strategic AI
  blackboard, `EnemyController.cpp` ~776) and enemy-owned `IBuildingExpansionOwner`s use the same spawner and
  the same maps. That is safe because a faction is always unique to one player: the spawner never spawns units
  of a different faction for another player, so the enemy never requests a German subtype and never meets a
  variant class.
- **Therefore do not check who owns the trainer component** in `AsyncSpawnTrainingOption` /
  `LoadNextTrainingOptionInQueue`, do not add an owner or "is player request" parameter to
  `AsyncSpawnOptionAtLocation`, and do not gate the BXP paths by owner. The override on the shared map is the
  whole mechanism.

Out of scope, because it does not spawn through the spawner's maps:

- Units placed in the level by hand (they begin play before anything is overridden).
- Nomadic vehicle conversion to and from buildings (`NomadicVehicle.cpp`, its own class properties).
- The replacement controller when a squad re-crews an abandoned team weapon
  (`ASquadController::TryRemanAbandonedTeamWeapon_*`, class resolved there, `SquadController.cpp` ~1038–1049).
- The faction-selection preview spawn (`FactionPlayerController.cpp` ~63): it does go through the spawner,
  but runs on the faction menu where no profile is loaded, so it shows the base unit. Acceptable.

---

## 7. Blueprint work (per variant, by the designer)

1. Right-click the base unit Blueprint (tank, squad controller, team weapon controller or BXP) → *Create Child
   Blueprint Class*. Name it `<Base>_VariantA` / `<Base>_VariantB`.
2. In the child's class defaults set the `RTSComponent` → `Unit Variant` to `VariantA` / `VariantB`. Leave the
   subtype and owner as inherited: every variant uses the base unit's subtype (see "One subtype per unit").
3. Override what differs: meshes and materials, turret/weapon child actor classes, vehicle modules, ability
   lists, `SquadUnitClasses` (squads), `TeamWeaponClass` (team weapon crews), building mesh and attachments
   (BXPs). Do **not** change the RTS subtype, the unit type or anything that `DT_Training` /
   `DT_TrainingOptions` keys by name (cost, training time and requirements stay those of the base unit; the
   training icon, title and description are changed in step 4, not in those tables).
4. In `DA_UnitVariants` add or extend the entry for the subtype: element 0 is Base (no class), element 1/2 get
   the child class, a display name, description and icon for the later selection UI, and for BXPs an optional
   preview mesh. To change what the training menu shows while this variant is selected, tick
   `bOverrideTrainingIcon` / `bOverrideTrainingTitle` / `bOverrideTrainingDescription` and fill `TrainingIcon` /
   `TrainingTitle` / `TrainingDescription` (3.2, 3.8). Unticked means the base unit's icon or text.
5. Run *Asset Actions → Validate* on the asset; the rules in 3.2 must pass.
6. Soldier-level changes for squads: make a child of the soldier Blueprint too and list it in the variant
   controller's `SquadUnitClasses`.

No event, no switch, no BeginPlay logic. A variant Blueprint can still use its own Event BeginPlay like any
other Blueprint.

---

## 8. Migration from the committed implementation

The tree already contains the earlier pull-based design (commits `c110e711` … `ae21d32f`). Apply this table
before adding anything from section 3. "Keep" means leave as committed.

| Symbol / file | Action |
|---|---|
| `Interfaces/UnitVariantReceiver.h` | **Delete.** Remove `public IUnitVariantReceiver` and the include from `ATankMaster`, `ASquadController`, `ASquadUnit`, `ATeamWeapon`, `ABuildingExpansion` (`ATeamWeaponController` inherits from the squad controller; nothing to edit there for the interface). |
| `ATankMaster::ApplyUnitVariant` (private, called in `BeginPlay` before `CheckForUpgrades`) | **Delete** declaration, definition and call. |
| `ASquadController::ApplyUnitVariant` (protected, called in `InitSquadData` before `SetDataLoaded(true)`) | **Delete** declaration, definition and call. |
| `ASquadUnit::OnDataLoaded_ApplyUnitVariantFromSquad` and its call in the `SetSquadController` lambda | **Delete.** |
| `ATeamWeaponController::ApplyUnitVariantToTeamWeapon` (called at `OnAllSquadUnitsLoaded` ~149), `ForwardUnitVariantToTeamWeapon`, `M_UnitVariantAppliedHandle` and its unbind | **Delete.** |
| `ABuildingExpansion::ApplyUnitVariant` (called in `InitBuildingExpansion` before `CheckForUpgrades`) | **Delete** declaration, definition and call. |
| `URTSComponent`: `OnUnitVariantApplied`, `FOnUnitVariantApplied`, `GetHasAppliedUnitVariant`, `ApplyUnitVariant`, `ApplyUnitVariantFromParent`, `ApplyUnitVariant_Internal`, `M_UnitVariant`, `bM_HasAppliedUnitVariant` | **Replace** with the `UnitVariant` class default and `GetUnitVariant()` of 3.7. |
| `UPlayerUnitVariantManager::ResolveVariantForUnit` | **Delete.** Everything else in the manager: keep; add 3.5. |
| `FRTS_Statics::GetPlayerUnitVariantManager`, `ACPPController::M_PlayerUnitVariantManager` / getter / validator | Keep. |
| `Units/Variants/UnitVariantTypes.h` | Keep. |
| `Units/Variants/UnitVariantSettings.h/.cpp` | Keep the properties; rewrite the `DebugForcedVariants` comment (it no longer bypasses the clamp, 3.3). |
| `UPlayerUnitVariantManager::ApplyDebugForcedVariants` | **Change**: route through `StoreSelectionClamped` instead of bypassing it. |
| `Units/Variants/UnitVariantDataAsset.h/.cpp` | Keep; extend per 3.2. |
| Save data, `FRTS_Profile::ValidateUnitVariantSelections`, `USavePlayerProfile`, `FPlayerCardSaveData` | Keep. |
| `UPlayerProfileLoader::ApplyUnitVariantSelections` | Keep; extend per 3.6. |
| `URTSCheatManager::RTSCheatSetUnitVariant` / `RTSCheatPrintUnitVariants` | Keep; adapt per section 11. |
| `Units/Variants/Tests/UnitVariantTests.cpp` | Keep `RTS.UnitVariants.DataAssetCounts` and `RTS.UnitVariants.ManagerClampsSavedSelection`. **Delete** `OnlyPlayerUnitsGetVariants` and `ComponentAppliesOnce`. Rewrite `UnsupportedUnitTypesGetBase` against `GetIsUnitTypeSupported` / `StoreSelectionClamped` only. See section 10. |
| `Units/TeamWeapons/AGENTS.md` "Unit variants" section (~line 149) | **Rewrite** to two lines: a team weapon variant is a child Blueprint of the crew controller with its own `TeamWeaponClass` and `SquadUnitClasses`; the spawner swaps the class, see this doc. |
| The explicit `InfantryWeaponMaster.h` include added to `TeamWeaponDigInTestSubsystem.cpp` (unity build fix) | Keep. |
| The Russian key check using enumerator name **or** display name prefix `Rus` | Keep. |

After the table, nothing outside `Units/Variants/`, the manager, the spawner, `URTSComponent`, the profile
loader, the profile save code, the cheat manager and (after step 5 of section 9) `FTrainingWidgetState` /
`UTrainingMenuManager::CreateWidgetStateForOption` should mention variants. `grep -rn "UnitVariant" RTS_Survival`
is the check.

---

## 9. Implementation order

Each step compiles on its own. Build with `Build.bat RTS_SurvivalEditor Win64 Development ...` (memory note
"UE build command") after every step.

1. Migration table of section 8 (deletions and the `URTSComponent` replacement). Build; fix the test compile
   errors by deleting the listed tests.
2. Data asset extension (3.2) with validation and the two accessors.
3. Spawner additions (3.4) in `RTSAsyncSpawner_UnitVariants.cpp`; add `M_DirectorOverriddenOptions.Add` to
   `ApplyTrainingOptionOverride`.
4. Manager repurpose (3.5) and the profile loader call (3.6).
5. Training UI presentation (3.8): `FUnitVariantTrainingPresentation` (3.1), the training fields and
   `GetTrainingPresentation` on the asset (3.2), `GetTrainingPresentation` on the manager, the
   `VariantPresentation` field on `FTrainingWidgetState` and the helper in `CreateWidgetStateForOption`.
6. Cheat manager adaptation (section 11).
7. Tests (section 10).
8. Editor work (needs the editor or the UECP bridge; cannot be done from the filesystem): the three training
   widget Blueprint changes of 3.8 (`W_TrainingItem`, `W_TrainingDescription`, `W_TrainingRequirement`), one
   child Blueprint for one tank (e.g. `BP_PzIV_H_VariantA` with a visibly different mesh or turret), its asset
   entry (with all three training overrides ticked, so 10.2 can check them) in
   `DA_UnitVariants` (the asset lives where the other settings assets do, e.g. next to
   `/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DA_TrainingLayout`; picking it writes a
   `[/Script/RTS_Survival.UnitVariantSettings]` block into `Config/DefaultGame.ini` like the
   `TrainingLayoutSettings` block at ~line 189), and the manual test of 10.2.
9. Update the `Docs/README.md` hook text if it still describes the pull design.

---

## 10. Tests

### 10.1 Automation tests — `Units/Variants/Tests/UnitVariantTests.cpp`

Run headless (does not touch an open editor; needs `-DisablePlugins=Fab`):

```
UnrealEditor-Cmd.exe D:\UE5Projects\RTS_Survival\RTS_Survival.uproject -ExecCmds="Automation RunTests RTS.UnitVariants; Quit" -unattended -nullrhi -nosplash -DisablePlugins=Fab -log
```

Keep:

- `RTS.UnitVariants.DataAssetCounts`: counts of 1 / 2 / clamped 3 and `GetIsVariantImplemented`.
- `RTS.UnitVariants.ManagerClampsSavedSelection`: an unimplemented VariantB clamps to Base with one reported
  error (wrapped in `RTSFunctionLibrary::Push/PopModalDialogSuppression`, expects the `LogRTS` error). A
  non-empty `DebugForcedVariants` in `Config/DefaultGame.ini` makes it fail; clear the list first.

Add:

- `RTS.UnitVariants.DataAssetClassRules`: fill an asset through `FUnitVariantTestAccess` with a Base element
  that has a class, a VariantA without a class, a tank entry whose class is a squad controller subclass, and
  the same class twice; `IsDataValid` must return `Invalid` and report at least one error per broken rule
  (do not assert an exact count; the preview-mesh warning and future rules would make it brittle). Use
  `ATankMaster::StaticClass()` etc. as the classes; no Blueprints needed.
- `RTS.UnitVariants.FindVariantClass`: Base → null, VariantA → the class, missing entry → null.
- `RTS.UnitVariants.TrainingPresentation`: a VariantA definition with only `bOverrideTrainingTitle` on gives a
  presentation with `Variant == VariantA`, `bOverrideTitle` true and the title, `bOverrideIcon` and
  `bOverrideDescription` false; Base and a missing entry give everything off. Through the manager: a selection
  stored for `(UNType_Tank, Tank_PzI_Harvester)` is found when asked with `(UNType_Harvester, Tank_PzI_Harvester)`.
  `IsDataValid` must fail for an override bool on element 0 and for a ticked override with an empty value.
- `RTS.UnitVariants.UnsupportedUnitTypesRejected` (rewrite of the old supported-types test):
  `StoreSelectionClamped` returns false for `UNType_Nomadic` / `UNType_Aircraft` and the selection is not stored.

The spawner override itself needs a world with a spawner actor and loaded Blueprints; it is not an automation
test. Cover it with 10.2.

### 10.2 Map test (manual, or a `-UnitVariantValidate` subsystem later)

1. Set `DebugForcedVariants` to `{Tank Tank_PzIV_H → VariantA}` (and the entries of any other variant
   Blueprints that exist).
2. Run `RTS_ToCopySmall` (flat test map, see memory note "Team weapon quality test"). Train a Pz IV H; the
   spawned actor's class must be `BP_PzIV_H_VariantA_C` (`RTSCheatPrintUnitVariants` prints the mapped class
   per selection; or check the actor name in the outliner).
3. Spawn the same tank as a starting unit (profile card) and through a mission spawn if one is handy; same
   class.
4. `RTSCheatSetUnitVariant <UNType_Tank value> <Tank_PzIV_H value> 0` (Base) at runtime; the next trained
   Pz IV H is the base class, the one already on the map is unchanged.
5. With a BXP variant that sets a preview mesh: open the build menu and confirm the placement preview changed.
6. Training menu, with the forced Pz IV H VariantA whose three training overrides are ticked: the slot shows the
   variant icon, hovering shows the variant title and description, and the cost and training time are the base
   unit's. Untick `bOverrideTrainingDescription` and restart: the base description returns while icon and title
   stay. After step 4 (back to Base), the next menu refresh shows the base icon and texts.

---

## 11. Development aids

### 11.1 Cheat manager — `Player/RTSCheatManager/RTSCheatManager.h/.cpp` (exists, adapted)

- `RTSCheatSetUnitVariant <UnitType> <Subtype> <Variant>` (numeric enum values): unchanged entry point; it
  now takes effect through the manager → spawner path, so the message should print the result of
  `SetSelectedVariant` and the class name `GetTrainingOptionClass` returns afterwards.
- `RTSCheatPrintUnitVariants`: for every stored selection print `TrainingName -> VariantString -> mapped class
  asset name`; for every unit with a choice in the asset print the implemented count.

### 11.2 Forced variants in Project Settings (exists, now clamped)

`UUnitVariantSettings::DebugForcedVariants` is applied in `InitFromProfile` before `ApplySelectionsToSpawner`,
so forced entries reach the spawner like saved ones, and they are clamped like saved ones: a forced variant
without an asset entry (and therefore without a class) clamps to Base with one reported error. Each forced
entry that survives the clamp is logged as a warning so a forgotten override is visible.

---

## 12. Pitfalls for the implementer

- **Do not reuse `ApplyTrainingOptionOverride` for variants.** It is silent on a missing key and its errors say
  "Enemy director". Variants need the loud version (3.4).
- **Remember the pre-variant class only once per key.** Check `Contains` before writing the pre-variant map;
  a second variant application must not overwrite the remembered base with the first variant.
- **`BuildingExpansionMap` is typed `TSoftClassPtr<ABuildingExpansion>`**, the asset holds
  `TSoftClassPtr<AActor>`. Convert through `ToSoftObjectPath()`; do not add a second typed field to the asset.
- **`BxpPreviewMeshMap` holds hard `UStaticMesh*`** read synchronously at placement. Load the variant preview
  mesh with `LoadSynchronous()` when applying, and keep the remembered base mesh in a `UPROPERTY` map so GC
  sees it.
- **Order in `LoadProfile`**: the variant application sits before the `bDoNotLoadPlayerUnits` early return and
  before `LoadInUnits`; moving it after either breaks starting units or no-unit maps.
- **`FTrainingOption` keys for tanks are always `UNType_Tank`**, including the harvester. A selection keyed
  `UNType_Harvester` finds no base mapping; the manager normalises it (3.5), the later UI must not rely on
  that and should key the harvester as a tank itself.
- **No owner checks** (section 6). Do not check the trainer component's owner or add an owner parameter to any
  spawner function: a faction is always unique to one player, so variant keys (German subtypes) are only ever
  spawned for the player.
- **Never give a variant its own subtype** ("One subtype per unit" at the top). No new enum values, no new
  `DT_Training` rows, no new spawner keys; a variant replaces the class under the base unit's existing key, and
  shares its cost, training time and requirements.
- **Cards, `DT_Training` and `DT_TrainingOptions` are untouched.** All variants share the base
  `FTrainingOption`. Anything keyed by class (there should be nothing) would break; grep for `UnitClass ==`
  style comparisons before assuming.
- **Variant training icon / title / description come only from the variant data asset** (3.8). Do not add
  variant rows to `DT_TrainingOptions` or `DT_Training`, and do not override cost, training time or
  requirements in the training UI. Each override is shown only when its bool is ticked; the Blueprints fall back
  to their existing table lookup otherwise.
- **`TMap::operator[]` inserts.** The spawner's existing code uses `Contains` then `[]`; new code uses `Find`.
- **Shipping builds** compile out `DebugForcedVariants` application (`#if !UE_BUILD_SHIPPING`), as committed.
- **Pointer rule 22**: the `TSoftClassPtr<AActor>*` from `M_TrainingOptionMap.Find` is used in the same
  function with no mutation of that map in between. Writing the pre-variant map is a different container; fine.

---

## 13. Extension points (not in this change)

- **Nomadic trucks and aircraft**: add `TMap<ENomadicSubtype, …>` / `TMap<EAircraftSubtype, …>` to the asset,
  the two cases to `FindEntry`, `GetIsUnitTypeSupported` and `ApplyPlayerUnitVariants`'s dispatch. The spawner
  already keys both kinds in `M_TrainingOptionMap`, so `ApplyTrainingOptionVariant` works unchanged.
- **Per-doctrine availability**: wrap the three maps in a struct keyed by `ERTSFaction` like
  `UTrainingLayoutDataAsset::FactionLayouts`, with a fallback to a shared entry.
- **Selection UI**: reads `GetUnitsWithChoice()` and `FindVariantDefinition` for name / icon / description,
  writes `FUnitVariantSelection` arrays into the profile (section 4), and calls
  `UPlayerUnitVariantManager::SetSelectedVariant` if it ever runs inside a mission.
- **Per-unit choice at training time**: would pass the class per request instead of overriding the map; keep
  `GetTrainingOptionClass` as the single read point so that change stays local to the spawner.
