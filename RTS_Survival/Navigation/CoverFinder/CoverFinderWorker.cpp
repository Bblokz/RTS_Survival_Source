#include "CoverFinderWorker.h"

#include "Algo/Sort.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"

namespace CoverFinderWorkerPrivate
{
	constexpr uint64 StableIdOffset = 1469598103934665603ull;
	constexpr uint64 StableIdPrime = 1099511628211ull;
	constexpr float StableIdLocationQuantum = 10.0f;
	constexpr float StableIdNormalQuantum = 0.05f;

	FIntVector BuildSampleKey(
		const int32 GridX,
		const int32 GridY,
		const FBox& TileBounds,
		const float GridSpacing)
	{
		const int32 GridZ = FMath::RoundToInt(TileBounds.GetCenter().Z / GridSpacing);
		return FIntVector(GridX, GridY, GridZ);
	}

	bool AppendTileColumn(
		const int32 GridX,
		const int32 MinimumGridY,
		const int32 MaximumGridY,
		const FBox& TileBounds,
		const float GridSpacing,
		TSet<FIntVector>& UsedSampleKeys,
		TArray<FVector>& OutLocations)
	{
		for (int32 GridY = MinimumGridY; GridY <= MaximumGridY; ++GridY)
		{
			if (OutLocations.Num() >= RTSCoverFinderConstants::MaxSampleLocations)
			{
				return true;
			}

			const FIntVector SampleKey = BuildSampleKey(GridX, GridY, TileBounds, GridSpacing);
			if (UsedSampleKeys.Contains(SampleKey))
			{
				continue;
			}

			UsedSampleKeys.Add(SampleKey);
			OutLocations.Emplace(
				static_cast<float>(GridX) * GridSpacing,
				static_cast<float>(GridY) * GridSpacing,
				TileBounds.GetCenter().Z);
		}
		return false;
	}

	bool AppendTileSamples(
		const FBox& TileBounds,
		const float GridSpacing,
		TSet<FIntVector>& UsedSampleKeys,
		TArray<FVector>& OutLocations)
	{
		if (TileBounds.IsValid == 0)
		{
			return false;
		}

		const int32 MinimumGridX = FMath::CeilToInt(TileBounds.Min.X / GridSpacing);
		const int32 MaximumGridX = FMath::FloorToInt(TileBounds.Max.X / GridSpacing);
		const int32 MinimumGridY = FMath::CeilToInt(TileBounds.Min.Y / GridSpacing);
		const int32 MaximumGridY = FMath::FloorToInt(TileBounds.Max.Y / GridSpacing);
		for (int32 GridX = MinimumGridX; GridX <= MaximumGridX; ++GridX)
		{
			if (AppendTileColumn(
				GridX,
				MinimumGridY,
				MaximumGridY,
				TileBounds,
				GridSpacing,
				UsedSampleKeys,
				OutLocations))
			{
				return true;
			}
		}
		return false;
	}

	bool SortLocations(const FVector& Left, const FVector& Right)
	{
		if (not FMath::IsNearlyEqual(Left.X, Right.X))
		{
			return Left.X < Right.X;
		}
		if (not FMath::IsNearlyEqual(Left.Y, Right.Y))
		{
			return Left.Y < Right.Y;
		}
		return Left.Z < Right.Z;
	}

	void AppendCandidate(
		const FVector& Location,
		const FVector& CoverNormal,
		const ERTSCoverType CoverType,
		const uint64 BlockingProviderHandle,
		TArray<FRTSCoverPoint>& OutCandidates)
	{
		FRTSCoverPoint& Candidate = OutCandidates.AddDefaulted_GetRef();
		Candidate.Location = Location;
		Candidate.CoverNormal = CoverNormal;
		Candidate.CoverType = CoverType;
		Candidate.BlockingProviderHandle = BlockingProviderHandle;
	}

