// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "RTSShortsAircraftCrashState.generated.h"

class AAircraftMaster;

/** @brief Tracks the one aircraft of a running AircraftCrash segment from its run through the kill to the wreck. */
USTRUCT()
struct FRTSShortsAircraftCrashState
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AAircraftMaster> Aircraft;

	// The run's heading: the chase and crash cameras are placed in this frame, so the spiral never rolls them.
	FRotator RunRotation = FRotator::ZeroRotator;

	FVector CrashCameraLocation = FVector::ZeroVector;

	// Smoothed aim of the crash camera; it lags the falling aircraft and settles on the impact point.
	FVector CrashCameraLookAt = FVector::ZeroVector;

	// Niagara components active when the segment began: every other active one is stopped at its end.
	TSet<uint32> EffectIdsAtBegin;

	FVector ImpactLocation = FVector::ZeroVector;
	int32 ImpactLocalFrame = -1;
	bool bIsActive = false;
	bool bKilled = false;
	bool bCrashViewCut = false;
	bool bSegmentShortened = false;
};
