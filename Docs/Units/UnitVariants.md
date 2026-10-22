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
| Variant definition store | `UUnitVariantDataAsset` (`DA_UnitVariants`) holds **all** variant data: classes, preview meshes, selection-UI texts, training UI overrides and the debug forced list. Project Settings > Unit Variants holds **only** the reference to that asset (3.3). |
| Subtype | **All variants of a unit share that unit's one subtype.** Three Pz II F Blueprints are all `Tank_PzII_F`. No variant ever gets its own `ETankSubtype` / `ESquadSubtype` / `EBuildingExpansionType`; see "One subtype per unit" below. |
| Cost / training time / requirements | Identical for every variant, because they are keyed by the shared subtype (`FTrainingOption`, `DT_Training` row). This is intended. |
| Training UI presentation | While a variant is selected, the training menu can show that variant's own value for any of the four `DT_TrainingOptions` columns (`BP_FTrainingUI_OptionSlate`): **`ImageBrush`, `Title`, `Description`, `Tooltip`**. Each of the four is opt-in per variant with its own bool on the variant data asset entry; off means the base unit's `DT_TrainingOptions` row value is shown. The values come from the variant data asset, never from extra `DT_Training` / `DT_TrainingOptions` rows. The image brush is a hard `USlateBrushAsset` reference held by `UPROPERTY`s end to end so GC cannot collect it (3.9). See 3.8. |
| Per doctrine | No. The implemented variants are per subtype, not per `ERTSFaction`. |
| Stale saves | A saved variant the asset no longer implements clamps to `Base` and reports an error once (already in place). |
| Precedence vs director overrides | Director overrides apply first, variants second. A variant wins on the same key; the collision is reported. |
| Testing before the UI exists | Cheat manager commands plus the forced-variant list, moved from the developer settings onto the data asset (3.3). |

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
- The **only** per-variant exception the player sees in the training menu is presentation: the four
  `DT_TrainingOptions` columns image brush, title, description and tooltip, each opt-in (3.8). They are read from
  the variant data asset by the training menu, keyed by the shared `FTrainingOption` plus the selected
  `EUnitVariant`. Cost, training time and requirements in that same menu stay the shared ones.
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
class USlateBrushAsset;

/**
 * What the training UI shows instead of the base unit's DT_TrainingOptions row while a variant is selected.
 * Mirrors the row struct BP_FTrainingUI_OptionSlate (ImageBrush, Title, Description, Tooltip), one opt-in bool each.
 */
USTRUCT(BlueprintType)
struct FUnitVariantTrainingPresentation
{
	GENERATED_BODY()

	// The selected variant; Base when the player kept the base unit, and then every override below is off.
	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	EUnitVariant Variant = EUnitVariant::Base;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	bool bOverrideImageBrush = false;

	// Hard reference on purpose, same type as the DT_TrainingOptions ImageBrush column. Being a UPROPERTY keeps the
	// brush asset (and through it its texture) reachable for GC from every struct copy that is itself a UPROPERTY,
	// such as UW_TrainingItem::M_TrainingItemState. Never turn this into a soft pointer or a by-value FSlateBrush (3.9).
	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	TObjectPtr<USlateBrushAsset> ImageBrush = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	bool bOverrideTitle = false;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	bool bOverrideDescription = false;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	bool bOverrideTooltip = false;

	UPROPERTY(BlueprintReadOnly, Category = "Unit Variant")
	FText Tooltip;
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

	// Training UI (3.8): one opt-in override per DT_TrainingOptions column (row struct BP_FTrainingUI_OptionSlate).
	// Off shows the base unit's DT_TrainingOptions row value. DisplayName / Description / Icon above are for the
	// later variant selection UI and are not used here, so the selection UI can say "Long 75 mm gun" while the
	// training menu says "Pz IV H (L/48)".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI")
	bool bOverrideTrainingImageBrush = false;

