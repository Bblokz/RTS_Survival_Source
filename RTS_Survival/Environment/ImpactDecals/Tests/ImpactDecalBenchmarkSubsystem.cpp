// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ImpactDecalBenchmarkSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraActor.h"
#include "Components/ComboBoxString.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderTimer.h"
#include "RenderingThread.h"
#include "RTS_Survival/Environment/ImpactDecals/LandscapeImpactDecalSubsystem.h"
#include "RTS_Survival/Environment/ImpactDecals/RTSImpactDecalSettings.h"
#include "RTS_Survival/Game/UserSettings/GraphicsSettings/ImpactDecalsLevel.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Subsystems/SettingsMenuSubsystem/RTSSettingsMenuSubsystem.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "ShaderCompiler.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSImpactDecalBench, Log, All);

namespace ImpactDecalBenchPrivate
{
	const TCHAR* const BenchmarkSwitch = TEXT("ImpactDecalBench");
	const TCHAR* const DirectorySwitch = TEXT("ImpactDecalBenchDir=");
	const TCHAR* const LocationSwitch = TEXT("ImpactDecalBenchAt=");
	const TCHAR* const CountSwitch = TEXT("ImpactDecalBenchCount=");
	const TCHAR* const SecondsSwitch = TEXT("ImpactDecalBenchSeconds=");
	const TCHAR* const RepeatsSwitch = TEXT("ImpactDecalBenchRepeats=");
	const TCHAR* const RadiusSwitch = TEXT("ImpactDecalBenchRadius=");
	const TCHAR* const ProfileGpuSwitch = TEXT("ImpactDecalBenchProfileGPU");
	const TCHAR* const PooledOnlySwitch = TEXT("ImpactDecalBenchPooledOnly");
	const TCHAR* const MicroOnlySwitch = TEXT("ImpactDecalBenchMicroOnly");
	const TCHAR* const MicroRepeatsSwitch = TEXT("ImpactDecalBenchMicroRepeats=");
	constexpr int32 DefaultMicroRepeats = 8;
	const TCHAR* const LiveSwitch = TEXT("ImpactDecalBenchLive");
	const TCHAR* const LiveSecondsSwitch = TEXT("ImpactDecalBenchLiveSeconds=");
	const TCHAR* const SettingsWidgetPath =
		TEXT("/Game/RTS_Survival/Blueprints/GameUI/EscapeMenu/BP_W_EscapeMenuSettings.BP_W_EscapeMenuSettings_C");
	const FName ImpactDecalsComboName(TEXT("M_ComboImpactDecals"));
	const FName ImpactDecalsLabelName(TEXT("M_TextImpactDecalsLabel"));
	constexpr int32 LiveCraterCap = 400;
	constexpr int32 MenuCheckFrame = 10;
	constexpr int32 MenuOptionCount = 5;
	constexpr float CraterCheckTraceHalfHeight = 300.f;
	constexpr float CraterSurfaceToleranceCm = 75.f;
	constexpr int32 EvictionCraterCount = 10;
	constexpr int32 EvictionShrunkCount = 5;
	constexpr float EvictionCalibreMm = 20.f;
	constexpr int32 SizeCheckPoolMax = 50;
	// Shell craters 75 mm and up must be at least this much larger than the previous band design sizes.
	constexpr float SizeCheckMinGrowth = 1.3f;
	// -ImpactDecalBenchSpawnFight: lines of tanks facing each other, inside sight range so they engage at once.
	const TCHAR* const FightGermanTank =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H.Ger_Vehicle_PanzerIV_H_C");
	const TCHAR* const FightSovietTank =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");
	constexpr uint8 FightGermanOwner = 1;
	constexpr uint8 FightSovietOwner = 2;
	constexpr int32 FightTanksPerSide = 3;
	constexpr float FightHalfGapCm = 2000.f;
	constexpr float FightLaneSpacingCm = 700.f;
	constexpr double FightWaveSeconds = 30.0;
	const FVector FightNavExtent(1500.f, 1500.f, 3000.f);
	const TCHAR* const PoolCVarName = TEXT("rts.ImpactDecals");

	// Open grass next to the player start on Ger_Trains.
	const FVector DefaultCentre(13250.f, -28000.f, 0.f);
	// Calibres the craters cycle through: every band, weighted to the common guns.
	constexpr float BenchCalibres[] = {20.f, 20.f, 30.f, 37.f, 50.f, 75.f, 75.f, 88.f, 105.f, 128.f, 150.f, 210.f};

	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 120.0;
	constexpr double MaxShaderCompileSeconds = 1200.0;
	constexpr int32 ShaderQuietFrames = 30;
	constexpr int32 WarmUpFrames = 300;
	constexpr int32 BaselineFrames = 150;
	constexpr int32 HoldFrames = 150;
	constexpr int32 BurstFrameInStep = 3;
	constexpr int32 BurstTailFrames = 30;
	// Craters are spawned a little past the spread, so the last one is in the measured frames too.
	constexpr double SpawnTailSeconds = 0.25;
	constexpr int32 ProfileGpuFrame = 60;
	constexpr int32 ProfileGpuSettleFrames = 10;
	constexpr int32 PictureFrames = 150;
	constexpr int32 PointsPerDecal = 4;
	constexpr int32 MaxPointTraces = 4000;
	constexpr float TraceHalfHeight = 100000.f;

	// The benchmark view: an RTS camera zoomed in over the craters (more screen pixels per crater = more GPU work).
	constexpr float CameraDistance = 4500.f;
	constexpr float CameraPitch = -60.f;
	constexpr float CameraFieldOfView = 90.f;
	constexpr double MillisecondsPerSecond = 1000.0;
	constexpr double MicrosecondsPerSecond = 1000000.0;
	constexpr float AtlasCellsTotal = 16.f;

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

	float GetMedian(const TArray<float>& Values)
	{
		TArray<float> Sorted = Values;
		Sorted.Sort();
		return GetPercentile(Sorted, 0.5f);
	}

	float GetP95(const TArray<float>& Values)
	{
		TArray<float> Sorted = Values;
		Sorted.Sort();
		return GetPercentile(Sorted, 0.95f);
	}

	FString DescribeMetric(const TCHAR* Name, const TArray<float>& Values)
	{
		TArray<float> Sorted = Values;
		Sorted.Sort();
		return FString::Printf(TEXT("%s mean=%.3f p50=%.3f p95=%.3f max=%.3f"), Name, GetMean(Values),
		                       GetPercentile(Sorted, 0.5f), GetPercentile(Sorted, 0.95f),
		                       Sorted.IsEmpty() ? 0.f : Sorted.Last());
	}

	const TCHAR* GetBenchPathName(const EImpactDecalBenchPath Path)
	{
		return Path == EImpactDecalBenchPath::Pooled ? TEXT("pooled") : TEXT("legacy");
	}

	const TCHAR* GetBenchKindName(const EImpactDecalBenchStepKind Kind)
	{
		switch (Kind)
		{
		case EImpactDecalBenchStepKind::Baseline:
			return TEXT("baseline");
		case EImpactDecalBenchStepKind::Fill:
			return TEXT("fill");
		case EImpactDecalBenchStepKind::Hold:
			return TEXT("hold");
		case EImpactDecalBenchStepKind::Recycle:
			return TEXT("recycle");
		case EImpactDecalBenchStepKind::Burst:
			return TEXT("burst");
		case EImpactDecalBenchStepKind::Clear:
			return TEXT("clear");
		case EImpactDecalBenchStepKind::Micro:
			return TEXT("micro");
		}
		return TEXT("unknown");
	}
}

bool UImpactDecalBenchmarkSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), ImpactDecalBenchPrivate::BenchmarkSwitch);
}

void UImpactDecalBenchmarkSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	using namespace ImpactDecalBenchPrivate;
	Super::OnWorldBeginPlay(InWorld);
	M_OutputDirectory = FPaths::ProjectSavedDir() / TEXT("ImpactDecalBench");
	FParse::Value(FCommandLine::Get(), DirectorySwitch, M_OutputDirectory);
	FParse::Value(FCommandLine::Get(), CountSwitch, M_DecalCount);
	FParse::Value(FCommandLine::Get(), SecondsSwitch, M_SpawnSeconds);
	FParse::Value(FCommandLine::Get(), RepeatsSwitch, M_Repeats);
	FParse::Value(FCommandLine::Get(), RadiusSwitch, M_SpreadRadius);
	bM_IsLiveMode = FParse::Param(FCommandLine::Get(), LiveSwitch);
	bM_SkipGameStart = FParse::Param(FCommandLine::Get(), TEXT("ImpactDecalBenchNoStart"));
	bM_SpawnFight = FParse::Param(FCommandLine::Get(), TEXT("ImpactDecalBenchSpawnFight"));
	FParse::Value(FCommandLine::Get(), LiveSecondsSwitch, M_LiveSeconds);
	M_DecalCount = FMath::Max(1, M_DecalCount);
	M_SpawnSeconds = FMath::Max(0.1f, M_SpawnSeconds);
	M_Repeats = FMath::Max(1, M_Repeats);
	M_Centre = DefaultCentre;
	FString LocationText;
	if (FParse::Value(FCommandLine::Get(), LocationSwitch, LocationText, false))
	{
		FString XText;
		FString YText;
		if (LocationText.Split(TEXT(","), &XText, &YText))
		{
			M_Centre = FVector(FCString::Atof(*XText), FCString::Atof(*YText), 0.f);
			bM_HasExplicitCentre = true;
		}
	}
	BuildSteps();
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	EnterState(EImpactDecalBenchState::WaitingForGameStart);
	UE_LOG(LogRTSImpactDecalBench, Display,
	       TEXT("RTS_IMPACT_DECAL_BENCH started: decals=%d seconds=%.1f repeats=%d steps=%d centre=%s radius=%.0f output=%s"),
	       M_DecalCount, M_SpawnSeconds, M_Repeats, M_Steps.Num(), *M_Centre.ToCompactString(), M_SpreadRadius,
	       *M_OutputDirectory);
}

void UImpactDecalBenchmarkSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

void UImpactDecalBenchmarkSubsystem::Tick(float DeltaTime)
{
	if (M_State == EImpactDecalBenchState::Inactive || M_State == EImpactDecalBenchState::Done ||
		not IsValid(GetWorld()))
	{
		return;
	}
	if (M_State != EImpactDecalBenchState::WaitingForGameStart)
	{
		FGrenadeTestHelpers::KeepGameRunning(*GetWorld(), M_PlayerController.Get());
		KeepEngineUnpaused();
		HoldCamera();
	}
	++M_FramesInState;
	switch (M_State)
	{
	case EImpactDecalBenchState::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case EImpactDecalBenchState::WaitingForShaders:
		TickWaitingForShaders();
		break;
	case EImpactDecalBenchState::WarmingUp:
		TickWarmingUp();
		break;
	case EImpactDecalBenchState::RunningSteps:
		TickRunningSteps();
		break;
	case EImpactDecalBenchState::TakingPicture:
		TickTakingPicture();
		break;
	case EImpactDecalBenchState::LiveCombat:
		TickLiveCombat();
		break;
	case EImpactDecalBenchState::MenuCheck:
		TickMenuCheck();
		break;
	default:
		break;
	}
}

TStatId UImpactDecalBenchmarkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UImpactDecalBenchmarkSubsystem, STATGROUP_Tickables);
}

void UImpactDecalBenchmarkSubsystem::EnterState(const EImpactDecalBenchState NewState)
{
	M_State = NewState;
	M_StateStartRealSeconds = FPlatformTime::Seconds();
	M_LastFrameSeconds = M_StateStartRealSeconds;
	M_FramesInState = 0;
}

double UImpactDecalBenchmarkSubsystem::GetRealSecondsInState() const
{
	return FPlatformTime::Seconds() - M_StateStartRealSeconds;
}

void UImpactDecalBenchmarkSubsystem::Finish(const bool bSucceeded, const FString& Reason)
{
	M_State = EImpactDecalBenchState::Done;
	UE_LOG(LogRTSImpactDecalBench, Display, TEXT("RTS_IMPACT_DECAL_BENCH RESULT %s %s"),
	       bSucceeded ? TEXT("PASS") : TEXT("FAIL"), *Reason);
	FPlatformMisc::RequestExit(false, TEXT("ImpactDecalBench"));
}

void UImpactDecalBenchmarkSubsystem::TickWaitingForGameStart()
{
	using namespace ImpactDecalBenchPrivate;
	if (GetRealSecondsInState() > MaxStartSeconds)
	{
		Finish(false, TEXT("the game did not start"));
		return;
	}
	if (GetRealSecondsInState() < HudBuildSeconds)
	{
		return;
	}
	if (not bM_SkipGameStart && not FGrenadeTestHelpers::TryStartGame(*GetWorld(), M_PlayerController))
	{
		return;
	}
	M_DecalMaterial = URTSImpactDecalSettings::Get()->DecalMaterial.LoadSynchronous();
	if (not IsValid(M_DecalMaterial))
	{
		Finish(false, FString::Printf(TEXT("decal material %s did not load"),
		                              *URTSImpactDecalSettings::Get()->DecalMaterial.ToString()));
		return;
	}
	// The live check only needs points for its menu crater test; the map's own fighting picks the spots.
	bool bHasPoints = CollectLandscapePoints();
	if (not bHasPoints && bM_SpawnFight && CentreOnLandscape())
	{
		bHasPoints = CollectLandscapePoints();
	}
	if (not bHasPoints && not bM_IsLiveMode)
	{
		Finish(false, TEXT("no landscape under the benchmark centre"));
		return;
	}
	SpawnCamera();
	// One crater of every band so each shader permutation and PSO is ready before measuring.
	SetPoolMax(M_DecalCount);
	if (ULandscapeImpactDecalSubsystem* Pool = GetWorld()->GetSubsystem<ULandscapeImpactDecalSubsystem>())
	{
		for (int32 Index = 0; Index < 12 && M_Points.IsValidIndex(Index); ++Index)
		{
			Pool->AddImpactDecal(M_Points[Index].CalibreMm, M_Points[Index].Location, M_Points[Index].Normal);
		}
	}
	for (int32 Index = 12; Index < 24 && M_Points.IsValidIndex(Index); ++Index)
	{
		SpawnLegacy(M_Points[Index]);
	}
	EnterState(EImpactDecalBenchState::WaitingForShaders);
}

void UImpactDecalBenchmarkSubsystem::TickWaitingForShaders()
{
	using namespace ImpactDecalBenchPrivate;
	const int32 RemainingShaderJobs = GShaderCompilingManager != nullptr
		                                  ? GShaderCompilingManager->GetNumRemainingJobs()
		                                  : 0;
	if (RemainingShaderJobs > 0 && GetRealSecondsInState() < MaxShaderCompileSeconds)
	{
		M_FramesInState = 0;
		return;
	}
	if (M_FramesInState < ShaderQuietFrames)
	{
		return;
	}
	UE_LOG(LogRTSImpactDecalBench, Display, TEXT("RTS_IMPACT_DECAL_BENCH shaders ready after %.1f s"),
	       GetRealSecondsInState());
	EnterState(EImpactDecalBenchState::WarmingUp);
}

void UImpactDecalBenchmarkSubsystem::TickWarmingUp()
{
	if (M_FramesInState == 2)
	{
		ClearAll();
	}
	if (M_FramesInState < ImpactDecalBenchPrivate::WarmUpFrames)
	{
		return;
	}
	if (bM_IsLiveMode)
	{
		StartLiveCombat();
		return;
	}
	EnterState(EImpactDecalBenchState::RunningSteps);
	StartStep(0);
}

void UImpactDecalBenchmarkSubsystem::BuildSteps()
{
	const bool bPooledOnly = FParse::Param(FCommandLine::Get(), ImpactDecalBenchPrivate::PooledOnlySwitch);
	const bool bMicroOnly = FParse::Param(FCommandLine::Get(), ImpactDecalBenchPrivate::MicroOnlySwitch);
	for (int32 Repeat = 0; Repeat < M_Repeats && not bMicroOnly; ++Repeat)
	{
		TArray<EImpactDecalBenchPath> Paths = {EImpactDecalBenchPath::Pooled};
		if (not bPooledOnly)
		{
			// Alternate which path goes first, so drifting machine load hits both alike.
			Paths.Insert(EImpactDecalBenchPath::Legacy, Repeat % 2 == 0 ? 1 : 0);
		}
		for (const EImpactDecalBenchPath Path : Paths)
		{
			AddStep(EImpactDecalBenchStepKind::Baseline, Path, Repeat);
			AddStep(EImpactDecalBenchStepKind::Fill, Path, Repeat);
			AddStep(EImpactDecalBenchStepKind::Hold, Path, Repeat);
			AddStep(EImpactDecalBenchStepKind::Recycle, Path, Repeat);
			AddStep(EImpactDecalBenchStepKind::Burst, Path, Repeat);
			AddStep(EImpactDecalBenchStepKind::Clear, Path, Repeat);
		}
	}
	int32 MicroRepeats = ImpactDecalBenchPrivate::DefaultMicroRepeats;
	FParse::Value(FCommandLine::Get(), ImpactDecalBenchPrivate::MicroRepeatsSwitch, MicroRepeats);
	for (int32 Repeat = 0; Repeat < MicroRepeats; ++Repeat)
	{
		// Alternate which path goes first.
		const bool bPooledFirst = Repeat % 2 == 0;
		AddStep(EImpactDecalBenchStepKind::Micro,
		        bPooledFirst || bPooledOnly ? EImpactDecalBenchPath::Pooled : EImpactDecalBenchPath::Legacy, Repeat);
		if (not bPooledOnly)
		{
			AddStep(EImpactDecalBenchStepKind::Micro,
			        bPooledFirst ? EImpactDecalBenchPath::Legacy : EImpactDecalBenchPath::Pooled, Repeat);
		}
	}
}