	void AppendCandidatesForDirection(
		const FCoverProbeObservation& Observation,
		const FCoverDirectionalObservation& DirectionObservation,
		const FCoverFinderSettingsSnapshot& Settings,
		TArray<FRTSCoverPoint>& OutCandidates)
	{
		if (not FCoverFinderAlgorithms::GetIsSameSurface(
			DirectionObservation.LowerTrace,
			DirectionObservation.CrouchTrace,
			Settings))
		{
			// No regular cover here; an open frame found by the game thread still gives its own crouch point.
			if (DirectionObservation.bOpenFrameCover)
			{
				AppendCandidate(
					DirectionObservation.OpenFrameCoverLocation,
					-DirectionObservation.SearchDirection.GetSafeNormal2D(),
					ERTSCoverType::Crouch,
					DirectionObservation.OpenFrameProviderHandle,
					OutCandidates);
				return;
			}
			// Too low to crouch behind, but a soldier can lie behind it.
			if (DirectionObservation.bHasProneLyingSpace &&
				FCoverFinderAlgorithms::GetIsProneCoverEvidence(DirectionObservation, Settings))
			{
				AppendCandidate(
					DirectionObservation.ProneCoverLocation,
					FCoverFinderAlgorithms::BuildCoverNormal(
						DirectionObservation.LowerTrace,
						DirectionObservation.SearchDirection),
					ERTSCoverType::Prone,
					DirectionObservation.LowerTrace.BlockingProviderHandle,
					OutCandidates);
			}
			return;
		}

		const bool bStandingSurface = FCoverFinderAlgorithms::GetIsSameSurface(
			DirectionObservation.CrouchTrace,
			DirectionObservation.StandingTrace,
			Settings);
		const FCoverTraceObservation& SurfaceTrace = bStandingSurface
			? DirectionObservation.StandingTrace
			: DirectionObservation.CrouchTrace;
		const FVector CoverNormal = FCoverFinderAlgorithms::BuildCoverNormal(
			SurfaceTrace,
			DirectionObservation.SearchDirection);

		if (not bStandingSurface)
		{
			AppendCandidate(
				Observation.ProjectedLocation,
				CoverNormal,
				ERTSCoverType::Crouch,
				SurfaceTrace.BlockingProviderHandle,
				OutCandidates);
			return;
		}

		if (DirectionObservation.bLeftGapOpen)
		{
			AppendCandidate(
				DirectionObservation.LeftCoverLocation,
				CoverNormal,
				ERTSCoverType::StandingLeft,
				SurfaceTrace.BlockingProviderHandle,
				OutCandidates);
		}
		if (DirectionObservation.bRightGapOpen)
		{
			AppendCandidate(
				DirectionObservation.RightCoverLocation,
				CoverNormal,
				ERTSCoverType::StandingRight,
				SurfaceTrace.BlockingProviderHandle,
				OutCandidates);
		}
	}

	void AppendCandidatesForObservation(
		const FCoverProbeObservation& Observation,
		const FCoverFinderSettingsSnapshot& Settings,
		TArray<FRTSCoverPoint>& OutCandidates)
	{
		for (const FCoverDirectionalObservation& DirectionObservation : Observation.DirectionalObservations)
		{
			AppendCandidatesForDirection(Observation, DirectionObservation, Settings, OutCandidates);
		}
	}

	bool SortCandidates(const FRTSCoverPoint& Left, const FRTSCoverPoint& Right)
	{
		const bool bLeftIsAuthored = Left.ProviderRegistrationId != 0;
		const bool bRightIsAuthored = Right.ProviderRegistrationId != 0;
		if (bLeftIsAuthored != bRightIsAuthored)
		{
			return bLeftIsAuthored;
		}
		if (not FMath::IsNearlyEqual(Left.Location.X, Right.Location.X))
		{
			return Left.Location.X < Right.Location.X;
		}
		if (not FMath::IsNearlyEqual(Left.Location.Y, Right.Location.Y))
		{
			return Left.Location.Y < Right.Location.Y;
		}
		if (not FMath::IsNearlyEqual(Left.Location.Z, Right.Location.Z))
		{
			return Left.Location.Z < Right.Location.Z;
		}
		return static_cast<uint8>(Left.CoverType) < static_cast<uint8>(Right.CoverType);
	}

	FIntPoint GetBucketCell(const FVector& Location, const float CellSize)
	{
		return FIntPoint(FMath::FloorToInt(Location.X / CellSize), FMath::FloorToInt(Location.Y / CellSize));
	}

	/** @return True when any location in the cell's neighbourhood lies within Distance of Location. */
	bool GetHasLocationWithin(
		const TMap<FIntPoint, TArray<FVector>>& LocationBuckets,
		const FVector& Location,
		const float Distance)
	{
		const FIntPoint LocationCell = GetBucketCell(Location, Distance);
		for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
		{
			for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
			{
				const TArray<FVector>* CellLocations = LocationBuckets.Find(LocationCell + FIntPoint(OffsetX, OffsetY));
				if (CellLocations == nullptr)
				{
					continue;
				}
				if (CellLocations->ContainsByPredicate([&Location, Distance](const FVector& OtherLocation)
				{
					return FVector::DistSquared(OtherLocation, Location) <= FMath::Square(Distance);
				}))
				{
					return true;
				}
			}
		}
		return false;
	}

	bool BucketContainsDuplicate(
		const TArray<int32>& CandidateIndices,
		const TArray<FRTSCoverPoint>& AcceptedCandidates,
		const FRTSCoverPoint& Candidate,
		const float MinimumSpacing)
	{
		for (const int32 CandidateIndex : CandidateIndices)
		{
			if (not AcceptedCandidates.IsValidIndex(CandidateIndex))
			{
				continue;
			}

			if (FCoverFinderAlgorithms::GetAreDuplicateCandidates(
				AcceptedCandidates[CandidateIndex],
				Candidate,
				MinimumSpacing))
			{
				return true;
			}
		}
		return false;
	}

