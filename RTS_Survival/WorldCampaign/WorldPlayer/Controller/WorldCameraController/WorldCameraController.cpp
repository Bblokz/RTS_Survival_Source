// Copyright (C) Bas Blokzijl - All rights reserved.


#include "WorldCameraController.h"

namespace WorldCameraSettings
{
	constexpr float XYSpeed = 800.f;
	constexpr float ZoomSpeed  = 300.f;
}

// Sets default values for this component's properties
UWorldCameraController::UWorldCameraController()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}


// Called when the game starts
void UWorldCameraController::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UWorldCameraController::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

