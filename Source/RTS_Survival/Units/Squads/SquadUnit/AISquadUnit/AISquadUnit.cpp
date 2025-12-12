// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "AISquadUnit.h"

#include "Navigation/PathFollowingComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Navigation/RTSNavAgentRegistery/RTSNavAgentRegistery.h"
#include "RTS_Survival/Navigation/RTSNavigationHelpers/FRTSNavigationHelpers.h"


// Sets default values
AAISquadUnit::AAISquadUnit()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AAISquadUnit::SetDefaultQueryFilter(const TSubclassOf<UNavigationQueryFilter> NewDefaultFilter)
{
	DefaultNavigationFilterClass = NewDefaultFilter;
}


void AAISquadUnit::BeginPlay()
{
	Super::BeginPlay();
}

void AAISquadUnit::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	OnPosses_SetupNavAgent(InPawn);
}

void AAISquadUnit::OnPosses_SetupNavAgent(APawn* InPawn) const
{
	if (not InPawn)
	{
		return;
	}
	FRTSNavigationHelpers::SetupNavDataForTypeOnPawn(GetWorld(), InPawn);
}
