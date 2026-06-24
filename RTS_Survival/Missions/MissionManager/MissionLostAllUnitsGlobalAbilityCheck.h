#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "MissionLostAllUnitsGlobalAbilityCheck.generated.h"

class UGlobalAbility;

UENUM(BlueprintType)
enum class EReinforcementAirdropLocation : uint8
{
	NotSet,
	OverrideLocation,
	CommanderVehicleLocation,
	HQLocation
};

/**
 * @brief Configures one mission-manager reinforcement check that fires after the player loses a unit subtype.
 * Runtime flags live with the setup data so the mission manager only ticks an array of checks.
 */
USTRUCT(BlueprintType)
struct FMissionLostAllUnitsGlobalAbilityCheck
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Reinforcement")
	TSubclassOf<UGlobalAbility> M_GlobalAbilityClass;
	
	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_GlobalAbility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Reinforcement")
	EReinforcementAirdropLocation M_AirdropLocation = EReinforcementAirdropLocation::NotSet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Reinforcement")
	EAllUnitType M_UnitType = EAllUnitType::UNType_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Reinforcement",
		meta = (EditCondition = "M_UnitType == EAllUnitType::UNType_Squad", EditConditionHides))
	ESquadSubtype M_SquadSubtype = ESquadSubtype::Squad_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Reinforcement",
		meta = (EditCondition = "M_UnitType == EAllUnitType::UNType_Tank", EditConditionHides))
	ETankSubtype M_TankSubtype = ETankSubtype::Tank_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Reinforcement",
		meta = (EditCondition = "M_UnitType == EAllUnitType::UNType_Nomadic", EditConditionHides))
	ENomadicSubtype M_NomadicSubtype = ENomadicSubtype::Nomadic_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Reinforcement",
		meta = (EditCondition = "M_AirdropLocation == EReinforcementAirdropLocation::OverrideLocation",
			EditConditionHides))
	FVector M_OverrideLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Reinforcement")
	FVector2D M_XYOffset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Reinforcement")
	bool bM_RenablePostGAComplete = false;

	UPROPERTY(Transient)
	bool bM_IsExecutingGlobalAbility = false;
};
