#include "CoverFinderWorldSubsystem.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderDeveloperSettings.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorker.h"
#include "RTS_Survival/Navigation/RTSNavAgentRegistery/RTSNavAgentRegistery.h"
#include "RTS_Survival/Navigation/RTSNavAgents/ERTSNavAgents.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "ShowFlags.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCoverFinder, Log, All);

DECLARE_STATS_GROUP(TEXT("RTS Cover Finder"), STATGROUP_RTSCoverFinder, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Game-thread sampling"), STAT_RTSCoverFinder_GameThreadSampling, STATGROUP_RTSCoverFinder);

namespace CoverFinderWorldSubsystemPrivate
{
	constexpr float FullCircleDegrees = 360.0f;
	constexpr float SurfaceDistanceToleranceRatio = 0.65f;
	constexpr float MinimumSurfaceDistanceTolerance = 40.0f;
	constexpr float MinimumStandingFaceAlignment = 0.8f;
	constexpr float CapsuleGroundClearance = 3.0f;
	constexpr float GapContinuityProbeStep = 10.0f;
	constexpr float DebugPointHeight = 45.0f;
	constexpr float DebugSphereRadius = 18.0f;
	constexpr float DebugNormalLength = 85.0f;
	constexpr float DebugArrowHeadSize = 18.0f;
	constexpr float DebugLineThickness = 4.0f;
	constexpr float CaptureMinimumHeight = 1800.0f;
	constexpr float CaptureHeightScale = 0.85f;
	constexpr float CaptureMaximumHeight = 50000.0f;
	constexpr int32 CaptureFrameDelay = 3;
	constexpr int32 MaximumQueriesForOneDirection = 105;
	const FColor CrouchCoverColor(173, 216, 230);
	const FColor StandingCoverColor(255, 165, 0);

	const FCollisionObjectQueryParams& GetAllCoverObjectQueryParams()
	{
		static const FCollisionObjectQueryParams ObjectQueryParams = []()
		{
			FCollisionObjectQueryParams NewObjectQueryParams;
			NewObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
			NewObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
			NewObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
			NewObjectQueryParams.AddObjectTypesToQuery(ECC_Vehicle);
			NewObjectQueryParams.AddObjectTypesToQuery(ECC_Destructible);
			NewObjectQueryParams.AddObjectTypesToQuery(COLLISION_OBJ_PLAYER);
			NewObjectQueryParams.AddObjectTypesToQuery(COLLISION_OBJ_ENEMY);
			return NewObjectQueryParams;
		}();
		return ObjectQueryParams;
	}

	bool GetIsLandscapeComponent(const UPrimitiveComponent* PrimitiveComponent)
	{
		return IsValid(PrimitiveComponent)
			&& PrimitiveComponent->GetCollisionResponseToChannel(COLLISION_TRACE_LANDSCAPE) == ECR_Block;
	}

	FCollisionQueryParams BuildCoverCollisionQueryParams();

	void AppendEnvironmentScanBounds(
		const UPrimitiveComponent* PrimitiveComponent,
		const float HorizontalExpansion,
		TArray<FBox>& OutEnvironmentBounds)
	{
		if (not IsValid(PrimitiveComponent) || GetIsLandscapeComponent(PrimitiveComponent))
		{
			return;
		}

		FBox EnvironmentBounds = PrimitiveComponent->Bounds.GetBox();
		if (EnvironmentBounds.IsValid == 0)
		{
			return;
		}
		EnvironmentBounds.Min.X -= HorizontalExpansion;
		EnvironmentBounds.Min.Y -= HorizontalExpansion;
		EnvironmentBounds.Max.X += HorizontalExpansion;
		EnvironmentBounds.Max.Y += HorizontalExpansion;
		EnvironmentBounds.Max.Z = EnvironmentBounds.Min.Z;
		OutEnvironmentBounds.Add(EnvironmentBounds);
	}

	bool TraceEnvironmentCover(
		UWorld& World,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		FHitResult& OutHitResult)
	{
		TArray<FHitResult> HitResults;
		if (not World.LineTraceMultiByObjectType(
			HitResults,
			TraceStart,
			TraceEnd,
			GetAllCoverObjectQueryParams(),
			BuildCoverCollisionQueryParams()))
		{
			return false;
		}

		HitResults.Sort([](const FHitResult& Left, const FHitResult& Right)
		{
			return Left.Distance < Right.Distance;
		});
		for (const FHitResult& HitResult : HitResults)
		{
			if (GetIsLandscapeComponent(HitResult.GetComponent()))
			{
				continue;
			}
			OutHitResult = HitResult;
			return true;
		}
		return false;
	}

	FCollisionQueryParams BuildCoverCollisionQueryParams()
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RTSCoverFinder), true);
		QueryParams.bFindInitialOverlaps = true;
		return QueryParams;
	}

	FCollisionQueryParams BuildCoverOverlapQueryParams()
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RTSCoverFinderBounds), false);
		QueryParams.bFindInitialOverlaps = true;
		return QueryParams;
	}

	FVector GetSearchDirection(const int32 DirectionIndex)
	{
		const float DirectionAngleDegrees = FullCircleDegrees
			* static_cast<float>(DirectionIndex)
			/ static_cast<float>(RTSCoverFinderConstants::SearchDirectionCount);
		const float DirectionAngleRadians = FMath::DegreesToRadians(DirectionAngleDegrees);
		return FVector(FMath::Cos(DirectionAngleRadians), FMath::Sin(DirectionAngleRadians), 0.0f);
	}

	FVector GetStandingSideDirection(const FVector& SearchDirection, const ERTSCoverType CoverType)
	{
		const FVector RightDirection = FVector::CrossProduct(FVector::UpVector, SearchDirection).GetSafeNormal();
		return CoverType == ERTSCoverType::StandingLeft ? -RightDirection : RightDirection;
	}

	FString BuildDefaultScreenshotName(const UWorld& World)
	{
		const FString SafeMapName = FPaths::MakeValidFileName(World.GetMapName());
		return FString::Printf(TEXT("CoverFinder_%s.png"), *SafeMapName);
	}

	URTSCoverFinderWorldSubsystem* GetCoverSubsystem(UWorld* World)
	{
		if (not IsValid(World))
		{
			return nullptr;
		}
		return World->GetSubsystem<URTSCoverFinderWorldSubsystem>();
	}

	void RunCoverReportCommand(const TArray<FString>&, UWorld* World)
	{
		URTSCoverFinderWorldSubsystem* CoverSubsystem = GetCoverSubsystem(World);
		if (not IsValid(CoverSubsystem))
		{
			UE_LOG(LogRTSCoverFinder, Error, TEXT("RTS.CoverFinder.Report failed: subsystem is unavailable."));
			return;
		}
		CoverSubsystem->LogPerformanceReport();
	}

	void RunCoverRescanCommand(const TArray<FString>&, UWorld* World)
	{
		URTSCoverFinderWorldSubsystem* CoverSubsystem = GetCoverSubsystem(World);
		if (not IsValid(CoverSubsystem))
		{
			UE_LOG(LogRTSCoverFinder, Error, TEXT("RTS.CoverFinder.Rescan failed: subsystem is unavailable."));
			return;
		}
		CoverSubsystem->ForceRescan();
	}

	void RunCoverCaptureCommand(const TArray<FString>& Arguments, UWorld* World)
	{
		URTSCoverFinderWorldSubsystem* CoverSubsystem = GetCoverSubsystem(World);
		if (not IsValid(CoverSubsystem))
		{
			UE_LOG(LogRTSCoverFinder, Error, TEXT("RTS.CoverFinder.Capture failed: subsystem is unavailable."));
			return;
		}
		const FString ScreenshotName = Arguments.IsEmpty() ? FString() : Arguments[0];
		CoverSubsystem->RequestDebugCapture(ScreenshotName);
	}

	FAutoConsoleCommandWithWorldAndArgs GCoverFinderReportCommand(
		TEXT("RTS.CoverFinder.Report"),
		TEXT("Logs the latest cover scan counts and measured game-thread/worker timings."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunCoverReportCommand));

	FAutoConsoleCommandWithWorldAndArgs GCoverFinderRescanCommand(
		TEXT("RTS.CoverFinder.Rescan"),
		TEXT("Requests another cover scan after the active generation completes."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunCoverRescanCommand));

	FAutoConsoleCommandWithWorldAndArgs GCoverFinderCaptureCommand(
		TEXT("RTS.CoverFinder.Capture"),
		TEXT("Rescans, frames the cover points from above, and saves a debug screenshot. Optional filename argument."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunCoverCaptureCommand));
}

