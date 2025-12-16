#pragma once

#include "CoreMinimal.h"
#include "UnitCost.h"
#include "RTS_Survival/GameUI/BuildingExpansion/WidgetBxpOptionState/BxpOptionData.h"
#include "RTS_Survival/UnitData/ArmorAndResistanceData.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/RTSExperienceLevels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

#include "NomadicVehicleData.generated.h"

struct FUnitCost;
class RTSFunctionLibrary;
enum class EAbilityID : uint8;
enum class EBuildingExpansionType : uint8;


USTRUCT(Blueprintable)
struct FNomadicData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	float VehicleRotationSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float VehicleMaxSpeedKmh = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float MaxHealthVehicle = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FResistanceAndDamageReductionData Vehicle_ResistancesAndDamageMlt;

	UPROPERTY(BlueprintReadOnly)
	float MaxHealthBuilding = 0.0f;
	
	UPROPERTY(BlueprintReadOnly)
	FResistanceAndDamageReductionData Building_ResistancesAndDamageMlt;

	UPROPERTY(BlueprintReadOnly)
	TArray<FTrainingOption> TrainingOptions = {};

	UPROPERTY(BlueprintReadWrite)
	TArray<FBxpOptionData> BuildingExpansionOptions = {};

	// Used as the construction montage time on nomadic vehicles
	// The time the expanding animation on the mesh takes.
	UPROPERTY(BlueprintReadOnly)
	float VehicleExpansionTime = 5.f;

	// This is the time spend on mesh animation where material slots and smoke effects are used.
	// forms total construction time with the vehicle expansion time.
	UPROPERTY(BlueprintReadOnly)
	float BuildingAnimationTime = 5.f;

	// Time it takes to pack up the building and expansions back into the truck.
	UPROPERTY(BlueprintReadOnly)
	float VehiclePackUpTime = 10.f;

	UPROPERTY(BlueprintReadOnly)
	int MaxAmountBuildingExpansions = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 EnergySupply = 0;

	UPROPERTY(BlueprintReadOnly)
	float BuildRadius = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float ExpansionRadius = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float VisionRadiusAsTruck = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float VisionRadiusAsBuilding = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FUnitCost Cost;

        UPROPERTY(BlueprintReadOnly)
        TArray<FUnitAbilityEntry> Abilities;

	UPROPERTY(BlueprintReadOnly)
	TMap<ERTSResourceType, int32> ResourceDropOffCapacity;

	UPROPERTY()
	float ExperienceWorth = 0.0f;

	UPROPERTY()
	float ExperienceMultiplier = 0.0f;

	UPROPERTY()
	TArray<FExperienceLevel> ExperienceLevels;

	// Scales how fast airports can repair aircraft.
	UPROPERTY(BlueprintReadOnly)
	float RepairPowerMlt = 1.f;
};

static void Global_DebugNomadicData(const FNomadicData& NomadicData)
{
	FString DebugString;

	DebugString += FString::Printf(TEXT("VehicleRotationSpeed: %f\n"), NomadicData.VehicleRotationSpeed);
	DebugString += FString::Printf(TEXT("VehicleMaxSpeedKmh: %f\n"), NomadicData.VehicleMaxSpeedKmh);
	DebugString += FString::Printf(TEXT("MaxHealthVehicle: %f\n"), NomadicData.MaxHealthVehicle);
	DebugString += FString::Printf(TEXT("MaxHealthBuilding: %f\n"), NomadicData.MaxHealthBuilding);

	// TrainingOptions
	DebugString += TEXT("TrainingOptions:\n");
	for (const FTrainingOption& Option : NomadicData.TrainingOptions)
	{
		// Convert FTrainingOption enum value to string
		FString OptionName = Option.GetTrainingName();
		DebugString += FString::Printf(TEXT("  %s\n"), *OptionName);
	}

	// BuildingExpansionOptions
	DebugString += TEXT("BuildingExpansionOptions:\n");
	for (const FBxpOptionData& Expansion : NomadicData.BuildingExpansionOptions)
	{
		// Convert EBuildingExpansionType enum value to string
		FString ExpansionName = FString::FromInt(static_cast<int>(Expansion.ExpansionType));
		DebugString += FString::Printf(TEXT("  %s\n"), *ExpansionName);
	}

	DebugString += FString::Printf(TEXT("VehicleExpansionTime: %f\n"), NomadicData.VehicleExpansionTime);
	DebugString += FString::Printf(TEXT("BuildingAnimationTime: %f\n"), NomadicData.BuildingAnimationTime);
	DebugString += FString::Printf(TEXT("MaxAmountBuildingExpansions: %d\n"), NomadicData.MaxAmountBuildingExpansions);

	// Use your RTSFunctionLibrary to print the DebugString
	RTSFunctionLibrary::PrintString(DebugString);
}
