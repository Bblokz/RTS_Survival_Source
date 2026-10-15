#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/StrongObjectPtr.h"

class APointLight;
class ASceneCapture2D;
class UWorld;

/** One camera placement, given in the space of the frame passed to FB4HowitzerCaptureRig::CaptureViews. */
struct FB4HowitzerCameraView
{
	FString Name;
	FVector Location = FVector::ZeroVector;
	FVector LookAt = FVector::ZeroVector;
	float FieldOfView = 60.f;
};

/**
 * @brief Saves pictures of the B-4 test from scene-capture cameras, so an offscreen run (no window, nothing on the
 * user's screen) still shows how the deploy and the pit loading drill look. Pictures go to the folder given with
 * -B4HowitzerCapture=<folder>. Not in shipping.
 */
struct FB4HowitzerCaptureRig
{
	bool Init(UWorld& World, const FString& Directory);
	bool GetIsEnabled() const { return M_Camera.IsValid() && M_RenderTarget.IsValid(); }

	/**
	 * @brief Takes one picture per view; views are relative to Frame so cameras follow a turning gun.
	 * @param World World the camera lives in.
	 * @param Frame Space the views are given in (gun socket, team weapon actor).
	 * @param Views Cameras to shoot from.
	 * @param Label Prefix of the file names, e.g. the time in the drill.
	 */
	void CaptureViews(UWorld& World, const FTransform& Frame, const TArray<FB4HowitzerCameraView>& Views,
	                  const FString& Label) const;

private:
	TWeakObjectPtr<ASceneCapture2D> M_Camera;
	// Rides with the camera: the map's low sun leaves the side facing the camera in deep shadow.
	TWeakObjectPtr<APointLight> M_FillLight;
	TStrongObjectPtr<UTextureRenderTarget2D> M_RenderTarget;
	FString M_Directory;
};