	// Hard reference (not soft): it loads with the data asset and stays alive as long as the asset does, exactly
	// like the brushes the DT_TrainingOptions rows hold. See 3.9 for why this must not be a soft pointer.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI",
		meta = (EditCondition = "bOverrideTrainingImageBrush"))
	TObjectPtr<USlateBrushAsset> TrainingImageBrush = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI")
	bool bOverrideTrainingTitle = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI", meta = (EditCondition = "bOverrideTrainingTitle"))
	FText TrainingTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI")
	bool bOverrideTrainingDescription = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI",
		meta = (EditCondition = "bOverrideTrainingDescription", MultiLine = "true"))
	FText TrainingDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI")
	bool bOverrideTrainingTooltip = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training UI",
		meta = (EditCondition = "bOverrideTrainingTooltip", MultiLine = "true"))
	FText TrainingTooltip;
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

`GetTrainingPresentation` copies each override only when its bool is on (and, for the image brush, the pointer is
valid); `Variant` in the result is always the requested variant so a Blueprint can show a variant badge even
when no override is on. Copying the `TObjectPtr<USlateBrushAsset>` into the returned struct is safe because both
ends are `UPROPERTY` and the source object is the data asset the manager keeps alive (3.9).

`IsDataValid` additions (editor only, so `LoadSynchronous` is fine there), all reported as errors:

- Element 0 has a non-null `UnitClass`.
- Element 1 or 2 has a null `UnitClass` (an entry without a class is not implemented; shorten the array instead).
- A class whose native parent is wrong for its map: tank map classes must `IsChildOf(ATankMaster::StaticClass())`,
  squad map classes `ASquadController`, building expansion map classes `ABuildingExpansion`.
