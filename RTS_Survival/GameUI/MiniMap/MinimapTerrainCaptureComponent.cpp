// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MinimapTerrainCaptureComponent.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace MinimapTerrainCaptureConstants
{
	constexpr float MapExtentToWidthMultiplier = 2.0f;
	const FRotator TopDownCaptureRotation(-90.0f, 0.0f, -90.0f);
}

UMinimapTerrainCaptureComponent::UMinimapTerrainCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

UTextureRenderTarget2D* UMinimapTerrainCaptureComponent::GetMinimapTerrainRenderTarget() const
{
	if (not GetIsValidMinimapTerrainRenderTarget())
	{
		return nullptr;
	}

	return M_MinimapTerrainRenderTarget;
}

void UMinimapTerrainCaptureComponent::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_CapturePlayableArea();
}

void UMinimapTerrainCaptureComponent::BeginPlay_CapturePlayableArea()
{
	UWorld* const World = GetWorld();
	if (not IsValid(World) || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	AFowManager* const FowManager = FRTS_Statics::GetFowManager(this);
	if (not IsValid(FowManager))
	{
		RTSFunctionLibrary::ReportError(
			"Minimap terrain capture could not find the FOW manager during the first frame."
			"\n See function: UMinimapTerrainCaptureComponent::BeginPlay_CapturePlayableArea");
		return;
	}

	if (FowManager->GetMapExtent() <= 0.0f)
	{
		RTSFunctionLibrary::ReportError(
			"Minimap terrain capture requires a positive map extent on the FOW manager."
			"\n See function: UMinimapTerrainCaptureComponent::BeginPlay_CapturePlayableArea");
		return;
	}

	if (not CreateMinimapTerrainRenderTarget())
	{
		return;
	}

	CapturePlayableArea(*FowManager, *World);
}

bool UMinimapTerrainCaptureComponent::CreateMinimapTerrainRenderTarget()
{
	if (M_RenderTargetResolution <= 0)
	{
		RTSFunctionLibrary::ReportError(
			"Minimap terrain capture render target resolution must be positive."
			"\n See function: UMinimapTerrainCaptureComponent::CreateMinimapTerrainRenderTarget");
		return false;
	}

	M_MinimapTerrainRenderTarget = NewObject<UTextureRenderTarget2D>(
		this,
		TEXT("MinimapTerrainRenderTarget"),
		RF_Transient);
	if (not GetIsValidMinimapTerrainRenderTarget())
	{
		return false;
	}

	M_MinimapTerrainRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;
	M_MinimapTerrainRenderTarget->ClearColor = M_ClearColor;
	M_MinimapTerrainRenderTarget->bAutoGenerateMips = false;
	M_MinimapTerrainRenderTarget->bForceLinearGamma = true;
	M_MinimapTerrainRenderTarget->InitAutoFormat(M_RenderTargetResolution, M_RenderTargetResolution);
	M_MinimapTerrainRenderTarget->UpdateResourceImmediate(true);
	return true;
}

bool UMinimapTerrainCaptureComponent::CapturePlayableArea(const AFowManager& FowManager, UWorld& World)
{
	FVector CaptureLocation = FowManager.GetActorLocation();
	CaptureLocation.Z += M_CaptureHeight;
	const FTransform CaptureTransform(MinimapTerrainCaptureConstants::TopDownCaptureRotation, CaptureLocation);
	ASceneCapture2D* const CaptureActor = World.SpawnActorDeferred<ASceneCapture2D>(
		ASceneCapture2D::StaticClass(),
		CaptureTransform,
		GetOwner(),
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(CaptureActor))
	{
		RTSFunctionLibrary::ReportError(
			"Minimap terrain capture failed to create its temporary scene capture actor."
			"\n See function: UMinimapTerrainCaptureComponent::CapturePlayableArea");
		return false;
	}

	USceneCaptureComponent2D* const SceneCaptureComponent = CaptureActor->GetCaptureComponent2D();
	if (not IsValid(SceneCaptureComponent))
	{
		RTSFunctionLibrary::ReportError(
			"Minimap terrain capture actor did not provide a scene capture component."
			"\n See function: UMinimapTerrainCaptureComponent::CapturePlayableArea");
		CaptureActor->Destroy();
		return false;
	}

	ConfigureSceneCapture(*SceneCaptureComponent, FowManager);
	UGameplayStatics::FinishSpawningActor(CaptureActor, CaptureTransform);
	SceneCaptureComponent->CaptureScene();
	CaptureActor->Destroy();
	return true;
}

void UMinimapTerrainCaptureComponent::ConfigureSceneCapture(
	USceneCaptureComponent2D& SceneCaptureComponent,
	const AFowManager& FowManager) const
{
	SceneCaptureComponent.bCaptureEveryFrame = false;
	SceneCaptureComponent.bCaptureOnMovement = false;
	SceneCaptureComponent.bAlwaysPersistRenderingState = false;
	SceneCaptureComponent.ProjectionType = ECameraProjectionMode::Orthographic;
	SceneCaptureComponent.OrthoWidth =
		FowManager.GetMapExtent() * MinimapTerrainCaptureConstants::MapExtentToWidthMultiplier;
	SceneCaptureComponent.bAutoCalculateOrthoPlanes = true;
	SceneCaptureComponent.CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	SceneCaptureComponent.PostProcessBlendWeight = 0.0f;
	SceneCaptureComponent.ShowFlags.SetPostProcessing(false);
	SceneCaptureComponent.TextureTarget = M_MinimapTerrainRenderTarget;
}

bool UMinimapTerrainCaptureComponent::GetIsValidMinimapTerrainRenderTarget() const
{
	if (IsValid(M_MinimapTerrainRenderTarget))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_MinimapTerrainRenderTarget",
		"GetIsValidMinimapTerrainRenderTarget",
		this);
	return false;
}
