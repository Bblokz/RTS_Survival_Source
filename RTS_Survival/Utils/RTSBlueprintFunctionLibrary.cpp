// Copyright (C) Bas Blokzijl - All rights reserved.


#include "RTSBlueprintFunctionLibrary.h"

#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Statics/RTS_Statics.h"
#include "RTS_Statics/SubSystems/DecalManagerSubsystem/DecalManagerSubsystem.h"
#include "RTS_Statics/SubSystems/ExplosionManagerSubsystem/ExplosionManagerSubsystem.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/CardSystem/CardUI/NomadLayoutWidget/NomadicLayoutBuilding/NomadicLayoutBuildingType.h"
#include "RTS_Survival/CardSystem/ERTSCard/ERTSCard.h"
#include "RTS_Survival/Game/GameBalance/RTSGameBalanceHealthTypes.h"
#include "RTS_Survival/Game/GameBalance/RTSGameBalanceInfantrySettings.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionManager.h"
#include "RTS_Survival/GameUI/ActionUI/ItemActionUI/W_ItemActionUI.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextWidgetPoolManager/AnimatedTextWidgetPoolManager.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/WorldSubSystem/AnimatedTextWorldSubsystem.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/Pooling/WorldSubSystem/RTSTimedProgressBarWorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptionLibrary/TrainingOptionLibrary.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/RTSRadiusPoolSubsystem/RTSRadiusPoolSubsystem.h"
#include "RTS_Survival/UnitData/BuildingExpansionData.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"


void URTSBlueprintFunctionLibrary::ApplyRTSDamage(AActor* ActorToDamage, const ERTSDamageType DamageType, const float DamageAmount) 
{
	if(not IsValid(ActorToDamage))
	{
		return;
	}
	TSubclassOf<UDamageType> DamageTypeClass = FRTSWeaponHelpers::GetDamageTypeClass(DamageType);
	FDamageEvent DamageEvent(DamageTypeClass);
	ActorToDamage->TakeDamage(DamageAmount, DamageEvent, nullptr, nullptr);
}

FString URTSBlueprintFunctionLibrary::BP_GetRTSCardAsString(const ERTSCard Card)
{
	return Global_GetCardAsString(Card);
}

FString URTSBlueprintFunctionLibrary::BP_GetNomadicBuildingLayoutTypeString(const ENomadicLayoutBuildingType Type)
{
	return Global_GetNomadicLayoutBuildingTypeString(Type);
}

FString URTSBlueprintFunctionLibrary::BP_GetTrainingOptionDisplayName(const FTrainingOption TrainingOption)
{
	return TrainingOption.GetDisplayName();
}


FString URTSBlueprintFunctionLibrary::BP_GetNomadicSubtypeString(const ENomadicSubtype NomadicSubtype)
{
	return UTrainingOptionLibrary::GetEnumValueName(EAllUnitType::UNType_Nomadic, static_cast<uint8>(NomadicSubtype));
}

FString URTSBlueprintFunctionLibrary::BP_GetWeaponEnumAsString(const EWeaponName WeaponName)
{
	return Global_GetWeaponEnumAsString(WeaponName);
}

FString URTSBlueprintFunctionLibrary::BP_GetBxpTypeString(const EBuildingExpansionType BuildingExpansionType)
{
	return Global_GetBxpTypeEnumAsString(BuildingExpansionType);
}

FString URTSBlueprintFunctionLibrary::BP_GetBxpDisplayNameString(const EBuildingExpansionType BuildingExpansionType)
{
	return Global_GetBxpDisplayString(BuildingExpansionType);
}

FString URTSBlueprintFunctionLibrary::BP_GetTankSubtypeString(const ETankSubtype TankSubtype)
{
	return UTrainingOptionLibrary::GetEnumValueName(EAllUnitType::UNType_Tank, static_cast<uint8>(TankSubtype));
}

FString URTSBlueprintFunctionLibrary::BP_GetAircraftSubtypeString(EAircraftSubtype AircraftSubtype)
{
	return UTrainingOptionLibrary::GetEnumValueName(EAllUnitType::UNType_Aircraft, static_cast<uint8>(AircraftSubtype));
}

