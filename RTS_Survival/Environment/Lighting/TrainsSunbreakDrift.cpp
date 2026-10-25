// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TrainsSunbreakDrift.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace TrainsSunbreakDrift
{
	const FName CloudDriftScaleParameter(TEXT("CloudDriftScale"));
	const FName RayDetailDriftParameter(TEXT("RayDetailDrift"));
	const FName CloudOffsetParameter(TEXT("CloudDriftOffset"));
	const FName BoundaryFadeParameter(TEXT("CloudBoundaryFade"));
	constexpr float ReferenceDetailDrift = 4.f;
	constexpr float GroundTraceTop = 15000.f;
	constexpr float GroundTraceBottom = -10000.f;
	constexpr float GroundHeightInterpolationSpeed = 12.f;
}

ATrainsSunbreakDrift::ATrainsSunbreakDrift()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("CloudDriftRoot")));
}

void ATrainsSunbreakDrift::BeginPlay()
{
	Super::BeginPlay();
	ResetSunbreakDrift();
}

void ATrainsSunbreakDrift::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AdvanceSunbreakDrift(DeltaSeconds);
}

bool ATrainsSunbreakDrift::ShouldTickIfViewportsOnly() const
{
	return true;
}

void ATrainsSunbreakDrift::ConfigureSunbreakDrift(
	UMaterialParameterCollection* SunbreakControls, const TArray<AActor*>& RayActors)
{
	M_SunbreakControls = SunbreakControls;
	if (not GetIsValidSunbreakControls())
	{
		return;
	}
	M_RayBindings.Reset();
	M_RayBindings.Reserve(RayActors.Num());
	for (AActor* RayActor : RayActors)
	{
		if (not IsValid(RayActor))
		{
			continue;
		}
		FTrainsSunbreakRayBinding RayBinding;
		RayBinding.RayActor = RayActor;
		RayBinding.AnchorLocation = RayActor->GetActorLocation();
		RayBinding.GroundHeight = RayBinding.AnchorLocation.Z;
		M_RayBindings.Add(RayBinding);
	}
	ResetSunbreakDrift();
}

void ATrainsSunbreakDrift::AdvanceSunbreakDrift(const float DeltaSeconds)
{
	if (DeltaSeconds <= 0.f || not FMath::IsFinite(DeltaSeconds) || not GetIsValidSunbreakControls())
	{
		return;
	}
	M_CloudOffset += GetCloudVelocity() * DeltaSeconds;
	PublishCloudOffset();
	M_GroundSampleElapsedSeconds += DeltaSeconds;
	const bool bSampleGround = M_GroundSampleElapsedSeconds >= M_Settings.GroundSampleInterval;
	if (bSampleGround)
	{
		M_GroundSampleElapsedSeconds = 0.f;
	}
	UpdateRayBindings(DeltaSeconds, bSampleGround);
}

void ATrainsSunbreakDrift::ResetSunbreakDrift()
{
	if (not GetIsValidSunbreakControls())
	{
		return;
	}
	M_CloudOffset = FVector2D::ZeroVector;
	M_GroundSampleElapsedSeconds = 0.f;
	PublishCloudOffset();
	UpdateRayBindings(0.f, true);
}

bool ATrainsSunbreakDrift::GetIsValidSunbreakControls() const
{
	if (M_SunbreakControls.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, TEXT("M_SunbreakControls"), TEXT("GetIsValidSunbreakControls"), this);
	return false;
}

bool ATrainsSunbreakDrift::GetIsValidRayActor(const FTrainsSunbreakRayBinding& RayBinding) const
{
	if (RayBinding.RayActor.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, TEXT("M_RayBindings.RayActor"), TEXT("GetIsValidRayActor"), this);
	return false;
}

FVector2D ATrainsSunbreakDrift::GetCloudVelocity() const
{
	if (not GetIsValidSunbreakControls())
	{
		return FVector2D::ZeroVector;
	}
	const float DriftScale = UKismetMaterialLibrary::GetScalarParameterValue(
		GetWorld(), M_SunbreakControls.Get(), TrainsSunbreakDrift::CloudDriftScaleParameter);
	const float DetailDrift = UKismetMaterialLibrary::GetScalarParameterValue(
		GetWorld(), M_SunbreakControls.Get(), TrainsSunbreakDrift::RayDetailDriftParameter);
	const float Speed = M_Settings.SpeedPerDriftScale * DriftScale * DetailDrift
		/ TrainsSunbreakDrift::ReferenceDetailDrift;
	return FMath::IsFinite(Speed) ? M_Settings.WindDirection.GetSafeNormal() * Speed : FVector2D::ZeroVector;
}

