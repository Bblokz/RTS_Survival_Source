// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "AircraftDropTypes.generated.h"

class ATankMaster;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

namespace AircraftRopeSling
{
	// Distance from the aircraft origin down to a rope-slung payload's origin; pass -DefaultHangDistance as
	// TankAttachZOffset. Long enough for the ropes to read from the RTS camera and for a truck to clear the plane.
	constexpr float DefaultHangDistance = 1200.0f;

	// While cruising, the slung payload's origin stays at least this high above the highest ground sampled between
	// the aircraft's start and the drop point, so it clears trees and buildings.
	constexpr float MinPayloadCruiseClearance = 1500.0f;

	/**
	 * @brief Lowest cruise height the aircraft climbs to before carrying a rope-slung payload; spawning the aircraft
	 * at or above it saves the climb at the start of the approach.
	 * @param HighestGroundZ Highest ground under the approach (at least the drop point's Z).
	 * @param HangDistance Distance from the aircraft origin down to the payload origin.
	 * @return World Z for the aircraft origin.
	 */
	inline float GetMinCruiseZ(const float HighestGroundZ, const float HangDistance)
	{
		return HighestGroundZ + FMath::Abs(HangDistance) + MinPayloadCruiseClearance;
	}
}

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
	int32 OwningPlayer = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite)
	FVector ExecuteLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite)
	FVector RetreatLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite)
	TArray<ESquadSubtype> SquadSubtypes;

	UPROPERTY(BlueprintReadWrite)
	float TankAttachZOffset = 0.0f;

	// Tank payload hangs on ropes (visual + flight behaviour): the aircraft hovers while the tank touches down instead
	// of landing; TankAttachZOffset is then the negative hang distance (see AircraftRopeSling::DefaultHangDistance).
	UPROPERTY(BlueprintReadWrite)
	bool bRopeSlungPayload = false;

	// Seconds on the ground after the drop; with a rope-slung payload the seconds the aircraft hovers after the
	// release (at least the rope winch-up time).
	UPROPERTY(BlueprintReadWrite)
	float HowLongStayLanded = 0.0f;

	UPROPERTY(BlueprintReadWrite)
	float RadiusSpawnSquads = 0.0f;

	// Squads appear at the landed aircraft's cargo door and walk out to their spot on the RadiusSpawnSquads circle.
	UPROPERTY(BlueprintReadWrite)
	bool bSquadsWalkOutOfCargoDoor = false;

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
