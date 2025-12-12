// Copyright 2020 313 Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "VehicleAIController.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API AVehicleAIController : public AAIController
{
	GENERATED_BODY()


public:

	AVehicleAIController(const FObjectInitializer& ObjectInitializer);


	/**
	* Allows you to change the current path point to a different location (Debug mode: Yellow square).
	* This is executed once every time the vehicle reaches a new path point, so can be more optimial than GetCustomDestination (executed every frame)
	* @param PathPointIndex - The index of this new path point, which can then be used to check other path points relative to this one
	* @param StartSegmentLocation - The point which the vehicle reached when it triggered this event
	* @param TargetSegmentLocation - The unchanged path point this vehicle is trying to reach
	* @param AreaClass - The area class of the target segment
	* @param bIsOnNavLink - Whether or not the target segment is on a nav link or not
	* @return The new location for this path point to be set
	*/
	UFUNCTION(BlueprintNativeEvent, Category = "Custom Path Control")
	FVector OnNewPathSegment(int PathPointIndex, FVector StartSegmentLocation, FVector TargetSegmentLocation, const UClass* AreaClass, bool bIsOnNavLink);

};
