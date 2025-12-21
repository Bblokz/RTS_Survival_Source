// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/CardSystem/ERTSCard/ERTSCard.h"
#include "RTS_Survival/Game/GameState/GameDecalManager/GameDecalManager.h"
#include "RTS_Survival/Resources/ResourceSceneSetup/ResourceSceneSetup.h"
#include "RTS_Survival/UnitData/AircraftData.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTSBlueprintFunctionLibrary.generated.h"


class UPlayerPortraitManager;
struct FTrainingOption;
enum class ERTSProgressBarType : uint8;
struct FRTSVerticalAnimTextSettings;
class AActor;
class ABuildingExpansion;
class ATankMaster;
enum class ERTSArchiveItem : uint8;
class UMainGameUI;
class ACPPController;
enum class ENomadicLayoutBuildingType : uint8;
enum class ERTSCardRarity : uint8;
enum class ERTSCard : uint8;
struct FBxpData;
struct FSquadData;
struct FNomadicData;
struct FTankData;
enum class ETechnology : uint8;
enum class ERTSInfantryBalanceSetting : uint8;
enum class EGameBalanceHealthTypes : uint8;
enum class EWeaponName : uint8;
enum class EAbilityID : uint8;
enum class EBuildingExpansionType : uint8;
enum class ENomadicSubtype : uint8;

USTRUCT(BlueprintType)
struct FHeAmmoProperties
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	float ArmorPenNegPercentage = 100 - DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt * 100;
	UPROPERTY(BlueprintReadOnly)
	float TNTExplosiveGramsPercentage = DeveloperSettings::GameBalance::Weapons::Projectiles::HE_TNTExplosiveGramsMlt *
		100 - 100;
	UPROPERTY(BlueprintReadOnly)
	float DamagePercentage = DeveloperSettings::GameBalance::Weapons::Projectiles::HE_DamageMlt * 100 - 100;
	UPROPERTY(BlueprintReadOnly)
	float ShrapnelDamagePercentage = DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ShrapnelDamageMlt * 100 -
		100;
	UPROPERTY(BlueprintReadOnly)
	float ShrapnelRangePercentage = DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ShrapnelRangeMlt * 100 -
		100;
	UPROPERTY(BlueprintReadOnly)
	float ShrapnelPenPercentage = DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ShrapnelPenMlt * 100 - 100;
};

USTRUCT(BlueprintType)
struct FAPCRAmmoProperties
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	float ArmorPenPercentage = DeveloperSettings::GameBalance::Weapons::Projectiles::APCR_ArmorPenMlt * 100 - 100;
	UPROPERTY(BlueprintReadOnly)
	float DamageNegPercentage = 100 - DeveloperSettings::GameBalance::Weapons::Projectiles::APCR_DamageMlt * 100;
	UPROPERTY(BlueprintReadOnly)
	bool bHasNoHeFiller = true;
};

USTRUCT(BlueprintType)
struct FHeatAmmoProperties
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	float ArmorPenPercentage = DeveloperSettings::GameBalance::Weapons::Projectiles::HEAT_ArmorPenMlt * 100 - 100;
	UPROPERTY(BlueprintReadOnly)
	bool bArmorPenIsSameAtAllRanges = true;
};

USTRUCT(BlueprintType)
struct FAPHEBCAmmoProperties
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	float PercentageDecreaseMaxMinArmorPen =
		DeveloperSettings::GameBalance::Weapons::Projectiles::APHEBC_ArmorPenLerpFactor * 100;
};

