// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "ImpactVfxCaptureSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "ShaderCompiler.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSImpactVfxCapture, Log, All);

namespace ImpactVfxCaptureConstants
{
	// Niagara system paths separated by '+', e.g. /Game/A/NS_A+/Game/B/NS_B.
	const TCHAR* const SystemsSwitch = TEXT("ImpactVfxCapture=");
	const TCHAR* const DirectorySwitch = TEXT("ImpactVfxDir=");
	// The weapon's ImpactScale, applied as a uniform component scale.
	const TCHAR* const ScaleSwitch = TEXT("ImpactVfxScale=");
	// Game seconds after the spawn to take pictures at, comma separated.
	const TCHAR* const MomentsSwitch = TEXT("ImpactVfxMoments=");
	const TCHAR* const SkyFillSwitch = TEXT("ImpactVfxSkyFill=");
	const TCHAR* const ExposureBiasSwitch = TEXT("ImpactVfxExposureBias=");
	// X,Y (and optionally Z) of the impact point; the point is dropped onto the ground below it.
	const TCHAR* const LocationSwitch = TEXT("ImpactVfxAt=");
	const TCHAR* const AnchorClassPrefix = TEXT("BP_UnitImage_LandscapeAnchor");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	const TCHAR* const CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const CylinderMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

	// Real seconds: the HUD and start widget need a moment before the game can be started.
	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 60.0;
	// Game seconds.
	constexpr float SettleSeconds = 3.f;
	constexpr float MaxSettleSeconds = 120.f;
	constexpr float MinWarmUpSeconds = 2.f;
	constexpr float MaxWarmUpSeconds = 60.f;
	// Long enough for the smoke of a large impact to clear out of the next effect's pictures.
	constexpr float ClearSeconds = 8.f;
	constexpr float DefaultCaptureMoments[] = {0.03f, 0.1f, 0.2f, 0.35f, 0.6f, 1.f, 1.6f, 2.5f, 4.f, 6.f};

	constexpr int32 PictureWidth = 1280;
	constexpr int32 PictureHeight = 720;
	constexpr float GroundTraceHalfHeight = 5000.f;
	constexpr float ExposureAdaptSpeed = 10.f;

	// The game camera: spring arm 3000 at -70 degrees with a 90 degree field of view, plus a low side view that
	// shows the height of the column and a zoomed out game view.
	const FImpactVfxCaptureView Views[] = {
		{TEXT("Game"), 0.f, -70.f, 3000.f, 90.f, 0.f},
		{TEXT("GameFar"), 0.f, -70.f, 5500.f, 90.f, 0.f},
		{TEXT("Side"), 40.f, -18.f, 6000.f, 50.f, 700.f}
	};

	// Blocks for judging the size of the effect: a medium tank and a soldier.
	const FVector TankBlockOffset(900.f, -700.f, 120.f);
	const FVector TankBlockScale(6.5f, 3.2f, 2.4f);
	const FVector SoldierBlockOffset(-500.f, 450.f, 90.f);
	const FVector SoldierBlockScale(0.5f, 0.5f, 1.8f);

	bool GetIsStillCompilingOrLoading()
	{
		const bool bIsCompilingShaders = GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0;
		return bIsCompilingShaders || IsAsyncLoading();
	}
}

bool UImpactVfxCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	FString Systems;
	return FParse::Value(FCommandLine::Get(), ImpactVfxCaptureConstants::SystemsSwitch, Systems);
}

void UImpactVfxCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (not LoadSystemPaths())
	{
		Finish();
		return;
	}
	M_OutputDirectory = FPaths::ProjectSavedDir() / TEXT("ImpactVfx");
	FParse::Value(FCommandLine::Get(), ImpactVfxCaptureConstants::DirectorySwitch, M_OutputDirectory);
	FParse::Value(FCommandLine::Get(), ImpactVfxCaptureConstants::ScaleSwitch, M_EffectScale);
	LoadCaptureMoments();
	// Error dialogs are answered automatically in unattended runs, but each one stalls a frame; log them instead.
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	EnterPhase(EImpactVfxCapturePhase::WaitingForGameStart);
	UE_LOG(LogRTSImpactVfxCapture, Display, TEXT("RTS_IMPACT_VFX started: %d systems, scale %.2f, writing to %s"),
	       M_SystemPaths.Num(), M_EffectScale, *M_OutputDirectory);
}

void UImpactVfxCaptureSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

void UImpactVfxCaptureSubsystem::Tick(float DeltaTime)
{
	if (M_Phase == EImpactVfxCapturePhase::Inactive || M_Phase == EImpactVfxCapturePhase::Done)
	{
		return;
	}
	if (M_Phase != EImpactVfxCapturePhase::WaitingForGameStart)
	{
		KeepGameRunning();
		AimCameras();
	}
	switch (M_Phase)
	{
	case EImpactVfxCapturePhase::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case EImpactVfxCapturePhase::Settling:
		TickSettling();
		break;
	case EImpactVfxCapturePhase::WarmingUp:
		TickWarmingUp();
		break;
	case EImpactVfxCapturePhase::ClearingWarmUp:
		TickClearingWarmUp();
		break;
	case EImpactVfxCapturePhase::Playing:
		TickPlaying();
		break;
	case EImpactVfxCapturePhase::Clearing:
		TickClearing();
		break;
	default:
		break;
	}
}

TStatId UImpactVfxCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UImpactVfxCaptureSubsystem, STATGROUP_Tickables);
}

void UImpactVfxCaptureSubsystem::TickWaitingForGameStart()
{
	if (GetRealSecondsInPhase() > ImpactVfxCaptureConstants::MaxStartSeconds)
	{
		UE_LOG(LogRTSImpactVfxCapture, Error, TEXT("RTS_IMPACT_VFX could not start the game"));
		Finish();
		return;
	}
	if (GetRealSecondsInPhase() < ImpactVfxCaptureConstants::HudBuildSeconds || not TryStartGame())
	{
		return;
	}
	SpawnSkyFill();
	if (not FindImpactBaseLocation(M_ImpactLocation) || not CreateCameras())
	{
		Finish();
		return;
	}
	DropImpactLocationToGround();
	SpawnScaleReferences();
	EnterPhase(EImpactVfxCapturePhase::Settling);
}

void UImpactVfxCaptureSubsystem::TickSettling()
{
	using namespace ImpactVfxCaptureConstants;
	const bool bIsStillPreparing = GetIsStillCompilingOrLoading() && GetGameSecondsInPhase() < MaxSettleSeconds;
	if (GetGameSecondsInPhase() < SettleSeconds || bIsStillPreparing)
	{
		return;
	}
	ExportCameras(TEXT("empty"));
	if (not SpawnCurrentEffect())
	{
		EnterPhase(EImpactVfxCapturePhase::Clearing);
		return;
	}
	EnterPhase(EImpactVfxCapturePhase::WarmingUp);
}

void UImpactVfxCaptureSubsystem::TickWarmingUp()
{
	using namespace ImpactVfxCaptureConstants;
	const bool bIsStillPreparing = GetIsStillCompilingOrLoading() && GetGameSecondsInPhase() < MaxWarmUpSeconds;
	if (GetGameSecondsInPhase() < MinWarmUpSeconds || bIsStillPreparing)
	{
		return;
	}
	DestroyEffect();
	EnterPhase(EImpactVfxCapturePhase::ClearingWarmUp);
}

void UImpactVfxCaptureSubsystem::TickClearingWarmUp()
{
	if (GetGameSecondsInPhase() < ImpactVfxCaptureConstants::ClearSeconds)
	{
		return;
	}
	M_MaxParticlesPerEmitter.Reset();
	M_MaxTotalParticles = 0;
	M_NextMomentIndex = 0;
	if (not SpawnCurrentEffect())
	{
		EnterPhase(EImpactVfxCapturePhase::Clearing);
		return;
	}
	UE_LOG(LogRTSImpactVfxCapture, Display, TEXT("RTS_IMPACT_VFX %s playing"), *GetCurrentSystemName());
	EnterPhase(EImpactVfxCapturePhase::Playing);
}

