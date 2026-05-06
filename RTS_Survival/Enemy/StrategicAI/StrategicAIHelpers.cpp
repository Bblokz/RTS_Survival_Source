#include "StrategicAIHelpers.h"

#include "RTS_Survival/Game/GameState/GameUnitManager/AsyncUnitDetailedState/AsyncUnitDetailedState.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

namespace StrategicAIHelperConstants
{
	constexpr int32 PlayerOwningId = 1;
	constexpr int32 EnemyOwningId = 2;
	constexpr int32 MaxRetreatGroups = 3;
	constexpr float FlankLeftYawOffset = -90.f;
	constexpr float FlankRightYawOffset = 90.f;
}

namespace StrategicAIHelperUtilities
{
	bool GetIsNearZeroStrategicLocation(const FVector& Location)
	{
		return Location.IsNearlyZero();
	}

	void RemoveNearZeroStrategicLocations(TArray<FVector>& Locations)
	{
		Locations.RemoveAll([](const FVector& Location)
		{
			return GetIsNearZeroStrategicLocation(Location);
		});
	}

	void RemoveNearZeroStrategicActorLocations(TArray<FWeakActorLocations>& ActorLocations)
	{
		for (FWeakActorLocations& ActorLocation : ActorLocations)
		{
			RemoveNearZeroStrategicLocations(ActorLocation.Locations);
		}

		ActorLocations.RemoveAll([](const FWeakActorLocations& ActorLocation)
		{
			return ActorLocation.Locations.IsEmpty();
		});
	}

	void RemoveNearZeroStrategicRetreatLocations(FDamagedTanksRetreatGroup& RetreatGroup)
	{
		RemoveNearZeroStrategicActorLocations(RetreatGroup.HazmatsWithFormationLocations);
	}

	void RemoveNearZeroStrategicAttackLocations(TArray<FPlayerAttackLocationEvaluation>& AttackLocations)
	{
		AttackLocations.RemoveAll([](const FPlayerAttackLocationEvaluation& AttackLocation)
		{
			return GetIsNearZeroStrategicLocation(AttackLocation.LocationUnderAttack);
		});
	}

	void RemoveNearZeroStrategicPlayerBulks(TArray<FPlayerUnitBulkLocation>& PlayerUnitBulks)
	{
		PlayerUnitBulks.RemoveAll([](const FPlayerUnitBulkLocation& PlayerUnitBulk)
		{
			return GetIsNearZeroStrategicLocation(PlayerUnitBulk.BulkLocation);
		});
	}

}

FResultEnemyBaseClusters FStrategicAIHelpers::BuildEnemyBaseClustersResult(
	const FFindEnemyBaseClusters& Request,
	const TArray<FAsyncDetailedUnitState>& DetailedUnitStates)
{
	FResultEnemyBaseClusters Result;
	Result.RequestID = Request.RequestID;

	const TSet<EBuildingExpansionType> CoreTypes(Request.CoreBuildingTypes);
	const TSet<EBuildingExpansionType> SatelliteTypes(Request.SatelliteBuildingTypes);
	if (CoreTypes.IsEmpty())
	{
		return Result;
	}

	TArray<FVector2D> CorePoints;
	TArray<FVector2D> SatellitePoints;
	for (const FAsyncDetailedUnitState& UnitState : DetailedUnitStates)
	{
		if (UnitState.OwningPlayer != StrategicAIHelperConstants::EnemyOwningId || UnitState.UnitType != EAllUnitType::UNType_BuildingExpansion)
		{
			continue;
		}

		const EBuildingExpansionType BuildingType = static_cast<EBuildingExpansionType>(UnitState.UnitSubtypeRaw);
		const FVector2D XY(UnitState.UnitLocation.X, UnitState.UnitLocation.Y);
		if (CoreTypes.Contains(BuildingType))
		{
			CorePoints.Add(XY);
		}
		else if (SatelliteTypes.Contains(BuildingType))
		{
			SatellitePoints.Add(XY);
		}
	}

	const float EpsSq = FMath::Square(FMath::Max(0.f, Request.CoreClusterDistanceXY));
	const int32 MinCoreNeighbors = FMath::Max(1, Request.MinCoreNeighbors);
	TArray<int32> Labels;
	Labels.Init(-1, CorePoints.Num());
	int32 CurrentCluster = 0;
	for (int32 PointIndex = 0; PointIndex < CorePoints.Num(); ++PointIndex)
	{
		if (Labels[PointIndex] != -1)
		{
			continue;
		}
		TArray<int32> Neighbors;
		for (int32 NeighborIndex = 0; NeighborIndex < CorePoints.Num(); ++NeighborIndex)
		{
			if (FVector2D::DistSquared(CorePoints[PointIndex], CorePoints[NeighborIndex]) <= EpsSq)
			{
				Neighbors.Add(NeighborIndex);
			}
		}
		if (Neighbors.Num() < MinCoreNeighbors)
		{
			Labels[PointIndex] = -2;
			continue;
		}
		for (int32 NeighborId : Neighbors)
		{
			Labels[NeighborId] = CurrentCluster;
		}
		++CurrentCluster;
	}

	TArray<TArray<FVector2D>> ClusterPoints;
	ClusterPoints.SetNum(CurrentCluster);
	for (int32 PointIndex = 0; PointIndex < CorePoints.Num(); ++PointIndex)
	{
		if (Labels[PointIndex] >= 0)
		{
			ClusterPoints[Labels[PointIndex]].Add(CorePoints[PointIndex]);
		}
	}

	for (const FVector2D& SatellitePoint : SatellitePoints)
	{
		int32 BestCluster = INDEX_NONE;
		float BestDistSq = EpsSq;
		for (int32 ClusterIndex = 0; ClusterIndex < ClusterPoints.Num(); ++ClusterIndex)
		{
			for (const FVector2D& CorePoint : ClusterPoints[ClusterIndex])
			{
				const float DistanceSq = FVector2D::DistSquared(SatellitePoint, CorePoint);
				if (DistanceSq < BestDistSq)
				{
					BestDistSq = DistanceSq;
					BestCluster = ClusterIndex;
				}
			}
		}
		if (BestCluster != INDEX_NONE)
		{
			ClusterPoints[BestCluster].Add(SatellitePoint);
		}
	}

	struct FBaseScorePoint { float Score; FVector Point; };
	TArray<FBaseScorePoint> ScoredBases;
	for (const TArray<FVector2D>& Cluster : ClusterPoints)
	{
		if (Cluster.Num() < Request.MinTotalBuildingsPerBase)
		{
			continue;
		}
		FVector2D Sum = FVector2D::ZeroVector;
		for (const FVector2D& Point : Cluster)
		{
			Sum += Point;
		}
		const FVector2D Center = Sum / static_cast<float>(Cluster.Num());
		const float Score = static_cast<float>(Cluster.Num());
		if (Score < Request.MinBaseScoreToReturn)
		{
			continue;
		}
		ScoredBases.Add({Score, FVector(Center.X, Center.Y, 0.f)});
	}

	ScoredBases.Sort([](const FBaseScorePoint& Left, const FBaseScorePoint& Right) { return Left.Score > Right.Score; });
	const int32 MaxBases = FMath::Max(0, Request.MaxBasesToReturn);
	const int32 BasesToTake = MaxBases == 0 ? ScoredBases.Num() : FMath::Min(MaxBases, ScoredBases.Num());
	Result.BasePoints.Reserve(BasesToTake);
	for (int32 Index = 0; Index < BasesToTake; ++Index)
	{
		Result.BasePoints.Add(ScoredBases[Index].Point);
	}

	StrategicAIHelperUtilities::RemoveNearZeroStrategicLocations(Result.BasePoints);

	return Result;
}