- The same class used by two definitions anywhere in the asset.
- `BxpPreviewMesh` set on a tank or squad definition (warning, it is ignored there).
- Any `bOverrideTraining*` on element 0 (Base always shows the base unit's table entries).
- `bOverrideTrainingImageBrush` on with a null `TrainingImageBrush`, or a brush whose
  `Brush.GetResourceObject()` is not a `UTexture2D` (`UW_TrainingItem::GetIconTextureFromSlateBrushAsset` needs
  a texture for the cooldown clock), `bOverrideTrainingTitle` on with an empty `TrainingTitle`,
  `bOverrideTrainingDescription` on with an empty `TrainingDescription`, `bOverrideTrainingTooltip` on with an
  empty `TrainingTooltip`.

`GetImplementedVariantCount` stays "array length clamped to 1..3": an entry with elements that fail validation
is a data error, not a runtime case.

### 3.3 `UUnitVariantSettings` — `Units/Variants/UnitVariantSettings.h/.cpp` (exists, reduced to the asset)

**Project Settings holds only the reference to the data asset.** Everything else about variants, including the
debug forced-variant list, lives in `DA_UnitVariants`, so all variant data is edited, validated, diffed and
cooked in one place.

- Keep `VariantDataAsset` (`TSoftObjectPtr<UUnitVariantDataAsset>`) and `LoadVariantDataAsset()` (falls back to
  the class defaults when none is picked).
- **Move** `DebugForcedVariants` out of the settings into `UUnitVariantDataAsset` (below). Remove the property and
  its `Debug` category from the settings class, and delete a stale `DebugForcedVariants=` line from the
  `[/Script/RTS_Survival.UnitVariantSettings]` block in `Config/DefaultGame.ini` if one was ever saved.
- Rewrite the class Doxygen: "Project Settings > Unit Variants: pick the `UUnitVariantDataAsset` the game uses.
  All variant data, including the debug forced-variant list, is on that asset."

On `UUnitVariantDataAsset` (3.2), add:

```cpp
	/** @return The forced selections; always empty in Shipping builds. */
	const TArray<FUnitVariantSelection>& GetDebugForcedVariants() const;

private:
	// Development aid until the selection UI exists: applied over the saved selection when the manager
	// initialises, clamped exactly like saved selections. Ignored in Shipping builds. Clear it before committing.
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (TitleProperty = "Unit"))
	TArray<FUnitVariantSelection> M_DebugForcedVariants;
```

`GetDebugForcedVariants` returns a static empty array under `UE_BUILD_SHIPPING`. `IsDataValid` warns (not
errors) while the list is non-empty, so a forgotten forced variant shows on every *Validate* run.

Semantics change from the committed version: it bypassed the asset clamp "so unlisted Blueprint variants can be
tested". Under class swap an unlisted variant has no class to swap to, so bypassing only produces a "no class"
error every mission start. Forced entries are now clamped exactly like saved ones.

### 3.4 `ARTSAsyncSpawner` additions — `Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h` and a new `RTSAsyncSpawner_UnitVariants.cpp`

Mirror the director override file. The variant data asset is passed in by the manager (the one asset it clamped
the selection against and keeps alive), so the spawner stores no asset and never reads the settings. Public:

```cpp
	/**
	 * @brief Replaces the spawn class of every selected unit type with its variant class.
	 * Runs after the director overrides so a variant wins on a shared key; the collision is reported.
	 * @param VariantDataAsset The asset the selections were clamped against.
	 * @param Selections Clamped selections (Base entries are skipped).
	 * @return The units whose variant could not be applied (reported); the manager drops them from its selection.
	 */
	TArray<FTrainingOption> ApplyPlayerUnitVariants(const UUnitVariantDataAsset& VariantDataAsset,
	                                                const TArray<FUnitVariantSelection>& Selections);

	/**
	 * @brief Changes one unit type at runtime (cheat manager, future UI). Base restores the class the mapping held
	 * before the first variant override, so a mission can go back and forth.
	 * @return False when the unit type has no base mapping or the variant has no class in the asset.
	 */
	bool SetPlayerUnitVariant(const UUnitVariantDataAsset& VariantDataAsset, const FTrainingOption& Unit,
	                          const EUnitVariant Variant);

	/** @return The class currently mapped for the training option (after overrides), or a null soft pointer. */
	TSoftClassPtr<AActor> GetTrainingOptionClass(const FTrainingOption& TrainingOption) const;
```

Private:

```cpp
	// Each takes the variant data asset first, like the public functions.
	bool ApplyUnitVariant(const FTrainingOption& Unit, const EUnitVariant Variant);  // dispatch on the unit kind
	bool ApplyTrainingOptionVariant(const FTrainingOption& Unit, const EUnitVariant Variant);
	bool ApplyBuildingExpansionVariant(const EBuildingExpansionType BxpType, const EUnitVariant Variant);
	void ApplyBxpVariantPreviewMesh(const EBuildingExpansionType BxpType, const EUnitVariant Variant);
	void RestoreBasePreviewMesh(const EBuildingExpansionType BxpType);

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
  `ToSoftObjectPath()`), plus the preview mesh: remember the `BxpPreviewMeshMap` entry in
  `M_PreVariantBxpPreviewMeshes` the same way (only when the base has an entry, so restoring also restores "no
  entry"), then if `FindBxpVariantPreviewMesh` is non-null, `LoadSynchronous()` it and write it into
  `BxpPreviewMeshMap` (the map holds hard pointers and `SyncGetBuildingExpansionPreviewMesh` reads it
  synchronously at placement time). Base, a variant without a preview mesh and a mesh that fails to load restore
  the remembered base entry.
- `ApplyTrainingOptionOverride` (director path) gets one extra line: `M_DirectorOverriddenOptions.Add(TrainingOption)`
  after a successful replacement.
- The variant data asset comes in as a parameter from `UPlayerUnitVariantManager`; store nothing on the spawner.
- No runtime class-hierarchy check. The asset's `IsDataValid` is the gate (3.2).

### 3.5 `UPlayerUnitVariantManager` — `Player/PlayerUnitVariantManager/` (exists, repurposed)

It stays the **selection store** on `ACPPController` (created in the constructor next to the tech manager,
`GetPlayerUnitVariantManager()`, `FRTS_Statics::GetPlayerUnitVariantManager`). Keep: `InitFromProfile` (clamps
through `StoreSelectionClamped`, applies the asset's forced list), `GetSelectedVariant`, `SetSelectedVariant`,
`GetSelectionsForSave`, `GetVariantDataAsset`, `GetIsUnitTypeSupported` (Tank, Squad, BuildingExpansion), the
test friend access.

Change: `StoreSelectionClamped` normalises `UNType_Harvester` to `UNType_Tank` before anything else (the
spawner has no harvester keys, see 3.1), and `ApplyDebugForcedVariants` reads
`GetVariantDataAsset()->GetDebugForcedVariants()` instead of the settings and goes through the same clamp as saved
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

	/** @return The key the spawner uses for the unit: the training menu lists harvesters as UNType_Harvester. */
	static FTrainingOption GetSpawnerKey(const FTrainingOption& Unit);

private:
	// No report: outside a mission (menus, automation tests) selections are only stored.
	bool GetIsValidAsyncSpawnerNoReport() const;

	UPROPERTY()
	TWeakObjectPtr<ARTSAsyncSpawner> M_AsyncSpawner;
```

`GetSpawnerKey` is the one place the harvester normalisation lives; `StoreSelectionClamped`,
`GetSelectedVariant`, `SetSelectedVariant` and `GetTrainingPresentation` all key through it.

`SetSelectedVariant` keeps its clamp, then, when `M_AsyncSpawner` is valid, calls
`M_AsyncSpawner->SetPlayerUnitVariant(*M_VariantDataAsset, Unit, StoredVariant)` with the variant that survived
the clamp (so a rejected VariantB after a VariantA also puts the spawner back on Base). When the spawner refuses,
the selection is dropped so the stored choice always equals what spawns. `ApplySelectionsToSpawner` builds the
array from `M_SelectedVariants`, calls `ApplyPlayerUnitVariants` and removes the units it returns.

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

### 3.8 Training UI: variant image brush, title, description and tooltip (new)

Goal: while the player has a variant selected for a unit, its training menu slot, its hover description and its
requirement panel can show the variant's own value for any of the four `DT_TrainingOptions` columns
(`/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DT_TrainingOptions`, row struct `BP_FTrainingUI_OptionSlate`:
`ImageBrush` (`USlateBrushAsset*`), `Title`, `Description`, `Tooltip`). Each is opt-in on the variant definition
(3.2: `bOverrideTrainingImageBrush`, `bOverrideTrainingTitle`, `bOverrideTrainingDescription`,
`bOverrideTrainingTooltip`). Cost, training time and requirements are **not** overridable; they stay the shared
ones ("One subtype per unit").

How the training UI gets its data today (traced):

- `UTrainingMenuManager::CreateWidgetStateForOption` (`TrainingMenuManager.cpp` ~3020) builds one
  `FTrainingWidgetState` per shown slot, called from `UpdateWidgetsWithShownSubSection` (~3016) on every
  refresh. It is the **only** place a `FTrainingWidgetState` is built from a training option.
- That state reaches the three Blueprint events that draw a training option:
  `UW_TrainingItem::OnUpdateTrainingItem` (the slot), `UW_TrainingDescription::OnShowTrainingDescription` (the
  hover panel, via `OnTrainingItemHovered` → `UMainGameUI::SetBottomUIPanel`) and
  `UW_TrainingRequirement::OnUpdateRequirement`.
- `FTrainingWidgetState` (`GameUI/TrainingUI/TrainingUIWidget/TrainingItemState/TrainingWidgetState.h`) carries
  no brush or text: the Blueprints look up the `DT_TrainingOptions` row by `ItemID` (row name
  `GetTrainingOptionName`; where exactly title, description and tooltip are read inside `W_TrainingItem` /
  `W_TrainingDescription` was not traced, it is in their graphs). The cost panel
  (`UW_TrainingDescription::CostDisplay`) is filled from C++ by `ItemID` and stays as is.
- `W_TrainingItem`'s `OnUpdateTrainingItem` graph loads the row's `ImageBrush` asset, sets the item image, then
  calls the C++ `UW_TrainingItem::OnObtainedSlateBrushFromTrainingTable(USlateBrushAsset*)`, which extracts the
  brush's `UTexture2D` and caches it in the `Transient UPROPERTY M_CooldownIconTexture` for the cooldown clock
  material. A variant brush must go through that same call, or the cooldown clock keeps the base unit's icon.
- C++ reads the same table for the quick navigation buttons
  (`UTrainingLayoutSubsystem::GetIconBrushFromTrainingOptionTable` finds the `USlateBrushAsset` member by type
  because the row is a Blueprint struct). Those show buildings and are not overridden.

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

Blueprint changes (done 2026-10-22). Both widgets keep their `DT_TrainingOptions` lookup and pass each row value
through `UUnitVariantPresentationLibrary` (`Units/Variants/UnitVariantPresentationLibrary.h`): `GetTrainingImageBrush`,
`GetTrainingTitle`, `GetTrainingDescription`, `GetTrainingTooltip` take `VariantPresentation` and the table value and
return the variant value when its bool is on, the table value otherwise.

- `BP_W_TrainingItem` `OnUpdateTrainingItem`: *Get Data Table Row DT_TrainingOptions* → *Break* → the four helpers →
  *Make BP_FTrainingUI_OptionSlate* → *Set Slate* (the widget's row variable, which its image binding reads), and
  `GetTrainingImageBrush` → `OnObtainedSlateBrushFromTrainingTable` (cooldown clock texture). `Slate` is a
  `UPROPERTY` of the widget, so the brush stays GC-visible there too. The *Row Not Found* branch is unchanged: a
  unit without a row (Pz IV H today) shows no image, variant or not.
- `BP_W_TrainingDescription` `OnShowTrainingDescription`: *Break* row → `GetTrainingImageBrush` → *Set Brush from
  Asset*, `GetTrainingTitle` → `RichTitle`, `GetTrainingDescription` → `RichContextBlock`. It shows no tooltip.
- `BP_W_Requirement`: no change. It only shows the *required* units, which stay the base names and icons.
- Do **not** use *Make Brush from Texture*, *Set Brush from Soft Texture* or an async load node for the variant
  brush, and do not store the brush in a Blueprint variable of type Slate Brush. The brush asset is already
  loaded and held (3.9); setting it the same way the table brush is set keeps both paths identical.
- Editing these graphs through the UECP bridge: do not call `arrange_blueprint_nodes`; it reroutes every existing
  wire through reroute-node chains and moves the user's nodes.

### 3.9 Keeping variant brushes alive (GC)

Slate brushes are a common source of "the icon turns white / blank after a while" bugs: `FSlateBrush` only holds
its texture through `ResourceObject`, Slate's own copy of a brush (inside `SImage`) is invisible to GC, and a
texture that only a Slate-side copy, a soft pointer or a non-`UPROPERTY` struct refers to is collected at the next
GC. The variant path therefore keeps a strong, GC-visible chain from a rooted object down to the texture at all
times, the same chain the `DT_TrainingOptions` rows already have:

1. `DefaultGame.ini` / Project Settings → `UUnitVariantSettings::VariantDataAsset` (soft, and the only thing
   the settings hold) is only the *path*.
   The strong owner of the loaded asset is `UPlayerUnitVariantManager::M_VariantDataAsset`
   (`UPROPERTY() TObjectPtr<const UUnitVariantDataAsset>`, cached in `BeginPlay_LoadVariantDataAsset`), and
   the manager is a `UPROPERTY` of `ACPPController`. Keep that cache; `GetTrainingPresentation` on the manager
   reads through `GetVariantDataAsset()`, whose uncached fallback is only for menus without a mission.
2. `FUnitVariantDefinition::TrainingImageBrush` is a **hard** `TObjectPtr<USlateBrushAsset>` `UPROPERTY`, so the
   brush asset loads with the data asset and is referenced by it.
3. `USlateBrushAsset::Brush` is a `UPROPERTY FSlateBrush`, whose `ResourceObject` is a `UPROPERTY`, so the
   texture is referenced by the brush asset.
4. Every copy the training menu makes is a `UPROPERTY` struct holding the brush **asset** pointer, never an
   `FSlateBrush` by value: `FUnitVariantTrainingPresentation::ImageBrush` inside
   `FTrainingWidgetState::VariantPresentation` inside `UW_TrainingItem::M_TrainingItemState` (`UPROPERTY`).
   The cooldown clock's texture is held by `UW_TrainingItem::M_CooldownIconTexture` (`UPROPERTY(Transient)`).

Consequences for the implementer:

- No `TSoftObjectPtr` for the training brush anywhere (definition, presentation, widget state). A soft pointer
  resolved with `Get()` / `LoadSynchronous()` has no strong owner and is exactly the GC trap.
- No raw `USlateBrushAsset*` / `UTexture2D*` members or containers without `UPROPERTY`, and no `FSlateBrush`
  cached in a non-`UPROPERTY` member. Pass the brush asset pointer; let the widget set it.
- The `TObjectPtr` in a `USTRUCT` is only seen by GC while the struct itself sits in a `UPROPERTY` of a live
  `UObject`. A local `FUnitVariantTrainingPresentation` on the stack is fine for the duration of the call
  (the data asset still holds the brush); do not stash one in a lambda capture or a timer.
- Cooking: the brush and texture are cooked because the data asset references them hard. Verify that
  `DA_UnitVariants` itself is cooked: a settings-only soft reference is not always followed by the cooker (memory
  note "Packaged debug testing": C++-only asset paths are not cooked). Check how `DA_TrainingLayout`, which is
  referenced the same way, ends up in the package, and do the same (or add it to *Additional Asset Directories
  to Cook* / a primary asset rule).

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
   training image brush, title, description and tooltip are changed in step 4, not in those tables).
4. In `DA_UnitVariants` add or extend the entry for the subtype: element 0 is Base (no class), element 1/2 get
   the child class, a display name, description and icon for the later selection UI, and for BXPs an optional
   preview mesh. To change what the training menu shows while this variant is selected, tick any of
   `bOverrideTrainingImageBrush` / `bOverrideTrainingTitle` / `bOverrideTrainingDescription` /
   `bOverrideTrainingTooltip` and fill `TrainingImageBrush` (a Slate Brush asset whose image is a texture, like
   the ones the `DT_TrainingOptions` rows use) / `TrainingTitle` / `TrainingDescription` / `TrainingTooltip`
   (3.2, 3.8). Unticked means the base unit's `DT_TrainingOptions` value.
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
| `Units/Variants/UnitVariantSettings.h/.cpp` | Keep `VariantDataAsset` / `LoadVariantDataAsset`. **Move** `DebugForcedVariants` to the data asset (3.3); the settings keep only the asset reference. Rewrite the class comment. |
| `UPlayerUnitVariantManager::ApplyDebugForcedVariants` | **Change**: read the list from the data asset and route it through `StoreSelectionClamped` instead of bypassing it. |
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
   `VariantPresentation` field on `FTrainingWidgetState` and the helper in `CreateWidgetStateForOption`. Brush
   pointers follow 3.9 (hard `UPROPERTY` `USlateBrushAsset` everywhere).
6. Cheat manager adaptation (section 11).
7. Tests (section 10).
8. Editor work (needs the editor or the UECP bridge; cannot be done from the filesystem): the three training
   widget Blueprint changes of 3.8 (`W_TrainingItem`, `W_TrainingDescription`, `W_TrainingRequirement`), one
   child Blueprint for one tank (e.g. `BP_PzIV_H_VariantA` with a visibly different mesh or turret), its asset
   entry (with all four training overrides ticked, so 10.2 can check them) in
   `DA_UnitVariants` (the asset lives where the other settings assets do, e.g. next to
   `/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DA_TrainingLayout`; picking it writes a
   `[/Script/RTS_Survival.UnitVariantSettings]` block into `Config/DefaultGame.ini` like the
   `TrainingLayoutSettings` block at ~line 189; that block holds only `VariantDataAsset=`), and the manual test
   of 10.2.
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
  error (wrapped in `RTSFunctionLibrary::Push/PopModalDialogSuppression`, expects the `LogRTS` error). Give the
  manager a test data asset through `FUnitVariantTestAccess` with an empty forced list, so a forced entry left
  in `DA_UnitVariants` cannot make it fail.
- Extend `RTS.UnitVariants.ManagerClampsSavedSelection` (or add `RTS.UnitVariants.ForcedVariantsFromAsset`): a
  test asset whose forced list holds one implemented and one unimplemented variant gives the implemented one
  and Base for the other, with one reported error.

Add:

- `RTS.UnitVariants.DataAssetClassRules`: fill an asset through `FUnitVariantTestAccess` with a Base element
  that has a class, a VariantA without a class, a tank entry whose class is a squad controller subclass, and
  the same class twice; `IsDataValid` must return `Invalid` and report at least one error per broken rule
  (do not assert an exact count; the preview-mesh warning and future rules would make it brittle). Use
  `ATankMaster::StaticClass()` etc. as the classes; no Blueprints needed.
- `RTS.UnitVariants.FindVariantClass`: Base → null, VariantA → the class, missing entry → null.
- `RTS.UnitVariants.TrainingPresentation`: a VariantA definition with only `bOverrideTrainingTitle` on gives a
  presentation with `Variant == VariantA`, `bOverrideTitle` true and the title, `bOverrideImageBrush`,
  `bOverrideDescription` and `bOverrideTooltip` false; a definition with `bOverrideTrainingImageBrush` on and a
  transient `USlateBrushAsset` (`NewObject`, brush resource a transient `UTexture2D`) gives that same pointer in
  `ImageBrush`; Base and a missing entry give everything off. Through the manager: a selection stored for
  `(UNType_Tank, Tank_PzI_Harvester)` is found when asked with `(UNType_Harvester, Tank_PzI_Harvester)`.
  `IsDataValid` must fail for an override bool on element 0 and for a ticked override with an empty value.
- `RTS.UnitVariants.TrainingBrushSurvivesGC`: put a transient brush asset (with a transient texture) only into a
  test data asset's definition, keep the data asset in a `TStrongObjectPtr` (stand-in for the manager's
  `UPROPERTY`), build a presentation, drop every other reference, run
  `CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS)`, then check the presentation's `ImageBrush` and its resource
  texture are still valid. Release the strong pointer, collect again and check through a `TWeakObjectPtr` that
  both are gone (proves the test really depends on the data asset's reference, not on something else).
- `RTS.UnitVariants.UnsupportedUnitTypesRejected` (rewrite of the old supported-types test):
  `StoreSelectionClamped` returns false for `UNType_Nomadic` / `UNType_Aircraft` and the selection is not stored.

The spawner override itself needs a world with a spawner actor and loaded Blueprints; it is not an automation
test. Cover it with 10.2.

### 10.2 Map test

Automated: `UUnitVariantTestSubsystem` (`Units/Variants/Tests/`), started with `-UnitVariantValidate`
(optional `-UnitVariantTank=<ETankSubtype value>`, default Pz IV G, which has the test `VariantA` in
`DA_UnitVariants`). It selects VariantA at runtime, checks the spawner's mapped class and the training
presentation (also after a full GC), spawns one tank through the spawner and checks its class, `UnitVariant` and
hull mesh, then selects Base, checks the mapping is restored and spawns a base tank next to it. Logs
`RTS_UNIT_VARIANT_TEST`; the run exits by itself.

```
UnrealEditor-Cmd.exe <uproject> /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game -unattended -NullRHI -nosound -UnitVariantValidate -abslog=<file>
```

Manual (the menu itself, which the automated run does not draw):

1. In `DA_UnitVariants` (Debug category), set the forced-variant list to `{Tank Tank_PzIV_H → VariantA}` (and
   the entries of any other variant Blueprints that exist). Save the asset.
2. Run `RTS_ToCopySmall` (flat test map, see memory note "Team weapon quality test"). Train a Pz IV H; the
   spawned actor's class must be `BP_PzIV_H_VariantA_C` (`RTSCheatPrintUnitVariants` prints the mapped class
   per selection; or check the actor name in the outliner).