void FCoverFinderSamplingState::Reset()
{
	SampleLocations.Reset();
	ObservationChunk.Reset();
	CurrentObservation = FCoverProbeObservation();
	NextSampleIndex = 0;
	CurrentDirectionIndex = 0;
	bHasCurrentObservation = false;
}

void FCoverFinderPerformanceAccumulator::Reset(const double StartSeconds)
{
	ScanStartSeconds = StartSeconds;
	TotalGameThreadSeconds = 0.0;
	MaximumGameThreadFrameSeconds = 0.0;
	SamplingFrameCount = 0;
	ProjectedSampleCount = 0;
	WorldQueryCount = 0;
	StandingSurfaceDirectionCount = 0;
	StandingOpeningCount = 0;
	ValidatedStandingGapCount = 0;
}

bool URTSCoverFinderWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld))
	{
		return false;
	}
	return OuterWorld->IsGameWorld();
}

void URTSCoverFinderWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		UE_LOG(LogRTSCoverFinder, Display, TEXT("RTS_COVER_INITIALIZED world=%s"), *GetNameSafe(GetWorld()));
	}
	M_Worker = MakeUnique<FCoverFinderWorker>();
	if (M_Worker->Start())
	{
		return;
	}

	RTSFunctionLibrary::ReportError(TEXT("Cover finder worker thread could not be created."));
	M_Worker.Reset();
}

void URTSCoverFinderWorldSubsystem::Deinitialize()
{
	RestoreCaptureLighting();
	if (M_Worker != nullptr)
	{
		M_Worker->StopAndWait();
		M_Worker.Reset();
	}
	M_SamplingState.Reset();
	M_CoverPoints.Reset();
	M_LandscapeCoverPoints.Reset();
	M_EnvironmentCoverPoints.Reset();
	M_AuthoredCoverProviders.Reset();
	M_CachedNavigationTileBounds.Reset();
	Super::Deinitialize();
}

void URTSCoverFinderWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		UE_LOG(
			LogRTSCoverFinder,
			Display,
			TEXT("RTS_COVER_WORLD_BEGIN_PLAY world=%s paused=%s"),
			*InWorld.GetName(),
			InWorld.IsPaused() ? TEXT("true") : TEXT("false"));
		if (FParse::Param(FCommandLine::Get(), TEXT("CoverFinderCapture")))
		{
			RequestDebugCapture();
		}
	}
}

void URTSCoverFinderWorldSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	TickDebugCapture();

	const URTSCoverFinderDeveloperSettings* CoverSettings = GetCoverFinderSettings();
	if (not IsValid(CoverSettings) || not CoverSettings->bM_EnableCoverSearch || M_Worker == nullptr)
	{
		return;
	}

	PollWorkerResults();
	if (M_ScanState == ECoverFinderScanState::SamplingWorld)
	{
		ProcessWorldSampling();
		return;
	}
	if (M_ScanState != ECoverFinderScanState::Idle)
	{
		return;
	}

	M_TimeUntilNextScan -= DeltaTime;
	if (M_TimeUntilNextScan <= 0.0f)
	{
		BeginScan();
	}
}

TStatId URTSCoverFinderWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URTSCoverFinderWorldSubsystem, STATGROUP_Tickables);
}

TArray<FRTSCoverPoint> URTSCoverFinderWorldSubsystem::FindCoverPointsInRadius(
	const FVector& SearchLocation,
	const float SearchRadius) const
{
	TArray<FRTSCoverPoint> NearbyCoverPoints;
	const float SearchRadiusSquared = FMath::Square(FMath::Max(0.0f, SearchRadius));
	for (const FRTSCoverPoint& CoverPoint : M_CoverPoints)
	{
		if (FVector::DistSquared(SearchLocation, CoverPoint.Location) <= SearchRadiusSquared)
		{
			NearbyCoverPoints.Add(CoverPoint);
		}
	}
	return NearbyCoverPoints;
}

void URTSCoverFinderWorldSubsystem::ForceRescan()
{
	if (M_ScanState == ECoverFinderScanState::Idle)
	{
		M_TimeUntilNextScan = 0.0f;
		return;
	}
	bM_ForceRescanAfterCurrent = true;
}

uint64 URTSCoverFinderWorldSubsystem::RegisterAuthoredCoverProvider(
	TArray<FRTSCoverPoint>&& CoverPoints)
{
	if (CoverPoints.IsEmpty())
	{
		return 0;
	}

	uint64 RegistrationId = M_NextProviderRegistrationId;
	while (RegistrationId == 0 || M_AuthoredCoverProviders.Contains(RegistrationId))
	{
		++RegistrationId;
	}
	M_NextProviderRegistrationId = RegistrationId + 1;
	for (FRTSCoverPoint& CoverPoint : CoverPoints)
	{
		CoverPoint.ProviderRegistrationId = RegistrationId;
		CoverPoint.CoverNormal = CoverPoint.CoverNormal.GetSafeNormal2D();
		if (CoverPoint.CoverNormal.IsNearlyZero())
		{
			CoverPoint.CoverNormal = FVector::ForwardVector;
		}
	}

	M_AuthoredCoverProviders.Add(RegistrationId, MoveTemp(CoverPoints));
	RebuildPublishedCoverPoints();
	DrawPublishedCover();
	return RegistrationId;
}

void URTSCoverFinderWorldSubsystem::UnregisterAuthoredCoverProvider(const uint64 RegistrationId)
{
	if (RegistrationId == 0)
	{
		return;
	}

	TArray<FRTSCoverPoint> RemovedProviderPoints;
	if (not M_AuthoredCoverProviders.RemoveAndCopyValue(RegistrationId, RemovedProviderPoints))
	{
		return;
	}

	RemoveGeneratedDuplicates(RemovedProviderPoints);
	RebuildPublishedCoverPoints();
	DrawPublishedCover();
	ForceRescan();
}