namespace StrategicAIHelperUtilities
{
	float GetUnitThreatWeight(
		const FAsyncDetailedUnitState& UnitState,
		const FFindLocationsUnderPlayerAttack& Request)
	{
		if (UnitState.UnitType == EAllUnitType::UNType_Squad)
		{
			return Request.SquadThreatScore;
		}

		if (UnitState.UnitType != EAllUnitType::UNType_Tank)
		{
			return 0.f;
		}

		const ETankSubtype TankSubtype = static_cast<ETankSubtype>(UnitState.UnitSubtypeRaw);
		if (Global_GetIsArmoredCar(TankSubtype))
		{
			return Request.ArmoredCarThreatScore;
		}
		if (Global_GetIsLightTank(TankSubtype))
		{
			return Request.LightTankThreatScore;
		}
		if (Global_GetIsMediumTank(TankSubtype))
		{
			return Request.MediumTankThreatScore;
		}
		if (Global_GetIsHeavyTank(TankSubtype))
		{
			return Request.HeavyTankThreatScore;
		}

		return 0.f;
	}

	float GetPlayerBulkWeight(
		const FAsyncDetailedUnitState& UnitState,
		const FFindPlayerUnitBulkLocations& Request)
	{
		if (UnitState.UnitType == EAllUnitType::UNType_Squad)
		{
			return Request.SquadBulkScore;
		}

		if (UnitState.UnitType != EAllUnitType::UNType_Tank)
		{
			return 0.f;
		}

		const ETankSubtype TankSubtype = static_cast<ETankSubtype>(UnitState.UnitSubtypeRaw);
		if (Global_GetIsArmoredCar(TankSubtype))
		{
			return Request.ArmoredCarBulkScore;
		}
		if (Global_GetIsLightTank(TankSubtype))
		{
			return Request.LightTankBulkScore;
		}
		if (Global_GetIsMediumTank(TankSubtype))
		{
			return Request.MediumTankBulkScore;
		}
		if (Global_GetIsHeavyTank(TankSubtype))
		{
			return Request.HeavyTankBulkScore;
		}

		return 0.f;
	}

	struct FPlayerBulkCandidate
	{
		const FAsyncDetailedUnitState* UnitState = nullptr;
		FVector2D LocationXY = FVector2D::ZeroVector;
		float Weight = 0.f;
	};

	struct FPlayerBulkClusterBuilder
	{
		TArray<int32> CandidateIndices;
	};

	TArray<FPlayerBulkCandidate> GatherPlayerBulkCandidates(
		const FFindPlayerUnitBulkLocations& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates)
	{
		TArray<FPlayerBulkCandidate> Candidates;
		for (const FAsyncDetailedUnitState& UnitState : DetailedUnitStates)
		{
			if (UnitState.OwningPlayer != StrategicAIHelperConstants::PlayerOwningId)
			{
				continue;
			}

			const float UnitWeight = GetPlayerBulkWeight(UnitState, Request);
			if (UnitWeight <= 0.f)
			{
				continue;
			}

			FPlayerBulkCandidate Candidate;
			Candidate.UnitState = &UnitState;
			Candidate.LocationXY = FVector2D(UnitState.UnitLocation.X, UnitState.UnitLocation.Y);
			Candidate.Weight = UnitWeight;
			Candidates.Add(Candidate);
		}

		return Candidates;
	}

