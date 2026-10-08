#include "CoverFinderWorldSubsystem.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Components/InstancedStaticMeshComponent.h"
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
#include "PhysicsEngine/BodyInstance.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderDeveloperSettings.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorker.h"
#include "RTS_Survival/Navigation/RTSNavAgentRegistery/RTSNavAgentRegistery.h"
#include "RTS_Survival/Navigation/RTSNavAgents/ERTSNavAgents.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "ShowFlags.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCoverFinder, Log, All);

DECLARE_STATS_GROUP(TEXT("RTS Cover Finder"), STATGROUP_RTSCoverFinder, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Game-thread sampling"), STAT_RTSCoverFinder_GameThreadSampling, STATGROUP_RTSCoverFinder);
DECLARE_CYCLE_STAT(TEXT("Tactical cover updates"), STAT_RTSCoverFinder_TacticalUpdates, STATGROUP_RTSCoverFinder);

namespace CoverFinderWorldSubsystemPrivate
{
	constexpr float FullCircleDegrees = 360.0f;
	constexpr float SurfaceDistanceToleranceRatio = 0.65f;
	constexpr float MinimumSurfaceDistanceTolerance = 40.0f;
	// Above this footprint an object's scan area is cut down to the navigation tiles it overlaps.
	constexpr float LargeObjectFootprintSquareCentimeters = 2500.0f * 10000.0f;
	// Objects wider than this are walls and wrecks, not trees whose trunk is worth looking for.
	constexpr float MaximumObjectWidthForTrunkMeasurement = 2000.0f;
	constexpr int32 TrunkMeasurementRayCount = 8;
	// With fewer rays hitting, the pivot is not inside anything solid at crouch height.
	constexpr int32 MinimumTrunkMeasurementHits = 4;
	constexpr float TrunkGroundSearchExtent = 250.0f;
	constexpr float TrunkRemeasureDistance = 10.0f;
	// Two re-aimed probes closer together than this, looking the same way, would find the same cover.
	constexpr float ReaimDeduplicationCellSize = 45.0f;
	constexpr float ReaimDeduplicationYawStepDegrees = 30.0f;
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
	constexpr int32 StandingEdgeRefinementSteps = 3;
	// Worst case for one direction, including the edge refinement traces of both sides.
	constexpr int32 MaximumQueriesForOneDirection = 105 + StandingEdgeRefinementSteps * 2;
	constexpr float CoverSpatialCellSize = 500.0f;
	constexpr float CrouchFiringHeight = 150.0f;
	constexpr float StandingFiringHeight = 160.0f;
	constexpr int32 MaximumCandidateLaneTests = 8;
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

	/** Soldiers and what they carry stand on cover points all the time and must never change the published cover. */
	bool GetIsInfantryActor(const AActor* Actor)
	{
		if (not IsValid(Actor))
		{
			return false;
		}
		return IsValid(Cast<ACharacter>(Actor)) ||
			IsValid(Cast<ACharacter>(Actor->GetAttachParentActor())) ||
			IsValid(Cast<ACharacter>(Actor->GetOwner()));
	}

	/** @return Owning player of the actor or of the unit that owns it; INDEX_NONE for map geometry. */
	int32 FindOwningPlayer(const AActor* Actor)
	{
		constexpr int32 MaximumOwnerDepth = 4;
		const AActor* OwnershipActor = Actor;
		for (int32 OwnerDepth = 0; OwnerDepth < MaximumOwnerDepth && IsValid(OwnershipActor); ++OwnerDepth)
		{
			const URTSComponent* RTSComponent = OwnershipActor->FindComponentByClass<URTSComponent>();
			if (IsValid(RTSComponent))
			{
				return RTSComponent->GetOwningPlayer();
			}
			OwnershipActor = OwnershipActor->GetOwner();
		}
		return INDEX_NONE;
	}

	bool GetIsLandscapeComponent(const UPrimitiveComponent* PrimitiveComponent)
	{
		return IsValid(PrimitiveComponent)
			&& PrimitiveComponent->GetCollisionResponseToChannel(COLLISION_TRACE_LANDSCAPE) == ECR_Block;
	}

	FCollisionQueryParams BuildCoverCollisionQueryParams();

	void AppendEnvironmentScanBounds(
		const FBox& CollisionBounds,
		const float HorizontalExpansion,
		TArray<FBox>& OutEnvironmentBounds)
	{
		if (CollisionBounds.IsValid == 0)
		{
			return;
		}
		FBox EnvironmentBounds = CollisionBounds;
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
			if (GetIsLandscapeComponent(HitResult.GetComponent()) ||
				GetIsInfantryActor(HitResult.GetActor()))
			{
				continue;
			}
			OutHitResult = HitResult;
			return true;
		}
		return false;
	}

	bool TraceAllCoverProviders(
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
			if (GetIsInfantryActor(HitResult.GetActor()))
			{
				continue;
			}
			OutHitResult = HitResult;
			return true;
		}
		return false;
	}

	bool GetHasNonInfantryOverlap(
		const UWorld& World,
		const FVector& ShapeCenter,
		const FCollisionShape& CollisionShape)
	{
		TArray<FOverlapResult> Overlaps;
		World.OverlapMultiByObjectType(
			Overlaps,
			ShapeCenter,
			FQuat::Identity,
			GetAllCoverObjectQueryParams(),
			CollisionShape,
			BuildCoverCollisionQueryParams());
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (not GetIsInfantryActor(Overlap.GetActor()))
			{
				return true;
			}
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
		URTSCoverFinderWorldSubsystem* GameCoverSubsystem = nullptr;
		if (IsValid(GEngine))
		{
			for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
			{
				UWorld* CandidateWorld = WorldContext.World();
				if (not IsValid(CandidateWorld) || not CandidateWorld->HasBegunPlay())
				{
					continue;
				}
				if (CandidateWorld->WorldType == EWorldType::Game ||
					CandidateWorld->WorldType == EWorldType::PIE)
				{
					GameCoverSubsystem = CandidateWorld->GetSubsystem<URTSCoverFinderWorldSubsystem>();
				}
			}
		}
		return IsValid(GameCoverSubsystem)
			? GameCoverSubsystem
			: IsValid(World) ? World->GetSubsystem<URTSCoverFinderWorldSubsystem>() : nullptr;
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

	void RunTestCoverValidationCommand(const TArray<FString>& Arguments, UWorld* World)
	{
		URTSCoverFinderWorldSubsystem* CoverSubsystem = GetCoverSubsystem(World);
		if (not IsValid(CoverSubsystem))
		{
			UE_LOG(LogRTSCoverFinder, Error, TEXT("RTS.CoverFinder.ValidateTestCover failed: subsystem is unavailable."));
			return;
		}
		constexpr float DefaultValidationDelaySeconds = 15.0f;
		const float DelaySeconds = Arguments.IsEmpty()
			? DefaultValidationDelaySeconds
			: FMath::Max(1.0f, FCString::Atof(*Arguments[0]));
		const bool bCaptureScreenshot = Arguments.Num() > 1 && Arguments[1].Equals(TEXT("capture"));
		CoverSubsystem->RequestTestCoverValidation(DelaySeconds, bCaptureScreenshot);
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

	FAutoConsoleCommandWithWorldAndArgs GCoverFinderValidateTestCoverCommand(
		TEXT("RTS.CoverFinder.ValidateTestCover"),
		TEXT("Runs the TestCover scenario: waits for the cover scan, checks idle cover after the given seconds, then walks the enemy into contact and checks cover use in combat. Add 'capture' as the second argument."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunTestCoverValidationCommand));
}

void FCoverFinderSamplingState::Reset()
{
	SampleLocations.Reset();
	FocusedSamples.Reset();
	PendingTrunkMeasurements.Reset();
	ReaimedSampleCount = 0;
	MeasuredTrunkCount = 0;
	ReusedRingObstacleCount = 0;
	CurrentObstacleCacheId = 0;
	QueuedReaimKeys.Reset();
	ObservationChunk.Reset();
	CurrentObservation = FCoverProbeObservation();
	CurrentFocusDirection = FVector::ZeroVector;
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
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		RegisterExplainConsoleCommand();
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
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		UnregisterExplainConsoleCommand();
	}
	RestoreCaptureLighting();
	if (M_Worker != nullptr)
	{
		M_Worker->StopAndWait();
		M_Worker.Reset();
	}
	M_SamplingState.Reset();
	M_PendingThinObstacleSamples.Reset();
	M_PendingReusedRingCandidates.Reset();
	M_PendingTrunkMeasurements.Reset();
	M_ThinObstacleCaches.Reset();
	M_ObstacleKeysByCacheId.Reset();
	M_CoverPoints.Reset();
	M_LandscapeCoverPoints.Reset();
	M_EnvironmentCoverPoints.Reset();
	M_AuthoredCoverProviders.Reset();
	M_BlockingProviderActors.Reset();
	M_BlockingProviderHandles.Reset();
	M_CoverPointIndices.Reset();
	M_CoverSpatialGrid.Reset();
	M_CoverReservations.Reset();
	M_UnreachableCoverPointExpiry.Reset();
	M_RegisteredSquadUnits.Reset();
	M_CachedNavigationTileBounds.Reset();
	Super::Deinitialize();
}

void URTSCoverFinderWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		constexpr float DefaultCommandLineValidationDelaySeconds = 20.0f;
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
		if (FParse::Param(FCommandLine::Get(), TEXT("CoverFinderValidateCombatCover")))
		{
			M_CombatCoverScenario.Start();
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("CoverFinderValidateTestCover")))
		{
			float ValidationDelaySeconds = DefaultCommandLineValidationDelaySeconds;
			FParse::Value(
				FCommandLine::Get(),
				TEXT("CoverFinderValidationDelay="),
				ValidationDelaySeconds);
			const bool bCaptureValidation = FParse::Param(
				FCommandLine::Get(),
				TEXT("CoverFinderValidationCapture"));
			RequestTestCoverValidation(ValidationDelaySeconds, bCaptureValidation);
		}
	}
}

void URTSCoverFinderWorldSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	TickDebugCapture();
	M_TestCoverScenario.Tick(*this, DeltaTime);
	M_CombatCoverScenario.Tick(*this);
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		TickCommandLineExplain();
	}

	const URTSCoverFinderDeveloperSettings* CoverSettings = GetCoverFinderSettings();
	if (IsValid(CoverSettings) && CoverSettings->bM_EnableCoverSearch && M_Worker != nullptr)
	{
		PollWorkerResults();
		if (M_ScanState == ECoverFinderScanState::SamplingWorld)
		{
			ProcessWorldSampling();
		}
		else if (M_ScanState == ECoverFinderScanState::Idle)
		{
			M_TimeUntilNextScan -= DeltaTime;
			if (M_TimeUntilNextScan <= 0.0f)
			{
				BeginScan();
			}
		}
	}

	TickTacticalCoverUnits();
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
	const FIntPoint MinimumCell = GetCoverSpatialCell(
		SearchLocation - FVector(SearchRadius, SearchRadius, 0.0f));
	const FIntPoint MaximumCell = GetCoverSpatialCell(
		SearchLocation + FVector(SearchRadius, SearchRadius, 0.0f));
	for (int32 CellX = MinimumCell.X; CellX <= MaximumCell.X; ++CellX)
	{
		for (int32 CellY = MinimumCell.Y; CellY <= MaximumCell.Y; ++CellY)
		{
			const TArray<int32>* CellPointIndices = M_CoverSpatialGrid.Find(FIntPoint(CellX, CellY));
			if (CellPointIndices == nullptr)
			{
				continue;
			}
			for (const int32 CoverPointIndex : *CellPointIndices)
			{
				if (not M_CoverPoints.IsValidIndex(CoverPointIndex))
				{
					continue;
				}
				const FRTSCoverPoint& CoverPoint = M_CoverPoints[CoverPointIndex];
				if (FVector::DistSquared(SearchLocation, CoverPoint.Location) <= SearchRadiusSquared)
				{
					NearbyCoverPoints.Add(CoverPoint);
				}
			}
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
	AActor* ProviderActor,
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
	const uint64 BlockingProviderHandle = FindOrAddBlockingProviderHandle(ProviderActor);
	for (FRTSCoverPoint& CoverPoint : CoverPoints)
	{
		CoverPoint.ProviderRegistrationId = RegistrationId;
		CoverPoint.BlockingProviderHandle = BlockingProviderHandle;
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

void URTSCoverFinderWorldSubsystem::RegisterSquadUnit(ASquadUnit* SquadUnit)
{
	if (not IsValid(SquadUnit))
	{
		return;
	}
	M_RegisteredSquadUnits.AddUnique(TWeakObjectPtr<ASquadUnit>(SquadUnit));
}

void URTSCoverFinderWorldSubsystem::UnregisterSquadUnit(ASquadUnit* SquadUnit)
{
	if (not IsValid(SquadUnit))
	{
		return;
	}
	const int64 ReservedPointId = SquadUnit->GetCoverRuntimeState().AssignedCoverPoint.PointId;
	ReleaseCoverReservation(*SquadUnit, ReservedPointId);
	M_RegisteredSquadUnits.Remove(TWeakObjectPtr<ASquadUnit>(SquadUnit));
	M_NextTacticalUnitIndex = FMath::Min(M_NextTacticalUnitIndex, M_RegisteredSquadUnits.Num());
}

bool URTSCoverFinderWorldSubsystem::TryReserveBestCoverPoint(
	ASquadUnit& SquadUnit,
	AActor* TargetActor,
	const FVector& TargetLocation,
	FRTSCoverPoint& OutCoverPoint,
	const float MaximumDistanceToTarget)
{
	const URTSCoverFinderDeveloperSettings* CoverSettings = GetCoverFinderSettings();
	if (not IsValid(CoverSettings) || M_CoverPoints.IsEmpty())
	{
		return false;
	}
	const float SearchRadius = FMath::Max(100.0f, CoverSettings->M_AutomaticCoverSearchRadius);
	const TArray<FRTSCoverPoint> BestCandidates = GatherBestTacticalCoverCandidates(
		SquadUnit,
		TargetActor,
		TargetLocation,
		SearchRadius,
		MaximumDistanceToTarget);
	for (const FRTSCoverPoint& CoverPoint : BestCandidates)
	{
		if (IsValid(TargetActor) && not GetHasTargetSpecificFiringLane(
			SquadUnit,
			CoverPoint,
			*TargetActor,
			TargetLocation))
		{
			continue;
		}
		M_CoverReservations.Add(CoverPoint.PointId, TWeakObjectPtr<ASquadUnit>(&SquadUnit));
		OutCoverPoint = CoverPoint;
		return true;
	}
	return false;
}

/** A candidate point with its combat score, so only the best few get a firing-lane trace. */
struct FRTSScoredCombatCoverPoint
{
	FRTSCoverPoint CoverPoint;
	float Score = 0.0f;
};

FRTSCombatCoverSettings URTSCoverFinderWorldSubsystem::BuildCombatCoverSettings() const
{
	FRTSCombatCoverSettings CombatSettings;
	const URTSCoverFinderDeveloperSettings* CoverSettings = GetCoverFinderSettings();
	if (not IsValid(CoverSettings))
	{
		return CombatSettings;
	}
	CombatSettings.MaximumAimYawDegrees = FMath::Clamp(CoverSettings->M_CombatCoverMaximumAimYawDegrees, 20.0f, 90.0f);
	CombatSettings.ProtectedHalfAngleDegrees = FMath::Clamp(
		CoverSettings->M_CombatCoverProtectedHalfAngleDegrees,
		10.0f,
		90.0f);
	CombatSettings.ProtectionWeight = FMath::Max(0.0f, CoverSettings->M_CombatCoverProtectionWeight);
	CombatSettings.FacingWeight = FMath::Max(0.0f, CoverSettings->M_CombatCoverFacingWeight);
	CombatSettings.TravelWeight = FMath::Max(0.0f, CoverSettings->M_CombatCoverTravelWeight);
	CombatSettings.TravelReferenceDistance = FMath::Max(100.0f, CoverSettings->M_CombatCoverRepositionRadius);
	CombatSettings.MinimumScoreGain = FMath::Max(0.0f, CoverSettings->M_CombatCoverMinimumScoreGain);
	return CombatSettings;
}

bool URTSCoverFinderWorldSubsystem::GetIsCoverPointAvailableToUnit(
	const FRTSCoverPoint& CoverPoint,
	const ASquadUnit& SquadUnit) const
{
	if (GetIsPointReservedByAnotherUnit(CoverPoint.PointId, SquadUnit) ||
		GetIsCoverPointTemporarilyUnreachable(CoverPoint.PointId) ||
		SquadUnit.GetIsCoverPointRejectedForTarget(CoverPoint.PointId))
	{
		return false;
	}
	if (CoverPoint.BlockingProviderHandle != 0 && not IsValid(ResolveBlockingProvider(CoverPoint)))
	{
		return false;
	}
	// A pole has cover on every side but no room for a whole squad.
	return CoverPoint.ThinObstacleId == 0 ||
		GetThinObstacleReservationCount(CoverPoint.ThinObstacleId, &SquadUnit) < CoverPoint.ThinObstacleCapacity;
}

bool URTSCoverFinderWorldSubsystem::TryReserveCombatCoverPoint(
	ASquadUnit& SquadUnit,
	AActor& TargetActor,
	const FRTSCombatCoverThreats& Threats,
	const float MaximumDistanceToTarget,
	const FRTSCoverPoint* OccupiedCoverPoint,
	FRTSCoverPoint& OutCoverPoint)
{
	const URTSCoverFinderDeveloperSettings* CoverSettings = GetCoverFinderSettings();
	if (not IsValid(CoverSettings) || M_CoverPoints.IsEmpty())
	{
		return false;
	}
	const FRTSCombatCoverSettings CombatSettings = BuildCombatCoverSettings();
	const FVector UnitLocation = SquadUnit.GetActorLocation();
	const float SearchRadius = OccupiedCoverPoint != nullptr
		? CombatSettings.TravelReferenceDistance
		: FMath::Max(100.0f, CoverSettings->M_AutomaticCoverSearchRadius);
	// A point the unit cannot aim from any more is worth leaving for anything usable.
	float ScoreToBeat = TNumericLimits<float>::Lowest();
	float OccupiedScore = 0.0f;
	if (OccupiedCoverPoint != nullptr && FRTSCombatCoverScoring::TryScoreCoverPoint(
		*OccupiedCoverPoint, UnitLocation, Threats, CombatSettings, OccupiedScore))
	{
		ScoreToBeat = OccupiedScore + CombatSettings.MinimumScoreGain;
	}

	const float MaximumDistanceToTargetSquared = FMath::Square(MaximumDistanceToTarget);
	const TArray<FRTSCoverPoint> NearbyPoints = FindCoverPointsInRadius(UnitLocation, SearchRadius);
	M_TacticalPerformanceSnapshot.CandidateChecksLastFrame += NearbyPoints.Num();
	TArray<FRTSScoredCombatCoverPoint> ScoredCandidates;
	ScoredCandidates.Reserve(NearbyPoints.Num());
	for (const FRTSCoverPoint& CoverPoint : NearbyPoints)
	{
		const bool bIsOccupiedPoint = OccupiedCoverPoint != nullptr && CoverPoint.PointId == OccupiedCoverPoint->PointId;
		const bool bOutOfWeaponRange = MaximumDistanceToTarget > 0.0f && FVector::DistSquared(
			CoverPoint.Location, Threats.PrimaryTargetLocation) > MaximumDistanceToTargetSquared;
		float CandidateScore = 0.0f;
		if (bIsOccupiedPoint || bOutOfWeaponRange || not GetIsCoverPointAvailableToUnit(CoverPoint, SquadUnit) ||
			not FRTSCombatCoverScoring::TryScoreCoverPoint(
				CoverPoint, UnitLocation, Threats, CombatSettings, CandidateScore) ||
			CandidateScore < ScoreToBeat)
		{
			continue;
		}
		ScoredCandidates.Add({CoverPoint, CandidateScore});
	}
	ScoredCandidates.Sort([](const FRTSScoredCombatCoverPoint& Left, const FRTSScoredCombatCoverPoint& Right)
	{
		return Left.Score > Right.Score;
	});

	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		UE_LOG(
			LogRTSCoverFinder,
			Verbose,
			TEXT("RTS_COVER_COMBAT_SEARCH unit=%s occupied=%d occupied_score=%.2f nearby=%d better=%d best_score=%.2f threats=%d"),
			*SquadUnit.GetName(),
			OccupiedCoverPoint != nullptr ? 1 : 0,
			OccupiedScore,
			NearbyPoints.Num(),
			ScoredCandidates.Num(),
			ScoredCandidates.IsEmpty() ? 0.0f : ScoredCandidates[0].Score,
			Threats.ThreatLocations.Num());
	}
	const int32 LaneTestCount = FMath::Min(
		ScoredCandidates.Num(),
		CoverFinderWorldSubsystemPrivate::MaximumCandidateLaneTests);
	for (int32 CandidateIndex = 0; CandidateIndex < LaneTestCount; ++CandidateIndex)
	{
		const FRTSCoverPoint& CoverPoint = ScoredCandidates[CandidateIndex].CoverPoint;
		if (not GetHasTargetSpecificFiringLane(SquadUnit, CoverPoint, TargetActor, Threats.PrimaryTargetLocation))
		{
			continue;
		}
		M_CoverReservations.Add(CoverPoint.PointId, TWeakObjectPtr<ASquadUnit>(&SquadUnit));
		OutCoverPoint = CoverPoint;
		return true;
	}
	return false;
}

TArray<FRTSCoverPoint> URTSCoverFinderWorldSubsystem::GatherBestTacticalCoverCandidates(
	const ASquadUnit& SquadUnit,
	const AActor* TargetActor,
	const FVector& TargetLocation,
	const float SearchRadius,
	const float MaximumDistanceToTarget)
{
	const FVector UnitLocation = SquadUnit.GetActorLocation();
	TArray<FRTSCoverPoint> BestCandidates = FindCoverPointsInRadius(UnitLocation, SearchRadius);
	M_TacticalPerformanceSnapshot.CandidateChecksLastFrame += BestCandidates.Num();
	const bool bLimitTargetDistance = IsValid(TargetActor) && MaximumDistanceToTarget > 0.0f;
	const float MaximumDistanceToTargetSquared = FMath::Square(MaximumDistanceToTarget);
	BestCandidates.RemoveAll([&](const FRTSCoverPoint& CoverPoint)
	{
		if (bLimitTargetDistance &&
			FVector::DistSquared(CoverPoint.Location, TargetLocation) > MaximumDistanceToTargetSquared)
		{
			return true;
		}
		if (not GetIsCoverPointAvailableToUnit(CoverPoint, SquadUnit))
		{
			return true;
		}
		return IsValid(TargetActor) && not GetIsCandidateProtectedFromTarget(CoverPoint, TargetLocation);
	});
	BestCandidates.Sort([&UnitLocation](const FRTSCoverPoint& Left, const FRTSCoverPoint& Right)
	{
		return FVector::DistSquared(UnitLocation, Left.Location) <
			FVector::DistSquared(UnitLocation, Right.Location);
	});
	if (BestCandidates.Num() > CoverFinderWorldSubsystemPrivate::MaximumCandidateLaneTests)
	{
		BestCandidates.SetNum(CoverFinderWorldSubsystemPrivate::MaximumCandidateLaneTests);
	}
	return BestCandidates;
}

bool URTSCoverFinderWorldSubsystem::TryReserveCoverPointById(
	ASquadUnit& SquadUnit,
	const int64 PointId,
	FRTSCoverPoint& OutCoverPoint)
{
	const int32* CoverPointIndex = M_CoverPointIndices.Find(PointId);
	if (CoverPointIndex == nullptr || not M_CoverPoints.IsValidIndex(*CoverPointIndex))
	{
		return false;
	}
	if (GetIsPointReservedByAnotherUnit(PointId, SquadUnit) || GetIsCoverPointTemporarilyUnreachable(PointId))
	{
		return false;
	}
	const FRTSCoverPoint& RequestedPoint = M_CoverPoints[*CoverPointIndex];
	const bool bObstacleIsFull = RequestedPoint.ThinObstacleId != 0 && GetThinObstacleReservationCount(
		RequestedPoint.ThinObstacleId, &SquadUnit) >= RequestedPoint.ThinObstacleCapacity;
	if (bObstacleIsFull)
	{
		return false;
	}
	M_CoverReservations.Add(PointId, TWeakObjectPtr<ASquadUnit>(&SquadUnit));
	OutCoverPoint = M_CoverPoints[*CoverPointIndex];
	return true;
}

const ASquadUnit* URTSCoverFinderWorldSubsystem::GetCoverReservationOwner(const int64 PointId) const
{
	const TWeakObjectPtr<ASquadUnit>* ReservingUnit = M_CoverReservations.Find(PointId);
	return ReservingUnit != nullptr ? ReservingUnit->Get() : nullptr;
}

void URTSCoverFinderWorldSubsystem::ReleaseCoverReservation(
	const ASquadUnit& SquadUnit,
	const int64 PointId)
{
	if (PointId == 0)
	{
		return;
	}
	const TWeakObjectPtr<ASquadUnit>* ReservingUnit = M_CoverReservations.Find(PointId);
	if (ReservingUnit == nullptr || ReservingUnit->Get() != &SquadUnit)
	{
		return;
	}
	M_CoverReservations.Remove(PointId);
}

void URTSCoverFinderWorldSubsystem::ReportCoverPointUnreachable(const int64 PointId)
{
	const UWorld* World = GetWorld();
	if (PointId == 0 || not IsValid(World))
	{
		return;
	}
	// Long enough to stop a queue of units trying the same slot, short enough to recover once a blocker leaves.
	constexpr float UnreachablePointTimeoutSeconds = 30.0f;
	M_UnreachableCoverPointExpiry.Add(PointId, World->GetTimeSeconds() + UnreachablePointTimeoutSeconds);
}

bool URTSCoverFinderWorldSubsystem::GetIsCoverPointTemporarilyUnreachable(const int64 PointId) const
{
	const UWorld* World = GetWorld();
	const float* ExpiryWorldSeconds = M_UnreachableCoverPointExpiry.Find(PointId);
	return ExpiryWorldSeconds != nullptr && IsValid(World) && World->GetTimeSeconds() < *ExpiryWorldSeconds;
}

bool URTSCoverFinderWorldSubsystem::GetIsCoverPointPublished(const int64 PointId) const
{
	return PointId != 0 && M_CoverPointIndices.Contains(PointId);
}

AActor* URTSCoverFinderWorldSubsystem::ResolveBlockingProvider(const FRTSCoverPoint& CoverPoint) const
{
	if (CoverPoint.BlockingProviderHandle == 0)
	{
		return nullptr;
	}
	const TWeakObjectPtr<AActor>* ProviderActor = M_BlockingProviderActors.Find(
		CoverPoint.BlockingProviderHandle);
	return ProviderActor != nullptr ? ProviderActor->Get() : nullptr;
}

bool URTSCoverFinderWorldSubsystem::GetHasTargetSpecificFiringLane(
	const ASquadUnit& SquadUnit,
	const FRTSCoverPoint& CoverPoint,
	const AActor& TargetActor,
	const FVector& TargetLocation)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	const ECollisionChannel TargetTraceChannel = SquadUnit.GetOwningPlayer() == 1
		? COLLISION_TRACE_ENEMY
		: COLLISION_TRACE_PLAYER;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RTSCoverFiringLane), true);
	QueryParams.AddIgnoredActor(&SquadUnit);
	FHitResult HitResult;
	++M_TacticalPerformanceSnapshot.FiringLaneTracesLastFrame;
	const FVector LaneStart = BuildFiringLaneStart(SquadUnit, CoverPoint);
	const bool bBlockingHit = World->LineTraceSingleByChannel(
		HitResult,
		LaneStart,
		TargetLocation,
		TargetTraceChannel,
		QueryParams);
	const AActor* HitActor = HitResult.GetActor();
	// Another enemy standing in front of the target is still something this unit may shoot, so the lane stays open.
	constexpr int32 PlayerOwnedTeam = 1;
	const int32 HitOwningPlayer = bBlockingHit
		? CoverFinderWorldSubsystemPrivate::FindOwningPlayer(HitActor)
		: INDEX_NONE;
	const bool bHitOtherEnemy = HitOwningPlayer > 0 &&
		(HitOwningPlayer == PlayerOwnedTeam) != (SquadUnit.GetOwningPlayer() == PlayerOwnedTeam);
	const bool bHitTarget = bBlockingHit && (HitActor == &TargetActor || bHitOtherEnemy ||
		(IsValid(HitActor) && HitActor->IsOwnedBy(&TargetActor)) ||
		(IsValid(HitActor) && TargetActor.IsOwnedBy(HitActor)));
	M_TacticalPerformanceSnapshot.FiringLaneRejectionsLastFrame += bHitTarget ? 0 : 1;
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		UE_LOG(
			LogRTSCoverFinder,
			Verbose,
			TEXT("Firing lane %s unit=%s type=%d target=%s hit=%s comp=%s lane_cm=%.0f hit_cm=%.0f start_z_above_target=%.0f"),
			bHitTarget ? TEXT("open") : TEXT("rejected"),
			*SquadUnit.GetName(),
			static_cast<int32>(CoverPoint.CoverType),
			*TargetActor.GetName(),
			bBlockingHit ? *GetNameSafe(HitActor) : TEXT("nothing"),
			bBlockingHit ? *GetNameSafe(HitResult.GetComponent()) : TEXT("-"),
			FVector::Dist(LaneStart, TargetLocation),
			bBlockingHit ? HitResult.Distance : -1.0f,
			LaneStart.Z - TargetLocation.Z);
	}
	return bHitTarget;
}

