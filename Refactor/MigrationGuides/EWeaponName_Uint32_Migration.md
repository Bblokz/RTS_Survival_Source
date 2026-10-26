# EWeaponName: uint8 → uint32 migration guide

Engine: UE 5.5.4 (source build at `D:/UnrealEngine/UE5.5_ReleaseBranch`).
Enum: `EWeaponName` in `Source/RTS_Survival/Weapons/WeaponData/WeaponSystems.h:33`. It has 181 entries, so 75 slots are left before uint8 runs out.

> **The rule this guide is built around:** do not change one line of the enum until every Blueprint's weapon value is written to a file that is committed to git (Phase 1). After that, do not open or save a Blueprint in the editor until the restore pass (Phase 4) has run.

---

## 0. Order of operations (checklist)

| # | Phase | Done when |
|---|-------|-----------|
| 1 | **Snapshot.** Add the migration commandlet and run `-Mode=Snapshot` with the enum still `uint8` | `EWeaponName_Snapshot.tsv` is committed, coverage is checked (§4.4), and tag `weaponname-uint32-pre` exists |
| 2 | **Spike.** Do the full C++ change on a throwaway branch and open 4 test Blueprints | You know which restore outcome (A, B or C, §5.3) applies |
| 3 | **C++ change.** Make the full header and source change on the real branch in one commit | Editor target builds; UHT is clean |
| 4 | **Restore.** Run `-Mode=Apply` with the editor closed, then fix the manual Blueprints | Apply log has no `FAILED` lines |
| 5 | **Verify.** Run `-Mode=Verify`, `CompileAllBlueprints`, then PIE | Verify diff is empty, there are no BP compile errors, and the log has no `Unknown weapon name` errors |

Do not skip or reorder phases 1 and 4.

### Phase 1 status (2026-10-25)

Done headless in a separate worktree (`D:/UE5Projects/RTS_WN`; superproject and Source branch `migration/weaponname-uint32`), so the live editors and the UECP bridge were not touched. Ignored marketplace folders under `Content/` are junctions to the main checkout; `Plugins/` is a plain copy of the plugin sources.