	void AddUnassignedNeighborCandidates(
		const TArray<FPlayerBulkCandidate>& Candidates,
		const int32 SourceCandidateIndex,
		const float ClusterRadiusSq,
		TArray<int32>& CandidateLabels,
		TArray<int32>& Frontier,
		const int32 ClusterLabel)
	{
		for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
		{
			if (CandidateLabels[CandidateIndex] != INDEX_NONE)
			{
				continue;
			}

			const float DistanceSq = FVector2D::DistSquared(
				Candidates[SourceCandidateIndex].LocationXY,
				Candidates[CandidateIndex].LocationXY);
			if (DistanceSq > ClusterRadiusSq)
			{
				continue;
			}

			CandidateLabels[CandidateIndex] = ClusterLabel;
			Frontier.Add(CandidateIndex);
		}
	}

	FPlayerBulkClusterBuilder BuildPlayerBulkCluster(
		const TArray<FPlayerBulkCandidate>& Candidates,
		const int32 SeedCandidateIndex,
		const float ClusterRadiusSq,
		TArray<int32>& CandidateLabels,
		const int32 ClusterLabel)
	{
		FPlayerBulkClusterBuilder Cluster;
		TArray<int32> Frontier;
		CandidateLabels[SeedCandidateIndex] = ClusterLabel;
		Frontier.Add(SeedCandidateIndex);

		while (not Frontier.IsEmpty())
		{
			const int32 CurrentCandidateIndex = Frontier.Last();
			Frontier.RemoveAt(Frontier.Num() - 1, 1, false);
			Cluster.CandidateIndices.Add(CurrentCandidateIndex);
			AddUnassignedNeighborCandidates(
				Candidates,
				CurrentCandidateIndex,
				ClusterRadiusSq,
				CandidateLabels,
				Frontier,
				ClusterLabel);
		}

		return Cluster;
	}

	TArray<FPlayerBulkClusterBuilder> BuildPlayerBulkClusters(
		const TArray<FPlayerBulkCandidate>& Candidates,
		const float ClusterRadiusXY)
	{
		TArray<FPlayerBulkClusterBuilder> Clusters;
		if (Candidates.IsEmpty())
		{
			return Clusters;
		}

		const float ClusterRadiusSq = FMath::Square(FMath::Max(0.f, ClusterRadiusXY));
		TArray<int32> CandidateLabels;
		CandidateLabels.Init(INDEX_NONE, Candidates.Num());
		for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
		{
			if (CandidateLabels[CandidateIndex] != INDEX_NONE)
			{
				continue;
			}

			const int32 ClusterLabel = Clusters.Num();
			Clusters.Add(BuildPlayerBulkCluster(
				Candidates,
				CandidateIndex,
				ClusterRadiusSq,
				CandidateLabels,
				ClusterLabel));
		}

		return Clusters;
	}

	void AddPlayerTankUnitStateCount(
		FResultPlayerUnitCounts& Result,
		const FAsyncDetailedUnitState& UnitState)
	{
		if (UnitState.UnitType != EAllUnitType::UNType_Tank)
		{
			return;
		}

		const ETankSubtype TankSubtype = static_cast<ETankSubtype>(UnitState.UnitSubtypeRaw);
		if (Global_GetIsArmoredCar(TankSubtype))
		{
			Result.PlayerArmoredCars++;
			return;
		}
		if (Global_GetIsLightTank(TankSubtype))
		{
			Result.PlayerLightTanks++;
			return;
		}
		if (Global_GetIsMediumTank(TankSubtype))
		{
			Result.PlayerMediumTanks++;
			return;
		}
		if (Global_GetIsHeavyTank(TankSubtype))
		{
			Result.PlayerHeavyTanks++;
		}
	}

	void AddUnitTypeToBulkCounts(const FAsyncDetailedUnitState& UnitState, FPlayerUnitBulkLocation& BulkLocation)
	{
		if (UnitState.UnitType == EAllUnitType::UNType_Squad)
		{
			BulkLocation.SquadCount++;
			return;
		}

		if (UnitState.UnitType != EAllUnitType::UNType_Tank)
		{
			return;
		}

		const ETankSubtype TankSubtype = static_cast<ETankSubtype>(UnitState.UnitSubtypeRaw);
		if (Global_GetIsArmoredCar(TankSubtype))
		{
			BulkLocation.ArmoredCarCount++;
			return;
		}
		if (Global_GetIsLightTank(TankSubtype))
		{
			BulkLocation.LightTankCount++;
			return;
		}
		if (Global_GetIsMediumTank(TankSubtype))
		{
			BulkLocation.MediumTankCount++;
			return;
		}
		if (Global_GetIsHeavyTank(TankSubtype))
		{
			BulkLocation.HeavyTankCount++;
		}
	}

