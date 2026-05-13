#include "StochasticHelpers.h"

#include "DrawDebugHelpers.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyNavigationAIComponent/EnemyNavigationAIComponent.h"
#include "RTS_Survival/Enemy/StrategicAI/BlackboardIdleUnitsResult/FBlackboardIdleUnitsResult.h"
#include "RTS_Survival/Enemy/StrategicAI/StochasticDecisionTree/StochasticDecisionTree.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicActions/StrategicActions.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"

namespace
{
	struct FClosestLocationPair
	{
		FVector FirstLocation = FVector::ZeroVector;
		FVector SecondLocation = FVector::ZeroVector;
	};

	/**
	 * @brief Generates a random value in range [0, MaxValue].
	 *
	 * If deterministic mode is enabled, combines Seed and Time into a reproducible random stream.
	 */
	float GetRandomValue(
		const bool bUseSeed,
		const float Seed,
		const float Time,
		const float MaxValue)
	{
		if (not bUseSeed)
		{
			return FMath::FRandRange(0.0f, MaxValue);
		}

		const int32 CombinedSeed =
			static_cast<int32>(Seed * 1000.0f) ^
			static_cast<int32>(Time * 1000.0f);

		FRandomStream RandomStream(CombinedSeed);

		return RandomStream.FRandRange(0.0f, MaxValue);
	}

	/**
	 * @brief Generic weighted picker for pointer arrays.
	 *
	 * @tparam T Type of object.
	 * @param Options Array of pointers.
	 * @param GetScore Lambda returning score for an option.
	 * @param bUseSeed Deterministic toggle.
	 * @param Seed Seed value.
	 * @param Time Time value.
	 * @return Selected pointer or nullptr.
	 */
	template <typename T>
	T* PickWeightedInternal(
		const TArray<T*>& Options,
		const TFunctionRef<float(T&)> GetScore,
		const bool bUseSeed,
		const float Seed,
		const float Time)
	{
		float TotalScore = 0.0f;

		for (T* Option : Options)
		{
			if (not Option)
			{
				continue;
			}

			const float Score = FMath::Max(0.0f, GetScore(*Option));
			TotalScore += Score;
		}

		if (TotalScore <= 0.0f)
		{
			return nullptr;
		}

		const float RandomValue = GetRandomValue(bUseSeed, Seed, Time, TotalScore);

		float RunningScore = 0.0f;

		for (T* Option : Options)
		{
			if (not Option)
			{
				continue;
			}

			const float Score = FMath::Max(0.0f, GetScore(*Option));
			RunningScore += Score;

			if (RandomValue <= RunningScore)
			{
				return Option;
			}
		}

		return nullptr;
	}

	TArray<FVector> GetUniqueLocations(const TArray<FVector>& Locations)
	{
		TArray<FVector> UniqueLocations;
		UniqueLocations.Reserve(Locations.Num());

		TSet<FVector> SeenLocations;
		SeenLocations.Reserve(Locations.Num());

		for (const FVector& EachLocation : Locations)
		{
			if (SeenLocations.Contains(EachLocation))
			{
				continue;
			}

			SeenLocations.Add(EachLocation);
			UniqueLocations.Add(EachLocation);
		}

		return UniqueLocations;
	}

	int32 FindClosestLocationIndex(const TArray<FVector>& Locations, const int32 SourceIndex)
	{
		if (not Locations.IsValidIndex(SourceIndex))
		{
			return INDEX_NONE;
		}

		int32 ClosestLocationIndex = INDEX_NONE;
		float ClosestDistanceSquared = 0.0f;

		for (int32 CandidateIndex = 0; CandidateIndex < Locations.Num(); ++CandidateIndex)
		{
			if (CandidateIndex == SourceIndex)
			{
				continue;
			}

			const float CandidateDistanceSquared =
				FVector::DistSquared(Locations[SourceIndex], Locations[CandidateIndex]);

			if (ClosestLocationIndex == INDEX_NONE || CandidateDistanceSquared < ClosestDistanceSquared)
			{
				ClosestLocationIndex = CandidateIndex;
				ClosestDistanceSquared = CandidateDistanceSquared;
			}
		}

		return ClosestLocationIndex;
	}