3. Spawn the same tank as a starting unit (profile card) and through a mission spawn if one is handy; same
   class.
4. `RTSCheatSetUnitVariant <UNType_Tank value> <Tank_PzIV_H value> 0` (Base) at runtime; the next trained
   Pz IV H is the base class, the one already on the map is unchanged.
5. With a BXP variant that sets a preview mesh: open the build menu and confirm the placement preview changed.
6. Training menu, with the forced Pz IV H VariantA whose four training overrides are ticked: the slot shows the
   variant image (also on the cooldown clock while it trains), the tooltip is the variant's, hovering shows the
   variant title and description, and the cost and training time are the base unit's. Untick
   `bOverrideTrainingDescription` and restart: the base description returns while the others stay. After step 4
   (back to Base), the next menu refresh shows the base image and texts.
7. GC: with the variant image showing, run `obj gc` in the console a few times (and leave the menu open for a
   few minutes while playing); the variant image must not go blank or white, on the slot or the cooldown clock.

---

## 11. Development aids

### 11.1 Cheat manager — `Player/RTSCheatManager/RTSCheatManager.h/.cpp` (exists, adapted)

- `RTSCheatSetUnitVariant <UnitType> <Subtype> <Variant>` (numeric enum values): unchanged entry point; it
  now takes effect through the manager → spawner path, so the message should print the result of
  `SetSelectedVariant` and the class name `GetTrainingOptionClass` returns afterwards.