	bool GetIsDuplicateCandidate(
		const FRTSCoverPoint& Candidate,
		const float MinimumSpacing,
		const TMap<FIntPoint, TArray<int32>>& SpatialBuckets,
		const TArray<FRTSCoverPoint>& AcceptedCandidates)
	{
		constexpr int32 NeighbourOffsets[][2] =
		{
			{-1, -1}, {-1, 0}, {-1, 1},
			{0, -1}, {0, 0}, {0, 1},
			{1, -1}, {1, 0}, {1, 1}
		};
		const FIntPoint CandidateCell(
			FMath::FloorToInt(Candidate.Location.X / MinimumSpacing),
			FMath::FloorToInt(Candidate.Location.Y / MinimumSpacing));
		for (const int32 (&NeighbourOffset)[2] : NeighbourOffsets)
		{
			const FIntPoint NeighbourCell = CandidateCell
				+ FIntPoint(NeighbourOffset[0], NeighbourOffset[1]);
			const TArray<int32>* CandidateIndices = SpatialBuckets.Find(NeighbourCell);
			if (CandidateIndices != nullptr && BucketContainsDuplicate(
				*CandidateIndices,
				AcceptedCandidates,
				Candidate,
				MinimumSpacing))
			{
				return true;
			}
		}
		return false;
	}

	void AddCandidateToSpatialBuckets(
		const FRTSCoverPoint& Candidate,
		const float MinimumSpacing,
		const int32 CandidateIndex,
		TMap<FIntPoint, TArray<int32>>& SpatialBuckets)
	{
		const FIntPoint CandidateCell(
			FMath::FloorToInt(Candidate.Location.X / MinimumSpacing),
			FMath::FloorToInt(Candidate.Location.Y / MinimumSpacing));
		SpatialBuckets.FindOrAdd(CandidateCell).Add(CandidateIndex);
	}

	void MixStableIdValue(uint64& InOutHash, const int32 Value)
	{
		InOutHash ^= static_cast<uint32>(Value);
		InOutHash *= StableIdPrime;
	}

	int64 BuildStableId(const FRTSCoverPoint& Candidate)
	{
		uint64 StableId = StableIdOffset;
		MixStableIdValue(StableId, FMath::RoundToInt(Candidate.Location.X / StableIdLocationQuantum));
		MixStableIdValue(StableId, FMath::RoundToInt(Candidate.Location.Y / StableIdLocationQuantum));
		MixStableIdValue(StableId, FMath::RoundToInt(Candidate.Location.Z / StableIdLocationQuantum));
		MixStableIdValue(StableId, FMath::RoundToInt(Candidate.CoverNormal.X / StableIdNormalQuantum));
		MixStableIdValue(StableId, FMath::RoundToInt(Candidate.CoverNormal.Y / StableIdNormalQuantum));
		MixStableIdValue(StableId, static_cast<int32>(Candidate.CoverType));
		return static_cast<int64>(StableId & MAX_int64);
	}
}

FRTSCoverPoint FCoverFinderAlgorithms::BuildWorldAuthoredPoint(
	const FTransform& OwnerTransform,
	const FRTSLocalCoverPoint& LocalCoverPoint)
{
	FRTSCoverPoint WorldCoverPoint;
	WorldCoverPoint.Location = OwnerTransform.TransformPosition(LocalCoverPoint.LocalTransform.GetLocation());
	const FQuat WorldRotation = OwnerTransform.GetRotation() * LocalCoverPoint.LocalTransform.GetRotation();
	WorldCoverPoint.CoverNormal = WorldRotation.GetForwardVector().GetSafeNormal2D();
	if (WorldCoverPoint.CoverNormal.IsNearlyZero())
	{
		WorldCoverPoint.CoverNormal = FVector::ForwardVector;
	}
	WorldCoverPoint.CoverType = LocalCoverPoint.CoverType;
	return WorldCoverPoint;
}

bool FCoverFinderAlgorithms::GetAreDuplicateCandidates(
	const FRTSCoverPoint& FirstCandidate,
	const FRTSCoverPoint& SecondCandidate,
	const float MinimumSpacing)
{
	if (FirstCandidate.CoverType != SecondCandidate.CoverType)
	{
		return false;
	}
	if (FVector::DotProduct(FirstCandidate.CoverNormal, SecondCandidate.CoverNormal)
		< RTSCoverFinderConstants::DuplicateNormalSimilarity)
	{
		return false;
	}
	return FVector::DistSquared(FirstCandidate.Location, SecondCandidate.Location)
		<= FMath::Square(MinimumSpacing);
}

TArray<FVector> FCoverFinderAlgorithms::BuildSamplePlan(
	const TArray<FBox>& TileBounds,
	const FCoverFinderSettingsSnapshot& Settings,
	bool& OutWasTruncated)
{
	OutWasTruncated = false;
	TArray<FVector> SampleLocations;
	TSet<FIntVector> UsedSampleKeys;
	for (const FBox& TileBox : TileBounds)
	{
		if (CoverFinderWorkerPrivate::AppendTileSamples(
			TileBox,
			Settings.SearchGridSpacing,
			UsedSampleKeys,
			SampleLocations))
		{
			OutWasTruncated = true;
			break;
		}
	}
	SampleLocations.Sort(&CoverFinderWorkerPrivate::SortLocations);
	return SampleLocations;
}

