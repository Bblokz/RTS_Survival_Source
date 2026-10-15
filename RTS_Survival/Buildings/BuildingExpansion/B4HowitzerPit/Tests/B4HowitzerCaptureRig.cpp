#include "B4HowitzerCaptureRig.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/PointLight.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/CommandLine.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSB4Capture, Log, All);

namespace B4HowitzerCaptureRigPrivate
{
	constexpr int32 PictureWidth = 1280;
	constexpr int32 PictureHeight = 720;
	constexpr float FillLightIntensity = 60000.f;
	constexpr float FillLightRadius = 4000.f;
	constexpr float FillLightHeightAboveCamera = 150.f;
}

bool FB4HowitzerCaptureRig::Init(UWorld& World, const FString& Directory)
{
	using namespace B4HowitzerCaptureRigPrivate;
	M_Directory = Directory;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASceneCapture2D* Camera = World.SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform::Identity,
	                                                             SpawnParameters);
	UTextureRenderTarget2D* RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		&World, PictureWidth, PictureHeight, RTF_RGBA8_SRGB);
	if (not IsValid(Camera) || not IsValid(RenderTarget))
	{
		UE_LOG(LogRTSB4Capture, Warning, TEXT("RTS_B4_TEST capture rig could not be created"));
		return false;
	}
	USceneCaptureComponent2D* Capture = Camera->GetCaptureComponent2D();
	Capture->TextureTarget = RenderTarget;
	Capture->CaptureSource = SCS_FinalColorLDR;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	// The fog of war is a post-process material; the pictures show the units, not what the player may see.
	Capture->ShowFlags.SetPostProcessMaterial(false);
	// -CaptureExposureBias=<stops> brightens pictures taken under a map's low evening sun.
	float ExposureBias = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("CaptureExposureBias="), ExposureBias))
	{
		Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
		Capture->PostProcessSettings.AutoExposureBias = ExposureBias;
		Capture->PostProcessBlendWeight = 1.f;
	}
	M_Camera = Camera;
	M_RenderTarget.Reset(RenderTarget);
	APointLight* FillLight = World.SpawnActor<APointLight>(APointLight::StaticClass(), FTransform::Identity, SpawnParameters);
	if (IsValid(FillLight))
	{
		UPointLightComponent* Light = Cast<UPointLightComponent>(FillLight->GetLightComponent());
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetIntensity(FillLightIntensity);
		Light->SetAttenuationRadius(FillLightRadius);
		Light->SetCastShadows(false);
		M_FillLight = FillLight;
	}
	UE_LOG(LogRTSB4Capture, Display, TEXT("RTS_B4_TEST capture rig writing to %s"), *Directory);
	return true;
}

void FB4HowitzerCaptureRig::CaptureViews(UWorld& World, const FTransform& Frame,
                                         const TArray<FB4HowitzerCameraView>& Views, const FString& Label) const
{
	if (not GetIsEnabled())
	{
		return;
	}
	USceneCaptureComponent2D* Capture = M_Camera->GetCaptureComponent2D();
	for (const FB4HowitzerCameraView& View : Views)
	{
		const FVector CameraLocation = Frame.TransformPosition(View.Location);
		const FVector LookAtLocation = Frame.TransformPosition(View.LookAt);
		M_Camera->SetActorLocationAndRotation(CameraLocation, (LookAtLocation - CameraLocation).Rotation());
		if (M_FillLight.IsValid())
		{
			M_FillLight->SetActorLocation(CameraLocation + FVector(0.f, 0.f, B4HowitzerCaptureRigPrivate::FillLightHeightAboveCamera));
		}
		Capture->FOVAngle = View.FieldOfView;
		Capture->CaptureScene();
		const FString FileName = FString::Printf(TEXT("%s_%s.png"), *Label, *View.Name);
		UKismetRenderingLibrary::ExportRenderTarget(&World, M_RenderTarget.Get(), M_Directory, FileName);
	}
}

#endif
