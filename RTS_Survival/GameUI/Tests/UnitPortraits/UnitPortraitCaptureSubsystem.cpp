// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "UnitPortraitCaptureSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "AIController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Button.h"
#include "Components/SkyLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UObject/UObjectGlobals.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/HullWeaponComponent/HullWeaponComponent.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSUnitPortrait, Log, All);

namespace UnitPortraitConstants
{
	const TCHAR* const ListSwitch = TEXT("UnitPortraitCapture=");
	const TCHAR* const DirectorySwitch = TEXT("UnitPortraitDir=");
	const TCHAR* const ShotsSwitch = TEXT("UnitPortraitShots=");
	// Game seconds after a shot to take pictures at, comma separated, e.g. 0,0.034,0.067 for every frame at 30 fps
	// (short muzzle flashes peak between the default moments).
	const TCHAR* const ShotDelaysSwitch = TEXT("UnitPortraitShotDelays=");
	const TCHAR* const ExposureBiasSwitch = TEXT("UnitPortraitExposureBias=");
	// Scales sky and bounce light (post-process Indirect Lighting Intensity) to lift the shadows.
	const TCHAR* const IndirectLightSwitch = TEXT("UnitPortraitIndirectLight=");
	// Adds a movable sky light with the map's own sky cubemap; Lumen and the stationary sky light leave every
	// shadow black in scene captures. The captures then use the classic sky light path instead of Lumen.
	const TCHAR* const SkyFillSwitch = TEXT("UnitPortraitSkyFill=");
	const TCHAR* const TargetYawSwitch = TEXT("UnitPortraitTargetYaw=");
	const TCHAR* const FramingSwitch = TEXT("UnitPortraitFraming=");
	const TCHAR* const UnitYawSwitch = TEXT("UnitPortraitUnitYaw=");
	// Adds a copy of the first view that keeps the post-process materials, to compare against the game view's look.
	const TCHAR* const DebugPostProcessSwitch = TEXT("UnitPortraitDebugPostProcess");
	// Also takes a high-res screenshot of the real game view at each idle picture, as a reference for the lighting.
	const TCHAR* const DebugViewportShotSwitch = TEXT("UnitPortraitDebugViewportShot");
	// Adds a steep camera like the in-game RTS view, which mostly sees the deck and turret roof.
	const TCHAR* const RtsViewSwitch = TEXT("UnitPortraitRtsView");
	// Manual exposure (the exposure bias still applies) so every picture of a comparison is lit the same.
	const TCHAR* const FixedExposureSwitch = TEXT("UnitPortraitFixedExposure");
	// Optional list entry fields after the class path, separated by '|'.
	const TCHAR* const EntryOptionSeparator = TEXT("|");
	const TCHAR* const SwapOption = TEXT("Swap=");
	const TCHAR* const SwapTargetSeparator = TEXT(">");
	const TCHAR* const TeamColorOption = TEXT("TeamColor=");
	// Custom primitive data 0 holds the faction flag atlas index (FRTS_FactionFlags); team colour uses 4-7 (RGBA).
	constexpr int32 TeamColorPrimitiveDataIndex = 4;
	constexpr int32 MinTeamColorComponents = 3;
	constexpr int32 AlphaComponentIndex = 3;
	const TCHAR* const AnchorClassPrefix = TEXT("BP_UnitImage_LandscapeAnchor");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));

	// Real seconds: the HUD and start widget need a moment before the game can be started.
	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 60.0;
	// Game seconds.
	constexpr float SettleSeconds = 3.f;
	constexpr float ShotTimeoutSeconds = 25.f;
	// Smoke of the last unit's shots drifts through the next unit's idle picture without this.
	constexpr float ClearSeconds = 6.f;
	// Muzzle flash, then blast and tracer, then the smoke cloud.
	constexpr float CaptureDelaysAfterShot[] = {0.f, 0.06f, 0.14f, 0.3f, 0.6f};

	constexpr int32 PictureSize = 1536;
	constexpr float FieldOfView = 30.f;
	// Distance multiplier on the unit's bounding sphere; the crop step tightens the frame with the logged crop box.
	constexpr float DefaultFraming = 1.25f;
	// Some vehicles are larger than their mesh bounds say (skeletal bounds come from the physics asset).
	constexpr float CropBoxMarginFraction = 0.06f;
	// The first unit also waits for shaders and loads, which a fresh Development run still compiles.
	constexpr float MaxSettleSeconds = 120.f;
	constexpr float GroundTraceHalfHeight = 5000.f;
	constexpr float SpawnHeightAboveGround = 30.f;
	constexpr float TargetRangeFraction = 0.5f;
	constexpr float MaxTargetRangeFraction = 0.9f;
	constexpr float MinTargetDistance = 1500.f;
	constexpr int32 DefaultShotsPerUnit = 3;
	// The vehicle faces the anchor's yaw plus this. Compared 0/90/180/270 on the tutorial map's anchor: at 90 the
	// sun's cast shadow falls under the vehicle and the hull reads best from these views.
	constexpr float DefaultUnitYawFromAnchor = 90.f;
	constexpr float MaxUnitDrift = 30.f;
	// Meshes bigger than this, or this far from the unit, are not part of its look (helper or hidden meshes) and
	// would zoom the camera far out. Large enough for the captured B-4 pit (20 m footprint).
	constexpr float MaxUnitMeshRadius = 2500.f;

	// Auto exposure adapts in a few frames instead of seconds, so the first unit is not pictured over-exposed.
	constexpr float ExposureAdaptSpeed = 10.f;
	constexpr float LumenSurfaceCacheResolution = 1.f;

	// Mostly from the unit's right so the gun points right, as in the hand-made portraits.
	const FUnitPortraitView Views[] = {
		{TEXT("FrontRight"), 50.f, -45.f},
		{TEXT("Right"), 95.f, -48.f},
		{TEXT("Front"), 20.f, -45.f}
	};
	const FUnitPortraitView RtsView = {TEXT("RtsCamera"), 70.f, -62.f};

	/**
	 * Scene captures turn Lumen off (SceneCaptureRendering.cpp); in this project Lumen also carries the sky light, so
	 * without it every shadow is black. Turning it back on makes the pictures lit like the game view.
	 */
	void ApplyGameViewLighting(FPostProcessSettings& Settings, const bool bUseLumen)
	{
		Settings.bOverride_DynamicGlobalIlluminationMethod = true;
		Settings.DynamicGlobalIlluminationMethod = bUseLumen
			                                           ? EDynamicGlobalIlluminationMethod::Lumen
			                                           : EDynamicGlobalIlluminationMethod::None;
		Settings.bOverride_ReflectionMethod = true;
		Settings.ReflectionMethod = bUseLumen ? EReflectionMethod::Lumen : EReflectionMethod::None;
		Settings.bOverride_LumenSurfaceCacheResolution = true;
		Settings.LumenSurfaceCacheResolution = LumenSurfaceCacheResolution;
		Settings.bOverride_AutoExposureSpeedUp = true;
		Settings.AutoExposureSpeedUp = ExposureAdaptSpeed;
		Settings.bOverride_AutoExposureSpeedDown = true;
		Settings.AutoExposureSpeedDown = ExposureAdaptSpeed;
	}

	bool GetIsStillCompilingOrLoading()
	{
		const bool bIsCompilingShaders = GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0;
		return bIsCompilingShaders || IsAsyncLoading();
	}
}

