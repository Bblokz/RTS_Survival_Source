// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Navigation/PathFollowingComponent.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Navigation/RTSNavAI/IRTSNavAI.h"
#include "VehicleAI/VehicleAIController.h"
#include "AITankMaster.generated.h"

class UTrackPathFollowingComponent;
struct FAIRequestID;
struct FPathFollowingResult;
// Forward Declaration
class RTS_SURVIVAL_API ATankMaster;
/**
 * 
 */

USTRUCT()
struct FTankSteering
{
	GENERATED_BODY()
	float LeftTrack;
	float RightTrack;
};


/**
 * @brief AI controller for tank masters that centralises tank-only navigation handling and diagnostics.
 */
UCLASS()
class RTS_SURVIVAL_API AAITankMaster : public AVehicleAIController, public IRTSNavAIInterface
{
	GENERATED_BODY()
	
public:
	AAITankMaster(const FObjectInitializer& ObjectInitializer);
	
	UFUNCTION(BlueprintPure)
	inline ATankMaster* GetControlledTank() const { return ControlledTank; };

	// Stops all AI brain logic.
	void StopBehaviourTreeAI();

	// overwritten in chaos tank AI to directly set movement controls. Stops movement using BP instance
	// for derived Tankmasters.
	virtual void StopMovement() override;
	
	// /**
	//  * @brief Calculates the path towards locationMoveTo, stores all points in TLocationsToDestination
	//  * resets the CurrentLocationIndex
	//  * @param locationMoveTo: The location to which the path of points is calculated.
	//  * @param bFoundPath: Whether a valid path was found.
	//  * @post CurrentLocationIndex = -1.
	//  */ 
	// UFUNCTION(BlueprintCallable, Category = "AI pathfinding")
	// TArray<FVector> SearchPathToLocation(
	// 	const FVector& locationMoveTo,
	// 	bool& bFoundPath);

	inline void SetMoveToLocation(const FVector& Location) {m_MoveToLocation = Location;};

	/**
	 * @brief Keeps movement issuing in C++ so tracked tanks can bypass BT handoff gaps that cause COL stutter.
	 * @param Location Destination used for the active move request.
	 * @note Direct controller move requests need explicit queue-completion wiring in OnMoveCompleted.
	 */
	bool MoveToLocationWithGoalAcceptance(const FVector Location, const float GoalAcceptanceRadiusOverride = -1.f);

	/**
	 * @brief Stores which movement ability should be completed when the controller reports success.
	 * @param CompletionAbility Queue ability id expected to complete on successful arrival.
	 * @note This keeps DoneExecutingCommand source-of-truth in C++ when BT move task is not the movement owner.
	 */
	void SetQueuedMovementCompletionAbility(const EAbilityID CompletionAbility);

	// Ensures harvesters keep UE blocked-move detection disabled throughout harvesting.
	void SetHarvesterMoveBlockDetectionSuppressed(const bool bShouldSuppress);

	virtual void SetDefaultQueryFilter(const TSubclassOf<UNavigationQueryFilter> NewDefaultFilter) override final;

	virtual void OnQueuedMovementRequestFailed(const EAbilityID FailedMovementAbility);
protected:

	virtual void OnPossess(APawn* InPawn) override;

	virtual void BeginPlay() override;

	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	virtual void OnQueuedMovementCompleted(const EAbilityID CompletedMovementAbility);
	virtual void OnQueuedMovementFailed(
		const EAbilityID FailedMovementAbility,
		const EPathFollowingResult::Type FailedMovementResultCode);

	
	TArray<FVector> TLocationsToDestination;

	// Used in BT to obtain the location to move to.
	UFUNCTION(BlueprintCallable)
	inline FVector GetMoveToLocation() const {return m_MoveToLocation;}; 
	
	/** @brief specifies the navigation filter used for tanks */
    UPROPERTY(BlueprintReadWrite)
    TSubclassOf<UNavigationQueryFilter> NavFilterTank;

	virtual void FindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query, FNavPathSharedPtr& OutPath) const override final;

	/** @brief: the tank controlled by this AI */
	UPROPERTY(BlueprintReadOnly)
	ATankMaster* ControlledTank;

