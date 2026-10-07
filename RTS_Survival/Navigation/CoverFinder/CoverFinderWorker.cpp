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
	const FCoverFinderSettingsSnapshot& Settings)
{
	RawCandidates.Sort(&CoverFinderWorkerPrivate::SortCandidates);
	TArray<FRTSCoverPoint> AcceptedCandidates;
	AcceptedCandidates.Reserve(RawCandidates.Num());
	TMap<FIntPoint, TArray<int32>> SpatialBuckets;
	for (FRTSCoverPoint& Candidate : RawCandidates)
	{
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
	return AcceptedCandidates;
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