bool UUnitPortraitCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	FString ListFile;
	return FParse::Value(FCommandLine::Get(), UnitPortraitConstants::ListSwitch, ListFile);
}

void UUnitPortraitCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (not LoadEntries())
	{
		Finish();
		return;
	}
	M_OutputDirectory = FPaths::ProjectSavedDir() / TEXT("UnitPortraits");
	FParse::Value(FCommandLine::Get(), UnitPortraitConstants::DirectorySwitch, M_OutputDirectory);
	M_ShotsPerUnit = UnitPortraitConstants::DefaultShotsPerUnit;
	FParse::Value(FCommandLine::Get(), UnitPortraitConstants::ShotsSwitch, M_ShotsPerUnit);
	LoadCaptureDelays();
	// Error dialogs are answered automatically in unattended runs, but each one stalls a frame; log them instead.
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	EnterPhase(EUnitPortraitPhase::WaitingForGameStart);
	UE_LOG(LogRTSUnitPortrait, Display, TEXT("RTS_PORTRAIT started: %d units, writing to %s"), M_Entries.Num(),
	       *M_OutputDirectory);
}

void UUnitPortraitCaptureSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

void UUnitPortraitCaptureSubsystem::Tick(float DeltaTime)
{
	if (M_Phase == EUnitPortraitPhase::Inactive || M_Phase == EUnitPortraitPhase::Done)
	{
		return;
	}
	if (M_Phase != EUnitPortraitPhase::WaitingForGameStart)
	{
		KeepGameRunning();
		KeepUnitReady();
		AimCamerasAtUnit();
	}
	switch (M_Phase)
	{
	case EUnitPortraitPhase::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case EUnitPortraitPhase::SpawningUnit:
		TickSpawningUnit();
		break;
	case EUnitPortraitPhase::Settling:
		TickSettling();
		break;
	case EUnitPortraitPhase::WaitingForShot:
		TickWaitingForShot();
		break;
	case EUnitPortraitPhase::CapturingShot:
		TickCapturingShot();
		break;
	case EUnitPortraitPhase::ClearingUnit:
		TickClearingUnit();
		break;
	default:
		break;
	}
}

TStatId UUnitPortraitCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUnitPortraitCaptureSubsystem, STATGROUP_Tickables);
}

void UUnitPortraitCaptureSubsystem::TickWaitingForGameStart()
{
	if (GetRealSecondsInPhase() > UnitPortraitConstants::MaxStartSeconds)
	{
		UE_LOG(LogRTSUnitPortrait, Error, TEXT("RTS_PORTRAIT could not start the game"));
		Finish();
		return;
	}
	if (GetRealSecondsInPhase() < UnitPortraitConstants::HudBuildSeconds || not TryStartGame())
	{
		return;
	}
	LogMapLighting();
	SpawnSkyFill();
	if (not FindAnchor() || not CreateCameras())
	{
		Finish();
		return;
	}
	EnterPhase(EUnitPortraitPhase::SpawningUnit);
}

void UUnitPortraitCaptureSubsystem::TickSpawningUnit()
{
	if (not M_Entries.IsValidIndex(M_EntryIndex))
	{
		Finish();
		return;
	}
	const FUnitPortraitEntry& Entry = M_Entries[M_EntryIndex];
	M_Unit = SpawnUnit(Entry);
	if (not M_Unit.IsValid())
	{
		UE_LOG(LogRTSUnitPortrait, Error, TEXT("RTS_PORTRAIT %s could not be spawned from %s"), *Entry.Name,
		       *Entry.ClassPath);
		EnterPhase(EUnitPortraitPhase::ClearingUnit);
		return;
	}
	M_ShotState = FUnitPortraitShotState();
	M_UnitSpawnLocation = M_Unit->GetActorLocation();
	const int32 SwappedSlots = ApplyLookOverride();
	if (Entry.LookOverride.bHasTeamColor || not Entry.LookOverride.SwapToMaterialPath.IsEmpty())
	{
		UE_LOG(LogRTSUnitPortrait, Display, TEXT("RTS_PORTRAIT %s look: %d slots swapped to %s, team colour %s"),
		       *Entry.Name, SwappedSlots, *Entry.LookOverride.SwapToMaterialPath,
		       Entry.LookOverride.bHasTeamColor ? *Entry.LookOverride.TeamColor.ToString() : TEXT("none"));
	}
	if (GetIsValidPlayerController())
	{
		M_PlayerController->FocusCameraOnActor(M_Unit.Get());
	}
	UE_LOG(LogRTSUnitPortrait, Display, TEXT("RTS_PORTRAIT %s spawned"), *Entry.Name);
	EnterPhase(EUnitPortraitPhase::Settling);
}