* The committed `Utils/Migration/WeaponNameMigrationCommandlet.cpp` is authoritative; it differs from §4.2 in four ways. It only loads packages whose bytes contain `EWeaponName` (Verify loads the snapshot's packages instead). It collects garbage with `RF_NoFlags`, because `GARBAGE_COLLECTION_KEEPFLAGS` is `RF_Standalone` in an editor commandlet and would keep every asset alive. It records weapon values inside unsplit struct pins and struct variables (`Kind=StructPin`). It walks instanced subobjects (`Sub:` / `CDOSub:` containers).
* Run it with `-DisablePlugins=BpGeneratorUltimate`; the run then does not register with the UECP bridge. The snapshot takes about 50 s.
* Coverage: 289 of 289 inventory assets have rows; no unresolved or stale names. `check_weaponname_snapshot.py` in this folder reruns the §4.4 checks.
* **Correction to §1.2 / §6:** many turrets do not type their gun into the Make-node pin. 261 pin literals (208 assets) are plain literals. 72 assets wire the pin instead, mostly from an `EWeaponName` **Blueprint variable** whose value lives on the CDO (66 member variables, plus 9 local variables). Apply now handles these (see §6.1). There are no Switch-on-`EWeaponName` nodes.
* `Ger_Turret_Sdkfz231_MG231` gets its gun from `BPF_ImpactHelpers` (its local variables are recorded). `DA_GlobalAbilitySystem` stores `Bomb_250Gr` in `GA_Barrage::M_WeaponData` (bucket 1).
* Not done: the `CompileAllBlueprints` baseline (§4.1 item 4) and the spot-check (§4.4 item 6). The full compile after the migration was judged per error instead (see below).

### Phases 2–5 status (2026-10-25, worktree branch `migration/weaponname-uint32`)

The spike and the real change were the same work, because the worktree is already isolated.

* **C++ (Source `89f042f8`).** Enum `UENUM()` / `uint32`, with the two `DisplayName` typos fixed. 10 forward declarations. `Weapons/WeaponData/WeaponNameConversion.h/.cpp` holds the FName↔enum helpers and the options list. The four init-struct `WeaponName` fields (which also cover the 11 derived structs) are `FName` with `GetWeaponNameEnum()`. 76 read sites call that accessor; the doc's two test files read `FWeaponData`, so they needed no change. The three bucket-1 fields and `FWeaponDescription::WeaponName` lost their Blueprint specifiers. `OnSetupWidgetForWeapon`, `BP_GetWeapon*String` and `ManuallySetSecondaryWeapon` take `FName`. UHT and the editor build are clean.
* **Apply (Content `e87b72a06`).** 269 assets saved, 0 `FAILED`. 66 + 9 variables retyped, 72 property values restored (6 more than the dry run: child Blueprints and the Wespe's child-actor turret template lost their override once the parent was saved). 215 pin literals restored (the other 46 were at the `T26_Mg` default). 77 wires relinked. Three fixes were needed first, all in Source `d4dd7d2f`:
  1. Regenerate the skeleton class after retyping. Otherwise the getters stay enum pins and the relink inserts "Enum to Name" nodes.
  2. Delete the "Enum to Name" nodes K2 itself inserts when it rebuilds a mismatched wire on load.
  3. Look nodes up by GUID **within the row's graph**: duplicated graphs keep their node GUIDs (`BPF_ImpactHelpers`, `Rus_BXP_MegaBunker_1`).
* **Verify.** 0 `CHANGED`. 2 `MISSING`, both expected: the unused `WeaponName` outputs of `Break Weapon Description` in `BP_W_WeaponDescriptionHorizontal`. Verify now reads the saved graphs without refreshing nodes; refreshing crashed the `RF_NoFlags` collection.
* **CompileAllBlueprints.** 84 errors in 32 assets, none caused by the migration. They are marketplace demo assets with missing parents, `BP_Mission_Trains_Start` calling a library function removed in `3f52506d`, `ArmoredCar_ToCopy`, `SquadOptim_Target` and `W_BP_ShieldBar`.
* **Runtime smoke test (2026-10-26)** instead of PIE (§7.3). `UnrealEditor-Cmd <uproject> <map> -game -nullrhi -nosound -unattended -benchmark -benchmarkseconds=90 -fps=30 -DisablePlugins=BpGeneratorUltimate` ran 701 frames each on `Ger_DestroyT35`, `RTS_GameStartTutorial` and `Ger_FirstMission`. Each run had 0 `Unknown weapon name`, 0 `ReportError` dialogs (under `-unattended` they log as `Message dialog closed`), 0 `LogRTS` errors and no crash. The same `Ger_DestroyT35` run on the unmigrated main checkout gives the same warning and error set; the only difference is the worktree's missing `PlayerProfile.sav`. In Git Bash, set `MSYS_NO_PATHCONV=1`, otherwise `/Game/...` map arguments become `C:/Program Files/Git/Game/...`.
* **Not done:** a hands-on PIE pass that checks weapon UI icons and fire, which a headless run cannot see.

### Merging while other branches add weapon Blueprints

`main` keeps adding vehicles while this branch exists. Their Blueprints use `EWeaponName` pins, which this branch's C++ no longer has. At merge time:

1. Close the editors on `main`. On `main` (enum still `uint8`), build the commandlet and run `-Mode=Snapshot` to a new TSV.
2. Keep only rows whose package is new or changed on `main` since `16891bc9` (Source) / `8948a4c66` (superproject).
3. Merge: append-only enum entries merge as text. Content conflicts mean an asset changed on both sides. Take `main`'s version and re-apply it from the new TSV.
4. Build, then run `-Mode=Apply` and `-Mode=Verify` with the new TSV. Its rows record which vehicle uses which weapon, so they don't depend on asking the other agent.

---

## 1. What actually changes, and what does not

### 1.1 Stored values are *not* at risk from the widening itself

The research note said assets "store the byte value". That is not true for this project, and you should not spend time on it:

* Every C++ field is a plain `enum class` UPROPERTY, so it is an `FEnumProperty`. Tagged property serialization in editor assets stores the **enumerator name**, not the byte. You can see this in the assets: `Ger_Turret_PanzerIV_F1.uasset` contains the name `EWeaponName::KwK37_75MM`.
* Blueprint graph pin literals store the **short enumerator name as a string**. `Ger_Weapon_Kar98.uasset` contains `Kar_98k` and `Ger_Weapon_Kar98Sniper.uasset` contains `Kar_Sniper`. There is no byte in either case.
* Cooked builds use unversioned (by-value) serialization, but they are re-cooked from the editor assets, so this doesn't matter.
* The `EnumRedirects` in `Config/DefaultEngine.ini:394-396` (`Puma_50MM`, `Puma_Mg`, `Ba_12MM`) keep working because the enum's path does not change.

So **widening `uint8` to `uint32` by itself would lose nothing.**

### 1.2 What *is* at risk: `BlueprintType`

UHT only accepts `BlueprintType` on enums with a `uint8` base. A `uint32` enum must be a plain `UENUM()`. As a result, every place where Blueprints can *see* the type stops compiling:

* `UPROPERTY(BlueprintReadWrite / BlueprintReadOnly)` with type `EWeaponName` → UHT error.
* `UFUNCTION(BlueprintCallable / BlueprintPure / BlueprintImplementableEvent)` with an `EWeaponName` parameter or return → UHT error.
* Blueprint pins, variables, Switch nodes and enum literals typed `EWeaponName` → the Blueprint no longer compiles. When the node is reconstructed, **its pin literal can be dropped**.

The last point is where data can actually be lost. A turret Blueprint stores the weapon *only* as a default-value string on the `WeaponName` pin of a `Make InitWeaponState…` node. That pin is wired into `SetupProjectileWeapon` / `SetupTraceWeapon` / etc. in BeginPlay. If the node is rebuilt with the pin gone or retyped and someone saves the Blueprint, the value is gone. Nothing in C++ or config records which turret used which gun.

**This is why Phase 1 exists.**

### 1.3 Expected UHT errors

Use these to recognise a half-finished change (the exact wording may differ slightly):

* `Invalid BlueprintType enum base - currently only uint8 supported` means the enum is `uint32` and still `BlueprintType`.
* `Type 'EWeaponName' is not supported by blueprint. … Property: WeaponName` / `… Function: X Parameter: Y` means `BlueprintType` was removed but an exposure in §2 was not fixed.

UHT checks all headers together, so **the enum change and every exposure fix have to land in one compile**. You cannot convert the code one struct at a time while the enum is `uint32`.

---

## 2. Inventory: C++ exposure (verified line by line)

### 2.1 Bucket 1: Details-panel fields (keep the enum type, remove only the `Blueprint*` specifier)

The Details panel combo box uses `UEnum` reflection, not `BlueprintType`. These values are stored by name in the CDO, so they should survive without re-entry. Phase 2 checks this.

| Field | File:line | Current | New |
|---|---|---|---|
| `AWeaponPickup::WeaponName` | `PickupItems/Items/WeaponPickUp/WeaponPickup.h:34-35` | `EditDefaultsOnly, BlueprintReadOnly` | `EditDefaultsOnly` |
| `FBombBayData::WeaponName` | `Weapons/BombComponent/BombBayData/BombBayData.h:77-78` | `EditDefaultsOnly, BlueprintReadOnly` | `EditDefaultsOnly` |
| `FWeaponData::WeaponName` | `Weapons/WeaponData/WeaponData.h:156-157` | `BlueprintReadWrite, EditDefaultsOnly` | `EditDefaultsOnly` |

Graph nodes that *read* these fields show up in the snapshot as `Direction=Out` rows (`K2Node_VariableGet`, `Break WeaponData`, `Break BombBayData`). Those nodes stop compiling once `BlueprintReadOnly` is gone. For each field that has such a row, add a `UFUNCTION(BlueprintPure) FName Get…WeaponNameAsFName() const` and repoint the node by hand (§6.2). After the migration these rows come back as `VERIFY MISSING`; that is how you check none were left behind. `FWeaponData` is filled in C++ (`CPPGameState.cpp:840…1368`), so it keeps the enum.

### 2.2 Bucket 2: Graph-authored init structs (enum → `FName`, same property name)

Every turret, hull weapon, aircraft weapon, attached-ability weapon and infantry weapon sets its gun by filling one of these structs in a graph. They are not used as `UPROPERTY` members on any actor (I checked). They exist only as Make-struct nodes, split pins, or Blueprint variables.

| Struct | File:line | Notes |
|---|---|---|
| `FInitWeaponStateDirectHit::WeaponName` | `Weapons/WeaponData/WeaponData.h:1000-1001` | **Base of 11 structs**: `FInitWeaponStatTrace` (1075), `FInitWeaponStateProjectile` (1198), `…RailwayCannon` (1225), `…ArchProjectile` (1525), `…SplitterArchProjectile` (1534), `…RocketProjectile` (1729), `…HomingMissile` (1861), `…VerticalRocketProjectile` (1959), `FInitWeaponStateMultiProjectile` (`MultiProjectileWeapon/UWeaponStateMultiProjectile.h:13`), `FInitWeaponStateMultiTrace` (`MultiTraceWeapon/WeaponStateMultiTrace.h:14`). One field change covers all of them. |
| `FInitWeaponStateRailgun` | `WeaponData.h:1233-1240` | Has no field of its own. It **nests** `FInitWeaponStateProjectile ProjectileWeaponParameters`, so its pin shows up as a nested or split pin (`ProjectileWeaponParameters_WeaponName`). |
| `FInitWeaponStateLaser::WeaponName` | `WeaponData.h:2201-2202` | |
| `FInitWeaponStateMultiHitLaser::WeaponName` | `WeaponData.h:2225-2226` | |
| `FInitWeaponStateFlameThrower::WeaponName` | `Weapons/FlameThrowerWeapon/FlameThrowerInit.h:17-18` | |

Blueprint entry points that take these structs: `Setup*Weapon` on `ACPPTurretsMaster`, `AStandaloneTurret`, `AInfantryWeaponMaster`, `UHullWeaponComponent`, `UAircraftWeapon` and `UAttachedWeaponAbilityComponent`. Their signatures **don't change**; only the struct field changes.

### 2.3 Bucket 3: Blueprint function signatures (enum → `FName`, same parameter name)

| Function | File:line | Blueprints that use it (from asset grep) |
|---|---|---|
| `URTSBlueprintFunctionLibrary::BP_GetWeaponEnumAsString` | `Utils/RTSBlueprintFunctionLibrary.h:136-137` | `GameUI/WeaponUI/BP_W_WeaponItem` |
| `URTSBlueprintFunctionLibrary::BP_GetWeaponDisplayNameString` | `Utils/RTSBlueprintFunctionLibrary.h:210-211` | `GameUI/WeaponUI/BP_W_WeaponItem` |
| `UW_WeaponItem::OnSetupWidgetForWeapon` (BlueprintImplementableEvent) | `GameUI/ActionUI/WeaponUI/W_WeaponItem.h:56-57` | `GameUI/WeaponUI/BP_W_WeaponItem` (the **event node** pin changes type) |
| `USecondaryWeapon::ManuallySetSecondaryWeapon` | `Weapons/InfantryWeapon/SecondaryWeaponComp/SecondaryWeapon.h:41-44` | `Squads/Ger/Barracks_Infantry/SteelFistAssaultSquad/Ger_Inf_Faust`, `Squads/SquadUnits/Ger/Ger_Inf_Mauser` |
| `UW_WeaponDescription::WeaponName` (BlueprintReadOnly) | `GameUI/ActionUI/WeaponUI/OnHoverDescription/W_WeaponDescription.h:39-40` | Use the snapshot to find which widget reads it. Change to `UPROPERTY()` and add a `BlueprintPure` FName getter. |

**Not exposed:** `ACPPGameState::GetWeaponDataOfPlayer` / `UpgradeWeaponDataForPlayer` (`CPPGameState.h:110,119`) are plain C++. The `UFUNCTION` at :107 belongs to `StartClock`. `TMap<EWeaponName, FWeaponData>` (`CPPGameState.h:273,276`) is `SkipSerialization`, and enum keys hash at any width. `USecondaryWeapon::M_WeaponName` is `UPROPERTY()` only. Leave these alone.

### 2.4 Forward declarations (all ten must change in the same commit)

```
Game/GameState/CPPGameState.h:41
GameUI/ActionUI/ActionUIManager/ActionUIManager.h:20
GameUI/ActionUI/WeaponUI/OnHoverDescription/W_WeaponDescription.h:19
PickupItems/Items/WeaponPickUp/WeaponPickup.h:10
Player/CPPController.h:81
Units/SquadController.h:32
Units/Squads/SquadUnit/SquadUnit.h:34
Utils/RTSBlueprintFunctionLibrary.h:49
Weapons/InfantryWeapon/SecondaryWeaponComp/SecondaryWeapon.h:11
Weapons/WeaponData/WeaponData.h:36
```
`enum class EWeaponName : uint8;` → `enum class EWeaponName : uint32;`. A mismatch is a hard compile error, so you can't miss one.

### 2.5 C++ that reads the bucket-2 field

These sites read `Params.WeaponName` as an enum today. After the change they must call `GetWeaponNameEnum()` (§5.2). Counts are `.WeaponName` hits:

```
13 Weapons/Turret/StandaloneTurret/StandaloneTurret.cpp
13 Weapons/Turret/CPPTurretsMaster.cpp
13 Weapons/InfantryWeapon/InfantryWeaponMaster.cpp
13 RTSComponents/AbilityComponents/AttachedWeaponAbilityComponent/AttachedWeaponAbilityComponent.cpp
11 Weapons/HullWeaponComponent/HullWeaponComponent.cpp
11 Weapons/AircraftWeapon/AircraftWeapon.cpp
 1 Units/TeamWeapons/Tests/TeamWeaponDigInTestSubsystem.cpp   (constructs an init struct)
 1 Buildings/BuildingExpansion/GunPit/Tests/GunPitTestScenario.cpp (constructs an init struct)
```
The compiler finds every one of them for you: `EWeaponName x = Params.WeaponName` stops compiling once the field is an `FName`.

---

## 3. Inventory: assets

Here is `grep -rla "EWeaponName" Content --include=*.uasset`, run on 2026-10-20: **264 assets, 0 `.umap` files, 0 in `Plugins/`**. This is the *inventory*. The commandlet in Phase 1 is the *record*.

| Asset group (name prefix) | Count | Typical storage |
|---|---|---|
| `Ger_Turret_*` | 37 | Make `InitWeaponState*` pin → `Setup*Weapon` |
| `Rus_Turret_*` | 26 | same |
| `RTS_BP_*` (embedded / pit turrets, `RTS_BP_GameState`, BA turret) | 28 | same; GameState references the weapon-data maps |
| `Rus_Weapon_*` / `Ger_Weapon_*` (infantry weapons) | 18 / 17 | Make `InitWeaponStatTrace` etc. → `SetupTraceWeapon` |
| `Rus_Vehicle_*` / `Ger_Vehicle_*` (hull weapons) | 15 / 11 | Make struct → `UHullWeaponComponent::Setup*` |
| `Ger_EmbTurret_*`, `EmbTurret_*`, `Rus_EmbTurret_*`, `Rus_EIT_*` | 14 / 10 / 2 / 3 | Make struct |
| `Rus_BXP_*` (bunker expansions) | 12 | Make struct |
| `Ger_Aircraft_*` and others under `Aircraft/` | 10+ | Make struct, `BombBayData` (bucket 1) |
| `Rus_WeaponPickup_*` / `Ger_WeaponPickup_*` | 10 / 7 | CDO `WeaponName` (bucket 1) |
| `BP_TW_*`, `Rus_TW_*`, `Ger_TW_*` (team weapons) | 9 / 4 / 4 | Make struct |
| `BP_Train_*`, `BP_StandaloneTurret_*`, `Ger_Inf_*`, `BP_W_*`, misc | rest | mixed (see bucket 3) |

So this is not only turrets and infantry weapons: vehicles, bunkers, pits, aircraft, trains, team weapons, pickups and UI widgets are all affected. A turret Blueprint usually holds **more than one** weapon (main gun plus coax MG plus hull MG), each on its own Make node. The record must therefore be per node, not per asset.

---

## 4. Phase 1: Snapshot (enum still uint8, nothing else changed)

### 4.1 Preconditions

1. You are on `main` with a clean working tree (`git status` is empty) and LFS objects are pulled.
2. **Freeze Blueprint edits.** Nobody edits weapon-related Blueprints between the snapshot and Phase 4. If someone does, take the snapshot again.
3. Close **all** editor instances. Two editors open at once swap the UECP bridge, and the commandlet must not race an editor that has the same packages loaded.
4. Make sure the project compiles and **all Blueprints compile today** (`-run=CompileAllBlueprints`, §7.2). Save that log as your baseline. A Blueprint that is already broken produces unreliable snapshot rows. Fix those first or write them down.

### 4.2 Add the migration commandlet

This is temporary tooling. It touches no gameplay code and compiles into the existing `RTS_Survival` module, which already links `UnrealEd` and `BlueprintGraph` when `Target.bBuildEditor` is set (`RTS_Survival.Build.cs:67-70`). All editor code is inside `#if WITH_EDITOR`, so game targets still build.

It has three modes:

* `Snapshot` writes one row per weapon value: every pin (input and output), Blueprint variable, local variable, Switch case, CDO, component template and child-actor template property, DataAsset, DataTable row and User Defined Struct default. It runs before the change.
* `Apply` reads the snapshot and writes each pin literal back onto the **same node** (matched by `NodeGuid`, which stays stable across reconstruction), then compiles and saves. It runs after the change.
* `Verify` collects again and diffs against the snapshot. It runs after Apply, and you can rerun it any time as a lint.

`Source/RTS_Survival/Utils/Migration/WeaponNameMigrationCommandlet.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WeaponNameMigrationCommandlet.generated.h"

/**
 * @brief Records, restores and verifies every EWeaponName value authored in Blueprints and data assets.
 * Run with UnrealEditor-Cmd -run=WeaponNameMigration -Mode=Snapshot|Apply|Verify -Tsv=<path>.
 * Temporary tooling for the uint8 -> uint32 migration (Source/Refactor/MigrationGuides/EWeaponName_Uint32_Migration.md).
 */
UCLASS()
class RTS_SURVIVAL_API UWeaponNameMigrationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWeaponNameMigrationCommandlet();

	virtual int32 Main(const FString& Params) override;
};
```

`Source/RTS_Survival/Utils/Migration/WeaponNameMigrationCommandlet.cpp`

```cpp
#include "WeaponNameMigrationCommandlet.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/ChildActorComponent.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_SwitchEnum.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
// UE 5.5 location; before 5.5 this was Engine/UserDefinedStruct.h.
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/SavePackage.h"

namespace WeaponNameMigration
{
	const TCHAR* const TsvHeader =
		TEXT("AssetPackage\tKind\tContainer\tNodeClass\tNodeTitle\tNodeGuid\tPinOrPropertyPath\tDirection\tRawValue\tCanonicalValue\tNumericValue\tLinkSource");
	constexpr int32 TsvColumnCount = 12;
	// Loading every Blueprint without collecting runs the commandlet out of memory on this project.
	constexpr int32 AssetsPerGarbageCollect = 50;

	struct FRow
	{
		FString AssetPackage;
		// Pin | SwitchCase | Variable | LocalVariable | Property
		FString Kind;
		// Graph name for pins; CDO / SCS:<Var> / ICH:<Name> / ChildActor:<Var> / Row:<RowName> for properties.
		FString Container;
		FString NodeClass;
		FString NodeTitle;
		FString NodeGuid;
		FString PinOrPropertyPath;
		// In | Out for pins, "-" otherwise. Only unwired In pins hold a literal; Out rows show where a graph reads the value.
		FString Direction = TEXT("-");
		// Exactly as stored: "Kar_98k", "EWeaponName::KwK37_75MM", or a stale redirected name.
		FString RawValue;
		// Current enumerator short name after EnumRedirects. Empty means it could not be resolved; investigate.
		FString CanonicalValue;
		int64 NumericValue = INDEX_NONE;
		// Set when an input pin is wired: the value comes from another node, so there is no literal to restore.
		FString LinkSource;

		FString GetKey() const
		{
			return AssetPackage + TEXT("|") + Kind + TEXT("|") + Container + TEXT("|") + NodeGuid + TEXT("|") + PinOrPropertyPath;
		}
	};

	FString Sanitize(const FString& Value)
	{
		FString Result = Value;
		Result.ReplaceInline(TEXT("\t"), TEXT(" "));
		Result.ReplaceInline(TEXT("\r"), TEXT(" "));
		Result.ReplaceInline(TEXT("\n"), TEXT(" "));
		return Result;
	}

	void ResolveValue(const UEnum* WeaponEnum, FRow& Row)
	{
		// GetValueByNameString accepts the short and the prefixed form and follows the ini EnumRedirects.
		Row.NumericValue = WeaponEnum->GetValueByNameString(Row.RawValue);
		if (Row.NumericValue == INDEX_NONE)
		{
			return;
		}
		Row.CanonicalValue = WeaponEnum->GetNameStringByValue(Row.NumericValue);
	}

	bool IsWeaponNamePin(const UEdGraphPin* Pin, const UEnum* WeaponEnum)
	{
		if (Pin->PinType.PinSubCategoryObject == WeaponEnum)
		{
			return true;
		}
		// After the migration the struct and function pins are FName pins that kept the WeaponName name.
		return Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name
			&& Pin->PinName.ToString().EndsWith(TEXT("WeaponName"));
	}

	FString GetLinkSource(const UEdGraphPin* Pin)
	{
		if (Pin->Direction != EGPD_Input || Pin->LinkedTo.IsEmpty())
		{
			return FString();
		}
		const UEdGraphPin* SourcePin = Pin->LinkedTo[0];
		const UEdGraphNode* SourceNode = SourcePin->GetOwningNode();
		return SourceNode->GetNodeTitle(ENodeTitleType::ListView).ToString() + TEXT(".") + SourcePin->PinName.ToString();
	}

	void CollectSwitchCases(const UK2Node_SwitchEnum* SwitchNode, const FString& Package, const FString& GraphName,
	                        TArray<FRow>& OutRows)
	{
		for (const UEdGraphPin* Pin : SwitchNode->Pins)
		{
			if (Pin->Direction != EGPD_Output || Pin->LinkedTo.IsEmpty())
			{
				continue;
			}
			FRow Row;
			Row.AssetPackage = Package;
			Row.Kind = TEXT("SwitchCase");
			Row.Container = GraphName;
			Row.NodeClass = SwitchNode->GetClass()->GetName();
			Row.NodeTitle = Sanitize(SwitchNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
			Row.NodeGuid = SwitchNode->NodeGuid.ToString(EGuidFormats::Digits);
			Row.PinOrPropertyPath = Pin->PinName.ToString();
			Row.RawValue = Pin->PinName.ToString();
			OutRows.Add(Row);
		}
	}

	void CollectNodeRows(const UEdGraphNode* Node, const FString& Package, const FString& GraphName,
	                     const UEnum* WeaponEnum, TArray<FRow>& OutRows)
	{
		if (const UK2Node_SwitchEnum* SwitchNode = Cast<UK2Node_SwitchEnum>(Node))
		{
			if (SwitchNode->Enum == WeaponEnum)
			{
				CollectSwitchCases(SwitchNode, Package, GraphName, OutRows);
			}
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (not IsWeaponNamePin(Pin, WeaponEnum))
			{
				continue;
			}
			FRow Row;
			Row.AssetPackage = Package;
			Row.Kind = TEXT("Pin");
			Row.Container = GraphName;
			Row.NodeClass = Node->GetClass()->GetName();
			Row.NodeTitle = Sanitize(Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
			Row.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
			Row.PinOrPropertyPath = Pin->PinName.ToString() + (Pin->bOrphanedPin ? TEXT(" [ORPHANED]") : TEXT(""));
			Row.Direction = Pin->Direction == EGPD_Input ? TEXT("In") : TEXT("Out");
			Row.RawValue = Pin->DefaultValue;
			Row.LinkSource = Sanitize(GetLinkSource(Pin));
			ResolveValue(WeaponEnum, Row);
			OutRows.Add(Row);
		}
	}

	void CollectVariableRows(const TArray<FBPVariableDescription>& Variables, const FString& Package,
	                         const FString& Kind, const FString& Container, const UEnum* WeaponEnum,
	                         TArray<FRow>& OutRows)
	{
		for (const FBPVariableDescription& Variable : Variables)
		{
			if (Variable.VarType.PinSubCategoryObject != WeaponEnum)
			{
				continue;
			}
			FRow Row;
			Row.AssetPackage = Package;
			Row.Kind = Kind;
			Row.Container = Container;
			Row.PinOrPropertyPath = Variable.VarName.ToString();
			Row.RawValue = Variable.DefaultValue;
			ResolveValue(WeaponEnum, Row);
			OutRows.Add(Row);
		}
	}

	bool TryReadWeaponProperty(const FProperty* Property, const void* Value, const UEnum* WeaponEnum, FString& OutRaw)
	{
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			if (EnumProperty->GetEnum() != WeaponEnum)
			{
				return false;
			}
			const int64 IntValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value);
			OutRaw = WeaponEnum->GetNameStringByValue(IntValue);
			return true;
		}
		const FNameProperty* NameProperty = CastField<FNameProperty>(Property);
		if (not NameProperty || not NameProperty->GetName().EndsWith(TEXT("WeaponName")))
		{
			return false;
		}
		OutRaw = NameProperty->GetPropertyValue(Value).ToString();
		return true;
	}

	void CollectPropertyRows(const UStruct* Struct, const void* Container, const FString& Package,
	                         const FString& ContainerLabel, const UEnum* WeaponEnum, TArray<FRow>& OutRows)
	{
		if (not Struct || not Container)
		{
			return;
		}
		for (TPropertyValueIterator<const FProperty> It(Struct, Container); It; ++It)
		{
			FString RawValue;
			if (not TryReadWeaponProperty(It.Key(), It.Value(), WeaponEnum, RawValue))
			{
				continue;
			}
			FRow Row;
			Row.AssetPackage = Package;
			Row.Kind = TEXT("Property");
			Row.Container = ContainerLabel;
			Row.PinOrPropertyPath = It.GetPropertyPathDebugString();
			Row.RawValue = RawValue;
			ResolveValue(WeaponEnum, Row);
			OutRows.Add(Row);
		}
	}

	void CollectComponentTemplates(UBlueprint* Blueprint, const FString& Package, const UEnum* WeaponEnum,
	                               TArray<FRow>& OutRows)
	{
		if (Blueprint->SimpleConstructionScript)
		{
			for (const USCS_Node* ScsNode : Blueprint->SimpleConstructionScript->GetAllNodes())
			{
				UActorComponent* Template = ScsNode ? ScsNode->ComponentTemplate : nullptr;
				if (not Template)
				{
					continue;
				}
				const FString Label = TEXT("SCS:") + ScsNode->GetVariableName().ToString();
				CollectPropertyRows(Template->GetClass(), Template, Package, Label, WeaponEnum, OutRows);
				const UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>(Template);
				AActor* ChildTemplate = ChildActorComponent ? ChildActorComponent->GetChildActorTemplate() : nullptr;
				if (ChildTemplate)
				{
					CollectPropertyRows(ChildTemplate->GetClass(), ChildTemplate, Package,
					                    TEXT("ChildActor:") + ScsNode->GetVariableName().ToString(), WeaponEnum, OutRows);
				}
			}
		}
		UInheritableComponentHandler* Handler = Blueprint->GetInheritableComponentHandler(false);
		if (not Handler)
		{
			return;
		}
		TArray<UActorComponent*> Templates;
		Handler->GetAllTemplates(Templates);
		for (UActorComponent* Template : Templates)
		{
			CollectPropertyRows(Template->GetClass(), Template, Package, TEXT("ICH:") + Template->GetName(),
			                    WeaponEnum, OutRows);
		}
	}

	/**
	 * @param bRefreshNodes True after the migration: loading does not guarantee nodes were reconstructed against
	 * the new FName types, and reading the stale enum pins would report a false "verified".
	 */
	void CollectBlueprint(UBlueprint* Blueprint, const UEnum* WeaponEnum, const bool bRefreshNodes, TArray<FRow>& OutRows)
	{
		if (bRefreshNodes)
		{
			FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
		}
		const FString Package = Blueprint->GetOutermost()->GetName();
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (const UEdGraph* Graph : Graphs)
		{
			if (not Graph)
			{
				continue;
			}
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				if (not Node)
				{
					continue;
				}
				CollectNodeRows(Node, Package, Graph->GetName(), WeaponEnum, OutRows);
				if (const UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
				{
					CollectVariableRows(Entry->LocalVariables, Package, TEXT("LocalVariable"), Graph->GetName(),
					                    WeaponEnum, OutRows);
				}
			}
		}
		CollectVariableRows(Blueprint->NewVariables, Package, TEXT("Variable"), TEXT("Blueprint"), WeaponEnum, OutRows);
		if (UClass* GeneratedClass = Blueprint->GeneratedClass)
		{
			UObject* DefaultObject = GeneratedClass->GetDefaultObject();
			CollectPropertyRows(GeneratedClass, DefaultObject, Package, TEXT("CDO"), WeaponEnum, OutRows);
		}
		CollectComponentTemplates(Blueprint, Package, WeaponEnum, OutRows);
	}

	void CollectDataTable(const UDataTable* Table, const UEnum* WeaponEnum, TArray<FRow>& OutRows)
	{
		const FString Package = Table->GetOutermost()->GetName();
		for (const TPair<FName, uint8*>& RowPair : Table->GetRowMap())
		{
			CollectPropertyRows(Table->GetRowStruct(), RowPair.Value, Package,
			                    TEXT("Row:") + RowPair.Key.ToString(), WeaponEnum, OutRows);
		}
	}

	// Never saves: RefreshAllNodes only changes the in-memory Blueprint.
	TArray<FRow> CollectAll(const UEnum* WeaponEnum, const bool bRefreshNodes)
	{
		IAssetRegistry& AssetRegistry =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		AssetRegistry.SearchAllAssets(true);

		FARFilter Filter;
		Filter.PackagePaths.Add(TEXT("/Game"));
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;
		// bRecursiveClasses also picks up UWidgetBlueprint and other UBlueprint subclasses.
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UDataAsset::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UDataTable::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UUserDefinedStruct::StaticClass()->GetClassPathName());
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(Filter, Assets);

		TArray<FRow> Rows;
		for (int32 AssetIndex = 0; AssetIndex < Assets.Num(); ++AssetIndex)
		{
			if (AssetIndex > 0 && AssetIndex % AssetsPerGarbageCollect == 0)
			{
				CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
			}
			UObject* Asset = Assets[AssetIndex].GetAsset();
			if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
			{
				CollectBlueprint(Blueprint, WeaponEnum, bRefreshNodes, Rows);
			}
			else if (const UDataTable* Table = Cast<UDataTable>(Asset))
			{
				CollectDataTable(Table, WeaponEnum, Rows);
			}
			else if (const UUserDefinedStruct* UserStruct = Cast<UUserDefinedStruct>(Asset))
			{
				CollectPropertyRows(UserStruct, UserStruct->GetDefaultInstance(), UserStruct->GetOutermost()->GetName(),
				                    TEXT("StructDefaults"), WeaponEnum, Rows);
			}
			else if (Asset)
			{
				CollectPropertyRows(Asset->GetClass(), Asset, Asset->GetOutermost()->GetName(), TEXT("Asset"),
				                    WeaponEnum, Rows);
			}
		}
		return Rows;
	}

	bool WriteTsv(const TArray<FRow>& Rows, const FString& Path)
	{
		TArray<FString> Lines;
		Lines.Add(TsvHeader);
		for (const FRow& Row : Rows)
		{
			Lines.Add(FString::Join(TArray<FString>{
				Row.AssetPackage, Row.Kind, Row.Container, Row.NodeClass, Row.NodeTitle, Row.NodeGuid,
				Row.PinOrPropertyPath, Row.Direction, Row.RawValue, Row.CanonicalValue, LexToString(Row.NumericValue),
				Row.LinkSource}, TEXT("\t")));
		}
		return FFileHelper::SaveStringArrayToFile(Lines, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	TArray<FRow> ReadTsv(const FString& Path)
	{
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *Path);
		TArray<FRow> Rows;
		for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
		{
			TArray<FString> Cells;
			Lines[LineIndex].ParseIntoArray(Cells, TEXT("\t"), false);
			if (Cells.Num() != TsvColumnCount)
			{
				UE_LOG(LogTemp, Error, TEXT("WeaponNameMigration: malformed TSV line %d"), LineIndex + 1);
				continue;
			}
			FRow Row;
			Row.AssetPackage = Cells[0]; Row.Kind = Cells[1]; Row.Container = Cells[2]; Row.NodeClass = Cells[3];
			Row.NodeTitle = Cells[4]; Row.NodeGuid = Cells[5]; Row.PinOrPropertyPath = Cells[6];
			Row.Direction = Cells[7]; Row.RawValue = Cells[8]; Row.CanonicalValue = Cells[9];
			LexFromString(Row.NumericValue, *Cells[10]); Row.LinkSource = Cells[11];
			Rows.Add(Row);
		}
		return Rows;
	}

	UEdGraphNode* FindNodeByGuid(UBlueprint* Blueprint, const FString& GuidString)
	{
		FGuid Guid;
		if (not FGuid::Parse(GuidString, Guid))
		{
			return nullptr;
		}
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (const UEdGraph* Graph : Graphs)
		{
			UEdGraphNode* const* Found = Graph
				? Graph->Nodes.FindByPredicate([&Guid](const UEdGraphNode* Node) { return Node && Node->NodeGuid == Guid; })
				: nullptr;
			if (Found)
			{
				return *Found;
			}
		}
		return nullptr;
	}

	FString GetSnapshotPinName(const FRow& Row)
	{
		FString PinName = Row.PinOrPropertyPath;
		PinName.RemoveFromEnd(TEXT(" [ORPHANED]"));
		return PinName;
	}

	// @return Whether an orphan was removed, so the caller knows the Blueprint must be saved.
	bool RemoveOrphanedPin(UEdGraphNode* Node, const FName PinName)
	{
		UEdGraphPin* const* Orphan = Node->Pins.FindByPredicate([PinName](const UEdGraphPin* Pin)
		{
			return Pin->bOrphanedPin && Pin->PinName == PinName && Pin->LinkedTo.IsEmpty();
		});
		if (not Orphan)
		{
			return false;
		}
		Node->Modify();
		Node->RemovePin(*Orphan);
		return true;
	}

	// Only literals on unwired input pins can be restored automatically; everything else is a manual fix.
	bool IsRestorableRow(const FRow& Row)
	{
		return Row.Kind == TEXT("Pin") && Row.Direction == TEXT("In") && Row.LinkSource.IsEmpty()
			&& not Row.CanonicalValue.IsEmpty();
	}

	UEdGraphPin* FindLivePin(UEdGraphNode* Node, const FName PinName)
	{
		UEdGraphPin* const* LivePin = Node->Pins.FindByPredicate([PinName](const UEdGraphPin* Pin)
		{
			return not Pin->bOrphanedPin && Pin->PinName == PinName;
		});
		return LivePin ? *LivePin : nullptr;
	}

	// Returns the migrated FName pin. A pin that is still the old enum pin means the node was not reconstructed
	// yet; writing to it would be undone by the next reconstruct, so reconstruct first and refuse if it stays stale.
	UEdGraphPin* FindMigratedPin(UEdGraphNode* Node, const FName PinName)
	{
		UEdGraphPin* LivePin = FindLivePin(Node, PinName);
		if (LivePin && LivePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
		{
			return LivePin;
		}
		Node->ReconstructNode();
		LivePin = FindLivePin(Node, PinName);
		if (LivePin && LivePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
		{
			return LivePin;
		}
		return nullptr;
	}

	// @return Whether the Blueprint was modified and must be saved.
	bool ApplyRowToBlueprint(UBlueprint* Blueprint, const FRow& Row)
	{
		UEdGraphNode* Node = FindNodeByGuid(Blueprint, Row.NodeGuid);
		const FName PinName(*GetSnapshotPinName(Row));
		UEdGraphPin* MigratedPin = Node ? FindMigratedPin(Node, PinName) : nullptr;
		if (not MigratedPin)
		{
			UE_LOG(LogTemp, Error, TEXT("WeaponNameMigration FAILED: no migrated Name pin %s on node %s (%s) in %s"),
			       *PinName.ToString(), *Row.NodeTitle, *Row.NodeGuid, *Row.AssetPackage);
			return false;
		}
		const bool bRemovedOrphan = RemoveOrphanedPin(Node, PinName);
		if (MigratedPin->DefaultValue == Row.CanonicalValue)
		{
			return bRemovedOrphan;
		}
		Node->Modify();
		GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*MigratedPin, Row.CanonicalValue);
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration restored %s.%s = %s in %s"),
		       *Row.NodeTitle, *PinName.ToString(), *Row.CanonicalValue, *Row.AssetPackage);
		return true;
	}

	bool SavePackageToDisk(UPackage* Package)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, nullptr, *Filename, SaveArgs);
	}

	void ApplyBlueprintRows(const FString& Package, const TArray<const FRow*>& Rows)
	{
		const FString ObjectPath = Package + TEXT(".") + FPackageName::GetShortName(Package);
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
		if (not Blueprint)
		{
			UE_LOG(LogTemp, Error, TEXT("WeaponNameMigration FAILED: could not load %s"), *ObjectPath);
			return;
		}
		// Make K2 rebuild every node against the new FName types before we look at any pin.
		FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
		bool bAnyChanged = false;
		for (const FRow* Row : Rows)
		{
			bAnyChanged |= ApplyRowToBlueprint(Blueprint, *Row);
		}
		if (not bAnyChanged)
		{
			return;
		}
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (not SavePackageToDisk(Blueprint->GetOutermost()))
		{
			UE_LOG(LogTemp, Error, TEXT("WeaponNameMigration FAILED: could not save %s"), *Package);
		}
	}

	int32 RunApply(const FString& TsvPath)
	{
		const TArray<FRow> Rows = ReadTsv(TsvPath);
		TMap<FString, TArray<const FRow*>> RowsByPackage;
		for (const FRow& Row : Rows)
		{
			if (IsRestorableRow(Row))
			{
				RowsByPackage.FindOrAdd(Row.AssetPackage).Add(&Row);
			}
		}
		for (const TPair<FString, TArray<const FRow*>>& Pair : RowsByPackage)
		{
			ApplyBlueprintRows(Pair.Key, Pair.Value);
			// Save per package and collect garbage so a failure late in the run does not lose earlier work.
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
		return 0;
	}

	int32 RunVerify(const UEnum* WeaponEnum, const FString& TsvPath)
	{
		TMap<FString, FRow> AfterByKey;
		for (const FRow& Row : CollectAll(WeaponEnum, true))
		{
			FRow Normalised = Row;
			Normalised.PinOrPropertyPath.RemoveFromEnd(TEXT(" [ORPHANED]"));
			AfterByKey.Add(Normalised.GetKey(), Normalised);
		}
		int32 ProblemCount = 0;
		for (FRow Before : ReadTsv(TsvPath))
		{
			Before.PinOrPropertyPath.RemoveFromEnd(TEXT(" [ORPHANED]"));
			const FRow* After = AfterByKey.Find(Before.GetKey());
			if (not After)
			{
				UE_LOG(LogTemp, Warning, TEXT("VERIFY MISSING  %s  %s  %s = %s"), *Before.AssetPackage,
				       *Before.NodeTitle, *Before.PinOrPropertyPath, *Before.CanonicalValue);
				++ProblemCount;
				continue;
			}
			if (After->CanonicalValue != Before.CanonicalValue || After->LinkSource != Before.LinkSource)
			{
				UE_LOG(LogTemp, Warning, TEXT("VERIFY CHANGED  %s  %s  %s : %s -> %s"), *Before.AssetPackage,
				       *Before.NodeTitle, *Before.PinOrPropertyPath, *Before.CanonicalValue, *After->CanonicalValue);
				++ProblemCount;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration verify: %d problem rows"), ProblemCount);
		return ProblemCount == 0 ? 0 : 1;
	}
}
#endif

UWeaponNameMigrationCommandlet::UWeaponNameMigrationCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UWeaponNameMigrationCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WeaponNameMigration;
	const UEnum* WeaponEnum = StaticEnum<EWeaponName>();
	FString Mode;
	FString TsvPath;
	if (not WeaponEnum || not FParse::Value(*Params, TEXT("Mode="), Mode) || not FParse::Value(*Params, TEXT("Tsv="), TsvPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Usage: -run=WeaponNameMigration -Mode=Snapshot|Apply|Verify -Tsv=<path>"));
		return 1;
	}
	if (Mode == TEXT("Snapshot"))
	{
		// No refresh: before the migration the loaded graphs are exactly what is on disk.
		const TArray<FRow> Rows = CollectAll(WeaponEnum, false);
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration snapshot: %d rows"), Rows.Num());
		return WriteTsv(Rows, TsvPath) ? 0 : 1;
	}
	if (Mode == TEXT("Apply"))
	{
		return RunApply(TsvPath);
	}
	if (Mode == TEXT("Verify"))
	{
		return RunVerify(WeaponEnum, TsvPath);
	}
	UE_LOG(LogTemp, Error, TEXT("WeaponNameMigration: unknown mode %s"), *Mode);
	return 1;
#else
	return 1;
#endif
}
```

> This is a reference implementation written against the UE 5.5 API. Compile it and dry-run it in Phase 1 before you rely on it. The pieces most worth double-checking are `TPropertyValueIterator::GetPropertyPathDebugString`, `UEdGraphNode::RemovePin` on an orphaned pin, whether `FBlueprintEditorUtils::RefreshAllNodes` really leaves every bucket-2 pin as a `PC_Name` pin (Apply logs `FAILED` if not), and whether `UEnum::GetValueByNameString` follows the ini `EnumRedirects` (the `Puma_50MM` row tells you; see §4.4).

### 4.3 Run the snapshot

Build `RTS_SurvivalEditor` (Development Editor) first, with the editor closed. Then run (Git Bash):

```bash
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=WeaponNameMigration -Mode=Snapshot -Tsv="D:/UE5Projects/RTS_Survival/Source/Refactor/MigrationGuides/EWeaponName_Snapshot.tsv" -unattended -nullrhi -nosplash -stdout
```

The run loads every Blueprint, which takes a while. It does **not** save anything.

### 4.4 Check the snapshot before you trust it

Do all of the following. Each check catches a different way the record can be incomplete.

1. **Asset coverage.** Every asset in the grep inventory should show up in the snapshot.

   ```bash
   cd /d/UE5Projects/RTS_Survival
   grep -rla "EWeaponName" Content --include=*.uasset | sed 's|^Content/|/Game/|; s|\.uasset$||' | sort -u > /tmp/wn_inventory.txt
   tail -n +2 Source/Refactor/MigrationGuides/EWeaponName_Snapshot.tsv | cut -f1 | sort -u > /tmp/wn_snapshot.txt
   comm -23 /tmp/wn_inventory.txt /tmp/wn_snapshot.txt
   ```

   Every asset this prints is one the commandlet did not record. Find out why: an asset type it doesn't scan, a Blueprint that failed to load, or a reference that only appears in an `EWeaponName` *name-table* entry with no value (for example a `Break WeaponData` node whose `WeaponName` output isn't used). Extend the commandlet or write that asset's values down by hand. **Do not continue while this list contains an unexplained asset.**

TSV columns: `1 AssetPackage, 2 Kind, 3 Container, 4 NodeClass, 5 NodeTitle, 6 NodeGuid, 7 PinOrPropertyPath, 8 Direction, 9 RawValue, 10 CanonicalValue, 11 NumericValue, 12 LinkSource`.

2. **Every weapon Blueprint has at least one restorable literal.** For each `Ger_Turret_*`, `Rus_Turret_*`, `*_Weapon_*`, `*EmbTurret*`, `*BXP*`, `*Vehicle*`, `*Aircraft*` and `*TW*` asset, there should be ≥1 row with `Kind=Pin`, `Direction=In`, an empty `LinkSource` and a non-empty `CanonicalValue`. The row count should match the number of `Setup*Weapon` calls in that Blueprint (a Panzer IV turret has main gun + coax, so 2+).

   ```bash
   awk -F'\t' 'NR>1 && $2=="Pin" && $8=="In" && $12=="" && $10!="" {print $1}' Source/Refactor/MigrationGuides/EWeaponName_Snapshot.tsv | sort | uniq -c | sort -n | head -40
   ```

3. **No unresolved values.** `CanonicalValue` must never be empty on an unwired input pin, or on a `Variable`, `LocalVariable` or `Property` row. An empty value means the stored string doesn't match any enumerator, even after redirects. Fix that Blueprint *now*, while the enum is still `uint8` and the editor shows the dropdown. (`Direction=Out` rows normally have no value. They mark graph nodes that *read* a weapon name; see §2.1 and §6.2.)

   ```bash
   awk -F'\t' 'NR>1 && $10=="" && (($2=="Pin" && $8=="In" && $12=="") || $2=="Variable" || $2=="LocalVariable" || $2=="Property")' Source/Refactor/MigrationGuides/EWeaponName_Snapshot.tsv
   ```

4. **Stale redirected names.** Rows where `RawValue` is neither `CanonicalValue` nor `EWeaponName::<CanonicalValue>` are still stored under an old name (`Puma_50MM`, `Puma_Mg`, `Ba_12MM`). Apply writes the canonical name, so they get cleaned up. Just know that they exist.

5. **Wired pins.** Rows with a `LinkSource` get their value from another node (a variable, a function return, a Select). Apply does not touch them. Each one goes on the manual list in §6.2 together with its source.

6. **Spot-check by hand.** Open 5 Blueprints from different groups in the editor (one turret, one infantry weapon, one vehicle, one pickup, `BP_W_WeaponItem`). Compare what you see with their snapshot rows. **Close them without saving.**

### 4.5 Commit and tag the record

```bash
git add Source/RTS_Survival/Utils/Migration Source/Refactor/MigrationGuides/EWeaponName_Snapshot.tsv
git commit -m "Add EWeaponName migration commandlet and pre-migration weapon snapshot"
git tag weaponname-uint32-pre
```

This commit is your rollback point. Unlike a `.uasset` diff, the TSV can be reviewed. If anyone asks later "which gun did the KV-2 turret use before the migration", the answer is in this file.

---

## 5. Phases 2 & 3: the C++ change

### 5.1 Decision: what Blueprints use instead of the enum

**Use `FName` with the same property and parameter names, resolved to `EWeaponName` in C++.**

* An enum pin literal is already stored as the short enumerator name (`Kar_98k`), which is also a valid `FName` value. Keeping the property named `WeaponName` gives K2 a chance to carry the literal over when it reconstructs the node (pins are matched by name), so restore may only need checking.
* Wires between bucket-3 pins stay type-compatible (Name → Name) because those functions switch to `FName` too.
* `meta=(GetOptions=…)` gives a dropdown in the Details panel and in variable defaults. It probably does **not** show on Make-struct graph pins; check this in the spike. Without the dropdown, a typo is caught at runtime by the resolver's `ReportError` and by `-Mode=Verify`.
* The alternative, `FGameplayTag`, gives a real picker on pins but is a much bigger refactor. Splitting the enum instead (keep the `uint8` enum and add a second ID type for vehicle variants) is still the option with zero Blueprint churn, if you ever want to back out.

### 5.2 Code changes (all in one commit)

**a) The enum** (`WeaponSystems.h:33-35`):

