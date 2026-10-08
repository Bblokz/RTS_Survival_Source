// The "explain at cursor" debug tool of the cover finder. Every function here returns at once unless
// DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols is set, and the console command is only
// registered in that case.

#include "CoverFinderWorldSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorker.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCoverExplain, Log, All);

namespace CoverFinderExplainPrivate
{
	const TCHAR* const ExplainCommandName = TEXT("RTS.CoverFinder.ExplainAtCursor");
	constexpr float PublishedCoverReportRadius = 400.0f;
	constexpr float ObstacleReportRadius = 350.0f;
	constexpr int32 MaximumReportedObstacles = 6;
	constexpr float CursorProjectionExtent = 100.0f;
	constexpr float DebugDrawSeconds = 15.0f;
	constexpr float DebugDrawHeight = 120.0f;
	constexpr float DebugLineThickness = 3.0f;
	constexpr int32 ExpectedLocationArgumentCount = 3;

	const TCHAR* GetCoverTypeName(const ERTSCoverType CoverType)
	{
		switch (CoverType)
		{
		case ERTSCoverType::Crouch:
			return TEXT("crouch");
		case ERTSCoverType::StandingLeft:
			return TEXT("standing-left");
		case ERTSCoverType::StandingRight:
			return TEXT("standing-right");
		case ERTSCoverType::TrenchStandUp:
			return TEXT("trench-stand-up");
		default:
			return TEXT("unknown");
		}
	}

	FColor GetResultColor(const FString& Result)
	{
		if (Result.StartsWith(TEXT("STANDING")))
		{
			return FColor::Green;
		}
		return Result.StartsWith(TEXT("CROUCH")) ? FColor::Cyan : FColor::Red;
	}
}

void URTSCoverFinderWorldSubsystem::RegisterExplainConsoleCommand()
{
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		// Console commands are global while every play world has its own subsystem; the first one registers it.
		IConsoleManager& ConsoleManager = IConsoleManager::Get();
		if (ConsoleManager.IsNameRegistered(CoverFinderExplainPrivate::ExplainCommandName))
		{
			return;
		}
		M_ExplainConsoleCommand = ConsoleManager.RegisterConsoleCommand(
			CoverFinderExplainPrivate::ExplainCommandName,
			TEXT("Logs and draws why the cover scanner does or does not publish cover at the ground under the cursor. Optional arguments: X Y Z of a location to explain instead."),
			FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&URTSCoverFinderWorldSubsystem::RunExplainConsoleCommand));
	}
}

void URTSCoverFinderWorldSubsystem::UnregisterExplainConsoleCommand()
{
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		if (M_ExplainConsoleCommand == nullptr)
		{
			return;
		}
		IConsoleManager::Get().UnregisterConsoleObject(M_ExplainConsoleCommand);
		M_ExplainConsoleCommand = nullptr;
	}
}

void URTSCoverFinderWorldSubsystem::TickCommandLineExplain()
{
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		if (bM_HasRunCommandLineExplain || not bM_EnvironmentScanComplete)
		{
			return;
		}
		bM_HasRunCommandLineExplain = true;
		FString LocationText;
		FVector ExplainLocation = FVector::ZeroVector;
		if (FParse::Value(FCommandLine::Get(), TEXT("CoverFinderExplainAt="), LocationText) &&
			ExplainLocation.InitFromString(LocationText))
		{
			ExplainCoverAtLocation(ExplainLocation);
		}
	}
}

void URTSCoverFinderWorldSubsystem::RunExplainConsoleCommand(const TArray<FString>& Arguments, UWorld* World)
{
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		URTSCoverFinderWorldSubsystem* CoverSubsystem = IsValid(World)
			? World->GetSubsystem<URTSCoverFinderWorldSubsystem>()
			: nullptr;
		if (not IsValid(CoverSubsystem))
		{
			UE_LOG(LogRTSCoverExplain, Error, TEXT("RTS_COVER_EXPLAIN failed: no cover subsystem in this world; run it while playing."));
			return;
		}
		if (Arguments.Num() >= CoverFinderExplainPrivate::ExpectedLocationArgumentCount)
		{
			CoverSubsystem->ExplainCoverAtLocation(FVector(
				FCString::Atof(*Arguments[0]),
				FCString::Atof(*Arguments[1]),
				FCString::Atof(*Arguments[2])));
			return;
		}
		const APlayerController* PlayerController = World->GetFirstPlayerController();
		FHitResult CursorHit;
		if (not IsValid(PlayerController) ||
			not PlayerController->GetHitResultUnderCursor(COLLISION_TRACE_LANDSCAPE, false, CursorHit))
		{
			UE_LOG(LogRTSCoverExplain, Error, TEXT("RTS_COVER_EXPLAIN failed: the cursor is not over the ground."));
			return;
		}
		CoverSubsystem->ExplainCoverAtLocation(CursorHit.Location);
	}
}