UCLASS()
class RTS_SURVIVAL_API URTSBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------
	// --------- RTS Card System ------------ ------
	// ------------------------------------------------------------
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "RTSCard")
	static FString BP_GetRTSCardAsString(const ERTSCard Card);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "RTSCard")
	static FString BP_GetNomadicBuildingLayoutTypeString(const ENomadicLayoutBuildingType Type);

	// ------------------------------------------------------------
	//  -------------- Sub types string translations --------------
	// ------------------------------------------------------------
	
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Subtypes")
	static FString BP_GetTrainingOptionDisplayName(const FTrainingOption TrainingOption);
	
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Subtypes")
	static FString BP_GetNomadicSubtypeString(const ENomadicSubtype NomadicSubtype);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Get weapon enum string")
	static FString BP_GetWeaponEnumAsString(const EWeaponName WeaponName);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "BuildingExpansionType")
	static FString BP_GetBxpTypeString(const EBuildingExpansionType BuildingExpansionType);
	
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "BuildingExpansionType")
	static FString BP_GetBxpDisplayNameString(const EBuildingExpansionType BuildingExpansionType);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "TankSubtype")
	static FString BP_GetTankSubtypeString(ETankSubtype TankSubtype);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "AircraftSubtype")
	static FString BP_GetAircraftSubtypeString(EAircraftSubtype AircraftSubtype);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "SquadSubtype")
	static FString BP_GetSquadSubtypeString(ESquadSubtype SquadSubtype);

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, Category = "TankSubtype")
	static uint8 BP_GetTankSubtypeRawValue(const ETankSubtype TankSubtype);

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, Category = "TankSubtype")
	static uint8 BP_GetNomadicSubtypeRawValue(const ENomadicSubtype NomadicSubtype);

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, Category = "SquadSubtype")
	static uint8 BP_GetSquadSubtypeRawValue(const ESquadSubtype SquadSubtype);

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, Category = "BuildingExpansionType")
	static uint8 BP_GetBxpTypeRawValue(const EBuildingExpansionType BuildingExpansionType);

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, Category = "ArchiveItem")
	static FName BP_GetArchiveItemName(const ERTSArchiveItem ArchiveItem);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	static UPlayerPortraitManager* BP_GetPlayerPortraitManager(const UObject* WorldContextObject);


	/** 
	 * Ensures that every `<Text_Armor>-</>` tag (after the first) is preceded by an empty line.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category="Utilities|Text")
	static FText BP_FixArmorTagSpacing(const FText& SourceText);

	// ------------------------------------------------------------
	// ------------ Tech Tree and Abilities string translations ------------
	// ------------------------------------------------------------

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "TechTree")
	static FString BP_GetTechString(const ETechnology Tech);
	
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "TechTree")
	static FString BP_GetTechDisplayNameString(const ETechnology Tech);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "TrainingOptions")
	static FString BP_GetAbilityIDString(const EAbilityID AbilityID);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "TrainingOptions")
	static FString BP_GetWeaponDisplayNameString(const EWeaponName WeaponName);

	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category="GameBalance")
	static float BP_GetHealthGameBalanceValue(const EGameBalanceHealthTypes HealthType);

	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category="GameBalance")
	static float BP_GetInfantryGameBalanceValue(const ERTSInfantryBalanceSetting InfantryBalanceSetting);

	// ------------------------------------------------------------
	// --------------- Ammo Properties ----------------------------
	// ------------------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category="GameBalance")
	static FHeAmmoProperties BP_GetHeAmmoProperties() { return FHeAmmoProperties(); };

	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category="GameBalance")
	static FAPCRAmmoProperties BP_GetAPCRAmmoProperties() { return FAPCRAmmoProperties(); };

	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category="GameBalance")
	static FHeatAmmoProperties BP_GetHeatAmmoProperties() { return FHeatAmmoProperties(); };

	UFUNCTION(blueprintCallable, BlueprintPure, NotBlueprintable, Category="GameBalance")
	static FAPHEBCAmmoProperties BP_GetAPHEBCAmmoProperties() { return FAPHEBCAmmoProperties(); };

	//
	// ---------------------- Unit Data ----------------------
	//
	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category="UnitData")
	static FTankData BP_GetTankDataOfPlayer(const int32 PlayerOwningTank, const ETankSubtype TankSubtype,
	                                        UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category="UnitData")
	static FNomadicData BP_GetNomadicDataOfPlayer(const int32 PlayerOwningNomadic, const ENomadicSubtype NomadicSubtype,
	                                              UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category="UnitData")
	static FSquadData BP_GetSquadDataOfPlayer(const int32 PlayerOwningSquad, const ESquadSubtype SquadSubtype,
	                                          UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category="UnitData")
	static FBxpData BP_GetPlayerBxpData(const EBuildingExpansionType BxpType, UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category="UnitData")
	static FAircraftData BP_GetAircraftDataOfPlayer(const int32 PlayerOwningAircraft,
	                                                const EAircraftSubtype AircraftSubtype,
	                                                UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="UnitData")
	static void BP_DebugNomadicData(const FNomadicData NomadicData);


	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, Category="UnitData")
	static float GetDestroyedTankHealth(UObject* WorldContextObject, ETankSubtype TankSubtype);

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, Category="UnitData")
	static float GetDestroyedTankVehiclePartsRewardAndScavTime(UObject* WorldContextObject, ETankSubtype TankSubtype, float& TimeToScavenge);


	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="RTSDecal")
	static void RTSSpawnDecal(const UObject* WorldContextObject,
	                          const ERTS_DecalType DecalType,
	                          const FVector& SpawnLocation,
	                          const FVector2D& MinMaxSize, const float LifeTime, const FVector2D& MinMaxXYOffset);


	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="WorldSubsystem|ExplosionManager")
	static void RTSSpawnExplosionAtLocation(
		const UObject* WorldContextObject,
		const ERTS_ExplosionType ExplosionType,
		const FVector& SpawnLocation,
		const bool bPlaySound,
		const float Delay);
	
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="WorldSubsystem|ExplosionManager")
	static void RTSSpawnExplosionAtRandomSocket(
		const UObject* WorldContextObject,
		const ERTS_ExplosionType ExplosionType,
		UMeshComponent* MeshComp,
		const bool bPlaySound,
		const float Delay);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="WorldSubsystem|VerticalAnimatedTextManager")
	static void RTSSpawnVerticalAnimatedTextAtLocation(
		const UObject* WorldContextObject,
		const FString& InText,
		const FVector& InWorldStartLocation,
		const bool bInAutoWrap,
		const float InWrapAt,
		const TEnumAsByte<ETextJustify::Type> InJustification,
		const FRTSVerticalAnimTextSettings& InSettings
	);
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="WorldSubsystem|VerticalAnimatedTextManager")
	static void RTSSpawnVerticalAnimatedTextAttachedToActor(
		const UObject* WorldContextObject,
		const FString& InText,
		AActor* InAttachActor,
		const FVector& InAttachOffset,
		const bool bInAutoWrap,
		const float InWrapAt,
		const TEnumAsByte<ETextJustify::Type> InJustification,
		const FRTSVerticalAnimTextSettings& InSettings
	);
	/** @return The the ID that identifies this progress bar instance */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="WorldSubsystem|TimedProgressBarManager")
	static int RTSActivatedTimedProgressBar(
		const UObject* WorldContextObject,
		float RatioStart,
		float TimeTillFull,
		bool bUsePercentageText,
		ERTSProgressBarType BarType,
		const bool bUseDescriptionText,
		const FString& InText,
		const FVector& Location,
		const float ScaleMlt);


	/** @return The the ID that identifies this progress bar instance */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="WorldSubsystem|TimedProgressBarManager")
	static int ActivateTimedProgressBarAnchored(
		const UObject* WorldContextObject,
		USceneComponent* AnchorComponent,
		const FVector AttachOffset,
		float RatioStart,
		float TimeTillFull,
		bool bUsePercentageText,
		ERTSProgressBarType BarType,
		const bool bUseDescriptionText,
		const FString& InText,
		const float ScaleMlt);

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, Category="UnitData")
	static bool RTSIsValid(AActor* ActorToCheck);

	// ------------------------------- Game Architecture Utils -------------------------------
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="RTS_Statics")
	static ACPPController* GetPlayerController(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="RTS_Statics")
	static UMainGameUI* GetMainGameUI(const UObject* WorldContextObject);

	// ------------------------------- ------------------------ -------------------------------


	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="UnitData")
	static FVector RTSGetLocationAtRandomOffsetProjected(
		const UObject* WorldContext,
		const FVector& OriginalLocation,
		const FVector2D MinMaxXYOffset, bool& bWasSuccessful);


	/**
	 * Appends two arrays of UMaterialInterface pointers and returns the merged array.
	 */
	UFUNCTION(BlueprintCallable, Category="ArrayHelpers", meta=(DisplayName="Append Material Array"))
	static TArray<UMaterialInterface*> BP_AppendMaterialArray(
		const TArray<UMaterialInterface*>& ArrayA,
		const TArray<UMaterialInterface*>& ArrayB
	);

	UFUNCTION(BlueprintCallable, Category="ArrayHelpers", meta=(DisplayName="Append Weighted Material Array"))
	static TArray<FWeightedDecalMaterial> BP_AppendDecalMaterialArray(
		const TArray<FWeightedDecalMaterial>& ArrayA,
		const TArray<FWeightedDecalMaterial>& ArrayB
	);

	// -------------------------------------------------------------------------
	//  ------------------------ Actor Utilities -------------------------
	// -------------------------------------------------------------------------


	UFUNCTION(BlueprintCallable, Category="Utilities|Array")
	static void PickRandomActor(TArray<AActor*>& ActorArray, AActor*& PickedActor, int32& PickedIndex);

	/** 
	   * Destroy all ATankMaster‐derived actors within Range of Origin.
	   */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Array")
	static void DestroyAllTankMastersInRange(const FVector& Origin, float Range, const UObject* WorldContext);

	/** 
	 * Destroy all ABuildingExpansion‐derived actors within Range of Origin.
	 */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Array")
	static void DestroyAllBxpsInRange(const FVector& Origin, float Range, const UObject* WorldContext);
};
