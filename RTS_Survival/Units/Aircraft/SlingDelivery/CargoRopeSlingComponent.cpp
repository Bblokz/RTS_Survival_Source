// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CargoRopeSlingComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UObject/ConstructorHelpers.h"

namespace CargoRopeSlingConstants
{
	// The engine cylinder is 100 cm tall and 50 cm in radius.
	constexpr float RopeMeshHeight = 100.f;
	constexpr float RopeMeshRadius = 50.f;

	constexpr int32 PayloadCornerCount = 4;
	// Main line + one leg per corner.
	constexpr int32 RopeSegmentCount = PayloadCornerCount + 1;
	constexpr int32 MainLineSegmentIndex = 0;

	// The legs grip the roof slightly inside the mesh's bounding box so they do not float beside the payload.
	constexpr float CornerInsetFraction = 0.8f;
	// The spreader ring never sits more than this fraction of the way from the payload roof up to the hook.
	constexpr float MaxSpreaderFractionOfLine = 0.5f;

	// Used when the aircraft mesh is missing; and the deepest the belly hook may hang below the aircraft origin.
	constexpr float DefaultHookDepth = 150.f;
	constexpr float MaxHookDepth = 800.f;

	constexpr float GravityCmPerSecondSquared = 980.f;
	// The sideways axis swings slower and smaller so the payload traces a lazy ellipse instead of a line.
	constexpr float SideSwayFrequencyRatio = 0.73f;
	constexpr float SideSwayAmplitudeRatio = 0.6f;
	constexpr float SideSwayPhaseRadians = 1.3f;

	constexpr float MinRopeLength = 1.f;

	const FName RopeColorParameterName = TEXT("Color");
}

UCargoRopeSlingComponent::UCargoRopeSlingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// After the owner and its movement have moved this frame, so the ropes never lag behind a fast aircraft.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	// Hard references on the class default object keep both assets in the cook.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RopeMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RopeMaterialFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (RopeMeshFinder.Succeeded())
	{
		RopeMesh = RopeMeshFinder.Object;
	}
	if (RopeMaterialFinder.Succeeded())
	{
		RopeMaterial = RopeMaterialFinder.Object;
	}
}

bool UCargoRopeSlingComponent::StartSling(const UPrimitiveComponent* AircraftMesh, ATankMaster* Payload,
                                          const float HangDistance)
{
	if (not IsValid(Payload) || not IsValid(GetOwner()))
	{
		RTSFunctionLibrary::ReportError(TEXT("UCargoRopeSlingComponent::StartSling has no payload or no owner."));
		return false;
	}
	USkeletalMeshComponent* PayloadMesh = Payload->GetTankMesh();
	if (not IsValid(PayloadMesh))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UCargoRopeSlingComponent::StartSling: the payload has no tank mesh to tie the ropes to: ")
			+ Payload->GetName());
		return false;
	}

	M_Payload = Payload;
	M_PayloadMesh = PayloadMesh;
	M_HangDistance = FMath::Max(0.f, HangDistance);
	if (not StartSling_CachePayloadCorners(PayloadMesh))
	{
		return false;
	}
	StartSling_CacheHookOffset(AircraftMesh);
	if (not StartSling_CreateRopeSegments())
	{
		return false;
	}

	M_Sway = FCargoRopeSwayState();
	M_Sway.bM_IsEnabled = true;
	M_Release = FCargoRopeReleaseState();
	AddTickPrerequisiteActor(GetOwner());
	SetComponentTickEnabled(true);
	UpdateAttachedRopes(GetHookWorldLocation());
	return true;
}

void UCargoRopeSlingComponent::SetSwayEnabled(const bool bEnabled)
{
	M_Sway.bM_IsEnabled = bEnabled;
}

void UCargoRopeSlingComponent::SnapPayloadToRest()
{
	M_Sway.bM_IsEnabled = false;
	M_Sway.Weight = 0.f;
	ApplyPayloadSwayOffset(FVector::ZeroVector);
	if (not M_Release.bM_IsReleased)
	{
		UpdateAttachedRopes(GetHookWorldLocation());
	}
}

