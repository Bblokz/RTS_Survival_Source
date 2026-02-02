// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundBase.h"
#include "MovePlayerCameraTypes.generated.h"

USTRUCT(BlueprintType, Blueprintable)
struct FMovePlayerCamera
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector MoveToLocation = FVector::ZeroVector;

	// Time it takes to get from the current location to the new location.
	// If zero the camera will instantly move to the location.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimeToMove = 0.0f;

	// Time during which camera input is disabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimeCameraInputDisabled = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* MoveSound = nullptr;
};