	TArray<FClosestLocationPair> BuildClosestLocationPairs(const TArray<FVector>& Locations)
	{
		TArray<FClosestLocationPair> ClosestPairs;
		ClosestPairs.Reserve(Locations.Num());

		for (int32 LocationIndex = 0; LocationIndex < Locations.Num(); ++LocationIndex)
		{
			const int32 ClosestLocationIndex = FindClosestLocationIndex(Locations, LocationIndex);
			if (ClosestLocationIndex == INDEX_NONE)
			{
				continue;
			}

			ClosestPairs.Add(
				{
					Locations[LocationIndex],
					Locations[ClosestLocationIndex]
				});
		}

		return ClosestPairs;
	}
	bool ArePathPointsNearlyEqual(const FVector& FirstPoint, const FVector& SecondPoint)
	{
		constexpr float DuplicatePointToleranceSquared = 1.f;
		return FVector::DistSquared(FirstPoint, SecondPoint) <= DuplicatePointToleranceSquared;
	}

	void AddUniquePathPoint(TArray<FVector>& OutPathPoints, const FVector& PathPoint)
	{
		if (OutPathPoints.IsEmpty())
		{
			OutPathPoints.Add(PathPoint);
			return;
		}

		if (ArePathPointsNearlyEqual(OutPathPoints.Last(), PathPoint))
		{
			return;
		}

		OutPathPoints.Add(PathPoint);
	}

	FVector ProjectPathPointOrUseOriginal(const FStochasticPathFindingParams& Params, const FVector& PathPoint)
	{
		if (not IsValid(Params.NavComp))
		{
			return PathPoint;
		}

		FVector ProjectedLocation = FVector::ZeroVector;
		if (Params.NavComp->GetNavigablePoint(
			PathPoint,
			Params.Settings.ProjectionScale,
			ProjectedLocation,
			EOnProjectionFailedStrategy::LookAtDoubleExtent))
		{
			return ProjectedLocation;
		}

		return PathPoint;
	}

	TArray<FVector> BuildSourcePathPoints(
		const FStochasticPathFindingParams& Params,
		const UNavigationPath& NavigationPath)
	{
		TArray<FVector> SourcePathPoints;
		SourcePathPoints.Reserve(NavigationPath.PathPoints.Num() + 2);
		if (Params.bUseStartLocationAsFirstPathPoint)
		{
			AddUniquePathPoint(SourcePathPoints, Params.StartLocation);
		}

		for (const FVector& PathPoint : NavigationPath.PathPoints)
		{
			AddUniquePathPoint(SourcePathPoints, PathPoint);
		}

		AddUniquePathPoint(SourcePathPoints, Params.TargetLocation);
		return SourcePathPoints;
	}

	void AppendProjectedPathPoint(
		const FStochasticPathFindingParams& Params,
		const FVector& PathPoint,
		TArray<FVector>& OutPathPoints)
	{
		AddUniquePathPoint(OutPathPoints, ProjectPathPointOrUseOriginal(Params, PathPoint));
	}

	TArray<FVector> ResamplePathPoints(
		const FStochasticPathFindingParams& Params,
		const UNavigationPath& NavigationPath)
	{
		const TArray<FVector> SourcePathPoints = BuildSourcePathPoints(Params, NavigationPath);
		TArray<FVector> ResampledPathPoints;
		if (SourcePathPoints.Num() < 2)
		{
			return ResampledPathPoints;
		}

		const float SafeAveragePointDistance = FMath::Max(Params.Settings.AveragePathPointDistance, 100.f);
		const int32 MaxIntermediatePathPoints = FMath::Max(Params.Settings.MaxIntermediatePathPoints, 0);
		float DistanceUntilNextPoint = SafeAveragePointDistance;

		ResampledPathPoints.Reserve(MaxIntermediatePathPoints + 2);
		AddUniquePathPoint(ResampledPathPoints, SourcePathPoints[0]);

		for (int32 PointIndex = 1; PointIndex < SourcePathPoints.Num(); ++PointIndex)
		{
			FVector SegmentStart = SourcePathPoints[PointIndex - 1];
			const FVector SegmentEnd = SourcePathPoints[PointIndex];
			float SegmentLength = FVector::Distance(SegmentStart, SegmentEnd);

			while (SegmentLength >= DistanceUntilNextPoint && ResampledPathPoints.Num() - 1 < MaxIntermediatePathPoints)
			{
				const FVector Direction = (SegmentEnd - SegmentStart).GetSafeNormal();
				const FVector NewPathPoint = SegmentStart + Direction * DistanceUntilNextPoint;
				AppendProjectedPathPoint(Params, NewPathPoint, ResampledPathPoints);

				SegmentStart = NewPathPoint;
				SegmentLength = FVector::Distance(SegmentStart, SegmentEnd);
				DistanceUntilNextPoint = SafeAveragePointDistance;
			}

			DistanceUntilNextPoint -= SegmentLength;
			if (DistanceUntilNextPoint <= KINDA_SMALL_NUMBER)
			{
				DistanceUntilNextPoint = SafeAveragePointDistance;
			}
		}

		if (ResampledPathPoints.Num() > 1 &&
			FVector::Distance(
				ResampledPathPoints.Last(),
				Params.TargetLocation) < Params.Settings.MinimumFinalSegmentDistance)
		{
			ResampledPathPoints.RemoveAt(ResampledPathPoints.Num() - 1, 1, EAllowShrinking::No);
		}

		AddUniquePathPoint(ResampledPathPoints, Params.TargetLocation);
		return ResampledPathPoints;
	}

