#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/SteamCapture/RTSSteamCaptureSettings.h"
#include "RTSSteamCaptureCameraActor.generated.h"

class UCameraComponent;
class USceneCaptureComponent2D;
class USceneComponent;
class UTextureRenderTarget2D;

/**
 * @brief Hidden capture camera used by the Steam capture subsystem during PIE.
 * It mirrors the player camera transform into a fixed render target so output aspect never depends on the viewport.
 */
UCLASS()
class RTS_SURVIVAL_API ARTSSteamCaptureCameraActor : public AActor
{
	GENERATED_BODY()

public:
	ARTSSteamCaptureCameraActor();

	bool InitCaptureCamera(UTextureRenderTarget2D* RenderTarget);
	bool SyncToPlayerCamera(
		const UCameraComponent* PlayerCameraComponent,
		const FRTSSteamCaptureCameraSettings& CameraSettings);
	bool CaptureFrame();

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> M_RootSceneComponent;

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> M_SceneCaptureComponent;

	bool GetIsValidSceneCaptureComponent() const;
};