void URTSCoverFinderWorldSubsystem::LogPerformanceReport() const
{
	int32 CrouchCoverCount = 0;
	int32 StandingLeftCoverCount = 0;
	int32 StandingRightCoverCount = 0;
	int32 AuthoredCoverCount = 0;
	for (const FRTSCoverPoint& CoverPoint : M_CoverPoints)
	{
		if (CoverPoint.ProviderRegistrationId != 0)
		{
			++AuthoredCoverCount;
		}
		switch (CoverPoint.CoverType)
		{
		case ERTSCoverType::Crouch:
			++CrouchCoverCount;
			break;
		case ERTSCoverType::StandingLeft:
			++StandingLeftCoverCount;
			break;
		case ERTSCoverType::StandingRight:
			++StandingRightCoverCount;
			break;
		default:
			break;
		}
	}
	UE_LOG(
		LogRTSCoverFinder,
		Display,
		TEXT("RTS_COVER_PERF domain=%s samples=%d projected=%d queries=%d raw=%d points=%d authored=%d landscape_cached=%d environment_cached=%d crouch=%d standing_left=%d standing_right=%d high_directions=%d openings=%d valid_gaps=%d gt_total_ms=%.3f gt_avg_frame_ms=%.3f gt_max_frame_ms=%.3f worker_ms=%.3f wall_ms=%.3f"),
		M_ActiveScanDomain == ECoverFinderScanDomain::Landscape ? TEXT("landscape") : TEXT("environment"),
		M_LastPerformanceSnapshot.PlannedSampleCount,
		M_LastPerformanceSnapshot.ProjectedSampleCount,
		M_LastPerformanceSnapshot.WorldQueryCount,
		M_LastPerformanceSnapshot.RawCandidateCount,
		M_LastPerformanceSnapshot.CoverPointCount,
		AuthoredCoverCount,
		M_LandscapeCoverPoints.Num(),
		M_EnvironmentCoverPoints.Num(),
		CrouchCoverCount,
		StandingLeftCoverCount,
		StandingRightCoverCount,
		M_LastPerformanceSnapshot.StandingSurfaceDirectionCount,
		M_LastPerformanceSnapshot.StandingOpeningCount,
		M_LastPerformanceSnapshot.ValidatedStandingGapCount,
		M_LastPerformanceSnapshot.TotalGameThreadMilliseconds,
		M_LastPerformanceSnapshot.AverageGameThreadMillisecondsPerSamplingFrame,
		M_LastPerformanceSnapshot.MaximumGameThreadMillisecondsInOneSamplingFrame,
		M_LastPerformanceSnapshot.WorkerMilliseconds,
		M_LastPerformanceSnapshot.WallClockMilliseconds);
}

void URTSCoverFinderWorldSubsystem::RequestDebugCapture(const FString& ScreenshotName)
{
	if constexpr (not DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const FString RequestedName = ScreenshotName.IsEmpty()
		? CoverFinderWorldSubsystemPrivate::BuildDefaultScreenshotName(*World)
		: FPaths::MakeValidFileName(ScreenshotName);
	const FString ScreenshotDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CoverFinderDebug"));
	IFileManager::Get().MakeDirectory(*ScreenshotDirectory, true);
	M_PendingScreenshotPath = FPaths::Combine(ScreenshotDirectory, RequestedName);
	bM_CaptureAfterScan = true;
	ForceRescan();
}

const URTSCoverFinderDeveloperSettings* URTSCoverFinderWorldSubsystem::GetCoverFinderSettings() const
{
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	if (IsValid(CoverSettings))
	{
		return CoverSettings;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("URTSCoverFinderDeveloperSettings default object"),
		TEXT("GetCoverFinderSettings"),
		this);
	return nullptr;
}

const ANavigationData* URTSCoverFinderWorldSubsystem::GetCharacterNavigationData() const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}

	URTSNavAgentRegistry* NavAgentRegistry = URTSNavAgentRegistry::Get(World);
	if (not IsValid(NavAgentRegistry))
	{
		return nullptr;
	}

	const FNavDataConfig* CharacterConfig = NavAgentRegistry->FindAgentConfig(World, ERTSNavAgents::Character);
	if (CharacterConfig == nullptr)
	{
		return nullptr;
	}
	return NavAgentRegistry->GetNavDataForAgent(World, *CharacterConfig);
}

bool URTSCoverFinderWorldSubsystem::GatherNavigationScanInputs(
	TArray<FBox>& OutTileBounds,
	float& OutAgentRadius,
	float& OutAgentHeight)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}

	URTSNavAgentRegistry* NavAgentRegistry = URTSNavAgentRegistry::Get(World);
	if (not IsValid(NavAgentRegistry))
	{
		return false;
	}

	const FNavDataConfig* CharacterConfig = NavAgentRegistry->FindAgentConfig(World, ERTSNavAgents::Character);
	if (CharacterConfig == nullptr)
	{
		return false;
	}
	OutAgentRadius = CharacterConfig->AgentRadius;
	OutAgentHeight = CharacterConfig->AgentHeight;

	const ANavigationData* NavigationData = NavAgentRegistry->GetNavDataForAgent(World, *CharacterConfig);
	const ARecastNavMesh* RecastNavMesh = Cast<ARecastNavMesh>(NavigationData);
	if (not IsValid(RecastNavMesh))
	{
		return false;
	}

	TArray<FNavTileRef> NavigationTileReferences;
	RecastNavMesh->GetAllNavMeshTiles(NavigationTileReferences);
	M_LastScanBounds = FBox(ForceInit);
	for (const FNavTileRef NavigationTileReference : NavigationTileReferences)
	{
		const FBox NavigationTileBounds = RecastNavMesh->GetNavMeshTileBounds(NavigationTileReference);
		if (NavigationTileBounds.IsValid == 0)
		{
			continue;
		}
		OutTileBounds.Add(NavigationTileBounds);
		M_LastScanBounds += NavigationTileBounds;
	}
	return not OutTileBounds.IsEmpty();
}

bool URTSCoverFinderWorldSubsystem::GatherEnvironmentScanBounds(
	TArray<FBox>& OutEnvironmentBounds)
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || M_LastScanBounds.IsValid == 0)
	{
		return false;
	}

	TArray<FOverlapResult> EnvironmentOverlaps;
	World->OverlapMultiByObjectType(
		EnvironmentOverlaps,
		M_LastScanBounds.GetCenter(),
		FQuat::Identity,
		CoverFinderWorldSubsystemPrivate::GetAllCoverObjectQueryParams(),
		FCollisionShape::MakeBox(M_LastScanBounds.GetExtent()),
		CoverFinderWorldSubsystemPrivate::BuildCoverOverlapQueryParams());

	const float HorizontalExpansion = M_ActiveSettings.MaximumCoverSearchDistance
		+ M_ActiveSettings.AgentRadius;
	for (const FOverlapResult& EnvironmentOverlap : EnvironmentOverlaps)
	{
		CoverFinderWorldSubsystemPrivate::AppendEnvironmentScanBounds(
			EnvironmentOverlap.GetComponent(),
			HorizontalExpansion,
			OutEnvironmentBounds);
	}
	return not OutEnvironmentBounds.IsEmpty();
}

FCoverFinderSettingsSnapshot URTSCoverFinderWorldSubsystem::BuildSettingsSnapshot(
	const float AgentRadius,
	const float AgentHeight) const
{
	FCoverFinderSettingsSnapshot Snapshot;
	const URTSCoverFinderDeveloperSettings* CoverSettings = GetCoverFinderSettings();
	if (not IsValid(CoverSettings))
	{
		return Snapshot;
	}

	Snapshot.SearchGridSpacing = FMath::Clamp(CoverSettings->M_SearchGridSpacing, 60.0f, 300.0f);
	Snapshot.MinimumCrouchCoverHeight = FMath::Clamp(
		CoverSettings->M_MinimumCrouchCoverHeight,
		RTSCoverFinderConstants::LowerSupportProbeHeight + 1.0f,
		RTSCoverFinderConstants::StandingCoverHeight - 1.0f);
	Snapshot.MaximumCoverSearchDistance = FMath::Clamp(
		CoverSettings->M_MaximumCoverSearchDistance,
		60.0f,
		300.0f);
	Snapshot.StandingPeekGapWidth = FMath::Clamp(CoverSettings->M_StandingPeekGapWidth, 80.0f, 250.0f);
	Snapshot.CoverPointSpacing = FMath::Clamp(CoverSettings->M_CoverPointSpacing, 50.0f, 250.0f);
	Snapshot.GameThreadBudgetMilliseconds = FMath::Max(0.05f, CoverSettings->M_GameThreadBudgetMilliseconds);
	Snapshot.AgentRadius = FMath::Max(1.0f, AgentRadius);
	Snapshot.AgentHeight = FMath::Max(RTSCoverFinderConstants::InfantryHeight, AgentHeight);
	Snapshot.SurfaceDistanceTolerance = FMath::Max(
		CoverFinderWorldSubsystemPrivate::MinimumSurfaceDistanceTolerance,
		Snapshot.MaximumCoverSearchDistance * CoverFinderWorldSubsystemPrivate::SurfaceDistanceToleranceRatio);
	return Snapshot;
}