	bool TryBuildPlayerBulkLocation(
		const FPlayerBulkClusterBuilder& Cluster,
		const TArray<FPlayerBulkCandidate>& Candidates,
		const FFindPlayerUnitBulkLocations& Request,
		FPlayerUnitBulkLocation& OutBulkLocation)
	{
		const int32 MinUnitsPerBulk = FMath::Max(1, Request.MinUnitsPerBulk);
		if (Cluster.CandidateIndices.Num() < MinUnitsPerBulk)
		{
			return false;
		}

		FVector WeightedLocationSum = FVector::ZeroVector;
		float WeightedUnitCount = 0.f;
		for (const int32 CandidateIndex : Cluster.CandidateIndices)
		{
			const FPlayerBulkCandidate& Candidate = Candidates[CandidateIndex];
			WeightedLocationSum += Candidate.UnitState->UnitLocation * Candidate.Weight;
			WeightedUnitCount += Candidate.Weight;
		}

		const float MinWeightedBulkScore = FMath::Max(0.f, Request.MinWeightedBulkScore);
		if (WeightedUnitCount < MinWeightedBulkScore || WeightedUnitCount <= 0.f)
		{
			return false;
		}

		OutBulkLocation.BulkLocation = WeightedLocationSum / WeightedUnitCount;
		OutBulkLocation.Score = WeightedUnitCount;
		OutBulkLocation.WeightedUnitCount = WeightedUnitCount;
		OutBulkLocation.UnitsInBulk.Reserve(Cluster.CandidateIndices.Num());

		float DistanceSum = 0.f;
		for (const int32 CandidateIndex : Cluster.CandidateIndices)
		{
			const FPlayerBulkCandidate& Candidate = Candidates[CandidateIndex];
			const float DistanceFromCenter = FVector::Dist2D(
				Candidate.UnitState->UnitLocation,
				OutBulkLocation.BulkLocation);
			DistanceSum += DistanceFromCenter;
			OutBulkLocation.MaxUnitDistanceFromCenter = FMath::Max(
				OutBulkLocation.MaxUnitDistanceFromCenter,
				DistanceFromCenter);
			OutBulkLocation.UnitsInBulk.Add(Candidate.UnitState->UnitActorPtr);
			AddUnitTypeToBulkCounts(*Candidate.UnitState, OutBulkLocation);
		}

		OutBulkLocation.AverageUnitDistanceFromCenter =
			DistanceSum / static_cast<float>(Cluster.CandidateIndices.Num());
		const float MaxAverageBulkRadiusXY = FMath::Max(0.f, Request.MaxAverageBulkRadiusXY);
		return MaxAverageBulkRadiusXY <= 0.f
			|| OutBulkLocation.AverageUnitDistanceFromCenter <= MaxAverageBulkRadiusXY;
	}

	float GetClampedDistance(const float MinDistance, const float MaxDistance)
	{
		const float SafeMin = FMath::Max(0.f, FMath::Min(MinDistance, MaxDistance));
		const float SafeMax = FMath::Max(SafeMin, FMath::Max(MinDistance, MaxDistance));
		return SafeMax;
	}

	TArray<FVector> BuildArcPositions(
		const FVector& CenterLocation,
		const float CenterYaw,
		const float DeltaYaw,
		const float DistanceFromCenter,
		const int32 PointCount,
		const float SpreadScaler)
	{
		TArray<FVector> Positions;
		if (PointCount <= 0)
		{
			return Positions;
		}

		Positions.Reserve(PointCount);

		const float ArcStart = CenterYaw - DeltaYaw;
		const float ArcEnd = CenterYaw + DeltaYaw;

		if (PointCount == 1)
		{
			const FVector Direction = FRotator(0.f, CenterYaw, 0.f).Vector();
			Positions.Add(CenterLocation + Direction * DistanceFromCenter);
			return Positions;
		}

		const float ArcRange = ArcEnd - ArcStart;
		const float NormalStep = ArcRange / static_cast<float>(PointCount - 1);
		float DesiredStep = NormalStep * SpreadScaler;
		const float ArcCenter = (ArcStart + ArcEnd) * 0.5f;
		float StartYaw = ArcStart;
		float EndYaw = ArcEnd;

		const float HalfRange = DesiredStep * static_cast<float>(PointCount - 1) * 0.5f;
		StartYaw = ArcCenter - HalfRange;
		EndYaw = ArcCenter + HalfRange;

		if (StartYaw < ArcStart || EndYaw > ArcEnd)
		{
			DesiredStep = NormalStep;
			StartYaw = ArcStart;
		}

		for (int32 Index = 0; Index < PointCount; ++Index)
		{
			const float Yaw = StartYaw + DesiredStep * static_cast<float>(Index);
			const FVector Direction = FRotator(0.f, Yaw, 0.f).Vector();
			Positions.Add(CenterLocation + Direction * DistanceFromCenter);
		}

		return Positions;
	}

	TArray<FVector> BuildFlankPositions(
		const FAsyncDetailedUnitState& UnitState,
		const FFindClosestFlankableEnemyHeavy& Request)
	{
		const int32 TotalPositions = FMath::Max(Request.MaxSuggestedFlankPositionsPerTank, 0);
		TArray<FVector> Positions;
		if (TotalPositions == 0)
		{
			return Positions;
		}

		const float DistanceFromTank = GetClampedDistance(Request.MinDistanceToTank, Request.MaxDistanceToTank);
		const int32 LeftPositions = (TotalPositions + 1) / 2;
		const int32 RightPositions = TotalPositions - LeftPositions;
		const float BaseYaw = UnitState.UnitRotation.Yaw;

		const TArray<FVector> LeftArcPositions = BuildArcPositions(
			UnitState.UnitLocation,
			BaseYaw + StrategicAIHelperConstants::FlankLeftYawOffset,
			Request.DeltaYawFromLeftRight,
			DistanceFromTank,
			LeftPositions,
			Request.FlankingPositionsSpreadScaler);

		const TArray<FVector> RightArcPositions = BuildArcPositions(
			UnitState.UnitLocation,
			BaseYaw + StrategicAIHelperConstants::FlankRightYawOffset,
			Request.DeltaYawFromLeftRight,
			DistanceFromTank,
			RightPositions,
			Request.FlankingPositionsSpreadScaler);

		Positions.Reserve(LeftArcPositions.Num() + RightArcPositions.Num());
		Positions.Append(LeftArcPositions);
		Positions.Append(RightArcPositions);

		return Positions;
	}

