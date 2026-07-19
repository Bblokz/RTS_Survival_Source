#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/SteamCapture/RTSSteamCaptureSettings.h"
#include "RTSSteamCaptureCameraActor.generated.h"

class USceneCaptureComponent2D;
class USceneComponent;
class UTextureRenderTarget2D;
struct FMinimalViewInfo;

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

	/**
	 * @brief Mirrors the player's final camera view while retaining capture-specific framing controls.
	 * @param PlayerCameraView Final camera-manager view used by the player's viewport.
	 * @param CameraSettings Capture-only transform and field-of-view adjustments.
	 * @return True when the capture camera was synchronized successfully.
	 */
	bool SyncToPlayerCamera(
		const FMinimalViewInfo& PlayerCameraView,
		const FRTSSteamCaptureCameraSettings& CameraSettings);
	bool CaptureFrame();

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> M_RootSceneComponent;

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> M_SceneCaptureComponent;

	bool GetIsValidSceneCaptureComponent() const;
};