void FCoverFinderAlgorithms::AppendClassifiedCandidates(
	const TArray<FCoverProbeObservation>& Observations,
	const FCoverFinderSettingsSnapshot& Settings,
	TArray<FRTSCoverPoint>& OutCandidates)
{
	for (const FCoverProbeObservation& Observation : Observations)
	{
		CoverFinderWorkerPrivate::AppendCandidatesForObservation(Observation, Settings, OutCandidates);
	}
}

TArray<FRTSCoverPoint> FCoverFinderAlgorithms::FinalizeCandidates(
	TArray<FRTSCoverPoint>&& RawCandidates,
	const FCoverFinderSettingsSnapshot& Settings,
	const TSet<int64>* PreferredPronePointIds)
{
	RawCandidates.Sort(&CoverFinderWorkerPrivate::SortCandidates);
	TArray<FRTSCoverPoint> AcceptedCandidates;
	AcceptedCandidates.Reserve(RawCandidates.Num());
	TArray<FRTSCoverPoint> GeneratedPronePoints;
	TMap<FIntPoint, TArray<int32>> SpatialBuckets;
	for (FRTSCoverPoint& Candidate : RawCandidates)
	{
		// Found prone cover is thinned out after everything else has its place; authored points are never thinned.
		if (Candidate.CoverType == ERTSCoverType::Prone && Candidate.ProviderRegistrationId == 0)
		{
			GeneratedPronePoints.Add(MoveTemp(Candidate));
			continue;
		}
		if (CoverFinderWorkerPrivate::GetIsDuplicateCandidate(
			Candidate,
			Settings.CoverPointSpacing,
			SpatialBuckets,
			AcceptedCandidates))
		{
			continue;
		}

		Candidate.PointId = CoverFinderWorkerPrivate::BuildStableId(Candidate);
		const int32 CandidateIndex = AcceptedCandidates.Add(MoveTemp(Candidate));
		CoverFinderWorkerPrivate::AddCandidateToSpatialBuckets(
			AcceptedCandidates[CandidateIndex],
			Settings.CoverPointSpacing,
			CandidateIndex,
			SpatialBuckets);
	}
	AppendSpacedPronePoints(MoveTemp(GeneratedPronePoints), Settings, AcceptedCandidates, PreferredPronePointIds);
	return AcceptedCandidates;
}

bool FCoverFinderAlgorithms::GetIsProneCoverEvidence(
	const FCoverDirectionalObservation& DirectionObservation,
	const FCoverFinderSettingsSnapshot& Settings)
{
	const FCoverTraceObservation& LowerTrace = DirectionObservation.LowerTrace;
	if (not Settings.bFindProneCover || not LowerTrace.bBlockingHit)
	{
		return false;
	}
	if (GetIsSameSurface(LowerTrace, DirectionObservation.CrouchTrace, Settings))
	{
		return false;
	}
	if (LowerTrace.ImpactNormal.Z > Settings.ProneCoverMaximumFaceNormalZ)
	{
		return false;
	}
	const float RequiredClearDistance = LowerTrace.Distance + RTSCoverFinderConstants::ProneCoverClearDepth;
	const auto GetIsClearBehindFace = [RequiredClearDistance](const FCoverTraceObservation& Trace)
	{
		return not Trace.bBlockingHit || Trace.Distance >= RequiredClearDistance;
	};
	return GetIsClearBehindFace(DirectionObservation.ProneFireOverTrace) &&
		GetIsClearBehindFace(DirectionObservation.CrouchTrace);
}