void URTSCoverFinderWorldSubsystem::ExplainCoverAtLocation(const FVector& GroundLocation)
{
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		UE_LOG(
			LogRTSCoverExplain,
			Display,
			TEXT("RTS_COVER_EXPLAIN ===== location=%s published_points=%d ====="),
			*GroundLocation.ToCompactString(),
			M_CoverPoints.Num());
		if (not bM_EnvironmentScanComplete)
		{
			UE_LOG(LogRTSCoverExplain, Display, TEXT("RTS_COVER_EXPLAIN the first scan of this map has not finished: no cover exists anywhere yet. Try again in a few seconds."));
			return;
		}
		// The probes below reuse the scan's own functions, which read the scan's domain and count their queries;
		// both are put back so explaining never changes a scan that is running.
		TGuardValue<ECoverFinderScanDomain> ScanDomainGuard(M_ActiveScanDomain, ECoverFinderScanDomain::Environment);
		TGuardValue<FCoverFinderPerformanceAccumulator> PerformanceGuard(M_PerformanceAccumulator, M_PerformanceAccumulator);

		UE_LOG(LogRTSCoverExplain, Display, TEXT("RTS_COVER_EXPLAIN the probes below test objects only; cover from the landscape itself comes from the one-time landscape scan."));
		Explain_LogPublishedCoverNear(GroundLocation);
		TArray<FCoverFocusedSample> ThinObstacleSamples;
		Explain_LogObstaclesNear(GroundLocation, ThinObstacleSamples);
		Explain_ProbeSample(TEXT("grid probe at cursor"), GroundLocation, FVector::ZeroVector, false);
		for (int32 SampleIndex = 0; SampleIndex < ThinObstacleSamples.Num(); ++SampleIndex)
		{
			Explain_ProbeSample(
				FString::Printf(TEXT("thin-obstacle probe %d of %d"), SampleIndex + 1, ThinObstacleSamples.Num()),
				ThinObstacleSamples[SampleIndex].Location,
				ThinObstacleSamples[SampleIndex].AimLocation,
				true);
		}
		UE_LOG(LogRTSCoverExplain, Display, TEXT("RTS_COVER_EXPLAIN ===== end ====="));
	}
}

void URTSCoverFinderWorldSubsystem::Explain_LogPublishedCoverNear(const FVector& GroundLocation) const
{
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		const TArray<FRTSCoverPoint> NearbyPoints = FindCoverPointsInRadius(
			GroundLocation,
			CoverFinderExplainPrivate::PublishedCoverReportRadius);
		UE_LOG(
			LogRTSCoverExplain,
			Display,
			TEXT("RTS_COVER_EXPLAIN published cover within %.0f cm: %d"),
			CoverFinderExplainPrivate::PublishedCoverReportRadius,
			NearbyPoints.Num());
		for (const FRTSCoverPoint& CoverPoint : NearbyPoints)
		{
			UE_LOG(
				LogRTSCoverExplain,
				Display,
				TEXT("RTS_COVER_EXPLAIN   %s at %s, %.0f cm away, source=%s, reserved=%s, thin_obstacle_room=%d of %d used"),
				CoverFinderExplainPrivate::GetCoverTypeName(CoverPoint.CoverType),
				*CoverPoint.Location.ToCompactString(),
				FVector::Dist2D(CoverPoint.Location, GroundLocation),
				CoverPoint.ProviderRegistrationId != 0 ? TEXT("authored") : *GetNameSafe(ResolveBlockingProvider(CoverPoint)),
				IsValid(GetCoverReservationOwner(CoverPoint.PointId)) ? TEXT("yes") : TEXT("no"),
				GetThinObstacleReservationCount(CoverPoint.ThinObstacleId, nullptr),
				static_cast<int32>(CoverPoint.ThinObstacleCapacity));
		}
	}
}