void UImpactDecalBenchmarkSubsystem::AddStep(const EImpactDecalBenchStepKind Kind, const EImpactDecalBenchPath Path,
                                             const int32 Repeat)
{
	FImpactDecalBenchStep& Step = M_Steps.AddDefaulted_GetRef();
	Step.Kind = Kind;
	Step.Path = Path;
	Step.Repeat = Repeat;
	Step.Label = FString::Printf(TEXT("%s_%s_%d"), ImpactDecalBenchPrivate::GetBenchPathName(Path),
	                             ImpactDecalBenchPrivate::GetBenchKindName(Kind), Repeat);
}

bool UImpactDecalBenchmarkSubsystem::CollectLandscapePoints()
{
	using namespace ImpactDecalBenchPrivate;
	UWorld& World = *GetWorld();
	FRandomStream Random(1234);
	const int32 WantedPoints = M_DecalCount * PointsPerDecal;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ImpactDecalBenchPoints), false);
	for (int32 Attempt = 0; Attempt < MaxPointTraces && M_Points.Num() < WantedPoints; ++Attempt)
	{
		const FVector2D Offset = FVector2D(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)) *
			M_SpreadRadius;
		const FVector Start(M_Centre.X + Offset.X, M_Centre.Y + Offset.Y, TraceHalfHeight);
		const FVector End(Start.X, Start.Y, -TraceHalfHeight);
		FHitResult Hit;
		if (not World.LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams) ||
			not ULandscapeImpactDecalSubsystem::GetIsLandscapeActor(Hit.GetActor()))
		{
			continue;
		}
		FImpactDecalBenchPoint& Point = M_Points.AddDefaulted_GetRef();
		Point.Location = Hit.ImpactPoint;
		Point.Normal = Hit.ImpactNormal;
		Point.CalibreMm = BenchCalibres[M_Points.Num() % UE_ARRAY_COUNT(BenchCalibres)];
	}
	UE_LOG(LogRTSImpactDecalBench, Display, TEXT("RTS_IMPACT_DECAL_BENCH landscape points=%d of %d"),
	       M_Points.Num(), WantedPoints);
	if (M_Points.IsEmpty())
	{
		return false;
	}
	FVector Centre = M_Centre;
	FGrenadeTestHelpers::DropToGround(World, Centre);
	M_Centre = Centre;
	return true;
}

void UImpactDecalBenchmarkSubsystem::SpawnCamera()
{
	using namespace ImpactDecalBenchPrivate;
	UWorld& World = *GetWorld();
	const FRotator Rotation(CameraPitch, 0.f, 0.f);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* Camera = World.SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(), FTransform(Rotation, M_Centre - Rotation.Vector() * CameraDistance),
		SpawnParameters);
	if (not IsValid(Camera))
	{
		return;
	}
	Camera->GetCameraComponent()->SetFieldOfView(CameraFieldOfView);
	Camera->GetCameraComponent()->bConstrainAspectRatio = false;
	M_Camera = Camera;
}

void UImpactDecalBenchmarkSubsystem::HoldCamera() const
{
	if (not M_PlayerController.IsValid() || not M_Camera.IsValid())
	{
		return;
	}
	if (M_PlayerController->GetViewTarget() != M_Camera.Get())
	{
		M_PlayerController->SetViewTarget(M_Camera.Get());
	}
}

void UImpactDecalBenchmarkSubsystem::KeepEngineUnpaused() const
{
	if (UGameplayStatics::IsGamePaused(GetWorld()))
	{
		UGameplayStatics::SetGamePaused(GetWorld(), false);
	}
}

void UImpactDecalBenchmarkSubsystem::StartStep(const int32 StepIndex)
{
	M_StepIndex = StepIndex;
	M_FramesInStep = 0;
	M_SpawnedInStep = 0;
	M_StepStartGameSeconds = GetWorld()->GetTimeSeconds();
	M_LastFrameSeconds = FPlatformTime::Seconds();
	FImpactDecalBenchStep& Step = M_Steps[StepIndex];
	// Only the measured path may have craters on the map; the pool is off during legacy steps.
	SetPoolMax(Step.Path == EImpactDecalBenchPath::Pooled ? M_DecalCount : 0);
	if (Step.Kind == EImpactDecalBenchStepKind::Burst || Step.Kind == EImpactDecalBenchStepKind::Baseline)
	{
		ClearAll();
	}
	if (ULandscapeImpactDecalSubsystem* Pool = GetWorld()->GetSubsystem<ULandscapeImpactDecalSubsystem>())
	{
		M_MergedAtStepStart = Pool->GetStats().MergedIntoExisting;
	}
}

void UImpactDecalBenchmarkSubsystem::TickRunningSteps()
{
	using namespace ImpactDecalBenchPrivate;
	FImpactDecalBenchStep& Step = M_Steps[M_StepIndex];
	++M_FramesInStep;
	const double NowSeconds = FPlatformTime::Seconds();
	const double FrameMs = (NowSeconds - M_LastFrameSeconds) * MillisecondsPerSecond;
	M_LastFrameSeconds = NowSeconds;

	const bool bProfilesGpu = FParse::Param(FCommandLine::Get(), ProfileGpuSwitch);
	if (bProfilesGpu && Step.Kind == EImpactDecalBenchStepKind::Hold && M_FramesInStep == ProfileGpuFrame &&
		GEngine != nullptr)
	{
		UE_LOG(LogRTSImpactDecalBench, Display, TEXT("RTS_IMPACT_DECAL_BENCH profilegpu step=%s decals=%d"),
		       *Step.Label, GetCurrentDecalCount(Step.Path));
		GEngine->Exec(GetWorld(), TEXT("ProfileGPU"));
	}
	const bool bIsProfileHitch = bProfilesGpu && Step.Kind == EImpactDecalBenchStepKind::Hold &&
		M_FramesInStep >= ProfileGpuFrame && M_FramesInStep < ProfileGpuFrame + ProfileGpuSettleFrames;
	// The first frame of a step still carries the previous step's switch; it is not measured.
	const bool bIsUnmeasuredStep = Step.Kind == EImpactDecalBenchStepKind::Clear ||
		Step.Kind == EImpactDecalBenchStepKind::Micro;
	if (M_FramesInStep > 1 && not bIsProfileHitch && not bIsUnmeasuredStep)
	{
		RecordFrame(Step, FrameMs);
	}
	if (Step.Kind == EImpactDecalBenchStepKind::Micro && M_FramesInStep == 2)
	{
		RunMicroStep(Step);
	}
	SpawnForStep(Step);
	if (M_FramesInStep < GetStepFrames(Step))
	{
		return;
	}
	if (Step.Kind == EImpactDecalBenchStepKind::Clear)
	{
		ClearAll();
	}
	Step.DecalsAtEnd = GetCurrentDecalCount(Step.Path);
	if (const ULandscapeImpactDecalSubsystem* Pool = GetWorld()->GetSubsystem<ULandscapeImpactDecalSubsystem>())
	{
		Step.Merged = Pool->GetStats().MergedIntoExisting - M_MergedAtStepStart;
	}
	ReportStep(Step);
	if (M_Steps.IsValidIndex(M_StepIndex + 1))
	{
		StartStep(M_StepIndex + 1);
		return;
	}
	ReportSummary();
	SaveFrameCsv();
	EnterState(EImpactDecalBenchState::TakingPicture);
}

int32 UImpactDecalBenchmarkSubsystem::GetStepFrames(const FImpactDecalBenchStep& Step) const
{
	using namespace ImpactDecalBenchPrivate;
	switch (Step.Kind)
	{
	case EImpactDecalBenchStepKind::Baseline:
		return BaselineFrames;
	case EImpactDecalBenchStepKind::Hold:
		return HoldFrames;
	case EImpactDecalBenchStepKind::Burst:
		return BurstFrameInStep + BurstTailFrames;
	case EImpactDecalBenchStepKind::Clear:
		return 2;
	case EImpactDecalBenchStepKind::Micro:
		return 3;
	case EImpactDecalBenchStepKind::Fill:
	case EImpactDecalBenchStepKind::Recycle:
	{
		// Runs on game time: done once every crater is out and the tail has passed.
		const double Elapsed = GetWorld()->GetTimeSeconds() - M_StepStartGameSeconds;
		const bool bDone = M_SpawnedInStep >= M_DecalCount && Elapsed >= M_SpawnSeconds + SpawnTailSeconds;
		return bDone ? 0 : TNumericLimits<int32>::Max();
	}
	}
	return 1;
}