```cpp
// also fill in Global_GetWeaponDisplayName and Global_GetWeaponEnumAsString (see below)
// uint32 so we can grow past 256 weapons; Blueprints therefore use FName weapon names, see Global_GetWeaponNameFromFName.
UENUM()
enum class
	EWeaponName : uint32
```

Leave every entry exactly where it is. **Append only. Never reorder or remove an entry.** At the same time, fix the two broken metadata keys so their display names start working:
`WeaponSystems.h:139` `Flak37_88MM UMETA(DisplaynName = …)` → `DisplayName`
`WeaponSystems.h:233` `D_25T_122MM_IS3 UMETA(Displayname = …)` → `DisplayName`

**b) The ten forward declarations** in §2.4: `: uint8;` → `: uint32;`.

**c) Conversion helpers.** Put them in a new `Weapons/WeaponData/WeaponNameConversion.h/.cpp` so the widely included `WeaponSystems.h` doesn't need to include `RTSFunctionLibrary`.

```cpp
/**
 * @brief Resolves a Blueprint-authored weapon name to the enum; Blueprints cannot hold EWeaponName since it is uint32.
 * Accepts "Kar_98k" and "EWeaponName::Kar_98k" and follows the EWeaponName EnumRedirects in DefaultEngine.ini.
 * @return DEFAULT_WEAPON (and reports an error) when the name matches no entry.
 */
EWeaponName Global_GetWeaponNameFromFName(const FName WeaponName);

// Reflection based so it always matches the enumerator name that Blueprints store.
FName Global_GetWeaponNameAsFName(const EWeaponName WeaponName);
```

