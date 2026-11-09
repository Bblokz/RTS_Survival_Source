// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PerfProbeSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "PipelineStateCache.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSPerfProbe, Log, All);

namespace PerfProbePrivate
{
	const TCHAR* const ProbeSwitch = TEXT("PerfProbe");
	const TCHAR* const ViewsSwitch = TEXT("PerfProbeViews=");
	const TCHAR* const FramesSwitch = TEXT("PerfProbeFrames=");
	const TCHAR* const WarmupSwitch = TEXT("PerfProbeWarmup=");
	const TCHAR* const RepeatsSwitch = TEXT("PerfProbeRepeats=");
	const TCHAR* const VariantsSwitch = TEXT("PerfProbeVariants=");
	const TCHAR* const ProfileGpuSwitch = TEXT("PerfProbeProfileGPU");
	const TCHAR* const KeepUISwitch = TEXT("PerfProbeKeepUI");
	const TCHAR* const FovSwitch = TEXT("PerfProbeFov=");
	const TCHAR* const NameSwitch = TEXT("PerfProbeName=");
	const TCHAR* const MeshesSwitch = TEXT("PerfProbeMeshes=");
	const TCHAR* const MeshAtSwitch = TEXT("PerfProbeMeshAt=");
	const TCHAR* const MeshSettleSwitch = TEXT("PerfProbeMeshSettle=");
	const TCHAR* const MeshCloseOnlySwitch = TEXT("PerfProbeMeshCloseOnly");
	const TCHAR* const TimelapseSwitch = TEXT("PerfProbeTimelapse=");

	// Mesh shots: frames to settle (TSR history, streaming) before each screenshot, and the two views.
	constexpr int32 DefaultMeshShotSettleFrames = 90;
	// Two views (close-up, RTS distance) x three LOD variants: forced LOD0, forced LOD1/last, automatic (skeletal only;
	// logs the LOD the engine picks itself).
	constexpr int32 MeshShotLodVariants = 3;
	constexpr int32 MeshShotSlotsPerMesh = 2 * MeshShotLodVariants;
	constexpr int32 AutoLodVariant = 2;
	constexpr float MeshCloseUpDistanceInRadii = 2.6f;
	constexpr float MeshCloseUpPitch = -30.f;
	constexpr float MeshCloseUpYaw = 35.f;
	constexpr float MeshCloseUpFov = 40.f;
	constexpr float MeshRtsDistance = 3140.f;
	constexpr float MeshRtsPitch = -70.f;
	constexpr float MeshRtsFov = 90.f;
	constexpr float GroundTraceHalfHeight = 100000.f;

	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 120.0;
	constexpr double MaxShaderCompileSeconds = 1800.0;
	// After the shader and PSO queues are empty, streaming and Lumen still settle for a moment.
	constexpr int32 ShaderQuietFrames = 300;
	// Proxies wait for their PSOs (r.PSOPrecache.ProxyCreationWhenPSOReady) and draw the default material meanwhile;
	// the queues can read empty in the first seconds before the requests are made.
	constexpr double MinSettleSeconds = 30.0;

	float Median(TArray<float> Values)
	{
		if (Values.Num() == 0)
		{
			return 0.f;
		}
		Values.Sort();
		return Values[Values.Num() / 2];
	}

	float Percentile(TArray<float> Values, const float Fraction)
	{
		if (Values.Num() == 0)
		{
			return 0.f;
		}
		Values.Sort();
		const int32 Index = FMath::Clamp(FMath::FloorToInt(Values.Num() * Fraction), 0, Values.Num() - 1);
		return Values[Index];
	}
}

bool UPerfProbeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), PerfProbePrivate::ProbeSwitch);
}

void UPerfProbeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	using namespace PerfProbePrivate;
	Super::OnWorldBeginPlay(InWorld);
	FParse::Value(FCommandLine::Get(), FramesSwitch, M_MeasureFrames);
	FParse::Value(FCommandLine::Get(), WarmupSwitch, M_WarmupFrames);
	FParse::Value(FCommandLine::Get(), RepeatsSwitch, M_Repeats);
	FParse::Value(FCommandLine::Get(), FovSwitch, M_FieldOfView);
	M_MeasureFrames = FMath::Max(M_MeasureFrames, 10);
	M_Repeats = FMath::Max(M_Repeats, 1);
	bM_ProfilesGpu = FParse::Param(FCommandLine::Get(), ProfileGpuSwitch);
	bM_KeepsUI = FParse::Param(FCommandLine::Get(), KeepUISwitch);
	ParseViews();
	ParseVariants();
	FString MeshesText;
	if (FParse::Value(FCommandLine::Get(), MeshesSwitch, MeshesText, false))
	{
		MeshesText.ParseIntoArray(M_MeshPaths, TEXT(";"));
	}
	FString MeshAtText;
	if (FParse::Value(FCommandLine::Get(), MeshAtSwitch, MeshAtText, false))
	{
		FString XText;
		FString YText;
		if (MeshAtText.Split(TEXT(","), &XText, &YText))
		{
			M_MeshSpot = FVector2D(FCString::Atof(*XText), FCString::Atof(*YText));
		}
	}
	// An error dialog costs milliseconds in an editor build and would be measured instead of the scene.
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	EnterPhase(ParseTimelapse() ? EPerfProbePhase::Timelapse : EPerfProbePhase::WaitingForGameStart);
	UE_LOG(LogRTSPerfProbe, Display,
	       TEXT("RTS_PERF_PROBE started map=%s views=%d variants=%d repeats=%d frames=%d warmup=%d profilegpu=%s"),
	       *InWorld.GetMapName(), M_Views.Num(), M_Variants.Num(), M_Repeats, M_MeasureFrames, M_WarmupFrames,
	       bM_ProfilesGpu ? TEXT("yes") : TEXT("no"));
}

void UPerfProbeSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

TStatId UPerfProbeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPerfProbeSubsystem, STATGROUP_Tickables);
}

void UPerfProbeSubsystem::ParseViews()
{
	FString ViewsText;
	if (not FParse::Value(FCommandLine::Get(), PerfProbePrivate::ViewsSwitch, ViewsText, false))
	{
		return;
	}
	TArray<FString> ViewTexts;
	ViewsText.ParseIntoArray(ViewTexts, TEXT(";"));
	for (const FString& ViewText : ViewTexts)
	{
		TArray<FString> Parts;
		ViewText.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() < 5)
		{
			UE_LOG(LogRTSPerfProbe, Warning, TEXT("RTS_PERF_PROBE ignored view '%s' (need X,Y,Z,Pitch,Yaw)"), *ViewText);
			continue;
		}
		FPerfProbeView& View = M_Views.AddDefaulted_GetRef();
		View.Location = FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
		View.Rotation = FRotator(FCString::Atof(*Parts[3]), FCString::Atof(*Parts[4]), 0.f);
	}
}

void UPerfProbeSubsystem::ParseVariants()
{
	FString VariantsText;
	if (FParse::Value(FCommandLine::Get(), PerfProbePrivate::VariantsSwitch, VariantsText, false))
	{
		TArray<FString> VariantTexts;
		VariantsText.ParseIntoArray(VariantTexts, TEXT("|"), false);
		for (const FString& VariantText : VariantTexts)
		{
			TArray<FString> Commands;
			VariantText.ParseIntoArray(Commands, TEXT("&"));
			for (FString& Command : Commands)
			{
				Command.TrimStartAndEndInline();
			}
			M_Variants.Add(Commands);
		}
	}
	if (M_Variants.Num() == 0)
	{
		M_Variants.AddDefaulted();
	}
}

void UPerfProbeSubsystem::BuildSteps()
{
	M_Steps.Reset();
	for (int32 ViewIndex = 0; ViewIndex < M_Views.Num(); ++ViewIndex)
	{
		for (int32 Repeat = 0; Repeat < M_Repeats; ++Repeat)
		{
			for (int32 VariantIndex = 0; VariantIndex < M_Variants.Num(); ++VariantIndex)
			{
				FPerfProbeStep& Step = M_Steps.AddDefaulted_GetRef();
				Step.ViewIndex = ViewIndex;
				Step.VariantIndex = VariantIndex;
				Step.Repeat = Repeat;
			}
		}
	}
}

void UPerfProbeSubsystem::EnterPhase(const EPerfProbePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
	M_FramesInPhase = 0;
}

double UPerfProbeSubsystem::GetRealSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartRealSeconds;
}

