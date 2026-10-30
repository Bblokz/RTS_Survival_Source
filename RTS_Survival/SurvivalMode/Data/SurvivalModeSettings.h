// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/SurvivalMode/Army/SurvivalArmyTypes.h"
#include "RTS_Survival/SurvivalMode/Army/SurvivalRewardTypes.h"

#include "SurvivalModeSettings.generated.h"

class AAircraftMaster;
class UGA_Paratroopers;
class USoundBase;
class USurvivalStartContentSettings;
class USurvivalSovietBaseSettings;
class USurvivalWaveSettings;

/**
 * @brief How the starting army is flown in after the player picked a start location.
 */
USTRUCT(BlueprintType)
struct FSurvivalDeploymentSettings
{
	GENERATED_BODY()

	// Carries one vehicle (tank, armored car, nomadic truck, harvester) hanging below it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deployment")
	TSoftClassPtr<AAircraftMaster> VehicleDropAircraftClass;

	// Relative Z of the vehicle under the aircraft while carried.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deployment")
	float VehicleAttachZOffset = -250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deployment", meta = (ClampMin = "0.0"))
	float VehicleDropStayLandedSeconds = 2.f;

	// Template for the infantry drop; its aircraft class, jump interval and descent settings are used,
	// its squad list is replaced by the army's squads. Empty: the paratrooper ability of the global ability set.
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Deployment")
	TObjectPtr<UGA_Paratroopers> ParatrooperTemplate = nullptr;

	// Used when ParatrooperTemplate is empty: copied from DA_GlobalAbilitySystem.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deployment")
	EGlobalAbility ParatrooperAbilityType = EGlobalAbility::GA_GerParatroopers;

	// Squads that jump from one transport.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deployment", meta = (ClampMin = "1"))
	int32 SquadsPerParatrooperAircraft = 3;

	// Squads that cannot jump (team weapons, walkers): a drop aircraft lands and they walk out.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deployment")
	TArray<ESquadSubtype> SquadsDeliveredByLandingAircraft = {
		ESquadSubtype::Squad_Ger_PaK38, ESquadSubtype::Squad_Ger_PaK40, ESquadSubtype::Squad_Ger_37mmFlak,
		ESquadSubtype::Squad_Ger_88mmFlak, ESquadSubtype::Squad_Ger_GrW42_80mm, ESquadSubtype::Squad_Ger_LefH18,
		ESquadSubtype::Squad_Ger_210MM_Morser, ESquadSubtype::Squad_Ger_SFH18_150mm,
		ESquadSubtype::Squad_Ger_Nebelwerfer, ESquadSubtype::Squad_Ger_RedSpine, ESquadSubtype::Squad_Ger_FelsKrieger,
		ESquadSubtype::Squad_Ger_KR_12_Dachs
	};

	// Squads per landing aircraft for SquadsDeliveredByLandingAircraft.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deployment", meta = (ClampMin = "1"))
	int32 SquadsPerLandingAircraft = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deployment", meta = (ClampMin = "0.0"))
	float SquadDropStayLandedSeconds = 5.f;

	// Seconds between two aircraft of the drop; the HQ aircraft always goes first.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deployment", meta = (ClampMin = "0.0"))
	float SecondsBetweenAircraft = 1.25f;

	// How far from the deploy centre the aircraft start, along the direction to the nearest map edge.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deployment", meta = (ClampMin = "1000.0"))
	float ApproachDistance = 30000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deployment", meta = (ClampMin = "0.0"))
	float AircraftFlightHeight = 3500.f;

	// Vehicles land on rings around the deploy centre with this spacing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deployment", meta = (ClampMin = "200.0"))
	float VehicleLandingSpacing = 1100.f;

	// Infantry land this far beyond the outermost vehicle ring.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deployment", meta = (ClampMin = "0.0"))
	float InfantryLandingOffset = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deployment")
	TObjectPtr<USoundBase> DeploymentStartedSound = nullptr;
};

/**
 * @brief When the run ends.
 */
UENUM(BlueprintType)
enum class ESurvivalDefeatRule : uint8
{
	// Defeat when the HQ is destroyed.
	HQDestroyed,
	// Defeat when the HQ and every unit and building are gone.
	EverythingDestroyed
};

/**
 * @brief Everything a survival level is configured with; one asset per map (or shared between maps).
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API USurvivalModeSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Army")
	FSurvivalArmySettings Army;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Deployment")
	FSurvivalDeploymentSettings Deployment;

	// Seconds between the HQ landing and the first wave.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Grace Period", meta = (ClampMin = "0.0"))
	float GracePeriodSeconds = 180.f;

	// Intermission between waves and the reinforcement sets offered in it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Rewards")
	FSurvivalRewardSettings Rewards;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Defeat")
	ESurvivalDefeatRule DefeatRule = ESurvivalDefeatRule::HQDestroyed;

	// Resource fields, wrecks and capturable pits generated around the chosen start location.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Start Content")
	TObjectPtr<USurvivalStartContentSettings> StartContent = nullptr;

	// Wave growth per difficulty, wave composition by tech stage, edge selection.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Waves")
	TObjectPtr<USurvivalWaveSettings> WaveSettings = nullptr;

	// Soviet bases generated on the start locations the player did not pick.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Soviet Bases")
	TObjectPtr<USurvivalSovietBaseSettings> SovietBaseSettings = nullptr;
};
