// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AddOnArmorBreakTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "AddOnArmorTestVehicle.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkyLightComponent.h"
#include "DynamicRHI.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/PlatformMemory.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraComponentPool.h"
#include "NiagaraSystem.h"
#include "NiagaraWorldManager.h"
#include "RenderTimer.h"
#include "ShaderCompiler.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAddOnArmorBreakTest, Log, All);

namespace AddOnArmorBreakTestConstants
{
	const TCHAR* const ModeSwitch = TEXT("AddOnArmorBreakTest=");
	const TCHAR* const TechniqueSwitch = TEXT("AddOnArmorBreakTechnique=");
	const TCHAR* const GcCollisionSwitch = TEXT("AddOnArmorBreakGcCollision=");
	const TCHAR* const VehiclesSwitch = TEXT("AddOnArmorBreakVehicles=");
	const TCHAR* const HitsPerSecondSwitch = TEXT("AddOnArmorBreakHitsPerSecond=");
	const TCHAR* const DriveSwitch = TEXT("AddOnArmorBreakDrive=");
	const TCHAR* const SeedSwitch = TEXT("AddOnArmorBreakSeed=");
	const TCHAR* const IdleSecondsSwitch = TEXT("AddOnArmorBreakIdleSeconds=");
	const TCHAR* const HitSecondsSwitch = TEXT("AddOnArmorBreakHitSeconds=");
	const TCHAR* const SettleSecondsSwitch = TEXT("AddOnArmorBreakSettleSeconds=");
	const TCHAR* const TagSwitch = TEXT("AddOnArmorBreakTag=");
	const TCHAR* const DirectorySwitch = TEXT("AddOnArmorBreakDir=");
	const TCHAR* const LocationSwitch = TEXT("AddOnArmorBreakAt=");
	const TCHAR* const SkyFillSwitch = TEXT("AddOnArmorBreakSkyFill=");
	// Capture: techniques separated by '+', e.g. BakedMask+BakedWpo (default Niagara+GeometryCollection).
	const TCHAR* const CaptureTechniquesSwitch = TEXT("AddOnArmorBreakCaptureTechniques=");
	const TCHAR* const ViewDistanceSwitch = TEXT("AddOnArmorBreakViewDistance=");
	const TCHAR* const ViewPitchSwitch = TEXT("AddOnArmorBreakViewPitch=");
	const TCHAR* const ProfileGpuSwitch = TEXT("AddOnArmorBreakProfileGpu");
	const TCHAR* const ExposureBiasSwitch = TEXT("AddOnArmorBreakExposureBias=");
	const TCHAR* const AnchorClassPrefix = TEXT("BP_UnitImage_LandscapeAnchor");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));

	// Real seconds: the HUD and start widget need a moment before the game can be started.
	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 60.0;
	// Game seconds.
	constexpr float MinSettleSeconds = 3.f;
	constexpr float MaxSettleSeconds = 120.f;
	// Long enough for the warm-up debris to be removed before measuring.
	constexpr float WarmUpSeconds = 6.f;
	constexpr int32 WarmUpHits = 3;
	constexpr float GroundTraceHalfHeight = 5000.f;
	constexpr float ExposureAdaptSpeed = 10.f;

	// Capture: the two vehicles side by side along X, their right (+Y) sides towards the cameras.
	constexpr float CaptureVehicleSpacing = 1000.f;
	constexpr int32 PictureWidth = 1280;
	constexpr int32 PictureHeight = 720;
	const float CaptureMomentOffsets[] = {-0.05f, 0.05f, 0.2f, 0.5f, 1.f, 2.f, 3.3f, 4.5f};
	constexpr float CaptureEndPadding = 1.f;

	// Spike: one vehicle on a circle, hit at a fixed interval; each released piece is checked once it had time to land.
	constexpr float SpikeCircleRadius = 1200.f;
	constexpr float SpikeSpeed = 350.f;
	constexpr int32 SpikeHitCount = 6;
	constexpr float SpikeHitInterval = 1.5f;
	constexpr float SpikeLandingAge = 2.2f;
	constexpr float SpikeIntactTolerance = 3.f;
	constexpr float SpikeBreakEventWait = 1.f;
	const float SpikePictureOffsets[] = {0.3f, 1.2f};

	// Perf: a grid of vehicles, each driving its own circle; one camera renders a game-like view every frame.
	constexpr float PerfGridSpacing = 1800.f;
	constexpr float PerfCircleRadius = 400.f;
	constexpr float PerfSpeed = 400.f;
	constexpr int32 PerfViewWidth = 1920;
	constexpr int32 PerfViewHeight = 1080;
	constexpr float PercentileHigh = 0.95f;
	constexpr float BytesPerMegabyte = 1024.f * 1024.f;

	struct FHitKind
	{
		const TCHAR* Name;
		float Weight;
		float BreakRadius;
		float PushSpeed;
		int32 MaxPieces;
	};
	// Small cannon, a 75 mm AP shell and a 105 mm HE shell.
	const FHitKind HitKinds[] = {
		{TEXT("Small"), 0.25f, 8.f, 300.f, 2},
		{TEXT("Medium"), 0.6f, 22.f, 450.f, 5},
		{TEXT("Heavy"), 0.15f, 48.f, 650.f, 12}
	};
	// Shells come from the side the hit is on, turned up to this far from straight in, and slightly downward.
	constexpr float MaxShellYawDegrees = 40.f;
	constexpr float MinShellPitchDegrees = -15.f;
	constexpr float MaxShellPitchDegrees = -3.f;
	// Hits land up to this far from the centre of the picked piece, within the plate.
	constexpr float HitOffsetOnPiece = 12.f;

	bool GetIsStillCompilingOrLoading()
	{
		const bool bIsCompilingShaders = GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0;
		return bIsCompilingShaders || IsAsyncLoading();
	}

	bool ParseTechnique(const FString& Text, EAddOnArmorTestTechnique& OutTechnique)
	{
		if (Text.Equals(TEXT("Niagara"), ESearchCase::IgnoreCase))
		{
			OutTechnique = EAddOnArmorTestTechnique::Niagara;
			return true;
		}
		if (Text.Equals(TEXT("GeometryCollection"), ESearchCase::IgnoreCase) || Text.Equals(TEXT("GC"), ESearchCase::IgnoreCase))
		{
			OutTechnique = EAddOnArmorTestTechnique::GeometryCollection;
			return true;
		}
		const TPair<const TCHAR*, EAddOnArmorTestTechnique> BakedTechniques[] = {
			{TEXT("BakedOpaque"), EAddOnArmorTestTechnique::BakedOpaque},
			{TEXT("BakedMask"), EAddOnArmorTestTechnique::BakedMask},
			{TEXT("BakedWpo"), EAddOnArmorTestTechnique::BakedWpo},
			{TEXT("None"), EAddOnArmorTestTechnique::None}
		};
		for (const TPair<const TCHAR*, EAddOnArmorTestTechnique>& Candidate : BakedTechniques)
		{
			if (Text.Equals(Candidate.Key, ESearchCase::IgnoreCase))
			{
				OutTechnique = Candidate.Value;
				return true;
			}
		}
		return false;
	}

	const TCHAR* GetTechniqueName(const EAddOnArmorTestTechnique Technique)
	{
		switch (Technique)
		{
		case EAddOnArmorTestTechnique::Niagara:
			return TEXT("Niagara");
		case EAddOnArmorTestTechnique::GeometryCollection:
			return TEXT("GeometryCollection");
		case EAddOnArmorTestTechnique::BakedOpaque:
			return TEXT("BakedOpaque");
		case EAddOnArmorTestTechnique::BakedMask:
			return TEXT("BakedMask");
		case EAddOnArmorTestTechnique::BakedWpo:
			return TEXT("BakedWpo");
		default:
			return TEXT("None");
		}
	}

	float CyclesToMs(const uint32 Cycles)
	{
		return static_cast<float>(FPlatformTime::ToMilliseconds(Cycles));
	}
}