	TArray<FVector> BuildFormationPositions(
		const FVector& CenterLocation,
		const FRotator& CenterRotation,
		const int32 UnitCount,
		const float RetreatFormationDistance,
		const int32 MaxFormationWidth)
	{
		TArray<FVector> Positions;
		if (UnitCount <= 0)
		{
			return Positions;
		}

		const int32 SafeWidth = FMath::Max(1, MaxFormationWidth);
		const FVector Forward = FRotator(0.f, CenterRotation.Yaw, 0.f).Vector();
		const FVector Right = FRotationMatrix(FRotator(0.f, CenterRotation.Yaw, 0.f)).GetScaledAxis(EAxis::Y);

		Positions.Reserve(UnitCount);

		for (int32 UnitIndex = 0; UnitIndex < UnitCount; ++UnitIndex)
		{
			const int32 RowIndex = UnitIndex / SafeWidth;
			const int32 ColumnIndex = UnitIndex % SafeWidth;
			const int32 ColumnsInRow = FMath::Min(SafeWidth, UnitCount - (RowIndex * SafeWidth));
			const float RowOffset = static_cast<float>(RowIndex) * RetreatFormationDistance;
			const float RowCenterOffset = (static_cast<float>(ColumnsInRow - 1) * 0.5f) * RetreatFormationDistance;
			const float ColumnOffset = static_cast<float>(ColumnIndex) * RetreatFormationDistance - RowCenterOffset;

			const FVector Offset = Forward * RowOffset + Right * ColumnOffset;
			Positions.Add(CenterLocation + Offset);
		}

		return Positions;
	}

	FVector GetAverageLocation(const TArray<FVector>& Locations)
	{
		if (Locations.IsEmpty())
		{
			return FVector::ZeroVector;
		}

		FVector Sum = FVector::ZeroVector;
		for (const FVector& Location : Locations)
		{
			Sum += Location;
		}

		return Sum / static_cast<float>(Locations.Num());
	}

	struct FDamagedTankGroupBuilder
	{
		FDamagedTanksRetreatGroup Group;
		TArray<FVector> Locations;
	};

	bool GetIsExcludedRetreatUnit(
		const FFindAlliedTanksToRetreat& Request,
		const TWeakObjectPtr<AActor>& UnitActor)
	{
		if (not UnitActor.IsValid())
		{
			return false;
		}

		for (const TWeakObjectPtr<AActor>& ExcludedActor : Request.ExcludedRetreatUnitActors)
		{
			if (not ExcludedActor.IsValid())
			{
				continue;
			}
			if (ExcludedActor == UnitActor)
			{
				return true;
			}
		}

		return false;
	}

	TArray<const FAsyncDetailedUnitState*> GatherDamagedTanks(
		const FFindAlliedTanksToRetreat& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates)
	{
		TArray<const FAsyncDetailedUnitState*> DamagedTanks;
		for (const FAsyncDetailedUnitState& UnitState : DetailedUnitStates)
		{
			if (UnitState.OwningPlayer != StrategicAIHelperConstants::EnemyOwningId)
			{
				continue;
			}
			if (UnitState.UnitType != EAllUnitType::UNType_Tank)
			{
				continue;
			}
			if (UnitState.HealthRatio > Request.HealthRatioThresholdConsiderUnitToRetreat)
			{
				continue;
			}
			if (GetIsExcludedRetreatUnit(Request, UnitState.UnitActorPtr))
			{
				continue;
			}

			DamagedTanks.Add(&UnitState);
		}

		DamagedTanks.Sort([](const FAsyncDetailedUnitState& Left, const FAsyncDetailedUnitState& Right)
		{
			return Left.HealthRatio < Right.HealthRatio;
		});

		return DamagedTanks;
	}

	TArray<const FAsyncDetailedUnitState*> GatherIdleHazmats(
		const FFindAlliedTanksToRetreat& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates)
	{
		TArray<const FAsyncDetailedUnitState*> IdleHazmats;
		for (const FAsyncDetailedUnitState& UnitState : DetailedUnitStates)
		{
			if (UnitState.OwningPlayer != StrategicAIHelperConstants::EnemyOwningId)
			{
				continue;
			}
			if (not FStrategicAIHelpers::GetIsHazmatEngineer(UnitState))
			{
				continue;
			}
			if (UnitState.CurrentActiveAbility != EAbilityID::IdIdle)
			{
				continue;
			}
			if (UnitState.bIsInCombat)
			{
				continue;
			}
			if (GetIsExcludedRetreatUnit(Request, UnitState.UnitActorPtr))
			{
				continue;
			}

			IdleHazmats.Add(&UnitState);
		}

		return IdleHazmats;
	}

	TArray<FDamagedTankGroupBuilder> BuildDamagedTankGroups(
		const TArray<const FAsyncDetailedUnitState*>& DamagedTanks,
		const FFindAlliedTanksToRetreat& Request)
	{
		const int32 MaxTanksToFind = FMath::Max(0, Request.MaxTanksToFind);
		const int32 TanksToProcess = FMath::Min(MaxTanksToFind, DamagedTanks.Num());

		TArray<FDamagedTankGroupBuilder> Groups;
		Groups.Reserve(StrategicAIHelperConstants::MaxRetreatGroups);

		for (int32 Index = 0; Index < TanksToProcess; ++Index)
		{
			const FAsyncDetailedUnitState* UnitState = DamagedTanks[Index];
			if (not UnitState)
			{
				continue;
			}

			bool bAddedToGroup = false;
			for (FDamagedTankGroupBuilder& GroupBuilder : Groups)
			{
				for (const FVector& Location : GroupBuilder.Locations)
				{
					if (FVector::Dist(Location, UnitState->UnitLocation) <= Request.MaxDistanceFromOtherGroupMembers)
					{
						GroupBuilder.Group.DamagedTanks.Add(UnitState->UnitActorPtr);
						GroupBuilder.Locations.Add(UnitState->UnitLocation);
						bAddedToGroup = true;
						break;
					}
				}

				if (bAddedToGroup)
				{
					break;
				}
			}

			if (bAddedToGroup)
			{
				continue;
			}

			if (Groups.Num() >= StrategicAIHelperConstants::MaxRetreatGroups)
			{
				continue;
			}

			FDamagedTankGroupBuilder NewGroup;
			NewGroup.Group.DamagedTanks.Add(UnitState->UnitActorPtr);
			NewGroup.Locations.Add(UnitState->UnitLocation);
			Groups.Add(MoveTemp(NewGroup));
		}

		return Groups;
	}

