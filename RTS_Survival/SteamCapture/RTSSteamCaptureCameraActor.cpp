#include "RTSSteamCaptureCameraActor.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace RTSSteamCaptureCameraConstants
{
	inline constexpr float MinFovDegrees = 5.0f;
	inline constexpr float MaxFovDegrees = 170.0f;
}

ARTSSteamCaptureCameraActor::ARTSSteamCaptureCameraActor()
	: M_RootSceneComponent(nullptr)
	  , M_SceneCaptureComponent(nullptr)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	M_RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SteamCaptureRoot"));
	SetRootComponent(M_RootSceneComponent);

	M_SceneCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SteamCaptureSceneCapture"));
	M_SceneCaptureComponent->SetupAttachment(M_RootSceneComponent);
	M_SceneCaptureComponent->bCaptureEveryFrame = false;
	M_SceneCaptureComponent->bCaptureOnMovement = false;
	M_SceneCaptureComponent->bAlwaysPersistRenderingState = true;
	M_SceneCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
}

bool ARTSSteamCaptureCameraActor::InitCaptureCamera(UTextureRenderTarget2D* RenderTarget)
{
	if (not GetIsValidSceneCaptureComponent())
	{
		return false;
	}
	if (not IsValid(RenderTarget))
	{
		RTSFunctionLibrary::ReportError(TEXT("Steam capture render target was invalid."));
		return false;
	}

	M_SceneCaptureComponent->TextureTarget = RenderTarget;
	return true;
}

bool ARTSSteamCaptureCameraActor::SyncToPlayerCamera(
	const UCameraComponent* PlayerCameraComponent,
	const FRTSSteamCaptureCameraSettings& CameraSettings)
{
	if (not GetIsValidSceneCaptureComponent())
	{
		return false;
	}
	if (not IsValid(PlayerCameraComponent))
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot sync Steam capture camera because player camera is invalid."));
		return false;
	}

	const FTransform PlayerCameraTransform = PlayerCameraComponent->GetComponentTransform();
	const FVector WorldOffset = PlayerCameraTransform.TransformVectorNoScale(CameraSettings.M_LocalLocationOffset);
	const FQuat CaptureRotation = PlayerCameraTransform.GetRotation() * CameraSettings.M_RotationOffset.Quaternion();
	SetActorLocationAndRotation(PlayerCameraTransform.GetLocation() + WorldOffset, CaptureRotation);

	const float BaseFovDegrees = CameraSettings.bM_MatchPlayerCameraFov
		                             ? PlayerCameraComponent->FieldOfView
		                             : CameraSettings.M_CaptureFovDegrees;
	M_SceneCaptureComponent->FOVAngle = FMath::Clamp(
		BaseFovDegrees * CameraSettings.M_FovMultiplier,
		RTSSteamCaptureCameraConstants::MinFovDegrees,
		RTSSteamCaptureCameraConstants::MaxFovDegrees);

	M_SceneCaptureComponent->ProjectionType = PlayerCameraComponent->ProjectionMode;
	M_SceneCaptureComponent->OrthoWidth = PlayerCameraComponent->OrthoWidth;
	return true;
}

bool ARTSSteamCaptureCameraActor::CaptureFrame()
{
	if (not GetIsValidSceneCaptureComponent())
	{
		return false;
	}

	M_SceneCaptureComponent->CaptureScene();
	return true;
}

bool ARTSSteamCaptureCameraActor::GetIsValidSceneCaptureComponent() const
{
	if (IsValid(M_SceneCaptureComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_SceneCaptureComponent"),
		TEXT("GetIsValidSceneCaptureComponent"),
		this);
	return false;
}