FString URTSBlueprintFunctionLibrary::BP_GetSquadSubtypeString(ESquadSubtype SquadSubtype)
{
	return UTrainingOptionLibrary::GetEnumValueName(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadSubtype));
}

uint8 URTSBlueprintFunctionLibrary::BP_GetTankSubtypeRawValue(const ETankSubtype TankSubtype)
{
	return static_cast<uint8>(TankSubtype);
}

uint8 URTSBlueprintFunctionLibrary::BP_GetNomadicSubtypeRawValue(const ENomadicSubtype NomadicSubtype)
{
	return static_cast<uint8>(NomadicSubtype);
}

uint8 URTSBlueprintFunctionLibrary::BP_GetSquadSubtypeRawValue(const ESquadSubtype SquadSubtype)
{
	return static_cast<uint8>(SquadSubtype);
}


uint8 URTSBlueprintFunctionLibrary::BP_GetBxpTypeRawValue(const EBuildingExpansionType BuildingExpansionType)
{
	return static_cast<uint8>(BuildingExpansionType);
}

FName URTSBlueprintFunctionLibrary::BP_GetArchiveItemName(const ERTSArchiveItem ArchiveItem)
{
	return Global_GetArchiveItemName(ArchiveItem);
}

UPlayerPortraitManager* URTSBlueprintFunctionLibrary::BP_GetPlayerPortraitManager(const UObject* WorldContextObject)
{
	return FRTS_Statics::GetPlayerPortraitManager(WorldContextObject);
}

FText URTSBlueprintFunctionLibrary::BP_FixArmorTagSpacing(const FText& SourceText)
{
	// Convert to mutable string
	FString Working = SourceText.ToString();

	const FString Tag = TEXT("<Text_Armor>-</>");
	const int32 TagLen = Tag.Len();
	const FString Prefix = TEXT("\n\n");
	const int32 PrefixLen = Prefix.Len();

	bool bSkippedFirstAtStart = false;
	int32 SearchPos = 0;

	while (true)
	{
		int32 FoundPos = Working.Find(Tag, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchPos);
		if (FoundPos == INDEX_NONE)
			break;

		// Skip the very first occurrence if it's at the very start
		if (!bSkippedFirstAtStart && FoundPos == 0)
		{
			bSkippedFirstAtStart = true;
			SearchPos = FoundPos + TagLen;
			continue;
		}

		bSkippedFirstAtStart = true;

		// Check if there's already a blank line immediately before
		bool bHasBlankLineBefore = (FoundPos >= PrefixLen &&
			Working.Mid(FoundPos - PrefixLen, PrefixLen) == Prefix);

		if (!bHasBlankLineBefore)
		{
			// Insert two newlines as a blank line
			Working.InsertAt(FoundPos, Prefix);
			// Advance past what we just inserted plus the tag
			SearchPos = FoundPos + PrefixLen + TagLen;
		}
		else
		{
			// Already has blank line, just skip past this tag
			SearchPos = FoundPos + TagLen;
		}
	}

	return FText::FromString(Working);
}

FString URTSBlueprintFunctionLibrary::BP_GetTechString(const ETechnology Tech)
{
	return Global_GetTechAsString(Tech);
}

FString URTSBlueprintFunctionLibrary::BP_GetTechDisplayNameString(const ETechnology Tech)
{
	return Global_GetTechDisplayName(Tech);
}