void UImpactVfxCaptureSubsystem::TickPlaying()
{
	RecordParticleCounts();
	if (not M_CaptureMoments.IsValidIndex(M_NextMomentIndex))
	{
		LogEffectCost();
		DestroyEffect();
		EnterPhase(EImpactVfxCapturePhase::Clearing);
		return;
	}
	const float Moment = M_CaptureMoments[M_NextMomentIndex];
	if (GetGameSecondsInPhase() < Moment)
	{
		return;
	}
	ExportCameras(FString::Printf(TEXT("t%05.2f"), Moment));
	++M_NextMomentIndex;
}

void UImpactVfxCaptureSubsystem::TickClearing()
{
	if (GetGameSecondsInPhase() < ImpactVfxCaptureConstants::ClearSeconds)
	{
		return;
	}
	++M_SystemIndex;
	if (not M_SystemPaths.IsValidIndex(M_SystemIndex))
	{
		Finish();
		return;
	}
	if (not SpawnCurrentEffect())
	{
		EnterPhase(EImpactVfxCapturePhase::Clearing);
		return;
	}
	EnterPhase(EImpactVfxCapturePhase::WarmingUp);
}

void UImpactVfxCaptureSubsystem::EnterPhase(const EImpactVfxCapturePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
	M_PhaseStartGameSeconds = GetGameSeconds();
}

double UImpactVfxCaptureSubsystem::GetRealSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartRealSeconds;
}

float UImpactVfxCaptureSubsystem::GetGameSecondsInPhase() const
{
	return GetGameSeconds() - M_PhaseStartGameSeconds;
}

float UImpactVfxCaptureSubsystem::GetGameSeconds() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

bool UImpactVfxCaptureSubsystem::LoadSystemPaths()
{
	FString Systems;
	FParse::Value(FCommandLine::Get(), ImpactVfxCaptureConstants::SystemsSwitch, Systems);
	Systems.ParseIntoArray(M_SystemPaths, TEXT("+"), true);
	if (M_SystemPaths.IsEmpty())
	{
		UE_LOG(LogRTSImpactVfxCapture, Error, TEXT("RTS_IMPACT_VFX no systems given"));
		return false;
	}
	return true;
}

void UImpactVfxCaptureSubsystem::LoadCaptureMoments()
{
	using namespace ImpactVfxCaptureConstants;
	M_CaptureMoments = TArray<float>(DefaultCaptureMoments, UE_ARRAY_COUNT(DefaultCaptureMoments));
	FString MomentsText;
	// Not stopping on separators: the list itself is comma separated.
	if (not FParse::Value(FCommandLine::Get(), MomentsSwitch, MomentsText, false))
	{
		return;
	}
	TArray<FString> MomentTexts;
	MomentsText.ParseIntoArray(MomentTexts, TEXT(","), true);
	TArray<float> Moments;
	for (const FString& MomentText : MomentTexts)
	{
		Moments.Add(FCString::Atof(*MomentText));
	}
	if (Moments.IsEmpty())
	{
		return;
	}
	Moments.Sort();
	M_CaptureMoments = Moments;
}

bool UImpactVfxCaptureSubsystem::TryStartGame()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	M_PlayerController = Cast<ACPPController>(World->GetFirstPlayerController());
	if (not M_PlayerController.IsValid())
	{
		return false;
	}
	UPlayerStartGameControl* StartGameControl = M_PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	KeepGameRunning();
	UE_LOG(LogRTSImpactVfxCapture, Display, TEXT("RTS_IMPACT_VFX game started"));
	return true;
}