void UImpactDecalBenchmarkSubsystem::SpawnForStep(FImpactDecalBenchStep& Step)
{
	using namespace ImpactDecalBenchPrivate;
	if (Step.Kind == EImpactDecalBenchStepKind::Burst)
	{
		if (M_FramesInStep == BurstFrameInStep)
		{
			Step.BurstFrame = Step.WallMs.Num();
			for (int32 Index = 0; Index < M_DecalCount; ++Index)
			{
				SpawnOne(Step);
			}
		}
		return;
	}
	if (Step.Kind != EImpactDecalBenchStepKind::Fill && Step.Kind != EImpactDecalBenchStepKind::Recycle)
	{
		return;
	}
	const double Elapsed = GetWorld()->GetTimeSeconds() - M_StepStartGameSeconds;
	const double Interval = M_SpawnSeconds / M_DecalCount;
	while (M_SpawnedInStep < M_DecalCount && Elapsed >= M_SpawnedInStep * Interval)
	{
		SpawnOne(Step);
	}
}

void UImpactDecalBenchmarkSubsystem::SpawnOne(FImpactDecalBenchStep& Step)
{
	using namespace ImpactDecalBenchPrivate;
	if (M_Points.IsEmpty())
	{
		return;
	}
	const FImpactDecalBenchPoint& Point = M_Points[M_NextPoint];
	M_NextPoint = (M_NextPoint + 1) % M_Points.Num();
	++M_SpawnedInStep;
	const uint64 StartCycles = FPlatformTime::Cycles64();
	if (Step.Path == EImpactDecalBenchPath::Pooled)
	{
		// The points are on the landscape already (traced), so this is RequestImpactDecal minus its landscape check.
		if (ULandscapeImpactDecalSubsystem* Pool = GetWorld()->GetSubsystem<ULandscapeImpactDecalSubsystem>())
		{
			Pool->AddImpactDecal(Point.CalibreMm, Point.Location, Point.Normal);
		}
	}
	else
	{
		SpawnLegacy(Point);
	}
	Step.SpawnMicros.Add(static_cast<float>(
		FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - StartCycles) * MicrosecondsPerSecond));
}

void UImpactDecalBenchmarkSubsystem::SpawnLegacy(const FImpactDecalBenchPoint& Point)
{
	using namespace ImpactDecalBenchPrivate;
	const URTSImpactDecalSettings* Settings = URTSImpactDecalSettings::Get();
	int32 BandIndex = INDEX_NONE;
	const FRTSImpactDecalBand* Band = Settings->FindBand(Point.CalibreMm, BandIndex);
	if (Band == nullptr || not IsValid(M_DecalMaterial))
	{
		return;
	}
	// The way UGameDecalManager spawns decals: a new decal component per crater, sized and coloured after spawning.
	const float CalibreScale = FMath::Clamp(Point.CalibreMm / Band->DesignCalibreMm, Settings->CalibreScaleClamp.Min,
	                                        Settings->CalibreScaleClamp.Max);
	const float Radius = 0.5f * Band->DesignDiameterCm * CalibreScale *
		FMath::FRandRange(Settings->RandomSizeScale.Min, Settings->RandomSizeScale.Max);
	const FVector ProjectionAxis = -Point.Normal.GetSafeNormal();
	const FQuat Rotation = FQuat(ProjectionAxis, FMath::FRandRange(0.f, UE_TWO_PI)) *
		FRotationMatrix::MakeFromX(ProjectionAxis).ToQuat();
	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
		GetWorld(), M_DecalMaterial, FVector(0.5f * Band->ProjectionDepthCm, Radius, Radius), Point.Location,
		Rotation.Rotator());
	if (not IsValid(Decal))
	{
		return;
	}
	const int32 Cell = Band->FirstAtlasCell + FMath::RandRange(0, FMath::Max(1, Band->AtlasCellCount) - 1);
	Decal->SetDecalColor(FLinearColor((Cell + 0.5f) / AtlasCellsTotal, FMath::FRand(), 0.f, 1.f));
	Decal->SetSortOrder(Settings->SortOrder);
	M_LegacyDecals.Add(Decal);
	if (M_LegacyDecals.Num() > M_DecalCount)
	{
		if (IsValid(M_LegacyDecals[0]))
		{
			M_LegacyDecals[0]->DestroyComponent();
		}
		M_LegacyDecals.RemoveAt(0);
	}
}

double UImpactDecalBenchmarkSubsystem::FlushRenderWork() const
{
	const double StartSeconds = FPlatformTime::Seconds();
	GetWorld()->SendAllEndOfFrameUpdates();
	FlushRenderingCommands();
	return FPlatformTime::Seconds() - StartSeconds;
}

void UImpactDecalBenchmarkSubsystem::RunMicroStep(FImpactDecalBenchStep& Step)
{
	using namespace ImpactDecalBenchPrivate;
	ClearAll();
	SetPoolMax(Step.Path == EImpactDecalBenchPath::Pooled ? M_DecalCount : 0);
	FlushRenderWork();
	// What the flush costs with nothing to do; subtracted from every measured block.
	const double FlushOverhead = FMath::Min(FlushRenderWork(), FlushRenderWork());
	FImpactDecalBenchStep Scratch;
	Scratch.Path = Step.Path;
	auto TimeBlock = [this, FlushOverhead](TFunctionRef<void()> Work)
	{
		const double StartSeconds = FPlatformTime::Seconds();
		Work();
		FlushRenderWork();
		return static_cast<float>(FMath::Max(0.0, FPlatformTime::Seconds() - StartSeconds - FlushOverhead) *
			MicrosecondsPerSecond / M_DecalCount);
	};
	auto SpawnBatch = [this, &Scratch]()
	{
		for (int32 Index = 0; Index < M_DecalCount; ++Index)
		{
			SpawnOne(Scratch);
		}
	};
	Step.MicroCreateMicros = TimeBlock(SpawnBatch);
	Step.MicroRecycleMicros = TimeBlock(SpawnBatch);
	Step.DecalsAtEnd = GetCurrentDecalCount(Step.Path);
	Step.MicroClearMicros = TimeBlock([this]() { ClearAll(); });
	UE_LOG(LogRTSImpactDecalBench, Display,
	       TEXT("RTS_IMPACT_DECAL_BENCH micro %s per crater: create=%.2f us recycle=%.2f us clear=%.2f us"
		       " (flush overhead %.1f us, decals %d)"),
	       *Step.Label, Step.MicroCreateMicros, Step.MicroRecycleMicros, Step.MicroClearMicros,
	       FlushOverhead * MicrosecondsPerSecond, Step.DecalsAtEnd);
}

void UImpactDecalBenchmarkSubsystem::ClearAll()
{
	if (ULandscapeImpactDecalSubsystem* Pool = GetWorld()->GetSubsystem<ULandscapeImpactDecalSubsystem>())
	{
		Pool->ClearDecals();
	}
	for (UDecalComponent* Decal : M_LegacyDecals)
	{
		if (IsValid(Decal))
		{
			Decal->DestroyComponent();
		}
	}
	M_LegacyDecals.Reset();
}

int32 UImpactDecalBenchmarkSubsystem::GetCurrentDecalCount(const EImpactDecalBenchPath Path) const
{
	if (Path == EImpactDecalBenchPath::Legacy)
	{
		return M_LegacyDecals.Num();
	}
	const ULandscapeImpactDecalSubsystem* Pool = GetWorld()->GetSubsystem<ULandscapeImpactDecalSubsystem>();
	return IsValid(Pool) ? Pool->GetDecalCount() : 0;
}

void UImpactDecalBenchmarkSubsystem::SetPoolMax(const int32 MaxDecals)
{
	if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(ImpactDecalBenchPrivate::PoolCVarName))
	{
		// Same priority as the settings menu, so the menu check's Apply can still override the bench's value.
		Variable->Set(MaxDecals, ECVF_SetByGameSetting);
	}
}

void UImpactDecalBenchmarkSubsystem::RecordFrame(FImpactDecalBenchStep& Step, const double FrameMs) const
{
	Step.WallMs.Add(static_cast<float>(FrameMs));
	Step.GameThreadMs.Add(static_cast<float>(FPlatformTime::ToMilliseconds(GGameThreadTime)));
	Step.RenderThreadMs.Add(static_cast<float>(FPlatformTime::ToMilliseconds(GRenderThreadTime)));
	Step.GpuMs.Add(static_cast<float>(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0))));
}

void UImpactDecalBenchmarkSubsystem::ReportStep(const FImpactDecalBenchStep& Step) const
{
	using namespace ImpactDecalBenchPrivate;
	UE_LOG(LogRTSImpactDecalBench, Display,
	       TEXT("RTS_IMPACT_DECAL_BENCH step %s frames=%d decals=%d merged=%d | %s | %s | %s | %s | %s"),
	       *Step.Label, Step.WallMs.Num(), Step.DecalsAtEnd, Step.Merged, *DescribeMetric(TEXT("wall"), Step.WallMs),
	       *DescribeMetric(TEXT("game"), Step.GameThreadMs), *DescribeMetric(TEXT("render"), Step.RenderThreadMs),
	       *DescribeMetric(TEXT("gpu"), Step.GpuMs), *DescribeMetric(TEXT("spawn_us"), Step.SpawnMicros));
}