bool URTSCoverFinderWorldSubsystem::GetIsCoverPointValidAgainstTarget(
	const ASquadUnit& SquadUnit,
	const FRTSCoverPoint& CoverPoint,
	const AActor& TargetActor,
	const FVector& TargetLocation)
{
	if (not GetIsCandidateProtectedFromTarget(CoverPoint, TargetLocation))
	{
		return false;
	}
	return GetHasTargetSpecificFiringLane(SquadUnit, CoverPoint, TargetActor, TargetLocation);
}

FVector URTSCoverFinderWorldSubsystem::GetStandingPeekOffset(const FRTSCoverPoint& CoverPoint) const
{
	if (CoverPoint.CoverType == ERTSCoverType::Crouch)
	{
		return FVector::ZeroVector;
	}
	constexpr float MinimumPeekOffset = 84.0f;
	const FVector SideDirection = CoverFinderWorldSubsystemPrivate::GetStandingSideDirection(
		-CoverPoint.CoverNormal,
		CoverPoint.CoverType);
	return SideDirection * FMath::Max(M_CachedAgentRadius * 2.0f, MinimumPeekOffset);
}

uint64 URTSCoverFinderWorldSubsystem::FindOrAddBlockingProviderHandle(AActor* ProviderActor)
{
	if (not IsValid(ProviderActor) || CoverFinderWorldSubsystemPrivate::GetIsInfantryActor(ProviderActor))
	{
		return 0;
	}
	const TWeakObjectPtr<AActor> WeakProviderActor(ProviderActor);
	if (const uint64* ExistingHandle = M_BlockingProviderHandles.Find(WeakProviderActor))
	{
		return *ExistingHandle;
	}

	uint64 ProviderHandle = M_NextBlockingProviderHandle;
	while (ProviderHandle == 0 || M_BlockingProviderActors.Contains(ProviderHandle))
	{
		++ProviderHandle;
	}
	M_NextBlockingProviderHandle = ProviderHandle + 1;
	M_BlockingProviderActors.Add(ProviderHandle, WeakProviderActor);
	M_BlockingProviderHandles.Add(WeakProviderActor, ProviderHandle);
	return ProviderHandle;
}

