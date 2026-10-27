// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TankTrailBenchmarkSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "RenderTimer.h"
#include "DynamicRHI.h"
#include "RTS_Survival/Environment/TankTrails/TankTrailSubsystem.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSTankTrailBench, Log, All);

namespace TankTrailBenchPrivate
{
	const TCHAR* const BenchmarkSwitch = TEXT("TankTrailBench");
	const TCHAR* const DirectorySwitch = TEXT("TankTrailBenchDir=");
	const TCHAR* const CountSwitch = TEXT("TankTrailBenchCount=");
	const TCHAR* const FramesSwitch = TEXT("TankTrailBenchFrames=");
	const TCHAR* const RepeatsSwitch = TEXT("TankTrailBenchRepeats=");
	const TCHAR* const LocationSwitch = TEXT("TankTrailBenchAt=");
	const TCHAR* const ProfileGpuSwitch = TEXT("TankTrailBenchProfileGPU");
	const TCHAR* const TrailConsoleVariable = TEXT("rts.TankTrails");

	const TCHAR* const TankClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H.Ger_Vehicle_PanzerIV_H_C");
	constexpr uint8 PlayerOwner = 1;

	constexpr int32 DefaultTankCount = 40;
	// Many short off/on pairs: load from other programs drifts over seconds, a pair sees the same drift.
	constexpr int32 DefaultFramesPerPhase = 240;
	constexpr int32 DefaultPhaseRepeats = 10;
	// With -TankTrailBenchProfileGPU one frame per phase dumps its GPU pass timings (ProfileGPU) to the log; that
	// frame and the hitch after it are not measured.
	constexpr int32 ProfileGpuFrame = 120;
	constexpr int32 ProfileGpuSettleFrames = 10;
	// Open grass next to the player start on Ger_Trains.
	const FVector DefaultDriveCentre(13250.f, -28000.f, 0.f);

	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 120.0;
	// Ger_Trains never reports its navmesh idle (moving trains and crop fields keep dirtying modifier tiles); the
	// built tiles path fine, so after this long the tanks are spawned anyway.
	constexpr double MaxNavigationSeconds = 60.0;
	constexpr int32 NavigationQuietFrames = 60;
	constexpr double MaxTankLoadSeconds = 90.0;
	constexpr int32 WarmUpFrames = 450;
	// Long enough for the capture's auto exposure to settle.
	constexpr int32 PictureFrames = 150;

	// Tanks shuttle along parallel lanes in world Y.
	constexpr int32 LaneCount = 8;
	constexpr float LaneSpacing = 700.f;
	constexpr float HalfLaneLength = 4000.f;
	constexpr float TankSpacingInLane = 900.f;
	constexpr float ArrivalDistance = 600.f;
	// Units the map placed this close to the drive centre are removed, so mission enemies do not fight the tanks.
	constexpr float ClearUnitsRadius = 12000.f;
	// A tank stuck behind another one gets the other lane end after this many frames.
	constexpr int32 MaxFramesPerOrder = 900;
	constexpr int32 FramesBetweenArrivalChecks = 30;
	const FVector NavExtent(1500.f, 1500.f, 3000.f);

	// The benchmark view: high over the lanes, looking down like a zoomed out RTS camera.
	constexpr float CameraDistance = 7000.f;
	constexpr float CameraPitch = -55.f;
	constexpr float CameraFieldOfView = 90.f;

	constexpr double MillisecondsPerSecond = 1000.0;

	float GetPercentile(const TArray<float>& SortedValues, const float Percentile)
	{
		if (SortedValues.IsEmpty())
		{
			return 0.f;
		}
		const int32 Index = FMath::Clamp(FMath::FloorToInt(Percentile * (SortedValues.Num() - 1)), 0,
		                                 SortedValues.Num() - 1);
		return SortedValues[Index];
	}

	float GetMean(const TArray<float>& Values)
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

	/** Mean, median, p95 and p99 of a metric, e.g. "wall mean=16.2 p50=16.0 p95=18.1 p99=21.4". */
	FString DescribeMetric(const TCHAR* Name, const TArray<float>& Values)
	{
		TArray<float> Sorted = Values;
		Sorted.Sort();
		return FString::Printf(TEXT("%s mean=%.3f p50=%.3f p95=%.3f p99=%.3f"), Name, GetMean(Values),
		                       GetPercentile(Sorted, 0.5f), GetPercentile(Sorted, 0.95f),
		                       GetPercentile(Sorted, 0.99f));
	}