void UPerfProbeSubsystem::Finish(const bool bSucceeded, const FString& Reason)
{
	M_Phase = EPerfProbePhase::Done;
	UE_LOG(LogRTSPerfProbe, Display, TEXT("RTS_PERF_PROBE RESULT %s %s"), bSucceeded ? TEXT("PASS") : TEXT("FAIL"),
	       *Reason);
	FPlatformMisc::RequestExit(false, TEXT("PerfProbe"));
}

void UPerfProbeSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	++M_FramesInPhase;
	switch (M_Phase)
	{
	case EPerfProbePhase::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case EPerfProbePhase::WaitingForShaders:
		KeepViewRendering();
		TickWaitingForShaders();
		break;
	case EPerfProbePhase::WarmingUp:
		KeepViewRendering();
		TickWarmingUp();
		break;
	case EPerfProbePhase::Measuring:
		KeepViewRendering();
		TickMeasuring();
		break;
	case EPerfProbePhase::MeshShots:
		KeepViewRendering();
		TickMeshShots();
		break;
	case EPerfProbePhase::Timelapse:
		TickTimelapse();
		break;
	default:
		break;
	}
}

void UPerfProbeSubsystem::TickWaitingForGameStart()
{
	using namespace PerfProbePrivate;
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
	if (M_Views.Num() == 0)
	{
		// The player's own RTS camera at game start.
		const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
		FPerfProbeView& View = M_Views.AddDefaulted_GetRef();
		if (IsValid(CameraManager))
		{
			View.Location = CameraManager->GetCameraLocation();
			View.Rotation = CameraManager->GetCameraRotation();
			M_FieldOfView = CameraManager->GetFOVAngle();
		}
	}
	BuildSteps();
	if (not bM_KeepsUI)
	{
		UWidgetLayoutLibrary::RemoveAllWidgets(GetWorld());
	}
	EnsureCamera();
	EnterPhase(EPerfProbePhase::WaitingForShaders);
}

void UPerfProbeSubsystem::TickWaitingForShaders()
{
	using namespace PerfProbePrivate;
	const int32 RemainingShaderJobs = GShaderCompilingManager != nullptr
		                                  ? GShaderCompilingManager->GetNumRemainingJobs()
		                                  : 0;
	const uint32 RemainingPsoRequests = PipelineStateCache::NumActivePrecacheRequests();
	if ((RemainingShaderJobs > 0 || RemainingPsoRequests > 0) && GetRealSecondsInPhase() < MaxShaderCompileSeconds)
	{
		M_FramesInPhase = 0;
		return;
	}
	if (M_FramesInPhase < ShaderQuietFrames || GetRealSecondsInPhase() < MinSettleSeconds)
	{
		return;
	}
	UE_LOG(LogRTSPerfProbe, Display, TEXT("RTS_PERF_PROBE shaders ready after %.1f s"), GetRealSecondsInPhase());
	if (M_MeshPaths.Num() > 0)
	{
		M_MeshShotIndex = 0;
		EnterPhase(EPerfProbePhase::MeshShots);
		return;
	}
	BeginStep(0);
}

void UPerfProbeSubsystem::BeginStep(const int32 StepIndex)
{
	if (not M_Steps.IsValidIndex(StepIndex))
	{
		LogSummary();
		Finish(true, FString::Printf(TEXT("%d steps measured"), M_Steps.Num()));
		return;
	}
	M_StepIndex = StepIndex;
	const FPerfProbeStep& Step = M_Steps[StepIndex];
	for (const FString& Command : M_Variants[Step.VariantIndex])
	{
		GEngine->Exec(GetWorld(), *Command);
	}
	EnsureCamera();
	if (M_Camera.IsValid())
	{
		const FPerfProbeView& View = M_Views[Step.ViewIndex];
		M_Camera->SetActorLocationAndRotation(View.Location, View.Rotation);
	}
	EnterPhase(EPerfProbePhase::WarmingUp);
}