	void DebugDrawPathFindingResult(const FStochasticPathFindingParams& Params, const TArray<FVector>& PathPoints)
	{
		if constexpr (EnemyAISettings::Debugging::StochasticPathFindingDebugging)
		{
			if (PathPoints.IsEmpty() || not IsValid(Params.WorldContextObject))
			{
				return;
			}

			UWorld* World = Params.WorldContextObject->GetWorld();
			if (not IsValid(World))
			{
				return;
			}

			const FVector DebugOffset(0.f, 0.f, Params.Settings.DebugDrawZOffset);
			for (int32 PathPointIndex = 0; PathPointIndex < PathPoints.Num(); ++PathPointIndex)
			{
				const FVector DebugLocation = PathPoints[PathPointIndex] + DebugOffset;
				DrawDebugSphere(
					World,
					DebugLocation,
					Params.Settings.DebugSphereRadius,
					Params.Settings.DebugSphereSegments,
					FColor::Orange,
					false,
					Params.Settings.DebugDrawDuration);

				if (not PathPoints.IsValidIndex(PathPointIndex + 1))
				{
					continue;
				}

				DrawDebugDirectionalArrow(
					World,
					DebugLocation,
					PathPoints[PathPointIndex + 1] + DebugOffset,
					Params.Settings.DebugSphereRadius,
					FColor::Orange,
					false,
					Params.Settings.DebugDrawDuration);
			}
		}
	}

}

namespace StochasticHelpers
{
	FStrategicAIAction* PickStrategicAction(
		const TArray<FStrategicAIAction*>& ActionDefinitions,
		const bool bUseSeed,
		const float Seed,
		const float Time)
	{
		if (ActionDefinitions.IsEmpty())
		{
			return nullptr;
		}

		return PickWeightedInternal<FStrategicAIAction>(
			ActionDefinitions,
			[](FStrategicAIAction& Action)
			{
				return Action.GetScore();
			},
			bUseSeed,
			Seed,
			Time);
	}

	UStrategicAISubAction* PickSubAction(
		const TArray<UStrategicAISubAction*>& SubActions,
		const bool bUseSeed,
		const float Seed,
		const float Time)
	{
		if (SubActions.IsEmpty())
		{
			return nullptr;
		}

		return PickWeightedInternal<UStrategicAISubAction>(
			SubActions,
			[](UStrategicAISubAction& SubAction)
			{
				return SubAction.GetScore();
			},
			bUseSeed,
			Seed,
			Time);
	}

	FVector PickRandomLocation(const TArray<FVector>& Locations)
	{
		if (Locations.Num() <= 0)
		{
			return FVector::ZeroVector;
		}
		const int32 Index = FMath::RandRange(0, Locations.Num() - 1);
		return Locations[Index];
	}