```cpp
EWeaponName Global_GetWeaponNameFromFName(const FName WeaponName)
{
	const UEnum* WeaponEnum = StaticEnum<EWeaponName>();
	const int64 Value = WeaponEnum ? WeaponEnum->GetValueByNameString(WeaponName.ToString()) : INDEX_NONE;
	if (Value == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError("Unknown weapon name: " + WeaponName.ToString()
			+ "\n Falling back to DEFAULT_WEAPON. Check the WeaponName pin of the Setup weapon call.");
		return EWeaponName::DEFAULT_WEAPON;
	}
	return static_cast<EWeaponName>(Value);
}

FName Global_GetWeaponNameAsFName(const EWeaponName WeaponName)
{
	const UEnum* WeaponEnum = StaticEnum<EWeaponName>();
	if (not WeaponEnum)
	{
		return NAME_None;
	}
	return FName(*WeaponEnum->GetNameStringByValue(static_cast<int64>(WeaponName)));
}
```

Use reflection, **not** the hand-written `Global_GetWeaponEnumAsString` switch, to build the `FName`. The switch's strings are written by hand and nothing guarantees they match the enumerator names.

Also add a `UFUNCTION() static TArray<FName> GetWeaponNameOptions()` to `URTSBlueprintFunctionLibrary`. `GetOptions` only needs the function to be reflected. It should return every enumerator short name except `DEFAULT_WEAPON` (and the hidden `MAX` if there is one), for use with `GetOptions`.