void UImpactVfxCaptureSubsystem::KeepGameRunning() const
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(
			Picker->WidgetTree->FindWidget(ImpactVfxCaptureConstants::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	if (M_PlayerController.IsValid() && M_PlayerController->GetIsGameOnPause())
	{
		M_PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

bool UImpactVfxCaptureSubsystem::FindImpactBaseLocation(FVector& OutLocation)
{
	using namespace ImpactVfxCaptureConstants;
	FString LocationText;
	if (FParse::Value(FCommandLine::Get(), LocationSwitch, LocationText, false))
	{
		TArray<FString> Coordinates;
		LocationText.ParseIntoArray(Coordinates, TEXT(","), true);
		if (Coordinates.Num() >= 2)
		{
			const float Height = Coordinates.IsValidIndex(2) ? FCString::Atof(*Coordinates[2]) : 0.f;
			OutLocation = FVector(FCString::Atof(*Coordinates[0]), FCString::Atof(*Coordinates[1]), Height);
			return true;
		}
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName().StartsWith(AnchorClassPrefix))
		{
			OutLocation = It->GetActorLocation();
			M_ViewYaw = It->GetActorRotation().Yaw;
			return true;
		}
	}
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		OutLocation = It->GetActorLocation();
		return true;
	}
	UE_LOG(LogRTSImpactVfxCapture, Error, TEXT("RTS_IMPACT_VFX no -ImpactVfxAt, %s or player start on this map"),
	       AnchorClassPrefix);
	return false;
}

void UImpactVfxCaptureSubsystem::DropImpactLocationToGround()
{
	const FVector BaseLocation = M_ImpactLocation;
	const FVector TraceOffset(0.f, 0.f, ImpactVfxCaptureConstants::GroundTraceHalfHeight);
	FHitResult GroundHit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ImpactVfxCaptureGround), false);
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, BaseLocation + TraceOffset, BaseLocation - TraceOffset,
	                                         ECC_Visibility, QueryParams))
	{
		M_ImpactLocation = GroundHit.ImpactPoint;
	}
	UE_LOG(LogRTSImpactVfxCapture, Display, TEXT("RTS_IMPACT_VFX impact point %s"), *M_ImpactLocation.ToCompactString());
}

void UImpactVfxCaptureSubsystem::SpawnScaleReferences() const
{
	using namespace ImpactVfxCaptureConstants;
	UStaticMesh* const CubeMesh = LoadObject<UStaticMesh>(nullptr, CubeMeshPath);
	UStaticMesh* const CylinderMesh = LoadObject<UStaticMesh>(nullptr, CylinderMeshPath);
	const TPair<UStaticMesh*, FTransform> Blocks[] = {
		{CubeMesh, FTransform(FRotator::ZeroRotator, M_ImpactLocation + TankBlockOffset, TankBlockScale)},
		{CylinderMesh, FTransform(FRotator::ZeroRotator, M_ImpactLocation + SoldierBlockOffset, SoldierBlockScale)}
	};
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const TPair<UStaticMesh*, FTransform>& Block : Blocks)
	{
		AStaticMeshActor* const BlockActor = GetWorld()->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), Block.Value, SpawnParameters);
		if (not IsValid(BlockActor) || not IsValid(Block.Key))
		{
			continue;
		}
		BlockActor->SetMobility(EComponentMobility::Movable);
		BlockActor->GetStaticMeshComponent()->SetStaticMesh(Block.Key);
		BlockActor->SetActorScale3D(Block.Value.GetScale3D());
		BlockActor->SetActorEnableCollision(false);
	}
}

void UImpactVfxCaptureSubsystem::SpawnSkyFill()
{
	float SkyFillIntensity = 0.f;
	if (not FParse::Value(FCommandLine::Get(), ImpactVfxCaptureConstants::SkyFillSwitch, SkyFillIntensity))
	{
		return;
	}
	UTextureCube* MapSkyCubemap = nullptr;
	for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
	{
		USkyLightComponent* MapSkyLight = It->GetLightComponent();
		MapSkyCubemap = MapSkyLight->Cubemap;
		// Only the newest registered sky light renders; the map's one would replace the fill a frame later.
		MapSkyLight->SetVisibility(false);
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkyLight* SkyFill = GetWorld()->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity,
	                                                        SpawnParameters);
	if (not IsValid(SkyFill))
	{
		return;
	}
	USkyLightComponent* SkyLight = SkyFill->GetLightComponent();
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SourceType = IsValid(MapSkyCubemap) ? SLS_SpecifiedCubemap : SLS_CapturedScene;
	SkyLight->SetCubemap(MapSkyCubemap);
	SkyLight->SetIntensity(SkyFillIntensity);
	SkyLight->RecaptureSky();
	M_SkyFill = SkyFill;
}