	TArray<FVector> PickRandomMaxLocations(const TArray<FVector>& Locations, const int32 MaxLocations)
	{
		if (MaxLocations <= 0)
		{
			return TArray<FVector>();
		}

		const TArray<FVector> UniqueLocations = GetUniqueLocations(Locations);
		if (UniqueLocations.IsEmpty())
		{
			return TArray<FVector>();
		}

		const int32 LocationCountToPick = FMath::Min(MaxLocations, UniqueLocations.Num());
		TArray<int32> RemainingLocationIndices;
		RemainingLocationIndices.Reserve(UniqueLocations.Num());

		for (int32 LocationIndex = 0; LocationIndex < UniqueLocations.Num(); ++LocationIndex)
		{
			RemainingLocationIndices.Add(LocationIndex);
		}

		TArray<FVector> PickedLocations;
		PickedLocations.Reserve(LocationCountToPick);

		for (int32 PickedCount = 0; PickedCount < LocationCountToPick; ++PickedCount)
		{
			const int32 RandomRemainingIndex =
				FMath::RandRange(0, RemainingLocationIndices.Num() - 1);
			const int32 PickedLocationIndex = RemainingLocationIndices[RandomRemainingIndex];
			PickedLocations.Add(UniqueLocations[PickedLocationIndex]);
			RemainingLocationIndices.RemoveAtSwap(RandomRemainingIndex, 1, EAllowShrinking::No);
		}

		return PickedLocations;
	}

	TArray<FVector> PickRandomClosestPair(const TArray<FVector>& Locations)
	{
		if (Locations.IsEmpty())
		{
			return TArray<FVector>();
		}

		if (Locations.Num() == 1)
		{
			return {Locations[0]};
		}

		const TArray<FClosestLocationPair> ClosestPairs = BuildClosestLocationPairs(Locations);
		if (ClosestPairs.IsEmpty())
		{
			return TArray<FVector>();
		}

		const int32 RandomPairIndex = FMath::RandRange(0, ClosestPairs.Num() - 1);
		const FClosestLocationPair& PickedPair = ClosestPairs[RandomPairIndex];

		TArray<FVector> PickedLocations;
		PickedLocations.Reserve(2);
		PickedLocations.Add(PickedPair.FirstLocation);
		PickedLocations.Add(PickedPair.SecondLocation);
		return PickedLocations;
	}

	TArray<FVector> PickPairOfPointsClosestTo(const FVector& OriginalPoint, const TArray<FVector>& Locations)
	{
		if (Locations.IsEmpty())
		{
			return TArray<FVector>();
		}

		if (Locations.Num() == 1)
		{
			return {Locations[0]};
		}

		TArray<FVector> SortedLocations = Locations;
		SortArrayByDistanceToLocation(SortedLocations, OriginalPoint);

		TArray<FVector> PickedLocations;
		PickedLocations.Reserve(2);
		PickedLocations.Add(SortedLocations[0]);
		PickedLocations.Add(SortedLocations[1]);
		return PickedLocations;
	}

	TArray<FVector> ExhaustivePick(const TArray<FVector>& Locations, const int32 MaxPick)
	{
		return PickRandomMaxLocations(Locations, MaxPick);
	}

	TArray<FVector> BuildNavigablePathPoints(const FStochasticPathFindingParams& Params)
	{
		TArray<FVector> PathPoints;
		if (not IsValid(Params.WorldContextObject))
		{
			return PathPoints;
		}

		UObject* WorldContextObject = const_cast<UObject*>(Params.WorldContextObject);
		UNavigationPath* NavigationPath = UNavigationSystemV1::FindPathToLocationSynchronously(
			WorldContextObject,
			Params.StartLocation,
			Params.TargetLocation);
		if (not IsValid(NavigationPath) || NavigationPath->PathPoints.Num() < 2)
		{
			return PathPoints;
		}

		PathPoints = ResamplePathPoints(Params, *NavigationPath);
		DebugDrawPathFindingResult(Params, PathPoints);
		return PathPoints;
	}

	bool CanProjectNavigable_BulkLocation(const UEnemyNavigationAIComponent* NavComp, const FVector& BulkLocation,
	                                      FVector& OutProjectedLocation)
	{
		return NavComp->GetNavigablePoint(BulkLocation, 5.0,
		                                  OutProjectedLocation,
		                                  EOnProjectionFailedStrategy::LookAtDoubleExtent);
	}

	bool CanProjectNavigable_AverageLocationAttacker(const UEnemyNavigationAIComponent* NavComp,
	                                                 const FVector& AvgAttackerLoc, FVector& OutProjectedLocation)
	{
		return NavComp->GetNavigablePoint(AvgAttackerLoc, 8.0,
		                                  OutProjectedLocation,
		                                  EOnProjectionFailedStrategy::LookAtDoubleExtent);
	}