bool UAddOnArmorBreakTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	FString Mode;
	return FParse::Value(FCommandLine::Get(), AddOnArmorBreakTestConstants::ModeSwitch, Mode);
}

void UAddOnArmorBreakTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (not ReadSettings())
	{
		Finish();
		return;
	}
	// Error dialogs are answered automatically in unattended runs, but each one stalls a frame; log them instead.
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	M_HitStream.Initialize(M_Settings.Seed);
	EnterPhase(EAddOnArmorBreakTestPhase::WaitingForGameStart);
	UE_LOG(LogRTSAddOnArmorBreakTest, Display, TEXT("RTS_ADDON_ARMOR started mode=%d technique=%s vehicles=%d hits_per_s=%.1f drive=%d tag=%s"),
	       static_cast<int32>(M_Mode), AddOnArmorBreakTestConstants::GetTechniqueName(M_Technique), M_Settings.VehicleCount,
	       M_Settings.HitsPerSecond, M_Settings.bDrive ? 1 : 0, *M_Settings.Tag);
}

void UAddOnArmorBreakTestSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

void UAddOnArmorBreakTestSubsystem::Tick(const float DeltaTime)
{
	if (M_Phase == EAddOnArmorBreakTestPhase::Inactive || M_Phase == EAddOnArmorBreakTestPhase::Done)
	{
		return;
	}
	if (M_Phase != EAddOnArmorBreakTestPhase::WaitingForGameStart)
	{
		KeepGameRunning();
		AimCameras();
	}
	switch (M_Phase)
	{
	case EAddOnArmorBreakTestPhase::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case EAddOnArmorBreakTestPhase::Settling:
		TickSettling();
		break;
	case EAddOnArmorBreakTestPhase::WarmingUp:
		TickWarmingUp();
		break;
	case EAddOnArmorBreakTestPhase::Idle:
		TickIdle();
		break;
	case EAddOnArmorBreakTestPhase::Hitting:
		TickHitting(DeltaTime);
		break;
	case EAddOnArmorBreakTestPhase::SettlingDebris:
		TickSettlingDebris();
		break;
	default:
		break;
	}
	M_LastTickRealSeconds = FPlatformTime::Seconds();
}

TStatId UAddOnArmorBreakTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAddOnArmorBreakTestSubsystem, STATGROUP_Tickables);
}

// ----------------------------------------------------------------------------------------------------------------
// Phases
// ----------------------------------------------------------------------------------------------------------------

void UAddOnArmorBreakTestSubsystem::TickWaitingForGameStart()
{
	if (GetRealSecondsInPhase() > AddOnArmorBreakTestConstants::MaxStartSeconds)
	{
		UE_LOG(LogRTSAddOnArmorBreakTest, Error, TEXT("RTS_ADDON_ARMOR could not start the game"));
		Finish();
		return;
	}
	if (GetRealSecondsInPhase() < AddOnArmorBreakTestConstants::HudBuildSeconds || not TryStartGame())
	{
		return;
	}
	SpawnSkyFill();
	if (not FindBaseLocation() || not SpawnVehicles())
	{
		Finish();
		return;
	}
	CreateCameras();
	EnterPhase(EAddOnArmorBreakTestPhase::Settling);
}

void UAddOnArmorBreakTestSubsystem::TickSettling()
{
	using namespace AddOnArmorBreakTestConstants;
	const bool bIsStillPreparing = GetIsStillCompilingOrLoading() && GetGameSecondsInPhase() < MaxSettleSeconds;
	if (GetGameSecondsInPhase() < MinSettleSeconds || bIsStillPreparing)
	{
		return;
	}
	if (M_Mode == EAddOnArmorBreakTestMode::Perf)
	{
		UNiagaraSystem* const DebrisSystem = LoadObject<UNiagaraSystem>(nullptr, AddOnArmorBreakTest::NiagaraSystemPath);
		FNiagaraWorldManager* const WorldManager = FNiagaraWorldManager::Get(GetWorld());
		if (M_Technique == EAddOnArmorTestTechnique::Niagara && IsValid(DebrisSystem) && WorldManager != nullptr)
		{
			// The burst phase should measure reuse, not the pool growing.
			WorldManager->GetComponentPool()->PrimePool(DebrisSystem, GetWorld());
		}
		for (int32 HitIndex = 0; HitIndex < WarmUpHits; ++HitIndex)
		{
			FireRandomHit();
		}
		EnterPhase(EAddOnArmorBreakTestPhase::WarmingUp);
		return;
	}
	ExportCameras(TEXT("intact"));
	EnterPhase(EAddOnArmorBreakTestPhase::Hitting);
}

void UAddOnArmorBreakTestSubsystem::TickWarmingUp()
{
	if (GetGameSecondsInPhase() < AddOnArmorBreakTestConstants::WarmUpSeconds)
	{
		return;
	}
	ExportCameras(TEXT("perf_view"));
	EnterPhase(EAddOnArmorBreakTestPhase::Idle);
}