FString URTSBlueprintFunctionLibrary::BP_GetAbilityIDString(const EAbilityID AbilityID)
{
	switch (AbilityID)
	{
	case EAbilityID::IdAttack: return "IdAttack";
	case EAbilityID::IdCrouch: return "IdCrouch";
	case EAbilityID::IdIdle: return "IdIdle";
	case EAbilityID::IdMove: return "IdMove";
	case EAbilityID::IdPatrol: return "IdPatrol";
	case EAbilityID::IdProne: return "IdProne";
	case EAbilityID::IdReverseMove: return "IdReverseMove";
	case EAbilityID::IdStand: return "IdStand";
	case EAbilityID::IdStop: return "IdStop";
	case EAbilityID::IdCreateBuilding: return "IdCreateBuilding";
	case EAbilityID::IdHarvestResource: return "IdHarvestResource";
	case EAbilityID::IdNoAbility: return "IdNoAbility";
	case EAbilityID::IdRotateTowards: return "IdRotateTowards";
	case EAbilityID::IdSwitchMelee: return "IdSwitchMelee";
	case EAbilityID::IdSwitchSingleBurst: return "IdSwitchSingleBurst";
	case EAbilityID::IdSwitchWeapon: return "IdSwitchWeapon";
	case EAbilityID::IdConvertToVehicle: return "IdConvertToVehicle";
	case EAbilityID::IdActivateMode: return "IdActivateMode";
	case EAbilityID::IdDisableMode: return "IdDisableMode";
	default:
		{
			break;
		}
	}
	const FName Name = StaticEnum<EAbilityID>()->GetNameByValue((int64)AbilityID);
	const FString CleanName = *Name.ToString().RightChop(FString("EAbilityID::").Len());
	return CleanName;
}

FString URTSBlueprintFunctionLibrary::BP_GetWeaponDisplayNameString(const EWeaponName WeaponName)
{
	return Global_GetWeaponDisplayName(WeaponName);
}

float URTSBlueprintFunctionLibrary::BP_GetHealthGameBalanceValue(const EGameBalanceHealthTypes HealthType)
{
	switch (HealthType)
	{
	case EGameBalanceHealthTypes::HT_None:
		RTSFunctionLibrary::ReportError("Requested health type is HT_None. This is not a valid health type.");
		return 0;
	case EGameBalanceHealthTypes::HT_LightTank:
		return DeveloperSettings::GameBalance::UnitHealth::LightTankHealthBase;
	case EGameBalanceHealthTypes::HT_LightMediumTank:
		return DeveloperSettings::GameBalance::UnitHealth::LightMediumTankBase;
	case EGameBalanceHealthTypes::HT_MediumTank:
		return DeveloperSettings::GameBalance::UnitHealth::MediumTankHealthBase;
	case EGameBalanceHealthTypes::HT_T2HeavyTank:
		return DeveloperSettings::GameBalance::UnitHealth::T2HeavyTankBase;
	case EGameBalanceHealthTypes::HT_T3MediumTank:
		return DeveloperSettings::GameBalance::UnitHealth::T3MediumTankBase;
	case EGameBalanceHealthTypes::HT_T3HeavyTank:
		return DeveloperSettings::GameBalance::UnitHealth::T3HeavyTankBase;
	case EGameBalanceHealthTypes::HT_LightTankShot:
		return DeveloperSettings::GameBalance::UnitHealth::OneLightTankShotHp;
	case EGameBalanceHealthTypes::HT_MediumTankShot:
		return DeveloperSettings::GameBalance::UnitHealth::OneMediumTankShotHp;
	case EGameBalanceHealthTypes::HT_HeavyTankShot:
		return DeveloperSettings::GameBalance::UnitHealth::OneHeavyTankShotHp;
	case EGameBalanceHealthTypes::HT_BasicInfantry:
		return DeveloperSettings::GameBalance::UnitHealth::BasicInfantryHealth;
	case EGameBalanceHealthTypes::HT_ArmoredInfantry:
		return DeveloperSettings::GameBalance::UnitHealth::ArmoredInfantryHealth;
	case EGameBalanceHealthTypes::HT_T2Infantry:
		return DeveloperSettings::GameBalance::UnitHealth::T2InfantryHealth;
	case EGameBalanceHealthTypes::HT_EliteInfantry:
		return DeveloperSettings::GameBalance::UnitHealth::EliteInfantryHealth;
	default:
		RTSFunctionLibrary::ReportError(
			"Requested health type is not defined. Provided type: " + FString::FromInt((int32)HealthType));
		return 0;
	}
}