void FCoverFinderAlgorithms::AppendSpacedPronePoints(
	TArray<FRTSCoverPoint>&& PronePoints,
	const FCoverFinderSettingsSnapshot& Settings,
	TArray<FRTSCoverPoint>& InOutAcceptedPoints,
	const TSet<int64>* PreferredPointIds)
{
	if (PronePoints.IsEmpty())
	{
		return;
	}
	for (FRTSCoverPoint& PronePoint : PronePoints)
	{
		PronePoint.PointId = CoverFinderWorkerPrivate::BuildStableId(PronePoint);
	}
	if (PreferredPointIds != nullptr)
	{
		PronePoints.StableSort([PreferredPointIds](const FRTSCoverPoint& Left, const FRTSCoverPoint& Right)
		{
			return PreferredPointIds->Contains(Left.PointId) && not PreferredPointIds->Contains(Right.PointId);
		});
	}
	const float AnyPointSpacing = FMath::Max(1.0f, Settings.CoverPointSpacing);
	const float PronePointSpacing = FMath::Max(AnyPointSpacing, Settings.ProneCoverPointSpacing);
	TMap<FIntPoint, TArray<FVector>> AnyPointBuckets;
	TMap<FIntPoint, TArray<FVector>> PronePointBuckets;
	const auto AddToBuckets = [&](const FRTSCoverPoint& CoverPoint)
	{
		AnyPointBuckets.FindOrAdd(CoverFinderWorkerPrivate::GetBucketCell(CoverPoint.Location, AnyPointSpacing))
			.Add(CoverPoint.Location);
		if (CoverPoint.CoverType == ERTSCoverType::Prone)
		{
			PronePointBuckets.FindOrAdd(
				CoverFinderWorkerPrivate::GetBucketCell(CoverPoint.Location, PronePointSpacing))
				.Add(CoverPoint.Location);
		}
	};
	for (const FRTSCoverPoint& AcceptedPoint : InOutAcceptedPoints)
	{
		AddToBuckets(AcceptedPoint);
	}
	for (FRTSCoverPoint& PronePoint : PronePoints)
	{
		const bool bIsTooClose =
			CoverFinderWorkerPrivate::GetHasLocationWithin(AnyPointBuckets, PronePoint.Location, AnyPointSpacing) ||
			CoverFinderWorkerPrivate::GetHasLocationWithin(PronePointBuckets, PronePoint.Location, PronePointSpacing);
		if (bIsTooClose)
		{
			continue;
		}
		AddToBuckets(PronePoint);
		InOutAcceptedPoints.Add(MoveTemp(PronePoint));
	}
}

bool FCoverFinderAlgorithms::TryBuildProneCompanion(
	const FRTSCoverPoint& SourcePoint,
	const FCoverFinderSettingsSnapshot& Settings,
	FRTSCoverPoint& OutCompanion)
{
	constexpr uint64 PercentScale = 100;
	const bool bIsEligibleSource = SourcePoint.ProviderRegistrationId == 0 &&
		(SourcePoint.CoverType == ERTSCoverType::Crouch ||
			SourcePoint.CoverType == ERTSCoverType::StandingLeft ||
			SourcePoint.CoverType == ERTSCoverType::StandingRight);
	const uint64 SourceHash = static_cast<uint64>(SourcePoint.PointId);
	const bool bIsChosen = static_cast<int32>(SourceHash % PercentScale) < Settings.ProneCompanionChancePercent;
	if (not Settings.bFindProneCover || not bIsEligibleSource || not bIsChosen)
	{
		return false;
	}
	const FVector AwayFromCover = SourcePoint.CoverNormal.GetSafeNormal2D();
	const FVector RightDirection = FVector::CrossProduct(FVector::UpVector, -AwayFromCover);
	// A standing point sits at the end of its wall and steps out past it; the prone point goes the other way,
	// where the wall still is.
	const bool bGoesRight = SourcePoint.CoverType == ERTSCoverType::StandingLeft ||
		(SourcePoint.CoverType == ERTSCoverType::Crouch && (SourceHash / PercentScale) % 2 == 0);
	const int32 YawJitterRange = RTSCoverFinderConstants::ProneCompanionMaximumYawJitterDegrees * 2 + 1;
	const float YawJitterDegrees = static_cast<float>(
		static_cast<int32>((SourceHash / (PercentScale * 2)) % YawJitterRange) -
		RTSCoverFinderConstants::ProneCompanionMaximumYawJitterDegrees);
	OutCompanion = FRTSCoverPoint();
	OutCompanion.Location = SourcePoint.Location +
		RightDirection * (bGoesRight ? Settings.ProneCompanionOffset : -Settings.ProneCompanionOffset) +
		AwayFromCover * RTSCoverFinderConstants::ProneCompanionBackOffset;
	OutCompanion.CoverNormal = AwayFromCover.RotateAngleAxis(YawJitterDegrees, FVector::UpVector);
	OutCompanion.CoverType = ERTSCoverType::Prone;
	OutCompanion.BlockingProviderHandle = SourcePoint.BlockingProviderHandle;
	return true;
}

float FCoverFinderAlgorithms::GetProbeStandOffDistance(const FCoverFinderSettingsSnapshot& Settings)
{
	return Settings.AgentRadius + RTSCoverFinderConstants::ProbeSurfaceClearance;
}

bool FCoverFinderAlgorithms::GetIsThinObstacle(
	const FBox& CollisionBounds,
	const FCoverFinderSettingsSnapshot& Settings)
{
	if (CollisionBounds.IsValid == 0)
	{
		return false;
	}
	const FVector ObstacleSize = CollisionBounds.GetSize();
	const bool bIsThin = FMath::Max(ObstacleSize.X, ObstacleSize.Y) <= Settings.ThinObstacleMaximumWidth;
	// A low object is ringed as well when a soldier may lie behind it; the grid easily steps over small ones.
	const float MinimumCoverHeight = Settings.bFindProneCover
		? RTSCoverFinderConstants::LowerSupportProbeHeight
		: Settings.MinimumCrouchCoverHeight;
	return bIsThin && ObstacleSize.Z >= MinimumCoverHeight;
}

