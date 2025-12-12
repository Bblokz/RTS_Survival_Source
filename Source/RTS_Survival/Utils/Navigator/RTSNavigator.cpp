// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "RTSNavigator.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Units/Tanks/VehicleAI/Components/VehiclePathFollowingComponent.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/AI/AITrackTank.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


// Sets default values
ARTSNavigator::ARTSNavigator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 2.f;
}

void ARTSNavigator::RegisterWithNavigator(UVehiclePathFollowingComponent* Vehicle)
{
	M_TVehicles.Add(Vehicle);
}

void ARTSNavigator::BeginPlay()
{
	Super::BeginPlay();
	// Find all AAITrackTank controllers
	TArray<AActor*> FoundTanks;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAITrackTank::StaticClass(), FoundTanks);

	for (AActor* Actor : FoundTanks)
	{
		AAITrackTank* Tank = Cast<AAITrackTank>(Actor);
		if (Tank)
		{
			// Assuming your tank has a component that is of type UVehiclePathFollowingComponent
			UVehiclePathFollowingComponent* PathComp = Cast<UVehiclePathFollowingComponent>(Tank->GetPathFollowingComponent());
			if (PathComp)
			{
				M_TVehicles.Add(PathComp);
			}
		}
	}
}	

// Called every frame
void ARTSNavigator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	return;
	// for(auto Vehicle : M_TVehicles)
	// {
	// 	FVector VehicleFWD = Vehicle->GetControlledPawn()->GetActorForwardVector();
	// 	FVector TargetLocation = Vehicle->GetControlledPawn()->GetActorLocation();
	// 	Vehicle->ChangeActivePathPoint(TargetLocation + FVector(100,100, 0));
	// 	RTSFunctionLibrary::PrintString("Injected Path Point");
	// 	DrawDebugLine(GetWorld(), TargetLocation, TargetLocation + 300 * VehicleFWD, FColor::Red, false, 1.f, 0, 2.f);
	// }
}