float URTSBlueprintFunctionLibrary::BP_GetInfantryGameBalanceValue(
	const ERTSInfantryBalanceSetting InfantryBalanceSetting)
{
	switch (InfantryBalanceSetting)
	{
	case ERTSInfantryBalanceSetting::RTS_Inf_BasicSpeed:
		return DeveloperSettings::GameBalance::InfantrySettings::BasicInfantrySpeed;
	case ERTSInfantryBalanceSetting::RTS_Inf_BasicAcceleration:
		return DeveloperSettings::GameBalance::InfantrySettings::BasicInfantryAcceleration;
	case ERTSInfantryBalanceSetting::RTS_Inf_FastSpeed:
		return DeveloperSettings::GameBalance::InfantrySettings::FastInfantrySpeed;
	case ERTSInfantryBalanceSetting::RTS_Inf_FastAcceleration:
		return DeveloperSettings::GameBalance::InfantrySettings::FastInfantryAcceleration;
	case ERTSInfantryBalanceSetting::RTS_Inf_SlowSpeed:
		return DeveloperSettings::GameBalance::InfantrySettings::SlowInfantrySpeed;
	case ERTSInfantryBalanceSetting::RTS_Inf_SlowAcceleration:
		return DeveloperSettings::GameBalance::InfantrySettings::SlowInfantryAcceleration;
	}

	RTSFunctionLibrary::ReportError(
		"Requested infantry balance setting is not defined. Provided setting: " + FString::FromInt(
			(int32)InfantryBalanceSetting));
	return 300;
}

FTankData URTSBlueprintFunctionLibrary::BP_GetTankDataOfPlayer(
	const int32 PlayerOwningTank,
	const ETankSubtype TankSubtype,
	UObject* WorldContextObject)
{
	if (IsValid(WorldContextObject))
	{
		if (const ACPPGameState* GameState = FRTS_Statics::GetGameState(WorldContextObject))
		{
			return GameState->GetTankDataOfPlayer(PlayerOwningTank, TankSubtype);
		}
	}
	return FTankData();
}

FNomadicData URTSBlueprintFunctionLibrary::BP_GetNomadicDataOfPlayer(const int32 PlayerOwningNomadic,
                                                                     const ENomadicSubtype NomadicSubtype,
                                                                     UObject* WorldContextObject)
{
	if (IsValid(WorldContextObject))
	{
		if (const ACPPGameState* GameState = FRTS_Statics::GetGameState(WorldContextObject))
		{
			return GameState->GetNomadicDataOfPlayer(PlayerOwningNomadic, NomadicSubtype);
		}
	}
	return FNomadicData();
}

FSquadData URTSBlueprintFunctionLibrary::BP_GetSquadDataOfPlayer(const int32 PlayerOwningSquad,
                                                                 const ESquadSubtype SquadSubtype,
                                                                 UObject* WorldContextObject)
{
	if (IsValid(WorldContextObject))
	{
		if (const ACPPGameState* GameState = FRTS_Statics::GetGameState(WorldContextObject))
		{
			return GameState->GetSquadDataOfPlayer(PlayerOwningSquad, SquadSubtype);
		}
	}
	return FSquadData();
}

FBxpData URTSBlueprintFunctionLibrary::BP_GetPlayerBxpData(const EBuildingExpansionType BxpType,
                                                           UObject* WorldContextObject)
{
	if (IsValid(WorldContextObject))
	{
		if (const ACPPGameState* GameState = FRTS_Statics::GetGameState(WorldContextObject))
		{
			return GameState->GetPlayerBxpData(BxpType);
		}
	}
	return FBxpData();
}

FAircraftData URTSBlueprintFunctionLibrary::BP_GetAircraftDataOfPlayer(const int32 PlayerOwningAircraft,
                                                                       const EAircraftSubtype AircraftSubtype,
                                                                       UObject* WorldContextObject)
{
	if (not IsValid(WorldContextObject))
	{
		return FAircraftData();
	}
	if (const ACPPGameState* GameState = FRTS_Statics::GetGameState(WorldContextObject))
	{
		return GameState->GetAircraftDataOfPlayer(AircraftSubtype, PlayerOwningAircraft);
	}
	return FAircraftData();
}

void URTSBlueprintFunctionLibrary::BP_DebugNomadicData(const FNomadicData NomadicData)
{
	Global_DebugNomadicData(NomadicData);
}

