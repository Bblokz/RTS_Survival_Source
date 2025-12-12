// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#include "RTSActorSpline.h"
#include "Components/SplineComponent.h"
#include "Components/ChildActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

ARTSActorSpline::ARTSActorSpline()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ActorSpline = CreateDefaultSubobject<USplineComponent>(TEXT("ActorSpline"));
	RootComponent = ActorSpline;

	if (ActorSpline)
	{
		ActorSpline->SetMobility(EComponentMobility::Movable);
		ActorSpline->bAllowDiscontinuousSpline = true;
		ActorSpline->CastShadow = false;
		ActorSpline->bCastDynamicShadow = false;
	}
}

void ARTSActorSpline::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITOR
	Editor_ApplyDefaultYawToNewPoints();
	DestroyEditorPreview();
	if (bSpawnPreviewInEditor)
	{
		BuildEditorPreview();
	}
#endif
}


void ARTSActorSpline::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_CleanupEditorPreview();
	BeginPlay_SpawnActorsFromSpline();
}

// ------------------------------------------------------------
// Validators (rule 0.5 - centralize error reporting)
// ------------------------------------------------------------

bool ARTSActorSpline::GetIsValidActorSpline() const
{
	if (IsValid(ActorSpline))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("ActorSpline"),
	                                                      TEXT("GetIsValidActorSpline"), this);
	return false;
}

bool ARTSActorSpline::GetIsValidActorClassToPlace() const
{
	if (ActorClassToPlace)
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("RTSActorSpline -> ActorClassToPlace is not set."));
	return false;
}

// ------------------------------------------------------------
// Transform helper
// ------------------------------------------------------------

FTransform ARTSActorSpline::MakeSpawnTransformAtPoint(const int32 SplinePointIndex) const
{
	// World-space transform from the spline point.
	const FVector LocationWS = ActorSpline->GetLocationAtSplinePoint(SplinePointIndex, ESplineCoordinateSpace::World);
	const FRotator RotationWS = ActorSpline->GetRotationAtSplinePoint(SplinePointIndex, ESplineCoordinateSpace::World)
		+ AdditionalRotationOffset;

	const FVector ScaleWS = bUseSplinePointScale
		                        ? ActorSpline->GetScaleAtSplinePoint(SplinePointIndex)
		                        : SpawnScaleOverride;

	return FTransform(RotationWS, LocationWS, ScaleWS);
}

// ------------------------------------------------------------
// Editor preview (ChildActorComponents) - editor only
// ------------------------------------------------------------

void ARTSActorSpline::BuildEditorPreview()
{
#if WITH_EDITOR
	if (not GetIsValidActorSpline())
	{
		return;
	}
	if (not GetIsValidActorClassToPlace())
	{
		return;
	}

	const int32 NumPoints = ActorSpline->GetNumberOfSplinePoints();
	if (NumPoints <= 0)
	{
		return;
	}

	M_PreviewChildActors.Reserve(NumPoints);

	for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
	{
		UChildActorComponent* PreviewComp = NewObject<UChildActorComponent>(this);
		if (not IsValid(PreviewComp))
		{
			continue;
		}

		PreviewComp->SetChildActorClass(ActorClassToPlace);
		PreviewComp->RegisterComponent();

		// Attach to root to keep transforms simple.
		PreviewComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

		const FTransform SpawnXform = MakeSpawnTransformAtPoint(PointIdx);
		PreviewComp->SetWorldLocationAndRotation(SpawnXform.GetLocation(), SpawnXform.GetRotation().Rotator());
		PreviewComp->SetWorldScale3D(SpawnXform.GetScale3D());

		M_PreviewChildActors.Add(PreviewComp);
	}
#endif
}

void ARTSActorSpline::DestroyEditorPreview()
{
#if WITH_EDITOR
	for (UChildActorComponent* Comp : M_PreviewChildActors)
	{
		if (IsValid(Comp))
		{
			Comp->DestroyComponent();
		}
	}
	M_PreviewChildActors.Empty();
#endif
}

// ------------------------------------------------------------
// Runtime spawning
// ------------------------------------------------------------

void ARTSActorSpline::ClearRuntimeActors()
{
	// We keep weak refs so destroyed actors won't crash; just clear our bookkeeping.
	M_SpawnedActors.Empty();
}

