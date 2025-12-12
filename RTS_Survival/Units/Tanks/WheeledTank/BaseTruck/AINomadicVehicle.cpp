// Copyright Bas Blokzijl - All rights reserved.


#include "AINomadicVehicle.h"

#include "NomadicVehicle.h"
#include "BuildingCreationTask/Task_CreateBuilding.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


AAINomadicVehicle::AAINomadicVehicle(const FObjectInitializer& ObjectInitializer)
	: AAITrackTank(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

void AAINomadicVehicle::StopBehaviourTree()
{
	if (BrainComponent)
	{
		BrainComponent->StopLogic("Stop Behaviour Tree");
	}
}


// Called when the game starts or when spawned
void AAINomadicVehicle::BeginPlay()
{
	Super::BeginPlay();
}

void AAINomadicVehicle::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (ANomadicVehicle* NomadicVehicle = Cast<ANomadicVehicle>(InPawn))
	{
		NomadicVehicle->SetAINomadicVehicle(this);
	}
	else
	{
		RTSFunctionLibrary::ReportFailedCastError(
			"InPawn",
			"ANomadicVehicle",
			"AAINomadicVehicle::OnPossess");
	}
}
