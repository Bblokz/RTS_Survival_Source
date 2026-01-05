// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "TimerManager.h"
#include "SquadReinforcementComponent.generated.h"

class ASquadController;
class ASquadUnit;
class UPlayerResourceManager;
class UReinforcementPoint;
class UTrainingMenuManager;

USTRUCT()
struct FReinforcementRequestState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bM_IsInProgress = false;

	UPROPERTY()
	FTimerHandle M_TimerHandle;

	UPROPERTY()
	FVector M_CachedLocation = FVector::ZeroVector;

	UPROPERTY()
	TWeakObjectPtr<UReinforcementPoint> M_ReinforcementProvider;

	UPROPERTY()
	TArray<TSubclassOf<ASquadUnit>> M_PendingClasses;
};

/**
 * @brief Handles reinforcement ability activation and spawning missing squad units.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API USquadReinforcementComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class RTS_SURVIVAL_API UFieldConstructionAbilityComponent;

public:
	USquadReinforcementComponent();

	UReinforcementPoint* GetActiveReinforcementPoint() const;
	/**
	 * @brief Toggle the reinforcement ability on the owning squad controller.
	 * @param bActivate True to add ability, false to remove.
	 * @param InstigatingPoint
	 */
	void ActivateReinforcements(const bool bActivate, UReinforcementPoint* InstigatingPoint);

	/**
	 * @brief Update reinforcement availability when squad composition changes.
	 */
	void NotifySquadMembershipChanged();

	/**
	 * @brief Reinforce missing units at the provided reinforcement point.
	 * @param ReinforcementPoint Point that provides the spawn location.
	 */
	void Reinforce(UReinforcementPoint* ReinforcementPoint);

protected:
	virtual void BeginPlay() override;

private:
	static constexpr int32 ReinforcementAbilityIndex = 11;

	void BeginPlay_InitSquadDataSnapshot();
	bool GetIsValidSquadController();
	bool GetIsValidRTSComponent();
	bool EnsureAbilityArraySized();
	void AddReinforcementAbility();
	void RemoveReinforcementAbility();
	void RefreshReinforcementAbility();
	void UpdateMissingSquadMemberState();
	bool DoesSquadNeedReinforcement() const;

	/**
	 * @brief Validate if reinforcement can proceed and gather missing units/location.
	 * @param ReinforcementPoint Source point for reinforcements.
	 * @param OutMissingUnitClasses Missing unit classes for respawn.
	 * @param OutReinforcementLocation Location to spawn reinforcements.
	 * @return True when the request can proceed and the location is valid.
	 */
	bool CanProcessReinforcement(UReinforcementPoint* ReinforcementPoint,
	                             TArray<TSubclassOf<ASquadUnit>>& OutMissingUnitClasses,
	                             FVector& OutReinforcementLocation);
	bool GetIsReinforcementAllowed(UReinforcementPoint* ReinforcementPoint);

	/**
	 * @brief Resolve reinforcement cost based on missing units ratio.
	 * @param MissingUnits Number of units that need replacement.
	 * @param OutCost Computed resource cost rounded up to multiples of five.
	 * @return True when cost data could be resolved.
	 */
	bool TryResolveReinforcementCost(const int32 MissingUnits, TMap<ERTSResourceType, int32>& OutCost);

	/**
	 * @brief Attempt to pay the computed reinforcement cost.
	 * @param ReinforcementCost Resource map to charge.
	 * @return True when payment succeeds.
	 */
	bool TryPayReinforcementCost(const TMap<ERTSResourceType, int32>& ReinforcementCost) const;

	bool TryGetResourceManager(UPlayerResourceManager*& OutManager) const;
	bool TryGetTrainingMenuManager(UTrainingMenuManager*& OutMenuManager) const;
	float CalculateReinforcementTime(const int32 MissingUnits) const;
	bool GetMissingUnitClasses(TArray<TSubclassOf<ASquadUnit>>& OutMissingUnitClasses) const;
	void ScheduleReinforcement(const float ReinforcementTime, const FVector& SpawnLocation,
	                           const TArray<TSubclassOf<ASquadUnit>>& MissingUnitClasses,
	                           UReinforcementPoint* ReinforcementPoint);
	void SpawnMissingUnits();
	void MoveSpawnedUnitsToController(const TArray<ASquadUnit*>& SpawnedUnits) const;
	bool GetIsReinforcementPointEnabled(const UReinforcementPoint* ReinforcementPoint) const;
	void DrawReinforcementDebugString(const FString& DebugText, const FVector& WorldLocation) const;
	void DrawReinforcementDebugSphere(const FVector& WorldLocation, const float Radius, const FColor& Color) const;
	int32 GetMaxSquadUnits() const { return M_MaxSquadUnits; }

	bool bM_IsActivated;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_SquadController;

	// Set when close enough for reinforcement.
	UPROPERTY()
	TWeakObjectPtr<UReinforcementPoint> M_ReinforcementPoint;

	UPROPERTY()
	int32 M_MaxSquadUnits;

	UPROPERTY()
	TArray<TSubclassOf<ASquadUnit>> M_InitialSquadUnitClasses;

	UPROPERTY()
	FReinforcementRequestState M_ReinforcementRequestState;

	bool bM_HasMissingSquadMembers;

	bool bM_ReinforcementAbilityAdded;
};