- `RTSCheatPrintUnitVariants`: for every stored selection print `TrainingName -> VariantString -> mapped class
  asset name`; for every unit with a choice in the asset print the implemented count.

### 11.2 Forced variants on the data asset (moved from Project Settings, now clamped)

`UUnitVariantDataAsset::GetDebugForcedVariants()` (the Debug category of `DA_UnitVariants`, 3.3) is applied in
`InitFromProfile` before `ApplySelectionsToSpawner`,
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
- **Variant training image brush / title / description / tooltip come only from the variant data asset** (3.8).
  Do not add variant rows to `DT_TrainingOptions` or `DT_Training`, and do not override cost, training time or
  requirements in the training UI. Each override is shown only when its bool is ticked; the Blueprints fall back
  to their existing table lookup otherwise.
- **Variant brushes are hard `UPROPERTY` `USlateBrushAsset` pointers end to end** (3.9). No soft pointers, no
  by-value `FSlateBrush` caches, no *Make Brush from Texture*; route the variant brush through
  `OnObtainedSlateBrushFromTrainingTable` so the cooldown clock texture is held too.
- **`TMap::operator[]` inserts.** The spawner's existing code uses `Contains` then `[]`; new code uses `Find`.
- **Shipping builds** ignore the forced list: `GetDebugForcedVariants` returns an empty array under
  `UE_BUILD_SHIPPING`, and the manager keeps its `#if !UE_BUILD_SHIPPING` around the application, as committed.
