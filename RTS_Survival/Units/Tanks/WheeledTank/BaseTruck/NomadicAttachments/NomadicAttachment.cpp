// Copyright (C) Bas Blokzijl - All rights reserved.

#include "NomadicAttachment.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSOptimizer.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

// Sets default values
ANomadicAttachment::ANomadicAttachment()
{
	// Allow ticking but keep it disabled until we start rotating.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	OptimizationComponent = CreateDefaultSubobject<URTSOptimizer>(TEXT("OptimizationComponent"));
}

void ANomadicAttachment::BeginPlay()
{
	Super::BeginPlay();
}

void ANomadicAttachment::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopStaticRotation();
	Super::EndPlay(EndPlayReason);
}

void ANomadicAttachment::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bM_UseStaticRotation)
	{
		StaticMeshRotation(DeltaSeconds);
	}
}

void ANomadicAttachment::StaticMeshRotation(const float DeltaSeconds)
{
	// Always validate before touching the mesh
	if (!EnsureStaticRotationMeshIsValid())
	{
		StopStaticRotation();
		return;
	}

	// Handle waiting window
	if (M_bWaiting)
	{
		M_TimeUntilNextRotation -= DeltaSeconds;
		if (M_TimeUntilNextRotation > 0.f)
		{
			return; // keep waiting
		}

		// Done waiting: start a new rotation iteration
		M_bWaiting = false;
		BeginNextRotation();
	}

	// If nothing to rotate, enter a new waiting window
	if (FMath::IsNearlyZero(M_RemainingDeltaDeg, 0.001f))
	{
		M_bWaiting = true;
		M_TimeUntilNextRotation = FMath::FRandRange(M_StaticRotationSettings.MinWaitBetweenRotations,
		                                            M_StaticRotationSettings.MaxWaitBetweenRotations);
		return;
	}

	// Rotate toward the remaining delta at the configured speed
	const float MaxStep = M_StaticRotationSettings.RotationSpeedDegPerSec * DeltaSeconds;
	const float StepMag = FMath::Min(MaxStep, FMath::Abs(M_RemainingDeltaDeg));
	const float StepSigned = StepMag * FMath::Sign(M_RemainingDeltaDeg);

	ApplyAxisDelta(StepSigned);
	M_RemainingDeltaDeg -= StepSigned;
}

void ANomadicAttachment::StartStaticRotatationOn(UStaticMeshComponent* MeshCompToRotate,
                                                 FStaticRotationMeshSettings RotationSettings)
{
	// Store mesh + settings
	M_StaticRotatingMesh = MeshCompToRotate;
	M_StaticRotationSettings = RotationSettings;

	// Basic validation & normalization
	if (!M_StaticRotationSettings.RotationDeltasDeg.Num())
	{
		RTSFunctionLibrary::ReportError(TEXT("NomadicAttachment: RotationDeltasDeg is empty."));
		return;
	}

	// Ensure wait range is sane
	if (M_StaticRotationSettings.MaxWaitBetweenRotations < M_StaticRotationSettings.MinWaitBetweenRotations)
	{
		Swap(M_StaticRotationSettings.MinWaitBetweenRotations, M_StaticRotationSettings.MaxWaitBetweenRotations);
	}
	M_StaticRotationSettings.MinWaitBetweenRotations =
		FMath::Max(0.f, M_StaticRotationSettings.MinWaitBetweenRotations);
	// NOTE: Correct the previous line to use the right struct:
	M_StaticRotationSettings.MinWaitBetweenRotations =
		FMath::Max(0.f, M_StaticRotationSettings.MinWaitBetweenRotations);
	M_StaticRotationSettings.MaxWaitBetweenRotations = FMath::Max(M_StaticRotationSettings.MinWaitBetweenRotations,
	                                                              M_StaticRotationSettings.MaxWaitBetweenRotations);

	// Reset state
	M_TimeUntilNextRotation = FMath::FRandRange(M_StaticRotationSettings.MinWaitBetweenRotations,
	                                            M_StaticRotationSettings.MaxWaitBetweenRotations);
	M_RemainingDeltaDeg = 0.f;
	M_bWaiting = true;

	// Enable ticking now that we need it
	SetActorTickEnabled(true);
}

void ANomadicAttachment::StopStaticRotation()
{
	M_bWaiting = false;
	M_TimeUntilNextRotation = 0.f;
	M_RemainingDeltaDeg = 0.f;
	// Do not disable Tick here; skeletal subclass may be using it.
}

bool ANomadicAttachment::EnsureStaticRotationMeshIsValid() const
{
	if (!M_StaticRotatingMesh.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("No valid rotation mesh setup for Nomadic Attachment: ") + GetName());
		return false;
	}
	return true;
}

void ANomadicAttachment::BeginNextRotation()
{
	// Safety: if settings are invalid, bail to wait mode
	if (M_StaticRotationSettings.RotationDeltasDeg.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("NomadicAttachment: No RotationDeltasDeg to pick from."));
		M_RemainingDeltaDeg = 0.f;
		M_bWaiting = true;
		M_TimeUntilNextRotation = FMath::FRandRange(M_StaticRotationSettings.MinWaitBetweenRotations,
		                                            M_StaticRotationSettings.MaxWaitBetweenRotations);
		return;
	}

	// Pick one delta from the array (uniform random)
	const int32 Index = FMath::RandRange(0, M_StaticRotationSettings.RotationDeltasDeg.Num() - 1);
	const float PickedDelta = M_StaticRotationSettings.RotationDeltasDeg[Index];

	// Use the picked value as the signed remaining delta for this iteration
	M_RemainingDeltaDeg = PickedDelta;
}

void ANomadicAttachment::ApplyAxisDelta(float DeltaDegrees)
{
	if (!EnsureStaticRotationMeshIsValid())
	{
		return;
	}

	UStaticMeshComponent* Mesh = M_StaticRotatingMesh.Get();
	FRotator R = Mesh->GetRelativeRotation();

	switch (M_StaticRotationSettings.Axis)
	{
	case ENomadicStaticAxis::X:
		R.Roll += DeltaDegrees;
		break;
	case ENomadicStaticAxis::Y:
		R.Pitch += DeltaDegrees;
		break;
	case ENomadicStaticAxis::Z:
	default:
		R.Yaw += DeltaDegrees;
		break;
	}

	Mesh->SetRelativeRotation(R, false, nullptr, ETeleportType::None);
}
