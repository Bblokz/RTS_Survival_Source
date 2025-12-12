// Copyright 2020 313 Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "RTS_Survival/Units/Tanks/VehicleAI/PIDController.h"
#include "VehiclePathFollowingComponent.generated.h"

// Declares the stat group for CPU performance tracking
DECLARE_STATS_GROUP(TEXT("Vehicle Path Following"), STATGROUP_VehiclePathFollowing, STATCAT_Advanced);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVehiclePathFinished);

class UCurveFloat;





/**
 * Handles the path following for a wheeled vehicle. Inherits from Crowd Following for Detour Crowd control
 */
UCLASS()
class RTS_SURVIVAL_API UVehiclePathFollowingComponent : public UPathFollowingComponent
{
	GENERATED_BODY()

public:
	UVehiclePathFollowingComponent();
protected:

	virtual FVector GetMoveFocus(bool bAllowStrafe) const override;

};