void UAddOnArmorBreakTestSubsystem::ProfileGpuHalfwayThrough(const float PhaseSeconds)
{
	if (not M_Settings.bProfileGpu || M_ProfiledPhases.Contains(M_Phase) || GetGameSecondsInPhase() < 0.5f * PhaseSeconds)
	{
		return;
	}
	M_ProfiledPhases.Add(M_Phase);
	UE_LOG(LogRTSAddOnArmorBreakTest, Display, TEXT("RTS_ADDON_ARMOR_PROFILEGPU begin tag=%s phase=%s"), *M_Settings.Tag,
	       GetPhaseName(M_Phase));
	// The pass breakdown of the next rendered frame is written to the log.
	GEngine->Exec(GetWorld(), TEXT("ProfileGPU"));
}

void UAddOnArmorBreakTestSubsystem::TickIdle()
{
	RecordFrame();
	ProfileGpuHalfwayThrough(M_Settings.IdleSeconds);
	if (GetGameSecondsInPhase() < M_Settings.IdleSeconds)
	{
		return;
	}
	EnterPhase(EAddOnArmorBreakTestPhase::Hitting);
}

void UAddOnArmorBreakTestSubsystem::TickHitting(const float DeltaTime)
{
	switch (M_Mode)
	{
	case EAddOnArmorBreakTestMode::Capture:
		TickCapture();
		return;
	case EAddOnArmorBreakTestMode::Spike:
		CheckSpikeReleases();
		FireRandomHits(DeltaTime);
		ExportDueSpikePictures();
		if (GetGameSecondsInPhase() >= AddOnArmorBreakTestConstants::SpikeHitCount * AddOnArmorBreakTestConstants::SpikeHitInterval)
		{
			EnterPhase(EAddOnArmorBreakTestPhase::SettlingDebris);
		}
		return;
	default:
		RecordFrame();
		ProfileGpuHalfwayThrough(M_Settings.HitSeconds);
		FireRandomHits(DeltaTime);
		if (GetGameSecondsInPhase() >= M_Settings.HitSeconds)
		{
			EnterPhase(EAddOnArmorBreakTestPhase::SettlingDebris);
		}
		return;
	}
}

void UAddOnArmorBreakTestSubsystem::TickSettlingDebris()
{
	using namespace AddOnArmorBreakTestConstants;
	if (M_Mode == EAddOnArmorBreakTestMode::Spike)
	{
		CheckSpikeReleases();
		ExportDueSpikePictures();
		const float SpikeSettleSeconds = SpikeLandingAge + SpikeHitInterval;
		if (GetGameSecondsInPhase() < SpikeSettleSeconds)
		{
			return;
		}
		ReportSpikeResult();
	}
	else
	{
		RecordFrame();
		if (GetGameSecondsInPhase() < M_Settings.SettleSeconds)
		{
			return;
		}
		WritePerfResults();
	}
	Finish();
}

void UAddOnArmorBreakTestSubsystem::EnterPhase(const EAddOnArmorBreakTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
	M_PhaseStartGameSeconds = GetGameSeconds();
	M_HitBudget = 0.f;
	UE_LOG(LogRTSAddOnArmorBreakTest, Display, TEXT("RTS_ADDON_ARMOR phase %s at game %.2f"), GetPhaseName(NewPhase), GetGameSeconds());
}

double UAddOnArmorBreakTestSubsystem::GetRealSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartRealSeconds;
}

float UAddOnArmorBreakTestSubsystem::GetGameSecondsInPhase() const
{
	return GetGameSeconds() - M_PhaseStartGameSeconds;
}

float UAddOnArmorBreakTestSubsystem::GetGameSeconds() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

const TCHAR* UAddOnArmorBreakTestSubsystem::GetPhaseName(const EAddOnArmorBreakTestPhase Phase)
{
	switch (Phase)
	{
	case EAddOnArmorBreakTestPhase::WaitingForGameStart:
		return TEXT("waiting");
	case EAddOnArmorBreakTestPhase::Settling:
		return TEXT("settling");
	case EAddOnArmorBreakTestPhase::WarmingUp:
		return TEXT("warmup");
	case EAddOnArmorBreakTestPhase::Idle:
		return TEXT("idle");
	case EAddOnArmorBreakTestPhase::Hitting:
		return TEXT("hitting");
	case EAddOnArmorBreakTestPhase::SettlingDebris:
		return TEXT("settle");
	case EAddOnArmorBreakTestPhase::Done:
		return TEXT("done");
	default:
		return TEXT("inactive");
	}
}

// ----------------------------------------------------------------------------------------------------------------
// Setup
// ----------------------------------------------------------------------------------------------------------------