FCoverFinderSettingsSnapshot URTSCoverFinderWorldSubsystem::BuildPublicationSettings() const
{
	FCoverFinderSettingsSnapshot PublicationSettings = M_ActiveSettings;
	const URTSCoverFinderDeveloperSettings* CoverSettings = GetCoverFinderSettings();
	if (not IsValid(CoverSettings))
	{
		return PublicationSettings;
	}
	PublicationSettings.CoverPointSpacing = FMath::Clamp(
		CoverSettings->M_CoverPointSpacing,
		50.0f,
		250.0f);
	return PublicationSettings;
}

void URTSCoverFinderWorldSubsystem::BeginScan()
{
	const double ScanStartSeconds = FPlatformTime::Seconds();
	TArray<FBox> ScanBounds;
	if (not PrepareScanInputs(ScanBounds))
	{
		M_TimeUntilNextScan = 1.0f;
		UE_LOG(LogRTSCoverFinder, Warning, TEXT("Cover scan postponed: Character Recast navigation has no populated tiles."));
		return;
	}

	++M_ActiveGeneration;
	if (M_ActiveGeneration == 0)
	{
		++M_ActiveGeneration;
	}
	M_SamplingState.Reset();
	M_PerformanceAccumulator.Reset(ScanStartSeconds);
	M_PerformanceAccumulator.TotalGameThreadSeconds = FPlatformTime::Seconds() - ScanStartSeconds;
	M_LastPerformanceSnapshot = FRTSCoverFinderPerformance();
	M_ScanState = ECoverFinderScanState::WaitingForPlan;
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		UE_LOG(
			LogRTSCoverFinder,
			Display,
			TEXT("RTS_COVER_SCAN_STARTED generation=%llu domain=%s bounds=%d grid_cm=%.1f budget_ms=%.3f"),
			M_ActiveGeneration,
			M_ActiveScanDomain == ECoverFinderScanDomain::Landscape ? TEXT("landscape") : TEXT("environment"),
			ScanBounds.Num(),
			M_ActiveSettings.SearchGridSpacing,
			M_ActiveSettings.GameThreadBudgetMilliseconds);
	}
	M_Worker->EnqueuePlanRequest(M_ActiveGeneration, MoveTemp(ScanBounds), M_ActiveSettings);
}

bool URTSCoverFinderWorldSubsystem::PrepareScanInputs(TArray<FBox>& OutScanBounds)
{
	if (M_CachedNavigationTileBounds.IsEmpty())
	{
		if (not GatherNavigationScanInputs(
			M_CachedNavigationTileBounds,
			M_CachedAgentRadius,
			M_CachedAgentHeight))
		{
			return false;
		}
	}

	M_ActiveSettings = BuildSettingsSnapshot(M_CachedAgentRadius, M_CachedAgentHeight);
	if (not bM_LandscapeScanComplete)
	{
		M_ActiveScanDomain = ECoverFinderScanDomain::Landscape;
		OutScanBounds = M_CachedNavigationTileBounds;
		return true;
	}

	M_ActiveScanDomain = ECoverFinderScanDomain::Environment;
	GatherEnvironmentScanBounds(OutScanBounds);
	return true;
}

void URTSCoverFinderWorldSubsystem::PollWorkerResults()
{
	FCoverSamplePlanResult PlanResult;
	while (M_Worker->DequeueSamplePlan(PlanResult))
	{
		AcceptSamplePlan(MoveTemp(PlanResult));
	}

	FCoverGenerationResult GenerationResult;
	while (M_Worker->DequeueGenerationResult(GenerationResult))
	{
		AcceptGenerationResult(MoveTemp(GenerationResult));
	}
}

void URTSCoverFinderWorldSubsystem::AcceptSamplePlan(FCoverSamplePlanResult&& PlanResult)
{
	if (PlanResult.Generation != M_ActiveGeneration || M_ScanState != ECoverFinderScanState::WaitingForPlan)
	{
		return;
	}

	M_SamplingState.SampleLocations = MoveTemp(PlanResult.SampleLocations);
	M_LastPerformanceSnapshot.PlannedSampleCount = M_SamplingState.SampleLocations.Num();
	M_ScanState = ECoverFinderScanState::SamplingWorld;
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		UE_LOG(
			LogRTSCoverFinder,
			Display,
			TEXT("RTS_COVER_PLAN_READY generation=%llu samples=%d worker_ms=%.3f"),
			PlanResult.Generation,
			M_SamplingState.SampleLocations.Num(),
			PlanResult.WorkerMilliseconds);
	}
	if (PlanResult.bWasTruncated)
	{
		UE_LOG(
			LogRTSCoverFinder,
			Warning,
			TEXT("Cover sample plan reached its %d-location safety limit."),
			RTSCoverFinderConstants::MaxSampleLocations);
	}
}

void URTSCoverFinderWorldSubsystem::AcceptGenerationResult(FCoverGenerationResult&& GenerationResult)
{
	if (GenerationResult.Generation != M_ActiveGeneration
		|| M_ScanState != ECoverFinderScanState::WaitingForWorker)
	{
		return;
	}

	PublishPerformanceSnapshot(GenerationResult);
	M_ScanState = ECoverFinderScanState::Idle;
	if (M_ActiveScanDomain == ECoverFinderScanDomain::Landscape)
	{
		M_LandscapeCoverPoints = MoveTemp(GenerationResult.CoverPoints);
		bM_LandscapeScanComplete = true;
		RebuildPublishedCoverPoints();
		M_LastPerformanceSnapshot.CoverPointCount = M_CoverPoints.Num();
		M_TimeUntilNextScan = 0.0f;
		DrawPublishedCover();
		LogPerformanceReport();
		return;
	}

	M_EnvironmentCoverPoints = MoveTemp(GenerationResult.CoverPoints);
	RebuildPublishedCoverPoints();
	M_LastPerformanceSnapshot.CoverPointCount = M_CoverPoints.Num();
	const URTSCoverFinderDeveloperSettings* CoverSettings = GetCoverFinderSettings();
	M_TimeUntilNextScan = bM_ForceRescanAfterCurrent || not IsValid(CoverSettings)
		? 0.0f
		: FMath::Max(1.0f, CoverSettings->M_RescanIntervalSeconds);
	bM_ForceRescanAfterCurrent = false;
	DrawPublishedCover();
	LogPerformanceReport();
	if (bM_CaptureAfterScan)
	{
		PrepareDebugCapture();
	}
}