void UUnitPortraitCaptureSubsystem::TickSettling()
{
	using namespace UnitPortraitConstants;
	// Turrets and other child actors may set up their meshes after the spawn frame.
	ApplyLookOverride();
	const bool bIsStillPreparing = GetIsStillCompilingOrLoading() && GetGameSecondsInPhase() < MaxSettleSeconds;
	if (GetGameSecondsInPhase() < SettleSeconds || bIsStillPreparing)
	{
		return;
	}
	if (not M_Unit.IsValid())
	{
		EnterPhase(EUnitPortraitPhase::ClearingUnit);
		return;
	}
	ExportCameras(TEXT("idle"));
	LogSunLights();
	if (FParse::Param(FCommandLine::Get(), DebugViewportShotSwitch) && GEngine)
	{
		GEngine->Exec(GetWorld(), TEXT("HighResShot 1"));
	}
	if (M_ShotsPerUnit <= 0)
	{
		// Look comparisons only need the idle picture.
		EnterPhase(EUnitPortraitPhase::ClearingUnit);
		return;
	}
	OrderAttackGroundOutOfFrame();
	EnterPhase(EUnitPortraitPhase::WaitingForShot);
}

void UUnitPortraitCaptureSubsystem::TickWaitingForShot()
{
	if (GetDidMainWeaponFire())
	{
		M_ShotState.ShotGameSeconds = GetGameSeconds();
		M_ShotState.NextMomentIndex = 0;
		EnterPhase(EUnitPortraitPhase::CapturingShot);
		return;
	}
	if (GetGameSecondsInPhase() < UnitPortraitConstants::ShotTimeoutSeconds)
	{
		return;
	}
	UE_LOG(LogRTSUnitPortrait, Warning, TEXT("RTS_PORTRAIT %s fired %d of %d shots before the timeout"),
	       *GetCurrentEntryName(), M_ShotState.ShotCount, M_ShotsPerUnit);
	if (M_ShotState.ShotCount == 0)
	{
		ExportCameras(TEXT("noshot"));
	}
	EnterPhase(EUnitPortraitPhase::ClearingUnit);
}

void UUnitPortraitCaptureSubsystem::TickCapturingShot()
{
	using namespace UnitPortraitConstants;
	const int32 MomentCount = M_CaptureDelaysAfterShot.Num();
	const float SecondsSinceShot = GetGameSeconds() - M_ShotState.ShotGameSeconds;
	// At most one picture per tick: the render targets only hold what was rendered last frame.
	if (M_ShotState.NextMomentIndex < MomentCount
		&& SecondsSinceShot >= M_CaptureDelaysAfterShot[M_ShotState.NextMomentIndex])
	{
		ExportCameras(FString::Printf(TEXT("shot%d_t%d"), M_ShotState.ShotCount + 1, M_ShotState.NextMomentIndex));
		++M_ShotState.NextMomentIndex;
	}
	if (M_ShotState.NextMomentIndex < MomentCount)
	{
		return;
	}
	++M_ShotState.ShotCount;
	UE_LOG(LogRTSUnitPortrait, Display, TEXT("RTS_PORTRAIT %s pictured shot %d"), *GetCurrentEntryName(),
	       M_ShotState.ShotCount);
	if (M_ShotState.ShotCount >= M_ShotsPerUnit)
	{
		EnterPhase(EUnitPortraitPhase::ClearingUnit);
		return;
	}
	ReloadMainWeapon();
	EnterPhase(EUnitPortraitPhase::WaitingForShot);
}

void UUnitPortraitCaptureSubsystem::TickClearingUnit()
{
	if (M_Unit.IsValid())
	{
		DestroyUnit();
	}
	if (GetGameSecondsInPhase() < UnitPortraitConstants::ClearSeconds)
	{
		return;
	}
	++M_EntryIndex;
	EnterPhase(M_Entries.IsValidIndex(M_EntryIndex) ? EUnitPortraitPhase::SpawningUnit : EUnitPortraitPhase::Done);
	if (M_Phase == EUnitPortraitPhase::Done)
	{
		Finish();
	}
}

void UUnitPortraitCaptureSubsystem::EnterPhase(const EUnitPortraitPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
	M_PhaseStartGameSeconds = GetGameSeconds();
}

double UUnitPortraitCaptureSubsystem::GetRealSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartRealSeconds;
}

float UUnitPortraitCaptureSubsystem::GetGameSecondsInPhase() const
{
	return GetGameSeconds() - M_PhaseStartGameSeconds;
}

float UUnitPortraitCaptureSubsystem::GetGameSeconds() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

bool UUnitPortraitCaptureSubsystem::LoadEntries()
{
	FString ListFile;
	FParse::Value(FCommandLine::Get(), UnitPortraitConstants::ListSwitch, ListFile);
	TArray<FString> Lines;
	if (not FFileHelper::LoadFileToStringArray(Lines, *ListFile))
	{
		UE_LOG(LogRTSUnitPortrait, Error, TEXT("RTS_PORTRAIT cannot read the unit list %s"), *ListFile);
		return false;
	}
	// One unit per line as Name=/Game/Path/BP_Unit.BP_Unit_C; # starts a comment. Optional look fields follow the
	// class path: Name=/Game/..._C|Swap=/Game/Old.Old>/Game/New.New|TeamColor=0.8,0.02,0.02
	for (const FString& RawLine : Lines)
	{
		const FString Line = RawLine.TrimStartAndEnd();
		FUnitPortraitEntry Entry;
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")) || not Line.Split(TEXT("="), &Entry.Name, &Entry.ClassPath))
		{
			continue;
		}
		FString OptionsText;
		if (Entry.ClassPath.Split(UnitPortraitConstants::EntryOptionSeparator, &Entry.ClassPath, &OptionsText))
		{
			ParseLookOverride(OptionsText, Entry.LookOverride);
		}
		Entry.Name.TrimStartAndEndInline();
		Entry.ClassPath.TrimStartAndEndInline();
		M_Entries.Add(Entry);
	}
	if (M_Entries.IsEmpty())
	{
		UE_LOG(LogRTSUnitPortrait, Error, TEXT("RTS_PORTRAIT the unit list %s has no units"), *ListFile);
		return false;
	}
	return true;
}