bool UAddOnArmorBreakTestSubsystem::ReadSettings()
{
	using namespace AddOnArmorBreakTestConstants;
	const TCHAR* const CommandLine = FCommandLine::Get();
	FString ModeText;
	FParse::Value(CommandLine, ModeSwitch, ModeText);
	if (ModeText.Equals(TEXT("Capture"), ESearchCase::IgnoreCase))
	{
		M_Mode = EAddOnArmorBreakTestMode::Capture;
	}
	else if (ModeText.Equals(TEXT("Perf"), ESearchCase::IgnoreCase))
	{
		M_Mode = EAddOnArmorBreakTestMode::Perf;
	}
	else if (not ModeText.Equals(TEXT("Spike"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogRTSAddOnArmorBreakTest, Error, TEXT("RTS_ADDON_ARMOR unknown mode %s (Spike, Capture or Perf)"), *ModeText);
		return false;
	}
	FString TechniqueText;
	if (FParse::Value(CommandLine, TechniqueSwitch, TechniqueText) && not ParseTechnique(TechniqueText, M_Technique))
	{
		UE_LOG(LogRTSAddOnArmorBreakTest, Error, TEXT("RTS_ADDON_ARMOR unknown technique %s"), *TechniqueText);
		return false;
	}
	FString CollisionText;
	if (FParse::Value(CommandLine, GcCollisionSwitch, CollisionText))
	{
		M_GcCollision = CollisionText.Equals(TEXT("OnFirstBreak"), ESearchCase::IgnoreCase)
			                ? EAddOnArmorTestGcCollision::OnFirstBreak
			                : EAddOnArmorTestGcCollision::FromStart;
	}
	FParse::Value(CommandLine, VehiclesSwitch, M_Settings.VehicleCount);
	FParse::Value(CommandLine, HitsPerSecondSwitch, M_Settings.HitsPerSecond);
	FParse::Value(CommandLine, SeedSwitch, M_Settings.Seed);
	FParse::Value(CommandLine, IdleSecondsSwitch, M_Settings.IdleSeconds);
	FParse::Value(CommandLine, HitSecondsSwitch, M_Settings.HitSeconds);
	FParse::Value(CommandLine, SettleSecondsSwitch, M_Settings.SettleSeconds);
	int32 DriveValue = M_Mode == EAddOnArmorBreakTestMode::Capture ? 0 : 1;
	FParse::Value(CommandLine, DriveSwitch, DriveValue);
	M_Settings.bDrive = DriveValue != 0;
	M_Settings.OutputDirectory = FPaths::ProjectSavedDir() / TEXT("AddOnArmorBreak");
	FParse::Value(CommandLine, DirectorySwitch, M_Settings.OutputDirectory);
	M_Settings.Tag = FString::Printf(TEXT("%s_%s_v%d_h%.0f"), *ModeText, GetTechniqueName(M_Technique),
	                                 M_Settings.VehicleCount, M_Settings.HitsPerSecond);
	FParse::Value(CommandLine, TagSwitch, M_Settings.Tag);
	if (M_Mode == EAddOnArmorBreakTestMode::Spike)
	{
		M_Settings.VehicleCount = 1;
		M_Settings.HitsPerSecond = 1.f / SpikeHitInterval;
	}
	FParse::Value(CommandLine, ViewDistanceSwitch, M_Settings.ViewDistance);
	FParse::Value(CommandLine, ViewPitchSwitch, M_Settings.ViewPitch);
	M_Settings.bProfileGpu = FParse::Param(CommandLine, ProfileGpuSwitch);
	FString CaptureTechniquesText = TEXT("Niagara+GeometryCollection");
	FParse::Value(CommandLine, CaptureTechniquesSwitch, CaptureTechniquesText);
	TArray<FString> CaptureTechniqueNames;
	CaptureTechniquesText.ParseIntoArray(CaptureTechniqueNames, TEXT("+"), true);
	for (const FString& Name : CaptureTechniqueNames)
	{
		EAddOnArmorTestTechnique CaptureTechnique;
		if (not ParseTechnique(Name, CaptureTechnique))
		{
			UE_LOG(LogRTSAddOnArmorBreakTest, Error, TEXT("RTS_ADDON_ARMOR unknown capture technique %s"), *Name);
			return false;
		}
		M_Settings.CaptureTechniques.Add(CaptureTechnique);
	}
	return true;
}

bool UAddOnArmorBreakTestSubsystem::TryStartGame()
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
	return true;
}

void UAddOnArmorBreakTestSubsystem::KeepGameRunning() const
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
			Picker->WidgetTree->FindWidget(AddOnArmorBreakTestConstants::DifficultyButtonName));
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

bool UAddOnArmorBreakTestSubsystem::FindBaseLocation()
{
	using namespace AddOnArmorBreakTestConstants;
	bool bHasLocation = false;
	FString LocationText;
	if (FParse::Value(FCommandLine::Get(), LocationSwitch, LocationText, false))
	{
		TArray<FString> Coordinates;
		LocationText.ParseIntoArray(Coordinates, TEXT(","), true);
		if (Coordinates.Num() >= 2)
		{
			M_BaseLocation = FVector(FCString::Atof(*Coordinates[0]), FCString::Atof(*Coordinates[1]), 0.f);
			bHasLocation = true;
		}
	}
	for (TActorIterator<AActor> It(GetWorld()); It && not bHasLocation; ++It)
	{
		if (It->GetClass()->GetName().StartsWith(AnchorClassPrefix))
		{
			M_BaseLocation = It->GetActorLocation();
			bHasLocation = true;
		}
	}
	for (TActorIterator<APlayerStart> It(GetWorld()); It && not bHasLocation; ++It)
	{
		M_BaseLocation = It->GetActorLocation();
		bHasLocation = true;
	}
	if (not bHasLocation)
	{
		UE_LOG(LogRTSAddOnArmorBreakTest, Error, TEXT("RTS_ADDON_ARMOR no -AddOnArmorBreakAt, anchor or player start"));
		return false;
	}
	const FVector TraceOffset(0.f, 0.f, GroundTraceHalfHeight);
	FHitResult GroundHit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AddOnArmorBreakGround), false);
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, M_BaseLocation + TraceOffset, M_BaseLocation - TraceOffset,
	                                         ECC_Visibility, QueryParams))
	{
		M_BaseLocation = GroundHit.ImpactPoint;
	}
	M_GroundHeight = M_BaseLocation.Z;
	UE_LOG(LogRTSAddOnArmorBreakTest, Display, TEXT("RTS_ADDON_ARMOR base %s"), *M_BaseLocation.ToCompactString());
	return true;
}

void UAddOnArmorBreakTestSubsystem::SpawnSkyFill()
{
	float SkyFillIntensity = 0.f;
	if (not FParse::Value(FCommandLine::Get(), AddOnArmorBreakTestConstants::SkyFillSwitch, SkyFillIntensity))
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
	ASkyLight* SkyFill = GetWorld()->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity, SpawnParameters);
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

bool UAddOnArmorBreakTestSubsystem::SpawnVehicles()
{
	using namespace AddOnArmorBreakTestConstants;
	if (M_Mode == EAddOnArmorBreakTestMode::Capture)
	{
		const int32 CaptureCount = M_Settings.CaptureTechniques.Num();
		for (int32 CaptureIndex = 0; CaptureIndex < CaptureCount; ++CaptureIndex)
		{
			const float OffsetX = (CaptureIndex - 0.5f * (CaptureCount - 1)) * CaptureVehicleSpacing;
			if (not IsValid(SpawnVehicle(M_Settings.CaptureTechniques[CaptureIndex], M_BaseLocation + FVector(OffsetX, 0.f, 0.f), 0.f)))
			{
				return false;
			}
		}
		BuildScriptedHits();
		return CaptureCount > 0;
	}
	if (M_Mode == EAddOnArmorBreakTestMode::Spike)
	{
		AAddOnArmorTestVehicle* const Vehicle = SpawnVehicle(M_Technique, M_BaseLocation, 0.f);
		if (IsValid(Vehicle) && M_Settings.bDrive)
		{
			Vehicle->StartDriving(M_BaseLocation, SpikeCircleRadius, SpikeSpeed, 0.f);
		}
		return IsValid(Vehicle);
	}
	const int32 Columns = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(M_Settings.VehicleCount))));
	const float GridHalfSize = 0.5f * (Columns - 1) * PerfGridSpacing;
	FRandomStream LayoutStream(M_Settings.Seed);
	for (int32 VehicleIndex = 0; VehicleIndex < M_Settings.VehicleCount; ++VehicleIndex)
	{
		const FVector CellCentre = M_BaseLocation + FVector((VehicleIndex % Columns) * PerfGridSpacing - GridHalfSize,
		                                                    (VehicleIndex / Columns) * PerfGridSpacing - GridHalfSize, 0.f);
		AAddOnArmorTestVehicle* const Vehicle = SpawnVehicle(M_Technique, CellCentre, LayoutStream.FRandRange(0.f, 360.f));
		if (not IsValid(Vehicle))
		{
			return false;
		}
		if (M_Settings.bDrive)
		{
			Vehicle->StartDriving(CellCentre, PerfCircleRadius, PerfSpeed, LayoutStream.FRandRange(0.f, UE_TWO_PI));
		}
	}
	return true;
}