void UPerfProbeSubsystem::TickWarmingUp()
{
	if (M_FramesInPhase < M_WarmupFrames)
	{
		return;
	}
	const FPerfProbeStep& Step = M_Steps[M_StepIndex];
	if (Step.Repeat == 0)
	{
		FString Name = TEXT("probe");
		FParse::Value(FCommandLine::Get(), PerfProbePrivate::NameSwitch, Name);
		// One shot per view and variant, so an A/B variant can be checked for visible change.
		const FString Path = FPaths::ProjectSavedDir() / TEXT("PerfAudit") / TEXT("Shots") /
			(Step.VariantIndex == 0
				 ? FString::Printf(TEXT("%s_view%d.png"), *Name, Step.ViewIndex)
				 : FString::Printf(TEXT("%s_view%d_var%d.png"), *Name, Step.ViewIndex, Step.VariantIndex));
		FScreenshotRequest::RequestScreenshot(Path, false, false);
	}
	EnterPhase(EPerfProbePhase::Measuring);
}

void UPerfProbeSubsystem::TickMeasuring()
{
	FPerfProbeStep& Step = M_Steps[M_StepIndex];
	// Skip the first frames: the GPU time reported lags the game thread by a frame or two.
	if (M_FramesInPhase > 3)
	{
		Step.GpuMs.Add(static_cast<float>(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0))));
		Step.GameThreadMs.Add(static_cast<float>(FPlatformTime::ToMilliseconds(GGameThreadTime)));
		Step.RenderThreadMs.Add(static_cast<float>(FPlatformTime::ToMilliseconds(GRenderThreadTime)));
	}
	if (bM_ProfilesGpu && M_FramesInPhase == M_MeasureFrames / 2 && Step.Repeat == 0)
	{
		UE_LOG(LogRTSPerfProbe, Display, TEXT("RTS_PERF_PROBE profilegpu view=%d variant=%d"), Step.ViewIndex,
		       Step.VariantIndex);
		GEngine->Exec(GetWorld(), TEXT("ProfileGPU"));
	}
	if (M_FramesInPhase < M_MeasureFrames + 3)
	{
		return;
	}
	LogStep(Step);
	BeginStep(M_StepIndex + 1);
}

void UPerfProbeSubsystem::LogStep(const FPerfProbeStep& Step) const
{
	using namespace PerfProbePrivate;
	const FString Commands = FString::Join(M_Variants[Step.VariantIndex], TEXT(" & "));
	UE_LOG(LogRTSPerfProbe, Display,
	       TEXT("RTS_PERF_PROBE step view=%d variant=%d repeat=%d gpu_median=%.3f gpu_p90=%.3f game=%.3f render=%.3f frames=%d cmds=[%s]"),
	       Step.ViewIndex, Step.VariantIndex, Step.Repeat, Median(Step.GpuMs), Percentile(Step.GpuMs, 0.9f),
	       Median(Step.GameThreadMs), Median(Step.RenderThreadMs), Step.GpuMs.Num(), *Commands);
}

void UPerfProbeSubsystem::LogSummary() const
{
	using namespace PerfProbePrivate;
	for (int32 ViewIndex = 0; ViewIndex < M_Views.Num(); ++ViewIndex)
	{
		const FPerfProbeView& View = M_Views[ViewIndex];
		for (int32 VariantIndex = 0; VariantIndex < M_Variants.Num(); ++VariantIndex)
		{
			TArray<float> Medians;
			for (const FPerfProbeStep& Step : M_Steps)
			{
				if (Step.ViewIndex == ViewIndex && Step.VariantIndex == VariantIndex)
				{
					Medians.Add(Median(Step.GpuMs));
				}
			}
			UE_LOG(LogRTSPerfProbe, Display,
			       TEXT("RTS_PERF_PROBE summary view=%d cam=%.0f,%.0f,%.0f,%.1f,%.1f variant=%d gpu_median_of_medians=%.3f cmds=[%s]"),
			       ViewIndex, View.Location.X, View.Location.Y, View.Location.Z, View.Rotation.Pitch, View.Rotation.Yaw,
			       VariantIndex, Median(Medians), *FString::Join(M_Variants[VariantIndex], TEXT(" & ")));
		}
	}
}