void UUnitPortraitCaptureSubsystem::LoadCaptureDelays()
{
	using namespace UnitPortraitConstants;
	M_CaptureDelaysAfterShot = TArray<float>(CaptureDelaysAfterShot, UE_ARRAY_COUNT(CaptureDelaysAfterShot));
	FString DelayList;
	if (not FParse::Value(FCommandLine::Get(), ShotDelaysSwitch, DelayList, false))
	{
		return;
	}
	TArray<FString> DelayTexts;
	DelayList.ParseIntoArray(DelayTexts, TEXT(","));
	TArray<float> Delays;
	for (const FString& DelayText : DelayTexts)
	{
		Delays.Add(FCString::Atof(*DelayText));
	}
	if (Delays.IsEmpty())
	{
		return;
	}
	Delays.Sort();
	M_CaptureDelaysAfterShot = Delays;
}

bool UUnitPortraitCaptureSubsystem::TryStartGame()
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
	UE_LOG(LogRTSUnitPortrait, Display, TEXT("RTS_PORTRAIT game started"));
	return true;
}

void UUnitPortraitCaptureSubsystem::KeepGameRunning() const
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
			Picker->WidgetTree->FindWidget(UnitPortraitConstants::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
			UE_LOG(LogRTSUnitPortrait, Display, TEXT("RTS_PORTRAIT picked normal difficulty"));
		}
	}
	if (M_PlayerController.IsValid() && M_PlayerController->GetIsGameOnPause())
	{
		M_PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

bool UUnitPortraitCaptureSubsystem::FindAnchor()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName().StartsWith(UnitPortraitConstants::AnchorClassPrefix))
		{
			M_Anchor = *It;
			UE_LOG(LogRTSUnitPortrait, Display, TEXT("RTS_PORTRAIT anchor at %s yaw %.1f"),
			       *It->GetActorLocation().ToCompactString(), It->GetActorRotation().Yaw);
			return true;
		}
	}
	UE_LOG(LogRTSUnitPortrait, Error, TEXT("RTS_PORTRAIT no %s on this map"), UnitPortraitConstants::AnchorClassPrefix);
	return false;
}

bool UUnitPortraitCaptureSubsystem::CreateCameras()
{
	using namespace UnitPortraitConstants;
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TArray<FUnitPortraitView> ViewsToCreate(Views, UE_ARRAY_COUNT(Views));
	if (FParse::Param(FCommandLine::Get(), DebugPostProcessSwitch))
	{
		FUnitPortraitView& DebugView = ViewsToCreate.Add_GetRef(Views[0]);
		DebugView.Name += TEXT("WithPostProcess");
		DebugView.bKeepPostProcessMaterials = true;
	}
	if (FParse::Param(FCommandLine::Get(), RtsViewSwitch))
	{
		ViewsToCreate.Add(RtsView);
	}
	for (const FUnitPortraitView& View : ViewsToCreate)
	{
		ASceneCapture2D* Capture = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(),
		                                                               FTransform::Identity, SpawnParameters);
		UTextureRenderTarget2D* RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
			World, PictureSize, PictureSize, RTF_RGBA8_SRGB);
		if (not IsValid(Capture) || not IsValid(RenderTarget))
		{
			UE_LOG(LogRTSUnitPortrait, Error, TEXT("RTS_PORTRAIT camera %s could not be created"), *View.Name);
			return false;
		}
		ConfigureCaptureComponent(*Capture->GetCaptureComponent2D(), *RenderTarget, View);
		FUnitPortraitCamera& Camera = M_Cameras.AddDefaulted_GetRef();
		Camera.View = View;
		Camera.Capture = Capture;
		Camera.RenderTarget.Reset(RenderTarget);
	}
	return true;
}

void UUnitPortraitCaptureSubsystem::ConfigureCaptureComponent(USceneCaptureComponent2D& CaptureComponent,
                                                              UTextureRenderTarget2D& RenderTarget,
                                                              const FUnitPortraitView& View) const
{
	using namespace UnitPortraitConstants;
	CaptureComponent.TextureTarget = &RenderTarget;
	CaptureComponent.CaptureSource = SCS_FinalColorLDR;
	CaptureComponent.FOVAngle = FieldOfView;
	// Rendering every frame with kept state lets auto exposure, Lumen and temporal AA settle like the game view.
	CaptureComponent.bCaptureEveryFrame = true;
	CaptureComponent.bCaptureOnMovement = false;
	CaptureComponent.bAlwaysPersistRenderingState = true;
	// The fog of war is a post-process material; the portrait shows the unit, not what the player may see.
	CaptureComponent.ShowFlags.SetPostProcessMaterial(View.bKeepPostProcessMaterials);
	CopyPlayerCameraPostProcess(CaptureComponent);
	FPostProcessSettings& Settings = CaptureComponent.PostProcessSettings;
	ApplyGameViewLighting(Settings, not M_SkyFill.IsValid());
	CaptureComponent.PostProcessBlendWeight = 1.f;
	float IndirectLightIntensity = 1.f;
	if (FParse::Value(FCommandLine::Get(), IndirectLightSwitch, IndirectLightIntensity))
	{
		Settings.bOverride_IndirectLightingIntensity = true;
		Settings.IndirectLightingIntensity = IndirectLightIntensity;
	}
	float ExposureBias = 0.f;
	if (FParse::Value(FCommandLine::Get(), ExposureBiasSwitch, ExposureBias))
	{
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = ExposureBias;
	}
	if (FParse::Param(FCommandLine::Get(), FixedExposureSwitch))
	{
		// Auto exposure follows what is in frame, so side-by-side look comparisons came out lit differently.
		Settings.bOverride_AutoExposureMethod = true;
		Settings.AutoExposureMethod = AEM_Manual;
		Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Settings.AutoExposureApplyPhysicalCameraExposure = false;
	}
}

