// Copyright (C) Bas Blokzijl - All rights reserved.


#include "ObstacleEnvActor.h"

#include "RTS_Survival/Environment/InstanceHelpers/FRTSInstanceHelpers.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"


AObstacleEnvActor::AObstacleEnvActor(const FObjectInitializer& ObjectInitializer)
	: AEnvironmentActor(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

void AObstacleEnvActor::BeginPlay()
{
	Super::BeginPlay();
}

void AObstacleEnvActor::SetupCollision(TArray<UMeshComponent*> Components, const bool bBlockWeapons)
{
	for (auto EachComp : Components)
	{
		FRTS_CollisionSetup::SetupObstacleCollision(EachComp, bBlockWeapons);
	}
}

void AObstacleEnvActor::SetupHierarchicalInstanceBox_BP(UInstancedStaticMeshComponent* InstStaticMesh,
                                                                  FVector2D BoxBounds,
                                                                  float DistanceBetweenInstances,
                                                                  AActor* RequestingActor)
{
	FRTSInstanceHelpers::SetupHierarchicalInstanceBox(InstStaticMesh, BoxBounds, DistanceBetweenInstances, RequestingActor);
}

void AObstacleEnvActor::SetupHierarchicalInstanceAlongRoad_BP(FRTSRoadInstanceSetup RoadSetup,
                                                                      UInstancedStaticMeshComponent* InstStaticMesh,
                                                                      AActor* RequestingActor)
{
	// Note: The original function accepts a const AActor*; AActor* is implicitly convertible.
	FRTSInstanceHelpers::SetupHierarchicalInstanceAlongRoad(RoadSetup, InstStaticMesh, RequestingActor);
}