AAddOnArmorTestVehicle* UAddOnArmorBreakTestSubsystem::SpawnVehicle(const EAddOnArmorTestTechnique Technique,
                                                                     const FVector& Location, const float Yaw)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAddOnArmorTestVehicle* const Vehicle = GetWorld()->SpawnActor<AAddOnArmorTestVehicle>(
		AAddOnArmorTestVehicle::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Location), SpawnParameters);
	if (not IsValid(Vehicle))
	{
		return nullptr;
	}
	if (not Vehicle->SetupSkirts(Technique, M_GcCollision, Location.Z))
	{
		UE_LOG(LogRTSAddOnArmorBreakTest, Error, TEXT("RTS_ADDON_ARMOR vehicle setup failed for %s"),
		       AddOnArmorBreakTestConstants::GetTechniqueName(Technique));
		Vehicle->Destroy();
		return nullptr;
	}
	M_Vehicles.Add(Vehicle);
	return Vehicle;
}

void UAddOnArmorBreakTestSubsystem::CreateCameras()
{
	using namespace AddOnArmorBreakTestConstants;
	// Cameras stand on the +Y side and look towards -Y, at the vehicles' right sides.
	constexpr float LookTowardsMinusY = -90.f;
	const FVector SideLookOffset(0.f, 150.f, 80.f);
	switch (M_Mode)
	{
	case EAddOnArmorBreakTestMode::Capture:
		AddCamera(TEXT("Game"), M_BaseLocation, LookTowardsMinusY, -70.f, 3000.f, 90.f, PictureWidth, PictureHeight);
		AddCamera(TEXT("Side"), M_BaseLocation + SideLookOffset, LookTowardsMinusY, -22.f, 2300.f, 55.f, PictureWidth, PictureHeight);
		for (int32 VehicleIndex = 0; VehicleIndex < M_Vehicles.Num(); ++VehicleIndex)
		{
			if (not M_Vehicles[VehicleIndex].IsValid())
			{
				continue;
			}
			const FString Name = FString::Printf(TEXT("Close%s"), GetTechniqueName(M_Vehicles[VehicleIndex]->GetTechnique()));
			AddCamera(Name, M_Vehicles[VehicleIndex]->GetActorLocation() + SideLookOffset, LookTowardsMinusY + 15.f, -18.f,
			          1100.f, 55.f, PictureWidth, PictureHeight);
		}
		return;
	case EAddOnArmorBreakTestMode::Spike:
		AddCamera(TEXT("Follow"), M_BaseLocation, LookTowardsMinusY, -35.f, 1600.f, 60.f, PictureWidth, PictureHeight);
		return;
	default:
		AddCamera(TEXT("PerfView"), M_BaseLocation, 0.f, M_Settings.ViewPitch, M_Settings.ViewDistance, 90.f, PerfViewWidth, PerfViewHeight);
		return;
	}
}

void UAddOnArmorBreakTestSubsystem::AddCamera(const FString& Name, const FVector& LookAt, const float Yaw, const float Pitch,
                                              const float Distance, const float FieldOfView, const int32 Width, const int32 Height)
{
	using namespace AddOnArmorBreakTestConstants;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASceneCapture2D* const Capture = GetWorld()->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform::Identity, SpawnParameters);
	UTextureRenderTarget2D* const RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), Width, Height, RTF_RGBA8_SRGB);
	if (not IsValid(Capture) || not IsValid(RenderTarget))
	{
		return;
	}
	USceneCaptureComponent2D* const CaptureComponent = Capture->GetCaptureComponent2D();
	CaptureComponent->TextureTarget = RenderTarget;
	CaptureComponent->CaptureSource = SCS_FinalColorLDR;
	CaptureComponent->FOVAngle = FieldOfView;
	// Rendering every frame with kept state lets exposure and temporal AA settle; in perf runs it is the game view.
	CaptureComponent->bCaptureEveryFrame = true;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	// The fog of war is a post-process material; the pictures show the debris, not what the player may see.
	CaptureComponent->ShowFlags.SetPostProcessMaterial(false);
	FPostProcessSettings& Settings = CaptureComponent->PostProcessSettings;
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
	CaptureComponent->PostProcessBlendWeight = 1.f;
	FAddOnArmorCaptureCamera& Camera = M_Cameras.AddDefaulted_GetRef();
	Camera.Name = Name;
	Camera.LookAt = LookAt;
	Camera.Yaw = Yaw;
	Camera.Pitch = Pitch;
	Camera.Distance = Distance;
	Camera.FieldOfView = FieldOfView;
	Camera.Capture = Capture;
	Camera.RenderTarget.Reset(RenderTarget);
}

void UAddOnArmorBreakTestSubsystem::AimCameras() const
{
	const bool bFollowsVehicle = M_Mode == EAddOnArmorBreakTestMode::Spike && M_Vehicles.Num() > 0 && M_Vehicles[0].IsValid();
	for (const FAddOnArmorCaptureCamera& Camera : M_Cameras)
	{
		if (not Camera.Capture.IsValid())
		{
			continue;
		}
		const FVector LookAt = bFollowsVehicle ? M_Vehicles[0]->GetActorLocation() : Camera.LookAt;
		const FRotator CameraRotation(Camera.Pitch, Camera.Yaw, 0.f);
		Camera.Capture->SetActorLocationAndRotation(LookAt - CameraRotation.Vector() * Camera.Distance, CameraRotation);
	}
}

void UAddOnArmorBreakTestSubsystem::ExportCameras(const FString& Label) const
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const FString Directory = M_Settings.OutputDirectory / M_Settings.Tag;
	for (const FAddOnArmorCaptureCamera& Camera : M_Cameras)
	{
		if (not Camera.RenderTarget.IsValid())
		{
			continue;
		}
		UKismetRenderingLibrary::ExportRenderTarget(World, Camera.RenderTarget.Get(), Directory,
		                                            FString::Printf(TEXT("%s_%s.png"), *Camera.Name, *Label));
	}
}

