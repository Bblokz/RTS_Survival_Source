// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "RTSShowcaseCamera.h"

#include "CineCameraComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponentBase.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"

namespace RTSShowcaseCameraConstants
{
	// Sensor sizes in mm; only their ratio matters for the picture, the width keeps focal lengths full-frame like.
	constexpr float LandscapeSensorWidthMm = 36.f;
	constexpr float PortraitSensorWidthMm = 18.f;
	constexpr float Aspect16x9 = 16.f / 9.f;
	constexpr float AspectVertical1x2 = 0.5f;
	constexpr float AspectPanorama2x1 = 2.f;
	constexpr int32 MinOutputEdgePixels = 16;
}

ARTSShowcaseCamera::ARTSShowcaseCamera(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

void ARTSShowcaseCamera::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyFilmbackForFormat();
}

FIntPoint ARTSShowcaseCamera::GetOutputResolution(const float ResolutionScale) const
{
	using namespace RTSShowcaseCameraConstants;
	const float LongEdge = FMath::Max(1.f, LongEdgePixels * ResolutionScale);
	float Aspect = Aspect16x9;
	switch (ShotFormat)
	{
	case ERTSShowcaseShotFormat::Vertical1x2:
		Aspect = AspectVertical1x2;
		break;
	case ERTSShowcaseShotFormat::Panorama2x1:
		Aspect = AspectPanorama2x1;
		break;
	default:
		break;
	}
	const bool bIsPortrait = Aspect < 1.f;
	const float ShortEdge = bIsPortrait ? LongEdge * Aspect : LongEdge / Aspect;
	// Even sizes keep video encoders and contact-sheet tools happy.
	const int32 LongPixels = FMath::Max(MinOutputEdgePixels, FMath::RoundToInt(LongEdge / 2.f) * 2);
	const int32 ShortPixels = FMath::Max(MinOutputEdgePixels, FMath::RoundToInt(ShortEdge / 2.f) * 2);
	return bIsPortrait ? FIntPoint(ShortPixels, LongPixels) : FIntPoint(LongPixels, ShortPixels);
}

FString ARTSShowcaseCamera::GetShotId() const
{
	const FString Shot = ShotName.IsEmpty() ? GetName() : ShotName;
	return StageName.IsNone() ? Shot : StageName.ToString() + TEXT("__") + Shot;
}

void ARTSShowcaseCamera::ActivateStageLighting()
{
	FRTSShowcaseStageLighting::ActivateStage(GetWorld(), StageName, true);
}

void ARTSShowcaseCamera::ApplyFilmbackForFormat()
{
	using namespace RTSShowcaseCameraConstants;
	UCineCameraComponent* const CineCamera = GetCineCameraComponent();
	if (not IsValid(CineCamera))
	{
		return;
	}
	const FIntPoint Resolution = GetOutputResolution(1.f);
	const float Aspect = static_cast<float>(Resolution.X) / static_cast<float>(Resolution.Y);
	FCameraFilmbackSettings Filmback = CineCamera->Filmback;
	Filmback.SensorWidth = Aspect < 1.f ? PortraitSensorWidthMm : LandscapeSensorWidthMm;
	Filmback.SensorHeight = Filmback.SensorWidth / Aspect;
	CineCamera->SetFilmback(Filmback);
	CineCamera->SetConstraintAspectRatio(true);
}

int32 FRTSShowcaseStageLighting::ActivateStage(UWorld* World, const FName StageName, const bool bMarkModified)
{
	if (not IsValid(World))
	{
		return 0;
	}
	const FString Prefix = GetTagPrefix();
	const FName ActiveTag = StageName.IsNone() ? NAME_None : FName(Prefix + StageName.ToString());
	int32 ActiveActorCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* const Actor = *It;
		const bool bIsStageActor = Actor->Tags.ContainsByPredicate([&Prefix](const FName& Tag)
		{
			return Tag.ToString().StartsWith(Prefix);
		});
		if (not bIsStageActor)
		{
			continue;
		}
		const bool bEnabled = not ActiveTag.IsNone() && Actor->Tags.Contains(ActiveTag);
		ActiveActorCount += bEnabled ? 1 : 0;
		SetActorLightingEnabled(*Actor, bEnabled, bMarkModified);
	}
	return ActiveActorCount;
}

void FRTSShowcaseStageLighting::SetActorLightingEnabled(AActor& Actor, const bool bEnabled, const bool bMarkModified)
{
	if (APostProcessVolume* const Volume = Cast<APostProcessVolume>(&Actor))
	{
		if (bMarkModified)
		{
			Volume->Modify();
		}
		Volume->bEnabled = bEnabled;
		return;
	}
	TInlineComponentArray<USceneComponent*> SceneComponents(&Actor);
	for (USceneComponent* const Component : SceneComponents)
	{
		const bool bIsLightingComponent = Component->IsA<ULightComponentBase>()
			|| Component->IsA<USkyAtmosphereComponent>()
			|| Component->IsA<UExponentialHeightFogComponent>()
			|| Component->IsA<UVolumetricCloudComponent>();
		if (not bIsLightingComponent)
		{
			continue;
		}
		if (bMarkModified)
		{
			Component->Modify();
		}
		Component->SetVisibility(bEnabled);
	}
}
