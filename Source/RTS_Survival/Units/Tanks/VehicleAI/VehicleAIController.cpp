// Copyright 2020 313 Studios. All Rights Reserved.


#include "VehicleAIController.h"

#include "NavigationSystem.h"
#include "Components/VehiclePathFollowingComponent.h"

// class UVehiclePathFollowingComponent;

AVehicleAIController::AVehicleAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UVehiclePathFollowingComponent>(TEXT("PathFollowingComponent")))
{
}

FVector AVehicleAIController::OnNewPathSegment_Implementation(int PathPointIndex, FVector StartSegmentLocation, FVector TargetSegmentLocation, const UClass* AreaClass, bool bIsOnNavLink)
{
	return TargetSegmentLocation;
}
