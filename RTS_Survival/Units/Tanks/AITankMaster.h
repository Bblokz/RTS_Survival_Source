// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
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

	void MoveToLocationWithGoalAcceptance(const FVector Location);

	virtual void SetDefaultQueryFilter(const TSubclassOf<UNavigationQueryFilter> NewDefaultFilter) override final;
protected:

	virtual void OnPossess(APawn* InPawn) override;

	virtual void BeginPlay() override;

	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	
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
	
	UPROPERTY()
	TObjectPtr<UTrackPathFollowingComponent> m_VehiclePathComp;

	/**
	 * @brief Centralises tank-only cost-limit debug drawing so it cannot affect move execution.
	 * @param Query Active pathfinding query used for cost diagnostics.
	 * @param bIsHighCostStart Whether the start location is inside a high-cost area.
	 */
	void DebugPathCostLimit(const FPathFindingQuery& Query, bool bIsHighCostStart) const;
	void DebugFoundPathCost(const FNavPathSharedPtr& OutPath) const;
	void DebugPathPointsAndFilter(const FNavPathSharedPtr& OutPath, const FAIMoveRequest& MoveRequest) const;
	void DebugPathFollowingResult(const FPathFollowingResult& Result) const;
	void DebugPathFollowingResult_Draw(const FString& DebugText, const FColor& DebugColor, const ATankMaster* ValidTankMaster) const;

};