int32 FCoverFinderAlgorithms::GetThinObstacleSoldierCapacity(
	const FBox& ObstacleBounds,
	const FCoverFinderSettingsSnapshot& Settings)
{
	constexpr int32 MaximumCapacity = 255;
	const FVector ObstacleSize = ObstacleBounds.GetSize();
	const float ObstacleWidth = FMath::Max(ObstacleSize.X, ObstacleSize.Y);
	return FMath::Clamp(
		FMath::RoundToInt32(ObstacleWidth / FMath::Max(1.0f, Settings.ThinObstacleWidthPerSoldier)),
		1,
		MaximumCapacity);
}

float FCoverFinderAlgorithms::GetThinObstacleCoverRadius(
	const FBox& ObstacleBounds,
	const FCoverFinderSettingsSnapshot& Settings)
{
	// Standing points sit sideways of where their probe stood, toward the edge of the obstacle.
	constexpr float SidewaysPointAllowance = 75.0f;
	const FVector ObstacleSize = ObstacleBounds.GetSize();
	return FVector2D(ObstacleSize.X, ObstacleSize.Y).Size() * 0.5f
		+ GetProbeStandOffDistance(Settings)
		+ SidewaysPointAllowance;
}

bool FCoverFinderAlgorithms::AppendThinObstacleSamples(
	const FBox& CollisionBounds,
	const FCoverFinderSettingsSnapshot& Settings,
	TArray<FCoverFocusedSample>& OutSamples)
{
	constexpr float FullCircleDegrees = 360.0f;
	if (not GetIsThinObstacle(CollisionBounds, Settings) || Settings.ThinObstacleRingSampleCount <= 0)
	{
		return false;
	}
	const FVector ObstacleSize = CollisionBounds.GetSize();
	// The corners of a square post reach further out than its sides; the ring clears them as well.
	const float RingRadius = FVector2D(ObstacleSize.X, ObstacleSize.Y).Size() * 0.5f
		+ GetProbeStandOffDistance(Settings);
	const FVector ObstacleBase(CollisionBounds.GetCenter().X, CollisionBounds.GetCenter().Y, CollisionBounds.Min.Z);
	for (int32 SampleIndex = 0; SampleIndex < Settings.ThinObstacleRingSampleCount; ++SampleIndex)
	{
		const float AngleRadians = FMath::DegreesToRadians(
			FullCircleDegrees * static_cast<float>(SampleIndex) /
			static_cast<float>(Settings.ThinObstacleRingSampleCount));
		FCoverFocusedSample& Sample = OutSamples.AddDefaulted_GetRef();
		Sample.Location = ObstacleBase + FVector(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f) * RingRadius;
		Sample.AimLocation = ObstacleBase;
		// Picks OpenFramePointsPerObstacle of the ring's probes at even steps around it. An object that is only
		// ringed for prone cover is too low to crouch behind, frame or not.
		Sample.bMayFindOpenFrameCover = ObstacleSize.Z >= Settings.MinimumCrouchCoverHeight &&
			(SampleIndex * Settings.OpenFramePointsPerObstacle) %
			Settings.ThinObstacleRingSampleCount < Settings.OpenFramePointsPerObstacle;
	}
	return true;
}

bool FCoverFinderAlgorithms::TryBuildReaimedSample(
	const FVector& SampleLocation,
	const FVector& SearchDirection,
	const FCoverTraceObservation& SurfaceTrace,
	const FCoverFinderSettingsSnapshot& Settings,
	FCoverFocusedSample& OutSample)
{
	const FVector ProbeDirection = SearchDirection.GetSafeNormal2D();
	if (not SurfaceTrace.bBlockingHit || ProbeDirection.IsNearlyZero())
	{
		return false;
	}
	const FVector SurfaceNormal = BuildCoverNormal(SurfaceTrace, ProbeDirection);
	if (FVector::DotProduct(SurfaceNormal, -ProbeDirection) >= RTSCoverFinderConstants::MinimumStandingFaceAlignment)
	{
		return false;
	}
	const FVector HitLocation = SampleLocation + ProbeDirection * SurfaceTrace.Distance;
	OutSample.Location = HitLocation + SurfaceNormal * GetProbeStandOffDistance(Settings);
	OutSample.AimLocation = HitLocation;
	return true;
}

bool FCoverFinderAlgorithms::TryGetSocketCoverType(
	const FString& SocketName,
	const FCoverSocketNameParts& NameParts,
	ERTSCoverType& OutCoverType)
{
	const TPair<const FString*, ERTSCoverType> PartsPerType[] =
	{
		{&NameParts.Trench, ERTSCoverType::TrenchStandUp},
		{&NameParts.Crouch, ERTSCoverType::Crouch},
		{&NameParts.StandingLeft, ERTSCoverType::StandingLeft},
		{&NameParts.StandingRight, ERTSCoverType::StandingRight},
		{&NameParts.Prone, ERTSCoverType::Prone}
	};
	int32 LongestMatchLength = 0;
	for (const TPair<const FString*, ERTSCoverType>& PartForType : PartsPerType)
	{
		const FString& NamePart = *PartForType.Key;
		// The longest part is the most specific one: "standing_right" before "standing".
		if (NamePart.Len() > LongestMatchLength && SocketName.Contains(NamePart, ESearchCase::IgnoreCase))
		{
			LongestMatchLength = NamePart.Len();
			OutCoverType = PartForType.Value;
		}
	}
	return LongestMatchLength > 0;
}