void URTSCoverFinderWorldSubsystem::RebuildCoverSpatialGrid()
{
	M_CoverPointIndices.Reset();
	M_CoverSpatialGrid.Reset();
	for (int32 CoverPointIndex = 0; CoverPointIndex < M_CoverPoints.Num(); ++CoverPointIndex)
	{
		const FRTSCoverPoint& CoverPoint = M_CoverPoints[CoverPointIndex];
		M_CoverPointIndices.Add(CoverPoint.PointId, CoverPointIndex);
		M_CoverSpatialGrid.FindOrAdd(GetCoverSpatialCell(CoverPoint.Location)).Add(CoverPointIndex);
	}
}

void URTSCoverFinderWorldSubsystem::TickTacticalCoverUnits()
{
	SCOPE_CYCLE_COUNTER(STAT_RTSCoverFinder_TacticalUpdates);
	const double StartSeconds = FPlatformTime::Seconds();
	M_TacticalPerformanceSnapshot.UnitUpdatesLastFrame = 0;
	M_TacticalPerformanceSnapshot.CandidateChecksLastFrame = 0;
	M_TacticalPerformanceSnapshot.FiringLaneTracesLastFrame = 0;
	M_TacticalPerformanceSnapshot.FiringLaneRejectionsLastFrame = 0;
	RemoveInvalidTacticalReferences();

	const URTSCoverFinderDeveloperSettings* CoverSettings = GetCoverFinderSettings();
	const UWorld* World = GetWorld();
	// Units cannot move while paused, so deciding now would only reserve points nobody can walk to.
	const bool bWorldIsPaused = IsValid(World) && World->IsPaused();
	if (not IsValid(CoverSettings) || not CoverSettings->bM_EnableAutomaticCoverUse || bWorldIsPaused)
	{
		M_TacticalPerformanceSnapshot.RegisteredUnitCount = M_RegisteredSquadUnits.Num();
		M_TacticalPerformanceSnapshot.ReservedPointCount = M_CoverReservations.Num();
		M_TacticalPerformanceSnapshot.GameThreadMillisecondsLastFrame = 0.0f;
		return;
	}

	const int32 MaximumUpdates = FMath::Clamp(
		CoverSettings->M_MaximumTacticalUnitUpdatesPerFrame,
		1,
		64);
	const int32 UpdatesToPerform = FMath::Min(MaximumUpdates, M_RegisteredSquadUnits.Num());
	while (M_TacticalPerformanceSnapshot.UnitUpdatesLastFrame < UpdatesToPerform &&
		not M_RegisteredSquadUnits.IsEmpty())
	{
		if (M_NextTacticalUnitIndex >= M_RegisteredSquadUnits.Num())
		{
			M_NextTacticalUnitIndex = 0;
		}
		ASquadUnit* SquadUnit = M_RegisteredSquadUnits[M_NextTacticalUnitIndex].Get();
		++M_NextTacticalUnitIndex;
		if (not IsValid(SquadUnit))
		{
			continue;
		}
		SquadUnit->UpdatePlannedMoveArrival();
		SquadUnit->UpdateAutomaticCover(*this);
		++M_TacticalPerformanceSnapshot.UnitUpdatesLastFrame;
	}

	M_TacticalPerformanceSnapshot.RegisteredUnitCount = M_RegisteredSquadUnits.Num();
	M_TacticalPerformanceSnapshot.ReservedPointCount = M_CoverReservations.Num();
	M_TacticalPerformanceSnapshot.GameThreadMillisecondsLastFrame = static_cast<float>(
		(FPlatformTime::Seconds() - StartSeconds) * 1000.0);
	M_TacticalPerformanceSnapshot.MaximumGameThreadMilliseconds = FMath::Max(
		M_TacticalPerformanceSnapshot.MaximumGameThreadMilliseconds,
		M_TacticalPerformanceSnapshot.GameThreadMillisecondsLastFrame);
}

void URTSCoverFinderWorldSubsystem::RemoveInvalidTacticalReferences()
{
	M_RegisteredSquadUnits.RemoveAll([](const TWeakObjectPtr<ASquadUnit>& SquadUnit)
	{
		return not SquadUnit.IsValid();
	});
	for (auto ReservationIterator = M_CoverReservations.CreateIterator(); ReservationIterator; ++ReservationIterator)
	{
		if (not ReservationIterator.Value().IsValid() ||
			not M_CoverPointIndices.Contains(ReservationIterator.Key()))
		{
			ReservationIterator.RemoveCurrent();
		}
	}
	const UWorld* World = GetWorld();
	const float WorldSeconds = IsValid(World) ? World->GetTimeSeconds() : 0.0f;
	for (auto UnreachableIterator = M_UnreachableCoverPointExpiry.CreateIterator(); UnreachableIterator;
	     ++UnreachableIterator)
	{
		if (WorldSeconds >= UnreachableIterator.Value())
		{
			UnreachableIterator.RemoveCurrent();
		}
	}
	for (auto ProviderIterator = M_BlockingProviderHandles.CreateIterator(); ProviderIterator; ++ProviderIterator)
	{
		if (ProviderIterator.Key().IsValid())
		{
			continue;
		}
		M_BlockingProviderActors.Remove(ProviderIterator.Value());
		ProviderIterator.RemoveCurrent();
	}
	M_NextTacticalUnitIndex = FMath::Min(M_NextTacticalUnitIndex, M_RegisteredSquadUnits.Num());
}

FIntPoint URTSCoverFinderWorldSubsystem::GetCoverSpatialCell(const FVector& Location) const
{
	return FIntPoint(
		FMath::FloorToInt(Location.X / CoverFinderWorldSubsystemPrivate::CoverSpatialCellSize),
		FMath::FloorToInt(Location.Y / CoverFinderWorldSubsystemPrivate::CoverSpatialCellSize));
}

FVector URTSCoverFinderWorldSubsystem::BuildFiringLaneStart(
	const ASquadUnit& SquadUnit,
	const FRTSCoverPoint& CoverPoint) const
{
	if (CoverPoint.CoverType == ERTSCoverType::Crouch)
	{
		return CoverPoint.Location + FVector::UpVector * CoverFinderWorldSubsystemPrivate::CrouchFiringHeight;
	}
	// Tested from where this unit's expose animation ends, so an open lane means the real muzzle is clear.
	const FVector ExposedOffset = SquadUnit.GetStandingCoverExposedWorldOffset(
		CoverPoint,
		GetStandingPeekOffset(CoverPoint));
	return CoverPoint.Location + ExposedOffset
		+ FVector::UpVector * CoverFinderWorldSubsystemPrivate::StandingFiringHeight;
}

bool URTSCoverFinderWorldSubsystem::GetIsCandidateProtectedFromTarget(
	const FRTSCoverPoint& CoverPoint,
	const FVector& TargetLocation) const
{
	// The cover aim offsets decide how far to the side a target may be before the point is useless against it.
	return FRTSCombatCoverScoring::GetCanAimAt(CoverPoint, TargetLocation, BuildCombatCoverSettings());
}