private:

	// Set by controlled unit, used for movement behaviour trees.
	FVector m_MoveToLocation;

	bool GetIsValidControlledTank() const;
	bool GetIsValidVehiclePathComp() const;

	void OnPossess_SetupNavAgent(APawn* InPawn) const;
	void OnPosses_ConfigureBlockingDectionDistanceWithRTSRadius(ATankMaster* MyControlledTank);

	void OnFindPath_ClearOverlapsForNewMovement() const;

	/**
	 * @brief Runs the move request and retries once recovery teleports are applied.
	 * @param Location The desired world-space destination for this command.
	 * @param GoalAcceptanceRadius The acceptance radius used for this move request.
	 * @return True when any attempt starts path following, false when all attempts fail.
	 */
	bool TryMoveToLocationWithOffNavRecovery(const FVector& Location, const float GoalAcceptanceRadius);

	/**
	 * @brief Attempts to recover from an off-nav start by projecting and teleporting the controlled pawn.
	 * @param Location Current move destination used to key retry exhaustion for this command.
	 * @return True when recovery changed actor position and a retry should be attempted.
	 */
	bool TryRecoverFromOffNavStartForMoveLocation(const FVector& Location);

	/**
	 * @brief Finds the nav-projected location used by off-nav recovery checks.
	 * @param OutProjectedLocation Receives the nearest projected nav location when available.
	 * @return True when nav projection succeeds for the current pawn location.
	 */
	bool TryProjectPawnLocationToNavigation(FVector& OutProjectedLocation) const;

	/**
	 * @brief Validates whether a nav projection is far enough to confidently classify the pawn as off-nav.
	 * @param CurrentLocation Current pawn world location.
	 * @param ProjectedLocation Projected navmesh location derived from CurrentLocation.
	 * @return True when projection delta exceeds strict off-nav thresholds.
	 */
	bool GetIsOffNavProjectionDeltaSignificant(const FVector& CurrentLocation, const FVector& ProjectedLocation) const;

	/**
	 * @brief Converts a move destination into a stable key so retry exhaustion is scoped per target bucket.
	 * @param Location Destination to quantise into a retry key.
	 * @return Grid key used by off-nav retry tracking.
	 */
	FIntVector BuildMoveLocationRetryKey(const FVector& Location) const;

	/**
	 * @brief Checks and increments the retry budget for a move target bucket.
	 * @param Location Destination whose retry budget should be consumed.
	 * @return True when another retry is allowed, false when the bucket is exhausted.
	 */
	bool TryConsumeOffNavRetryBudgetForLocation(const FVector& Location);

	/**
	 * @brief Clears retry exhaustion state for a destination once movement succeeds.
	 * @param Location Destination whose retry tracking should be reset.
	 */
	void ResetOffNavRetryBudgetForLocation(const FVector& Location);
	
	UPROPERTY()
	TObjectPtr<UTrackPathFollowingComponent> m_VehiclePathComp;

	bool bM_HasQueuedMovementCompletionAbility = false;
	EAbilityID M_QueuedMovementCompletionAbility = EAbilityID::IdNoAbility;

	// Tracks how many off-nav recovery teleports were consumed per quantised destination.
	TMap<FIntVector, int32> M_OffNavRetriesPerLocation;

	void DebugFoundPathCost(const FNavPathSharedPtr& OutPath) const;
	void DebugPathPointsAndFilter(const FNavPathSharedPtr& OutPath, const FAIMoveRequest& MoveRequest) const;
	void DebugPathFollowingResult(const FPathFollowingResult& Result) const;
	void DebugPathFollowingResult_Draw(const FString& DebugText, const FColor& DebugColor, const ATankMaster* ValidTankMaster) const;

	inline static constexpr int32 M_MaxOffNavRecoveryRetriesPerLocation = 2;
	inline static constexpr float M_OffNavRetryGridSizeUnits = 200.f;
	inline static constexpr float M_OffNavProjectionDelta2DThresholdUnits = 160.f;
	inline static constexpr float M_OffNavProjectionDeltaZThresholdUnits = 120.f;
	inline static FVector M_OffNavProjectionExtent = FVector(300.f, 300.f, 300.f);

};
