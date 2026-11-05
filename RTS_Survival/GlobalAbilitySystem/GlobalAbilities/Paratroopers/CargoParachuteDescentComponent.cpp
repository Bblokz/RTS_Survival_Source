// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CargoParachuteDescentComponent.h"

#include "Components/SkinnedMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

namespace CargoParachuteDescentConstants
{
	constexpr float MinDescentSeconds = 1.0f;
	// Canopy radius of the soldier parachute (soldier space); the cluster spreads its canopies by this.
	constexpr float SoldierCanopyHeight = 742.f;
	// A pivot near the canopy centre gives a slow, heavy swing.
	constexpr float PivotCanopyFraction = 0.6f;
	constexpr int32 ClusterCanopyCount = 3;
}

UCargoParachuteDescentComponent::UCargoParachuteDescentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCargoParachuteDescentComponent::StartDescent(
	const FCargoParachuteDescentSettings& Settings,
	UStaticMesh* FallbackCanopyMesh,
	const FVector& StartLocation,
	const FTransform& LandingTransform)
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		OnDescentFinished.ExecuteIfBound(nullptr, false);
		return;
	}

	M_Settings = Settings;
	M_State.StartLocation = StartLocation;
	M_State.LandingTransform = LandingTransform;
	M_State.ElapsedSeconds = 0.0f;
	M_State.DurationSeconds = FMath::Max(
		CargoParachuteDescentConstants::MinDescentSeconds,
		FVector::Dist(StartLocation, LandingTransform.GetLocation()) / FMath::Max(1.0f, Settings.DescentSpeed));
	M_State.SwayAxis = FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector();
	M_State.SwayPhaseRadians = FMath::FRandRange(0.0f, UE_TWO_PI);
	M_State.bM_IsDescending = true;

	// Measure the load standing as it will land, before it is moved into the air.
	Owner->SetActorRotation(LandingTransform.GetRotation(), ETeleportType::TeleportPhysics);
	const float LoadTopHeight = GetLoadTopHeight();
	UStaticMesh* CanopyMesh = IsValid(Settings.CanopyMesh) ? Settings.CanopyMesh.Get() : FallbackCanopyMesh;
	if (Settings.CanopyCount > 0)
	{
		AttachCanopies(CanopyMesh, LoadTopHeight);
	}
	const float CanopyCentreHeight = LoadTopHeight + Settings.LineGapAboveLoad +
		(CargoParachuteDescentConstants::SoldierCanopyHeight - Settings.CanopyLineMeetHeight) * Settings.CanopyScale *
		CargoParachuteDescentConstants::PivotCanopyFraction;
	M_State.SwayPivotHeight = Settings.SwayPivotHeight > 0.0f ? Settings.SwayPivotHeight : CanopyCentreHeight;

	const FTransform StartTransform = GetSwayedTransform(StartLocation);
	Owner->SetActorLocationAndRotation(
		StartTransform.GetLocation(), StartTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	SilenceOwnerTurrets();
	SetComponentTickEnabled(true);
}

void UCargoParachuteDescentComponent::SetLandingTrackComponent(USceneComponent* TrackComponent)
{
	M_LandingTrackComponent = TrackComponent;
	M_State.bM_TracksLanding = IsValid(TrackComponent);
	UpdateTrackedLanding();
}

float UCargoParachuteDescentComponent::GetDescentAlpha() const
{
	if (not M_State.bM_IsDescending)
	{
		return 1.0f;
	}
	return FMath::Clamp(M_State.ElapsedSeconds / M_State.DurationSeconds, 0.0f, 1.0f);
}

void UCargoParachuteDescentComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (not M_State.bM_IsDescending)
	{
		return;
	}
	AActor* Owner = GetOwner();
	if (not IsValid(Owner) || Owner->IsActorBeingDestroyed() || not Owner->IsUnitAlive())
	{
		FinishDescent(false);
		return;
	}
	if (M_State.bM_TracksLanding && not M_LandingTrackComponent.IsValid())
	{
		FinishDescent(false);
		return;
	}
	TickDescent(DeltaTime);
}

void UCargoParachuteDescentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (M_State.bM_IsDescending)
	{
		if (EndPlayReason != EEndPlayReason::Destroyed)
		{
			OnDescentFinished.Unbind();
		}
		FinishDescent(false, false);
	}
	Super::EndPlay(EndPlayReason);
}

