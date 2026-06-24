// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "AircraftDropTypes.generated.h"

class ATankMaster;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

UENUM(BlueprintType)
enum class EAircraftDropRequestState : uint8
{
	Inactive,
	MovingToDropLocation,
	VerticalLanding,
	LandedWaiting,
	VerticalTakeOff,
	Retreating
};

UENUM(BlueprintType)
enum class EAircraftDropPayloadType : uint8
{
	None,
	Tank,
	Infantry
};

USTRUCT(BlueprintType)
struct FAircraftDropRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	EAircraftDropRequestState State = EAircraftDropRequestState::Inactive;

	UPROPERTY(BlueprintReadWrite)
	EAircraftDropPayloadType PayloadType = EAircraftDropPayloadType::None;

	UPROPERTY(BlueprintReadWrite)
	FVector ExecuteLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite)
	FVector RetreatLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite)
	TArray<ESquadSubtype> SquadSubtypes;

	UPROPERTY(BlueprintReadWrite)
	float TankAttachZOffset = 0.0f;

	UPROPERTY(BlueprintReadWrite)
	float HowLongStayLanded = 0.0f;

	UPROPERTY(BlueprintReadWrite)
	float RadiusSpawnSquads = 0.0f;

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<USoundBase> DropOffSound = nullptr;

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<USoundAttenuation> DropOffSoundAttenuation = nullptr;

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<USoundConcurrency> DropOffSoundConcurrency = nullptr;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> AttachedTank;
	
	float AttachedTankVisionRadius = 0.0f;
};