	void AppendAll(TArray<float>& Target, const TArray<float>& Source)
	{
		Target.Append(Source);
	}
}

bool UTankTrailBenchmarkSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TankTrailBenchPrivate::BenchmarkSwitch);
}

void UTankTrailBenchmarkSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	using namespace TankTrailBenchPrivate;
	Super::OnWorldBeginPlay(InWorld);
	M_OutputDirectory = FPaths::ProjectSavedDir() / TEXT("TankTrailBench");
	FParse::Value(FCommandLine::Get(), DirectorySwitch, M_OutputDirectory);
	M_TankCount = DefaultTankCount;
	FParse::Value(FCommandLine::Get(), CountSwitch, M_TankCount);
	M_FramesPerPhase = DefaultFramesPerPhase;
	FParse::Value(FCommandLine::Get(), FramesSwitch, M_FramesPerPhase);
	M_PhaseRepeats = DefaultPhaseRepeats;
	FParse::Value(FCommandLine::Get(), RepeatsSwitch, M_PhaseRepeats);
	M_DriveCentre = DefaultDriveCentre;
	FString LocationText;
	// "X,Y": the comma must not end the value.
	if (FParse::Value(FCommandLine::Get(), LocationSwitch, LocationText, false))
	{
		FString XText;
		FString YText;
		if (LocationText.Split(TEXT(","), &XText, &YText))
		{
			M_DriveCentre = FVector(FCString::Atof(*XText), FCString::Atof(*YText), 0.f);
		}
	}
	// Off, on, off, on...: background load on the machine lands on both alike.
	for (int32 RepeatIndex = 0; RepeatIndex < M_PhaseRepeats; ++RepeatIndex)
	{
		M_Phases.AddDefaulted_GetRef().bTrailsOn = false;
		M_Phases.AddDefaulted_GetRef().bTrailsOn = true;
	}
	// An error dialog costs milliseconds in an editor build; a repeating error would be measured instead of the trails.
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	EnterPhase(ETankTrailBenchPhase::WaitingForGameStart);
	UE_LOG(LogRTSTankTrailBench, Display,
	       TEXT("RTS_TANK_TRAIL_BENCH started: tanks=%d frames_per_phase=%d repeats=%d centre=%s output=%s"),
	       M_TankCount, M_FramesPerPhase, M_PhaseRepeats, *M_DriveCentre.ToCompactString(), *M_OutputDirectory);
}

void UTankTrailBenchmarkSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

void UTankTrailBenchmarkSubsystem::Tick(float DeltaTime)
{
	if (M_Phase == ETankTrailBenchPhase::Inactive || M_Phase == ETankTrailBenchPhase::Done || not IsValid(GetWorld()))
	{
		return;
	}
	if (M_Phase != ETankTrailBenchPhase::WaitingForGameStart)
	{
		FGrenadeTestHelpers::KeepGameRunning(*GetWorld(), M_PlayerController.Get());
		KeepEngineUnpaused();
		HoldCamera();
	}
	++M_FramesInPhase;
	switch (M_Phase)
	{
	case ETankTrailBenchPhase::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case ETankTrailBenchPhase::WaitingForNavigation:
		TickWaitingForNavigation();
		break;
	case ETankTrailBenchPhase::WaitingForTanks:
		TickWaitingForTanks();
		break;
	case ETankTrailBenchPhase::WarmingUp:
		TickWarmingUp();
		break;
	case ETankTrailBenchPhase::Measuring:
		TickMeasuring();
		break;
	case ETankTrailBenchPhase::TakingPicture:
		TickTakingPicture();
		break;
	default:
		break;
	}
}

TStatId UTankTrailBenchmarkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTankTrailBenchmarkSubsystem, STATGROUP_Tickables);
}

void UTankTrailBenchmarkSubsystem::EnterPhase(const ETankTrailBenchPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
	M_LastFrameSeconds = M_PhaseStartRealSeconds;
	M_FramesInPhase = 0;
}

