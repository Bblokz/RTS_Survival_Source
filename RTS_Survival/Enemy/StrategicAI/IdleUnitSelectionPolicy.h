#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "IdleUnitSelectionPolicy.generated.h"

UENUM(BlueprintType)
enum class EIdleUnitSelectionRuleType : uint8
{
	AnyIdleUnit,
	AnyTank,
	TankSubtype,
	SquadSubtype,
	TankCategory,
};

UENUM(BlueprintType)
enum class EIdleUnitSelectionTankCategory : uint8
{
	ArmoredCar,
	LightTank,
	MediumTank,
	HeavyTank,
};

USTRUCT(BlueprintType)
struct FIdleUnitSelectionRule
{
	GENERATED_BODY()

	static FIdleUnitSelectionRule CreateAnyTankRule(const int32 RequiredAmount);
	static FIdleUnitSelectionRule CreateTankSubtypeRule(const int32 RequiredAmount, const ETankSubtype TankSubtype);
	static FIdleUnitSelectionRule CreateSquadSubtypeRule(const int32 RequiredAmount, const ESquadSubtype SquadSubtype);
	static FIdleUnitSelectionRule CreateTankCategoryRule(
		const int32 RequiredAmount,
		const EIdleUnitSelectionTankCategory TankCategory);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EIdleUnitSelectionRuleType RuleType = EIdleUnitSelectionRuleType::AnyIdleUnit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	int32 RequiredAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETankSubtype RequiredTankSubtype = ETankSubtype::Tank_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESquadSubtype RequiredSquadSubtype = ESquadSubtype::Squad_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EIdleUnitSelectionTankCategory RequiredTankCategory = EIdleUnitSelectionTankCategory::LightTank;
};

/**
 * @brief Built by strategic subactions to let direct control consume blackboard idle units safely.
 */
USTRUCT(BlueprintType)
struct FIdleUnitSelectionPolicy
{
	GENERATED_BODY()

	void SetupFallbackMinMax(const int32 FallbackMinUnitsToPick, const int32 FallbackMaxUnitsToPick);
	void AddMinimumIdleUnitsContribution(const int32 RequiredAmount);
	void AddRequiredRule(const FIdleUnitSelectionRule& Rule);
	void NormalizeMinMax();
	int32 GetRequiredRuleUnitCount() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	int32 MinUnitsToPick = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	int32 MaxUnitsToPick = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FIdleUnitSelectionRule> RequiredRules;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasRequirementDrivenRules = false;
};