float URTSBlueprintFunctionLibrary::GetDestroyedTankHealth(UObject* WorldContextObject, const ETankSubtype TankSubtype)
{
	if (not IsValid(WorldContextObject))
	{
		return 300.f;
	}
	if (const ACPPGameState* GameState = FRTS_Statics::GetGameState(WorldContextObject))
	{
		constexpr float Flux = DeveloperSettings::GameBalance::UnitHealth::DestroyedTankHealthFlux;
		const float RandomRatio = 1 + FMath::FRandRange(-Flux, Flux);
		return GameState->GetTankData(TankSubtype).MaxHealth *
			DeveloperSettings::GameBalance::UnitHealth::DestroyedTankHealthFraction * RandomRatio;
	}
	return 300.f;
}

float URTSBlueprintFunctionLibrary::GetDestroyedTankVehiclePartsRewardAndScavTime(UObject* WorldContextObject,
	const ETankSubtype TankSubtype, float& TimeToScavenge)
{
	using namespace DeveloperSettings::GameBalance::Resources;
	TimeToScavenge = ScavengeTimePer70Parts;
	float VehicleParts = 0.f;
	if (not IsValid(WorldContextObject))
	{
		return VehicleParts;
	}
	if (const ACPPGameState* GameState = FRTS_Statics::GetGameState(WorldContextObject))
	{
		const float Mlt = FMath::RandRange(VehiclePartsFromDestroyedVehicleMltRange.X,
		                                   VehiclePartsFromDestroyedVehicleMltRange.Y);
		auto CostsMap = GameState->GetTankData(TankSubtype).Cost.ResourceCosts;
		if (CostsMap.Contains(ERTSResourceType::Resource_VehicleParts))
		{
			VehicleParts = CostsMap[ERTSResourceType::Resource_VehicleParts] * Mlt;
		}
		TimeToScavenge = (VehicleParts / 70.f) * ScavengeTimePer70Parts;
	}
	return VehicleParts;
}

void URTSBlueprintFunctionLibrary::RTSSpawnDecal(const UObject* WorldContextObject, const ERTS_DecalType DecalType,
                                                 const FVector& SpawnLocation, const FVector2D& MinMaxSize,
                                                 const float LifeTime, const FVector2D& MinMaxXYOffset)
{
	if (not IsValid(WorldContextObject) || not WorldContextObject->GetWorld())
	{
		return;
	}
	UDecalManagerSubsystem* DecalSubsystem = WorldContextObject->GetWorld()->GetSubsystem<UDecalManagerSubsystem>();
	if (not IsValid(DecalSubsystem))
	{
		return;
	}
	if (UGameDecalManager* DecalManager = DecalSubsystem->GetDecalManager())
	{
		DecalManager->SpawnRTSDecalAtLocation(DecalType, SpawnLocation, MinMaxSize, MinMaxXYOffset, LifeTime);
		return;
	}
}

void URTSBlueprintFunctionLibrary::RTSSpawnExplosionAtLocation(const UObject* WorldContextObject,
                                                               const ERTS_ExplosionType ExplosionType,
                                                               const FVector& SpawnLocation, const bool bPlaySound,
                                                               const float Delay)
{
	if (not IsValid(WorldContextObject) || not WorldContextObject->GetWorld())
	{
		return;
	}
	UExplosionManagerSubsystem* ExplosionSubsystem = WorldContextObject->GetWorld()->GetSubsystem<
		UExplosionManagerSubsystem>();
	if (not IsValid(ExplosionSubsystem))
	{
		return;
	}
	if (UGameExplosionsManager* ExplosionManager = ExplosionSubsystem->GetExplosionManager())
	{
		ExplosionManager->SpawnRTSExplosionAtLocation(ExplosionType, SpawnLocation, bPlaySound, Delay);
	}
}