FString URTSCoverFinderWorldSubsystem::Explain_DescribeCapsuleOverlaps(const FVector& GroundLocation) const
{
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		const UWorld* World = GetWorld();
		if (not IsValid(World))
		{
			return FString();
		}
		FVector CapsuleCenter = FVector::ZeroVector;
		const FCollisionShape StandingSpaceShape = BuildStandingSpaceShape(GroundLocation, CapsuleCenter);
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(
			Overlaps,
			CapsuleCenter,
			FQuat::Identity,
			FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
			StandingSpaceShape,
			FCollisionQueryParams(SCENE_QUERY_STAT(RTSCoverExplainCapsule), true));
		FString Description;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			const UPrimitiveComponent* OverlappedComponent = Overlap.GetComponent();
			if (not GetIsScannableEnvironmentComponent(OverlappedComponent))
			{
				continue;
			}
			// The scanner treats any overlap as "cannot stand here", whether or not the object would stop a soldier.
			Description += FString::Printf(
				TEXT("[%s.%s, %s soldiers]"),
				*GetNameSafe(Overlap.GetActor()),
				*GetNameSafe(OverlappedComponent),
				OverlappedComponent->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block
					? TEXT("blocks")
					: TEXT("does not block"));
		}
		return Description.IsEmpty()
			? FString(TEXT("nothing the scanner counts (the overlap was a soldier or landscape)"))
			: Description + TEXT(" blocked heights: ") + Explain_DescribeBlockedHeights(GroundLocation);
	}
	else
	{
		return FString();
	}
}

FString URTSCoverFinderWorldSubsystem::Explain_DescribeBlockedHeights(const FVector& GroundLocation) const
{
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		constexpr float SliceHeight = 15.0f;
		const UWorld* World = GetWorld();
		if (not IsValid(World))
		{
			return FString();
		}
		const float ColumnRadius = FMath::Min(M_ActiveSettings.StandingSpaceRadius, M_ActiveSettings.AgentRadius);
		const FCollisionShape SliceShape = FCollisionShape::MakeBox(FVector(ColumnRadius, ColumnRadius, SliceHeight * 0.5f));
		FString BlockedHeights;
		for (float SliceBottom = 0.0f; SliceBottom < M_ActiveSettings.AgentHeight; SliceBottom += SliceHeight)
		{
			TArray<FOverlapResult> SliceOverlaps;
			World->OverlapMultiByObjectType(
				SliceOverlaps,
				GroundLocation + FVector::UpVector * (SliceBottom + SliceHeight * 0.5f),
				FQuat::Identity,
				FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
				SliceShape,
				FCollisionQueryParams(SCENE_QUERY_STAT(RTSCoverExplainSlice), true));
			const bool bSliceBlocked = SliceOverlaps.ContainsByPredicate([](const FOverlapResult& Overlap)
			{
				return GetIsScannableEnvironmentComponent(Overlap.GetComponent());
			});
			if (bSliceBlocked)
			{
				BlockedHeights += FString::Printf(TEXT("%.0f-%.0f "), SliceBottom, SliceBottom + SliceHeight);
			}
		}
		return BlockedHeights.IsEmpty() ? FString(TEXT("none")) : BlockedHeights + TEXT("cm above the ground");
	}
	else
	{
		return FString();
	}
}

bool URTSCoverFinderWorldSubsystem::Explain_TryBuildObstacleRing(
	const FOverlapResult& Overlap,
	const FBox& CollisionBounds,
	TArray<FCoverFocusedSample>& OutSamples,
	FBox& OutTrunkBounds) const
{
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		if (FCoverFinderAlgorithms::AppendThinObstacleSamples(CollisionBounds, M_ActiveSettings, OutSamples))
		{
			return true;
		}
		const UPrimitiveComponent* PrimitiveComponent = Overlap.GetComponent();
		const bool bFoundTrunk = IsValid(PrimitiveComponent) && Overlap.ItemIndex == INDEX_NONE &&
			TryMeasureTrunk(*PrimitiveComponent, CollisionBounds, OutTrunkBounds);
		return bFoundTrunk &&
			FCoverFinderAlgorithms::AppendThinObstacleSamples(OutTrunkBounds, M_ActiveSettings, OutSamples);
	}
	else
	{
		return false;
	}
}