void UImpactDecalBenchmarkSubsystem::ReportSummary() const
{
	using namespace ImpactDecalBenchPrivate;
	for (const EImpactDecalBenchPath Path : {EImpactDecalBenchPath::Pooled, EImpactDecalBenchPath::Legacy})
	{
		TArray<float> BaselineGame, BaselineRender, BaselineGpu;
		TArray<float> HoldGame, HoldRender, HoldGpu;
		TArray<float> FillGame, FillRender, FillSpawn, RecycleGame, RecycleRender, RecycleSpawn, BurstSpawn;
		TArray<float> BurstGameSpike, BurstRenderSpike, BurstGpuSpike;
		for (const FImpactDecalBenchStep& Step : M_Steps)
		{
			if (Step.Path != Path)
			{
				continue;
			}
			switch (Step.Kind)
			{
			case EImpactDecalBenchStepKind::Baseline:
				BaselineGame.Append(Step.GameThreadMs);
				BaselineRender.Append(Step.RenderThreadMs);
				BaselineGpu.Append(Step.GpuMs);
				break;
			case EImpactDecalBenchStepKind::Hold:
				HoldGame.Append(Step.GameThreadMs);
				HoldRender.Append(Step.RenderThreadMs);
				HoldGpu.Append(Step.GpuMs);
				break;
			case EImpactDecalBenchStepKind::Fill:
				FillGame.Append(Step.GameThreadMs);
				FillRender.Append(Step.RenderThreadMs);
				FillSpawn.Append(Step.SpawnMicros);
				break;
			case EImpactDecalBenchStepKind::Recycle:
				RecycleGame.Append(Step.GameThreadMs);
				RecycleRender.Append(Step.RenderThreadMs);
				RecycleSpawn.Append(Step.SpawnMicros);
				break;
			case EImpactDecalBenchStepKind::Burst:
			{
				BurstSpawn.Append(Step.SpawnMicros);
				// The burst frame's work shows in the thread times of the next frames (they report the frame before).
				float GameSpike = 0.f, RenderSpike = 0.f, GpuSpike = 0.f;
				for (int32 Offset = 0; Offset <= 3; ++Offset)
				{
					const int32 Frame = Step.BurstFrame + Offset;
					if (not Step.WallMs.IsValidIndex(Frame))
					{
						continue;
					}
					GameSpike = FMath::Max(GameSpike, Step.GameThreadMs[Frame]);
					RenderSpike = FMath::Max(RenderSpike, Step.RenderThreadMs[Frame]);
					GpuSpike = FMath::Max(GpuSpike, Step.GpuMs[Frame]);
				}
				BurstGameSpike.Add(GameSpike);
				BurstRenderSpike.Add(RenderSpike);
				BurstGpuSpike.Add(GpuSpike);
				break;
			}
			default:
				break;
			}
		}
		TArray<float> MicroCreate, MicroRecycle, MicroClear;
		for (const FImpactDecalBenchStep& Step : M_Steps)
		{
			if (Step.Path == Path && Step.Kind == EImpactDecalBenchStepKind::Micro)
			{
				MicroCreate.Add(Step.MicroCreateMicros);
				MicroRecycle.Add(Step.MicroRecycleMicros);
				MicroClear.Add(Step.MicroClearMicros);
			}
		}
		if (not MicroCreate.IsEmpty())
		{
			UE_LOG(LogRTSImpactDecalBench, Display,
			       TEXT("RTS_IMPACT_DECAL_BENCH microsummary path=%s runs=%d per crater (median, incl. render thread):"
				       " create=%.2f us recycle=%.2f us clear=%.2f us | 100 craters over 10 s = %.3f ms/s create,"
				       " %.3f ms/s recycle"),
			       GetBenchPathName(Path), MicroCreate.Num(), GetMedian(MicroCreate), GetMedian(MicroRecycle),
			       GetMedian(MicroClear), GetMedian(MicroCreate) * 10.f / 1000.f,
			       GetMedian(MicroRecycle) * 10.f / 1000.f);
		}
		if (BaselineGpu.IsEmpty())
		{
			continue;
		}
		const float BaseGame = GetMedian(BaselineGame);
		const float BaseRender = GetMedian(BaselineRender);
		const float BaseGpu = GetMedian(BaselineGpu);
		UE_LOG(LogRTSImpactDecalBench, Display,
		       TEXT("RTS_IMPACT_DECAL_BENCH summary path=%s decals=%d | baseline p50 game=%.3f render=%.3f gpu=%.3f ms"
			       " | hold(+%d decals) p50 delta game=%+.3f render=%+.3f gpu=%+.3f ms"
			       " | fill(%d over %.0f s) p50 delta game=%+.3f render=%+.3f ms, per crater mean=%.1f p95=%.1f us"
			       " | recycle per crater mean=%.1f p95=%.1f us, p50 delta game=%+.3f render=%+.3f ms"
			       " | burst(%d in 1 frame) per crater mean=%.1f us, frame spike game=%+.2f render=%+.2f gpu=%+.2f ms"),
		       GetBenchPathName(Path), M_DecalCount, BaseGame, BaseRender, BaseGpu, M_DecalCount,
		       GetMedian(HoldGame) - BaseGame, GetMedian(HoldRender) - BaseRender, GetMedian(HoldGpu) - BaseGpu,
		       M_DecalCount, M_SpawnSeconds, GetMedian(FillGame) - BaseGame, GetMedian(FillRender) - BaseRender,
		       GetMean(FillSpawn), GetP95(FillSpawn), GetMean(RecycleSpawn), GetP95(RecycleSpawn),
		       GetMedian(RecycleGame) - BaseGame, GetMedian(RecycleRender) - BaseRender,
		       M_DecalCount, GetMean(BurstSpawn), GetMedian(BurstGameSpike) - BaseGame,
		       GetMedian(BurstRenderSpike) - BaseRender, GetMedian(BurstGpuSpike) - BaseGpu);
	}
}

void UImpactDecalBenchmarkSubsystem::SaveFrameCsv() const
{
	FString CsvText = TEXT("step,path,kind,repeat,frame,wall_ms,game_ms,render_ms,gpu_ms\n");
	for (const FImpactDecalBenchStep& Step : M_Steps)
	{
		for (int32 FrameIndex = 0; FrameIndex < Step.WallMs.Num(); ++FrameIndex)
		{
			CsvText += FString::Printf(TEXT("%s,%s,%s,%d,%d,%.4f,%.4f,%.4f,%.4f\n"), *Step.Label,
			                           ImpactDecalBenchPrivate::GetBenchPathName(Step.Path),
			                           ImpactDecalBenchPrivate::GetBenchKindName(Step.Kind), Step.Repeat, FrameIndex,
			                           Step.WallMs[FrameIndex], Step.GameThreadMs[FrameIndex],
			                           Step.RenderThreadMs[FrameIndex], Step.GpuMs[FrameIndex]);
		}
	}
	FFileHelper::SaveStringToFile(CsvText, *(M_OutputDirectory / TEXT("ImpactDecalBench_Frames.csv")));
}

void UImpactDecalBenchmarkSubsystem::TickTakingPicture()
{
	using namespace ImpactDecalBenchPrivate;
	UWorld& World = *GetWorld();
	if (M_FramesInState == 1)
	{
		ClearAll();
		SetPoolMax(M_DecalCount);
		FImpactDecalBenchStep PictureStep;
		PictureStep.Path = EImpactDecalBenchPath::Pooled;
		for (int32 Index = 0; Index < M_DecalCount; ++Index)
		{
			SpawnOne(PictureStep);
		}
		ASceneCapture2D* Capture = nullptr;
		UTextureRenderTarget2D* CaptureTarget = nullptr;
		if (FGrenadeTestHelpers::CreateCaptureCamera(World, CameraFieldOfView, Capture, CaptureTarget))
		{
			M_Capture = Capture;
			M_CaptureTarget = CaptureTarget;
			FGrenadeTestHelpers::AimLikeGameCamera(*Capture, M_Centre);
		}
		return;
	}
	if (M_FramesInState < PictureFrames)
	{
		return;
	}
	if (IsValid(M_CaptureTarget))
	{
		FGrenadeTestHelpers::SaveCapture(World, *M_CaptureTarget, M_OutputDirectory, TEXT("ImpactDecals_View.png"));
	}
	Finish(true, FString::Printf(TEXT("output %s"), *M_OutputDirectory));
}

void UImpactDecalBenchmarkSubsystem::StartLiveCombat()
{
	ClearAll();
	SetPoolMax(ImpactDecalBenchPrivate::LiveCraterCap);
	ULandscapeImpactDecalSubsystem::ResetMapTestImpactCounts();
	if (ULandscapeImpactDecalSubsystem* Pool = GetWorld()->GetSubsystem<ULandscapeImpactDecalSubsystem>())
	{
		Pool->ResetStats();
	}
	M_LiveStartGameSeconds = GetWorld()->GetTimeSeconds();
	EnterState(EImpactDecalBenchState::LiveCombat);
	if (bM_SpawnFight)
	{
		SpawnFightWave();
	}
	UE_LOG(LogRTSImpactDecalBench, Display,
	       TEXT("RTS_IMPACT_DECAL_BENCH live combat for %.0f game seconds, cap %d, map %s"), M_LiveSeconds,
	       ImpactDecalBenchPrivate::LiveCraterCap, *GetWorld()->GetMapName());
}