float UCargoParachuteDescentComponent::GetLoadTopHeight() const
{
	const AActor* Owner = GetOwner();
	TArray<AActor*> Parts;
	Owner->GetAttachedActors(Parts, true, true);
	Parts.Add(const_cast<AActor*>(Owner));
	// Only what is drawn counts: collision boxes, decals and health bar widgets reach well above some loads.
	FBox Bounds(ForceInit);
	for (const AActor* Part : Parts)
	{
		TArray<UMeshComponent*> Meshes;
		Part->GetComponents<UMeshComponent>(Meshes);
		for (const UMeshComponent* Mesh : Meshes)
		{
			const bool bIsDrawnMesh = Mesh->IsA<UStaticMeshComponent>() || Mesh->IsA<USkinnedMeshComponent>();
			if (bIsDrawnMesh && Mesh->IsRegistered() && Mesh->GetVisibleFlag() && not Mesh->bHiddenInGame)
			{
				Bounds += Mesh->Bounds.GetBox();
			}
		}
	}
	if (not Bounds.IsValid)
	{
		return 0.0f;
	}
	return FMath::Max(0.0f, Bounds.Max.Z - Owner->GetActorLocation().Z);
}

void UCargoParachuteDescentComponent::AttachCanopies(UStaticMesh* CanopyMesh, const float LoadTopHeight)
{
	if (not IsValid(CanopyMesh))
	{
		RTSFunctionLibrary::ReportError(TEXT("UCargoParachuteDescentComponent has no canopy mesh configured."));
		return;
	}
	const FVector AttachPoint(0.0f, 0.0f, LoadTopHeight + M_Settings.LineGapAboveLoad);
	if (M_Settings.CanopyCount < CargoParachuteDescentConstants::ClusterCanopyCount)
	{
		AttachCanopy(CanopyMesh, AttachPoint, FVector::ZeroVector);
		return;
	}
	for (int32 Index = 0; Index < CargoParachuteDescentConstants::ClusterCanopyCount; ++Index)
	{
		const float Angle = UE_TWO_PI * Index / CargoParachuteDescentConstants::ClusterCanopyCount;
		AttachCanopy(CanopyMesh, AttachPoint, FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f));
	}
}

void UCargoParachuteDescentComponent::AttachCanopy(
	UStaticMesh* CanopyMesh,
	const FVector& AttachPoint,
	const FVector& LeanDirection)
{
	AActor* Owner = GetOwner();
	USceneComponent* Root = Owner->GetRootComponent();
	if (not IsValid(Root))
	{
		return;
	}
	// Leans the canopy about the point where its lines meet, so all canopies of a cluster hang from one point.
	FQuat Lean = FQuat::Identity;
	if (not LeanDirection.IsNearlyZero())
	{
		const FVector LeanAxis = FVector::CrossProduct(FVector::UpVector, LeanDirection).GetSafeNormal();
		Lean = FQuat(LeanAxis, FMath::DegreesToRadians(M_Settings.ClusterTiltDegrees));
	}
	const FVector LineMeetPoint(0.0f, 0.0f, M_Settings.CanopyLineMeetHeight * M_Settings.CanopyScale);
	UStaticMeshComponent* Canopy = NewObject<UStaticMeshComponent>(Owner);
	Canopy->SetStaticMesh(CanopyMesh);
	Canopy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Canopy->SetCanEverAffectNavigation(false);
	Canopy->SetCastShadow(true);
	// Root space is the load's local space; the canopy must stay level when the load lands on a slope.
	Canopy->SetupAttachment(Root);
	Canopy->SetUsingAbsoluteScale(true);
	Canopy->SetRelativeScale3D(FVector(M_Settings.CanopyScale));
	Canopy->SetRelativeRotation(Lean);
	// Relative locations are scaled by the root; the attach point is meant in world centimetres.
	const FVector RootScale = Root->GetComponentScale();
	const FVector SafeRootScale(FMath::Max(RootScale.X, KINDA_SMALL_NUMBER), FMath::Max(RootScale.Y, KINDA_SMALL_NUMBER),
	                            FMath::Max(RootScale.Z, KINDA_SMALL_NUMBER));
	Canopy->SetRelativeLocation((AttachPoint - Lean.RotateVector(LineMeetPoint)) / SafeRootScale);
	Canopy->RegisterComponent();
	M_CanopyComponents.Add(Canopy);
}