double UTankTrailBenchmarkSubsystem::GetRealSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartRealSeconds;
}

void UTankTrailBenchmarkSubsystem::Finish(const bool bSucceeded, const FString& Reason)
{
	M_Phase = ETankTrailBenchPhase::Done;
	UE_LOG(LogRTSTankTrailBench, Display, TEXT("RTS_TANK_TRAIL_BENCH RESULT %s %s"),
	       bSucceeded ? TEXT("PASS") : TEXT("FAIL"), *Reason);
	FPlatformMisc::RequestExit(false, TEXT("TankTrailBench"));
}

void UTankTrailBenchmarkSubsystem::TickWaitingForGameStart()
{
	using namespace TankTrailBenchPrivate;
	if (GetRealSecondsInPhase() > MaxStartSeconds)
	{
		Finish(false, TEXT("the game did not start"));
		return;
	}
	if (GetRealSecondsInPhase() < HudBuildSeconds ||
		not FGrenadeTestHelpers::TryStartGame(*GetWorld(), M_PlayerController))
	{
		return;
	}
	SetTrailsOn(false);
	SpawnCamera();
	EnterPhase(ETankTrailBenchPhase::WaitingForNavigation);
}

void UTankTrailBenchmarkSubsystem::TickWaitingForNavigation()
{
	using namespace TankTrailBenchPrivate;
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const bool bIsBuilding = IsValid(NavigationSystem) && NavigationSystem->IsNavigationBuildInProgress();
	const bool bHasWaitedLongEnough = GetRealSecondsInPhase() > MaxNavigationSeconds;
	if (bIsBuilding && not bHasWaitedLongEnough)
	{
		M_FramesInPhase = 0;
		return;
	}
	if (not bIsBuilding && M_FramesInPhase < NavigationQuietFrames)
	{
		return;
	}
	UE_LOG(LogRTSTankTrailBench, Display,
	       TEXT("RTS_TANK_TRAIL_BENCH navigation ready after %.1f s (still building: %s)"), GetRealSecondsInPhase(),
	       bIsBuilding ? TEXT("yes") : TEXT("no"));
	SpawnTanks();
	EnterPhase(ETankTrailBenchPhase::WaitingForTanks);
}

void UTankTrailBenchmarkSubsystem::TickWaitingForTanks()
{
	using namespace TankTrailBenchPrivate;
	const int32 LivingTanks = CountLivingTanks();
	if (LivingTanks == 0 && GetRealSecondsInPhase() > MaxTankLoadSeconds)
	{
		Finish(false, TEXT("no tank could be spawned at the drive centre"));
		return;
	}
	if (GetRealSecondsInPhase() < HudBuildSeconds)
	{
		return;
	}
	for (FTankTrailBenchTank& BenchTank : M_Tanks)
	{
		OrderTankAlongLane(BenchTank);
	}
	// Warm up with trails on: shaders, the trail target and the tank movement all settle before measuring.
	SetTrailsOn(true);
	EnterPhase(ETankTrailBenchPhase::WarmingUp);
}

void UTankTrailBenchmarkSubsystem::TickWarmingUp()
{
	DriveTanks();
	if (M_FramesInPhase < TankTrailBenchPrivate::WarmUpFrames)
	{
		return;
	}
	EnterPhase(ETankTrailBenchPhase::Measuring);
	StartMeasuredPhase(0);
}