bool URTSCoverFinderWorldSubsystem::GetIsPointReservedByAnotherUnit(
	const int64 PointId,
	const ASquadUnit& SquadUnit) const
{
	const TWeakObjectPtr<ASquadUnit>* ReservingUnit = M_CoverReservations.Find(PointId);
	return ReservingUnit != nullptr && ReservingUnit->IsValid() && ReservingUnit->Get() != &SquadUnit;
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
	UE_LOG(
		LogRTSCoverFinder,
		Display,
		TEXT("RTS_COVER_TACTICAL_PERF registered=%d reserved=%d updates_frame=%d candidate_checks_frame=%d lane_traces_frame=%d gt_frame_ms=%.3f gt_max_ms=%.3f"),
		M_TacticalPerformanceSnapshot.RegisteredUnitCount,
		M_TacticalPerformanceSnapshot.ReservedPointCount,
		M_TacticalPerformanceSnapshot.UnitUpdatesLastFrame,
		M_TacticalPerformanceSnapshot.CandidateChecksLastFrame,
		M_TacticalPerformanceSnapshot.FiringLaneTracesLastFrame,
		M_TacticalPerformanceSnapshot.GameThreadMillisecondsLastFrame,
		M_TacticalPerformanceSnapshot.MaximumGameThreadMilliseconds);
}

void URTSCoverFinderWorldSubsystem::RequestDebugCapture(const FString& ScreenshotName)
{
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
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
}

void URTSCoverFinderWorldSubsystem::RequestTestCoverValidation(
	const float DelaySeconds,
	const bool bCaptureScreenshot)
{
	// The scenario unpauses the game and orders squads around, so it only exists in cover-debug builds.
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		M_TestCoverScenario.Start(DelaySeconds, bCaptureScreenshot);
	}
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

	++M_EnvironmentScanIndex;
	const float HorizontalExpansion = M_ActiveSettings.MaximumCoverSearchDistance
		+ M_ActiveSettings.AgentRadius;
	for (const FOverlapResult& EnvironmentOverlap : EnvironmentOverlaps)
	{
		if (not GetIsScannableEnvironmentComponent(EnvironmentOverlap.GetComponent()))
		{
			continue;
		}
		const FBox CollisionBounds = GetOverlapCollisionBounds(EnvironmentOverlap);
		const FVector FootprintSize = CollisionBounds.GetSize();
		const bool bIsLargeObject = FootprintSize.X * FootprintSize.Y >
			CoverFinderWorldSubsystemPrivate::LargeObjectFootprintSquareCentimeters;
		if (bIsLargeObject)
		{
			AppendLargeObjectScanBounds(
				CollisionBounds.ExpandBy(FVector(HorizontalExpansion, HorizontalExpansion, 0.0f)),
				OutEnvironmentBounds);
			if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
			{
				UE_LOG(
					LogRTSCoverFinder,
					Verbose,
					TEXT("RTS_COVER_LARGE_SCAN_OBJECT actor=%s component=%s footprint_m=(%.0f x %.0f): sampled only on the navigation tiles it overlaps"),
					*GetNameSafe(EnvironmentOverlap.GetActor()),
					*GetNameSafe(EnvironmentOverlap.GetComponent()),
					FootprintSize.X / 100.0f,
					FootprintSize.Y / 100.0f);
			}
			continue;
		}
		CoverFinderWorldSubsystemPrivate::AppendEnvironmentScanBounds(
			CollisionBounds,
			HorizontalExpansion,
			OutEnvironmentBounds);
		if (M_ActiveSettings.bProbeThinObstacles)
		{
			PlanThinObstacleProbing(EnvironmentOverlap, CollisionBounds);
		}
	}
	RemoveUnseenThinObstacleCaches();
	return not OutEnvironmentBounds.IsEmpty();
}

void URTSCoverFinderWorldSubsystem::AppendLargeObjectScanBounds(
	const FBox& ExpandedObjectBounds,
	TArray<FBox>& OutEnvironmentBounds) const
{
	for (const FBox& NavigationTileBounds : M_CachedNavigationTileBounds)
	{
		if (not NavigationTileBounds.IntersectXY(ExpandedObjectBounds))
		{
			continue;
		}
		// The tile keeps its own height: samples are projected onto the navmesh from the middle of the tile,
		// the same way the landscape scan does it, wherever the large object's own floor may be.
		FBox ClippedTileBounds = NavigationTileBounds;
		ClippedTileBounds.Min.X = FMath::Max(ClippedTileBounds.Min.X, ExpandedObjectBounds.Min.X);
		ClippedTileBounds.Min.Y = FMath::Max(ClippedTileBounds.Min.Y, ExpandedObjectBounds.Min.Y);
		ClippedTileBounds.Max.X = FMath::Min(ClippedTileBounds.Max.X, ExpandedObjectBounds.Max.X);
		ClippedTileBounds.Max.Y = FMath::Min(ClippedTileBounds.Max.Y, ExpandedObjectBounds.Max.Y);
		OutEnvironmentBounds.Add(ClippedTileBounds);
	}
}

void URTSCoverFinderWorldSubsystem::PlanThinObstacleProbing(
	const FOverlapResult& Overlap,
	const FBox& CollisionBounds)
{
	const bool bIsThinByCollision = FCoverFinderAlgorithms::GetIsThinObstacle(CollisionBounds, M_ActiveSettings);
	const FVector CollisionSize = CollisionBounds.GetSize();
	// Instances share one component, so there is no pivot per instance to measure a trunk around.
	const bool bMayHaveTrunk = Overlap.ItemIndex == INDEX_NONE &&
		CollisionSize.Z >= M_ActiveSettings.MinimumCrouchCoverHeight &&
		FMath::Max(CollisionSize.X, CollisionSize.Y) <=
		CoverFinderWorldSubsystemPrivate::MaximumObjectWidthForTrunkMeasurement;
	if (not bIsThinByCollision && not bMayHaveTrunk)
	{
		return;
	}
	const FCoverObstacleKey ObstacleKey{Overlap.GetComponent(), Overlap.ItemIndex};
	FCoverThinObstacleCache& ObstacleCache = FindOrAddThinObstacleCache(ObstacleKey, CollisionBounds);
	ObstacleCache.LastSeenScanIndex = M_EnvironmentScanIndex;
	if (not bIsThinByCollision && not ObstacleCache.bHasMeasuredTrunk)
	{
		// Measured inside the scan's frame budget; the ring follows in the same scan.
		M_PendingTrunkMeasurements.Add({ObstacleKey, CollisionBounds});
		return;
	}
	const FBox RingBounds = bIsThinByCollision ? CollisionBounds : ObstacleCache.TrunkBounds;
	const bool bHasRing = (bIsThinByCollision || ObstacleCache.bFoundTrunk) &&
		FCoverFinderAlgorithms::GetIsThinObstacle(RingBounds, M_ActiveSettings);
	if (not bHasRing)
	{
		ObstacleCache.CoverRadius = 0.0f;
		return;
	}
	// The cache id staggers the refreshes, so the obstacles of a map take turns instead of all coming due at once.
	const int32 RefreshScanCount = FMath::Max(1, M_ActiveSettings.ThinObstacleRefreshScanCount);
	const bool bRingIsDue = ObstacleCache.LastRingProbeScanIndex == INDEX_NONE ||
		(M_EnvironmentScanIndex + static_cast<int32>(ObstacleCache.CacheId)) % RefreshScanCount == 0;
	if (bRingIsDue && M_PendingThinObstacleSamples.Num() < RTSCoverFinderConstants::MaxFocusedSamples)
	{
		ScheduleRingProbe(ObstacleCache, RingBounds, M_PendingThinObstacleSamples);
		return;
	}
	// The designer may have changed the width per soldier since the ring was probed.
	ObstacleCache.SoldierCapacity = FCoverFinderAlgorithms::GetThinObstacleSoldierCapacity(RingBounds, M_ActiveSettings);
	M_PendingReusedRingCandidates.Append(ObstacleCache.RingCandidates);
	++M_PendingReusedRingObstacleCount;
}

void URTSCoverFinderWorldSubsystem::TagThinObstacleCoverPoints()
{
	constexpr int32 MaximumCapacityOnPoint = 255;
	// Obstacles are looked up through a coarse grid so a forest does not compare every point with every tree.
	TMap<FIntPoint, TArray<const FCoverThinObstacleCache*>> ObstaclesPerCell;
	for (const TPair<FCoverObstacleKey, FCoverThinObstacleCache>& CacheEntry : M_ThinObstacleCaches)
	{
		if (CacheEntry.Value.CoverRadius > 0.0f)
		{
			ObstaclesPerCell.FindOrAdd(GetCoverSpatialCell(CacheEntry.Value.RingCenter)).Add(&CacheEntry.Value);
		}
	}
	if (ObstaclesPerCell.IsEmpty())
	{
		return;
	}
	for (FRTSCoverPoint& CoverPoint : M_CoverPoints)
	{
		// Authored points are placed by a designer and keep whatever capacity the designer intended: none.
		if (CoverPoint.ProviderRegistrationId != 0)
		{
			continue;
		}
		const FIntPoint PointCell = GetCoverSpatialCell(CoverPoint.Location);
		float NearestDistanceSquared = TNumericLimits<float>::Max();
		for (int32 CellOffsetX = -1; CellOffsetX <= 1; ++CellOffsetX)
		{
			for (int32 CellOffsetY = -1; CellOffsetY <= 1; ++CellOffsetY)
			{
				const TArray<const FCoverThinObstacleCache*>* CellObstacles =
					ObstaclesPerCell.Find(PointCell + FIntPoint(CellOffsetX, CellOffsetY));
				if (CellObstacles == nullptr)
				{
					continue;
				}
				for (const FCoverThinObstacleCache* Obstacle : *CellObstacles)
				{
					const float DistanceSquared = FVector::DistSquared2D(CoverPoint.Location, Obstacle->RingCenter);
					if (DistanceSquared > FMath::Square(Obstacle->CoverRadius) || DistanceSquared >= NearestDistanceSquared)
					{
						continue;
					}
					NearestDistanceSquared = DistanceSquared;
					CoverPoint.ThinObstacleId = Obstacle->CacheId;
					CoverPoint.ThinObstacleCapacity = static_cast<uint8>(
						FMath::Clamp(Obstacle->SoldierCapacity, 1, MaximumCapacityOnPoint));
				}
			}
		}
	}
}

