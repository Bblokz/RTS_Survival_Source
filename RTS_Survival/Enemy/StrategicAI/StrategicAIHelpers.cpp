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
	constexpr float ConstructionArcUnitsPerDensityStep = 1000.f;
	constexpr float ConstructionArcDuplicateOffsetTolerance = 1.f;
	constexpr float MaxConstructionArcAngleDegrees = 170.f;
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

	struct FEnemyBasePoint
	{
		FVector2D Location = FVector2D::ZeroVector;
		TWeakObjectPtr<AActor> CoreBuildingActor;
		bool bIsCoreBuilding = false;
	};

	struct FEnemyCoreBuildingClusterPoint
	{
		FVector2D Location = FVector2D::ZeroVector;
		TWeakObjectPtr<AActor> Actor;
	};

	struct FEnemyBaseClusterCandidate
	{
		TArray<FEnemyBasePoint> BuildingPoints;
		FVector Point = FVector::ZeroVector;
		float Score = 0.f;
	};

	struct FDefenseArcAnchorCandidate
	{
		FVector2D Location = FVector2D::ZeroVector;
		float Score = 0.f;
	};

	void GatherEnemyBaseRequestPoints(
		const FFindEnemyBaseClusters& Request,
		const TArray<FAsyncDetailedUnitState>& DetailedUnitStates,
		TArray<FEnemyCoreBuildingClusterPoint>& OutCorePoints,
		TArray<FVector2D>& OutSatellitePoints,
		TArray<FVector>& OutAlliedUnitLocations)
	{
		const TSet<EBuildingExpansionType> CoreTypes(Request.CoreBuildingTypes);
		const TSet<EBuildingExpansionType> SatelliteTypes(Request.SatelliteBuildingTypes);
		for (const FAsyncDetailedUnitState& UnitState : DetailedUnitStates)
		{
			if (UnitState.OwningPlayer != StrategicAIHelperConstants::EnemyOwningId)
			{
				continue;
			}

			OutAlliedUnitLocations.Add(UnitState.UnitLocation);
			if (UnitState.UnitType != EAllUnitType::UNType_BuildingExpansion)
			{
				continue;
			}

			const EBuildingExpansionType BuildingType = static_cast<EBuildingExpansionType>(UnitState.UnitSubtypeRaw);
			const FVector2D UnitLocationXY(UnitState.UnitLocation.X, UnitState.UnitLocation.Y);
			if (CoreTypes.Contains(BuildingType))
			{
				FEnemyCoreBuildingClusterPoint CorePoint;
				CorePoint.Location = UnitLocationXY;
				CorePoint.Actor = UnitState.UnitActorPtr;
				OutCorePoints.Add(CorePoint);
				continue;
			}

			if (SatelliteTypes.Contains(BuildingType))
			{
				OutSatellitePoints.Add(UnitLocationXY);
			}
		}
	}

	TArray<int32> BuildCoreClusterLabels(
		const TArray<FEnemyCoreBuildingClusterPoint>& CorePoints,
		const float CoreClusterDistanceXY,
		const int32 RequestedMinCoreNeighbors,
		int32& OutClusterCount)
	{
		const float ClusterDistanceSq = FMath::Square(FMath::Max(0.f, CoreClusterDistanceXY));
		const int32 MinCoreNeighbors = FMath::Max(1, RequestedMinCoreNeighbors);
		TArray<int32> Labels;
		Labels.Init(INDEX_NONE, CorePoints.Num());
		OutClusterCount = 0;

		for (int32 PointIndex = 0; PointIndex < CorePoints.Num(); ++PointIndex)
		{
			if (Labels[PointIndex] != INDEX_NONE)
			{
				continue;
			}

			TArray<int32> Neighbors;
			for (int32 NeighborIndex = 0; NeighborIndex < CorePoints.Num(); ++NeighborIndex)
			{
				if (FVector2D::DistSquared(CorePoints[PointIndex].Location, CorePoints[NeighborIndex].Location) <= ClusterDistanceSq)
				{
					Neighbors.Add(NeighborIndex);
				}
			}

			if (Neighbors.Num() < MinCoreNeighbors)
			{
				Labels[PointIndex] = -2;
				continue;
			}

			for (const int32 NeighborId : Neighbors)
			{
				Labels[NeighborId] = OutClusterCount;
			}
			++OutClusterCount;
		}

		return Labels;
	}

	TArray<FEnemyBaseClusterCandidate> BuildCoreBaseClusters(
		const TArray<FEnemyCoreBuildingClusterPoint>& CorePoints,
		const TArray<int32>& Labels,
		const int32 ClusterCount)
	{
		TArray<FEnemyBaseClusterCandidate> Clusters;
		Clusters.SetNum(ClusterCount);
		for (int32 PointIndex = 0; PointIndex < CorePoints.Num(); ++PointIndex)
		{
			if (not Labels.IsValidIndex(PointIndex) || Labels[PointIndex] < 0)
			{
				continue;
			}

			FEnemyBasePoint BasePoint;
			BasePoint.Location = CorePoints[PointIndex].Location;
			BasePoint.CoreBuildingActor = CorePoints[PointIndex].Actor;
			BasePoint.bIsCoreBuilding = true;
			Clusters[Labels[PointIndex]].BuildingPoints.Add(BasePoint);
		}

		return Clusters;
	}

	void AttachSatellitePointsToBaseClusters(
		const TArray<FVector2D>& SatellitePoints,
		const float MaxAttachDistanceXY,
		TArray<FEnemyBaseClusterCandidate>& Clusters)
	{
		const float MaxAttachDistanceSq = FMath::Square(FMath::Max(0.f, MaxAttachDistanceXY));
		for (const FVector2D& SatellitePoint : SatellitePoints)
		{
			int32 BestCluster = INDEX_NONE;
			float BestDistanceSq = MaxAttachDistanceSq;
			for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ++ClusterIndex)
			{
				for (const FEnemyBasePoint& BasePoint : Clusters[ClusterIndex].BuildingPoints)
				{
					const float DistanceSq = FVector2D::DistSquared(SatellitePoint, BasePoint.Location);
					if (DistanceSq < BestDistanceSq)
					{
						BestDistanceSq = DistanceSq;
						BestCluster = ClusterIndex;
					}
				}
			}

			if (BestCluster == INDEX_NONE)
			{
				continue;
			}

			FEnemyBasePoint BasePoint;
			BasePoint.Location = SatellitePoint;
			BasePoint.bIsCoreBuilding = false;
			Clusters[BestCluster].BuildingPoints.Add(BasePoint);
		}
	}

	bool TryFinalizeBaseCluster(
		const FFindEnemyBaseClusters& Request,
		FEnemyBaseClusterCandidate& Cluster)
	{
		if (Cluster.BuildingPoints.Num() < Request.MinTotalBuildingsPerBase)
		{
			return false;
		}

		FVector2D Sum = FVector2D::ZeroVector;
		for (const FEnemyBasePoint& BasePoint : Cluster.BuildingPoints)
		{
			Sum += BasePoint.Location;
		}

		const FVector2D Center = Sum / static_cast<float>(Cluster.BuildingPoints.Num());
		Cluster.Score = static_cast<float>(Cluster.BuildingPoints.Num());
		Cluster.Point = FVector(Center.X, Center.Y, 0.f);
		return Cluster.Score >= Request.MinBaseScoreToReturn;
	}

	TArray<FEnemyBaseClusterCandidate> BuildAcceptedEnemyBaseClusters(
		const FFindEnemyBaseClusters& Request,
		const TArray<FEnemyCoreBuildingClusterPoint>& CorePoints,
		const TArray<FVector2D>& SatellitePoints)
	{
		int32 ClusterCount = 0;
		const TArray<int32> Labels = BuildCoreClusterLabels(
			CorePoints,
			Request.CoreClusterDistanceXY,
			Request.MinCoreNeighbors,
			ClusterCount);
		TArray<FEnemyBaseClusterCandidate> Clusters = BuildCoreBaseClusters(CorePoints, Labels, ClusterCount);
		AttachSatellitePointsToBaseClusters(SatellitePoints, Request.CoreClusterDistanceXY, Clusters);

		TArray<FEnemyBaseClusterCandidate> AcceptedClusters;
		for (FEnemyBaseClusterCandidate& Cluster : Clusters)
		{
			if (TryFinalizeBaseCluster(Request, Cluster))
			{
				AcceptedClusters.Add(Cluster);
			}
		}
		AcceptedClusters.Sort([](const FEnemyBaseClusterCandidate& Left, const FEnemyBaseClusterCandidate& Right)
		{
			return Left.Score > Right.Score;
		});

		const int32 MaxBases = FMath::Max(0, Request.MaxBasesToReturn);
		if (MaxBases > 0 && AcceptedClusters.Num() > MaxBases)
		{
			AcceptedClusters.SetNum(MaxBases);
		}

		return AcceptedClusters;
	}

	FEnemyBasePointCoreBuildings BuildEnemyBasePointCoreBuildings(const FEnemyBaseClusterCandidate& Cluster)
	{
		FEnemyBasePointCoreBuildings BasePoint;
		BasePoint.BaseLocation = Cluster.Point;
		for (const FEnemyBasePoint& BuildingPoint : Cluster.BuildingPoints)
		{
			if (not BuildingPoint.bIsCoreBuilding)
			{
				continue;
			}

			BasePoint.CoreBuildingActors.Add(BuildingPoint.CoreBuildingActor);
		}

		return BasePoint;
	}

	void RemoveNearZeroStrategicEnemyBasePoints(TArray<FEnemyBasePointCoreBuildings>& BasePoints)
	{
		BasePoints.RemoveAll([](const FEnemyBasePointCoreBuildings& BasePoint)
		{
			return GetIsNearZeroStrategicLocation(BasePoint.BaseLocation);
		});
	}

	float GetAngleDifferenceDegrees(const float LeftAngleDegrees, const float RightAngleDegrees)
	{
		return FMath::Abs(FMath::FindDeltaAngleDegrees(LeftAngleDegrees, RightAngleDegrees));
	}

	TArray<FVector> BuildMergedThreatLocationsForBase(
		const FFindEnemyBaseClusters& Request,
		const FEnemyBaseClusterCandidate& Cluster)
	{
		TArray<FVector> ThreatLocations = Request.PlayerUnitLocationsToDefendAgainst;
		RemoveNearZeroStrategicLocations(ThreatLocations);
		ThreatLocations.Sort([&Cluster](const FVector& Left, const FVector& Right)
		{
			return FVector::DistSquared2D(Left, Cluster.Point) < FVector::DistSquared2D(Right, Cluster.Point);
		});

		TArray<FVector> MergedLocations;
		const int32 MaxThreatLocations = FMath::Max(0, Request.DefenseArcCandidateParams.MaxThreatLocationsPerBase);
		if (MaxThreatLocations <= 0)
		{
			return MergedLocations;
		}

		const float MergeDistanceSq = FMath::Square(FMath::Max(0.f, Request.DefenseArcCandidateParams.ThreatLocationMergeDistanceXY));
		const float MergeAngle = FMath::Max(0.f, Request.DefenseArcCandidateParams.ThreatDirectionMergeAngleDegrees);
		for (const FVector& ThreatLocation : ThreatLocations)
		{
			const float ThreatYaw = (ThreatLocation - Cluster.Point).Rotation().Yaw;
			const bool bAlreadyRepresented = MergedLocations.ContainsByPredicate(
				[&](const FVector& MergedLocation)
				{
					const float MergedYaw = (MergedLocation - Cluster.Point).Rotation().Yaw;
					return FVector::DistSquared2D(ThreatLocation, MergedLocation) <= MergeDistanceSq
						|| GetAngleDifferenceDegrees(ThreatYaw, MergedYaw) <= MergeAngle;
				});
			if (bAlreadyRepresented)
			{
				continue;
			}

			MergedLocations.Add(ThreatLocation);
			if (MergedLocations.Num() >= MaxThreatLocations)
			{
				break;
			}
		}

		return MergedLocations;
	}

	TArray<FDefenseArcAnchorCandidate> BuildDefenseArcAnchors(
		const FFindEnemyBaseClusters& Request,
		const FEnemyBaseClusterCandidate& Cluster,
		const FVector& ThreatLocation)
	{
		TArray<FDefenseArcAnchorCandidate> Anchors;
		const FVector2D BaseCenterXY(Cluster.Point.X, Cluster.Point.Y);
		const FVector2D ThreatDirection = (FVector2D(ThreatLocation.X, ThreatLocation.Y) - BaseCenterXY).GetSafeNormal();
		if (ThreatDirection.IsNearlyZero())
		{
			return Anchors;
		}

		for (const FEnemyBasePoint& BasePoint : Cluster.BuildingPoints)
		{
			const FVector2D FromCenter = BasePoint.Location - BaseCenterXY;
			const float FrontSideScore = FVector2D::DotProduct(FromCenter.GetSafeNormal(), ThreatDirection);
			const float BuildingWeight = BasePoint.bIsCoreBuilding
				? Request.DefenseArcCandidateParams.CoreBuildingAnchorWeight
				: Request.DefenseArcCandidateParams.SatelliteBuildingAnchorWeight;

			FDefenseArcAnchorCandidate Anchor;
			Anchor.Location = BasePoint.Location;
			Anchor.Score = FrontSideScore + FMath::Max(0.f, BuildingWeight);
			Anchors.Add(Anchor);
		}

		Anchors.Sort([](const FDefenseArcAnchorCandidate& Left, const FDefenseArcAnchorCandidate& Right)
		{
			return Left.Score > Right.Score;
		});
		const int32 MaxAnchors = FMath::Max(0, Request.DefenseArcCandidateParams.MaxAnchorBuildingsPerThreatDirection);
		if (MaxAnchors > 0 && Anchors.Num() > MaxAnchors)
		{
			Anchors.SetNum(MaxAnchors);
		}

		return Anchors;
	}

	bool GetIsFarEnoughFromBaseBuildings(
		const FVector& CandidateLocation,
		const FEnemyBaseClusterCandidate& Cluster,
		const float MinDistanceXY)
	{
		const float MinDistanceSq = FMath::Square(FMath::Max(0.f, MinDistanceXY));
		return not Cluster.BuildingPoints.ContainsByPredicate(
			[&](const FEnemyBasePoint& BasePoint)
			{
				return FVector2D::DistSquared(
					FVector2D(CandidateLocation.X, CandidateLocation.Y),
					BasePoint.Location) < MinDistanceSq;
			});
	}

	bool GetIsFarEnoughFromDefenseCandidates(
		const FVector& CandidateLocation,
		const TArray<FDefensePositions>& DefenseCandidates,
		const float MinDistanceXY)
	{
		const float MinDistanceSq = FMath::Square(FMath::Max(0.f, MinDistanceXY));
		return not DefenseCandidates.ContainsByPredicate(
			[&](const FDefensePositions& DefenseCandidate)
			{
				return FVector::DistSquared2D(CandidateLocation, DefenseCandidate.Location) < MinDistanceSq;
			});
	}

	void TryAddDefenseArcCandidate(
		const FFindEnemyBaseClusters& Request,
		const FEnemyBaseClusterCandidate& Cluster,
		const FVector2D& AnchorLocation,
		const FVector& CandidateLocation,
		const float CandidateYaw,
		TArray<FDefensePositions>& OutDefenseCandidates)
	{
		const float AnchorDistance = FVector2D::Distance(
			FVector2D(CandidateLocation.X, CandidateLocation.Y),
			AnchorLocation);
		if (AnchorDistance < Request.DefenseArcCandidateParams.MinOffsetFromDefendedBuildingXY
			|| AnchorDistance > Request.DefenseArcCandidateParams.MaxOffsetFromDefendedBuildingXY)
		{
			return;
		}

		if (not GetIsFarEnoughFromBaseBuildings(
			CandidateLocation,
			Cluster,
			Request.DefenseArcCandidateParams.MinOffsetFromAnyBaseBuildingXY))
		{
			return;
		}

		if (not GetIsFarEnoughFromDefenseCandidates(
			CandidateLocation,
			OutDefenseCandidates,
			Request.DefenseArcCandidateParams.MinPointSpacingXY))
		{
			return;
		}

		FDefensePositions DefensePosition;
		DefensePosition.Location = CandidateLocation;
		DefensePosition.YawRotation = CandidateYaw;
		OutDefenseCandidates.Add(DefensePosition);
	}

	void BuildDefenseArcRowCandidates(
		const FFindEnemyBaseClusters& Request,
		const FEnemyBaseClusterCandidate& Cluster,
		const FDefenseArcAnchorCandidate& Anchor,
		const FVector& ThreatLocation,
		const int32 RowIndex,
		FRandomStream& RandomStream,
		TArray<FDefensePositions>& OutDefenseCandidates)
	{
		const int32 PointsPerArc = FMath::Max(0, Request.DefenseArcCandidateParams.PointsPerArc);
		if (PointsPerArc <= 0)
		{
			return;
		}

		const FVector AnchorLocation3D(Anchor.Location.X, Anchor.Location.Y, Cluster.Point.Z);
		const float DirectionToThreatYaw = (ThreatLocation - AnchorLocation3D).Rotation().Yaw;
		const float RowDistanceBase = Request.DefenseArcCandidateParams.PreferredOffsetFromDefendedBuildingXY
			+ Request.DefenseArcCandidateParams.ArcRowOffsetXY * static_cast<float>(RowIndex);
		const float RowAngle = Request.DefenseArcCandidateParams.ArcAngleDegrees
			* FMath::Pow(Request.DefenseArcCandidateParams.ArcRowAngleScale, static_cast<float>(RowIndex));
		const float RowYawScale = FMath::Pow(Request.DefenseArcCandidateParams.OuterArcYawScale, static_cast<float>(RowIndex));

		for (int32 PointIndex = 0; PointIndex < PointsPerArc; ++PointIndex)
		{
			const float ArcAlpha = PointsPerArc == 1
				? 0.5f
				: static_cast<float>(PointIndex) / static_cast<float>(PointsPerArc - 1);
			const float CenteredAlpha = ArcAlpha - 0.5f;
			const float PointYaw = DirectionToThreatYaw + CenteredAlpha * RowAngle;
			const float DistanceJitter = RowDistanceBase * Request.DefenseArcCandidateParams.ArcDistanceJitterRatio
				* RandomStream.FRandRange(-1.f, 1.f);
			const FVector Direction = FRotator(0.f, PointYaw, 0.f).Vector();
			FVector CandidateLocation = AnchorLocation3D + Direction * (RowDistanceBase + DistanceJitter);
			CandidateLocation.X += RandomStream.FRandRange(
				-Request.DefenseArcCandidateParams.PositionJitterXY,
				Request.DefenseArcCandidateParams.PositionJitterXY);
			CandidateLocation.Y += RandomStream.FRandRange(
				-Request.DefenseArcCandidateParams.PositionJitterXY,
				Request.DefenseArcCandidateParams.PositionJitterXY);

			const float YawOffset = CenteredAlpha
				* Request.DefenseArcCandidateParams.MaxYawOffsetFromThreatDegrees
				* RowYawScale;
			const float CandidateYaw = (ThreatLocation - CandidateLocation).Rotation().Yaw + YawOffset
				+ RandomStream.FRandRange(
					-Request.DefenseArcCandidateParams.YawJitterDegrees,
					Request.DefenseArcCandidateParams.YawJitterDegrees);
			TryAddDefenseArcCandidate(
				Request,
				Cluster,
				Anchor.Location,
				CandidateLocation,
				CandidateYaw,
				OutDefenseCandidates);
		}
	}

	void BuildDefenseArcCandidatesForBase(
		const FFindEnemyBaseClusters& Request,
		const FEnemyBaseClusterCandidate& Cluster,
		FRandomStream& RandomStream,
		TArray<FDefensePositions>& OutDefenseCandidates)
	{
		const TArray<FVector> ThreatLocations = BuildMergedThreatLocationsForBase(Request, Cluster);
		const int32 StartCandidateCount = OutDefenseCandidates.Num();
		const int32 MaxCandidatesPerBase = FMath::Max(0, Request.DefenseArcCandidateParams.MaxDefensePointCandidatesPerBase);
		for (const FVector& ThreatLocation : ThreatLocations)
		{
			const TArray<FDefenseArcAnchorCandidate> Anchors = BuildDefenseArcAnchors(Request, Cluster, ThreatLocation);
			for (const FDefenseArcAnchorCandidate& Anchor : Anchors)
			{
				const int32 ArcRows = FMath::Max(0, Request.DefenseArcCandidateParams.ArcRowsPerAnchor);
				for (int32 RowIndex = 0; RowIndex < ArcRows; ++RowIndex)
				{
					BuildDefenseArcRowCandidates(
						Request,
						Cluster,
						Anchor,
						ThreatLocation,
						RowIndex,
						RandomStream,
						OutDefenseCandidates);

					if (MaxCandidatesPerBase > 0 && OutDefenseCandidates.Num() - StartCandidateCount >= MaxCandidatesPerBase)
					{
						return;
					}
				}
			}
		}
	}

	void RemoveDefenseCandidatesTooCloseToAlliedUnits(
		const FFindEnemyBaseClusters& Request,
		const TArray<FVector>& AlliedUnitLocations,
		TArray<FDefensePositions>& DefenseCandidates)
	{
		const float MinDistance = FMath::Max(0.f, Request.DefenseArcCandidateParams.MinDistanceDefPositionFromAlliedUnits);
		if (MinDistance <= 0.f || AlliedUnitLocations.IsEmpty())
		{
			return;
		}

		const float MinDistanceSq = FMath::Square(MinDistance);
		DefenseCandidates.RemoveAll([&](const FDefensePositions& DefenseCandidate)
		{
			return AlliedUnitLocations.ContainsByPredicate([&](const FVector& AlliedUnitLocation)
			{
				return FVector::DistSquared2D(DefenseCandidate.Location, AlliedUnitLocation) < MinDistanceSq;
			});
		});
	}

	void RemoveDuplicateDefenseCandidates(
		const FFindEnemyBaseClusters& Request,
		TArray<FDefensePositions>& DefenseCandidates)
	{
		const float MergeDistance = FMath::Max(0.f, Request.DefenseArcCandidateParams.DuplicateCandidateMergeDistanceXY);
		if (MergeDistance <= 0.f)
		{
			return;
		}

		TArray<FDefensePositions> UniqueCandidates;
		UniqueCandidates.Reserve(DefenseCandidates.Num());
		for (const FDefensePositions& DefenseCandidate : DefenseCandidates)
		{
			if (GetIsFarEnoughFromDefenseCandidates(DefenseCandidate.Location, UniqueCandidates, MergeDistance))
			{
				UniqueCandidates.Add(DefenseCandidate);
			}
		}

		DefenseCandidates = MoveTemp(UniqueCandidates);
	}

	void BuildDefenseArcCandidatesForBases(
		const FFindEnemyBaseClusters& Request,
		const TArray<FEnemyBaseClusterCandidate>& Clusters,
		const TArray<FVector>& AlliedUnitLocations,
		TArray<FDefensePositions>& OutDefenseCandidates)
	{
		if (Request.PlayerUnitLocationsToDefendAgainst.IsEmpty())
		{
			return;
		}

		FRandomStream RandomStream(
			static_cast<int32>(FPlatformTime::Cycles64())
			^ Request.RequestID
			^ Request.PlayerUnitLocationsToDefendAgainst.Num());
		for (const FEnemyBaseClusterCandidate& Cluster : Clusters)
		{
			BuildDefenseArcCandidatesForBase(Request, Cluster, RandomStream, OutDefenseCandidates);
		}

		RemoveDuplicateDefenseCandidates(Request, OutDefenseCandidates);
		RemoveDefenseCandidatesTooCloseToAlliedUnits(Request, AlliedUnitLocations, OutDefenseCandidates);
		const int32 MaxCandidatesTotal = FMath::Max(0, Request.DefenseArcCandidateParams.MaxDefensePointCandidatesTotal);
		if (MaxCandidatesTotal > 0 && OutDefenseCandidates.Num() > MaxCandidatesTotal)
		{
			OutDefenseCandidates.SetNum(MaxCandidatesTotal);
		}
	}

	struct FConstructionLocationCandidate
	{
		FVector Location = FVector::ZeroVector;
		float Priority = 0.f;
	};

	bool TryGetClosestPlayerBulkLocation(
		const FVector& DefenseLocation,
		const TArray<FVector>& PlayerBulkLocations,
		FVector& OutPlayerBulkLocation)
	{
		float BestDistanceSq = FLT_MAX;
		bool bHasFoundLocation = false;
		for (const FVector& PlayerBulkLocation : PlayerBulkLocations)
		{
			if (GetIsNearZeroStrategicLocation(PlayerBulkLocation))
			{
				continue;
			}

			const float DistanceSq = FVector::DistSquared2D(DefenseLocation, PlayerBulkLocation);
			if (DistanceSq >= BestDistanceSq)
			{
				continue;
			}

			BestDistanceSq = DistanceSq;
			OutPlayerBulkLocation = PlayerBulkLocation;
			bHasFoundLocation = true;
		}

		return bHasFoundLocation;
	}

	FVector GetDirectionFromDefenseToPlayer(const FDefensePositions& DefensePosition, const FVector& PlayerBulkLocation)
	{
		FVector DirectionToPlayer = PlayerBulkLocation - DefensePosition.Location;
		DirectionToPlayer.Z = 0.f;
		if (not DirectionToPlayer.IsNearlyZero())
		{
			return DirectionToPlayer.GetSafeNormal();
		}

		FVector FallbackDirection = FRotator(0.f, DefensePosition.YawRotation, 0.f).Vector();
		FallbackDirection.Z = 0.f;
		return FallbackDirection.GetSafeNormal();
	}

	TArray<float> GetConstructionArcOffsets(const FFindConstructionLocations& Request)
	{
		TArray<float> Offsets;
		const float MinOffset = FMath::Max(
			0.f,
			FMath::Min(Request.MinOffsetTowardsPlayer, Request.MaxOffsetTowardsPlayer));
		const float MaxOffset = FMath::Max(
			0.f,
			FMath::Max(Request.MinOffsetTowardsPlayer, Request.MaxOffsetTowardsPlayer));
		if (MaxOffset > 0.f)
		{
			Offsets.Add(MaxOffset);
		}

		if (MinOffset > 0.f
			&& FMath::Abs(MaxOffset - MinOffset)
			> StrategicAIHelperConstants::ConstructionArcDuplicateOffsetTolerance)
		{
			Offsets.Add(MinOffset);
		}

		return Offsets;
	}

	int32 GetConstructionArcPointCount(const FFindConstructionLocations& Request, const float OffsetDistance)
	{
		const float ClampedArcAngleDegrees = FMath::Clamp(
			Request.ArcAngleDegrees,
			0.f,
			StrategicAIHelperConstants::MaxConstructionArcAngleDegrees);
		const float ArcLength = 2.f * PI * OffsetDistance * (ClampedArcAngleDegrees / 360.f);
		const float RequestedDensity = FMath::Max(0.f, Request.PlacementDensity);
		const int32 PointCount = FMath::CeilToInt(
			(ArcLength / StrategicAIHelperConstants::ConstructionArcUnitsPerDensityStep) * RequestedDensity);
		return FMath::Max(3, PointCount);
	}

	void AppendConstructionArcCandidates(
		const FFindConstructionLocations& Request,
		const FDefensePositions& DefensePosition,
		const FVector& PlayerBulkLocation,
		const float OffsetDistance,
		TArray<FConstructionLocationCandidate>& OutCandidates)
	{
		const FVector DirectionToPlayer = GetDirectionFromDefenseToPlayer(DefensePosition, PlayerBulkLocation);
		if (DirectionToPlayer.IsNearlyZero())
		{
			return;
		}

		const float ClampedArcAngleDegrees = FMath::Clamp(
			Request.ArcAngleDegrees,
			0.f,
			StrategicAIHelperConstants::MaxConstructionArcAngleDegrees);
		const float HalfArcAngleRadians = FMath::DegreesToRadians(ClampedArcAngleDegrees * 0.5f);
		const float HalfChordLength = FMath::Tan(HalfArcAngleRadians) * OffsetDistance;
		const float ArcDepth = FMath::Max(0.f, Request.ArcDepth);
		const int32 PointCount = GetConstructionArcPointCount(Request, OffsetDistance);
		const FVector DirectionToDefense = -DirectionToPlayer;
		const FVector RightVector(-DirectionToPlayer.Y, DirectionToPlayer.X, 0.f);
		const FVector ArcCenter = DefensePosition.Location + DirectionToPlayer * OffsetDistance;

		for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
		{
			const float ArcAlpha = static_cast<float>(PointIndex) / static_cast<float>(PointCount - 1);
			const float CenteredAlpha = (ArcAlpha - 0.5f) * 2.f;
			const float LateralOffset = CenteredAlpha * HalfChordLength;
			const float ParabolicDepth = ArcDepth * (1.f - FMath::Square(CenteredAlpha));

			FConstructionLocationCandidate Candidate;
			Candidate.Location = ArcCenter + RightVector * LateralOffset + DirectionToDefense * ParabolicDepth;
			Candidate.Location.Z = DefensePosition.Location.Z;
			Candidate.Priority = OffsetDistance;
			OutCandidates.Add(Candidate);
		}
	}

	void AppendConstructionCandidatesForDefensePosition(
		const FFindConstructionLocations& Request,
		const FDefensePositions& DefensePosition,
		const TArray<float>& ArcOffsets,
		TArray<FConstructionLocationCandidate>& OutCandidates)
	{
		if (GetIsNearZeroStrategicLocation(DefensePosition.Location))
		{
			return;
		}

		FVector PlayerBulkLocation = FVector::ZeroVector;
		if (not TryGetClosestPlayerBulkLocation(DefensePosition.Location, Request.PlayerBulkLocations, PlayerBulkLocation))
		{
			return;
		}

		for (const float ArcOffset : ArcOffsets)
		{
			AppendConstructionArcCandidates(Request, DefensePosition, PlayerBulkLocation, ArcOffset, OutCandidates);
		}
	}

	bool GetIsFarEnoughFromAcceptedConstructionLocations(
		const FVector& CandidateLocation,
		const TArray<FVector>& AcceptedLocations,
		const float CleanupDistanceSq)
	{
		return not AcceptedLocations.ContainsByPredicate(
			[&](const FVector& AcceptedLocation)
			{
				return FVector::DistSquared2D(CandidateLocation, AcceptedLocation) < CleanupDistanceSq;
			});
	}

	TArray<FVector> CleanupConstructionLocationCandidates(
		TArray<FConstructionLocationCandidate> Candidates,
		const float CleanupDistance)
	{
		Candidates.Sort([](const FConstructionLocationCandidate& Left, const FConstructionLocationCandidate& Right)
		{
			return Left.Priority > Right.Priority;
		});

		TArray<FVector> CleanedLocations;
		CleanedLocations.Reserve(Candidates.Num());
		const float CleanupDistanceSq = FMath::Square(FMath::Max(0.f, CleanupDistance));
		for (const FConstructionLocationCandidate& Candidate : Candidates)
		{
			if (GetIsNearZeroStrategicLocation(Candidate.Location))
			{
				continue;
			}

			if (CleanupDistanceSq > 0.f
				&& not GetIsFarEnoughFromAcceptedConstructionLocations(Candidate.Location, CleanedLocations, CleanupDistanceSq))
			{
				continue;
			}

			CleanedLocations.Add(Candidate.Location);
		}

		return CleanedLocations;
	}

	TArray<FVector> BuildCleanedConstructionLocations(const FFindConstructionLocations& Request)
	{
		const TArray<float> ArcOffsets = GetConstructionArcOffsets(Request);
		if (ArcOffsets.IsEmpty() || Request.DefensePositions.IsEmpty() || Request.PlayerBulkLocations.IsEmpty()
			|| Request.PlacementDensity <= 0.f || Request.ArcAngleDegrees <= 0.f)
		{
			return {};
		}

		TArray<FConstructionLocationCandidate> Candidates;
		for (const FDefensePositions& DefensePosition : Request.DefensePositions)
		{
			AppendConstructionCandidatesForDefensePosition(Request, DefensePosition, ArcOffsets, Candidates);
		}

		return CleanupConstructionLocationCandidates(MoveTemp(Candidates), Request.CleanupDistance);
	}

}

