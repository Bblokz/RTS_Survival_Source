// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "AITrackTank.h"

#include "NavigationSystem.h"
#include "NavMesh/NavMeshPath.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

AAITrackTank::AAITrackTank(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTrackPathFollowingComponent>(TEXT("PathFollowingComponent")))
{
}

ATrackedTankMaster* AAITrackTank::GetTrackedTankMaster() const
{
	if (GetPawn())
	{
		return Cast<ATrackedTankMaster>(GetPawn());
	}
	return nullptr;
}


void AAITrackTank::BeginPlay()
{
	Super::BeginPlay();
}

void AAITrackTank::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ATrackedTankMaster* TrackedTank = Cast<ATrackedTankMaster>(InPawn);
	if (not TrackedTank)
	{
		return;
	}
		UTrackPathFollowingComponent* TrackPathFollowingComp = Cast<UTrackPathFollowingComponent>(GetPathFollowingComponent());
	if(not TrackPathFollowingComp)
	{
		return;
	}
	TrackPathFollowingComp->SetTrackPhysicsComponent(TrackedTank->GetTrackPhysicsMovement());
	
}

FPathFollowingRequestResult AAITrackTank::MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath)
{
	return Super::MoveTo(MoveRequest, OutPath);
}


void AAITrackTank::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
