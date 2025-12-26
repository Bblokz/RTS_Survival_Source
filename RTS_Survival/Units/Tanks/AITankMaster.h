// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Navigation/RTSNavAI/IRTSNavAI.h"
#include "VehicleAI/VehicleAIController.h"
#include "AITankMaster.generated.h"

class UTrackPathFollowingComponent;
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


UCLASS()
class RTS_SURVIVAL_API AAITankMaster : public AVehicleAIController, public IRTSNavAIInterface
{
	GENERATED_BODY()
	
public:
	AAITankMaster(const FObjectInitializer& ObjectInitializer);
	
	UFUNCTION(BlueprintPure)
	inline ATankMaster* GetControlledTank() const {return ControlledTank;};

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

	void OnPossess_SetupNavAgent(APawn* InPawn) const;

	void OnFindPath_ClearOverlapsForNewMovement() const;
	
	UPROPERTY()
	UTrackPathFollowingComponent* m_VehiclePathComp;

	void DebugFoundPathCost(const FNavPathSharedPtr& OutPath) const;
	void DebugPathPointsAndFilter(const FNavPathSharedPtr& OutPath, const FAIMoveRequest& MoveRequest) const;

};