FResultEnemyBaseClusters FStrategicAIHelpers::BuildEnemyBaseClustersResult(
	const FFindEnemyBaseClusters& Request,
	const TArray<FAsyncDetailedUnitState>& DetailedUnitStates)
{
	FResultEnemyBaseClusters Result;
	Result.RequestID = Request.RequestID;
	if (Request.CoreBuildingTypes.IsEmpty())
	{
		return Result;
	}

	TArray<StrategicAIHelperUtilities::FEnemyCoreBuildingClusterPoint> CorePoints;
	TArray<FVector2D> SatellitePoints;
	TArray<FVector> AlliedUnitLocations;
	StrategicAIHelperUtilities::GatherEnemyBaseRequestPoints(
		Request,
		DetailedUnitStates,
		CorePoints,
		SatellitePoints,
		AlliedUnitLocations);

	const TArray<StrategicAIHelperUtilities::FEnemyBaseClusterCandidate> BaseClusters =
		StrategicAIHelperUtilities::BuildAcceptedEnemyBaseClusters(Request, CorePoints, SatellitePoints);
	Result.BasePoints.Reserve(BaseClusters.Num());
	for (const StrategicAIHelperUtilities::FEnemyBaseClusterCandidate& BaseCluster : BaseClusters)
	{
		Result.BasePoints.Add(StrategicAIHelperUtilities::BuildEnemyBasePointCoreBuildings(BaseCluster));
	}

	StrategicAIHelperUtilities::RemoveNearZeroStrategicEnemyBasePoints(Result.BasePoints);
	StrategicAIHelperUtilities::BuildDefenseArcCandidatesForBases(
		Request,
		BaseClusters,
		AlliedUnitLocations,
		Result.DefensePointCandidates);

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

	bool GetIsLocationWithinRetreatGroup(
		const FDamagedTankGroupBuilder& GroupBuilder,
		const FVector& UnitLocation,
		const float MaxDistanceFromOtherGroupMembers)
	{
		for (const FVector& GroupMemberLocation : GroupBuilder.Locations)
		{
			if (FVector::Dist(GroupMemberLocation, UnitLocation) <= MaxDistanceFromOtherGroupMembers)
			{
				return true;
			}
		}

		return false;
	}

	bool TryAddDamagedTankToExistingRetreatGroup(
		TArray<FDamagedTankGroupBuilder>& Groups,
		const FAsyncDetailedUnitState& UnitState,
		const float MaxDistanceFromOtherGroupMembers)
	{
		for (FDamagedTankGroupBuilder& GroupBuilder : Groups)
		{
			if (not GetIsLocationWithinRetreatGroup(
				GroupBuilder,
				UnitState.UnitLocation,
				MaxDistanceFromOtherGroupMembers))
			{
				continue;
			}

			GroupBuilder.Group.DamagedTanks.Add(UnitState.UnitActorPtr);
			GroupBuilder.Locations.Add(UnitState.UnitLocation);
			return true;
		}

		return false;
	}

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

			if (TryAddDamagedTankToExistingRetreatGroup(
				Groups,
				*UnitState,
				Request.MaxDistanceFromOtherGroupMembers))
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
	FResultClosestFlankableEnemyHeavy Result;
	Result.RequestID = Request.RequestID;
	Result.FlankLocationsAroundHeavyTank.Reserve(DetailedUnitStates.Num());

	for (const FAsyncDetailedUnitState& UnitState : DetailedUnitStates)
	{
		if (UnitState.OwningPlayer != StrategicAIHelperConstants::PlayerOwningId)
		{
			continue;
		}
		if (not UnitState.bIsInCombat)
		{
			continue;
		}
		if (not FStrategicAIHelpers::GetIsFlankableHeavyTank(UnitState))
		{
			continue;
		}

		FWeakActorLocations FlankLocations;
		FlankLocations.Actor = UnitState.UnitActorPtr;
		FlankLocations.Locations = StrategicAIHelperUtilities::BuildFlankPositions(UnitState, Request);
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

FResultConstructionLocations FStrategicAIHelpers::BuildConstructionLocationsResult(
	const FFindConstructionLocations& Request)
{
	FResultConstructionLocations Result;
	Result.RequestID = Request.RequestID;
	Result.ConstructionLocations = StrategicAIHelperUtilities::BuildCleanedConstructionLocations(Request);
	return Result;
}