bool UImpactDecalBenchmarkSubsystem::CentreOnLandscape()
{
	if (bM_HasExplicitCentre)
	{
		return false;
	}
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		FVector Origin;
		FVector Extent;
		It->GetActorBounds(false, Origin, Extent);
		M_Centre = FVector(Origin.X, Origin.Y, 0.f);
		UE_LOG(LogRTSImpactDecalBench, Display, TEXT("RTS_IMPACT_DECAL_BENCH centre moved to landscape %s at %s"),
		       *It->GetName(), *M_Centre.ToCompactString());
		return true;
	}
	return false;
}

void UImpactDecalBenchmarkSubsystem::SpawnFightWave()
{
	using namespace ImpactDecalBenchPrivate;
	UWorld& World = *GetWorld();
	M_LastFightWaveGameSeconds = World.GetTimeSeconds();
	++M_FightWaves;
	for (int32 Lane = 0; Lane < FightTanksPerSide; ++Lane)
	{
		const float LaneY = (Lane - (FightTanksPerSide - 1) * 0.5f) * FightLaneSpacingCm;
		FVector GermanSpot = M_Centre + FVector(-FightHalfGapCm, LaneY, 0.f);
		FVector SovietSpot = M_Centre + FVector(FightHalfGapCm, LaneY, 0.f);
		FGrenadeTestHelpers::ProjectOntoNavMesh(World, GermanSpot, FightNavExtent);
		FGrenadeTestHelpers::ProjectOntoNavMesh(World, SovietSpot, FightNavExtent);
		const AActor* German = FGrenadeTestHelpers::SpawnUnitForPlayer(
			World, FightGermanTank, FTransform(FRotator(0.f, 0.f, 0.f), GermanSpot), FightGermanOwner);
		const AActor* Soviet = FGrenadeTestHelpers::SpawnUnitForPlayer(
			World, FightSovietTank, FTransform(FRotator(0.f, 180.f, 0.f), SovietSpot), FightSovietOwner);
		M_FightUnitsSpawned += (IsValid(German) ? 1 : 0) + (IsValid(Soviet) ? 1 : 0);
	}
}

void UImpactDecalBenchmarkSubsystem::TickLiveCombat()
{
	const double NowGameSeconds = GetWorld()->GetTimeSeconds();
	if (bM_SpawnFight && NowGameSeconds - M_LastFightWaveGameSeconds >= ImpactDecalBenchPrivate::FightWaveSeconds)
	{
		SpawnFightWave();
	}
	if (NowGameSeconds - M_LiveStartGameSeconds < M_LiveSeconds)
	{
		return;
	}
	ReportLiveCraters();
	EnterState(EImpactDecalBenchState::MenuCheck);
}

void UImpactDecalBenchmarkSubsystem::ReportLiveCraters()
{
	using namespace ImpactDecalBenchPrivate;
	int32 LandscapeImpacts = 0;
	int32 OtherImpacts = 0;
	ULandscapeImpactDecalSubsystem::GetMapTestImpactCounts(LandscapeImpacts, OtherImpacts);
	const ULandscapeImpactDecalSubsystem* Pool = GetWorld()->GetSubsystem<ULandscapeImpactDecalSubsystem>();
	if (not IsValid(Pool))
	{
		ExpectLive(false, TEXT("crater subsystem exists"));
		return;
	}
	// Every crater centre must lie on a landscape surface: compared with the landscape heightfield itself, so a tree,
	// trench or wreck standing over the crater now does not hide it. "Covered" counts craters something stands on.
	TArray<const ALandscapeProxy*> Landscapes;
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		Landscapes.Add(*It);
	}
	int32 OnLandscape = 0;
	int32 Covered = 0;
	int32 OffLandscape = 0;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ImpactDecalBenchLiveCheck), false);
	for (const FVector& Centre : Pool->GetCraterCentres())
	{
		bool bOnSurface = false;
		float NearestHeightGap = TNumericLimits<float>::Max();
		for (const ALandscapeProxy* Landscape : Landscapes)
		{
			const TOptional<float> Height = Landscape->GetHeightAtLocation(Centre);
			if (not Height.IsSet())
			{
				continue;
			}
			NearestHeightGap = FMath::Min(NearestHeightGap, FMath::Abs(Height.GetValue() - Centre.Z));
			if (NearestHeightGap <= CraterSurfaceToleranceCm)
			{
				bOnSurface = true;
				break;
			}
		}
		if (not bOnSurface)
		{
			++OffLandscape;
			// Context for the off-surface crater: hole in the heightfield (no height) or how far off, and the slope.
			FHitResult GroundHit;
			const FVector Up(0.f, 0.f, CraterCheckTraceHalfHeight);
			const bool bHitGround = GetWorld()->LineTraceSingleByChannel(
				GroundHit, Centre + Up, Centre - Up, ECC_Visibility, QueryParams);
			UE_LOG(LogRTSImpactDecalBench, Display,
			       TEXT("RTS_IMPACT_DECAL_BENCH off-surface crater at %s: heightfield gap %s, trace %s normal z %.2f"),
			       *Centre.ToCompactString(),
			       NearestHeightGap == TNumericLimits<float>::Max()
				       ? TEXT("none (hole / outside)")
				       : *FString::Printf(TEXT("%.0f cm"), NearestHeightGap),
			       bHitGround && IsValid(GroundHit.GetActor()) ? *GroundHit.GetActor()->GetClass()->GetName() : TEXT("nothing"),
			       bHitGround ? GroundHit.ImpactNormal.Z : 0.f);
			continue;
		}
		++OnLandscape;
		FHitResult Hit;
		const FVector Up(0.f, 0.f, CraterCheckTraceHalfHeight);
		const bool bTopIsLandscape = GetWorld()->LineTraceSingleByChannel(
			Hit, Centre + Up, Centre - Up, ECC_Visibility, QueryParams) &&
			ULandscapeImpactDecalSubsystem::GetIsLandscapeActor(Hit.GetActor());
		Covered += bTopIsLandscape ? 0 : 1;
	}
	const FLandscapeImpactDecalStats& Stats = Pool->GetStats();
	if (bM_SpawnFight)
	{
		UE_LOG(LogRTSImpactDecalBench, Display, TEXT("RTS_IMPACT_DECAL_BENCH live spawned fight: %d waves, %d tanks at %s"),
		       M_FightWaves, M_FightUnitsSpawned, *M_Centre.ToCompactString());
	}
	UE_LOG(LogRTSImpactDecalBench, Display,
	       TEXT("RTS_IMPACT_DECAL_BENCH live map=%s shell impacts >15mm: landscape=%d other=%d | craters=%d added=%d"
		       " merged=%d | crater centres on landscape surface=%d (of which covered by something=%d) off-landscape=%d"),
	       *GetWorld()->GetMapName(), LandscapeImpacts, OtherImpacts, Pool->GetDecalCount(), Stats.AddedDecals,
	       Stats.MergedIntoExisting, OnLandscape, Covered, OffLandscape);
	UE_LOG(LogRTSImpactDecalBench, Display,
	       TEXT("RTS_IMPACT_DECAL_BENCH live bombs: landscape bomb impacts=%d bomb craters added=%d bomb craters on map=%d"),
	       ULandscapeImpactDecalSubsystem::GetMapTestBombLandscapeImpacts(), Stats.BombCraters,
	       [Pool]()
	       {
		       int32 Count = 0;
		       for (int32 Slot = 0; Slot < Pool->GetDecalCount(); ++Slot)
		       {
			       Count += Pool->GetIsBombCrater(Slot) ? 1 : 0;
		       }
		       return Count;
	       }());
	UE_LOG(LogRTSImpactDecalBench, Display, TEXT("RTS_IMPACT_DECAL_BENCH live other hits by class: %s"),
	       *ULandscapeImpactDecalSubsystem::DescribeMapTestOtherHitClasses());
	ExpectLive(LandscapeImpacts > 0, TEXT("the map's fighting hit the landscape at least once"));
	ExpectLive(Stats.Requests == LandscapeImpacts,
	           TEXT("every landscape impact (and no other impact) requested a crater"));
	ExpectLive(OffLandscape == 0, TEXT("no crater off the landscape"));
}