void ATrainsSunbreakDrift::PublishCloudOffset() const
{
	if (not GetIsValidSunbreakControls())
	{
		return;
	}
	UKismetMaterialLibrary::SetVectorParameterValue(
		GetWorld(), M_SunbreakControls.Get(), TrainsSunbreakDrift::CloudOffsetParameter,
		FLinearColor(M_CloudOffset.X, M_CloudOffset.Y, 0.f, 0.f));
}

void ATrainsSunbreakDrift::UpdateRayBindings(const float DeltaSeconds, const bool bSampleGround)
{
	for (FTrainsSunbreakRayBinding& RayBinding : M_RayBindings)
	{
		UpdateRayBinding(RayBinding, DeltaSeconds, bSampleGround);
	}
}

void ATrainsSunbreakDrift::UpdateRayBinding(
	FTrainsSunbreakRayBinding& RayBinding, const float DeltaSeconds, const bool bSampleGround)
{
	if (not GetIsValidRayActor(RayBinding))
	{
		return;
	}
	FVector RayLocation = RayBinding.AnchorLocation;
	RayLocation.X = WrapCoordinate(RayLocation.X + M_CloudOffset.X, M_Settings.MinimumBounds.X, M_Settings.MaximumBounds.X);
	RayLocation.Y = WrapCoordinate(RayLocation.Y + M_CloudOffset.Y, M_Settings.MinimumBounds.Y, M_Settings.MaximumBounds.Y);
	if (bSampleGround)
	{
		SampleRayGround(RayBinding, RayLocation);
	}
	AActor* const RayActor = RayBinding.RayActor.Get();
	RayLocation.Z = DeltaSeconds > 0.f
		? FMath::FInterpTo(RayActor->GetActorLocation().Z, RayBinding.GroundHeight, DeltaSeconds,
			TrainsSunbreakDrift::GroundHeightInterpolationSpeed)
		: RayBinding.GroundHeight;
	RayActor->SetActorLocation(RayLocation, false);
	ApplyRayFade(RayBinding, RayLocation);
}

void ATrainsSunbreakDrift::SampleRayGround(
	FTrainsSunbreakRayBinding& RayBinding, const FVector& RayLocation) const
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		RayBinding.bHasGround = false;
		return;
	}
	const FVector TraceStart(RayLocation.X, RayLocation.Y, TrainsSunbreakDrift::GroundTraceTop);
	const FVector TraceEnd(RayLocation.X, RayLocation.Y, TrainsSunbreakDrift::GroundTraceBottom);
	FHitResult GroundHit;
	RayBinding.bHasGround = World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_GameTraceChannel2)
		&& IsValid(Cast<ALandscapeProxy>(GroundHit.GetActor()));
	if (not RayBinding.bHasGround)
	{
		return;
	}
	RayBinding.GroundHeight = GroundHit.ImpactPoint.Z - M_Settings.GroundSinkDistance;
}

void ATrainsSunbreakDrift::ApplyRayFade(
	const FTrainsSunbreakRayBinding& RayBinding, const FVector& RayLocation) const
{
	if (not GetIsValidRayActor(RayBinding))
	{
		return;
	}
	UStaticMeshComponent* const RayPlane = RayBinding.RayActor->FindComponentByClass<UStaticMeshComponent>();
	if (not IsValid(RayPlane))
	{
		return;
	}
	UMaterialInstanceDynamic* const RayMaterial = Cast<UMaterialInstanceDynamic>(RayPlane->GetMaterial(0));
	if (not IsValid(RayMaterial))
	{
		return;
	}
	const float BoundaryFade = RayBinding.bHasGround ? GetBoundaryFade(RayLocation) : 0.f;
	RayMaterial->SetScalarParameterValue(TrainsSunbreakDrift::BoundaryFadeParameter, BoundaryFade);
}

float ATrainsSunbreakDrift::GetBoundaryFade(const FVector& RayLocation) const
{
	const double DistanceToBoundary = FMath::Min(
		FMath::Min(RayLocation.X - M_Settings.MinimumBounds.X, M_Settings.MaximumBounds.X - RayLocation.X),
		FMath::Min(RayLocation.Y - M_Settings.MinimumBounds.Y, M_Settings.MaximumBounds.Y - RayLocation.Y));
	const float NormalisedDistance = FMath::Clamp(
		static_cast<float>(DistanceToBoundary) / FMath::Max(M_Settings.BoundaryFadeDistance, 1.f), 0.f, 1.f);
	return FMath::SmoothStep(0.f, 1.f, NormalisedDistance);
}

float ATrainsSunbreakDrift::WrapCoordinate(const double Coordinate, const double Minimum, const double Maximum)
{
	const double Span = Maximum - Minimum;
	if (Span <= UE_DOUBLE_SMALL_NUMBER)
	{
		return Minimum;
	}
	double WrappedDistance = FMath::Fmod(Coordinate - Minimum, Span);
	if (WrappedDistance < 0.0)
	{
		WrappedDistance += Span;
	}
	return Minimum + WrappedDistance;
}