// ----------------------------------------------------------------------------------------------------------------
// Capture
// ----------------------------------------------------------------------------------------------------------------

void UAddOnArmorBreakTestSubsystem::BuildScriptedHits()
{
	using namespace AddOnArmorBreakTest;
	using namespace AddOnArmorBreakTestConstants;
	const float RightFaceY = SideOffsetY;
	const auto PanelX = [](const int32 PanelIndex)
	{
		return RailCentreX + GetPanelTopCentre(PanelIndex).X;
	};
	// A 75 mm AP shell from the front right, a 105 mm HE shell, then a short burst of 20 mm.
	M_ScriptedHits = {
		{TEXT("AP75"), 2.f, FVector(PanelX(1) + 10.f, RightFaceY, 112.f), FVector(-0.35f, -1.f, -0.15f).GetSafeNormal(), 22.f, 450.f, 5},
		{TEXT("HE105"), 8.f, FVector(PanelX(3), RightFaceY, 95.f), FVector(0.2f, -1.f, -0.3f).GetSafeNormal(), 48.f, 650.f, 12},
		{TEXT("MG20"), 14.f, FVector(PanelX(4) - 25.f, RightFaceY, 130.f), FVector(0.f, -1.f, -0.1f).GetSafeNormal(), 6.f, 300.f, 2},
		{TEXT("MG20b"), 14.15f, FVector(PanelX(4), RightFaceY, 100.f), FVector(0.05f, -1.f, -0.1f).GetSafeNormal(), 6.f, 300.f, 2},
		{TEXT("MG20c"), 14.3f, FVector(PanelX(4) + 20.f, RightFaceY, 80.f), FVector(-0.05f, -1.f, -0.1f).GetSafeNormal(), 6.f, 300.f, 2}
	};
	M_PictureMoments.Reset();
	float LastMoment = 0.f;
	for (const FAddOnArmorScriptedHit& ScriptedHit : M_ScriptedHits)
	{
		// Follow-up rounds of a burst share the first round's pictures.
		if (ScriptedHit.Name.Len() > 4 && ScriptedHit.Name.StartsWith(TEXT("MG20")))
		{
			continue;
		}
		for (const float Offset : CaptureMomentOffsets)
		{
			const FString Label = FString::Printf(TEXT("%s_t%+.2f"), *ScriptedHit.Name, Offset);
			M_PictureMoments.Add(TPair<float, FString>(ScriptedHit.GameSeconds + Offset, Label));
			LastMoment = FMath::Max(LastMoment, ScriptedHit.GameSeconds + Offset);
		}
	}
	M_PictureMoments.Add(TPair<float, FString>(LastMoment + CaptureEndPadding, TEXT("final")));
	M_PictureMoments.Sort([](const TPair<float, FString>& Left, const TPair<float, FString>& Right)
	{
		return Left.Key < Right.Key;
	});
}

void UAddOnArmorBreakTestSubsystem::TickCapture()
{
	const float SecondsInPhase = GetGameSecondsInPhase();
	while (M_ScriptedHits.IsValidIndex(M_NextScriptedHit) && M_ScriptedHits[M_NextScriptedHit].GameSeconds <= SecondsInPhase)
	{
		FireScriptedHit(M_ScriptedHits[M_NextScriptedHit]);
		++M_NextScriptedHit;
	}
	while (M_PictureMoments.IsValidIndex(M_NextPictureMoment) && M_PictureMoments[M_NextPictureMoment].Key <= SecondsInPhase)
	{
		ExportCameras(M_PictureMoments[M_NextPictureMoment].Value);
		++M_NextPictureMoment;
	}
	if (not M_PictureMoments.IsValidIndex(M_NextPictureMoment))
	{
		for (const TWeakObjectPtr<AAddOnArmorTestVehicle>& Vehicle : M_Vehicles)
		{
			if (not Vehicle.IsValid())
			{
				continue;
			}
			const FAddOnArmorTestVehicleStats& Stats = Vehicle->GetStats();
			UE_LOG(LogRTSAddOnArmorBreakTest, Display,
			       TEXT("RTS_ADDON_ARMOR capture %s hits=%d missed=%d released=%d break_events=%d unexpected=%d bursts=%d"),
			       AddOnArmorBreakTestConstants::GetTechniqueName(Vehicle->GetTechnique()), Stats.HitCount, Stats.HitsThatMissed,
			       Stats.PiecesReleased, Stats.MatchingBreakEvents, Stats.UnexpectedBreakEvents, Stats.NiagaraBursts);
		}
		Finish();
	}
}

void UAddOnArmorBreakTestSubsystem::FireScriptedHit(const FAddOnArmorScriptedHit& ScriptedHit)
{
	for (const TWeakObjectPtr<AAddOnArmorTestVehicle>& Vehicle : M_Vehicles)
	{
		if (not Vehicle.IsValid())
		{
			continue;
		}
		const FTransform VehicleTransform = Vehicle->GetActorTransform();
		FAddOnArmorTestHit Hit;
		Hit.WorldLocation = VehicleTransform.TransformPosition(ScriptedHit.LocalLocation);
		Hit.WorldDirection = VehicleTransform.TransformVectorNoScale(ScriptedHit.LocalDirection);
		Hit.BreakRadius = ScriptedHit.BreakRadius;
		Hit.PushSpeed = ScriptedHit.PushSpeed;
		Hit.MaxPieces = ScriptedHit.MaxPieces;
		Hit.Seed = GetTypeHash(ScriptedHit.Name);
		const int32 Released = Vehicle->ApplyTestHit(Hit);
		UE_LOG(LogRTSAddOnArmorBreakTest, Display, TEXT("RTS_ADDON_ARMOR capture hit %s on %s released=%d"), *ScriptedHit.Name,
		       AddOnArmorBreakTestConstants::GetTechniqueName(Vehicle->GetTechnique()), Released);
	}
}

// ----------------------------------------------------------------------------------------------------------------
// Random hits, frame recording and results
// ----------------------------------------------------------------------------------------------------------------

void UAddOnArmorBreakTestSubsystem::FireRandomHits(const float DeltaTime)
{
	M_HitBudget += M_Settings.HitsPerSecond * DeltaTime;
	if (M_Mode == EAddOnArmorBreakTestMode::Spike && M_HitSerial == 0 && M_HitBudget < 1.f)
	{
		// The spike's first hit lands right away.
		M_HitBudget = 1.f;
	}
	while (M_HitBudget >= 1.f)
	{
		M_HitBudget -= 1.f;
		FireRandomHit();
		if (M_Mode != EAddOnArmorBreakTestMode::Spike)
		{
			continue;
		}
		for (const float Offset : AddOnArmorBreakTestConstants::SpikePictureOffsets)
		{
			M_SpikePictures.Add(TPair<float, FString>(GetGameSeconds() + Offset,
			                                          FString::Printf(TEXT("hit%d_t%+.2f"), M_HitSerial, Offset)));
		}
	}
}