int32 URTSCoverFinderWorldSubsystem::GetThinObstacleReservationCount(
	const uint32 ThinObstacleId,
	const ASquadUnit* IgnoredUnit,
	const TSet<const ASquadUnit*>* IgnoredUnits) const
{
	if (ThinObstacleId == 0)
	{
		return 0;
	}
	int32 ReservationCount = 0;
	for (const TPair<int64, TWeakObjectPtr<ASquadUnit>>& Reservation : M_CoverReservations)
	{
		const ASquadUnit* ReservingUnit = Reservation.Value.Get();
		const int32* CoverPointIndex = M_CoverPointIndices.Find(Reservation.Key);
		const bool bCounts = IsValid(ReservingUnit) && ReservingUnit != IgnoredUnit &&
			(IgnoredUnits == nullptr || not IgnoredUnits->Contains(ReservingUnit)) &&
			CoverPointIndex != nullptr && M_CoverPoints.IsValidIndex(*CoverPointIndex) &&
			M_CoverPoints[*CoverPointIndex].ThinObstacleId == ThinObstacleId;
		ReservationCount += bCounts ? 1 : 0;
	}
	return ReservationCount;
}

int32 URTSCoverFinderWorldSubsystem::GetOverCapacityThinObstacleCount() const
{
	TMap<uint32, int32> ReservationsPerObstacle;
	TMap<uint32, int32> CapacityPerObstacle;
	for (const TPair<int64, TWeakObjectPtr<ASquadUnit>>& Reservation : M_CoverReservations)
	{
		const int32* CoverPointIndex = M_CoverPointIndices.Find(Reservation.Key);
		if (not Reservation.Value.IsValid() || CoverPointIndex == nullptr || not M_CoverPoints.IsValidIndex(*CoverPointIndex))
		{
			continue;
		}
		const FRTSCoverPoint& CoverPoint = M_CoverPoints[*CoverPointIndex];
		if (CoverPoint.ThinObstacleId == 0)
		{
			continue;
		}
		++ReservationsPerObstacle.FindOrAdd(CoverPoint.ThinObstacleId);
		CapacityPerObstacle.Add(CoverPoint.ThinObstacleId, CoverPoint.ThinObstacleCapacity);
	}
	int32 OverCapacityCount = 0;
	for (const TPair<uint32, int32>& ObstacleReservations : ReservationsPerObstacle)
	{
		OverCapacityCount += ObstacleReservations.Value > CapacityPerObstacle.FindRef(ObstacleReservations.Key) ? 1 : 0;
	}
	return OverCapacityCount;
}

FCoverThinObstacleCache& URTSCoverFinderWorldSubsystem::FindOrAddThinObstacleCache(
	const FCoverObstacleKey& Key,
	const FBox& CollisionBounds)
{
	FCoverThinObstacleCache& ObstacleCache = M_ThinObstacleCaches.FindOrAdd(Key);
	if (ObstacleCache.CacheId == 0)
	{
		ObstacleCache.CacheId = M_NextObstacleCacheId++;
		ObstacleCache.BoundsCenter = CollisionBounds.GetCenter();
		M_ObstacleKeysByCacheId.Add(ObstacleCache.CacheId, Key);
		return ObstacleCache;
	}
	const bool bHasMoved = FVector::DistSquared(ObstacleCache.BoundsCenter, CollisionBounds.GetCenter()) >
		FMath::Square(CoverFinderWorldSubsystemPrivate::TrunkRemeasureDistance);
	if (bHasMoved)
	{
		ObstacleCache.BoundsCenter = CollisionBounds.GetCenter();
		ObstacleCache.RingCandidates.Reset();
		ObstacleCache.LastRingProbeScanIndex = INDEX_NONE;
		ObstacleCache.bHasMeasuredTrunk = false;
		ObstacleCache.bFoundTrunk = false;
	}
	return ObstacleCache;
}

void URTSCoverFinderWorldSubsystem::ScheduleRingProbe(
	FCoverThinObstacleCache& InOutCache,
	const FBox& RingBounds,
	TArray<FCoverFocusedSample>& OutSamples) const
{
	InOutCache.RingCandidates.Reset();
	InOutCache.LastRingProbeScanIndex = M_EnvironmentScanIndex;
	InOutCache.RingCenter = RingBounds.GetCenter();
	InOutCache.CoverRadius = FCoverFinderAlgorithms::GetThinObstacleCoverRadius(RingBounds, M_ActiveSettings);
	InOutCache.SoldierCapacity = FCoverFinderAlgorithms::GetThinObstacleSoldierCapacity(RingBounds, M_ActiveSettings);
	const int32 FirstRingSampleIndex = OutSamples.Num();
	FCoverFinderAlgorithms::AppendThinObstacleSamples(RingBounds, M_ActiveSettings, OutSamples);
	for (int32 SampleIndex = FirstRingSampleIndex; SampleIndex < OutSamples.Num(); ++SampleIndex)
	{
		OutSamples[SampleIndex].ObstacleCacheId = InOutCache.CacheId;
	}
}

void URTSCoverFinderWorldSubsystem::ProcessNextTrunkMeasurement(int32& InOutFrameWorldQueries)
{
	// One navmesh projection for the ground plus the rays.
	constexpr int32 QueriesPerMeasurement = CoverFinderWorldSubsystemPrivate::TrunkMeasurementRayCount + 1;
	const FCoverPendingTrunkMeasurement PendingMeasurement =
		M_SamplingState.PendingTrunkMeasurements.Pop(EAllowShrinking::No);
	FCoverThinObstacleCache* ObstacleCache = M_ThinObstacleCaches.Find(PendingMeasurement.Key);
	const UPrimitiveComponent* PrimitiveComponent = PendingMeasurement.Key.Component.Get();
	if (ObstacleCache == nullptr || not IsValid(PrimitiveComponent))
	{
		return;
	}
	InOutFrameWorldQueries += QueriesPerMeasurement;
	M_PerformanceAccumulator.WorldQueryCount += QueriesPerMeasurement;
	++M_SamplingState.MeasuredTrunkCount;
	ObstacleCache->bHasMeasuredTrunk = true;
	ObstacleCache->bFoundTrunk = TryMeasureTrunk(
		*PrimitiveComponent,
		PendingMeasurement.CollisionBounds,
		ObstacleCache->TrunkBounds);
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		UE_LOG(
			LogRTSCoverFinder,
			Verbose,
			TEXT("RTS_COVER_TRUNK_MEASURED actor=%s found=%d trunk_cm=(%.0f x %.0f) collision_cm=(%.0f x %.0f) at=%s"),
			*GetNameSafe(PrimitiveComponent->GetOwner()),
			ObstacleCache->bFoundTrunk ? 1 : 0,
			ObstacleCache->bFoundTrunk ? ObstacleCache->TrunkBounds.GetSize().X : 0.0f,
			ObstacleCache->bFoundTrunk ? ObstacleCache->TrunkBounds.GetSize().Y : 0.0f,
			PendingMeasurement.CollisionBounds.GetSize().X,
			PendingMeasurement.CollisionBounds.GetSize().Y,
			*PrimitiveComponent->GetComponentLocation().ToString());
	}
	const bool bMayProbeRing = ObstacleCache->bFoundTrunk &&
		FCoverFinderAlgorithms::GetIsThinObstacle(ObstacleCache->TrunkBounds, M_ActiveSettings) &&
		M_SamplingState.FocusedSamples.Num() < RTSCoverFinderConstants::MaxFocusedSamples;
	if (bMayProbeRing)
	{
		ScheduleRingProbe(*ObstacleCache, ObstacleCache->TrunkBounds, M_SamplingState.FocusedSamples);
	}
}

void URTSCoverFinderWorldSubsystem::StoreRingCandidates(const FCoverProbeObservation& Observation)
{
	const FCoverObstacleKey* ObstacleKey = M_ObstacleKeysByCacheId.Find(M_SamplingState.CurrentObstacleCacheId);
	FCoverThinObstacleCache* ObstacleCache = ObstacleKey != nullptr ? M_ThinObstacleCaches.Find(*ObstacleKey) : nullptr;
	if (ObstacleCache == nullptr)
	{
		return;
	}
	// Classified here as well as on the worker: the worker's result is merged into one published list, while
	// the cache needs to know which candidates belong to this obstacle.
	TArray<FCoverProbeObservation> SingleObservation;
	SingleObservation.Add(Observation);
	FCoverFinderAlgorithms::AppendClassifiedCandidates(
		SingleObservation,
		M_ActiveSettings,
		ObstacleCache->RingCandidates);
}

void URTSCoverFinderWorldSubsystem::RemoveUnseenThinObstacleCaches()
{
	for (auto CacheIterator = M_ThinObstacleCaches.CreateIterator(); CacheIterator; ++CacheIterator)
	{
		// Destroyed, or no longer overlapping the navigable world: its cover must not be reused.
		if (CacheIterator.Value().LastSeenScanIndex == M_EnvironmentScanIndex)
		{
			continue;
		}
		M_ObstacleKeysByCacheId.Remove(CacheIterator.Value().CacheId);
		CacheIterator.RemoveCurrent();
	}
}