void UPerfProbeSubsystem::TickMeshShots()
{
	using namespace PerfProbePrivate;
	const int32 MeshIndex = M_MeshShotIndex / MeshShotSlotsPerMesh;
	const int32 Slot = M_MeshShotIndex % MeshShotSlotsPerMesh;
	const int32 ViewIndex = Slot / MeshShotLodVariants;
	const int32 LodVariant = Slot % MeshShotLodVariants;
	if (not M_MeshPaths.IsValidIndex(MeshIndex))
	{
		if (M_MeshActor.IsValid())
		{
			M_MeshActor->Destroy();
		}
		Finish(true, FString::Printf(TEXT("%d meshes photographed"), M_MeshPaths.Num()));
		return;
	}
	if (M_FramesInPhase == 1)
	{
		if (Slot == 0 && not SpawnMeshForShot(MeshIndex))
		{
			M_MeshShotIndex += MeshShotSlotsPerMesh;
			EnterPhase(EPerfProbePhase::MeshShots);
			return;
		}
		const bool bCloseOnly = FParse::Param(FCommandLine::Get(), MeshCloseOnlySwitch);
		const bool bSkipsVariant = (LodVariant > 0 && M_MeshLodCount < 2) ||
			(LodVariant == AutoLodVariant && not bM_MeshIsSkeletal) || (bCloseOnly && ViewIndex > 0);
		if (bSkipsVariant)
		{
			++M_MeshShotIndex;
			EnterPhase(EPerfProbePhase::MeshShots);
			return;
		}
		PlaceCameraForMeshShot(ViewIndex, LodVariant);
		return;
	}
	int32 SettleFrames = DefaultMeshShotSettleFrames;
	FParse::Value(FCommandLine::Get(), MeshSettleSwitch, SettleFrames);
	if (M_FramesInPhase < SettleFrames)
	{
		return;
	}
	FString Name = TEXT("probe");
	FParse::Value(FCommandLine::Get(), NameSwitch, Name);
	const FString MeshName = FPaths::GetBaseFilename(M_MeshPaths[MeshIndex]);
	const TCHAR* const ViewName = ViewIndex == 0 ? TEXT("close") : TEXT("rts");
	// Single-LOD static meshes keep the plain name (before/after reduction comparisons); meshes with LODs are shot
	// forced to LOD0 and to LOD1 (static) or the last LOD (skeletal).
	const int32 ShotLod = LodVariant == 0 ? 0 : (bM_MeshIsSkeletal ? M_MeshLodCount - 1 : 1);
	FString FileName = M_MeshLodCount > 1
		                   ? FString::Printf(TEXT("%s_%s_%s_lod%d.png"), *MeshName, *Name, ViewName, ShotLod)
		                   : FString::Printf(TEXT("%s_%s_%s.png"), *MeshName, *Name, ViewName);
	if (LodVariant == AutoLodVariant)
	{
		FileName = FString::Printf(TEXT("%s_%s_%s_auto.png"), *MeshName, *Name, ViewName);
	}
	if (bM_MeshIsSkeletal && M_MeshActor.IsValid())
	{
		if (const USkeletalMeshComponent* const SkeletalComponent = M_MeshActor->FindComponentByClass<
			USkeletalMeshComponent>())
		{
			UE_LOG(LogRTSPerfProbe, Display, TEXT("RTS_PERF_PROBE lodcheck %s view=%s variant=%d predicted_lod=%d"),
			       *MeshName, ViewName, LodVariant, SkeletalComponent->GetPredictedLODLevel());
		}
	}
	const FString Path = FPaths::ProjectSavedDir() / TEXT("PerfAudit") / TEXT("MeshShots") / FileName;
	FScreenshotRequest::RequestScreenshot(Path, false, false);
	UE_LOG(LogRTSPerfProbe, Display, TEXT("RTS_PERF_PROBE meshshot %s"), *Path);
	++M_MeshShotIndex;
	EnterPhase(EPerfProbePhase::MeshShots);
}

bool UPerfProbeSubsystem::ParseTimelapse()
{
	FString TimelapseText;
	if (not FParse::Value(FCommandLine::Get(), PerfProbePrivate::TimelapseSwitch, TimelapseText, false))
	{
		return false;
	}
	FString ShotsText;
	FString SecondsText;
	if (not TimelapseText.Split(TEXT(","), &ShotsText, &SecondsText))
	{
		ShotsText = TimelapseText;
	}
	M_TimelapseShots = FMath::Max(FCString::Atoi(*ShotsText), 1);
	if (not SecondsText.IsEmpty())
	{
		M_TimelapseIntervalSeconds = FMath::Max(FCString::Atod(*SecondsText), 0.5);
	}
	return true;
}