void ARTSActorSpline::BuildRuntimeActors()
{
	if (not GetIsValidActorSpline())
	{
		return;
	}
	if (not GetIsValidActorClassToPlace())
	{
		return;
	}

	const UWorld* WorldConst = GetWorld();
	if (WorldConst == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("RTSActorSpline -> No valid world to spawn actors."));
		return;
	}
	UWorld* World = const_cast<UWorld*>(WorldConst);

	const int32 NumPoints = ActorSpline->GetNumberOfSplinePoints();
	if (NumPoints <= 0)
	{
		return;
	}

	M_SpawnedActors.Reserve(NumPoints);

	for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
	{
		const FTransform SpawnXform = MakeSpawnTransformAtPoint(PointIdx);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AActor* NewActor = World->SpawnActor<AActor>(ActorClassToPlace, SpawnXform, SpawnParams);
		if (not IsValid(NewActor))
		{
			RTSFunctionLibrary::ReportError(TEXT("RTSActorSpline -> Failed to spawn actor at spline point."));
			continue;
		}

		if (bAttachSpawnedActorsToOwner)
		{
			NewActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		}

		// Track with weak references so actor destruction at runtime never breaks this owner.
		M_SpawnedActors.Add(TWeakObjectPtr<AActor>(NewActor));
	}
}

// ------------------------------------------------------------
// BeginPlay split
// ------------------------------------------------------------

void ARTSActorSpline::BeginPlay_CleanupEditorPreview()
{
#if WITH_EDITOR
	DestroyEditorPreview();
#endif
	ClearRuntimeActors();
}

void ARTSActorSpline::BeginPlay_SpawnActorsFromSpline()
{
	BuildRuntimeActors();
}

#if WITH_EDITOR
void ARTSActorSpline::Editor_ApplyDefaultYawToNewPoints()
{
	if (not GetIsValidActorSpline())
	{
		return;
	}

	const int32 NumPoints = ActorSpline->GetNumberOfSplinePoints();

	// First-ever pass after load: initialize and do NOT modify existing points.
	if (M_LastSplinePointCount < 0)
	{
		M_LastSplinePointCount = NumPoints;
		return;
	}

	// If points were removed, just sync the counter and do nothing.
	if (NumPoints <= M_LastSplinePointCount)
	{
		M_LastSplinePointCount = NumPoints;
		return;
	}

	// Apply the default yaw (Z-rotation in degrees) ONLY to the newly added points.
	for (int32 PointIdx = M_LastSplinePointCount; PointIdx < NumPoints; ++PointIdx)
	{
		const FRotator CurrentLocal =
			ActorSpline->GetRotationAtSplinePoint(PointIdx, ESplineCoordinateSpace::Local);

		FRotator TargetLocal = CurrentLocal;
		TargetLocal.Yaw = DefaultNewPointYawDegrees;

		ActorSpline->SetRotationAtSplinePoint(
			PointIdx, TargetLocal, ESplineCoordinateSpace::Local, true /* bUpdateSpline */
		);
	}

	M_LastSplinePointCount = NumPoints;
}
#endif
// ------------------------------------------------------------
// Editor bake-and-delete
// ------------------------------------------------------------

#if WITH_EDITOR
void ARTSActorSpline::Editor_ConvertSplineToPlacedActors()
{
	// Bake in the editor only.
	if (not GetIsValidActorSpline())
	{
		return;
	}
	if (not GetIsValidActorClassToPlace())
	{
		return;
	}

	DestroyEditorPreview();

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("RTSActorSpline -> Editor bake failed: World is null."));
		return;
	}

	const int32 NumPoints = ActorSpline->GetNumberOfSplinePoints();
	if (NumPoints <= 0)
	{
		// Nothing to do; just delete this spline actor.
		Destroy();
		return;
	}

	// Place independent level actors at all points with the same transforms.
	for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
	{
		const FTransform SpawnXform = MakeSpawnTransformAtPoint(PointIdx);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = nullptr; // ownership not needed for baked actors
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AActor* NewActor = World->SpawnActor<AActor>(ActorClassToPlace, SpawnXform, SpawnParams);
		if (not IsValid(NewActor))
		{
			RTSFunctionLibrary::ReportError(TEXT("RTSActorSpline -> Editor bake failed to spawn an actor."));
		}
	}

	// Remove any runtime bookkeeping and delete ourselves so only placed actors remain.
	M_SpawnedActors.Empty();
	Destroy();
}
#endif