bool URTSCoverFinderWorldSubsystem::TryMeasureTrunk(
	const UPrimitiveComponent& PrimitiveComponent,
	const FBox& CollisionBounds,
	FBox& OutTrunkBounds) const
{
	using namespace CoverFinderWorldSubsystemPrivate;
	const FVector Pivot = PrimitiveComponent.GetComponentLocation();
	const UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	const ANavigationData* NavigationData = GetCharacterNavigationData();
	if (not IsValid(NavigationSystem) || not IsValid(NavigationData) || not CollisionBounds.IsInsideXY(Pivot))
	{
		return false;
	}
	// The trunk itself is not walkable; the navmesh next to it tells how high the ground is there.
	FNavLocation GroundNextToPivot;
	if (not NavigationSystem->ProjectPointToNavigation(
		Pivot,
		GroundNextToPivot,
		FVector(TrunkGroundSearchExtent, TrunkGroundSearchExtent, CollisionBounds.GetSize().Z + M_ActiveSettings.AgentHeight),
		NavigationData))
	{
		return false;
	}
	const float ProbeHeight = GroundNextToPivot.Location.Z + M_ActiveSettings.MinimumCrouchCoverHeight;
	const FVector RayEnd(Pivot.X, Pivot.Y, ProbeHeight);
	// Longer than the object is wide, so every ray starts outside it wherever the pivot sits.
	const float RayLength = FVector2D(CollisionBounds.GetSize().X, CollisionBounds.GetSize().Y).Size();
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RTSCoverTrunk), true);
	FBox TrunkSlice(ForceInit);
	int32 HitCount = 0;
	for (int32 RayIndex = 0; RayIndex < TrunkMeasurementRayCount; ++RayIndex)
	{
		const float AngleRadians = FMath::DegreesToRadians(
			FullCircleDegrees * static_cast<float>(RayIndex) / static_cast<float>(TrunkMeasurementRayCount));
		const FVector RayStart = RayEnd + FVector(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f) * RayLength;
		FHitResult TrunkHit;
		// Non-const in the engine although a trace changes nothing about the component.
		if (const_cast<UPrimitiveComponent&>(PrimitiveComponent).LineTraceComponent(TrunkHit, RayStart, RayEnd, QueryParams))
		{
			TrunkSlice += TrunkHit.ImpactPoint;
			++HitCount;
		}
	}
	if (HitCount < MinimumTrunkMeasurementHits)
	{
		return false;
	}
	OutTrunkBounds = FBox(
		FVector(TrunkSlice.Min.X, TrunkSlice.Min.Y, GroundNextToPivot.Location.Z),
		FVector(TrunkSlice.Max.X, TrunkSlice.Max.Y, CollisionBounds.Max.Z));
	return true;
}

bool URTSCoverFinderWorldSubsystem::GetIsScannableEnvironmentComponent(const UPrimitiveComponent* PrimitiveComponent)
{
	if (not IsValid(PrimitiveComponent) ||
		CoverFinderWorldSubsystemPrivate::GetIsLandscapeComponent(PrimitiveComponent))
	{
		return false;
	}
	return not CoverFinderWorldSubsystemPrivate::GetIsInfantryActor(PrimitiveComponent->GetOwner());
}

FBox URTSCoverFinderWorldSubsystem::GetOverlapCollisionBounds(const FOverlapResult& Overlap)
{
	const UPrimitiveComponent* PrimitiveComponent = Overlap.GetComponent();
	if (not IsValid(PrimitiveComponent))
	{
		return FBox(ForceInit);
	}
	// Foliage and other instanced meshes are one component; each instance has a body of its own.
	const UInstancedStaticMeshComponent* InstancedComponent = Cast<UInstancedStaticMeshComponent>(PrimitiveComponent);
	const bool bHasInstanceBody = IsValid(InstancedComponent) &&
		InstancedComponent->InstanceBodies.IsValidIndex(Overlap.ItemIndex) &&
		InstancedComponent->InstanceBodies[Overlap.ItemIndex] != nullptr;
	const FBodyInstance* CollisionBody = bHasInstanceBody
		? InstancedComponent->InstanceBodies[Overlap.ItemIndex]
		: PrimitiveComponent->GetBodyInstance();
	if (CollisionBody != nullptr && CollisionBody->IsValidBodyInstance())
	{
		const FBox BodyBounds = CollisionBody->GetBodyBounds();
		if (BodyBounds.IsValid != 0)
		{
			return BodyBounds;
		}
	}
	return PrimitiveComponent->Bounds.GetBox();
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
	Snapshot.StandingPeekEdgeInset = FMath::Clamp(CoverSettings->M_StandingPeekEdgeInset, 0.0f, 80.0f);
	Snapshot.CoverPointSpacing = FMath::Clamp(CoverSettings->M_CoverPointSpacing, 50.0f, 250.0f);
	Snapshot.GameThreadBudgetMilliseconds = FMath::Max(
		0.05f,
		bM_EnvironmentScanComplete
			? CoverSettings->M_GameThreadBudgetMilliseconds
			: CoverSettings->M_FirstScanGameThreadBudgetMilliseconds);
	Snapshot.StandingSpaceRadius = FMath::Clamp(CoverSettings->M_StandingSpaceRadius, 10.0f, 60.0f);
	Snapshot.StandingSpaceHeight = FMath::Clamp(CoverSettings->M_StandingSpaceHeight, 60.0f, 200.0f);
	Snapshot.StandingSpaceFloorClearance = FMath::Clamp(CoverSettings->M_StandingSpaceFloorClearance, 0.0f, 90.0f);
	Snapshot.ThinObstacleMaximumWidth = FMath::Clamp(CoverSettings->M_ThinObstacleMaximumWidth, 20.0f, 400.0f);
	Snapshot.ThinObstacleRingSampleCount = FMath::Clamp(CoverSettings->M_ThinObstacleRingSamples, 4, 16);
	Snapshot.ThinObstacleRefreshScanCount = FMath::Clamp(CoverSettings->M_ThinObstacleRefreshScans, 1, 60);
	Snapshot.ThinObstacleWidthPerSoldier = FMath::Clamp(CoverSettings->M_ThinObstacleWidthPerSoldier, 20.0f, 400.0f);
	Snapshot.bProbeThinObstacles = CoverSettings->bM_ProbeThinObstacles;
	Snapshot.bReaimSlantedHits = CoverSettings->bM_ReaimSlantedHits;
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
	M_SamplingState.FocusedSamples = MoveTemp(M_PendingThinObstacleSamples);
	M_SamplingState.PendingTrunkMeasurements = MoveTemp(M_PendingTrunkMeasurements);
	M_SamplingState.ReusedRingObstacleCount = M_PendingReusedRingObstacleCount;
	M_PendingThinObstacleSamples.Reset();
	M_PendingTrunkMeasurements.Reset();
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
	// After the plan request, which starts the worker's generation: cover of rings that are not probed this scan.
	if (not M_PendingReusedRingCandidates.IsEmpty())
	{
		M_Worker->EnqueueCandidateChunk(M_ActiveGeneration, MoveTemp(M_PendingReusedRingCandidates));
	}
	M_PendingReusedRingCandidates.Reset();
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
	M_PendingThinObstacleSamples.Reset();
	M_PendingReusedRingCandidates.Reset();
	M_PendingTrunkMeasurements.Reset();
	M_PendingReusedRingObstacleCount = 0;
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
	bM_EnvironmentScanComplete = true;
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
	while (M_SamplingState.GetHasWorkLeft())
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

		const bool bMayMeasureTrunk = not M_SamplingState.bHasCurrentObservation &&
			not M_SamplingState.PendingTrunkMeasurements.IsEmpty();
		if (bMayMeasureTrunk)
		{
			ProcessNextTrunkMeasurement(FrameWorldQueries);
			bPerformedQuery = true;
			continue;
		}

		if (not M_SamplingState.bHasCurrentObservation)
		{
			++FrameWorldQueries;
			++M_PerformanceAccumulator.WorldQueryCount;
			bPerformedQuery = true;
			StartNextObservation(*NavigationSystem, *NavigationData, FrameWorldQueries);
			continue;
		}

		if (M_SamplingState.CurrentDirectionIndex >= GetCurrentSampleDirectionCount())
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
	if (not M_SamplingState.GetHasWorkLeft())
	{
		FinishWorldSampling();
	}
}

bool URTSCoverFinderWorldSubsystem::GetNextPlannedSample(FCoverFocusedSample& OutSample) const
{
	const int32 GridSampleCount = M_SamplingState.SampleLocations.Num();
	if (M_SamplingState.NextSampleIndex < GridSampleCount)
	{
		OutSample = FCoverFocusedSample();
		OutSample.Location = M_SamplingState.SampleLocations[M_SamplingState.NextSampleIndex];
		return false;
	}
	OutSample = M_SamplingState.FocusedSamples[M_SamplingState.NextSampleIndex - GridSampleCount];
	return true;
}

int32 URTSCoverFinderWorldSubsystem::GetCurrentSampleDirectionCount() const
{
	return M_SamplingState.GetIsSamplingFocusedSample() ? 1 : RTSCoverFinderConstants::SearchDirectionCount;
}

FVector URTSCoverFinderWorldSubsystem::GetCurrentSampleDirection() const
{
	return M_SamplingState.GetIsSamplingFocusedSample()
		? M_SamplingState.CurrentFocusDirection
		: CoverFinderWorldSubsystemPrivate::GetSearchDirection(M_SamplingState.CurrentDirectionIndex);
}

bool URTSCoverFinderWorldSubsystem::StartNextObservation(
	const UNavigationSystemV1& NavigationSystem,
	const ANavigationData& NavigationData,
	int32& InOutFrameWorldQueries)
{
	if (M_SamplingState.NextSampleIndex < 0 || M_SamplingState.NextSampleIndex >= M_SamplingState.GetSampleCount())
	{
		return false;
	}

	FCoverFocusedSample PlannedSample;
	const bool bIsFocusedSample = GetNextPlannedSample(PlannedSample);
	// A grid position may slide to the nearest navmesh; an aimed probe must stay where it was aimed from.
	const float HorizontalProjectionExtent = bIsFocusedSample
		? M_ActiveSettings.AgentRadius
		: M_ActiveSettings.SearchGridSpacing * 0.45f;
	const FVector ProjectionExtent(
		HorizontalProjectionExtent,
		HorizontalProjectionExtent,
		M_ActiveSettings.AgentHeight);
	FNavLocation ProjectedLocation;
	if (not NavigationSystem.ProjectPointToNavigation(
		PlannedSample.Location,
		ProjectedLocation,
		ProjectionExtent,
		&NavigationData))
	{
		++M_SamplingState.NextSampleIndex;
		return false;
	}
	const FVector FocusDirection = bIsFocusedSample
		? (PlannedSample.AimLocation - ProjectedLocation.Location).GetSafeNormal2D()
		: FVector::ZeroVector;
	const bool bHasNothingToAimAt = bIsFocusedSample && FocusDirection.IsNearlyZero();
	if (bHasNothingToAimAt || not GetCanInfantryOccupyLocation(ProjectedLocation.Location, InOutFrameWorldQueries))
	{
		++M_SamplingState.NextSampleIndex;
		return false;
	}

	M_SamplingState.CurrentObservation = FCoverProbeObservation();
	M_SamplingState.CurrentObservation.ProjectedLocation = ProjectedLocation.Location;
	M_SamplingState.CurrentObservation.DirectionalObservations.Reserve(
		bIsFocusedSample ? 1 : RTSCoverFinderConstants::SearchDirectionCount);
	M_SamplingState.CurrentFocusDirection = FocusDirection;
	M_SamplingState.CurrentObstacleCacheId = PlannedSample.ObstacleCacheId;
	M_SamplingState.CurrentDirectionIndex = 0;
	M_SamplingState.bHasCurrentObservation = true;
	++M_PerformanceAccumulator.ProjectedSampleCount;
	return true;
}