bool UAddOnArmorBreakTestSubsystem::FireRandomHit()
{
	using namespace AddOnArmorBreakTestConstants;
	if (M_Vehicles.IsEmpty())
	{
		return false;
	}
	++M_HitSerial;
	AAddOnArmorTestVehicle* const Vehicle = M_Vehicles[M_HitStream.RandRange(0, M_Vehicles.Num() - 1)].Get();
	if (not IsValid(Vehicle))
	{
		return false;
	}
	const int32 SideIndex = M_HitStream.RandRange(0, AddOnArmorBreakTest::SideCount - 1);
	FVector PieceLocation;
	if (not Vehicle->PickIntactPieceLocation(SideIndex, M_HitStream, PieceLocation))
	{
		// No skirt left on that side; a None vehicle has none at all and still counts the hit.
		++M_HitsWithoutTarget;
		PieceLocation = Vehicle->GetActorLocation();
	}
	float KindRoll = M_HitStream.FRand();
	const FHitKind* Kind = &HitKinds[0];
	for (const FHitKind& Candidate : HitKinds)
	{
		Kind = &Candidate;
		KindRoll -= Candidate.Weight;
		if (KindRoll <= 0.f)
		{
			break;
		}
	}
	const FVector Outward = Vehicle->GetSideOutwardNormal(SideIndex);
	const FRotator ShellRotation(M_HitStream.FRandRange(MinShellPitchDegrees, MaxShellPitchDegrees),
	                             (-Outward).Rotation().Yaw + M_HitStream.FRandRange(-MaxShellYawDegrees, MaxShellYawDegrees), 0.f);
	const FVector Lateral = FVector::CrossProduct(Outward, FVector::UpVector);
	FAddOnArmorTestHit Hit;
	Hit.WorldDirection = ShellRotation.Vector();
	Hit.WorldLocation = PieceLocation + Lateral * M_HitStream.FRandRange(-HitOffsetOnPiece, HitOffsetOnPiece)
		+ FVector::UpVector * M_HitStream.FRandRange(-HitOffsetOnPiece, HitOffsetOnPiece);
	Hit.BreakRadius = Kind->BreakRadius;
	Hit.PushSpeed = Kind->PushSpeed;
	Hit.MaxPieces = Kind->MaxPieces;
	Hit.Seed = M_HitSerial;
	Vehicle->ApplyTestHit(Hit);
	return true;
}

void UAddOnArmorBreakTestSubsystem::RecordFrame()
{
	using namespace AddOnArmorBreakTestConstants;
	if (M_LastTickRealSeconds <= 0.0)
	{
		return;
	}
	const float FrameMs = static_cast<float>((FPlatformTime::Seconds() - M_LastTickRealSeconds) * 1000.0);
	const float GameThreadMs = CyclesToMs(GGameThreadTime);
	const float RenderThreadMs = CyclesToMs(GRenderThreadTime);
	const float RhiThreadMs = CyclesToMs(GRHIThreadTime);
	const float GpuMs = CyclesToMs(RHIGetGPUFrameCycles(0));
	constexpr float PercentPerCore = 100.f;
	const float CpuCores = FPlatformTime::GetCPUTime().CPUTimePctRelative / PercentPerCore;
	int32 LiveDebris = 0;
	int32 Particles = 0;
	TSet<const UNiagaraComponent*> ActiveBursts;
	for (const TWeakObjectPtr<AAddOnArmorTestVehicle>& Vehicle : M_Vehicles)
	{
		if (Vehicle.IsValid())
		{
			LiveDebris += Vehicle->CountLiveDebrisPieces();
			Vehicle->AppendActiveBursts(ActiveBursts);
		}
	}
	for (const UNiagaraComponent* const Burst : ActiveBursts)
	{
		Particles += AAddOnArmorTestVehicle::CountParticles(*Burst);
	}
	const FString PhaseName = GetPhaseName(M_Phase);
	FAddOnArmorPerfPhaseStats& Stats = M_PhaseStats.FindOrAdd(PhaseName);
	Stats.FrameMs.Add(FrameMs);
	Stats.GameThreadMs.Add(GameThreadMs);
	Stats.RenderThreadMs.Add(RenderThreadMs);
	Stats.RhiThreadMs.Add(RhiThreadMs);
	Stats.GpuMs.Add(GpuMs);
	Stats.CpuCores.Add(CpuCores);
	Stats.MaxLiveDebris = FMath::Max(Stats.MaxLiveDebris, LiveDebris);
	Stats.MaxParticles = FMath::Max(Stats.MaxParticles, Particles);
	M_FrameCsv += FString::Printf(TEXT("%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%d,%d\n"), *PhaseName, GetGameSeconds(), FrameMs,
	                              GameThreadMs, RenderThreadMs, RhiThreadMs, GpuMs, CpuCores, LiveDebris, Particles);
}

