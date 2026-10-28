// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "RTSShortsCookOffState.generated.h"

class UStaticMeshComponent;

/** @brief Tracks one cook-off segment: the cook-off start, the death, the thrown turret and what the death spawned. */
USTRUCT()
struct FRTSShortsCookOffState
{
	GENERATED_BODY()

	// The wreck's turret mesh while the toss component simulates it; it has landed once physics is off again.
	UPROPERTY()
	TWeakObjectPtr<UStaticMeshComponent> TossedTurret;

	// Every actor spawned after the death (wreck, debris): removed at the segment's end so the next tank stands alone.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SpawnedActors;

	// Niagara components active when the segment began: every other active one is stopped at its end.
	TSet<uint32> EffectIdsAtBegin;

	// The framing's aim point at the segment start, and the current aim: it eases toward the flying turret so the
	// wreck and the turret both stay in the frame, then settles between them once the turret lies still.
	FVector BaseLookAt = FVector::ZeroVector;
	FVector CameraLookAt = FVector::ZeroVector;

	FDelegateHandle ActorSpawnedHandle;
	int32 DeathLocalFrame = -1;
	int32 LandedLocalFrame = -1;
	float MaxTurretHeightCm = 0.f;
	bool bIsActive = false;
	bool bCookOffStarted = false;
	bool bTurretFlying = false;
	bool bSegmentShortened = false;
};