**d) Bucket 2: the init structs.** Each one follows this pattern:

```cpp
// FName because EWeaponName is uint32 and so not Blueprint-exposable; resolved with GetWeaponNameEnum().
UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(GetOptions="RTSBlueprintFunctionLibrary.GetWeaponNameOptions"))
FName WeaponName = FName(TEXT("T26_Mg"));

EWeaponName GetWeaponNameEnum() const { return Global_GetWeaponNameFromFName(WeaponName); }
```

**Keep the C++ default the same (`T26_Mg`).** K2 only carries over a pin value that differs from the pin's autogenerated default. A Blueprint that left the pin at `T26_Mg` relies on the default, so the new default must be the same string or that Blueprint silently changes weapon. Apply restores these rows explicitly anyway, but don't rely on that alone.

Then fix the read sites in §2.5: `Params.WeaponName` → `Params.GetWeaponNameEnum()`. The two test files that *construct* the struct use `Params.WeaponName = Global_GetWeaponNameAsFName(EWeaponName::X);`.

**e) Bucket 1:** remove `BlueprintReadOnly` / `BlueprintReadWrite` from the three fields in §2.1. Leave everything else as it is.

**f) Bucket 3:**

```cpp
UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Get weapon enum string")
static FString BP_GetWeaponEnumAsString(const FName WeaponName);       // body: Global_GetWeaponEnumAsString(Global_GetWeaponNameFromFName(WeaponName))

UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "TrainingOptions")
static FString BP_GetWeaponDisplayNameString(const FName WeaponName);  // body: Global_GetWeaponDisplayName(Global_GetWeaponNameFromFName(WeaponName))

UFUNCTION(BlueprintImplementableEvent)
void OnSetupWidgetForWeapon(const FName WeaponName);                   // C++ caller passes Global_GetWeaponNameAsFName(...)

UFUNCTION(BlueprintCallable, NotBlueprintable)
void ManuallySetSecondaryWeapon(UStaticMeshComponent* OwnerSecondaryWeaponMesh, const FName NewWeaponName, ...);
// UHT rejects overloaded UFUNCTIONs: if C++ also calls this, give the enum version its own plain (non-UFUNCTION)
// name, e.g. SetSecondaryWeapon(..., const EWeaponName ...), and let the BP version resolve the FName and forward to it.
```