void UAddOnArmorBreakTestSubsystem::WritePerfResults()
{
	using namespace AddOnArmorBreakTestConstants;
	const FString CsvPath = M_Settings.OutputDirectory / (M_Settings.Tag + TEXT("_frames.csv"));
	FFileHelper::SaveStringToFile(TEXT("phase,game_s,frame_ms,game_thread_ms,render_thread_ms,rhi_thread_ms,gpu_ms,cpu_cores,live_debris,particles\n")
	                              + M_FrameCsv, *CsvPath);
	int32 PiecesReleased = 0;
	int32 IntactPieces = 0;
	for (const TWeakObjectPtr<AAddOnArmorTestVehicle>& Vehicle : M_Vehicles)
	{
		if (Vehicle.IsValid())
		{
			PiecesReleased += Vehicle->GetStats().PiecesReleased;
			IntactPieces += Vehicle->GetIntactPieceCount();
		}
	}
	const float UsedMegabytes = static_cast<float>(FPlatformMemory::GetStats().UsedPhysical) / BytesPerMegabyte;
	for (const TPair<FString, FAddOnArmorPerfPhaseStats>& PhaseStats : M_PhaseStats)
	{
		const FAddOnArmorPerfPhaseStats& Stats = PhaseStats.Value;
		UE_LOG(LogRTSAddOnArmorBreakTest, Display,
		       TEXT("RTS_ADDON_ARMOR_PERF tag=%s technique=%s vehicles=%d hits_per_s=%.1f phase=%s frames=%d frame_avg=%.2f frame_p95=%.2f ")
		       TEXT("game_avg=%.2f game_p95=%.2f render_avg=%.2f rhi_avg=%.2f gpu_avg=%.2f gpu_p95=%.2f cpu_cores=%.2f max_debris=%d max_particles=%d"),
		       *M_Settings.Tag, GetTechniqueName(M_Technique), M_Settings.VehicleCount, M_Settings.HitsPerSecond, *PhaseStats.Key,
		       Stats.FrameMs.Num(), GetAverage(Stats.FrameMs), GetPercentile(Stats.FrameMs, PercentileHigh),
		       GetAverage(Stats.GameThreadMs), GetPercentile(Stats.GameThreadMs, PercentileHigh),
		       GetAverage(Stats.RenderThreadMs), GetAverage(Stats.RhiThreadMs), GetAverage(Stats.GpuMs),
		       GetPercentile(Stats.GpuMs, PercentileHigh), GetAverage(Stats.CpuCores), Stats.MaxLiveDebris, Stats.MaxParticles);
	}
	UE_LOG(LogRTSAddOnArmorBreakTest, Display,
	       TEXT("RTS_ADDON_ARMOR_PERF tag=%s summary hits=%d no_target=%d released=%d intact_left=%d used_mb=%.0f csv=%s"),
	       *M_Settings.Tag, M_HitSerial, M_HitsWithoutTarget, PiecesReleased, IntactPieces, UsedMegabytes, *CsvPath);
}

float UAddOnArmorBreakTestSubsystem::GetAverage(const TArray<float>& Values)
{
	if (Values.IsEmpty())
	{
		return 0.f;
	}
	double Sum = 0.0;
	for (const float Value : Values)
	{
		Sum += Value;
	}
	return static_cast<float>(Sum / Values.Num());
}

float UAddOnArmorBreakTestSubsystem::GetPercentile(TArray<float> Values, const float Percentile)
{
	if (Values.IsEmpty())
	{
		return 0.f;
	}
	Values.Sort();
	const int32 Index = FMath::Clamp(FMath::FloorToInt(Percentile * (Values.Num() - 1)), 0, Values.Num() - 1);
	return Values[Index];
}

// ----------------------------------------------------------------------------------------------------------------
// Spike
// ----------------------------------------------------------------------------------------------------------------

void UAddOnArmorBreakTestSubsystem::CheckSpikeReleases()
{
	if (M_Vehicles.IsEmpty() || not M_Vehicles[0].IsValid())
	{
		return;
	}
	M_Vehicles[0]->CheckReleasedPieces(AddOnArmorBreakTestConstants::SpikeLandingAge, M_SpikeCounts);
}

void UAddOnArmorBreakTestSubsystem::ExportDueSpikePictures()
{
	M_SpikePictures.Sort([](const TPair<float, FString>& Left, const TPair<float, FString>& Right)
	{
		return Left.Key < Right.Key;
	});
	while (M_SpikePictures.Num() > 0 && M_SpikePictures[0].Key <= GetGameSeconds())
	{
		ExportCameras(M_SpikePictures[0].Value);
		M_SpikePictures.RemoveAt(0);
	}
}

void UAddOnArmorBreakTestSubsystem::ReportSpikeResult()
{
	using namespace AddOnArmorBreakTestConstants;
	if (M_Vehicles.IsEmpty() || not M_Vehicles[0].IsValid())
	{
		UE_LOG(LogRTSAddOnArmorBreakTest, Error, TEXT("RTS_ADDON_ARMOR_SPIKE RESULT FAIL no vehicle"));
		return;
	}
	const AAddOnArmorTestVehicle* const Vehicle = M_Vehicles[0].Get();
	const FAddOnArmorTestVehicleStats& Stats = Vehicle->GetStats();
	const FAddOnArmorTestSpikeCounts& Counts = M_SpikeCounts;
	const int32 MissingBreakEvents = Vehicle->CountReleasesWithoutBreakEvent(SpikeBreakEventWait);
	const int32 DisplacedIntact = Vehicle->CountDisplacedIntactPieces(SpikeIntactTolerance);
	bool bPassed = Stats.PiecesReleased > 0;
	if (Vehicle->GetTechnique() == EAddOnArmorTestTechnique::GeometryCollection)
	{
		bPassed = bPassed && Counts.Landed > 0 && Counts.StuckOnHull == 0 && Counts.FellThroughGround == 0
			&& MissingBreakEvents == 0 && Stats.UnexpectedBreakEvents == 0 && DisplacedIntact == 0;
	}
	else
	{
		bPassed = bPassed && Counts.BurstsWithAllPieces > 0 && Counts.BurstsWithWrongCount == 0;
	}
	UE_LOG(LogRTSAddOnArmorBreakTest, Display,
	       TEXT("RTS_ADDON_ARMOR_SPIKE RESULT %s technique=%s collision=%d hits=%d missed=%d released=%d landed=%d airborne=%d ")
	       TEXT("fell_through=%d stuck_on_hull=%d break_events=%d unexpected=%d missing_events=%d displaced_intact=%d bursts_ok=%d bursts_bad=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), GetTechniqueName(Vehicle->GetTechnique()), static_cast<int32>(M_GcCollision),
	       Stats.HitCount, Stats.HitsThatMissed, Stats.PiecesReleased, Counts.Landed, Counts.StillAirborne,
	       Counts.FellThroughGround, Counts.StuckOnHull, Stats.MatchingBreakEvents, Stats.UnexpectedBreakEvents,
	       MissingBreakEvents, DisplacedIntact, Counts.BurstsWithAllPieces, Counts.BurstsWithWrongCount);
}

void UAddOnArmorBreakTestSubsystem::Finish()
{
	M_Phase = EAddOnArmorBreakTestPhase::Done;
	UE_LOG(LogRTSAddOnArmorBreakTest, Display, TEXT("RTS_ADDON_ARMOR done"));
	FPlatformMisc::RequestExit(false, TEXT("AddOnArmorBreakTest"));
}

#else

bool UAddOnArmorBreakTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UAddOnArmorBreakTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UAddOnArmorBreakTestSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UAddOnArmorBreakTestSubsystem::Tick(float DeltaTime)
{
}

TStatId UAddOnArmorBreakTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAddOnArmorBreakTestSubsystem, STATGROUP_Tickables);
}

#endif // RTS_WITH_MAP_TESTS