void URTSCoverFinderWorldSubsystem::ProcessWorldSampling()
{
	SCOPE_CYCLE_COUNTER(STAT_RTSCoverFinder_GameThreadSampling);
	const double FrameStartSeconds = FPlatformTime::Seconds();
	const UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	const ANavigationData* NavigationData = GetCharacterNavigationData();
	if (not IsValid(NavigationSystem) || not IsValid(NavigationData))
	{
		M_ScanState = ECoverFinderScanState::Idle;
		M_TimeUntilNextScan = 1.0f;
		return;
	}

	int32 FrameWorldQueries = 0;
	bool bPerformedQuery = false;
	while (M_SamplingState.NextSampleIndex < M_SamplingState.SampleLocations.Num()
		|| M_SamplingState.bHasCurrentObservation)
	{
		if (FrameWorldQueries >= RTSCoverFinderConstants::MaxWorldQueriesPerFrame)
		{
			break;
		}

		const double ElapsedMilliseconds = (FPlatformTime::Seconds() - FrameStartSeconds) * 1000.0;
		if (bPerformedQuery && ElapsedMilliseconds >= M_ActiveSettings.GameThreadBudgetMilliseconds)
		{
			break;
		}

		if (not M_SamplingState.bHasCurrentObservation)
		{
			++FrameWorldQueries;
			++M_PerformanceAccumulator.WorldQueryCount;
			bPerformedQuery = true;
			StartNextObservation(*NavigationSystem, *NavigationData, FrameWorldQueries);
			continue;
		}

		if (M_SamplingState.CurrentDirectionIndex >= RTSCoverFinderConstants::SearchDirectionCount)
		{
			CompleteCurrentObservation();
			continue;
		}

		if (FrameWorldQueries + CoverFinderWorldSubsystemPrivate::MaximumQueriesForOneDirection
			> RTSCoverFinderConstants::MaxWorldQueriesPerFrame)
		{
			break;
		}
		SampleCurrentDirection(*NavigationSystem, *NavigationData, FrameWorldQueries);
		bPerformedQuery = true;
	}

	RecordSamplingFrame(FrameStartSeconds);
	const bool bFinished = M_SamplingState.NextSampleIndex >= M_SamplingState.SampleLocations.Num()
		&& not M_SamplingState.bHasCurrentObservation;
	if (bFinished)
	{
		FinishWorldSampling();
	}
}

bool URTSCoverFinderWorldSubsystem::StartNextObservation(
	const UNavigationSystemV1& NavigationSystem,
	const ANavigationData& NavigationData,
	int32& InOutFrameWorldQueries)
{
	if (not M_SamplingState.SampleLocations.IsValidIndex(M_SamplingState.NextSampleIndex))
	{
		return false;
	}

	const FVector& PlannedLocation = M_SamplingState.SampleLocations[M_SamplingState.NextSampleIndex];
	const FVector ProjectionExtent(
		M_ActiveSettings.SearchGridSpacing * 0.45f,
		M_ActiveSettings.SearchGridSpacing * 0.45f,
		M_ActiveSettings.AgentHeight);
	FNavLocation ProjectedLocation;
	if (not NavigationSystem.ProjectPointToNavigation(
		PlannedLocation,
		ProjectedLocation,
		ProjectionExtent,
		&NavigationData))
	{
		++M_SamplingState.NextSampleIndex;
		return false;
	}
	if (not GetCanInfantryOccupyLocation(ProjectedLocation.Location, InOutFrameWorldQueries))
	{
		++M_SamplingState.NextSampleIndex;
		return false;
	}

	M_SamplingState.CurrentObservation = FCoverProbeObservation();
	M_SamplingState.CurrentObservation.ProjectedLocation = ProjectedLocation.Location;
	M_SamplingState.CurrentObservation.DirectionalObservations.Reserve(
		RTSCoverFinderConstants::SearchDirectionCount);
	M_SamplingState.CurrentDirectionIndex = 0;
	M_SamplingState.bHasCurrentObservation = true;
	++M_PerformanceAccumulator.ProjectedSampleCount;
	return true;
}

void URTSCoverFinderWorldSubsystem::SampleCurrentDirection(
	const UNavigationSystemV1& NavigationSystem,
	const ANavigationData& NavigationData,
	int32& InOutFrameWorldQueries)
{
	FCoverDirectionalObservation DirectionObservation;
	DirectionObservation.SearchDirection = CoverFinderWorldSubsystemPrivate::GetSearchDirection(
		M_SamplingState.CurrentDirectionIndex);
	const FVector& ProtectedLocation = M_SamplingState.CurrentObservation.ProjectedLocation;
	DirectionObservation.CrouchTrace = TraceCoverHeight(
		ProtectedLocation,
		DirectionObservation.SearchDirection,
		M_ActiveSettings.MinimumCrouchCoverHeight,
		InOutFrameWorldQueries);
	if (not DirectionObservation.CrouchTrace.bBlockingHit)
	{
		M_SamplingState.CurrentObservation.DirectionalObservations.Add(MoveTemp(DirectionObservation));
		++M_SamplingState.CurrentDirectionIndex;
		return;
	}

	DirectionObservation.LowerTrace = TraceCoverHeight(
		ProtectedLocation,
		DirectionObservation.SearchDirection,
		RTSCoverFinderConstants::LowerSupportProbeHeight,
		InOutFrameWorldQueries);
	DirectionObservation.StandingTrace = TraceCoverHeight(
		ProtectedLocation,
		DirectionObservation.SearchDirection,
		RTSCoverFinderConstants::StandingCoverHeight,
		InOutFrameWorldQueries);

	const bool bCrouchSurface = FCoverFinderAlgorithms::GetIsSameSurface(
		DirectionObservation.LowerTrace,
		DirectionObservation.CrouchTrace,
		M_ActiveSettings);
	const bool bStandingSurface = FCoverFinderAlgorithms::GetIsSameSurface(
		DirectionObservation.CrouchTrace,
		DirectionObservation.StandingTrace,
		M_ActiveSettings);
	if (bCrouchSurface && bStandingSurface)
	{
		SampleStandingSides(
			NavigationSystem,
			NavigationData,
			ProtectedLocation,
			DirectionObservation,
			InOutFrameWorldQueries);
	}

	M_SamplingState.CurrentObservation.DirectionalObservations.Add(MoveTemp(DirectionObservation));
	++M_SamplingState.CurrentDirectionIndex;
}

void URTSCoverFinderWorldSubsystem::SampleStandingSides(
	const UNavigationSystemV1& NavigationSystem,
	const ANavigationData& NavigationData,
	const FVector& ProtectedLocation,
	FCoverDirectionalObservation& DirectionObservation,
	int32& InOutFrameWorldQueries)
{
	const FVector CoverNormal = FCoverFinderAlgorithms::BuildCoverNormal(
		DirectionObservation.StandingTrace,
		DirectionObservation.SearchDirection);
	const float FaceAlignment = FVector::DotProduct(
		CoverNormal,
		-DirectionObservation.SearchDirection.GetSafeNormal2D());
	if (FaceAlignment < CoverFinderWorldSubsystemPrivate::MinimumStandingFaceAlignment)
	{
		return;
	}

	++M_PerformanceAccumulator.StandingSurfaceDirectionCount;
	const FVector CoverFacingDirection = -CoverNormal;
	const FVector LeftDirection = CoverFinderWorldSubsystemPrivate::GetStandingSideDirection(
		CoverFacingDirection,
		ERTSCoverType::StandingLeft);
	const FVector RightDirection = -LeftDirection;
	DirectionObservation.bLeftGapOpen = SampleStandingGap(
		NavigationSystem,
		NavigationData,
		ProtectedLocation,
		CoverFacingDirection,
		LeftDirection,
		InOutFrameWorldQueries,
		DirectionObservation.LeftCoverLocation);
	DirectionObservation.bRightGapOpen = SampleStandingGap(
		NavigationSystem,
		NavigationData,
		ProtectedLocation,
		CoverFacingDirection,
		RightDirection,
		InOutFrameWorldQueries,
		DirectionObservation.RightCoverLocation);
}