void UTankTrailBenchmarkSubsystem::TickMeasuring()
{
	DriveTanks();
	const double NowSeconds = FPlatformTime::Seconds();
	const double FrameMs = (NowSeconds - M_LastFrameSeconds) * TankTrailBenchPrivate::MillisecondsPerSecond;
	M_LastFrameSeconds = NowSeconds;
	FTankTrailBenchPhase& Phase = M_Phases[M_PhaseIndex];
	// The first frame of a phase still carries the console switch; it is not measured.
	const bool bProfilesGpu = FParse::Param(FCommandLine::Get(), TankTrailBenchPrivate::ProfileGpuSwitch);
	if (bProfilesGpu && M_FramesInPhase == TankTrailBenchPrivate::ProfileGpuFrame && GEngine != nullptr)
	{
		UE_LOG(LogRTSTankTrailBench, Display, TEXT("RTS_TANK_TRAIL_BENCH profilegpu phase=%d trails=%s"),
		       M_PhaseIndex, Phase.bTrailsOn ? TEXT("on") : TEXT("off"));
		GEngine->Exec(GetWorld(), TEXT("ProfileGPU"));
	}
	const bool bIsProfileHitch = bProfilesGpu && M_FramesInPhase >= TankTrailBenchPrivate::ProfileGpuFrame &&
		M_FramesInPhase < TankTrailBenchPrivate::ProfileGpuFrame + TankTrailBenchPrivate::ProfileGpuSettleFrames;
	if (M_FramesInPhase > 1 && not bIsProfileHitch)
	{
		RecordFrame(Phase, FrameMs);
	}
	if (Phase.WallMs.Num() < M_FramesPerPhase)
	{
		return;
	}
	FinishMeasuredPhase(Phase);
	ReportPhase(M_PhaseIndex);
	if (M_Phases.IsValidIndex(M_PhaseIndex + 1))
	{
		M_FramesInPhase = 0;
		StartMeasuredPhase(M_PhaseIndex + 1);
		return;
	}
	ReportSummary();
	SaveFrameCsv();
	SetTrailsOn(true);
	EnterPhase(ETankTrailBenchPhase::TakingPicture);
}

void UTankTrailBenchmarkSubsystem::TickTakingPicture()
{
	DriveTanks();
	if (M_FramesInPhase == 1)
	{
		ASceneCapture2D* Capture = nullptr;
		UTextureRenderTarget2D* CaptureTarget = nullptr;
		if (FGrenadeTestHelpers::CreateCaptureCamera(*GetWorld(), TankTrailBenchPrivate::CameraFieldOfView, Capture,
		                                             CaptureTarget))
		{
			M_Capture = Capture;
			M_CaptureTarget = CaptureTarget;
			FGrenadeTestHelpers::AimLikeGameCamera(*Capture, M_DriveCentre);
		}
		return;
	}
	if (M_FramesInPhase < TankTrailBenchPrivate::PictureFrames)
	{
		return;
	}
	SaveTrailPictures();
	Finish(true, FString::Printf(TEXT("output %s"), *M_OutputDirectory));
}

void UTankTrailBenchmarkSubsystem::SpawnTanks()
{
	using namespace TankTrailBenchPrivate;
	UWorld& World = *GetWorld();
	FVector Centre = M_DriveCentre;
	FGrenadeTestHelpers::DropToGround(World, Centre);
	M_DriveCentre = Centre;
	ClearMapUnitsAroundLanes();
	const int32 TanksPerLane = FMath::DivideAndRoundUp(M_TankCount, LaneCount);
	for (int32 TankIndex = 0; TankIndex < M_TankCount; ++TankIndex)
	{
		const int32 LaneIndex = TankIndex % LaneCount;
		const int32 SlotInLane = TankIndex / LaneCount;
		const float LaneX = Centre.X + (LaneIndex - (LaneCount - 1) * 0.5f) * LaneSpacing;
		FVector LaneStart(LaneX, Centre.Y - HalfLaneLength, Centre.Z);
		FVector LaneEnd(LaneX, Centre.Y + HalfLaneLength, Centre.Z);
		FGrenadeTestHelpers::ProjectOntoNavMesh(World, LaneStart, NavExtent);
		FGrenadeTestHelpers::ProjectOntoNavMesh(World, LaneEnd, NavExtent);
		const float SlotY = -HalfLaneLength * 0.5f + (SlotInLane - (TanksPerLane - 1) * 0.5f) * TankSpacingInLane;
		const FVector SpawnLocation(LaneX, Centre.Y + SlotY, Centre.Z);
		FTankTrailBenchTank& BenchTank = M_Tanks.AddDefaulted_GetRef();
		BenchTank.LaneStart = LaneStart;
		BenchTank.LaneEnd = LaneEnd;
		// Neighbouring lanes drive in opposite directions so the column never moves as one block.
		BenchTank.bHeadingToEnd = LaneIndex % 2 == 0;
		BenchTank.Tank = FGrenadeTestHelpers::SpawnUnitForPlayer(
			World, TankClassPath, FTransform(FRotator(0.f, 90.f, 0.f), SpawnLocation), PlayerOwner);
	}
	UE_LOG(LogRTSTankTrailBench, Display, TEXT("RTS_TANK_TRAIL_BENCH spawned %d of %d tanks around %s"),
	       CountLivingTanks(), M_TankCount, *M_DriveCentre.ToCompactString());
}