void URTSCoverFinderWorldSubsystem::Explain_LogObstaclesNear(
	const FVector& GroundLocation,
	TArray<FCoverFocusedSample>& OutThinObstacleSamples)
{
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		const UWorld* World = GetWorld();
		if (not IsValid(World))
		{
			return;
		}
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(
			Overlaps,
			GroundLocation,
			FQuat::Identity,
			FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
			FCollisionShape::MakeSphere(CoverFinderExplainPrivate::ObstacleReportRadius),
			FCollisionQueryParams(SCENE_QUERY_STAT(RTSCoverExplain), false));
		Overlaps.Sort([&GroundLocation](const FOverlapResult& Left, const FOverlapResult& Right)
		{
			return FVector::DistSquared2D(GetOverlapCollisionBounds(Left).GetCenter(), GroundLocation) <
				FVector::DistSquared2D(GetOverlapCollisionBounds(Right).GetCenter(), GroundLocation);
		});
		int32 ReportedObstacleCount = 0;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (not GetIsScannableEnvironmentComponent(Overlap.GetComponent()) ||
				ReportedObstacleCount >= CoverFinderExplainPrivate::MaximumReportedObstacles)
			{
				continue;
			}
			++ReportedObstacleCount;
			const FBox CollisionBounds = GetOverlapCollisionBounds(Overlap);
			TArray<FCoverFocusedSample> RingSamples;
			FBox TrunkBounds(ForceInit);
			const bool bIsThinObstacle = M_ActiveSettings.bProbeThinObstacles &&
				Explain_TryBuildObstacleRing(Overlap, CollisionBounds, RingSamples, TrunkBounds);
			const FVector TrunkSize = TrunkBounds.IsValid != 0 ? TrunkBounds.GetSize() : FVector::ZeroVector;
			UE_LOG(
				LogRTSCoverExplain,
				Display,
				TEXT("RTS_COVER_EXPLAIN obstacle actor=%s component=%s instance=%d collision_size_cm=(%.0f x %.0f x %.0f) visual_size_cm=(%.0f x %.0f) trunk_at_crouch_height_cm=(%.0f x %.0f) scanned_as=%s"),
				*GetNameSafe(Overlap.GetActor()),
				*GetNameSafe(Overlap.GetComponent()),
				Overlap.ItemIndex,
				CollisionBounds.GetSize().X,
				CollisionBounds.GetSize().Y,
				CollisionBounds.GetSize().Z,
				Overlap.GetComponent()->Bounds.GetBox().GetSize().X,
				Overlap.GetComponent()->Bounds.GetBox().GetSize().Y,
				TrunkSize.X,
				TrunkSize.Y,
				bIsThinObstacle ? TEXT("thin obstacle: ring of aimed probes plus the grid") : TEXT("grid, with re-aimed probes for grazing hits"));
			if (bIsThinObstacle && OutThinObstacleSamples.IsEmpty())
			{
				OutThinObstacleSamples = MoveTemp(RingSamples);
			}
		}
		if (ReportedObstacleCount == 0)
		{
			UE_LOG(
				LogRTSCoverExplain,
				Display,
				TEXT("RTS_COVER_EXPLAIN no object with collision within %.0f cm: the scanner has nothing to find here. If something is visible, it has no collision on a channel the scanner reads."),
				CoverFinderExplainPrivate::ObstacleReportRadius);
		}
	}
}

