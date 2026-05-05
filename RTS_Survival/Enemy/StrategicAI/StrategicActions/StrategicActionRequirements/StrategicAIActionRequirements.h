#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StrategicAIActionRequirements.generated.h"

struct FStrategicAIBlackboard;
class AActor;

// DO NOT USE IN BLUEPRINTS; use the derived requirements!
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIActionRequirement : public UObject
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& RequirementContext, const float GameTimeSeconds) const
	{
		return true;
	}
	virtual FString GetDebugString() const;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIGameTimePassedRequirement final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& RequirementContext, const float GameTimeSeconds) const override;

	float GetRequiredGameTimeSeconds() const
	{
		return M_RequiredGameTimeSeconds;
	}
	virtual FString GetDebugString() const override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(AllowPrivateAccess=true, ClampMin="0.0"))
	float M_RequiredGameTimeSeconds = 0.0f;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIActorIsValidRequirement final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& RequirementContext, const float GameTimeSeconds) const override;

	AActor* GetRequiredActor() const
	{
		return M_RequiredActor;
	}

	bool GetIsValidRequiredActor() const;

	virtual FString GetDebugString() const override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(AllowPrivateAccess=true))
	TObjectPtr<AActor> M_RequiredActor = nullptr;
};
