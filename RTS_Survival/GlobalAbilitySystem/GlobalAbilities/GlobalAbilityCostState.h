#pragma once
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/UnitData/UnitCost.h"

#include "GlobalAbilityCostState.generated.h"

enum class ETechnology : uint8;

USTRUCT(BlueprintType)
struct FGLobalAbilityCostState
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FUnitCost Costs = FUnitCost();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CoolDownTime = 5;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CoolDownRemaining = 0;
	
};


USTRUCT(BlueprintType)
struct FGlobalAbilityRequirements
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETechnology RequiredTechnology = ETechnology::Tech_NONE;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTrainingOption RequiredUnit = FTrainingOption();
	
};

USTRUCT(BlueprintType)
struct FGlobalAbilityAimSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPlayerAimAbilityTypes AimType = EPlayerAimAbilityTypes::None;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AimRadius = 500.f;
};

	
	