void URTSCoverFinderWorldSubsystem::Explain_ProbeSample(
	const FString& Label,
	const FVector& PlannedLocation,
	const FVector& AimLocation,
	const bool bAimed)
{
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		UWorld* World = GetWorld();
		const UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(World);
		const ANavigationData* NavigationData = GetCharacterNavigationData();
		if (not IsValid(World) || not IsValid(NavigationSystem) || not IsValid(NavigationData))
		{
			UE_LOG(LogRTSCoverExplain, Display, TEXT("RTS_COVER_EXPLAIN %s: no infantry navigation data in this world."), *Label);
			return;
		}
		const float HorizontalExtent = bAimed ? M_ActiveSettings.AgentRadius : CoverFinderExplainPrivate::CursorProjectionExtent;
		FNavLocation ProjectedLocation;
		if (not NavigationSystem->ProjectPointToNavigation(
			PlannedLocation,
			ProjectedLocation,
			FVector(HorizontalExtent, HorizontalExtent, M_ActiveSettings.AgentHeight),
			NavigationData))
		{
			UE_LOG(
				LogRTSCoverExplain,
				Display,
				TEXT("RTS_COVER_EXPLAIN %s: REJECTED, no infantry navmesh within %.0f cm of %s."),
				*Label,
				HorizontalExtent,
				*PlannedLocation.ToCompactString());
			return;
		}
		int32 UnusedQueryCount = 0;
		if (not GetCanInfantryOccupyLocation(ProjectedLocation.Location, UnusedQueryCount))
		{
			UE_LOG(
				LogRTSCoverExplain,
				Display,
				TEXT("RTS_COVER_EXPLAIN %s: REJECTED, the space a soldier's body needs at %s (settings M_StandingSpaceRadius / M_StandingSpaceFloorClearance / M_StandingSpaceHeight) overlaps: %s"),
				*Label,
				*ProjectedLocation.Location.ToCompactString(),
				*Explain_DescribeCapsuleOverlaps(ProjectedLocation.Location));
			return;
		}
		const int32 DirectionCount = bAimed ? 1 : RTSCoverFinderConstants::SearchDirectionCount;
		for (int32 DirectionIndex = 0; DirectionIndex < DirectionCount; ++DirectionIndex)
		{
			const float GridYawDegrees = 360.0f * static_cast<float>(DirectionIndex) /
				static_cast<float>(RTSCoverFinderConstants::SearchDirectionCount);
			const FVector SearchDirection = bAimed
				? (AimLocation - ProjectedLocation.Location).GetSafeNormal2D()
				: FVector::ForwardVector.RotateAngleAxis(GridYawDegrees, FVector::UpVector);
			FString Result = Explain_ProbeDirection(
				*NavigationSystem,
				*NavigationData,
				ProjectedLocation.Location,
				SearchDirection);
			// A ring probe that found no regular cover gets the open-frame rule, exactly as in a scan.
			FCoverDirectionalObservation FrameObservation;
			FrameObservation.SearchDirection = SearchDirection;
			const bool bRegularRuleFoundCover = Result.StartsWith(TEXT("STANDING")) || Result.StartsWith(TEXT("CROUCH")) ||
				Result.StartsWith(TEXT("REJECTED for standing"));
			// The explanation tries every side; a scan only lets M_OpenFramePointsPerObstacle of them publish.
			if (bAimed && M_ActiveSettings.bFindOpenFrameCover && not bRegularRuleFoundCover)
			{
				SampleOpenFrameCover(
					*NavigationSystem,
					*NavigationData,
					ProjectedLocation.Location,
					FVector::Dist2D(AimLocation, ProjectedLocation.Location),
					FrameObservation,
					UnusedQueryCount);
				Result = FrameObservation.bOpenFrameCover
					? FString::Printf(
						TEXT("CROUCH cover behind an open frame, soldier at %s; regular rule: %s"),
						*FrameObservation.OpenFrameCoverLocation.ToCompactString(),
						*Result)
					: Result + TEXT("; no open frame either (too few beams in the way, or too tall to fire over)");
			}
			UE_LOG(
				LogRTSCoverExplain,
				Display,
				TEXT("RTS_COVER_EXPLAIN %s, yaw %.0f: %s"),
				*Label,
				SearchDirection.Rotation().Yaw,
				*Result);
			const FVector DrawStart = ProjectedLocation.Location + FVector::UpVector * CoverFinderExplainPrivate::DebugDrawHeight;
			DrawDebugLine(
				World,
				DrawStart,
				DrawStart + SearchDirection * M_ActiveSettings.MaximumCoverSearchDistance,
				CoverFinderExplainPrivate::GetResultColor(Result),
				false,
				CoverFinderExplainPrivate::DebugDrawSeconds,
				0,
				CoverFinderExplainPrivate::DebugLineThickness);
		}
	}
}