void URTSCoverFinderWorldSubsystem::QueueReaimedSample(
	const FVector& SampleLocation,
	const FCoverDirectionalObservation& DirectionObservation)
{
	FCoverFocusedSample ReaimedSample;
	if (M_SamplingState.FocusedSamples.Num() >= RTSCoverFinderConstants::MaxFocusedSamples ||
		not FCoverFinderAlgorithms::TryBuildReaimedSample(
			SampleLocation,
			DirectionObservation.SearchDirection,
			DirectionObservation.CrouchTrace,
			M_ActiveSettings,
			ReaimedSample))
	{
		return;
	}
	const FVector ReaimDirection = (ReaimedSample.AimLocation - ReaimedSample.Location).GetSafeNormal2D();
	const FIntVector ReaimKey(
		FMath::RoundToInt32(ReaimedSample.Location.X / CoverFinderWorldSubsystemPrivate::ReaimDeduplicationCellSize),
		FMath::RoundToInt32(ReaimedSample.Location.Y / CoverFinderWorldSubsystemPrivate::ReaimDeduplicationCellSize),
		FMath::RoundToInt32(ReaimDirection.Rotation().Yaw /
			CoverFinderWorldSubsystemPrivate::ReaimDeduplicationYawStepDegrees));
	bool bAlreadyQueued = false;
	M_SamplingState.QueuedReaimKeys.Add(ReaimKey, &bAlreadyQueued);
	if (not bAlreadyQueued)
	{
		M_SamplingState.FocusedSamples.Add(ReaimedSample);
		++M_SamplingState.ReaimedSampleCount;
	}
}

void URTSCoverFinderWorldSubsystem::SampleCurrentDirection(
	const UNavigationSystemV1& NavigationSystem,
	const ANavigationData& NavigationData,
	int32& InOutFrameWorldQueries)
{
	FCoverDirectionalObservation DirectionObservation;
	DirectionObservation.SearchDirection = GetCurrentSampleDirection();
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

	// Only grid probes ask for a second look, and only at objects: the result of that look is final.
	const bool bMayReaim = M_ActiveSettings.bReaimSlantedHits &&
		M_ActiveScanDomain == ECoverFinderScanDomain::Environment &&
		not M_SamplingState.GetIsSamplingFocusedSample();
	if (bMayReaim)
	{
		QueueReaimedSample(ProtectedLocation, DirectionObservation);
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
	if (FaceAlignment < RTSCoverFinderConstants::MinimumStandingFaceAlignment)
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
	if (M_SamplingState.CurrentObstacleCacheId != 0)
	{
		StoreRingCandidates(M_SamplingState.CurrentObservation);
		M_SamplingState.CurrentObstacleCacheId = 0;
	}
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
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		UE_LOG(
			LogRTSCoverFinder,
			Display,
			TEXT("RTS_COVER_AIMED_PROBES generation=%llu grid_samples=%d thin_obstacle_probes=%d reaimed_probes=%d thin_obstacles_reused=%d trunks_measured=%d thin_obstacles_known=%d"),
			M_ActiveGeneration,
			M_SamplingState.SampleLocations.Num(),
			M_SamplingState.FocusedSamples.Num() - M_SamplingState.ReaimedSampleCount,
			M_SamplingState.ReaimedSampleCount,
			M_SamplingState.ReusedRingObstacleCount,
			M_SamplingState.MeasuredTrunkCount,
			M_ThinObstacleCaches.Num());
	}
	M_Worker->EnqueueFinalizeRequest(M_ActiveGeneration);
	M_ScanState = ECoverFinderScanState::WaitingForWorker;
	M_SamplingState.SampleLocations.Reset();
	M_SamplingState.FocusedSamples.Reset();
	M_SamplingState.QueuedReaimKeys.Reset();
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

	// The sampled opening is only accurate to one search step; expose animations step a fixed distance, so the
	// point is anchored to the real edge instead of to wherever the sample happened to land.
	const float EdgeOffset = RefineStandingEdgeOffset(
		ProtectedLocation,
		SearchDirection,
		SideDirection,
		LastCoveredOffset,
		FirstOpenOffset,
		InOutFrameWorldQueries);
	const float CoverPointOffset = FMath::Max(0.0f, EdgeOffset - M_ActiveSettings.StandingPeekEdgeInset);
	const FVector RequestedCoverLocation = ProtectedLocation + SideDirection * CoverPointOffset;
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

float URTSCoverFinderWorldSubsystem::RefineStandingEdgeOffset(
	const FVector& ProtectedLocation,
	const FVector& SearchDirection,
	const FVector& SideDirection,
	const float LastCoveredOffset,
	const float FirstOpenOffset,
	int32& InOutFrameWorldQueries)
{
	float CoveredOffset = LastCoveredOffset;
	float OpenOffset = FirstOpenOffset;
	for (int32 RefinementStep = 0;
		RefinementStep < CoverFinderWorldSubsystemPrivate::StandingEdgeRefinementSteps;
		++RefinementStep)
	{
		const float MiddleOffset = (CoveredOffset + OpenOffset) * 0.5f;
		const FCoverTraceObservation StandingTrace = TraceCoverHeight(
			ProtectedLocation + SideDirection * MiddleOffset,
			SearchDirection,
			RTSCoverFinderConstants::StandingCoverHeight,
			InOutFrameWorldQueries);
		if (StandingTrace.bBlockingHit)
		{
			CoveredOffset = MiddleOffset;
			continue;
		}
		OpenOffset = MiddleOffset;
	}
	return CoveredOffset;
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

	FVector CapsuleCenter = FVector::ZeroVector;
	const FCollisionShape CapsuleShape = BuildStandingSpaceShape(GroundLocation, CapsuleCenter);
	++InOutFrameWorldQueries;
	++M_PerformanceAccumulator.WorldQueryCount;
	if (not World->OverlapAnyTestByObjectType(
		CapsuleCenter,
		FQuat::Identity,
		CoverFinderWorldSubsystemPrivate::GetAllCoverObjectQueryParams(),
		CapsuleShape,
		CoverFinderWorldSubsystemPrivate::BuildCoverCollisionQueryParams()))
	{
		return true;
	}

	// Only pay for the full overlap list when something is there: a soldier occupying the slot must not unpublish it.
	++InOutFrameWorldQueries;
	++M_PerformanceAccumulator.WorldQueryCount;
	return not CoverFinderWorldSubsystemPrivate::GetHasNonInfantryOverlap(*World, CapsuleCenter, CapsuleShape);
}

FCollisionShape URTSCoverFinderWorldSubsystem::BuildStandingSpaceShape(
	const FVector& GroundLocation,
	FVector& OutShapeCenter) const
{
	const float ColumnRadius = FMath::Min(M_ActiveSettings.StandingSpaceRadius, M_ActiveSettings.AgentRadius);
	const float ColumnBottom = FMath::Max(
		CoverFinderWorldSubsystemPrivate::CapsuleGroundClearance,
		M_ActiveSettings.StandingSpaceFloorClearance);
	// A capsule cannot be shorter than it is wide.
	const float ColumnHalfHeight = FMath::Max(
		ColumnRadius,
		(M_ActiveSettings.StandingSpaceHeight - ColumnBottom) * 0.5f);
	OutShapeCenter = GroundLocation + FVector::UpVector * (ColumnBottom + ColumnHalfHeight);
	return FCollisionShape::MakeCapsule(ColumnRadius, ColumnHalfHeight);
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
		Observation.bBlockingHit = CoverFinderWorldSubsystemPrivate::TraceAllCoverProviders(
			*World,
			TraceStart,
			TraceEnd,
			HitResult);
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
	if (TraceDomain != ECoverFinderTraceDomain::ActiveScan ||
		M_ActiveScanDomain == ECoverFinderScanDomain::Environment)
	{
		Observation.BlockingProviderHandle = FindOrAddBlockingProviderHandle(HitResult.GetActor());
	}
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
	TagThinObstacleCoverPoints();
	RebuildCoverSpatialGrid();
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
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
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
		for (const TPair<int64, TWeakObjectPtr<ASquadUnit>>& Reservation : M_CoverReservations)
		{
			const ASquadUnit* SquadUnit = Reservation.Value.Get();
			const int32* CoverPointIndex = M_CoverPointIndices.Find(Reservation.Key);
			if (not IsValid(SquadUnit) || CoverPointIndex == nullptr ||
				not M_CoverPoints.IsValidIndex(*CoverPointIndex))
			{
				continue;
			}
			DrawDebugLine(
				World,
				SquadUnit->GetActorLocation() + FVector::UpVector * CoverFinderWorldSubsystemPrivate::DebugPointHeight,
				M_CoverPoints[*CoverPointIndex].Location +
					FVector::UpVector * CoverFinderWorldSubsystemPrivate::DebugPointHeight,
				FColor::Cyan,
				false,
				CoverSettings->M_DebugDrawDurationSeconds,
				0,
				CoverFinderWorldSubsystemPrivate::DebugLineThickness);
		}
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
