#pragma once

#include "CoreMinimal.h"
#include "StrategicActionRequirements/StrategicAIActionRequirements.h"

#include "StrategicActions.generated.h"

UENUM(Blueprintable)
enum class ESubtypeAction : uint8
{
	DEFAULT_OBJECT,
	AttackMoveToPlayerUnits,
	AttackMoveLightTanksToPlayerUnits,
	HeavyTankPushPlayerBaseOrUnits,
	FlankPlayerHeavies,
	AttackMoveToPlayerHQ,
	AttackMoveToPlayerResourceBuildings,
	AttackMoveSpecificPoints,
	DefendBase,
	DefendImportantMissionPoint,
};

/**
 * @brief Used as instanced tree entries that score and gate strategic AI commands.
 */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAISubAction : public UObject
{
	GENERATED_BODY()

public:
	UStrategicAISubAction() = default;

	UPROPERTY(VisibleAnywhere)
	ESubtypeAction SubtypeAction = ESubtypeAction::DEFAULT_OBJECT;

	float GetScore() const
	{
		return M_Score;
	}

	const TArray<TObjectPtr<UStrategicAIActionRequirement>>& GetRequirements() const
	{
		return M_Requirements;
	}

	const TArray<TObjectPtr<UStrategicAIActionRequirement>>& GetNativeVisibleRequirements() const
	{
		return M_NativeVisibleRequirements;
	}

	bool GetAreRequirementsMet(
		const FStrategicAIBlackboard& RequirementContext,
		const float GameTimeSeconds) const;

	virtual FString GetDebugString() const;

	FString GetNameFromActionEnum()const;

protected:
	void AddNativeVisibleRequirement(UStrategicAIActionRequirement* Requirement);

	FString GetRequirementsDebugString() const;

private:
	bool GetAreRequirementsMetForArray(
		const TArray<TObjectPtr<UStrategicAIActionRequirement>>& Requirements,
		const FStrategicAIBlackboard& RequirementContext,
		const float GameTimeSeconds) const;
	FString GetRequirementsDebugStringForArray(
		const TArray<TObjectPtr<UStrategicAIActionRequirement>>& Requirements,
		const FString& HeaderText) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float M_Score = 1.0f;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TArray<TObjectPtr<UStrategicAIActionRequirement>> M_Requirements;

	// Native requirements are visible for designer clarity, but only C++ classes decide what is always enforced.
	UPROPERTY(VisibleAnywhere, Instanced, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TArray<TObjectPtr<UStrategicAIActionRequirement>> M_NativeVisibleRequirements;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_AttackMoveToPlayerUnits final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_AttackMoveToPlayerUnits();

	virtual FString GetDebugString() const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_AttackMoveToPlayerHQ final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_AttackMoveToPlayerHQ();

	virtual FString GetDebugString() const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_AttackMoveToPlayerResourceBuildings final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_AttackMoveToPlayerResourceBuildings();

	virtual FString GetDebugString() const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_AttackSpecificPoints final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_AttackSpecificPoints();

	const TArray<FVector>& GetTargetPoint() const
	{
		return TargetPoints;
	}

	virtual FString GetDebugString() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true, MakeEditWidget = true))
	TArray<FVector> TargetPoints ={}; 
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_DefendBase final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_DefendBase();

	virtual FString GetDebugString() const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_DefendImportantMissionPoint final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_DefendImportantMissionPoint();

	virtual FString GetDebugString() const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_LightTanksAttackPlayerUnits final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_LightTanksAttackPlayerUnits();

	virtual FString GetDebugString() const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_HeavyTankPushPlayerBaseOrUnits final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_HeavyTankPushPlayerBaseOrUnits();

	virtual FString GetDebugString() const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_FlankPlayerHeavies final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_FlankPlayerHeavies();

	virtual FString GetDebugString() const override;
};