FString URTSCoverFinderWorldSubsystem::Explain_ProbeDirection(
	const UNavigationSystemV1& NavigationSystem,
	const ANavigationData& NavigationData,
	const FVector& ProtectedLocation,
	const FVector& SearchDirection)
{
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		int32 UnusedQueryCount = 0;
		const FCoverTraceObservation CrouchTrace = TraceCoverHeight(
			ProtectedLocation, SearchDirection, M_ActiveSettings.MinimumCrouchCoverHeight, UnusedQueryCount);
		if (not CrouchTrace.bBlockingHit)
		{
			return FString::Printf(
				TEXT("nothing at crouch height (%.0f cm) within %.0f cm"),
				M_ActiveSettings.MinimumCrouchCoverHeight,
				M_ActiveSettings.MaximumCoverSearchDistance);
		}
		const FCoverTraceObservation LowerTrace = TraceCoverHeight(
			ProtectedLocation, SearchDirection, RTSCoverFinderConstants::LowerSupportProbeHeight, UnusedQueryCount);
		const FCoverTraceObservation StandingTrace = TraceCoverHeight(
			ProtectedLocation, SearchDirection, RTSCoverFinderConstants::StandingCoverHeight, UnusedQueryCount);
		if (not FCoverFinderAlgorithms::GetIsSameSurface(LowerTrace, CrouchTrace, M_ActiveSettings))
		{
			return FString::Printf(
				TEXT("REJECTED, hit %s at %.0f cm but it is not one surface from knee to crouch height (overhang, gap underneath, or two objects)"),
				*GetNameSafe(M_BlockingProviderActors.FindRef(CrouchTrace.BlockingProviderHandle).Get()),
				CrouchTrace.Distance);
		}
		if (not FCoverFinderAlgorithms::GetIsSameSurface(CrouchTrace, StandingTrace, M_ActiveSettings))
		{
			return FString::Printf(
				TEXT("CROUCH cover at %.0f cm; not standing cover because nothing continuous is there at %.0f cm height"),
				CrouchTrace.Distance,
				RTSCoverFinderConstants::StandingCoverHeight);
		}
		const FVector CoverNormal = FCoverFinderAlgorithms::BuildCoverNormal(StandingTrace, SearchDirection);
		const float FaceAlignment = FVector::DotProduct(CoverNormal, -SearchDirection.GetSafeNormal2D());
		if (FaceAlignment < RTSCoverFinderConstants::MinimumStandingFaceAlignment)
		{
			return FString::Printf(
				TEXT("REJECTED for standing, the surface was hit at a slant (alignment %.2f, needs %.2f); %s"),
				FaceAlignment,
				RTSCoverFinderConstants::MinimumStandingFaceAlignment,
				M_ActiveSettings.bReaimSlantedHits
					? TEXT("a scan probes this spot again from straight in front")
					: TEXT("re-aiming slanted hits is switched off in the settings"));
		}
		const FVector CoverFacingDirection = -CoverNormal;
		const FVector LeftDirection = FVector::CrossProduct(CoverFacingDirection, FVector::UpVector).GetSafeNormal();
		const FString LeftResult = Explain_ProbeStandingSide(
			NavigationSystem, NavigationData, ProtectedLocation, CoverFacingDirection, LeftDirection);
		const FString RightResult = Explain_ProbeStandingSide(
			NavigationSystem, NavigationData, ProtectedLocation, CoverFacingDirection, -LeftDirection);
		const bool bAnySideOpen = LeftResult == TEXT("ok") || RightResult == TEXT("ok");
		return FString::Printf(
			TEXT("%s, high cover at %.0f cm; peek left: %s; peek right: %s"),
			bAnySideOpen ? TEXT("STANDING cover") : TEXT("REJECTED for standing"),
			StandingTrace.Distance,
			*LeftResult,
			*RightResult);
	}
	else
	{
		return FString();
	}
}

FString URTSCoverFinderWorldSubsystem::Explain_ProbeStandingSide(
	const UNavigationSystemV1& NavigationSystem,
	const ANavigationData& NavigationData,
	const FVector& ProtectedLocation,
	const FVector& CoverFacingDirection,
	const FVector& SideDirection)
{
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		int32 UnusedQueryCount = 0;
		float LastCoveredOffset = 0.0f;
		float FirstOpenOffset = 0.0f;
		if (not FindStandingOpening(
			ProtectedLocation,
			CoverFacingDirection,
			SideDirection,
			UnusedQueryCount,
			LastCoveredOffset,
			FirstOpenOffset))
		{
			return FString::Printf(
				TEXT("the cover has no end within %.0f cm on this side"),
				M_ActiveSettings.SearchGridSpacing * 1.25f);
		}
		if (not GetIsStandingGapIntervalOpen(
			NavigationSystem,
			NavigationData,
			ProtectedLocation,
			CoverFacingDirection,
			SideDirection,
			FirstOpenOffset,
			UnusedQueryCount))
		{
			return FString::Printf(
				TEXT("the cover ends after %.0f cm, but the %.0f cm beside it is blocked, off the navmesh, or has no clear view past the cover"),
				FirstOpenOffset,
				M_ActiveSettings.StandingPeekGapWidth);
		}
		FVector CoverLocation = FVector::ZeroVector;
		if (not SampleStandingGap(
			NavigationSystem,
			NavigationData,
			ProtectedLocation,
			CoverFacingDirection,
			SideDirection,
			UnusedQueryCount,
			CoverLocation))
		{
			return TEXT("the position at the edge of the cover is off the infantry navmesh");
		}
		return TEXT("ok");
	}
	else
	{
		return FString();
	}
}
