#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/SquadController.h"
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
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const
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
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	float GetRequiredGameTimeSeconds() const
	{
		return RequiredGameTimeSeconds;
	}
	virtual FString GetDebugString() const override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess=true, ClampMin="0.0"))
	float RequiredGameTimeSeconds = 0.0f;
	

private:
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIActorIsValidRequirement final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	AActor* GetRequiredActor() const
	{
		return RequiredActor;
	}
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<AActor> RequiredActor = nullptr;
	

	bool GetIsValidRequiredActor() const;

	virtual FString GetDebugString() const override;

};

/**
 * @brief Used by strategic actions that need a known player HQ target before running.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasPlayerHQLocationRequirement final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	virtual FString GetDebugString() const override;
};

/**
 * @brief Used by strategic actions that need at least one known player resource building target.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasPlayerResourceBuildingLocationsRequirement final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	virtual FString GetDebugString() const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasAtLeastIdleSquads final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ESquadSubtype RequiredSquadSubtype = ESquadSubtype::Squad_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountIdleNeeded = 1;
	
	virtual FString GetDebugString() const override;

};

/**
 * @brief Used by strategic actions to require a minimum number of idle tanks of one subtype.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasAtLeastIdleTanks final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ETankSubtype RequiredTankSubtype = ETankSubtype::Tank_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountIdleNeeded = 1;

	virtual FString GetDebugString() const override;
};


/**
 * @brief Used by strategic actions to require a minimum number of idle tanks of any subtype.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasAtLeastAnyIdleTanks final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;


	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountIdleNeeded = 1;

	virtual FString GetDebugString() const override;
};

/**
 * @brief Used by strategic actions to require a minimum number of idle aircraft of one subtype.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasAtLeastIdleAircraft final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EAircraftSubtype RequiredAircraftSubtype = EAircraftSubtype::Aircarft_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountIdleNeeded = 1;

	virtual FString GetDebugString() const override;
};


// By default set to 4 light tanks needed.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasEnoughLightTanks final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountIdleNeeded = 4;
	
	virtual FString GetDebugString() const override;

};


// By default set to two heavy tanks needed.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasEnoughHeavyTanks final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountIdleNeeded = 2;
	
	virtual FString GetDebugString() const override;

};

// By default set to one player heavy tank needed.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIDoesPlayerHaveHeavyTanks final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountIdleNeeded = 1;
	
	virtual FString GetDebugString() const override;

};


// By default set to one player heavy tank needed.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIDoesBlackboardHaveHeavyTankFlankPositions final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	virtual FString GetDebugString() const override;

};