void UPerfProbeSubsystem::TickTimelapse()
{
	if (M_TimelapseShotsTaken >= M_TimelapseShots)
	{
		Finish(true, FString::Printf(TEXT("%d time-lapse shots"), M_TimelapseShotsTaken));
		return;
	}
	const double NextShotSeconds = M_TimelapseIntervalSeconds * (M_TimelapseShotsTaken + 1);
	if (GetRealSecondsInPhase() < NextShotSeconds)
	{
		return;
	}
	FString Name = TEXT("probe");
	FParse::Value(FCommandLine::Get(), PerfProbePrivate::NameSwitch, Name);
	const FString Path = FPaths::ProjectSavedDir() / TEXT("PerfAudit") / TEXT("Shots") /
		FString::Printf(TEXT("%s_tl%03d.png"), *Name, M_TimelapseShotsTaken);
	FScreenshotRequest::RequestScreenshot(Path, false, false);
	const UWorld* const World = GetWorld();
	UE_LOG(LogRTSPerfProbe, Display, TEXT("RTS_PERF_PROBE timelapse %d game_t=%.1f %s"), M_TimelapseShotsTaken,
	       IsValid(World) ? World->GetTimeSeconds() : 0.0, *Path);
	++M_TimelapseShotsTaken;
}

bool UPerfProbeSubsystem::SpawnMeshForShot(const int32 MeshIndex)
{
	using namespace PerfProbePrivate;
	if (M_MeshActor.IsValid())
	{
		M_MeshActor->Destroy();
	}
	const FString& MeshPath = M_MeshPaths[MeshIndex];
	UStaticMesh* const StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
	USkeletalMesh* const SkeletalMesh = IsValid(StaticMesh)
		                                    ? nullptr
		                                    : LoadObject<USkeletalMesh>(nullptr, *MeshPath, nullptr,
		                                                                LOAD_Quiet | LOAD_NoWarn);
	if (not IsValid(StaticMesh) && not IsValid(SkeletalMesh))
	{
		UE_LOG(LogRTSPerfProbe, Warning, TEXT("RTS_PERF_PROBE could not load mesh %s"), *MeshPath);
		return false;
	}
	UWorld* const World = GetWorld();
	FHitResult Hit;
	const FVector TraceStart(M_MeshSpot.X, M_MeshSpot.Y, GroundTraceHalfHeight);
	const FVector TraceEnd(M_MeshSpot.X, M_MeshSpot.Y, -GroundTraceHalfHeight);
	const float GroundZ = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility)
		                      ? Hit.ImpactPoint.Z
		                      : 0.f;
	const FBoxSphereBounds Bounds = IsValid(StaticMesh) ? StaticMesh->GetBounds() : SkeletalMesh->GetBounds();
	const float BottomZ = Bounds.Origin.Z - Bounds.BoxExtent.Z;
	const FVector Location(M_MeshSpot.X - Bounds.Origin.X, M_MeshSpot.Y - Bounds.Origin.Y, GroundZ - BottomZ);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	bM_MeshIsSkeletal = IsValid(SkeletalMesh);
	if (bM_MeshIsSkeletal)
	{
		ASkeletalMeshActor* const MeshActor = World->SpawnActor<ASkeletalMeshActor>(
			ASkeletalMeshActor::StaticClass(), FTransform(FRotator::ZeroRotator, Location), SpawnParameters);
		if (not IsValid(MeshActor))
		{
			return false;
		}
		USkeletalMeshComponent* const MeshComponent = MeshActor->GetSkeletalMeshComponent();
		MeshComponent->SetSkeletalMesh(SkeletalMesh);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M_MeshLodCount = FMath::Max(1, SkeletalMesh->GetLODNum());
		M_MeshActor = MeshActor;
		UE_LOG(LogRTSPerfProbe, Display, TEXT("RTS_PERF_PROBE skeletal mesh %s lods=%d radius=%.0f ground=%.0f"),
		       *SkeletalMesh->GetName(), M_MeshLodCount, Bounds.SphereRadius, GroundZ);
		return true;
	}
	AStaticMeshActor* const MeshActor = World->SpawnActor<AStaticMeshActor>(
		AStaticMeshActor::StaticClass(), FTransform(FRotator::ZeroRotator, Location), SpawnParameters);
	if (not IsValid(MeshActor))
	{
		return false;
	}
	UStaticMeshComponent* const MeshComponent = MeshActor->GetStaticMeshComponent();
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetStaticMesh(StaticMesh);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_MeshLodCount = StaticMesh->GetNumLODs();
	M_MeshActor = MeshActor;
	UE_LOG(LogRTSPerfProbe, Display, TEXT("RTS_PERF_PROBE mesh %s tris=%d radius=%.0f ground=%.0f"),
	       *StaticMesh->GetName(), StaticMesh->GetNumTriangles(0), Bounds.SphereRadius, GroundZ);
	return true;
}