void UTankTrailBenchmarkSubsystem::ClearMapUnitsAroundLanes() const
{
	using namespace TankTrailBenchPrivate;
	TArray<AActor*> UnitsToRemove;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		const APawn* Pawn = Cast<APawn>(Actor);
		const bool bIsPlayerCamera = IsValid(Pawn) && Pawn->IsPlayerControlled();
		if (Cast<ICommands>(Actor) == nullptr || bIsPlayerCamera ||
			FVector::Dist2D(Actor->GetActorLocation(), M_DriveCentre) > ClearUnitsRadius)
		{
			continue;
		}
		UnitsToRemove.Add(Actor);
	}
	for (AActor* Unit : UnitsToRemove)
	{
		Unit->Destroy();
	}
	UE_LOG(LogRTSTankTrailBench, Display, TEXT("RTS_TANK_TRAIL_BENCH removed %d map units around the lanes"),
	       UnitsToRemove.Num());
}

void UTankTrailBenchmarkSubsystem::SpawnCamera()
{
	using namespace TankTrailBenchPrivate;
	UWorld& World = *GetWorld();
	FVector LookAt = M_DriveCentre;
	FGrenadeTestHelpers::DropToGround(World, LookAt);
	const FRotator Rotation(CameraPitch, 0.f, 0.f);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* Camera = World.SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(), FTransform(Rotation, LookAt - Rotation.Vector() * CameraDistance),
		SpawnParameters);
	if (not IsValid(Camera))
	{
		return;
	}
	Camera->GetCameraComponent()->SetFieldOfView(CameraFieldOfView);
	Camera->GetCameraComponent()->bConstrainAspectRatio = false;
	M_Camera = Camera;
}

void UTankTrailBenchmarkSubsystem::KeepEngineUnpaused()
{
	// Mission intros (Ger_FirstMission) pause the engine without the controller's pause flag, which KeepGameRunning
	// reads; nothing would drive and the trail writer would not tick.
	if (not UGameplayStatics::IsGamePaused(GetWorld()))
	{
		return;
	}
	UGameplayStatics::SetGamePaused(GetWorld(), false);
	if (not bM_HasLoggedEngineUnpause)
	{
		bM_HasLoggedEngineUnpause = true;
		UE_LOG(LogRTSTankTrailBench, Display,
		       TEXT("RTS_TANK_TRAIL_BENCH unpaused the engine (paused outside the controller)"));
	}
}

void UTankTrailBenchmarkSubsystem::HoldCamera() const
{
	// Mission intros take the view; every phase has to look at the same picture.
	if (not M_PlayerController.IsValid() || not M_Camera.IsValid())
	{
		return;
	}
	if (M_PlayerController->GetViewTarget() != M_Camera.Get())
	{
		M_PlayerController->SetViewTarget(M_Camera.Get());
	}
}

void UTankTrailBenchmarkSubsystem::DriveTanks()
{
	using namespace TankTrailBenchPrivate;
	for (FTankTrailBenchTank& BenchTank : M_Tanks)
	{
		const AActor* Tank = BenchTank.Tank.Get();
		if (not IsValid(Tank))
		{
			continue;
		}
		++BenchTank.FramesSinceOrder;
		if (BenchTank.FramesSinceOrder % FramesBetweenArrivalChecks != 0)
		{
			continue;
		}
		const FVector& Target = BenchTank.bHeadingToEnd ? BenchTank.LaneEnd : BenchTank.LaneStart;
		const bool bHasArrived = FVector::Dist2D(Tank->GetActorLocation(), Target) < ArrivalDistance;
		if (bHasArrived || BenchTank.FramesSinceOrder >= MaxFramesPerOrder)
		{
			BenchTank.bHeadingToEnd = not BenchTank.bHeadingToEnd;
			OrderTankAlongLane(BenchTank);
		}
	}
}