	bool CanProjectNavigable_PlayerHQLocation(const UEnemyNavigationAIComponent* NavComp,
	                                            const FVector& PlayerHQLocation, FVector& OutProjectedLocation)
	{
		constexpr float ProjectionScale = 3.f;
		return NavComp->GetNavigablePoint(PlayerHQLocation, ProjectionScale,
		                                  OutProjectedLocation,
		                                  EOnProjectionFailedStrategy::LookAtDoubleExtent);
	}

	bool CanProjectNavigable_PlayerResourceBuildingLocation(const UEnemyNavigationAIComponent* NavComp,
	                                                        const FVector& PlayerResourceBuildingLocation,
	                                                        FVector& OutProjectedLocation)
	{
		constexpr float ProjectionScale = 3.f;
		return NavComp->GetNavigablePoint(PlayerResourceBuildingLocation, ProjectionScale,
		                                  OutProjectedLocation,
		                                  EOnProjectionFailedStrategy::LookAtDoubleExtent);
	}

	bool CanProjectNavigable_AttackSpecificPoint(const UEnemyNavigationAIComponent* NavComp,
	                                             const FVector& SpecificPoint, FVector& OutProjectedLocation)
	{
		constexpr float ProjectionScale = 3.f;
		return NavComp->GetNavigablePoint(SpecificPoint, ProjectionScale,
		                                  OutProjectedLocation,
		                                  EOnProjectionFailedStrategy::LookAtDoubleExtent);
	}

	bool CanProjectNavigable_FlankLocation(const UEnemyNavigationAIComponent* NavComp, const FVector& FlankLocation,
	                                       FVector& OutProjectedLocation)
	{
		// Keep this projection tight so async-generated flank positions stay close to their tactical intent.
		constexpr float ProjectionScale = 1.f;
		// No failed strategy to prevent points from being too far from their original non-projected location.
		return NavComp->GetNavigablePoint(FlankLocation, ProjectionScale,
		                                  OutProjectedLocation,
		                                  EOnProjectionFailedStrategy::None);
	}

	bool CanProjectNavigable_AveragePickedUnitLocation(const UEnemyNavigationAIComponent* NavComp,
	                                                   const FVector& AverageUnitLocation,
	                                                   FVector& OutProjectedLocation)
	{
		// Try get this point on default nav cost first.
		// Do not make this to high as it risk overshooting.
		constexpr float ProjectionScaleDefault = 3.f;
		if (not NavComp->GetNavPointDefaultCosts(AverageUnitLocation, ProjectionScaleDefault, OutProjectedLocation,
		                                         EOnProjectionFailedStrategy::LookAtDoubleExtent))
		{
			return NavComp->GetNavigablePoint(AverageUnitLocation, 15.0,
			                                  OutProjectedLocation,
			                                  EOnProjectionFailedStrategy::LookAtDoubleExtent);
		}
		return true;
	}

	FVector GetAverageLocationPickedBlackboardUnits(const FBlackboardIdleUnitsResult& PickedBlackboardUnits)
	{
		const int32 TankCount = PickedBlackboardUnits.TankMasters.Num();
		const int32 SquadCount = PickedBlackboardUnits.SquadControllers.Num();
		const int32 TotalUnitCount = TankCount + SquadCount;
		if (TotalUnitCount <= 0)
		{
			return FVector::ZeroVector;
		}

		FVector SummedLocations = FVector::ZeroVector;

		for (const TObjectPtr<ATankMaster>& TankMaster : PickedBlackboardUnits.TankMasters)
		{
			SummedLocations += TankMaster->GetActorLocation();
		}

		for (const TObjectPtr<ASquadController>& SquadController : PickedBlackboardUnits.SquadControllers)
		{
			SummedLocations += SquadController->GetActorLocation();
		}

		const float InverseUnitCount = 1.0f / static_cast<float>(TotalUnitCount);
		return SummedLocations * InverseUnitCount;
	}

	void SortArrayByDistanceToLocation(TArray<FVector>& OutLocations, const FVector& TargetLocation)
	{
		// compute the squared distance to the target location for each location and sort based on that
		OutLocations.Sort([&](const FVector& A, const FVector& B)
		{
			const float DistASq = FVector::DistSquared(A, TargetLocation);
			const float DistBSq = FVector::DistSquared(B, TargetLocation);
			return DistASq < DistBSq;
		});
	}
}