void URTSCoverFinderWorldSubsystem::CompleteCurrentObservation()
{
	M_SamplingState.ObservationChunk.Add(MoveTemp(M_SamplingState.CurrentObservation));
	M_SamplingState.CurrentObservation = FCoverProbeObservation();
	M_SamplingState.bHasCurrentObservation = false;
	M_SamplingState.CurrentDirectionIndex = 0;
	++M_SamplingState.NextSampleIndex;
	if (M_SamplingState.ObservationChunk.Num() >= RTSCoverFinderConstants::ObservationChunkSize)
	{
		FlushObservationChunk();
	}
}

void URTSCoverFinderWorldSubsystem::FlushObservationChunk()
{
	if (M_SamplingState.ObservationChunk.IsEmpty())
	{
		return;
	}
	TArray<FCoverProbeObservation> CompletedChunk = MoveTemp(M_SamplingState.ObservationChunk);
	M_SamplingState.ObservationChunk.Reset();
	M_Worker->EnqueueObservationChunk(M_ActiveGeneration, MoveTemp(CompletedChunk));
}

void URTSCoverFinderWorldSubsystem::FinishWorldSampling()
{
	FlushObservationChunk();
	M_Worker->EnqueueFinalizeRequest(M_ActiveGeneration);
	M_ScanState = ECoverFinderScanState::WaitingForWorker;
	M_SamplingState.SampleLocations.Reset();
}

bool URTSCoverFinderWorldSubsystem::SampleStandingGap(
	const UNavigationSystemV1& NavigationSystem,
	const ANavigationData& NavigationData,
	const FVector& ProtectedLocation,
	const FVector& SearchDirection,
	const FVector& SideDirection,
	int32& InOutFrameWorldQueries,
	FVector& OutCoverLocation)
{
	float LastCoveredOffset = 0.0f;
	float FirstOpenOffset = 0.0f;
	if (not FindStandingOpening(
		ProtectedLocation,
		SearchDirection,
		SideDirection,
		InOutFrameWorldQueries,
		LastCoveredOffset,
		FirstOpenOffset))
	{
		return false;
	}
	++M_PerformanceAccumulator.StandingOpeningCount;
	if (not GetIsStandingGapIntervalOpen(
		NavigationSystem,
		NavigationData,
		ProtectedLocation,
		SearchDirection,
		SideDirection,
		FirstOpenOffset,
		InOutFrameWorldQueries))
	{
		return false;
	}
	++M_PerformanceAccumulator.ValidatedStandingGapCount;

	const FVector RequestedCoverLocation = ProtectedLocation + SideDirection * LastCoveredOffset;
	const FVector ProjectionExtent(
		M_ActiveSettings.AgentRadius,
		M_ActiveSettings.AgentRadius,
		M_ActiveSettings.AgentHeight * 0.5f);
	FNavLocation ProjectedCoverLocation;
	++InOutFrameWorldQueries;
	++M_PerformanceAccumulator.WorldQueryCount;
	if (not NavigationSystem.ProjectPointToNavigation(
		RequestedCoverLocation,
		ProjectedCoverLocation,
		ProjectionExtent,
		&NavigationData))
	{
		return false;
	}
	OutCoverLocation = ProjectedCoverLocation.Location;
	return true;
}

bool URTSCoverFinderWorldSubsystem::FindStandingOpening(
	const FVector& ProtectedLocation,
	const FVector& SearchDirection,
	const FVector& SideDirection,
	int32& InOutFrameWorldQueries,
	float& OutLastCoveredOffset,
	float& OutFirstOpenOffset)
{
	const float SearchStep = FMath::Max(25.0f, M_ActiveSettings.AgentRadius);
	const float MaximumEndSearchDistance = M_ActiveSettings.SearchGridSpacing * 1.25f;
	OutLastCoveredOffset = 0.0f;
	for (float SideOffset = SearchStep;
		SideOffset <= MaximumEndSearchDistance;
		SideOffset += SearchStep)
	{
		const FVector SampleLocation = ProtectedLocation + SideDirection * SideOffset;
		const FCoverTraceObservation StandingTrace = TraceCoverHeight(
			SampleLocation,
			SearchDirection,
			RTSCoverFinderConstants::StandingCoverHeight,
			InOutFrameWorldQueries);
		if (StandingTrace.bBlockingHit)
		{
			OutLastCoveredOffset = SideOffset;
			continue;
		}

		OutFirstOpenOffset = SideOffset;
		return true;
	}
	return false;
}

bool URTSCoverFinderWorldSubsystem::GetIsStandingGapIntervalOpen(
	const UNavigationSystemV1& NavigationSystem,
	const ANavigationData& NavigationData,
	const FVector& ProtectedLocation,
	const FVector& SearchDirection,
	const FVector& SideDirection,
	const float FirstOpenOffset,
	int32& InOutFrameWorldQueries)
{
	if (not GetIsStandingGapTraceIntervalClear(
		ProtectedLocation,
		SearchDirection,
		SideDirection,
		FirstOpenOffset,
		InOutFrameWorldQueries))
	{
		return false;
	}

	const float CapsuleRadius = M_ActiveSettings.AgentRadius;
	const float FirstCapsuleCenterOffset = FirstOpenOffset + CapsuleRadius;
	const float LastCapsuleCenterOffset = FirstOpenOffset
		+ M_ActiveSettings.StandingPeekGapWidth
		- CapsuleRadius;
	if (LastCapsuleCenterOffset < FirstCapsuleCenterOffset)
	{
		return false;
	}

	const float ValidationStep = FMath::Max(25.0f, M_ActiveSettings.AgentRadius);
	for (float CapsuleCenterOffset = FirstCapsuleCenterOffset;
		CapsuleCenterOffset < LastCapsuleCenterOffset;
		CapsuleCenterOffset += ValidationStep)
	{
		const FVector RequestedLocation = ProtectedLocation
			+ SideDirection * CapsuleCenterOffset;
		if (not GetIsStandingGapPositionOpen(
			NavigationSystem,
			NavigationData,
			RequestedLocation,
			SearchDirection,
			InOutFrameWorldQueries))
		{
			return false;
		}
	}

	const FVector EndLocation = ProtectedLocation
		+ SideDirection * LastCapsuleCenterOffset;
	return GetIsStandingGapPositionOpen(
		NavigationSystem,
		NavigationData,
		EndLocation,
		SearchDirection,
		InOutFrameWorldQueries);
}

bool URTSCoverFinderWorldSubsystem::GetIsStandingGapTraceIntervalClear(
	const FVector& ProtectedLocation,
	const FVector& SearchDirection,
	const FVector& SideDirection,
	const float FirstOpenOffset,
	int32& InOutFrameWorldQueries)
{
	const float LastOpenOffset = FirstOpenOffset + M_ActiveSettings.StandingPeekGapWidth;
	for (float SideOffset = FirstOpenOffset;
		SideOffset < LastOpenOffset;
		SideOffset += CoverFinderWorldSubsystemPrivate::GapContinuityProbeStep)
	{
		const FVector SampleLocation = ProtectedLocation + SideDirection * SideOffset;
		const FCoverTraceObservation GapTrace = TraceCoverHeight(
			SampleLocation,
			SearchDirection,
			M_ActiveSettings.MinimumCrouchCoverHeight,
			InOutFrameWorldQueries,
			ECoverFinderTraceDomain::AllCoverProviders);
		if (GapTrace.bBlockingHit)
		{
			return false;
		}
	}

	const FVector LastSampleLocation = ProtectedLocation + SideDirection * LastOpenOffset;
	const FCoverTraceObservation LastGapTrace = TraceCoverHeight(
		LastSampleLocation,
		SearchDirection,
		M_ActiveSettings.MinimumCrouchCoverHeight,
		InOutFrameWorldQueries,
		ECoverFinderTraceDomain::AllCoverProviders);
	return not LastGapTrace.bBlockingHit;
}