void UTankTrailBenchmarkSubsystem::OrderTankAlongLane(FTankTrailBenchTank& BenchTank) const
{
	ICommands* Commands = Cast<ICommands>(BenchTank.Tank.Get());
	if (Commands == nullptr)
	{
		return;
	}
	const FVector& Target = BenchTank.bHeadingToEnd ? BenchTank.LaneEnd : BenchTank.LaneStart;
	const FRotator Heading(0.f, BenchTank.bHeadingToEnd ? 90.f : -90.f, 0.f);
	Commands->MoveToLocation(Target, true, Heading);
	BenchTank.FramesSinceOrder = 0;
}

int32 UTankTrailBenchmarkSubsystem::CountLivingTanks() const
{
	int32 LivingTanks = 0;
	for (const FTankTrailBenchTank& BenchTank : M_Tanks)
	{
		LivingTanks += IsValid(BenchTank.Tank.Get()) ? 1 : 0;
	}
	return LivingTanks;
}

void UTankTrailBenchmarkSubsystem::SetTrailsOn(const bool bTrailsOn)
{
	if (IConsoleVariable* TrailVariable = IConsoleManager::Get().FindConsoleVariable(
		TankTrailBenchPrivate::TrailConsoleVariable))
	{
		TrailVariable->Set(bTrailsOn ? 1 : 0, ECVF_SetByConsole);
	}
}

void UTankTrailBenchmarkSubsystem::StartMeasuredPhase(const int32 PhaseIndex)
{
	M_PhaseIndex = PhaseIndex;
	FTankTrailBenchPhase& Phase = M_Phases[PhaseIndex];
	Phase.WallMs.Reserve(M_FramesPerPhase);
	Phase.GameThreadMs.Reserve(M_FramesPerPhase);
	Phase.RenderThreadMs.Reserve(M_FramesPerPhase);
	Phase.RhiThreadMs.Reserve(M_FramesPerPhase);
	Phase.GpuMs.Reserve(M_FramesPerPhase);
	SetTrailsOn(Phase.bTrailsOn);
	const UTankTrailSubsystem* TrailSubsystem = GetWorld()->GetSubsystem<UTankTrailSubsystem>();
	if (IsValid(TrailSubsystem))
	{
		M_TrailSecondsAtPhaseStart = TrailSubsystem->GetStats().UpdateSeconds;
		M_TrailUpdatesAtPhaseStart = TrailSubsystem->GetStats().Updates;
		M_TrailSegmentsAtPhaseStart = TrailSubsystem->GetStats().SegmentsDrawn;
	}
	M_LastFrameSeconds = FPlatformTime::Seconds();
}

void UTankTrailBenchmarkSubsystem::RecordFrame(FTankTrailBenchPhase& Phase, const double FrameMs) const
{
	// The engine stores the previous frame's thread and GPU times in cycles.
	Phase.WallMs.Add(static_cast<float>(FrameMs));
	Phase.GameThreadMs.Add(static_cast<float>(FPlatformTime::ToMilliseconds(GGameThreadTime)));
	Phase.RenderThreadMs.Add(static_cast<float>(FPlatformTime::ToMilliseconds(GRenderThreadTime)));
	Phase.RhiThreadMs.Add(static_cast<float>(FPlatformTime::ToMilliseconds(GRHIThreadTime)));
	Phase.GpuMs.Add(static_cast<float>(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0))));
}

void UTankTrailBenchmarkSubsystem::FinishMeasuredPhase(FTankTrailBenchPhase& Phase) const
{
	Phase.LivingTanks = CountLivingTanks();
	const UTankTrailSubsystem* TrailSubsystem = GetWorld()->GetSubsystem<UTankTrailSubsystem>();
	if (not IsValid(TrailSubsystem))
	{
		return;
	}
	Phase.TrailUpdateSeconds = TrailSubsystem->GetStats().UpdateSeconds - M_TrailSecondsAtPhaseStart;
	Phase.TrailUpdates = TrailSubsystem->GetStats().Updates - M_TrailUpdatesAtPhaseStart;
	Phase.TrailSegments = TrailSubsystem->GetStats().SegmentsDrawn - M_TrailSegmentsAtPhaseStart;
}