void UPerfProbeSubsystem::PlaceCameraForMeshShot(const int32 ViewIndex, const int32 LodVariant) const
{
	using namespace PerfProbePrivate;
	if (not M_Camera.IsValid() || not M_MeshActor.IsValid())
	{
		return;
	}
	// Forced LOD values are LOD + 1 (0 = automatic).
	if (bM_MeshIsSkeletal)
	{
		if (USkeletalMeshComponent* const SkeletalComponent = M_MeshActor->FindComponentByClass<
			USkeletalMeshComponent>())
		{
			const int32 ForcedLod = LodVariant == AutoLodVariant ? 0 : (LodVariant == 0 ? 1 : M_MeshLodCount);
			SkeletalComponent->SetForcedLOD(ForcedLod);
		}
	}
	else if (M_MeshLodCount > 1)
	{
		if (UStaticMeshComponent* const StaticComponent = M_MeshActor->FindComponentByClass<UStaticMeshComponent>())
		{
			StaticComponent->SetForcedLodModel(LodVariant == 0 ? 1 : 2);
		}
	}
	FVector Origin;
	FVector Extent;
	M_MeshActor->GetActorBounds(false, Origin, Extent);
	const float Radius = FMath::Max(static_cast<float>(Extent.Size()), 10.f);
	const bool bIsCloseUp = ViewIndex == 0;
	const FRotator Rotation(bIsCloseUp ? MeshCloseUpPitch : MeshRtsPitch, MeshCloseUpYaw, 0.f);
	const float Distance = bIsCloseUp ? Radius * MeshCloseUpDistanceInRadii : MeshRtsDistance;
	M_Camera->SetActorLocationAndRotation(Origin - Rotation.Vector() * Distance, Rotation);
	M_Camera->GetCameraComponent()->SetFieldOfView(bIsCloseUp ? MeshCloseUpFov : MeshRtsFov);
}

void UPerfProbeSubsystem::EnsureCamera()
{
	if (M_Camera.IsValid() || M_Views.Num() == 0)
	{
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(), FTransform(M_Views[0].Rotation, M_Views[0].Location), SpawnParameters);
	if (not IsValid(Camera))
	{
		return;
	}
	Camera->GetCameraComponent()->SetFieldOfView(M_FieldOfView);
	Camera->GetCameraComponent()->bConstrainAspectRatio = false;
	M_Camera = Camera;
}

void UPerfProbeSubsystem::KeepViewRendering()
{
	UWorld* World = GetWorld();
	if (UGameplayStatics::IsGamePaused(World))
	{
		UGameplayStatics::SetGamePaused(World, false);
	}
	if (UGameViewportClient* ViewportClient = World->GetGameViewport())
	{
		ViewportClient->bDisableWorldRendering = false;
	}
	if (M_PlayerController.IsValid())
	{
		FGrenadeTestHelpers::KeepGameRunning(*World, M_PlayerController.Get());
		if (APlayerCameraManager* CameraManager = M_PlayerController->PlayerCameraManager)
		{
			CameraManager->StopCameraFade();
		}
		if (M_Camera.IsValid() && M_PlayerController->GetViewTarget() != M_Camera.Get())
		{
			M_PlayerController->SetViewTarget(M_Camera.Get());
		}
	}
}

#else

bool UPerfProbeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UPerfProbeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UPerfProbeSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UPerfProbeSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
}

TStatId UPerfProbeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPerfProbeSubsystem, STATGROUP_Tickables);
}

#endif