	void AppendHazmatsToGroups(
		TArray<FDamagedTankGroupBuilder>& Groups,
		TArray<const FAsyncDetailedUnitState*> IdleHazmats,
		const FFindAlliedTanksToRetreat& Request)
	{
		IdleHazmats.RemoveAll([](const FAsyncDetailedUnitState* UnitState)
		{
			return not UnitState;
		});

		for (FDamagedTankGroupBuilder& GroupBuilder : Groups)
		{
			if (GroupBuilder.Locations.IsEmpty())
			{
				continue;
			}

			const FVector GroupCenter = GetAverageLocation(GroupBuilder.Locations);
			IdleHazmats.Sort([&GroupCenter](const FAsyncDetailedUnitState& Left, const FAsyncDetailedUnitState& Right)
			{
				return FVector::Dist(Left.UnitLocation, GroupCenter) < FVector::Dist(Right.UnitLocation, GroupCenter);
			});

			const int32 MaxHazmatsToConsider = FMath::Max(0, Request.MaxIdleHazmatsToConsider);
			const int32 HazmatsToProcess = FMath::Min(MaxHazmatsToConsider, IdleHazmats.Num());

			for (int32 Index = 0; Index < HazmatsToProcess; ++Index)
			{
				const FAsyncDetailedUnitState* HazmatState = IdleHazmats[Index];
				if (not HazmatState)
				{
					continue;
				}

				FWeakActorLocations HazmatLocations;
				HazmatLocations.Actor = HazmatState->UnitActorPtr;
				HazmatLocations.Locations = BuildFormationPositions(
					HazmatState->UnitLocation,
					HazmatState->UnitRotation,
					GroupBuilder.Locations.Num(),
					Request.RetreatFormationDistance,
					Request.MaxFormationWidth);
				GroupBuilder.Group.HazmatsWithFormationLocations.Add(MoveTemp(HazmatLocations));
			}
		}
	}
}

FResultLocationsUnderPlayerAttack FStrategicAIHelpers::BuildLocationsUnderPlayerAttackResult(
	const FFindLocationsUnderPlayerAttack& Request,
	const TArray<FAsyncDetailedUnitState>& DetailedUnitStates)
{
	FResultLocationsUnderPlayerAttack Result;
	Result.RequestID = Request.RequestID;

	if (Request.LocationsToEvaluate.IsEmpty())
	{
		return Result;
	}

	const float MaxInfluenceRadius = FMath::Max(0.f, Request.MaxInfluenceRadius);
	if (MaxInfluenceRadius <= 0.f)
	{
		return Result;
	}

	const float SafeDistanceExponent = FMath::Max(0.01f, Request.DistanceExponent);
	const float MinimumThreatScore = FMath::Max(0.f, Request.MinimumThreatScoreToFlagLocation);
	const float MinimumEffectiveAttackerCount = FMath::Max(0.f, Request.MinimumEffectiveAttackerCount);
	const float GroupAmplifierPerExtraAttacker = FMath::Max(0.f, Request.GroupAmplifierPerExtraEffectiveAttacker);
	const float InverseMaxInfluenceRadius = 1.f / MaxInfluenceRadius;

	for (const FVector& EvaluatedLocation : Request.LocationsToEvaluate)
	{
		float RawThreatScore = 0.f;
		float WeightedAttackerCount = 0.f;
		float WeightedDistanceSum = 0.f;
		FVector WeightedLocationSum = FVector::ZeroVector;
		TArray<TWeakObjectPtr<AActor>> ContributingAttackerActors;

		for (const FAsyncDetailedUnitState& UnitState : DetailedUnitStates)
		{
			if (UnitState.OwningPlayer != StrategicAIHelperConstants::PlayerOwningId)
			{
				continue;
			}

			const float UnitThreatWeight = StrategicAIHelperUtilities::GetUnitThreatWeight(UnitState, Request);
			if (UnitThreatWeight <= 0.f)
			{
				continue;
			}

			const float DistanceToLocation = FVector::Dist(UnitState.UnitLocation, EvaluatedLocation);
			if (DistanceToLocation > MaxInfluenceRadius)
			{
				continue;
			}

			const float DistanceRatio = DistanceToLocation * InverseMaxInfluenceRadius;
			const float DistanceFalloff = FMath::Pow(FMath::Max(0.f, 1.f - DistanceRatio), SafeDistanceExponent);
			const float UnitThreatContribution = UnitThreatWeight * DistanceFalloff;
			if (UnitThreatContribution <= 0.f)
			{
				continue;
			}

			RawThreatScore += UnitThreatContribution;
			WeightedAttackerCount += FMath::Min(1.f, UnitThreatContribution);
			WeightedDistanceSum += DistanceToLocation * UnitThreatContribution;
			WeightedLocationSum += UnitState.UnitLocation * UnitThreatContribution;
			ContributingAttackerActors.Add(UnitState.UnitActorPtr);
		}

		if (RawThreatScore <= 0.f)
		{
			continue;
		}

		const float ExtraAttackers = FMath::Max(0.f, WeightedAttackerCount - 1.f);
		const float GroupAmplifier = 1.f + GroupAmplifierPerExtraAttacker * ExtraAttackers;
		const float FinalThreatScore = RawThreatScore * GroupAmplifier;

		if (FinalThreatScore < MinimumThreatScore || WeightedAttackerCount < MinimumEffectiveAttackerCount)
		{
			continue;
		}

		FPlayerAttackLocationEvaluation AttackEvaluation;
		AttackEvaluation.LocationUnderAttack = EvaluatedLocation;
		AttackEvaluation.AverageAttackerDistance = WeightedDistanceSum / RawThreatScore;
		AttackEvaluation.AverageAttackerLocation = WeightedLocationSum / RawThreatScore;
		AttackEvaluation.AttackingUnits = MoveTemp(ContributingAttackerActors);
		Result.LocationsUnderAttack.Add(MoveTemp(AttackEvaluation));
	}

	StrategicAIHelperUtilities::RemoveNearZeroStrategicAttackLocations(Result.LocationsUnderAttack);

	return Result;
}