void UCargoRopeSlingComponent::ReleasePayload()
{
	if (M_Release.bM_IsReleased)
	{
		return;
	}

	const FVector HookLocation = GetHookWorldLocation();
	M_Release = FCargoRopeReleaseState();
	M_Release.bM_IsReleased = true;
	M_Release.LegEndOffsetsFromHook.Init(FVector::ZeroVector, CargoRopeSlingConstants::PayloadCornerCount);
	if (GetHasValidPayloadMesh())
	{
		const FTransform MeshTransform = M_PayloadMesh->GetComponentTransform();
		const FVector TopCentre = MeshTransform.TransformPosition(M_PayloadTopCentreLocal);
		M_Release.RingOffsetFromHook = GetSpreaderRingLocation(HookLocation, TopCentre) - HookLocation;
		for (int32 CornerIndex = 0; CornerIndex < M_PayloadTopCornersLocal.Num()
		     && CornerIndex < CargoRopeSlingConstants::PayloadCornerCount; ++CornerIndex)
		{
			M_Release.LegEndOffsetsFromHook[CornerIndex] =
				MeshTransform.TransformPosition(M_PayloadTopCornersLocal[CornerIndex]) - HookLocation;
		}
	}
	// From here on the payload is the ground's; the sling never moves it again.
	M_Payload.Reset();
	M_PayloadMesh.Reset();
}

void UCargoRopeSlingComponent::DestroySlingImmediately()
{
	SetComponentTickEnabled(false);
	DestroyRopeSegments();
	M_Payload.Reset();
	M_PayloadMesh.Reset();
	DestroyComponent();
}

void UCargoRopeSlingComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (not IsValid(GetOwner()))
	{
		DestroySlingImmediately();
		return;
	}

	const FVector HookLocation = GetHookWorldLocation();
	if (M_Release.bM_IsReleased)
	{
		TickRetract(DeltaTime, HookLocation);
		return;
	}
	if (not GetHasValidPayloadMesh())
	{
		// The payload was destroyed in flight; there is nothing left to hold.
		DestroySlingImmediately();
		return;
	}

	TickSway(DeltaTime);
	UpdateAttachedRopes(HookLocation);
}

void UCargoRopeSlingComponent::OnComponentDestroyed(const bool bDestroyingHierarchy)
{
	DestroyRopeSegments();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

bool UCargoRopeSlingComponent::GetHasValidPayloadMesh() const
{
	return M_PayloadMesh.IsValid() && M_Payload.IsValid();
}

bool UCargoRopeSlingComponent::StartSling_CachePayloadCorners(const USkeletalMeshComponent* PayloadMesh)
{
	// The asset bounds cover the visible mesh only; actor bounds also hold invisible helpers (nav, overlap boxes).
	const USkinnedAsset* PayloadAsset = IsValid(PayloadMesh) ? PayloadMesh->GetSkinnedAsset() : nullptr;
	if (not IsValid(PayloadAsset))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UCargoRopeSlingComponent: the payload's tank mesh has no skeletal mesh asset; no ropes are shown."));
		return false;
	}

	const FBox MeshBox = PayloadAsset->GetBounds().GetBox();
	const FVector Centre = MeshBox.GetCenter();
	const FVector InsetExtent = MeshBox.GetExtent() * CargoRopeSlingConstants::CornerInsetFraction;
	const float RoofZ = MeshBox.Max.Z;
	M_PayloadTopCentreLocal = FVector(Centre.X, Centre.Y, RoofZ);
	M_PayloadTopCornersLocal.Reset(CargoRopeSlingConstants::PayloadCornerCount);
	M_PayloadTopCornersLocal.Add(FVector(Centre.X + InsetExtent.X, Centre.Y + InsetExtent.Y, RoofZ));
	M_PayloadTopCornersLocal.Add(FVector(Centre.X + InsetExtent.X, Centre.Y - InsetExtent.Y, RoofZ));
	M_PayloadTopCornersLocal.Add(FVector(Centre.X - InsetExtent.X, Centre.Y - InsetExtent.Y, RoofZ));
	M_PayloadTopCornersLocal.Add(FVector(Centre.X - InsetExtent.X, Centre.Y + InsetExtent.Y, RoofZ));
	return true;
}

void UCargoRopeSlingComponent::StartSling_CacheHookOffset(const UPrimitiveComponent* AircraftMesh)
{
	M_HookLocalOffset = FVector(0.f, 0.f, -CargoRopeSlingConstants::DefaultHookDepth);
	const AActor* Owner = GetOwner();
	if (not IsValid(AircraftMesh) || not IsValid(Owner))
	{
		return;
	}

	// Bounds in the owner's own space, so a banking aircraft does not push the hook down to a wing tip.
	const FTransform MeshToActor = AircraftMesh->GetComponentTransform().GetRelativeTransform(
		Owner->GetActorTransform());
	const FBox ActorSpaceBox = AircraftMesh->CalcBounds(MeshToActor).GetBox();
	if (not ActorSpaceBox.IsValid)
	{
		return;
	}
	const float HookZ = FMath::Clamp(ActorSpaceBox.Min.Z, -CargoRopeSlingConstants::MaxHookDepth, 0.f);
	M_HookLocalOffset = FVector(0.f, 0.f, HookZ);
}