bool URTSCoverFinderWorldSubsystem::GetIsStandingGapPositionOpen(
	const UNavigationSystemV1& NavigationSystem,
	const ANavigationData& NavigationData,
	const FVector& RequestedLocation,
	const FVector& SearchDirection,
	int32& InOutFrameWorldQueries)
{
	const FVector ProjectionExtent(
		M_ActiveSettings.AgentRadius,
		M_ActiveSettings.AgentRadius,
		M_ActiveSettings.AgentHeight * 0.5f);
	FNavLocation ProjectedGapLocation;
	++InOutFrameWorldQueries;
	++M_PerformanceAccumulator.WorldQueryCount;
	if (not NavigationSystem.ProjectPointToNavigation(
		RequestedLocation,
		ProjectedGapLocation,
		ProjectionExtent,
		&NavigationData))
	{
		return false;
	}

	const float MaximumProjectionOffset = M_ActiveSettings.AgentRadius;
	if (FVector::DistSquared2D(RequestedLocation, ProjectedGapLocation.Location)
		> FMath::Square(MaximumProjectionOffset))
	{
		return false;
	}

	if (not GetCanInfantryOccupyLocation(ProjectedGapLocation.Location, InOutFrameWorldQueries))
	{
		return false;
	}

	const FCoverTraceObservation CrouchTrace = TraceCoverHeight(
		ProjectedGapLocation.Location,
		SearchDirection,
		M_ActiveSettings.MinimumCrouchCoverHeight,
		InOutFrameWorldQueries,
		ECoverFinderTraceDomain::AllCoverProviders);
	return not CrouchTrace.bBlockingHit;
}

bool URTSCoverFinderWorldSubsystem::GetCanInfantryOccupyLocation(
	const FVector& GroundLocation,
	int32& InOutFrameWorldQueries)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}

	const float CapsuleHalfHeight = FMath::Max(
		M_ActiveSettings.AgentRadius,
		M_ActiveSettings.AgentHeight * 0.5f);
	const FVector CapsuleCenter = GroundLocation
		+ FVector::UpVector * (CapsuleHalfHeight + CoverFinderWorldSubsystemPrivate::CapsuleGroundClearance);
	++InOutFrameWorldQueries;
	++M_PerformanceAccumulator.WorldQueryCount;
	return not World->OverlapAnyTestByObjectType(
		CapsuleCenter,
		FQuat::Identity,
		CoverFinderWorldSubsystemPrivate::GetAllCoverObjectQueryParams(),
		FCollisionShape::MakeCapsule(M_ActiveSettings.AgentRadius, CapsuleHalfHeight),
		CoverFinderWorldSubsystemPrivate::BuildCoverCollisionQueryParams());
}

FCoverTraceObservation URTSCoverFinderWorldSubsystem::TraceCoverHeight(
	const FVector& GroundLocation,
	const FVector& SearchDirection,
	const float Height,
	int32& InOutFrameWorldQueries,
	const ECoverFinderTraceDomain TraceDomain)
{
	FCoverTraceObservation Observation;
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return Observation;
	}

	const FVector TraceStart = GroundLocation + FVector::UpVector * Height;
	const FVector TraceEnd = TraceStart + SearchDirection * M_ActiveSettings.MaximumCoverSearchDistance;
	FHitResult HitResult;
	++InOutFrameWorldQueries;
	++M_PerformanceAccumulator.WorldQueryCount;
	if (TraceDomain == ECoverFinderTraceDomain::AllCoverProviders)
	{
		Observation.bBlockingHit = World->LineTraceSingleByObjectType(
			HitResult,
			TraceStart,
			TraceEnd,
			CoverFinderWorldSubsystemPrivate::GetAllCoverObjectQueryParams(),
			CoverFinderWorldSubsystemPrivate::BuildCoverCollisionQueryParams());
	}
	else if (M_ActiveScanDomain == ECoverFinderScanDomain::Landscape)
	{
		Observation.bBlockingHit = World->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			COLLISION_TRACE_LANDSCAPE,
			CoverFinderWorldSubsystemPrivate::BuildCoverCollisionQueryParams());
	}
	else
	{
		Observation.bBlockingHit = CoverFinderWorldSubsystemPrivate::TraceEnvironmentCover(
			*World,
			TraceStart,
			TraceEnd,
			HitResult);
	}
	if (not Observation.bBlockingHit)
	{
		return Observation;
	}

	Observation.Distance = HitResult.Distance;
	Observation.ImpactNormal = HitResult.ImpactNormal;
	return Observation;
}

void URTSCoverFinderWorldSubsystem::RebuildPublishedCoverPoints()
{
	int32 CandidateCount = M_LandscapeCoverPoints.Num() + M_EnvironmentCoverPoints.Num();
	for (const TPair<uint64, TArray<FRTSCoverPoint>>& ProviderEntry : M_AuthoredCoverProviders)
	{
		CandidateCount += ProviderEntry.Value.Num();
	}

	TArray<FRTSCoverPoint> CombinedCandidates;
	CombinedCandidates.Reserve(CandidateCount);
	CombinedCandidates.Append(M_LandscapeCoverPoints);
	CombinedCandidates.Append(M_EnvironmentCoverPoints);
	for (const TPair<uint64, TArray<FRTSCoverPoint>>& ProviderEntry : M_AuthoredCoverProviders)
	{
		CombinedCandidates.Append(ProviderEntry.Value);
	}

	M_CoverPoints = FCoverFinderAlgorithms::FinalizeCandidates(
		MoveTemp(CombinedCandidates),
		BuildPublicationSettings());
	M_LastPerformanceSnapshot.CoverPointCount = M_CoverPoints.Num();
}

void URTSCoverFinderWorldSubsystem::RemoveGeneratedDuplicates(
	const TArray<FRTSCoverPoint>& RemovedProviderPoints)
{
	const float MinimumSpacing = BuildPublicationSettings().CoverPointSpacing;
	const auto GetMatchesRemovedProvider = [&RemovedProviderPoints, MinimumSpacing](
		const FRTSCoverPoint& GeneratedPoint)
	{
		for (const FRTSCoverPoint& RemovedPoint : RemovedProviderPoints)
		{
			if (FCoverFinderAlgorithms::GetAreDuplicateCandidates(
				GeneratedPoint,
				RemovedPoint,
				MinimumSpacing))
			{
				return true;
			}
		}
		return false;
	};

	M_LandscapeCoverPoints.RemoveAll(GetMatchesRemovedProvider);
	M_EnvironmentCoverPoints.RemoveAll(GetMatchesRemovedProvider);
}

void URTSCoverFinderWorldSubsystem::RecordSamplingFrame(const double FrameStartSeconds)
{
	const double FrameSeconds = FPlatformTime::Seconds() - FrameStartSeconds;
	M_PerformanceAccumulator.TotalGameThreadSeconds += FrameSeconds;
	M_PerformanceAccumulator.MaximumGameThreadFrameSeconds = FMath::Max(
		M_PerformanceAccumulator.MaximumGameThreadFrameSeconds,
		FrameSeconds);
	++M_PerformanceAccumulator.SamplingFrameCount;
}