void UImpactDecalBenchmarkSubsystem::TickMenuCheck()
{
	using namespace ImpactDecalBenchPrivate;
	if (M_FramesInState == 1)
	{
		UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, SettingsWidgetPath);
		APlayerController* Controller = M_PlayerController.IsValid()
			                                ? M_PlayerController.Get()
			                                : GetWorld()->GetFirstPlayerController();
		if (WidgetClass == nullptr || Controller == nullptr)
		{
			ExpectLive(false, TEXT("settings widget class and player controller exist"));
			Finish(false, FString::Join(M_LiveFailures, TEXT("; ")));
			return;
		}
		M_SettingsWidget = CreateWidget<UUserWidget>(Controller, WidgetClass);
		if (IsValid(M_SettingsWidget))
		{
			M_SettingsWidget->AddToViewport();
		}
		return;
	}
	if (M_FramesInState < MenuCheckFrame)
	{
		return;
	}
	RunEvictionCheck();
	RunSizeCheck();
	RunMenuCheck();
	if (IsValid(M_SettingsWidget))
	{
		M_SettingsWidget->RemoveFromParent();
	}
	const bool bPassed = M_LiveFailures.IsEmpty();
	Finish(bPassed, bPassed ? TEXT("live craters and Shell Craters menu ok")
		                    : FString::Join(M_LiveFailures, TEXT("; ")));
}

void UImpactDecalBenchmarkSubsystem::RunEvictionCheck()
{
	using namespace ImpactDecalBenchPrivate;
	ULandscapeImpactDecalSubsystem* Pool = GetWorld()->GetSubsystem<ULandscapeImpactDecalSubsystem>();
	if (not IsValid(Pool) || M_Points.Num() < EvictionCraterCount + 1)
	{
		UE_LOG(LogRTSImpactDecalBench, Display, TEXT("RTS_IMPACT_DECAL_BENCH eviction check skipped (no landscape points)"));
		return;
	}
	const int32 PreviousMax = ULandscapeImpactDecalSubsystem::GetMaxDecalsSetting();
	Pool->ClearDecals();
	SetPoolMax(EvictionCraterCount);
	// 20 mm craters (40 cm) at points metres apart: nothing merges, so every request is one crater.
	auto ContainsCentre = [](const TArray<FVector>& Centres, const FVector& Location)
	{
		return Centres.ContainsByPredicate([&Location](const FVector& Centre) { return Centre.Equals(Location, 1.f); });
	};
	for (int32 Index = 0; Index < EvictionCraterCount; ++Index)
	{
		Pool->AddImpactDecal(EvictionCalibreMm, M_Points[Index].Location, M_Points[Index].Normal);
	}
	ExpectLive(Pool->GetDecalCount() == EvictionCraterCount, TEXT("cap 10: ten craters on the map"));
	Pool->AddImpactDecal(EvictionCalibreMm, M_Points[EvictionCraterCount].Location, M_Points[EvictionCraterCount].Normal);
	const TArray<FVector>& AfterEleventh = Pool->GetCraterCentres();
	bool bAllButFirstRemain = ContainsCentre(AfterEleventh, M_Points[EvictionCraterCount].Location);
	for (int32 Index = 1; Index < EvictionCraterCount; ++Index)
	{
		bAllButFirstRemain = bAllButFirstRemain && ContainsCentre(AfterEleventh, M_Points[Index].Location);
	}
	const bool bFirstGone = not ContainsCentre(AfterEleventh, M_Points[0].Location);
	UE_LOG(LogRTSImpactDecalBench, Display,
	       TEXT("RTS_IMPACT_DECAL_BENCH eviction: 11th crater at cap 10 -> count %d, first (oldest) gone=%s, craters 2-11 present=%s"),
	       Pool->GetDecalCount(), bFirstGone ? TEXT("yes") : TEXT("no"), bAllButFirstRemain ? TEXT("yes") : TEXT("no"));
	ExpectLive(Pool->GetDecalCount() == EvictionCraterCount && bFirstGone && bAllButFirstRemain,
	           TEXT("over the cap the oldest crater (and only it) is removed"));
	// Lowering the cap keeps the newest: craters 7-11 (indices 6-10) must remain.
	SetPoolMax(EvictionShrunkCount);
	const TArray<FVector>& AfterShrink = Pool->GetCraterCentres();
	bool bNewestRemain = Pool->GetDecalCount() == EvictionShrunkCount;
	for (int32 Index = EvictionCraterCount + 1 - EvictionShrunkCount; Index <= EvictionCraterCount; ++Index)
	{
		bNewestRemain = bNewestRemain && ContainsCentre(AfterShrink, M_Points[Index].Location);
	}
	UE_LOG(LogRTSImpactDecalBench, Display,
	       TEXT("RTS_IMPACT_DECAL_BENCH eviction: cap 10 -> 5 -> count %d, newest five kept=%s"), Pool->GetDecalCount(),
	       bNewestRemain ? TEXT("yes") : TEXT("no"));
	ExpectLive(bNewestRemain, TEXT("lowering the cap keeps the newest craters"));
	Pool->ClearDecals();
	SetPoolMax(PreviousMax);
}

void UImpactDecalBenchmarkSubsystem::RunSizeCheck()
{
	using namespace ImpactDecalBenchPrivate;
	ULandscapeImpactDecalSubsystem* Pool = GetWorld()->GetSubsystem<ULandscapeImpactDecalSubsystem>();
	if (not IsValid(Pool) || M_Points.Num() < 2)
	{
		UE_LOG(LogRTSImpactDecalBench, Display, TEXT("RTS_IMPACT_DECAL_BENCH size check skipped (no landscape points)"));
		return;
	}
	// Exact sizes: switch the random spread off for the check (restored below).
	URTSImpactDecalSettings* Settings = GetMutableDefault<URTSImpactDecalSettings>();
	const FFloatInterval PreviousRandom = Settings->RandomSizeScale;
	Settings->RandomSizeScale = FFloatInterval(1.f, 1.f);
	const int32 PreviousMax = ULandscapeImpactDecalSubsystem::GetMaxDecalsSetting();
	SetPoolMax(SizeCheckPoolMax);
	const FVector Location = M_Points[0].Location;
	const FVector Normal = M_Points[0].Normal;

	// One crater at a time on an empty pool, so nothing merges and the last radius is the new crater.
	auto MeasureShell = [&](const float CalibreMm)
	{
		Pool->ClearDecals();
		Pool->AddImpactDecal(CalibreMm, Location, Normal);
		return Pool->GetCraterRadii().IsEmpty() ? 0.f : 2.f * Pool->GetCraterRadii().Last();
	};
	auto MeasureBomb = [&](const float TntGrams, bool& bOutIsBomb)
	{
		Pool->ClearDecals();
		Pool->AddBombImpactDecal(TntGrams, Location, Normal);
		bOutIsBomb = Pool->GetDecalCount() == 1 && Pool->GetIsBombCrater(0);
		return Pool->GetCraterRadii().IsEmpty() ? 0.f : 2.f * Pool->GetCraterRadii().Last();
	};

	// Shells 75 mm and up: larger than the previous defaults (160/210/290/400/600 cm at the design calibre).
	const float ShellCalibres[] = {20.f, 37.f, 50.f, 75.f, 88.f, 105.f, 122.f, 150.f, 240.f};
	const float PreviousShellDiameters[] = {40.f, 70.f, 110.f, 160.f, 210.f, 290.f, 290.f, 400.f, 600.f};
	bool bLargerFrom75 = true;
	float Shell240Diameter = 0.f;
	TArray<FString> ShellParts;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ShellCalibres); ++Index)
	{
		const float Diameter = MeasureShell(ShellCalibres[Index]);
		ShellParts.Add(FString::Printf(TEXT("%.0fmm=%.0fcm (old band design %.0f)"), ShellCalibres[Index], Diameter,
		                               PreviousShellDiameters[Index]));
		if (ShellCalibres[Index] >= 75.f)
		{
			bLargerFrom75 = bLargerFrom75 && Diameter >= PreviousShellDiameters[Index] * SizeCheckMinGrowth;
		}
		if (ShellCalibres[Index] >= 240.f)
		{
			Shell240Diameter = Diameter;
		}
	}
	UE_LOG(LogRTSImpactDecalBench, Display, TEXT("RTS_IMPACT_DECAL_BENCH sizes shell: %s"),
	       *FString::Join(ShellParts, TEXT(", ")));
	ExpectLive(bLargerFrom75, TEXT("shell craters 75 mm and up are at least 1.3x the previous size"));

	// Bombs: own set, by explosive weight, growing with the charge and all larger than the largest shell crater.
	const float BombGrams[] = {250.f, 400.f, 500.f, 700.f, 800.f, 1000.f, 1500.f, 2000.f, 3000.f, 5000.f};
	bool bAllBomb = true;
	bool bGrowing = true;
	float PreviousDiameter = 0.f;
	float Bomb250Diameter = 0.f;
	TArray<FString> BombParts;
	for (const float Grams : BombGrams)
	{
		bool bIsBomb = false;
		const float Diameter = MeasureBomb(Grams, bIsBomb);
		BombParts.Add(FString::Printf(TEXT("%.0f=%.0fcm%s"), Grams, Diameter, bIsBomb ? TEXT("") : TEXT(" (NOT bomb set)")));
		bAllBomb = bAllBomb && bIsBomb;
		bGrowing = bGrowing && Diameter > PreviousDiameter;
		PreviousDiameter = Diameter;
		if (Grams <= 250.f)
		{
			Bomb250Diameter = Diameter;
		}
	}
	UE_LOG(LogRTSImpactDecalBench, Display, TEXT("RTS_IMPACT_DECAL_BENCH sizes bomb (TNT grams): %s"),
	       *FString::Join(BombParts, TEXT(", ")));
	ExpectLive(bAllBomb, TEXT("every bomb weight leaves a bomb-set crater"));
	ExpectLive(bGrowing, TEXT("bomb craters grow with the charge"));
	ExpectLive(Bomb250Diameter > Shell240Diameter, TEXT("the smallest bomb crater is larger than the 240 mm shell crater"));

	// Merging across the sets: a shell into a bomb crater adds nothing; a bomb on a shell crater is a new crater.
	Pool->ClearDecals();
	Pool->AddBombImpactDecal(500.f, Location, Normal);
	Pool->AddImpactDecal(150.f, Location, Normal);
	const bool bShellMergedIntoBomb = Pool->GetDecalCount() == 1;
	Pool->ClearDecals();
	Pool->AddImpactDecal(240.f, Location, Normal);
	Pool->AddBombImpactDecal(250.f, Location, Normal);
	const bool bBombAddedOverShell = Pool->GetDecalCount() == 2 && Pool->GetIsBombCrater(1);
	UE_LOG(LogRTSImpactDecalBench, Display,
	       TEXT("RTS_IMPACT_DECAL_BENCH sizes merge: 150 mm shell into 500 bomb crater merged=%s, 250 bomb on 240 mm crater added=%s"),
	       bShellMergedIntoBomb ? TEXT("yes") : TEXT("no"), bBombAddedOverShell ? TEXT("yes") : TEXT("no"));
	ExpectLive(bShellMergedIntoBomb && bBombAddedOverShell, TEXT("shell/bomb merge order"));

	// Ring recycling across the sets swaps the material: fill with shell craters, then bombs push them out.
	Pool->ClearDecals();
	SetPoolMax(2);
	Pool->AddImpactDecal(20.f, M_Points[0].Location, M_Points[0].Normal);
	Pool->AddImpactDecal(20.f, M_Points[1].Location, M_Points[1].Normal);
	Pool->AddBombImpactDecal(250.f, M_Points[0].Location + FVector(5000.f, 0.f, 0.f), M_Points[0].Normal);
	const bool bRecycledToBomb = Pool->GetDecalCount() == 2 && Pool->GetIsBombCrater(0) && not Pool->GetIsBombCrater(1);
	ExpectLive(bRecycledToBomb, TEXT("a recycled shell crater becomes a bomb crater"));

	Pool->ClearDecals();
	Settings->RandomSizeScale = PreviousRandom;
	SetPoolMax(PreviousMax);
}