FResultPlayerUnitBulkLocations FStrategicAIHelpers::BuildPlayerUnitBulkLocationsResult(
	const FFindPlayerUnitBulkLocations& Request,
	const TArray<FAsyncDetailedUnitState>& DetailedUnitStates)
{
	FResultPlayerUnitBulkLocations Result;
	Result.RequestID = Request.RequestID;

	const TArray<StrategicAIHelperUtilities::FPlayerBulkCandidate> Candidates =
		StrategicAIHelperUtilities::GatherPlayerBulkCandidates(Request, DetailedUnitStates);
	const TArray<StrategicAIHelperUtilities::FPlayerBulkClusterBuilder> Clusters =
		StrategicAIHelperUtilities::BuildPlayerBulkClusters(Candidates, Request.ClusterRadiusXY);

	for (const StrategicAIHelperUtilities::FPlayerBulkClusterBuilder& Cluster : Clusters)
	{
		FPlayerUnitBulkLocation BulkLocation;
		if (not StrategicAIHelperUtilities::TryBuildPlayerBulkLocation(Cluster, Candidates, Request, BulkLocation))
		{
			continue;
		}

		Result.PlayerUnitBulks.Add(MoveTemp(BulkLocation));
	}

	StrategicAIHelperUtilities::RemoveNearZeroStrategicPlayerBulks(Result.PlayerUnitBulks);

	Result.PlayerUnitBulks.Sort([](const FPlayerUnitBulkLocation& Left, const FPlayerUnitBulkLocation& Right)
	{
		return Left.Score > Right.Score;
	});

	const int32 MaxBulksToReturn = FMath::Max(0, Request.MaxBulksToReturn);
	if (MaxBulksToReturn > 0 && Result.PlayerUnitBulks.Num() > MaxBulksToReturn)
	{
		Result.PlayerUnitBulks.SetNum(MaxBulksToReturn);
	}

	return Result;
}

bool FStrategicAIHelpers::GetIsFlankableHeavyTank(const FAsyncDetailedUnitState& UnitState)
{
	if (UnitState.UnitType != EAllUnitType::UNType_Tank)
	{
		return false;
	}
	const ETankSubtype TankSubtype = static_cast<ETankSubtype>(UnitState.UnitSubtypeRaw);
	return Global_GetIsHeavyTank(TankSubtype);
}

FResultClosestFlankableEnemyHeavy FStrategicAIHelpers::BuildClosestFlankableEnemyHeavyResult(
	const FFindClosestFlankableEnemyHeavy& Request,
	const TArray<FAsyncDetailedUnitState>& DetailedUnitStates)
{
	struct FDistanceEntry
	{
		float Distance = 0.f;
		const FAsyncDetailedUnitState* UnitState = nullptr;
	};

	FResultClosestFlankableEnemyHeavy Result;
	Result.RequestID = Request.RequestID;

	const int32 MaxHeavyTanksToFlank = FMath::Max(0, Request.MaxHeavyTanksToFlank);
	if (MaxHeavyTanksToFlank == 0)
	{
		return Result;
	}

	TArray<FDistanceEntry> CandidateTanks;
	for (const FAsyncDetailedUnitState& UnitState : DetailedUnitStates)
	{
		if (UnitState.OwningPlayer != StrategicAIHelperConstants::PlayerOwningId)
		{
			continue;
		}
		if (not FStrategicAIHelpers::GetIsFlankableHeavyTank(UnitState))
		{
			continue;
		}

		const float Distance = FVector::Dist(Request.StartSearchLocation, UnitState.UnitLocation);
		CandidateTanks.Add({Distance, &UnitState});
	}

	CandidateTanks.Sort([](const FDistanceEntry& Left, const FDistanceEntry& Right)
	{
		return Left.Distance < Right.Distance;
	});

	const int32 CountToProcess = FMath::Min(MaxHeavyTanksToFlank, CandidateTanks.Num());
	Result.FlankLocationsAroundHeavyTank.Reserve(CountToProcess);

	for (int32 Index = 0; Index < CountToProcess; ++Index)
	{
		const FAsyncDetailedUnitState* UnitState = CandidateTanks[Index].UnitState;
		if (not UnitState)
		{
			continue;
		}

		FWeakActorLocations FlankLocations;
		FlankLocations.Actor = UnitState->UnitActorPtr;
		FlankLocations.Locations = StrategicAIHelperUtilities::BuildFlankPositions(*UnitState, Request);
		Result.FlankLocationsAroundHeavyTank.Add(MoveTemp(FlankLocations));
	}

	StrategicAIHelperUtilities::RemoveNearZeroStrategicActorLocations(Result.FlankLocationsAroundHeavyTank);

	return Result;
}