void URTSBlueprintFunctionLibrary::RTSSpawnExplosionAtRandomSocket(const UObject* WorldContextObject,
                                                                   const ERTS_ExplosionType ExplosionType, UMeshComponent* MeshComp, const bool bPlaySound, const float Delay)
{
	if(not IsValid(MeshComp) || not IsValid(WorldContextObject))
	{
		return;
	}
	// Get random socket location on mesh.
	const TArray<FName> SocketNames = MeshComp->GetAllSocketNames();
	if(SocketNames.Num() == 0)
	{
		return;
	}
	const int32 RandomIndex = FMath::RandRange(0, SocketNames.Num() - 1);
	const FName RandomSocketName = SocketNames[RandomIndex];
	const FVector SocketLocation = MeshComp->GetSocketLocation(RandomSocketName);
	RTSSpawnExplosionAtLocation(WorldContextObject, ExplosionType, SocketLocation, bPlaySound, Delay);
}

void URTSBlueprintFunctionLibrary::RTSSpawnExplosionAtRandomSocketContaining(const UObject* WorldContextObject,
	const ERTS_ExplosionType ExplosionType, UMeshComponent* MeshComp, const FString StringToContain,
	const bool bPlaySound, const float Delay)
{
	
	if(not IsValid(MeshComp) || not IsValid(WorldContextObject))
	{
		return;
	}
	// Get random socket location on mesh.
	const TArray<FName> SocketNames = MeshComp->GetAllSocketNames();
	if(SocketNames.Num() == 0)
	{
		return;
	}
	TArray<FName> FilteredSocketNames;
	for (const FName& SocketName : SocketNames)
	{
		if (SocketName.ToString().Contains(StringToContain))
		{
			FilteredSocketNames.Add(SocketName);
		}
	}
	if (FilteredSocketNames.Num() == 0)
	{
		return;
	}
	const int32 RandomIndex = FMath::RandRange(0, FilteredSocketNames.Num() - 1);
	const FName RandomSocketName = FilteredSocketNames[RandomIndex];
	const FVector SocketLocation = MeshComp->GetSocketLocation(RandomSocketName);
	RTSSpawnExplosionAtLocation(WorldContextObject, ExplosionType, SocketLocation, bPlaySound, Delay);
}

void URTSBlueprintFunctionLibrary::RTSSpawnVerticalAnimatedTextAtLocation(const UObject* WorldContextObject,
                                                                          const FString& InText,
                                                                          const FVector& InWorldStartLocation,
                                                                          const bool bInAutoWrap, const float InWrapAt,
                                                                          const TEnumAsByte<ETextJustify::Type>
                                                                          InJustification,
                                                                          const FRTSVerticalAnimTextSettings&
                                                                          InSettings)
{
	if (not IsValid(WorldContextObject))
	{
		return;
	}
	UWorld* World = WorldContextObject->GetWorld();
	if (not World)
	{
		return;
	}
	if (const UAnimatedTextWorldSubsystem* Subsystem = World->GetSubsystem<UAnimatedTextWorldSubsystem>())
	{
		if (UAnimatedTextWidgetPoolManager* PoolManager = Subsystem->GetAnimatedTextWidgetPoolManager())
		{
			PoolManager->ShowAnimatedText(InText, InWorldStartLocation, bInAutoWrap, InWrapAt,
			                              InJustification, InSettings);
		}
	}
}

void URTSBlueprintFunctionLibrary::RTSSpawnVerticalAnimatedTextAttachedToActor(const UObject* WorldContextObject,
                                                                               const FString& InText,
                                                                               AActor* InAttachActor,
                                                                               const FVector& InAttachOffset,
                                                                               const bool bInAutoWrap,
                                                                               const float InWrapAt,
                                                                               const TEnumAsByte<ETextJustify::Type>
                                                                               InJustification,
                                                                               const FRTSVerticalAnimTextSettings&
                                                                               InSettings)
{
	if (not IsValid(WorldContextObject) || not IsValid(InAttachActor))
	{
		return;
	}
	UWorld* World = WorldContextObject->GetWorld();
	if (not World)
	{
		return;
	}
	if (const UAnimatedTextWorldSubsystem* Subsystem = World->GetSubsystem<UAnimatedTextWorldSubsystem>())
	{
		if (UAnimatedTextWidgetPoolManager* PoolManager = Subsystem->GetAnimatedTextWidgetPoolManager())
		{
			PoolManager->ShowAnimatedTextAttachedToActor(
				InText, InAttachActor, InAttachOffset, bInAutoWrap, InWrapAt,
				InJustification, InSettings);
		}
	}
}