- **Project Settings holds only the asset reference** (3.3). Do not add new variant properties to
  `UUnitVariantSettings`; they go on `UUnitVariantDataAsset`.
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

## 14. Faction selection variant screen (done 2026-10-22)

The first selection UI runs on the faction selection map (`RTS_FactionSelection`), until the world campaign has its
own section for unlocked units.

- **When:** after Launch Campaign on the faction menu, before the difficulty picker, only when
  `URTSGameInstance::GetAllowUnitVariantSelection()` is on (Blueprint: `SetAllowUnitVariantSelection`, off by
  default; in PIE: Project Settings > RTS PIE Startup > Allow Unit Variant Selection) and the faction's menu setting
  (`FFactionMenuSetting` on `BP_W_FactionSelectionMenu`) lists at least one unit with more than one variant.
- **Code:** `FactionSystem/FactionSelection/UnitVariantSelection/`. `UW_UnitVariantSelectionMenu` builds its own
  widget tree (unit list left, variant cards right, Back / Confirm), `UW_UnitVariantUnitEntry` and
  `UW_UnitVariantCard` are its rows. `AFactionPlayerController` opens it (`TryShowUnitVariantSelection`), previews
  each choice by calling `ARTSAsyncSpawner::SetPlayerUnitVariant` and respawning the hangar unit, saves on Confirm
  with `FRTS_Profile::SaveUnitVariantSelections` (only this faction's units change; the rest of the saved choices
  stay) and on Back puts the spawner back on Base.
- **Look:** `BP_W_UnitVariantSelectionMenu` (`/Game/RTS_Survival/FactionSelection/UI/`) only fills
  `FUnitVariantSelectionStyle` with the Frontstahl `Custom_Variant*` slice instances. Art and layout spec:
  `UISkins/_generator/variant_selection_parts.py`, `variant_selection_spec.md`, `mock_variant_selection.png`;
  import, instances and style: `UISkins/_generator/ui_variant_selection.py` (commandlet, re-runnable). Fonts are
  Roboto Bold / Regular (`RR_mainFont` is a display face); change them in the Blueprint's style.
- **Texts:** unit names come from the `DT_TrainingOptions` Title (markup stripped), card names and descriptions from
  the variant definition's `DisplayName` / `Description`, card pictures from its `Icon`, else its training image
  brush override, else the unit's table brush.
- **Check:** `-UnitVariantSelectionCapture` on `RTS_FactionSelection` (`-game -RenderOffscreen -windowed -ResX=1920
  -ResY=1080`) launches Breakthrough (or `-UnitVariantSelectionFaction=<value>`), checks the hangar spawns the base
  and then the Variant A class and writes `Saved/UnitVariantSelection/*.png`. It never confirms, so it does not write
  the player profile.