void UUnitPortraitCaptureSubsystem::CopyPlayerCameraPostProcess(USceneCaptureComponent2D& CaptureComponent) const
{
	// The player's camera may carry its own post process (exposure, Lumen); scene captures only see volumes.
	if (not M_PlayerController.IsValid() || not IsValid(M_PlayerController->PlayerCameraManager))
	{
		return;
	}
	const FMinimalViewInfo& PlayerView = M_PlayerController->PlayerCameraManager->GetCameraCacheView();
	if (PlayerView.PostProcessBlendWeight > 0.f)
	{
		CaptureComponent.PostProcessSettings = PlayerView.PostProcessSettings;
	}
	UE_LOG(LogRTSUnitPortrait, Display, TEXT("RTS_PORTRAIT player camera post process weight %.2f, exposure bias %d:%.2f"),
	       PlayerView.PostProcessBlendWeight, PlayerView.PostProcessSettings.bOverride_AutoExposureBias,
	       PlayerView.PostProcessSettings.AutoExposureBias);
	for (const FWeightedBlendable& Blendable : PlayerView.PostProcessSettings.WeightedBlendables.Array)
	{
		UE_LOG(LogRTSUnitPortrait, Display, TEXT("RTS_PORTRAIT   camera blendable %s weight %.2f"),
		       *GetNameSafe(Blendable.Object), Blendable.Weight);
	}
}

void UUnitPortraitCaptureSubsystem::SpawnSkyFill()
{
	float SkyFillIntensity = 0.f;
	if (not FParse::Value(FCommandLine::Get(), UnitPortraitConstants::SkyFillSwitch, SkyFillIntensity))
	{
		return;
	}
	UTextureCube* MapSkyCubemap = nullptr;
	for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
	{
		USkyLightComponent* MapSkyLight = It->GetLightComponent();
		MapSkyCubemap = MapSkyLight->Cubemap;
		// The scene renders only the newest registered sky light; the map's one re-registers after start and would
		// silently replace the fill (seen as black shadows in some runs), so it is taken out of the scene.
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
	UE_LOG(LogRTSUnitPortrait, Display, TEXT("RTS_PORTRAIT sky fill %.2f from %s"), SkyFillIntensity,
	       *GetNameSafe(MapSkyCubemap));
}

void UUnitPortraitCaptureSubsystem::LogSunLights() const
{
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		const UDirectionalLightComponent* Sun = Cast<UDirectionalLightComponent>(It->GetLightComponent());
		if (not IsValid(Sun))
		{
			continue;
		}
		UE_LOG(LogRTSUnitPortrait, Display,
		       TEXT("RTS_PORTRAIT sun %s rotation %s intensity %.2f mobility %d shadows %d visible %d atmosphere %d"),
		       *It->GetName(), *Sun->GetComponentRotation().ToCompactString(), Sun->Intensity,
		       static_cast<int32>(Sun->Mobility.GetValue()), Sun->CastShadows, Sun->IsVisible(),
		       Sun->IsUsedAsAtmosphereSunLight());
	}
}

void UUnitPortraitCaptureSubsystem::LogMapLighting() const
{
	LogSunLights();
	for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
	{
		const USkyLightComponent* SkyLight = It->GetLightComponent();
		UE_LOG(LogRTSUnitPortrait, Display,
		       TEXT("RTS_PORTRAIT sky light %s mobility %d intensity %.2f realtime %d source %d cubemap %s visible %d"),
		       *It->GetName(), static_cast<int32>(SkyLight->Mobility.GetValue()), SkyLight->Intensity, SkyLight->bRealTimeCapture,
		       static_cast<int32>(SkyLight->SourceType), *GetNameSafe(SkyLight->Cubemap.Get()), SkyLight->IsVisible());
	}
	for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
	{
		const FPostProcessSettings& Settings = It->Settings;
		UE_LOG(LogRTSUnitPortrait, Display,
		       TEXT("RTS_PORTRAIT post process volume %s unbound %d enabled %d gi %d:%d exposure method %d:%d bias %d:%.2f"),
		       *It->GetName(), It->bUnbound, It->bEnabled, Settings.bOverride_DynamicGlobalIlluminationMethod,
		       static_cast<int32>(Settings.DynamicGlobalIlluminationMethod), Settings.bOverride_AutoExposureMethod,
		       static_cast<int32>(Settings.AutoExposureMethod), Settings.bOverride_AutoExposureBias,
		       Settings.AutoExposureBias);
		for (const FWeightedBlendable& Blendable : Settings.WeightedBlendables.Array)
		{
			UE_LOG(LogRTSUnitPortrait, Display, TEXT("RTS_PORTRAIT   blendable %s weight %.2f"),
			       *GetNameSafe(Blendable.Object), Blendable.Weight);
		}
	}
}

AActor* UUnitPortraitCaptureSubsystem::SpawnUnit(const FUnitPortraitEntry& Entry) const
{
	using namespace UnitPortraitConstants;
	UWorld* World = GetWorld();
	if (not IsValid(World) || not GetIsValidAnchor())
	{
		return nullptr;
	}
	// Vehicles and building expansions (gun pits) alike; anything with weapons and ICommands can be pictured.
	UClass* UnitClass = LoadClass<AActor>(nullptr, *Entry.ClassPath);
	if (not IsValid(UnitClass))
	{
		return nullptr;
	}
	FVector SpawnLocation = M_Anchor->GetActorLocation();
	FHitResult GroundHit;
	const FVector Up(0.f, 0.f, GroundTraceHalfHeight);
	if (World->LineTraceSingleByChannel(GroundHit, SpawnLocation + Up, SpawnLocation - Up, ECC_Visibility))
	{
		SpawnLocation = GroundHit.Location + FVector(0.f, 0.f, SpawnHeightAboveGround);
	}
	float UnitYawFromAnchor = DefaultUnitYawFromAnchor;
	FParse::Value(FCommandLine::Get(), UnitYawSwitch, UnitYawFromAnchor);
	const FRotator SpawnRotation(0.f, M_Anchor->GetActorRotation().Yaw + UnitYawFromAnchor, 0.f);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AActor* Unit = World->SpawnActor<AActor>(UnitClass, SpawnLocation, SpawnRotation, SpawnParameters);
	APawn* UnitPawn = Cast<APawn>(Unit);
	// Look comparisons take no shots; without a controller the unit does not engage the map's enemies mid-picture.
	const bool bNeedsController = M_ShotsPerUnit > 0;
	if (bNeedsController && IsValid(UnitPawn) && not IsValid(UnitPawn->GetController()))
	{
		UnitPawn->SpawnDefaultController();
	}
	return Unit;
}