int URTSBlueprintFunctionLibrary::RTSActivatedTimedProgressBar(const UObject* WorldContextObject, float RatioStart,
                                                               float TimeTillFull, bool bUsePercentageText,
                                                               ERTSProgressBarType BarType,
                                                               const bool bUseDescriptionText,
                                                               const FString& InText, const FVector& Location,
                                                               const float ScaleMlt)
{
	if (not IsValid(WorldContextObject))
	{
		return -1;
	}
	UWorld* World = WorldContextObject->GetWorld();
	if (not World)
	{
		return -1;
	}
	if (URTSTimedProgressBarWorldSubsystem* Subsystem = World->GetSubsystem<URTSTimedProgressBarWorldSubsystem>())
	{
		return Subsystem->ActivateTimedProgressBar(RatioStart, TimeTillFull, bUsePercentageText, BarType,
		                                           bUseDescriptionText, InText, Location, ScaleMlt);
	}
	return -1;
}

int URTSBlueprintFunctionLibrary::ActivateTimedProgressBarAnchored(const UObject* WorldContextObject,
                                                                   USceneComponent* AnchorComponent,
                                                                   const FVector AttachOffset, float RatioStart,
                                                                   float TimeTillFull,
                                                                   bool bUsePercentageText, ERTSProgressBarType BarType,
                                                                   const bool bUseDescriptionText,
                                                                   const FString& InText,
                                                                   const float ScaleMlt)
{
	if (not IsValid(WorldContextObject))
	{
		return -1;
	}
	UWorld* World = WorldContextObject->GetWorld();
	if (not World)
	{
		return -1;
	}
	if (URTSTimedProgressBarWorldSubsystem* Subsystem = World->GetSubsystem<URTSTimedProgressBarWorldSubsystem>())
	{
		return Subsystem->ActivateTimedProgressBarAnchored(
			AnchorComponent, AttachOffset, RatioStart, TimeTillFull, bUsePercentageText, BarType,
			bUseDescriptionText, InText, ScaleMlt);
	}
	return -1;
}


int32 URTSBlueprintFunctionLibrary::CreateRTSRadius(const UObject* WorldContextObject, const FVector& Location, float Radius, ERTSRadiusType Type, float LifeTime)
{
    if (!IsValid(WorldContextObject))
    {
        return -1;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return -1;
    }

    if (URTSRadiusPoolSubsystem* Subsystem = World->GetSubsystem<URTSRadiusPoolSubsystem>())
    {
        return Subsystem->CreateRTSRadius(Location, Radius, Type, LifeTime);
    }

    return -1;
}

void URTSBlueprintFunctionLibrary::AttachRTSRadiusToActor(const UObject* WorldContextObject, int32 ID, AActor* TargetActor, FVector RelativeOffset)
{

    if (!IsValid(WorldContextObject) || !IsValid(TargetActor))
    {
        return; 
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return; 
    }

    if (URTSRadiusPoolSubsystem* Subsystem = World->GetSubsystem<URTSRadiusPoolSubsystem>())
    {
        Subsystem->AttachRTSRadiusToActor(ID, TargetActor, RelativeOffset);
    }
}


void URTSBlueprintFunctionLibrary::HideRTSRadiusById(const UObject* WorldContextObject, int32 ID)
{
    if (!IsValid(WorldContextObject))
    {
        return;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return;
    }

    if (URTSRadiusPoolSubsystem* Subsystem = World->GetSubsystem<URTSRadiusPoolSubsystem>())
    {
        Subsystem->HideRTSRadiusById(ID);
    }
}

bool URTSBlueprintFunctionLibrary::RTSIsValid(AActor* ActorToCheck)
{
	return RTSFunctionLibrary::RTSIsValid(ActorToCheck);
}

ACPPController* URTSBlueprintFunctionLibrary::GetPlayerController(const UObject* WorldContextObject)
{
	return FRTS_Statics::GetRTSController(WorldContextObject);
}