void UTankTrailBenchmarkSubsystem::ReportPhase(const int32 PhaseIndex) const
{
	using namespace TankTrailBenchPrivate;
	const FTankTrailBenchPhase& Phase = M_Phases[PhaseIndex];
	const double MicrosecondsPerUpdate = Phase.TrailUpdates > 0
		                                     ? Phase.TrailUpdateSeconds * 1.0e6 / Phase.TrailUpdates
		                                     : 0.0;
	UE_LOG(LogRTSTankTrailBench, Display,
	       TEXT("RTS_TANK_TRAIL_BENCH phase=%d trails=%s tanks=%d frames=%d paused=%s | %s | %s | %s | %s | %s | ")
	       TEXT("trail_updates=%d trail_segments=%d trail_cpu_us_per_update=%.1f"),
	       PhaseIndex, Phase.bTrailsOn ? TEXT("on") : TEXT("off"), Phase.LivingTanks, Phase.WallMs.Num(),
	       UGameplayStatics::IsGamePaused(GetWorld()) ? TEXT("yes") : TEXT("no"),
	       *DescribeMetric(TEXT("wall"), Phase.WallMs), *DescribeMetric(TEXT("game"), Phase.GameThreadMs),
	       *DescribeMetric(TEXT("render"), Phase.RenderThreadMs), *DescribeMetric(TEXT("rhi"), Phase.RhiThreadMs),
	       *DescribeMetric(TEXT("gpu"), Phase.GpuMs), Phase.TrailUpdates, Phase.TrailSegments,
	       MicrosecondsPerUpdate);
}

void UTankTrailBenchmarkSubsystem::ReportSummary() const
{
	using namespace TankTrailBenchPrivate;
	ReportPairDeltas();
	for (const bool bTrailsOn : {false, true})
	{
		TArray<float> Wall;
		TArray<float> Game;
		TArray<float> Render;
		TArray<float> Rhi;
		TArray<float> Gpu;
		double TrailSeconds = 0.0;
		int32 TrailUpdates = 0;
		int32 TrailSegments = 0;
		for (const FTankTrailBenchPhase& Phase : M_Phases)
		{
			if (Phase.bTrailsOn != bTrailsOn)
			{
				continue;
			}
			AppendAll(Wall, Phase.WallMs);
			AppendAll(Game, Phase.GameThreadMs);
			AppendAll(Render, Phase.RenderThreadMs);
			AppendAll(Rhi, Phase.RhiThreadMs);
			AppendAll(Gpu, Phase.GpuMs);
			TrailSeconds += Phase.TrailUpdateSeconds;
			TrailUpdates += Phase.TrailUpdates;
			TrailSegments += Phase.TrailSegments;
		}
		const double TrailMsPerFrame = Wall.Num() > 0 ? TrailSeconds * MillisecondsPerSecond / Wall.Num() : 0.0;
		UE_LOG(LogRTSTankTrailBench, Display,
		       TEXT("RTS_TANK_TRAIL_BENCH summary trails=%s frames=%d | %s | %s | %s | %s | %s | ")
		       TEXT("trail_updates=%d trail_segments=%d trail_cpu_ms_per_frame=%.4f"),
		       bTrailsOn ? TEXT("on") : TEXT("off"), Wall.Num(), *DescribeMetric(TEXT("wall"), Wall),
		       *DescribeMetric(TEXT("game"), Game), *DescribeMetric(TEXT("render"), Render),
		       *DescribeMetric(TEXT("rhi"), Rhi), *DescribeMetric(TEXT("gpu"), Gpu), TrailUpdates, TrailSegments,
		       TrailMsPerFrame);
	}
}