void UImpactDecalBenchmarkSubsystem::RunMenuCheck()
{
	using namespace ImpactDecalBenchPrivate;
	if (not IsValid(M_SettingsWidget) || not IsValid(M_SettingsWidget->WidgetTree))
	{
		ExpectLive(false, TEXT("settings widget created"));
		return;
	}
	UComboBoxString* Combo = Cast<UComboBoxString>(M_SettingsWidget->WidgetTree->FindWidget(ImpactDecalsComboName));
	const UWidget* Label = M_SettingsWidget->WidgetTree->FindWidget(ImpactDecalsLabelName);
	ExpectLive(IsValid(Label), TEXT("Shell Craters label is in the menu"));
	if (not IsValid(Combo))
	{
		ExpectLive(false, TEXT("Shell Craters combo is in the menu"));
		return;
	}
	TArray<FString> Options;
	for (int32 Index = 0; Index < Combo->GetOptionCount(); ++Index)
	{
		Options.Add(Combo->GetOptionAtIndex(Index));
	}
	const FString OriginalOption = Combo->GetSelectedOption();
	UE_LOG(LogRTSImpactDecalBench, Display,
	       TEXT("RTS_IMPACT_DECAL_BENCH menu options=[%s] selected=%s rts.ImpactDecals=%d"),
	       *FString::Join(Options, TEXT(", ")), *OriginalOption, ULandscapeImpactDecalSubsystem::GetMaxDecalsSetting());
	ExpectLive(Options.Num() == MenuOptionCount, TEXT("combo lists Off/50/100/200/400"));
	ExpectLive(Combo->GetSelectedIndex() != INDEX_NONE, TEXT("combo shows the current level"));

	ULandscapeImpactDecalSubsystem* Pool = GetWorld()->GetSubsystem<ULandscapeImpactDecalSubsystem>();
	const bool bHadCraters = IsValid(Pool) && Pool->GetDecalCount() > 0;
	ExpectLive(SelectAndApplyImpactDecalsOption(*Combo, global_GetImpactDecalsText(ERTSImpactDecalsLevel::Off).ToString(), 0),
	           TEXT("Off applies rts.ImpactDecals 0"));
	ExpectLive(IsValid(Pool) && Pool->GetDecalCount() == 0, TEXT("Off removes the craters on the map"));
	if (IsValid(Pool) && not M_Points.IsEmpty())
	{
		const FImpactDecalBenchPoint& Point = M_Points[0];
		Pool->AddImpactDecal(Point.CalibreMm, Point.Location, Point.Normal);
		ExpectLive(Pool->GetDecalCount() == 0, TEXT("Off spawns no crater"));
	}
	const int32 ManyCount = global_GetImpactDecalsCount(ERTSImpactDecalsLevel::Many);
	ExpectLive(SelectAndApplyImpactDecalsOption(*Combo, global_GetImpactDecalsText(ERTSImpactDecalsLevel::Many).ToString(),
	                                            ManyCount), TEXT("200 applies rts.ImpactDecals 200"));
	if (IsValid(Pool) && not M_Points.IsEmpty())
	{
		const FImpactDecalBenchPoint& Point = M_Points[0];
		Pool->AddImpactDecal(Point.CalibreMm, Point.Location, Point.Normal);
		ExpectLive(Pool->GetDecalCount() == 1, TEXT("craters spawn again after switching back on"));
	}
	// Leave the player's own level behind (Apply saved the config).
	if (not OriginalOption.IsEmpty())
	{
		const int32 OriginalIndex = Combo->FindOptionIndex(OriginalOption);
		SelectAndApplyImpactDecalsOption(*Combo, OriginalOption,
		                                 global_GetImpactDecalsCount(static_cast<ERTSImpactDecalsLevel>(OriginalIndex)));
	}
	UE_LOG(LogRTSImpactDecalBench, Display, TEXT("RTS_IMPACT_DECAL_BENCH menu done (had live craters before Off: %s)"),
	       bHadCraters ? TEXT("yes") : TEXT("no"));
}

bool UImpactDecalBenchmarkSubsystem::SelectAndApplyImpactDecalsOption(UComboBoxString& Combo, const FString& Option,
                                                                        const int32 ExpectedMax)
{
	// SetSelectedOption fires OnSelectionChanged, the same handler a click runs (pending level in the subsystem).
	Combo.SetSelectedOption(Option);
	URTSSettingsMenuSubsystem* Settings = GetWorld()->GetGameInstance() != nullptr
		                                      ? GetWorld()->GetGameInstance()->GetSubsystem<URTSSettingsMenuSubsystem>()
		                                      : nullptr;
	if (Settings == nullptr)
	{
		return false;
	}
	FRTSSettingsSnapshot Pending;
	Settings->GetPendingSettingsValues(Pending);
	const ERTSImpactDecalsLevel PendingLevel = Pending.M_GraphicsSettings.M_EnvironmentDetailSettings.M_ImpactDecalsLevel;
	Settings->ApplySettings();
	const int32 AppliedMax = ULandscapeImpactDecalSubsystem::GetMaxDecalsSetting();
	UE_LOG(LogRTSImpactDecalBench, Display,
	       TEXT("RTS_IMPACT_DECAL_BENCH menu selected %s -> pending %s -> Apply -> rts.ImpactDecals=%d (expected %d)"),
	       *Option, *global_GetImpactDecalsText(PendingLevel).ToString(), AppliedMax, ExpectedMax);
	return AppliedMax == ExpectedMax && global_GetImpactDecalsText(PendingLevel).ToString() == Option;
}

void UImpactDecalBenchmarkSubsystem::ExpectLive(const bool bCondition, const FString& What)
{
	UE_LOG(LogRTSImpactDecalBench, Display, TEXT("RTS_IMPACT_DECAL_BENCH check %s: %s"),
	       bCondition ? TEXT("ok") : TEXT("FAILED"), *What);
	if (not bCondition)
	{
		M_LiveFailures.Add(What);
	}
}

#else

bool UImpactDecalBenchmarkSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UImpactDecalBenchmarkSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UImpactDecalBenchmarkSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UImpactDecalBenchmarkSubsystem::Tick(float DeltaTime)
{
}

TStatId UImpactDecalBenchmarkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UImpactDecalBenchmarkSubsystem, STATGROUP_Tickables);
}

#endif