void UCargoParachuteDescentComponent::UpdateTrackedLanding()
{
	const USceneComponent* TrackComponent = M_LandingTrackComponent.Get();
	if (not IsValid(TrackComponent))
	{
		return;
	}
	const FRotator TrackRotation(0.0f, TrackComponent->GetComponentRotation().Yaw, 0.0f);
	M_State.LandingTransform = FTransform(TrackRotation, TrackComponent->GetComponentLocation(),
	                                      M_State.LandingTransform.GetScale3D());
}

void UCargoParachuteDescentComponent::TickDescent(const float DeltaTime)
{
	UpdateTrackedLanding();
	M_State.ElapsedSeconds += DeltaTime;
	const float Alpha = FMath::Clamp(M_State.ElapsedSeconds / M_State.DurationSeconds, 0.0f, 1.0f);
	if (Alpha >= 1.0f)
	{
		FinishDescent(true);
		return;
	}
	const FVector PathLocation = FMath::Lerp(M_State.StartLocation, M_State.LandingTransform.GetLocation(), Alpha);
	const FTransform SwayedTransform = GetSwayedTransform(PathLocation);
	GetOwner()->SetActorLocationAndRotation(
		SwayedTransform.GetLocation(), SwayedTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	// The load's own state machine may arm its weapons while it hangs in the air (a team weapon finishes deploying).
	SilenceOwnerTurrets();
}

FTransform UCargoParachuteDescentComponent::GetSwayedTransform(const FVector& PathLocation) const
{
	const float RemainingSeconds = M_State.DurationSeconds - M_State.ElapsedSeconds;
	const float SettleFade = M_Settings.SwaySettleSeconds > KINDA_SMALL_NUMBER
		                         ? FMath::Clamp(RemainingSeconds / M_Settings.SwaySettleSeconds, 0.0f, 1.0f)
		                         : 1.0f;
	const float SwayCycle = UE_TWO_PI * M_State.ElapsedSeconds / M_Settings.SwayPeriodSeconds;
	const float SwayDegrees = M_Settings.SwayAmplitudeDegrees * SettleFade *
		FMath::Sin(SwayCycle + M_State.SwayPhaseRadians);
	const FQuat SwayRotation(M_State.SwayAxis, FMath::DegreesToRadians(SwayDegrees));
	const FVector PivotOffset(0.0f, 0.0f, M_State.SwayPivotHeight);
	const FVector SwayedLocation = PathLocation + PivotOffset - SwayRotation.RotateVector(PivotOffset);
	const FQuat LandingYaw = FRotator(0.0f, M_State.LandingTransform.Rotator().Yaw, 0.0f).Quaternion();
	return FTransform(SwayRotation * LandingYaw, SwayedLocation);
}

void UCargoParachuteDescentComponent::SilenceOwnerTurrets() const
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}
	TArray<AActor*> Loads;
	Owner->GetAttachedActors(Loads, true, true);
	Loads.Add(Owner);
	for (AActor* Load : Loads)
	{
		if (ACPPTurretsMaster* Turret = Cast<ACPPTurretsMaster>(Load))
		{
			Turret->DisableTurret();
		}
	}
}

void UCargoParachuteDescentComponent::FinishDescent(const bool bLanded, const bool bDestroySelf)
{
	if (not M_State.bM_IsDescending)
	{
		return;
	}
	M_State.bM_IsDescending = false;
	SetComponentTickEnabled(false);
	RemoveCanopies();
	AActor* Owner = GetOwner();
	if (bLanded && IsValid(Owner))
	{
		UpdateTrackedLanding();
		Owner->SetActorLocationAndRotation(
			M_State.LandingTransform.GetLocation(), M_State.LandingTransform.GetRotation(), false, nullptr,
			ETeleportType::TeleportPhysics);
	}
	// Copy first: the listener may destroy this component or its owner.
	const FOnCargoDescentFinished FinishedDelegate = OnDescentFinished;
	OnDescentFinished.Unbind();
	FinishedDelegate.ExecuteIfBound(Owner, bLanded);
	if (bDestroySelf && IsValid(this) && not IsBeingDestroyed())
	{
		DestroyComponent();
	}
}

void UCargoParachuteDescentComponent::RemoveCanopies()
{
	for (UStaticMeshComponent* Canopy : M_CanopyComponents)
	{
		if (IsValid(Canopy))
		{
			Canopy->DestroyComponent();
		}
	}
	M_CanopyComponents.Reset();
}
