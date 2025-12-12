// Copyright (C) Bas Blokzijl - All rights reserved.


#include "RTSNavCollisionEnvObstacle.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"


URTSNavCollisionEnvObstacle::URTSNavCollisionEnvObstacle()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URTSNavCollisionEnvObstacle::SetUpNavModifierVolumeAsUnionOfComponents(
	TArray<USceneComponent*> ComponentsInSceneWithBounds, USceneComponent* RootComponent)
{
	// ---- Validate inputs early
	if (ComponentsInSceneWithBounds.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("EnvObstacle: No components provided to build union bounds."));
		return;
	}
	if (not IsValid(RootComponent))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("EnvObstacle: Invalid RootComponent passed to SetUpNavModifierVolumeAsUnionOfComponents."));
		return;
	}
	AActor* const OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("OwnerActor"),
		                                                      TEXT("SetUpNavModifierVolumeAsUnionOfComponents"));
		return;
	}

	// ---- Build world-space AABB union of all provided components
	FBox WorldUnion(ForceInit);
	for (USceneComponent* const SceneComp : ComponentsInSceneWithBounds)
	{
		if (not IsValid(SceneComp))
		{
			continue;
		}

		FBox CompWorldBox(ForceInit);

		// Prefer UPrimitiveComponent bounds if available (fast + robust when registered)
		if (const UPrimitiveComponent* const Prim = Cast<UPrimitiveComponent>(SceneComp))
		{
			// UPrimitiveComponent::Bounds is world-space; GetBox() yields world AABB.
			CompWorldBox = Prim->Bounds.GetBox(); // center/origin+extents in world
		}
		else
		{
			// Generic scene component: compute bounds on its ComponentToWorld.
			// CalcBounds returns FBoxSphereBounds (Origin + BoxExtent) -> convert to FBox.
			const FTransform& CompToWorld = SceneComp->GetComponentTransform();
			const FBoxSphereBounds B = SceneComp->CalcBounds(CompToWorld);
			CompWorldBox = FBox::BuildAABB(B.Origin, B.BoxExtent);
		}

		if (CompWorldBox.IsValid)
		{
			WorldUnion += CompWorldBox;
		}
	}

	if (!WorldUnion.IsValid)
	{
		RTSFunctionLibrary::ReportError(
			TEXT(
				"EnvObstacle: Failed to compute a valid union from provided components; keeping default nav modifier bounds."));
		return;
	}

	// ---- Store overall world bounds (used by nav dirtying paths) and a rotated local box the way NavModifier expects.
	// NOTE: UNavModifierComponent packs a Box in a rotation-only local frame and supplies the rotation separately.
	// (See UNavModifierComponent::CalculateBounds & GetNavigationData)  -> FAreaNavModifier(Box, FTransform(Quat), AreaClass).
	// We mirror that here.
	const FQuat RootQuat = RootComponent->GetComponentQuat();
	const FVector WorldCenter = WorldUnion.GetCenter();
	const FVector WorldExtent = WorldUnion.GetExtent();

	// Save for dirty-area bookkeeping (same member UNavModifierComponent writes in CalculateBounds)
	Bounds = WorldUnion;

	// Convert center into the rotation-only local frame the engine uses for ComponentBounds:
	// local = R^-1 * world (no translation removed here; this matches engine code path).
	const FVector LocalCenterInRotFrame = FTransform(RootQuat).InverseTransformPosition(WorldCenter);

	// Prepare the single rotated box that represents the union.
	const FBox LocalRotFrameBox = FBox::BuildAABB(LocalCenterInRotFrame, WorldExtent);

	ComponentBounds.Reset(1);
	ComponentBounds.Add(FRotatedBox(LocalRotFrameBox, RootQuat));

	// Also improve fallback in case engine ever ignores ComponentBounds (e.g., editor edge-cases)
	FailsafeExtent = WorldExtent;

	// Push changes to nav mesh systems. This just flags modifiers dirty; the nav sys will recalc as needed.
	RefreshNavigationModifiers();
}

void URTSNavCollisionEnvObstacle::BeginPlay()
{
	Super::BeginPlay();
}

void URTSNavCollisionEnvObstacle::ScaleCurrentExtent(const float Scale)
{
	if (Scale <= 0.f)
	{
		return;
	}
	FVector NewExtent = FailsafeExtent * Scale;
	FailsafeExtent = NewExtent;

	// Push changes to nav mesh systems. This just flags modifiers dirty; the nav sys will recalc as needed.
	RefreshNavigationModifiers();
}