void URTSCoverFinderWorldSubsystem::PublishPerformanceSnapshot(
	const FCoverGenerationResult& GenerationResult)
{
	M_LastPerformanceSnapshot.ProjectedSampleCount = M_PerformanceAccumulator.ProjectedSampleCount;
	M_LastPerformanceSnapshot.WorldQueryCount = M_PerformanceAccumulator.WorldQueryCount;
	M_LastPerformanceSnapshot.CoverPointCount = GenerationResult.CoverPoints.Num();
	M_LastPerformanceSnapshot.RawCandidateCount = GenerationResult.RawCandidateCount;
	M_LastPerformanceSnapshot.StandingSurfaceDirectionCount =
		M_PerformanceAccumulator.StandingSurfaceDirectionCount;
	M_LastPerformanceSnapshot.StandingOpeningCount = M_PerformanceAccumulator.StandingOpeningCount;
	M_LastPerformanceSnapshot.ValidatedStandingGapCount =
		M_PerformanceAccumulator.ValidatedStandingGapCount;
	M_LastPerformanceSnapshot.TotalGameThreadMilliseconds = static_cast<float>(
		M_PerformanceAccumulator.TotalGameThreadSeconds * 1000.0);
	M_LastPerformanceSnapshot.MaximumGameThreadMillisecondsInOneSamplingFrame = static_cast<float>(
		M_PerformanceAccumulator.MaximumGameThreadFrameSeconds * 1000.0);
	M_LastPerformanceSnapshot.AverageGameThreadMillisecondsPerSamplingFrame =
		M_PerformanceAccumulator.SamplingFrameCount > 0
			? M_LastPerformanceSnapshot.TotalGameThreadMilliseconds
			/ static_cast<float>(M_PerformanceAccumulator.SamplingFrameCount)
			: 0.0f;
	M_LastPerformanceSnapshot.WorkerMilliseconds = GenerationResult.WorkerMilliseconds;
	M_LastPerformanceSnapshot.WallClockMilliseconds = static_cast<float>(
		(FPlatformTime::Seconds() - M_PerformanceAccumulator.ScanStartSeconds) * 1000.0);
}

void URTSCoverFinderWorldSubsystem::DrawPublishedCover() const
{
	if constexpr (not DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		return;
	}

	const URTSCoverFinderDeveloperSettings* CoverSettings = GetCoverFinderSettings();
	UWorld* World = GetWorld();
	if (not IsValid(CoverSettings) || not CoverSettings->bM_DrawDetectedCover || not IsValid(World))
	{
		return;
	}

	for (const FRTSCoverPoint& CoverPoint : M_CoverPoints)
	{
		const bool bCrouchCover = CoverPoint.CoverType == ERTSCoverType::Crouch;
		const FColor DrawColor = bCrouchCover
			? CoverFinderWorldSubsystemPrivate::CrouchCoverColor
			: CoverFinderWorldSubsystemPrivate::StandingCoverColor;
		const FVector DrawLocation = CoverPoint.Location
			+ FVector::UpVector * CoverFinderWorldSubsystemPrivate::DebugPointHeight;
		DrawDebugSphere(
			World,
			DrawLocation,
			CoverFinderWorldSubsystemPrivate::DebugSphereRadius,
			8,
			DrawColor,
			false,
			CoverSettings->M_DebugDrawDurationSeconds,
			0,
			CoverFinderWorldSubsystemPrivate::DebugLineThickness);
		const FVector ArrowDirection = bCrouchCover
			? CoverPoint.CoverNormal
			: CoverFinderWorldSubsystemPrivate::GetStandingSideDirection(
				-CoverPoint.CoverNormal,
				CoverPoint.CoverType);
		DrawDebugDirectionalArrow(
			World,
			DrawLocation,
			DrawLocation + ArrowDirection * CoverFinderWorldSubsystemPrivate::DebugNormalLength,
			CoverFinderWorldSubsystemPrivate::DebugArrowHeadSize,
			DrawColor,
			false,
			CoverSettings->M_DebugDrawDurationSeconds,
			0,
			CoverFinderWorldSubsystemPrivate::DebugLineThickness);
	}
}

void URTSCoverFinderWorldSubsystem::PrepareDebugCapture()
{
	bM_CaptureAfterScan = false;
	UWorld* World = GetWorld();
	APlayerController* PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	if (not IsValid(PlayerController))
	{
		UE_LOG(LogRTSCoverFinder, Error, TEXT("Cover debug capture failed: no player controller."));
		return;
	}

	FBox FrameBounds(ForceInit);
	for (const FRTSCoverPoint& CoverPoint : M_CoverPoints)
	{
		FrameBounds += CoverPoint.Location;
	}
	if (FrameBounds.IsValid == 0)
	{
		FrameBounds = M_LastScanBounds;
	}
	if (FrameBounds.IsValid == 0)
	{
		return;
	}

	const FVector BoundsSize = FrameBounds.GetSize();
	const float LargestHorizontalExtent = FMath::Max(BoundsSize.X, BoundsSize.Y);
	const float CameraHeight = FMath::Clamp(
		LargestHorizontalExtent * CoverFinderWorldSubsystemPrivate::CaptureHeightScale,
		CoverFinderWorldSubsystemPrivate::CaptureMinimumHeight,
		CoverFinderWorldSubsystemPrivate::CaptureMaximumHeight);
	const FVector CameraLocation = FrameBounds.GetCenter() + FVector::UpVector * CameraHeight;
	ACameraActor* CameraActor = World->SpawnActor<ACameraActor>(CameraLocation, FRotator(-90.0f, 0.0f, 0.0f));
	if (not IsValid(CameraActor))
	{
		UE_LOG(LogRTSCoverFinder, Error, TEXT("Cover debug capture failed: camera actor could not be spawned."));
		return;
	}
	CameraActor->GetCameraComponent()->SetFieldOfView(70.0f);
	PlayerController->SetViewTarget(CameraActor);
	if (GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		bM_CaptureLightingWasEnabled = GEngine->GameViewport->EngineShowFlags.Lighting;
		bM_RestoreCaptureLighting = true;
		ApplyViewMode(VMI_Unlit, true, GEngine->GameViewport->EngineShowFlags);
	}
	M_CaptureFramesRemaining = CoverFinderWorldSubsystemPrivate::CaptureFrameDelay;
}

void URTSCoverFinderWorldSubsystem::TickDebugCapture()
{
	if (M_CaptureFramesRemaining == -1)
	{
		RestoreCaptureLighting();
		M_CaptureFramesRemaining = 0;
		return;
	}
	if (M_CaptureFramesRemaining <= 0)
	{
		return;
	}

	--M_CaptureFramesRemaining;
	if (M_CaptureFramesRemaining > 0)
	{
		return;
	}
	FScreenshotRequest::RequestScreenshot(M_PendingScreenshotPath, false, false);
	M_CaptureFramesRemaining = -1;
	UE_LOG(LogRTSCoverFinder, Display, TEXT("RTS_COVER_SCREENSHOT_REQUESTED path=%s"), *M_PendingScreenshotPath);
}

void URTSCoverFinderWorldSubsystem::RestoreCaptureLighting()
{
	if (not bM_RestoreCaptureLighting)
	{
		return;
	}
	if (GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		const EViewModeIndex RestoredViewMode = bM_CaptureLightingWasEnabled ? VMI_Lit : VMI_Unlit;
		ApplyViewMode(RestoredViewMode, true, GEngine->GameViewport->EngineShowFlags);
	}
	bM_RestoreCaptureLighting = false;
}