bool UImpactVfxCaptureSubsystem::CreateCameras()
{
	using namespace ImpactVfxCaptureConstants;
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FImpactVfxCaptureView& View : Views)
	{
		ASceneCapture2D* Capture = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(),
		                                                               FTransform::Identity, SpawnParameters);
		UTextureRenderTarget2D* RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
			World, PictureWidth, PictureHeight, RTF_RGBA8_SRGB);
		if (not IsValid(Capture) || not IsValid(RenderTarget))
		{
			UE_LOG(LogRTSImpactVfxCapture, Error, TEXT("RTS_IMPACT_VFX camera %s could not be created"), *View.Name);
			return false;
		}
		ConfigureCaptureComponent(*Capture->GetCaptureComponent2D(), *RenderTarget, View);
		FImpactVfxCaptureCamera& Camera = M_Cameras.AddDefaulted_GetRef();
		Camera.View = View;
		Camera.Capture = Capture;
		Camera.RenderTarget.Reset(RenderTarget);
	}
	return true;
}

void UImpactVfxCaptureSubsystem::ConfigureCaptureComponent(USceneCaptureComponent2D& CaptureComponent,
                                                           UTextureRenderTarget2D& RenderTarget,
                                                           const FImpactVfxCaptureView& View) const
{
	using namespace ImpactVfxCaptureConstants;
	CaptureComponent.TextureTarget = &RenderTarget;
	CaptureComponent.CaptureSource = SCS_FinalColorLDR;
	CaptureComponent.FOVAngle = View.FieldOfView;
	// Rendering every frame with kept state lets auto exposure and temporal AA settle like the game view.
	CaptureComponent.bCaptureEveryFrame = true;
	CaptureComponent.bCaptureOnMovement = false;
	CaptureComponent.bAlwaysPersistRenderingState = true;
	// The fog of war is a post-process material; the pictures show the effect, not what the player may see.
	CaptureComponent.ShowFlags.SetPostProcessMaterial(false);
	FPostProcessSettings& Settings = CaptureComponent.PostProcessSettings;
	Settings.bOverride_AutoExposureSpeedUp = true;
	Settings.AutoExposureSpeedUp = ExposureAdaptSpeed;
	Settings.bOverride_AutoExposureSpeedDown = true;
	Settings.AutoExposureSpeedDown = ExposureAdaptSpeed;
	float ExposureBias = 0.f;
	if (FParse::Value(FCommandLine::Get(), ExposureBiasSwitch, ExposureBias))
	{
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = ExposureBias;
	}
	CaptureComponent.PostProcessBlendWeight = 1.f;
}

void UImpactVfxCaptureSubsystem::AimCameras() const
{
	for (const FImpactVfxCaptureCamera& Camera : M_Cameras)
	{
		if (not Camera.Capture.IsValid())
		{
			continue;
		}
		const FImpactVfxCaptureView& View = Camera.View;
		const FVector LookAt = M_ImpactLocation + FVector(0.f, 0.f, View.LookAtHeight);
		const FRotator CameraRotation(View.Pitch, M_ViewYaw + View.YawFromImpact, 0.f);
		const FVector CameraLocation = LookAt - CameraRotation.Vector() * View.Distance;
		Camera.Capture->SetActorLocationAndRotation(CameraLocation, CameraRotation);
	}
}

