// Copyright 2020 313 Studios. All Rights Reserved.
// Modifications by Bas Blokzijl for RTS_Survival


#include "VehiclePathFollowingComponent.h"

#include "AIConfig.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Units/Tanks/VehicleAI/VehicleAIController.h"
#include "RTS_Survival/Units/Tanks/VehicleAI/Utils/VehicleAIFunctionLibrary.h"
#include "RTS_Survival/Units/Tanks/VehicleAI/Utils/VehicleAIInterface.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"
#include "Kismet/KismetMathLibrary.h"
#include "Curves/CurveFloat.h"
#include "Math/UnrealMathUtility.h"
#include "NavMesh/NavMeshPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "PhysicalMaterials/PhysicalMaterial.h"


#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "AI/NavigationSystemBase.h"
#include "Algo/Reverse.h"



/*********************************/
/* Blueprint Callable Functions */
/*******************************/


UVehiclePathFollowingComponent::UVehiclePathFollowingComponent()
{
}

FVector UVehiclePathFollowingComponent::GetMoveFocus(bool bAllowStrafe) const
{
	return Super::GetMoveFocus(bAllowStrafe);
}