`UW_WeaponDescription::WeaponName`: change to `UPROPERTY()` and add `UFUNCTION(BlueprintPure) FName GetWeaponNameAsFName() const`.

Keep every parameter name unchanged. K2 matches pins by name.

**g) Leave alone:** `TMap<EWeaponName, FWeaponData>`, the `GetNameByValue(static_cast<int64>(...))` fallback in `Global_GetWeaponEnumAsString` (it's int64 and safe at any width), the `EnumRedirects` in `DefaultEngine.ini`, and every switch over the enum.

### 5.3 Phase 2: the spike (a throwaway branch, before the real change)

As §1.3 explains, a one-struct spike can't compile. So the spike *is* the full change from §5.2, made on a throwaway branch:

1. `git switch -c spike/weaponname-uint32 weaponname-uint32-pre`. Make all the changes in §5.2 and build with the editor closed.
2. Open these in the editor and **do not save** any of them:
   * `Blueprints/Turrets/German/Medium/PanzerIVTurret/Ger_Turret_PanzerIV_F1` (Make-struct pin, more than one weapon)
   * `Blueprints/Squads/InfantryWeapons/German/Kar98/Ger_Weapon_Kar98` (infantry weapon)
   * one `Rus_WeaponPickup_*` (bucket 1: is the CDO value still there? is the Details dropdown still there?)
   * `Blueprints/GameUI/WeaponUI/BP_W_WeaponItem` (bucket 3: event node, function calls)
3. Look at the `WeaponName` pin on each Make node. One of three things will have happened:

| Outcome | What you see | What it means for Phase 4 |
|---|---|---|
| **A: Carried over** | The pin is a Name pin showing `KwK37_75MM` / `Kar_98k` | Apply only verifies and normalises. Run Apply and Verify anyway. |
| **B: Orphaned** | A red orphaned pin still shows the old value, plus a new empty Name pin | **Do not use "Refresh All Nodes" or "Remove orphaned pin" by hand.** Apply copies the value from the snapshot and removes the orphan. |
| **C: Lost** | A Name pin showing the C++ default (`T26_Mg`) | Apply is the only way the value comes back. Re-check the coverage results from §4.4 before you continue. |

4. Also note: whether the GetOptions dropdown appears on the graph pin, and whether the pickup's Details-panel value survived (bucket 1). If the pickup value did **not** survive, add `Kind=Property` restore support to Apply before Phase 4. The snapshot already has the rows.
5. Throw the spike away: `git restore Content` (in case the editor saved anything), then switch back. Cherry-pick the C++ commit onto the real branch if it was clean.

### 5.4 Phase 3: the real change

On a feature branch cut from `weaponname-uint32-pre`, apply §5.2 as **one commit**. Build `RTS_SurvivalEditor` **with the editor closed** (UHT changes can't be live-coded). Don't start the editor yet.

---

## 6. Phase 4: restore the Blueprints

### 6.1 Automatic restore

The editor stays closed. Run:

```bash
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=WeaponNameMigration -Mode=Apply -Tsv="D:/UE5Projects/RTS_Survival/Source/Refactor/MigrationGuides/EWeaponName_Snapshot.tsv" -unattended -nullrhi -nosplash -stdout > /tmp/wn_apply.log 2>&1
grep -E "FAILED|restored" /tmp/wn_apply.log | sort | uniq -c | sort -rn | head
```

Apply writes each literal back by `NodeGuid` + pin name, compiles each changed Blueprint and **saves after each package**, so a crash late in the run keeps everything before it. Every `FAILED` line is a node that no longer exists or a pin whose name changed. Handle those by hand using §6.2.

Packages run parents first (by the asset registry `ParentClass` tag). For each Blueprint, Apply:

1. Retypes `EWeaponName` member and local variables to `Name`, keeping the container type. It edits the variable description directly, because `ChangeMemberVariableType` asks for confirmation in a dialog. Local variables get their snapshot default.
2. Recompiles structurally and refreshes all nodes, so the getters and Make nodes rebuild as `Name` pins.
3. Restores pin literals, then reconnects any wired pin whose wire broke. It uses `LinkSourceNodeGuid` / `LinkSourcePin` from the snapshot and removes orphaned pins on both ends.
4. Compiles.
5. Writes every snapshot `Property` value back onto the new CDO, CDO subobjects, SCS/ICH templates and child-actor templates, matched by container label and property path. This restores the retyped variables' values, and child Blueprints that override them.

Non-Blueprint assets only get step 5. Data-table and struct-default rows are reported as `FAILED` (the snapshot has none).

Add `-DryRun` to do everything except save. Use it in the Phase 2 spike to read the `FAILED` list first. A dry run on the unmigrated project (2026-10-25) retyped 66 + 9 variables, restored all 66 CDO values and failed only the 261 pin literals, which need the `uint32` change first. No wires had to be reconnected: reconstruction kept them even across the type mismatch.

Commit right away: `git add Content && git commit -m "Restore EWeaponName pin literals after uint32 migration"`.

### 6.2 Things Apply cannot do

Apply only restores *literals on unwired input pins*. Everything below is manual. The snapshot gives you the exact asset, graph, node title and value for each one:

| Snapshot rows | Manual fix |
|---|---|
| `Kind=SwitchCase` (Switch on EWeaponName) | Replace with **Switch on Name**. Add one case per row, with the case name equal to the row's `PinOrPropertyPath`, then rewire the exec outputs. |
| `Kind=Variable` / `LocalVariable` typed `EWeaponName` | Change the variable type to **Name** and set its default to the row's `CanonicalValue`. |
| `Kind=Pin` with a non-empty `LinkSource` | Whatever feeds the pin must now produce a Name: a Name variable, `GetWeaponNameAsFName`, or a literal. |
| `Kind=Pin`, `Direction=Out` on `K2Node_VariableGet` / `Break…` of a bucket-1 field | Replace with the `Get…WeaponNameAsFName` getter from §2.1. |
| `Kind=Pin` on `K2Node_FunctionEntry` / `K2Node_FunctionResult` / `K2Node_CustomEvent` / macro tunnels / event dispatcher signatures | These are parameters of a **Blueprint-defined** function, event or dispatcher typed `EWeaponName`. Change the user-defined pin type to **Name** and reconnect its callers. |
| `Container=StructDefaults` (User Defined Struct asset) | Change the member type to **Name** and re-enter the default from `CanonicalValue`. Then fix every Make/Break of that struct (the snapshot lists them as pin rows). |
| `K2Node_EnumLiteral`, `K2Node_Select` / `EqualEqual` on the enum | Replace with a Name literal / Select on Name / `==` (Name). |
| Bucket-3 call sites: `BP_W_WeaponItem`, `Ger_Inf_Faust`, `Ger_Inf_Mauser`, the `W_WeaponDescription` reader | Refresh the node and reconnect. `OnSetupWidgetForWeapon` now outputs a Name, and `BP_GetWeapon*String` takes a Name, so a direct wire between them reconnects. |
| `Kind=Property` rows (only if the spike showed outcome C for bucket 1) | Re-enter them in the Details panel, or extend Apply. |

### 6.3 Rules while fixing Blueprints by hand

* Fix one Blueprint at a time. **Compile and save it before you open the next one.**
* Never run "Refresh All Nodes" or "Remove orphaned pins" on a Blueprint whose snapshot rows haven't been restored and verified yet.
* If you break a Blueprint beyond repair, `git restore` that one `.uasset` to the Phase-4 commit and re-apply.

---

## 7. Phase 5: verify

### 7.1 Diff against the snapshot

```bash
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=WeaponNameMigration -Mode=Verify -Tsv="D:/UE5Projects/RTS_Survival/Source/Refactor/MigrationGuides/EWeaponName_Snapshot.tsv" -unattended -nullrhi -nosplash -stdout > /tmp/wn_verify.log 2>&1
grep -E "VERIFY|problem rows" /tmp/wn_verify.log
```

* `VERIFY CHANGED` on a `Pin` row is never acceptable. It means a weapon changed.
* `VERIFY MISSING` is expected **only** for rows you replaced by hand in §6.2 (Switch nodes, enum variables, enum literals). Their node GUIDs or types changed. Go through each one against the manual list and confirm the replacement carries the same value.
* Once every remaining line is explained, the migration is verified.

### 7.2 Compile all Blueprints

```bash
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=CompileAllBlueprints -unattended -nullrhi -nosplash -stdout > /tmp/wn_compile.log 2>&1
grep -iE "error|warning.*WeaponName" /tmp/wn_compile.log | head -50
```

Compare the result with the run from §4.1. There must be no new errors.

### 7.3 Play-in-editor checks

PIE one map that spawns at least one of each: German turret tank, Soviet turret tank, infantry squad (with a secondary weapon, e.g. `Ger_Inf_Faust`), team weapon, gun pit or bunker, aircraft with a bomb bay, weapon pickup, train. For each one:

* open the weapon UI (`BP_W_WeaponItem`): the name and icon must be correct;
* fire it: the damage and range must come from the right `FWeaponData` (a wrong name falls back to `DEFAULT_WEAPON`, which you'll notice);
* afterwards, search the log for `Unknown weapon name`. It should not appear at all.

Also run the existing automation suites that construct init structs: the team weapon dig-in and gun pit test scenarios, and `JagdPanzerIV70Tests`.

### 7.4 Wrap up

* Merge the feature branch. Keep the `weaponname-uint32-pre` tag.
* Keep the commandlet until the vehicle-variant weapon batch has landed. `-Mode=Verify` against a fresh snapshot is a cheap lint for `FName` weapon typos. Delete it after that.

---

## 8. Rollback

* **Before the Phase-4 commit:** `git reset --hard weaponname-uint32-pre`. Content is untouched because nothing was saved.
* **After it:** revert the C++ commit and the restore commit together. The `.uasset` files go back to the enum-pin versions from the tag. Never revert only one of the two: the Blueprints would then hold `FName` pins that C++ no longer has.

---

## 9. Rules from now on

1. **Append only.** New weapons go at the end of `EWeaponName`. Never reorder or delete entries. Delete by marking `UMETA(Hidden)`.
2. **Renaming an entry** needs an `EnumRedirects` line in `DefaultEngine.ini`, the way `Puma_50MM` does it. `Global_GetWeaponNameFromFName` follows those redirects through `GetValueByNameString`, so Blueprint `FName` strings keep resolving. Still, run `-Mode=Snapshot` to a scratch file afterwards and grep for the old name.
3. **New Blueprint-facing weapon fields** are `FName` with `GetOptions="RTSBlueprintFunctionLibrary.GetWeaponNameOptions"` plus a C++ `Get…Enum()` accessor. They are never `EWeaponName`.
4. **New weapons still need** a case in `Global_GetWeaponDisplayName`, `Global_GetWeaponEnumAsString` and `Global_GetWeaponIconForWeapon`, as before.