void UTankTrailBenchmarkSubsystem::ReportPairDeltas() const
{
	using namespace TankTrailBenchPrivate;
	const auto GetMedian = [](const TArray<float>& Values)
	{
		TArray<float> Sorted = Values;
		Sorted.Sort();
		return GetPercentile(Sorted, 0.5f);
	};
	// Each pair is an off phase followed by an on phase; the difference of their medians cancels slow drift.
	TArray<float> WallDeltas;
	TArray<float> GameDeltas;
	TArray<float> RenderDeltas;
	TArray<float> GpuDeltas;
	for (int32 PhaseIndex = 0; PhaseIndex + 1 < M_Phases.Num(); PhaseIndex += 2)
	{
		const FTankTrailBenchPhase& Off = M_Phases[PhaseIndex];
		const FTankTrailBenchPhase& On = M_Phases[PhaseIndex + 1];
		WallDeltas.Add(GetMedian(On.WallMs) - GetMedian(Off.WallMs));
		GameDeltas.Add(GetMedian(On.GameThreadMs) - GetMedian(Off.GameThreadMs));
		RenderDeltas.Add(GetMedian(On.RenderThreadMs) - GetMedian(Off.RenderThreadMs));
		GpuDeltas.Add(GetMedian(On.GpuMs) - GetMedian(Off.GpuMs));
	}
	const auto DescribeDeltas = [](const TCHAR* Name, TArray<float> Deltas)
	{
		Deltas.Sort();
		return FString::Printf(TEXT("%s median=%+.3f min=%+.3f max=%+.3f"), Name, GetPercentile(Deltas, 0.5f),
		                       Deltas.IsEmpty() ? 0.f : Deltas[0], Deltas.IsEmpty() ? 0.f : Deltas.Last());
	};
	UE_LOG(LogRTSTankTrailBench, Display, TEXT("RTS_TANK_TRAIL_BENCH pairdelta pairs=%d | %s | %s | %s | %s"),
	       WallDeltas.Num(), *DescribeDeltas(TEXT("wall"), WallDeltas), *DescribeDeltas(TEXT("game"), GameDeltas),
	       *DescribeDeltas(TEXT("render"), RenderDeltas), *DescribeDeltas(TEXT("gpu"), GpuDeltas));
}

void UTankTrailBenchmarkSubsystem::SaveFrameCsv() const
{
	FString CsvText = TEXT("phase,trails,frame,wall_ms,game_ms,render_ms,rhi_ms,gpu_ms\n");
	for (int32 PhaseIndex = 0; PhaseIndex < M_Phases.Num(); ++PhaseIndex)
	{
		const FTankTrailBenchPhase& Phase = M_Phases[PhaseIndex];
		for (int32 FrameIndex = 0; FrameIndex < Phase.WallMs.Num(); ++FrameIndex)
		{
			CsvText += FString::Printf(TEXT("%d,%d,%d,%.4f,%.4f,%.4f,%.4f,%.4f\n"), PhaseIndex,
			                           Phase.bTrailsOn ? 1 : 0, FrameIndex, Phase.WallMs[FrameIndex],
			                           Phase.GameThreadMs[FrameIndex], Phase.RenderThreadMs[FrameIndex],
			                           Phase.RhiThreadMs[FrameIndex], Phase.GpuMs[FrameIndex]);
		}
	}
	FFileHelper::SaveStringToFile(CsvText, *(M_OutputDirectory / TEXT("TankTrailBench_Frames.csv")));
}

void UTankTrailBenchmarkSubsystem::SaveTrailPictures()
{
	UWorld& World = *GetWorld();
	if (IsValid(M_CaptureTarget))
	{
		FGrenadeTestHelpers::SaveCapture(World, *M_CaptureTarget, M_OutputDirectory, TEXT("TankTrails_View.png"));
	}
	const UTankTrailSubsystem* TrailSubsystem = World.GetSubsystem<UTankTrailSubsystem>();
	if (IsValid(TrailSubsystem) && IsValid(TrailSubsystem->GetTrailRenderTarget()))
	{
		UKismetRenderingLibrary::ExportRenderTarget(&World, TrailSubsystem->GetTrailRenderTarget(), M_OutputDirectory,
		                                            TEXT("TankTrails_RenderTarget"));
	}
	UE_LOG(LogRTSTankTrailBench, Display, TEXT("RTS_TANK_TRAIL_BENCH pictures saved, trail writer active=%s"),
	       IsValid(TrailSubsystem) && TrailSubsystem->GetIsTrailRenderingActive() ? TEXT("yes") : TEXT("no"));
}

#else

bool UTankTrailBenchmarkSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UTankTrailBenchmarkSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UTankTrailBenchmarkSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UTankTrailBenchmarkSubsystem::Tick(float DeltaTime)
{
}

TStatId UTankTrailBenchmarkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTankTrailBenchmarkSubsystem, STATGROUP_Tickables);
}

#endif