FResultPlayerUnitCounts FStrategicAIHelpers::BuildPlayerUnitCountsResult(
	const FGetPlayerUnitCountsAndBase& Request,
	const TArray<FAsyncDetailedUnitState>& DetailedUnitStates)
{
	FResultPlayerUnitCounts Result;
	Result.RequestID = Request.RequestID;

	for (const FAsyncDetailedUnitState& UnitState : DetailedUnitStates)
	{
		if (UnitState.OwningPlayer != StrategicAIHelperConstants::PlayerOwningId)
		{
			continue;
		}

		switch (UnitState.UnitType)
		{
		case EAllUnitType::UNType_Tank:
			StrategicAIHelperUtilities::AddPlayerTankUnitStateCount(Result, UnitState);
			break;
		case EAllUnitType::UNType_Squad:
			Result.PlayerSquads++;
			break;
		case EAllUnitType::UNType_Nomadic:
			if (FStrategicAIHelpers::GetIsNomadicHQ(UnitState))
			{
				Result.PlayerHQLocation = UnitState.UnitLocation;
			}
			else if (FStrategicAIHelpers::GetIsNomadicResourceBuilding(UnitState))
			{
				Result.PlayerResourceBuildings.Add(UnitState.UnitLocation);
			}
			else
			{
				Result.PlayerNomadicVehicles++;
			}
			break;
		default:
			break;
		}
	}

	StrategicAIHelperUtilities::RemoveNearZeroStrategicLocations(Result.PlayerResourceBuildings);

	return Result;
}

FResultAlliedTanksToRetreat FStrategicAIHelpers::BuildAlliedTanksToRetreatResult(
	const FFindAlliedTanksToRetreat& Request,
	const TArray<FAsyncDetailedUnitState>& DetailedUnitStates)
{
	FResultAlliedTanksToRetreat Result;
	Result.RequestID = Request.RequestID;

	const TArray<const FAsyncDetailedUnitState*> DamagedTanks =
		StrategicAIHelperUtilities::GatherDamagedTanks(Request, DetailedUnitStates);
	TArray<StrategicAIHelperUtilities::FDamagedTankGroupBuilder> Groups =
		StrategicAIHelperUtilities::BuildDamagedTankGroups(DamagedTanks, Request);
	const TArray<const FAsyncDetailedUnitState*> IdleHazmats =
		StrategicAIHelperUtilities::GatherIdleHazmats(Request, DetailedUnitStates);

	StrategicAIHelperUtilities::AppendHazmatsToGroups(Groups, IdleHazmats, Request);

	if (Groups.IsValidIndex(0))
	{
		Result.Group1 = MoveTemp(Groups[0].Group);
	}
	if (Groups.IsValidIndex(1))
	{
		Result.Group2 = MoveTemp(Groups[1].Group);
	}
	if (Groups.IsValidIndex(2))
	{
		Result.Group3 = MoveTemp(Groups[2].Group);
	}

	StrategicAIHelperUtilities::RemoveNearZeroStrategicRetreatLocations(Result.Group1);
	StrategicAIHelperUtilities::RemoveNearZeroStrategicRetreatLocations(Result.Group2);
	StrategicAIHelperUtilities::RemoveNearZeroStrategicRetreatLocations(Result.Group3);

	return Result;
}

bool FStrategicAIHelpers::GetIsLightTank(const FAsyncDetailedUnitState& UnitState)
{
	if (UnitState.UnitType != EAllUnitType::UNType_Tank)
	{
		return false;
	}
	const ETankSubtype TankSubtype = static_cast<ETankSubtype>(UnitState.UnitSubtypeRaw);
	return Global_GetIsLightTank(TankSubtype);
}

bool FStrategicAIHelpers::GetIsMediumTank(const FAsyncDetailedUnitState& UnitState)
{
	if (UnitState.UnitType != EAllUnitType::UNType_Tank)
	{
		return false;
	}
	const ETankSubtype TankSubtype = static_cast<ETankSubtype>(UnitState.UnitSubtypeRaw);
	return Global_GetIsMediumTank(TankSubtype);
}

bool FStrategicAIHelpers::GetIsHeavyTank(const FAsyncDetailedUnitState& UnitState)
{
	if (UnitState.UnitType != EAllUnitType::UNType_Tank)
	{
		return false;
	}
	const ETankSubtype TankSubtype = static_cast<ETankSubtype>(UnitState.UnitSubtypeRaw);
	return Global_GetIsHeavyTank(TankSubtype);
}

bool FStrategicAIHelpers::GetIsNomadicHQ(const FAsyncDetailedUnitState& UnitState)
{
	if (UnitState.UnitType != EAllUnitType::UNType_Nomadic)
	{
		return false;
	}
	const ENomadicSubtype NomadicSubtype = static_cast<ENomadicSubtype>(UnitState.UnitSubtypeRaw);
	return NomadicSubtype == ENomadicSubtype::Nomadic_GerHq || NomadicSubtype == ENomadicSubtype::Nomadic_RusHq;
}

bool FStrategicAIHelpers::GetIsNomadicResourceBuilding(const FAsyncDetailedUnitState& UnitState)
{
	if (UnitState.UnitType != EAllUnitType::UNType_Nomadic)
	{
		return false;
	}
	const ENomadicSubtype NomadicSubtype = static_cast<ENomadicSubtype>(UnitState.UnitSubtypeRaw);
	return NomadicSubtype == ENomadicSubtype::Nomadic_GerRefinery
		|| NomadicSubtype == ENomadicSubtype::Nomadic_GerMetalVault;
}

bool FStrategicAIHelpers::GetIsHazmatEngineer(const FAsyncDetailedUnitState& UnitState)
{
	if (UnitState.UnitType != EAllUnitType::UNType_Squad)
	{
		return false;
	}
	const ESquadSubtype SquadSubtype = static_cast<ESquadSubtype>(UnitState.UnitSubtypeRaw);
	return SquadSubtype == ESquadSubtype::Squad_Rus_HazmatEngineers;
}