UMainGameUI* URTSBlueprintFunctionLibrary::GetMainGameUI(const UObject* WorldContextObject)
{
	return FRTS_Statics::GetMainGameUI(WorldContextObject);
}

FVector URTSBlueprintFunctionLibrary::RTSGetLocationAtRandomOffsetProjected(const UObject* WorldContext,
                                                                            const FVector& OriginalLocation,
                                                                            const FVector2D MinMaxXYOffset,
                                                                            bool& bWasSuccessful)
{
	bWasSuccessful = false;
	if (not IsValid(WorldContext))
	{
		return OriginalLocation;
	}
	const float Offset = FMath::FRandRange(MinMaxXYOffset.X, MinMaxXYOffset.Y);
	const float OffsetY = FMath::FRandRange(MinMaxXYOffset.X, MinMaxXYOffset.Y);

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(WorldContext->GetWorld());
	if (not NavSys)
	{
		return OriginalLocation;
	}
	FNavLocation ProjectedLocation;
	const FVector ProjectionExtent(500, 500, 1.f);

	const FVector Location = OriginalLocation + FVector(Offset, OffsetY, 0.f);

	if (NavSys->ProjectPointToNavigation(Location, ProjectedLocation, ProjectionExtent))
	{
		// Successfully projected onto navmesh
		bWasSuccessful = true;
		return ProjectedLocation.Location;
	}
	return OriginalLocation;
}

TArray<UMaterialInterface*> URTSBlueprintFunctionLibrary::BP_AppendMaterialArray(
	const TArray<UMaterialInterface*>& ArrayA, const TArray<UMaterialInterface*>& ArrayB)
{
	TArray<UMaterialInterface*> AppendedArray = ArrayA;
	AppendedArray.Append(ArrayB);
	return AppendedArray;
}

TArray<FWeightedDecalMaterial> URTSBlueprintFunctionLibrary::BP_AppendDecalMaterialArray(
	const TArray<FWeightedDecalMaterial>& ArrayA, const TArray<FWeightedDecalMaterial>& ArrayB)
{
	TArray<FWeightedDecalMaterial> AppendedArray = ArrayA;
	AppendedArray.Append(ArrayB);
	return AppendedArray;
}

void URTSBlueprintFunctionLibrary::PickRandomActor(TArray<AActor*>& ActorArray, AActor*& PickedActor,
                                                   int32& PickedIndex)
{
	PickedActor = nullptr;
	PickedIndex = INDEX_NONE;

	if (ActorArray.Num() > 0)
	{
		// Generate a random index
		PickedIndex = FMath::RandRange(0, ActorArray.Num() - 1);

		// Get actor at random index
		PickedActor = ActorArray[PickedIndex];

		// Remove actor from array
		ActorArray.RemoveAt(PickedIndex);
	}
}

void URTSBlueprintFunctionLibrary::DestroyAllTankMastersInRange(
	const FVector& Origin,
	float Range,
	const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return;
	}

	UWorld* World = WorldContext->GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, ATankMaster::StaticClass(), FoundActors);

	const float RangeSq = FMath::Square(Range);

	for (AActor* Actor : FoundActors)
	{
		if (ATankMaster* Tank = Cast<ATankMaster>(Actor))
		{
			float DistSq = FVector::DistSquared(Tank->GetActorLocation(), Origin);
			if (DistSq <= RangeSq)
			{
				// This will mark the actor for destruction at the end of this frame
				Tank->Destroy();
			}
		}
	}
}

void URTSBlueprintFunctionLibrary::DestroyAllBxpsInRange(
	const FVector& Origin,
	float Range,
	const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return;
	}

	UWorld* World = WorldContext->GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, ABuildingExpansion::StaticClass(), FoundActors);

	const float RangeSq = FMath::Square(Range);

	for (AActor* Actor : FoundActors)
	{
		if (ABuildingExpansion* Bxp = Cast<ABuildingExpansion>(Actor))
		{
			float DistSq = FVector::DistSquared(Bxp->GetActorLocation(), Origin);
			if (DistSq <= RangeSq)
			{
				Bxp->Destroy();
			}
		}
	}
}