bool UCargoRopeSlingComponent::StartSling_CreateRopeSegments()
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner) || not IsValid(RopeMesh))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this, "RopeMesh", "UCargoRopeSlingComponent::StartSling_CreateRopeSegments", this);
		return false;
	}

	DestroyRopeSegments();
	if (IsValid(RopeMaterial) && not IsValid(M_RopeMaterialInstance))
	{
		M_RopeMaterialInstance = UMaterialInstanceDynamic::Create(RopeMaterial, this);
		if (IsValid(M_RopeMaterialInstance))
		{
			M_RopeMaterialInstance->SetVectorParameterValue(CargoRopeSlingConstants::RopeColorParameterName, RopeColor);
		}
	}
	UMaterialInterface* SegmentMaterial = RopeMaterial.Get();
	if (IsValid(M_RopeMaterialInstance))
	{
		SegmentMaterial = M_RopeMaterialInstance.Get();
	}

	for (int32 SegmentIndex = 0; SegmentIndex < CargoRopeSlingConstants::RopeSegmentCount; ++SegmentIndex)
	{
		UStaticMeshComponent* RopeSegment = NewObject<UStaticMeshComponent>(Owner);
		RopeSegment->SetStaticMesh(RopeMesh);
		if (IsValid(SegmentMaterial))
		{
			RopeSegment->SetMaterial(0, SegmentMaterial);
		}
		// Pure visual: nothing collides with, selects, navigates around or is shadowed by the ropes.
		RopeSegment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RopeSegment->SetGenerateOverlapEvents(false);
		RopeSegment->SetCanEverAffectNavigation(false);
		RopeSegment->SetCastShadow(false);
		RopeSegment->SetUsingAbsoluteLocation(true);
		RopeSegment->SetUsingAbsoluteRotation(true);
		RopeSegment->SetUsingAbsoluteScale(true);
		RopeSegment->SetupAttachment(Owner->GetRootComponent());
		RopeSegment->RegisterComponent();
		M_RopeSegments.Add(RopeSegment);
	}
	return true;
}

FVector UCargoRopeSlingComponent::GetHookWorldLocation() const
{
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return FVector::ZeroVector;
	}
	return Owner->GetActorTransform().TransformPosition(M_HookLocalOffset);
}

FVector UCargoRopeSlingComponent::GetSpreaderRingLocation(const FVector& HookLocation,
                                                          const FVector& PayloadTopCentre) const
{
	const FVector RoofToHook = HookLocation - PayloadTopCentre;
	const float LineLength = RoofToHook.Size();
	if (LineLength <= KINDA_SMALL_NUMBER)
	{
		return PayloadTopCentre;
	}
	const float LegLength = FMath::Min(SpreaderLegLength,
	                                   LineLength * CargoRopeSlingConstants::MaxSpreaderFractionOfLine);
	return PayloadTopCentre + RoofToHook / LineLength * LegLength;
}

void UCargoRopeSlingComponent::TickSway(const float DeltaTime)
{
	const float TargetWeight = M_Sway.bM_IsEnabled ? 1.f : 0.f;
	M_Sway.Weight = FMath::FInterpConstantTo(M_Sway.Weight, TargetWeight, DeltaTime, 1.f / SwayFadeSeconds);
	if (M_Sway.Weight <= KINDA_SMALL_NUMBER && not M_Sway.bM_IsEnabled)
	{
		M_Sway.Weight = 0.f;
		ApplyPayloadSwayOffset(FVector::ZeroVector);
		return;
	}

	M_Sway.Time += DeltaTime;
	// A real pendulum of this length: the longer the ropes, the slower the swing.
	const float SwingAngularSpeed = FMath::Sqrt(
		CargoRopeSlingConstants::GravityCmPerSecondSquared / FMath::Max(M_HangDistance, 1.f));
	const float Amplitude = SwayAmplitude * M_Sway.Weight;
	const float ForwardOffset = Amplitude * FMath::Sin(SwingAngularSpeed * M_Sway.Time);
	const float SideOffset = Amplitude * CargoRopeSlingConstants::SideSwayAmplitudeRatio
		* FMath::Sin(SwingAngularSpeed * CargoRopeSlingConstants::SideSwayFrequencyRatio * M_Sway.Time
			+ CargoRopeSlingConstants::SideSwayPhaseRadians);
	ApplyPayloadSwayOffset(FVector(ForwardOffset, SideOffset, 0.f));
}