void UUnitPortraitCaptureSubsystem::ParseLookOverride(const FString& OptionsText,
                                                      FUnitPortraitLookOverride& OutLookOverride)
{
	using namespace UnitPortraitConstants;
	TArray<FString> Options;
	OptionsText.ParseIntoArray(Options, EntryOptionSeparator);
	for (const FString& RawOption : Options)
	{
		const FString Option = RawOption.TrimStartAndEnd();
		if (Option.StartsWith(SwapOption))
		{
			Option.RightChop(FCString::Strlen(SwapOption)).Split(SwapTargetSeparator,
			                                                     &OutLookOverride.SwapFromMaterialPath,
			                                                     &OutLookOverride.SwapToMaterialPath);
			continue;
		}
		if (not Option.StartsWith(TeamColorOption))
		{
			continue;
		}
		TArray<FString> ComponentTexts;
		Option.RightChop(FCString::Strlen(TeamColorOption)).ParseIntoArray(ComponentTexts, TEXT(","));
		if (ComponentTexts.Num() < MinTeamColorComponents)
		{
			continue;
		}
		OutLookOverride.bHasTeamColor = true;
		OutLookOverride.TeamColor = FLinearColor(FCString::Atof(*ComponentTexts[0]), FCString::Atof(*ComponentTexts[1]),
		                                         FCString::Atof(*ComponentTexts[2]), 1.f);
		if (ComponentTexts.IsValidIndex(AlphaComponentIndex))
		{
			OutLookOverride.TeamColor.A = FCString::Atof(*ComponentTexts[AlphaComponentIndex]);
		}
	}
}

int32 UUnitPortraitCaptureSubsystem::ApplyLookOverride() const
{
	if (not M_Unit.IsValid() || not M_Entries.IsValidIndex(M_EntryIndex))
	{
		return 0;
	}
	const FUnitPortraitLookOverride& LookOverride = M_Entries[M_EntryIndex].LookOverride;
	UMaterialInterface* SwapToMaterial = LookOverride.SwapToMaterialPath.IsEmpty()
		                                     ? nullptr
		                                     : LoadObject<UMaterialInterface>(nullptr, *LookOverride.SwapToMaterialPath);
	TArray<AActor*> UnitActors;
	M_Unit->GetAttachedActors(UnitActors, true, true);
	UnitActors.Add(M_Unit.Get());
	int32 SwappedSlots = 0;
	for (const AActor* UnitActor : UnitActors)
	{
		TInlineComponentArray<UMeshComponent*> Meshes(UnitActor);
		for (UMeshComponent* Mesh : Meshes)
		{
			SwappedSlots += ApplyLookOverrideToMesh(*Mesh, LookOverride, SwapToMaterial);
		}
	}
	return SwappedSlots;
}

int32 UUnitPortraitCaptureSubsystem::ApplyLookOverrideToMesh(UMeshComponent& Mesh,
                                                             const FUnitPortraitLookOverride& LookOverride,
                                                             UMaterialInterface* SwapToMaterial)
{
	if (LookOverride.bHasTeamColor)
	{
		Mesh.SetCustomPrimitiveDataVector4(UnitPortraitConstants::TeamColorPrimitiveDataIndex,
		                                   FVector4(LookOverride.TeamColor));
	}
	if (not IsValid(SwapToMaterial))
	{
		return 0;
	}
	int32 SwappedSlots = 0;
	for (int32 SlotIndex = 0; SlotIndex < Mesh.GetNumMaterials(); ++SlotIndex)
	{
		const UMaterialInterface* SlotMaterial = Mesh.GetMaterial(SlotIndex);
		if (IsValid(SlotMaterial) && SlotMaterial->GetPathName() == LookOverride.SwapFromMaterialPath)
		{
			Mesh.SetMaterial(SlotIndex, SwapToMaterial);
			++SwappedSlots;
		}
	}
	return SwappedSlots;
}

void UUnitPortraitCaptureSubsystem::KeepUnitReady() const
{
	if (not M_Unit.IsValid())
	{
		return;
	}
	if (UHealthComponent* Health = M_Unit->FindComponentByClass<UHealthComponent>())
	{
		Health->SetCurrentHealth(Health->GetMaxHealth());
	}
	// Hull-aimed guns turn the vehicle and some drift off the road while doing so; keep it on the anchor.
	const FVector UnitLocation = M_Unit->GetActorLocation();
	if (FVector::DistSquared2D(UnitLocation, M_UnitSpawnLocation) > FMath::Square(UnitPortraitConstants::MaxUnitDrift))
	{
		const FVector PinnedLocation(M_UnitSpawnLocation.X, M_UnitSpawnLocation.Y, UnitLocation.Z);
		M_Unit->SetActorLocation(PinnedLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	TArray<AActor*> UnitActors;
	M_Unit->GetAttachedActors(UnitActors, true, true);
	UnitActors.Add(M_Unit.Get());
	for (const AActor* UnitActor : UnitActors)
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(UnitActor);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			// Gun pit crews are throttled by the optimizer too; their drill montages must play at full rate.
			Mesh->SetComponentTickEnabled(true);
			Mesh->SetComponentTickInterval(0.f);
		}
	}
}