bool FCoverFinderAlgorithms::GetIsSameSurface(
	const FCoverTraceObservation& FirstTrace,
	const FCoverTraceObservation& SecondTrace,
	const FCoverFinderSettingsSnapshot& Settings)
{
	if (not FirstTrace.bBlockingHit || not SecondTrace.bBlockingHit)
	{
		return false;
	}
	if (FirstTrace.BlockingProviderHandle != 0 &&
		SecondTrace.BlockingProviderHandle != 0 &&
		FirstTrace.BlockingProviderHandle != SecondTrace.BlockingProviderHandle)
	{
		return false;
	}
	if (FMath::Abs(FirstTrace.Distance - SecondTrace.Distance) > Settings.SurfaceDistanceTolerance)
	{
		return false;
	}

	const FVector FirstHorizontalNormal = FirstTrace.ImpactNormal.GetSafeNormal2D();
	const FVector SecondHorizontalNormal = SecondTrace.ImpactNormal.GetSafeNormal2D();
	if (FirstHorizontalNormal.IsNearlyZero() || SecondHorizontalNormal.IsNearlyZero())
	{
		return true;
	}
	return FVector::DotProduct(FirstHorizontalNormal, SecondHorizontalNormal)
		>= RTSCoverFinderConstants::SurfaceNormalSimilarity;
}

FVector FCoverFinderAlgorithms::BuildCoverNormal(
	const FCoverTraceObservation& SurfaceTrace,
	const FVector& SearchDirection)
{
	FVector CoverNormal = SurfaceTrace.ImpactNormal.GetSafeNormal2D();
	if (CoverNormal.IsNearlyZero())
	{
		CoverNormal = -SearchDirection.GetSafeNormal2D();
	}
	if (FVector::DotProduct(CoverNormal, -SearchDirection) < 0.0f)
	{
		CoverNormal *= -1.0f;
	}
	return CoverNormal.GetSafeNormal();
}

FCoverFinderWorker::~FCoverFinderWorker()
{
	StopAndWait();
}

bool FCoverFinderWorker::Start()
{
	if (M_Thread != nullptr)
	{
		return true;
	}

	M_WorkAvailableEvent = FPlatformProcess::GetSynchEventFromPool(false);
	if (M_WorkAvailableEvent == nullptr)
	{
		return false;
	}

	bM_StopRequested = false;
	M_Thread = FRunnableThread::Create(this, TEXT("RTSCoverFinderWorker"));
	if (M_Thread != nullptr)
	{
		return true;
	}

	FPlatformProcess::ReturnSynchEventToPool(M_WorkAvailableEvent);
	M_WorkAvailableEvent = nullptr;
	return false;
}

void FCoverFinderWorker::StopAndWait()
{
	Stop();
	if (M_Thread != nullptr)
	{
		M_Thread->WaitForCompletion();
		delete M_Thread;
		M_Thread = nullptr;
	}
	if (M_WorkAvailableEvent != nullptr)
	{
		FPlatformProcess::ReturnSynchEventToPool(M_WorkAvailableEvent);
		M_WorkAvailableEvent = nullptr;
	}
}

bool FCoverFinderWorker::Init()
{
	return true;
}

uint32 FCoverFinderWorker::Run()
{
	while (not bM_StopRequested)
	{
		bool bProcessedWork = false;
		FWorkerRequest Request;
		while (M_RequestQueue.Dequeue(Request))
		{
			ProcessRequest(MoveTemp(Request));
			bProcessedWork = true;
		}

		if (not bProcessedWork && M_WorkAvailableEvent != nullptr)
		{
			M_WorkAvailableEvent->Wait();
		}
	}
	return 0;
}

void FCoverFinderWorker::Stop()
{
	bM_StopRequested = true;
	TriggerWorker();
}

void FCoverFinderWorker::EnqueuePlanRequest(
	const uint64 Generation,
	TArray<FBox>&& TileBounds,
	const FCoverFinderSettingsSnapshot& Settings)
{
	FWorkerRequest Request;
	Request.Type = EWorkerRequestType::BuildPlan;
	Request.Generation = Generation;
	Request.Settings = Settings;
	Request.TileBounds = MoveTemp(TileBounds);
	M_RequestQueue.Enqueue(MoveTemp(Request));
	TriggerWorker();
}