void UCargoRopeSlingComponent::ApplyPayloadSwayOffset(const FVector& SwayOffset) const
{
	if (not GetHasValidPayloadMesh())
	{
		return;
	}
	ATankMaster* Payload = M_Payload.Get();
	if (Payload->GetAttachParentActor() != GetOwner())
	{
		return;
	}

	const FVector RelativeLocation(SwayOffset.X, SwayOffset.Y, -M_HangDistance);
	if (SwayOffset.IsNearlyZero())
	{
		Payload->SetActorRelativeLocation(RelativeLocation);
		Payload->SetActorRelativeRotation(FRotator::ZeroRotator);
		return;
	}
	// The payload's roof keeps pointing at the pivot above it, like a load swinging on a line.
	const FVector UpTowardsPivot = FVector(-SwayOffset.X, -SwayOffset.Y, M_HangDistance).GetSafeNormal();
	const FRotator SwingRotation = FRotationMatrix::MakeFromZX(UpTowardsPivot, FVector::ForwardVector).Rotator();
	Payload->SetActorRelativeLocation(RelativeLocation);
	Payload->SetActorRelativeRotation(SwingRotation);
}

void UCargoRopeSlingComponent::UpdateAttachedRopes(const FVector& HookLocation)
{
	if (not GetHasValidPayloadMesh())
	{
		return;
	}
	const FTransform MeshTransform = M_PayloadMesh->GetComponentTransform();
	const FVector TopCentre = MeshTransform.TransformPosition(M_PayloadTopCentreLocal);
	const FVector RingLocation = GetSpreaderRingLocation(HookLocation, TopCentre);
	PlaceRopeSegment(CargoRopeSlingConstants::MainLineSegmentIndex, HookLocation, RingLocation, MainLineRadius);
	for (int32 CornerIndex = 0; CornerIndex < M_PayloadTopCornersLocal.Num(); ++CornerIndex)
	{
		const FVector CornerLocation = MeshTransform.TransformPosition(M_PayloadTopCornersLocal[CornerIndex]);
		PlaceRopeSegment(CornerIndex + 1, RingLocation, CornerLocation, LegRadius);
	}
}

void UCargoRopeSlingComponent::TickRetract(const float DeltaTime, const FVector& HookLocation)
{
	M_Release.Elapsed += DeltaTime;
	const float LinearAlpha = FMath::Clamp(M_Release.Elapsed / CargoRopeSling::RetractSeconds, 0.f, 1.f);
	if (LinearAlpha >= 1.f)
	{
		DestroySlingImmediately();
		return;
	}

	const float RemainingFraction = 1.f - FMath::SmoothStep(0.f, 1.f, LinearAlpha);
	const FVector RingLocation = HookLocation + M_Release.RingOffsetFromHook * RemainingFraction;
	PlaceRopeSegment(CargoRopeSlingConstants::MainLineSegmentIndex, HookLocation, RingLocation, MainLineRadius);
	for (int32 LegIndex = 0; LegIndex < M_Release.LegEndOffsetsFromHook.Num(); ++LegIndex)
	{
		const FVector LegEnd = HookLocation + M_Release.LegEndOffsetsFromHook[LegIndex] * RemainingFraction;
		PlaceRopeSegment(LegIndex + 1, RingLocation, LegEnd, LegRadius);
	}
}

void UCargoRopeSlingComponent::PlaceRopeSegment(const int32 SegmentIndex, const FVector& Start, const FVector& End,
                                                const float Radius) const
{
	if (not M_RopeSegments.IsValidIndex(SegmentIndex))
	{
		return;
	}
	UStaticMeshComponent* RopeSegment = M_RopeSegments[SegmentIndex];
	if (not IsValid(RopeSegment))
	{
		return;
	}

	const FVector Rope = End - Start;
	const float RopeLength = Rope.Size();
	if (RopeLength < CargoRopeSlingConstants::MinRopeLength)
	{
		RopeSegment->SetVisibility(false);
		return;
	}
	const float RadiusScale = Radius / CargoRopeSlingConstants::RopeMeshRadius;
	RopeSegment->SetVisibility(true);
	RopeSegment->SetWorldLocationAndRotation((Start + End) * 0.5f,
	                                         FRotationMatrix::MakeFromZ(Rope / RopeLength).Rotator());
	RopeSegment->SetWorldScale3D(FVector(RadiusScale, RadiusScale, RopeLength / CargoRopeSlingConstants::RopeMeshHeight));
}

void UCargoRopeSlingComponent::DestroyRopeSegments()
{
	for (UStaticMeshComponent* RopeSegment : M_RopeSegments)
	{
		if (IsValid(RopeSegment))
		{
			RopeSegment->DestroyComponent();
		}
	}
	M_RopeSegments.Empty();
}