bool UImpactVfxCaptureSubsystem::SpawnCurrentEffect()
{
	if (not M_SystemPaths.IsValidIndex(M_SystemIndex))
	{
		return false;
	}
	UNiagaraSystem* const System = LoadObject<UNiagaraSystem>(nullptr, *M_SystemPaths[M_SystemIndex]);
	if (not IsValid(System))
	{
		UE_LOG(LogRTSImpactVfxCapture, Error, TEXT("RTS_IMPACT_VFX could not load %s"), *M_SystemPaths[M_SystemIndex]);
		return false;
	}
	M_EffectComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(), System, M_ImpactLocation, FRotator::ZeroRotator, FVector(M_EffectScale), false, true,
		ENCPoolMethod::None, false);
	return M_EffectComponent.IsValid();
}

void UImpactVfxCaptureSubsystem::RecordParticleCounts()
{
	if (not M_EffectComponent.IsValid())
	{
		return;
	}
	const FNiagaraSystemInstanceControllerConstPtr Controller = M_EffectComponent->GetSystemInstanceController();
	if (not Controller.IsValid())
	{
		return;
	}
	const FNiagaraSystemInstance* const Instance = Controller->GetSystemInstance_Unsafe();
	if (Instance == nullptr)
	{
		return;
	}
	int32 TotalParticles = 0;
	for (const FNiagaraEmitterInstanceRef& Emitter : Instance->GetEmitters())
	{
		const int32 EmitterParticles = Emitter->GetNumParticles();
		TotalParticles += EmitterParticles;
		int32& MaxParticles = M_MaxParticlesPerEmitter.FindOrAdd(Emitter->GetEmitterHandle().GetName());
		MaxParticles = FMath::Max(MaxParticles, EmitterParticles);
	}
	M_MaxTotalParticles = FMath::Max(M_MaxTotalParticles, TotalParticles);
}

void UImpactVfxCaptureSubsystem::LogEffectCost() const
{
	FString EmitterCounts;
	for (const TPair<FName, int32>& EmitterMax : M_MaxParticlesPerEmitter)
	{
		EmitterCounts += FString::Printf(TEXT(" %s=%d"), *EmitterMax.Key.ToString(), EmitterMax.Value);
	}
	UE_LOG(LogRTSImpactVfxCapture, Display, TEXT("RTS_IMPACT_VFX %s max particles %d:%s"), *GetCurrentSystemName(),
	       M_MaxTotalParticles, *EmitterCounts);
}

void UImpactVfxCaptureSubsystem::DestroyEffect()
{
	if (M_EffectComponent.IsValid())
	{
		M_EffectComponent->DestroyComponent();
	}
	M_EffectComponent.Reset();
}

void UImpactVfxCaptureSubsystem::ExportCameras(const FString& MomentLabel) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const FString SystemName = GetCurrentSystemName();
	const FString SystemDirectory = M_OutputDirectory / SystemName;
	for (const FImpactVfxCaptureCamera& Camera : M_Cameras)
	{
		if (not Camera.RenderTarget.IsValid())
		{
			continue;
		}
		const FString FileName = FString::Printf(TEXT("%s_%s_%s.png"), *SystemName, *Camera.View.Name, *MomentLabel);
		UKismetRenderingLibrary::ExportRenderTarget(World, Camera.RenderTarget.Get(), SystemDirectory, FileName);
	}
}

FString UImpactVfxCaptureSubsystem::GetCurrentSystemName() const
{
	if (not M_SystemPaths.IsValidIndex(M_SystemIndex))
	{
		return TEXT("none");
	}
	return FPackageName::GetShortName(M_SystemPaths[M_SystemIndex]);
}

void UImpactVfxCaptureSubsystem::Finish()
{
	DestroyEffect();
	M_Phase = EImpactVfxCapturePhase::Done;
	UE_LOG(LogRTSImpactVfxCapture, Display, TEXT("RTS_IMPACT_VFX done"));
	FPlatformMisc::RequestExit(false, TEXT("ImpactVfxCapture"));
}

#else

bool UImpactVfxCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UImpactVfxCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UImpactVfxCaptureSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UImpactVfxCaptureSubsystem::Tick(float DeltaTime)
{
}

TStatId UImpactVfxCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UImpactVfxCaptureSubsystem, STATGROUP_Tickables);
}

#endif