void FCoverFinderWorker::EnqueueObservationChunk(
	const uint64 Generation,
	TArray<FCoverProbeObservation>&& Observations)
{
	FWorkerRequest Request;
	Request.Type = EWorkerRequestType::ProcessObservations;
	Request.Generation = Generation;
	Request.Observations = MoveTemp(Observations);
	M_RequestQueue.Enqueue(MoveTemp(Request));
	TriggerWorker();
}

void FCoverFinderWorker::EnqueueCandidateChunk(const uint64 Generation, TArray<FRTSCoverPoint>&& Candidates)
{
	FWorkerRequest Request;
	Request.Type = EWorkerRequestType::AppendCandidates;
	Request.Generation = Generation;
	Request.Candidates = MoveTemp(Candidates);
	M_RequestQueue.Enqueue(MoveTemp(Request));
	TriggerWorker();
}

void FCoverFinderWorker::EnqueueFinalizeRequest(const uint64 Generation)
{
	FWorkerRequest Request;
	Request.Type = EWorkerRequestType::Finalize;
	Request.Generation = Generation;
	M_RequestQueue.Enqueue(MoveTemp(Request));
	TriggerWorker();
}

bool FCoverFinderWorker::DequeueSamplePlan(FCoverSamplePlanResult& OutResult)
{
	return M_SamplePlanQueue.Dequeue(OutResult);
}

bool FCoverFinderWorker::DequeueGenerationResult(FCoverGenerationResult& OutResult)
{
	return M_GenerationResultQueue.Dequeue(OutResult);
}

void FCoverFinderWorker::TriggerWorker()
{
	if (M_WorkAvailableEvent != nullptr)
	{
		M_WorkAvailableEvent->Trigger();
	}
}

void FCoverFinderWorker::ProcessRequest(FWorkerRequest&& Request)
{
	switch (Request.Type)
	{
	case EWorkerRequestType::BuildPlan:
		ProcessBuildPlan(MoveTemp(Request));
		return;
	case EWorkerRequestType::ProcessObservations:
		ProcessObservations(MoveTemp(Request));
		return;
	case EWorkerRequestType::AppendCandidates:
		ProcessAppendCandidates(MoveTemp(Request));
		return;
	case EWorkerRequestType::Finalize:
		ProcessFinalize(Request);
		return;
	default:
		return;
	}
}

void FCoverFinderWorker::ProcessBuildPlan(FWorkerRequest&& Request)
{
	M_ActiveGeneration = Request.Generation;
	M_ActiveSettings = Request.Settings;
	M_ActiveRawCandidates.Reset();
	M_ActiveWorkerSeconds = 0.0;

	const double StartSeconds = FPlatformTime::Seconds();
	FCoverSamplePlanResult Result;
	Result.Generation = Request.Generation;
	Result.SampleLocations = FCoverFinderAlgorithms::BuildSamplePlan(
		Request.TileBounds,
		Request.Settings,
		Result.bWasTruncated);
	const double WorkerSeconds = FPlatformTime::Seconds() - StartSeconds;
	M_ActiveWorkerSeconds += WorkerSeconds;
	Result.WorkerMilliseconds = static_cast<float>(WorkerSeconds * 1000.0);
	M_SamplePlanQueue.Enqueue(MoveTemp(Result));
}

void FCoverFinderWorker::ProcessObservations(FWorkerRequest&& Request)
{
	if (Request.Generation != M_ActiveGeneration)
	{
		return;
	}

	const double StartSeconds = FPlatformTime::Seconds();
	FCoverFinderAlgorithms::AppendClassifiedCandidates(
		Request.Observations,
		M_ActiveSettings,
		M_ActiveRawCandidates);
	M_ActiveWorkerSeconds += FPlatformTime::Seconds() - StartSeconds;
}

void FCoverFinderWorker::ProcessAppendCandidates(FWorkerRequest&& Request)
{
	if (Request.Generation != M_ActiveGeneration)
	{
		return;
	}
	M_ActiveRawCandidates.Append(MoveTemp(Request.Candidates));
}

void FCoverFinderWorker::ProcessFinalize(const FWorkerRequest& Request)
{
	if (Request.Generation != M_ActiveGeneration)
	{
		return;
	}

	const int32 RawCandidateCount = M_ActiveRawCandidates.Num();
	const double StartSeconds = FPlatformTime::Seconds();
	TArray<FRTSCoverPoint> CoverPoints = FCoverFinderAlgorithms::FinalizeCandidates(
		MoveTemp(M_ActiveRawCandidates),
		M_ActiveSettings);
	M_ActiveWorkerSeconds += FPlatformTime::Seconds() - StartSeconds;

	FCoverGenerationResult Result;
	Result.Generation = Request.Generation;
	Result.CoverPoints = MoveTemp(CoverPoints);
	Result.RawCandidateCount = RawCandidateCount;
	Result.WorkerMilliseconds = static_cast<float>(M_ActiveWorkerSeconds * 1000.0);
	M_GenerationResultQueue.Enqueue(MoveTemp(Result));
	M_ActiveGeneration = 0;
	M_ActiveWorkerSeconds = 0.0;
}
