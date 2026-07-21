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
struct FNavLocation;
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
	 * @brief Ensures physics drift cannot leave a tank without a filter-valid pathfinding start.
	 * @param Location The desired world-space destination for this command.
	 * @param GoalAcceptanceRadius The acceptance radius used for this move request.
	 * @return True when the validated move request starts path following.
	 */
	bool TryMoveToLocationWithOffNavRecovery(const FVector& Location, const float GoalAcceptanceRadius);

	/**
	 * @brief Recovers an invalid start before MoveTo can emit a synchronous invalid completion.
	 * @param MoveRequest Request whose agent, nav data and filter define a valid start.
	 * @return True when the existing or recovered start can be used for pathfinding.
	 */
	bool EnsureMoveStartIsNavigable(const FAIMoveRequest& MoveRequest);

	/**
	 * @brief Mirrors the normal Recast start projection with the move request's exact filter.
	 * @param PathFindingQuery Query containing the active agent navigation context.
	 * @return True when normal pathfinding can resolve a permitted start polygon.
	 */
	bool GetIsPathFindingStartNavigable(const FPathFindingQuery& PathFindingQuery) const;

	/**
	 * @brief Moves the pawn only after a bounded, filter-valid start is proven to produce a path.
	 * @param PathFindingQuery Original move query whose start could not be resolved normally.
	 * @return True when the pawn was safely repositioned for the original move request.
	 */
	bool TryRecoverMoveStartToFilterValidLocation(const FPathFindingQuery& PathFindingQuery);

	/**
	 * @brief Finds the nearest polygon permitted by the exact tank filter within the unit-sized recovery area.
	 * @param PathFindingQuery Query supplying the start, nav data and filter.
	 * @param OutRecoveryNavigationLocation Receives the permitted recovery polygon and location.
	 * @return True when the active filter accepts the projected polygon.
	 */
	bool TryProjectFilterValidRecoveryLocation(
		const FPathFindingQuery& PathFindingQuery,
		FNavLocation& OutRecoveryNavigationLocation) const;

	/**
	 * @brief Prevents recovery from moving a tank across an excluded region wider than the unit itself.
	 * @param PathFindingStartLocation Original nav-agent start location.
	 * @param RecoveryNavigationLocation Filter-valid projected location.
	 * @return True when the projection displacement is nonzero and within the cached formation radius.
	 */
	bool GetIsRecoveryProjectionWithinBounds(
		const FVector& PathFindingStartLocation,
		const FVector& RecoveryNavigationLocation) const;

	/**
	 * @brief Avoids mutating physics unless the projected start can serve the original request.
	 * @param PathFindingQuery Original move query to validate from the recovery point.
	 * @param RecoveryNavigationLocation Filter-valid location used as the validation start.
	 * @return True when regular or partial pathfinding succeeds from the recovery point.
	 */
	bool GetDoesRecoveryPointProducePath(
		const FPathFindingQuery& PathFindingQuery,
		const FNavLocation& RecoveryNavigationLocation) const;

	/**
	 * @brief Preserves the actor/nav-agent offset while resetting inertia that caused the invalid start.
	 * @param PathFindingQuery Query containing the original nav-agent start and exact navigation context.
	 * @param RecoveryNavigationLocation Filter-valid nav-agent destination.
	 * @return True when the corrected transform remains valid for the active filter.
	 */
	bool TryTeleportPawnToRecoveryLocation(
		const FPathFindingQuery& PathFindingQuery,
		const FVector& RecoveryNavigationLocation);
	
	UPROPERTY()
	TObjectPtr<UTrackPathFollowingComponent> m_VehiclePathComp;

	bool bM_HasQueuedMovementCompletionAbility = false;
	EAbilityID M_QueuedMovementCompletionAbility = EAbilityID::IdNoAbility;

	// Bounds recovery to the unit's stable formation footprint so barriers cannot be crossed by correction.
	float M_FormationUnitInnerRadius = 0.0f;

	void DebugFoundPathCost(const FNavPathSharedPtr& OutPath) const;
	void DebugPathPointsAndFilter(const FNavPathSharedPtr& OutPath, const FAIMoveRequest& MoveRequest) const;
	void DebugPathFollowingResult(const FPathFollowingResult& Result) const;
	void DebugPathFollowingResult_Draw(const FString& DebugText, const FColor& DebugColor, const ATankMaster* ValidTankMaster) const;

};