void UUnitPortraitCaptureSubsystem::AimCamerasAtUnit() const
{
	using namespace UnitPortraitConstants;
	FBox Bounds;
	if (not GetUnitVisibleBounds(Bounds))
	{
		return;
	}
	float Framing = DefaultFraming;
	FParse::Value(FCommandLine::Get(), FramingSwitch, Framing);
	const float Radius = Bounds.GetExtent().Size();
	const float Distance = Framing * Radius / FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f));
	const float UnitYaw = M_Unit->GetActorRotation().Yaw;
	for (const FUnitPortraitCamera& Camera : M_Cameras)
	{
		if (not Camera.Capture.IsValid())
		{
			continue;
		}
		// The camera looks down at the unit from the view's side; Pitch is negative (looking down).
		const FRotator LookRotation(Camera.View.Pitch, UnitYaw + Camera.View.YawFromUnitForward + 180.f, 0.f);
		const FVector CameraLocation = Bounds.GetCenter() - LookRotation.Vector() * Distance;
		Camera.Capture->SetActorLocationAndRotation(CameraLocation, LookRotation);
	}
}

bool UUnitPortraitCaptureSubsystem::GetUnitVisibleBounds(FBox& OutBounds) const
{
	if (not M_Unit.IsValid())
	{
		return false;
	}
	TArray<AActor*> UnitActors;
	M_Unit->GetAttachedActors(UnitActors, true, true);
	UnitActors.Add(M_Unit.Get());
	OutBounds = FBox(ForceInit);
	const FVector UnitLocation = M_Unit->GetActorLocation();
	const float MaxRadius = UnitPortraitConstants::MaxUnitMeshRadius;
	for (const AActor* UnitActor : UnitActors)
	{
		// Only the vehicle's own meshes: widget components (health bars) are meshes too and would widen the frame.
		TInlineComponentArray<UMeshComponent*> Meshes(UnitActor);
		for (const UMeshComponent* Mesh : Meshes)
		{
			const bool bIsVehicleMesh = Mesh->IsA<UStaticMeshComponent>() || Mesh->IsA<USkeletalMeshComponent>();
			const bool bIsNearUnit = Mesh->Bounds.SphereRadius < MaxRadius
				&& FVector::DistSquared(Mesh->Bounds.Origin, UnitLocation) < FMath::Square(MaxRadius);
			if (bIsVehicleMesh && bIsNearUnit && Mesh->IsVisible() && not Mesh->bHiddenInGame)
			{
				OutBounds += Mesh->Bounds.GetBox();
			}
		}
	}
	return OutBounds.IsValid != 0;
}

UWeaponState* UUnitPortraitCaptureSubsystem::FindMainWeapon() const
{
	if (not M_Unit.IsValid())
	{
		return nullptr;
	}
	UWeaponState* MainWeapon = nullptr;
	for (UWeaponState* Weapon : GetMountedWeapons())
	{
		const bool bIsLarger = IsValid(Weapon) && (not IsValid(MainWeapon)
			|| Weapon->GetRawWeaponData().WeaponCalibre > MainWeapon->GetRawWeaponData().WeaponCalibre);
		if (bIsLarger)
		{
			MainWeapon = Weapon;
		}
	}
	return MainWeapon;
}

TArray<UWeaponState*> UUnitPortraitCaptureSubsystem::GetMountedWeapons() const
{
	if (const ATankMaster* Tank = Cast<ATankMaster>(M_Unit.Get()))
	{
		return FRTSWeaponHelpers::GetWeaponsMountedOnTank(Tank);
	}
	// Building expansions carry their guns on child turret actors (and sometimes hull weapon components).
	TArray<UWeaponState*> Weapons;
	TArray<AActor*> UnitActors;
	M_Unit->GetAttachedActors(UnitActors, true, true);
	UnitActors.Add(M_Unit.Get());
	for (AActor* UnitActor : UnitActors)
	{
		if (ACPPTurretsMaster* Turret = Cast<ACPPTurretsMaster>(UnitActor))
		{
			Weapons.Append(Turret->GetWeapons());
		}
		TInlineComponentArray<UHullWeaponComponent*> HullWeapons(UnitActor);
		for (UHullWeaponComponent* HullWeapon : HullWeapons)
		{
			Weapons.Append(HullWeapon->GetWeapons());
		}
	}
	return Weapons;
}

void UUnitPortraitCaptureSubsystem::OrderAttackGroundOutOfFrame()
{
	using namespace UnitPortraitConstants;
	UWeaponState* MainWeapon = FindMainWeapon();
	M_ShotState.MainWeapon = MainWeapon;
	if (not IsValid(MainWeapon) || not M_Unit.IsValid())
	{
		UE_LOG(LogRTSUnitPortrait, Warning, TEXT("RTS_PORTRAIT %s has no weapon; only the idle picture is taken"),
		       *GetCurrentEntryName());
		return;
	}
	M_ShotState.LastMagazine = MainWeapon->GetCurrentMagCapacity();
	const float Range = MainWeapon->GetRange();
	const float TargetDistance = FMath::Min(FMath::Max(Range * TargetRangeFraction, MinTargetDistance),
	                                        Range * MaxTargetRangeFraction);
	float TargetYaw = 0.f;
	FParse::Value(FCommandLine::Get(), TargetYawSwitch, TargetYaw);
	const FVector Direction = M_Unit->GetActorForwardVector().GetSafeNormal2D().RotateAngleAxis(
		TargetYaw, FVector::UpVector);
	FVector Target = M_Unit->GetActorLocation() + Direction * TargetDistance;
	FHitResult GroundHit;
	const FVector Up(0.f, 0.f, GroundTraceHalfHeight);
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, Target + Up, Target - Up, ECC_Visibility))
	{
		Target = GroundHit.Location;
	}
	ICommands* Commands = Cast<ICommands>(M_Unit.Get());
	const ECommandQueueError Error = Commands ? Commands->AttackGround(Target, true) : ECommandQueueError::AbilityNotAllowed;
	UE_LOG(LogRTSUnitPortrait, Display, TEXT("RTS_PORTRAIT %s attack ground at %.0f cm (range %.0f): %s"),
	       *GetCurrentEntryName(), TargetDistance, Range,
	       Error == ECommandQueueError::NoError ? TEXT("accepted") : TEXT("refused"));
}

bool UUnitPortraitCaptureSubsystem::GetDidMainWeaponFire()
{
	const UWeaponState* MainWeapon = M_ShotState.MainWeapon.Get();
	if (not IsValid(MainWeapon))
	{
		return false;
	}
	const int32 Magazine = MainWeapon->GetCurrentMagCapacity();
	const bool bFired = M_ShotState.LastMagazine != INDEX_NONE && Magazine < M_ShotState.LastMagazine;
	M_ShotState.LastMagazine = Magazine;
	return bFired;
}

void UUnitPortraitCaptureSubsystem::ReloadMainWeapon()
{
	UWeaponState* MainWeapon = M_ShotState.MainWeapon.Get();
	if (not IsValid(MainWeapon))
	{
		return;
	}
	// The next shot comes right away instead of after a heavy gun's reload.
	MainWeapon->ForceInstantReload();
	M_ShotState.LastMagazine = MainWeapon->GetCurrentMagCapacity();
}

void UUnitPortraitCaptureSubsystem::ExportCameras(const FString& MomentLabel) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const FString UnitName = GetCurrentEntryName();
	const FString UnitDirectory = M_OutputDirectory / UnitName;
	FString CropLines;
	for (const FUnitPortraitCamera& Camera : M_Cameras)
	{
		if (not Camera.RenderTarget.IsValid())
		{
			continue;
		}
		const FString FileName = FString::Printf(TEXT("%s_%s_%s.png"), *UnitName, *Camera.View.Name, *MomentLabel);
		UKismetRenderingLibrary::ExportRenderTarget(World, Camera.RenderTarget.Get(), UnitDirectory, FileName);
		FBox2D CropBox;
		if (GetUnitCropBox(Camera, CropBox))
		{
			CropLines += FString::Printf(TEXT("%s,%.0f,%.0f,%.0f,%.0f\n"), *FileName, CropBox.Min.X, CropBox.Min.Y,
			                             CropBox.Max.X, CropBox.Max.Y);
		}
	}
	// The crop step (Tools/UnitPortraits/crop_portraits.py) squares the picture around the vehicle with these boxes.
	FFileHelper::SaveStringToFile(CropLines, *(UnitDirectory / TEXT("crops.csv")), FFileHelper::EEncodingOptions::AutoDetect,
	                              &IFileManager::Get(), FILEWRITE_Append);
}

bool UUnitPortraitCaptureSubsystem::GetUnitCropBox(const FUnitPortraitCamera& Camera, FBox2D& OutCropBox) const
{
	using namespace UnitPortraitConstants;
	FBox Bounds;
	if (not Camera.Capture.IsValid() || not GetUnitVisibleBounds(Bounds))
	{
		return false;
	}
	const FTransform CameraTransform = Camera.Capture->GetActorTransform();
	const float TanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f));
	const float HalfPicture = PictureSize * 0.5f;
	FVector Corners[8];
	Bounds.GetVertices(Corners);
	OutCropBox = FBox2D(ForceInit);
	for (const FVector& Corner : Corners)
	{
		// Camera space: X forward, Y right, Z up; the square picture has the same field of view on both axes.
		const FVector Local = CameraTransform.InverseTransformPosition(Corner);
		if (Local.X <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const float ScreenX = HalfPicture + HalfPicture * Local.Y / (Local.X * TanHalfFieldOfView);
		const float ScreenY = HalfPicture - HalfPicture * Local.Z / (Local.X * TanHalfFieldOfView);
		OutCropBox += FVector2D(ScreenX, ScreenY);
	}
	if (not OutCropBox.bIsValid)
	{
		return false;
	}
	OutCropBox = OutCropBox.ExpandBy(PictureSize * CropBoxMarginFraction);
	return true;
}

FString UUnitPortraitCaptureSubsystem::GetCurrentEntryName() const
{
	return M_Entries.IsValidIndex(M_EntryIndex) ? M_Entries[M_EntryIndex].Name : FString(TEXT("none"));
}

void UUnitPortraitCaptureSubsystem::DestroyUnit()
{
	TArray<AActor*> UnitActors;
	M_Unit->GetAttachedActors(UnitActors, true, true);
	const APawn* UnitPawn = Cast<APawn>(M_Unit.Get());
	AController* Controller = IsValid(UnitPawn) ? UnitPawn->GetController() : nullptr;
	for (AActor* UnitActor : UnitActors)
	{
		UnitActor->Destroy();
	}
	M_Unit->Destroy();
	if (IsValid(Controller))
	{
		Controller->Destroy();
	}
	M_Unit.Reset();
}

void UUnitPortraitCaptureSubsystem::Finish()
{
	M_Phase = EUnitPortraitPhase::Done;
	UE_LOG(LogRTSUnitPortrait, Display, TEXT("RTS_PORTRAIT done"));
	FPlatformMisc::RequestExit(false, TEXT("UnitPortraitCapture"));
}

bool UUnitPortraitCaptureSubsystem::GetIsValidAnchor() const
{
	if (M_Anchor.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_Anchor"),
		TEXT("UUnitPortraitCaptureSubsystem::GetIsValidAnchor"),
		this
	);
	return false;
}

bool UUnitPortraitCaptureSubsystem::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PlayerController"),
		TEXT("UUnitPortraitCaptureSubsystem::GetIsValidPlayerController"),
		this
	);
	return false;
}

#else

bool UUnitPortraitCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UUnitPortraitCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UUnitPortraitCaptureSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UUnitPortraitCaptureSubsystem::Tick(float DeltaTime)
{
}

TStatId UUnitPortraitCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUnitPortraitCaptureSubsystem, STATGROUP_Tickables);
}

#endif
