// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#include "PCGCreateReinforcementRoads.h"

#include "Algo/Reverse.h"
#include "PCGComponent.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGVolumeData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

#include "CollisionQueryParams.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableWireComponent/DestructableWireComp.h"

#define LOCTEXT_NAMESPACE "PCGCreateReinforcementRoads"

namespace ReinforcementRoadConstants
{
	const FName ReinforcementStartsPin = TEXT("ReinforcementStarts");
	// Kept intentionally for compatibility with the pin spelling requested for the graph.
	const FName OrpahnedRoadPointsPin = TEXT("OrpahnedRoadPoints");
	const FName BackupRoadEndsPin = TEXT("BackupRoadEnds");
	const FName ExcludedVolumesPin = TEXT("ExcludedVolumes");
	const FName RoadPointsPin = TEXT("RoadPoints");
	const FName RoadBoundsVolumePin = TEXT("RoadBoundsVolume");

	const FName RoadIndexAttribute = TEXT("RoadIndex");
	const FName PointIndexAttribute = TEXT("PointIndex");
	const FName ConnectionTypeAttribute = TEXT("ConnectionType");

	constexpr double ReinforcementGroundTraceUp = 15000.0;
	constexpr double ReinforcementGroundTraceDown = 40000.0;
	constexpr double ReinforcementRoadZOffset = 10.0;
	constexpr double GroundSampleSpacing = 300.0;
	constexpr double RouteTestSpacing = 125.0;
	constexpr double GridCellSize = 400.0;
	constexpr double GridMargin = 6000.0;
	constexpr int32 MaxGridDimension = 160;
	constexpr double DefaultRoadHalfWidth = 250.0;
	constexpr double MinimumClearance = 100.0;
	// CurveClamped interpolation can leave the sampled polyline slightly; keep that bend outside exclusions too.
	constexpr double SplineInterpolationSafetyMargin = 150.0;
	constexpr double MaximumDetourRatio = 4.0;
	constexpr double MaximumStartTurnDegrees = 115.0;
	constexpr double MaximumRoadGrade = 0.18;
	constexpr double MinimumMeshLength = 100.0;
	constexpr double DecorativeAmplitudePerCurvature = 300.0;
	constexpr int32 DecorativeAttempts = 5;
	constexpr double MinimumOrphanApproachLength = 500.0;
	constexpr double MaximumOrphanApproachLength = 2400.0;
	constexpr double OrphanApproachDistanceFactor = 0.35;
	constexpr double OrphanApproachHandleFactor = 0.4;
	constexpr double OrphanApproachSampleSpacing = 150.0;
	constexpr double OrphanApproachAttemptScales[] = {1.0, 0.75, 0.5};
	constexpr double MinimumPoleSpacing = 100.0;
	constexpr double TerrainSimplificationCostTolerance = 1.05;
	constexpr double MinimumTerrainGrade = 0.01;
	constexpr double MinimumElevationRange = 1.0;
	constexpr double MinimumEndpointTerrainTransitionLength = 200.0;
	constexpr double MaximumEndpointTerrainTransitionLength = 800.0;
	constexpr double EndpointTerrainTransitionLengthFactor = 0.15;
	constexpr double MaximumEndpointTerrainGrade = 0.35;

	enum class EConnectionType : uint8
	{
		Orphan = 0,
		ReinforcementStart = 1,
		Backup = 2
	};

	enum class ETerrainHeightSampleState : uint8
	{
		NotSampled,
		Valid,
		Invalid
	};
}

namespace
{
	using namespace ReinforcementRoadConstants;

	struct FEndpoint
	{
		FVector Position = FVector::ZeroVector;
		FVector2D Forward = FVector2D::UnitX();
		int32 SourceIndex = INDEX_NONE;
	};

	struct FRouteCandidate
	{
		FEndpoint Endpoint;
		EConnectionType Type = EConnectionType::Orphan;
		double Score = 0.0;
	};

	struct FGeneratedRoute
	{
		TArray<FVector> Points;
		EConnectionType Type = EConnectionType::Orphan;
	};

	class FExclusionTester
	{
	public:
		explicit FExclusionTester(TArray<const UPCGSpatialData*>&& InSpatialData)
			: M_SpatialData(MoveTemp(InSpatialData))
		{
		}

		bool IsPositionExcluded(const FVector2D& Position, const double Clearance) const
		{
			const FVector SampleExtent(Clearance, Clearance, Clearance);
			const FBox SampleBounds(-SampleExtent, SampleExtent);
			for (const UPCGSpatialData* SpatialData : M_SpatialData)
			{
				const FBox SpatialBounds = SpatialData->GetBounds();
				if (Position.X < SpatialBounds.Min.X - Clearance || Position.X > SpatialBounds.Max.X + Clearance
					|| Position.Y < SpatialBounds.Min.Y - Clearance || Position.Y > SpatialBounds.Max.Y + Clearance)
				{
					continue;
				}

				FPCGPoint SampledPoint;
				const FVector SamplePosition(Position, SpatialBounds.GetCenter().Z);
				if (SpatialData->SamplePoint(FTransform(SamplePosition), SampleBounds, SampledPoint, nullptr)
					&& SampledPoint.Density > 0.0f)
				{
					return true;
				}
			}
			return false;
		}

		bool IsSegmentClear(const FVector2D& Start, const FVector2D& End, const double Clearance) const
		{
			const double Length = FVector2D::Distance(Start, End);
			const int32 NumSamples = FMath::Max(1, FMath::CeilToInt32(Length / RouteTestSpacing));
			for (int32 SampleIndex = 0; SampleIndex <= NumSamples; ++SampleIndex)
			{
				const double Alpha = static_cast<double>(SampleIndex) / NumSamples;
				if (IsPositionExcluded(FMath::Lerp(Start, End, Alpha), Clearance))
				{
					return false;
				}
			}
			return true;
		}

		bool IsPathClear(const TArray<FVector2D>& Points, const double Clearance) const
		{
			for (int32 PointIndex = 1; PointIndex < Points.Num(); ++PointIndex)
			{
				if (not IsSegmentClear(Points[PointIndex - 1], Points[PointIndex], Clearance))
				{
					return false;
				}
			}
			return true;
		}

	private:
		TArray<const UPCGSpatialData*> M_SpatialData;
	};

	class FTerrainRouteTester
	{
	public:
		FTerrainRouteTester(
			UWorld& InWorld,
			const FVector& Start,
			const FVector& End,
			const UPCGCreateReinforcementRoadsSettings& Settings)
			: M_World(InWorld),
			  M_MaximumGrade(FMath::Max(MinimumTerrainGrade, static_cast<double>(Settings.MaximumTerrainGrade))),
			  M_HillAvoidanceStrength(FMath::Max(0.0, static_cast<double>(Settings.HillAvoidanceStrength))),
			  M_StartPosition(Start),
			  M_EndPosition(End),
			  M_EndpointTransitionLength(FMath::Clamp(
				  FVector2D::Distance(M_StartPosition, M_EndPosition) * EndpointTerrainTransitionLengthFactor,
				  MinimumEndpointTerrainTransitionLength, MaximumEndpointTerrainTransitionLength))
		{
			for (TActorIterator<ALandscapeProxy> LandscapeIterator(&M_World); LandscapeIterator; ++LandscapeIterator)
			{
				ALandscapeProxy* Landscape = *LandscapeIterator;
				if (IsValid(Landscape))
				{
					M_Landscapes.Add(Landscape);
				}
			}

			double StartHeight = Start.Z;
			double EndHeight = End.Z;
			TryGetGroundHeight(FVector2D(Start), StartHeight);
			TryGetGroundHeight(FVector2D(End), EndHeight);
			M_MaximumEndpointHeight = FMath::Max(StartHeight, EndHeight);
			M_MaximumAllowedHeight = M_MaximumEndpointHeight
				+ FMath::Max(0.0, static_cast<double>(Settings.MaximumElevationAboveEndpoints));
		}

		bool TryGetGroundHeight(const FVector2D& Position, double& OutHeight) const
		{
			for (const TWeakObjectPtr<ALandscapeProxy>& LandscapePointer : M_Landscapes)
			{
				const ALandscapeProxy* Landscape = LandscapePointer.Get();
				if (not IsValid(Landscape))
				{
					continue;
				}

				const TOptional<float> LandscapeHeight = Landscape->GetHeightAtLocation(FVector(Position, 0.0));
				if (LandscapeHeight.IsSet())
				{
					OutHeight = LandscapeHeight.GetValue();
					return true;
				}
			}

			FHitResult Hit;
			const FVector TraceStart(Position, ReinforcementGroundTraceUp);
			const FVector TraceEnd(Position, -ReinforcementGroundTraceDown);
			if (not M_World.LineTraceSingleByChannel(
				Hit, TraceStart, TraceEnd, ECC_WorldStatic, M_TraceParams))
			{
				return false;
			}

			OutHeight = Hit.ImpactPoint.Z;
			return true;
		}

		bool TryGetTraversalCost(
			const FVector2D& Start,
			const double StartHeight,
			const FVector2D& End,
			const double EndHeight,
			double& OutCost) const
		{
			const double HorizontalDistance = FVector2D::Distance(Start, End);
			if (HorizontalDistance <= UE_DOUBLE_SMALL_NUMBER
				|| StartHeight > M_MaximumAllowedHeight || EndHeight > M_MaximumAllowedHeight)
			{
				return false;
			}

			const double Grade = FMath::Abs(EndHeight - StartHeight) / HorizontalDistance;
			const FVector2D SegmentCenter = (Start + End) * 0.5;
			const bool bIsEndpointTransition = FVector2D::Distance(SegmentCenter, M_StartPosition)
				<= M_EndpointTransitionLength
				|| FVector2D::Distance(SegmentCenter, M_EndPosition) <= M_EndpointTransitionLength;
			const double AllowedGrade = bIsEndpointTransition
				? FMath::Max(M_MaximumGrade, MaximumEndpointTerrainGrade)
				: M_MaximumGrade;
			if (Grade > AllowedGrade)
			{
				return false;
			}

			const double AverageHeight = (StartHeight + EndHeight) * 0.5;
			const double ElevationAboveEndpoints = FMath::Max(0.0, AverageHeight - M_MaximumEndpointHeight);
			const double ElevationRange = FMath::Max(
				MinimumElevationRange, M_MaximumAllowedHeight - M_MaximumEndpointHeight);
			const double ElevationRatio = ElevationAboveEndpoints / ElevationRange;
			OutCost = HorizontalDistance
				* (1.0 + M_HillAvoidanceStrength * (Grade + ElevationRatio));
			return true;
		}

		bool TryGetSegmentCost(
			const FVector2D& Start,
			const FVector2D& End,
			double& OutCost) const
		{
			const double Length = FVector2D::Distance(Start, End);
			const int32 SampleCount = FMath::Max(1, FMath::CeilToInt32(Length / RouteTestSpacing));
			FVector2D PreviousPosition = Start;
			double PreviousHeight = 0.0;
			if (not TryGetGroundHeight(PreviousPosition, PreviousHeight))
			{
				return false;
			}

			OutCost = 0.0;
			for (int32 SampleIndex = 1; SampleIndex <= SampleCount; ++SampleIndex)
			{
				const FVector2D Position = FMath::Lerp(
					Start, End, static_cast<double>(SampleIndex) / SampleCount);
				double Height = 0.0;
				double StepCost = 0.0;
				if (not TryGetGroundHeight(Position, Height)
					|| not TryGetTraversalCost(PreviousPosition, PreviousHeight, Position, Height, StepCost))
				{
					return false;
				}

				OutCost += StepCost;
				PreviousPosition = Position;
				PreviousHeight = Height;
			}
			return true;
		}

		bool IsPathTraversable(const TArray<FVector2D>& Points) const
		{
			for (int32 PointIndex = 1; PointIndex < Points.Num(); ++PointIndex)
			{
				double SegmentCost = 0.0;
				if (not TryGetSegmentCost(Points[PointIndex - 1], Points[PointIndex], SegmentCost))
				{
					return false;
				}
			}
			return true;
		}

	private:
		UWorld& M_World;
		FCollisionQueryParams M_TraceParams;
		TArray<TWeakObjectPtr<ALandscapeProxy>> M_Landscapes;
		double M_MaximumGrade = 0.08;
		double M_HillAvoidanceStrength = 250.0;
		FVector2D M_StartPosition = FVector2D::ZeroVector;
		FVector2D M_EndPosition = FVector2D::ZeroVector;
		double M_EndpointTransitionLength = MinimumEndpointTerrainTransitionLength;
		double M_MaximumEndpointHeight = 0.0;
		double M_MaximumAllowedHeight = 0.0;
	};

	TArray<FEndpoint> CollectEndpoints(FPCGContext& Context, const FName Pin)
	{
		TArray<FEndpoint> Endpoints;
		const TArray<FPCGTaggedData> Inputs = Context.InputData.GetInputsByPin(Pin);
		for (const FPCGTaggedData& Input : Inputs)
		{
			const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data);
			if (PointData == nullptr)
			{
				continue;
			}

			for (const FPCGPoint& Point : PointData->GetPoints())
			{
				FEndpoint& Endpoint = Endpoints.Emplace_GetRef();
				Endpoint.Position = Point.Transform.GetLocation();
				const FVector Forward3D = Point.Transform.GetRotation().GetForwardVector();
				Endpoint.Forward = FVector2D(Forward3D.X, Forward3D.Y).GetSafeNormal();
				if (Endpoint.Forward.IsNearlyZero())
				{
					Endpoint.Forward = FVector2D::UnitX();
				}
				Endpoint.SourceIndex = Endpoints.Num() - 1;
			}
		}
		return Endpoints;
	}

	FExclusionTester BuildExclusionTester(FPCGContext& Context)
	{
		TArray<const UPCGSpatialData*> SpatialInputs;
		for (const FPCGTaggedData& Input : Context.InputData.GetInputsByPin(ExcludedVolumesPin))
		{
			if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data))
			{
				SpatialInputs.Add(SpatialData);
			}
		}
		return FExclusionTester(MoveTemp(SpatialInputs));
	}

	double ComputeRoadClearance(const UStaticMesh* RoadMesh)
	{
		if (RoadMesh == nullptr)
		{
			return DefaultRoadHalfWidth;
		}
		return FMath::Max(MinimumClearance, static_cast<double>(RoadMesh->GetBoundingBox().GetExtent().Y));
	}

	double ComputeMeshLength(const UStaticMesh* RoadMesh)
	{
		if (RoadMesh == nullptr)
		{
			return MinimumMeshLength;
		}
		return FMath::Max(MinimumMeshLength, static_cast<double>(RoadMesh->GetBoundingBox().GetSize().X));
	}

	bool IsNaturalEndpointDirection(const FEndpoint& Start, const FEndpoint& End)
	{
		const FVector2D Direction = (FVector2D(End.Position) - FVector2D(Start.Position)).GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			return false;
		}
		const double MinimumStartDot = FMath::Cos(FMath::DegreesToRadians(MaximumStartTurnDegrees));
		return FVector2D::DotProduct(Start.Forward, Direction) >= MinimumStartDot;
	}

	double CandidateScore(
		const FEndpoint& Start,
		const FEndpoint& End,
		const EConnectionType ConnectionType)
	{
		const FVector2D Direction = (FVector2D(End.Position) - FVector2D(Start.Position)).GetSafeNormal();
		const double Distance = FVector2D::Distance(FVector2D(Start.Position), FVector2D(End.Position));
		const double StartTurnPenalty = (1.0 - FVector2D::DotProduct(Start.Forward, Direction)) * 1000.0;
		const double EndDirectionDot = FVector2D::DotProduct(End.Forward, Direction);
		// Orphan forward points out of the city, so prefer starts on that side of the orphan road.
		const double EndAlignmentPenalty = ConnectionType == EConnectionType::Orphan
			? (1.0 + EndDirectionDot) * 500.0
			: (1.0 - FMath::Abs(EndDirectionDot)) * 500.0;
		return Distance + StartTurnPenalty + EndAlignmentPenalty;
	}

	void AddPrimaryCandidates(
		const FEndpoint& Start,
		const TArray<FEndpoint>& Orphans,
		const TArray<bool>& UsedOrphans,
		const TArray<FEndpoint>& Starts,
		const TArray<bool>& UsedStarts,
		TArray<FRouteCandidate>& OutCandidates)
	{
		for (int32 OrphanIndex = 0; OrphanIndex < Orphans.Num(); ++OrphanIndex)
		{
			if (UsedOrphans[OrphanIndex] || not IsNaturalEndpointDirection(Start, Orphans[OrphanIndex]))
			{
				continue;
			}
			OutCandidates.Add({Orphans[OrphanIndex], EConnectionType::Orphan,
				CandidateScore(Start, Orphans[OrphanIndex], EConnectionType::Orphan)});
		}

		for (int32 StartIndex = 0; StartIndex < Starts.Num(); ++StartIndex)
		{
			if (UsedStarts[StartIndex] || StartIndex == Start.SourceIndex
				|| not IsNaturalEndpointDirection(Start, Starts[StartIndex]))
			{
				continue;
			}
			OutCandidates.Add({Starts[StartIndex], EConnectionType::ReinforcementStart,
				CandidateScore(Start, Starts[StartIndex], EConnectionType::ReinforcementStart)});
		}
	}

	void AddBackupCandidates(
		const FEndpoint& Start,
		const TArray<FEndpoint>& Backups,
		const TArray<bool>& UsedBackups,
		TArray<FRouteCandidate>& OutCandidates)
	{
		for (int32 BackupIndex = 0; BackupIndex < Backups.Num(); ++BackupIndex)
		{
			if (UsedBackups[BackupIndex] || not IsNaturalEndpointDirection(Start, Backups[BackupIndex]))
			{
				continue;
			}
			OutCandidates.Add({Backups[BackupIndex], EConnectionType::Backup,
				CandidateScore(Start, Backups[BackupIndex], EConnectionType::Backup)});
		}
	}

	int32 GridIndex(const int32 GridX, const int32 GridY, const int32 Width)
	{
		return GridY * Width + GridX;
	}

	FIntPoint GridCoordinates(const int32 Index, const int32 Width)
	{
		return FIntPoint(Index % Width, Index / Width);
	}

	TArray<FVector2D> ReconstructGridPath(
		const TArray<int32>& Parents,
		const int32 EndIndex,
		const int32 Width,
		const FVector2D& GridOrigin)
	{
		TArray<FVector2D> ReversePath;
		for (int32 CurrentIndex = EndIndex; CurrentIndex != INDEX_NONE; CurrentIndex = Parents[CurrentIndex])
		{
			const FIntPoint GridPoint = GridCoordinates(CurrentIndex, Width);
			ReversePath.Add(GridOrigin + FVector2D(GridPoint) * GridCellSize);
		}
		Algo::Reverse(ReversePath);
		return ReversePath;
	}

	TArray<FVector2D> FindGridPath(
		const FVector2D& Start,
		const FVector2D& End,
		const FExclusionTester& Exclusions,
		const FTerrainRouteTester& Terrain,
		const double Clearance)
	{
		const FVector2D Minimum(
			FMath::Min(Start.X, End.X) - GridMargin,
			FMath::Min(Start.Y, End.Y) - GridMargin);
		const FVector2D Maximum(
			FMath::Max(Start.X, End.X) + GridMargin,
			FMath::Max(Start.Y, End.Y) + GridMargin);
		const int32 Width = FMath::Min(MaxGridDimension,
			FMath::Max(2, FMath::CeilToInt32((Maximum.X - Minimum.X) / GridCellSize) + 1));
		const int32 Height = FMath::Min(MaxGridDimension,
			FMath::Max(2, FMath::CeilToInt32((Maximum.Y - Minimum.Y) / GridCellSize) + 1));

		const FVector2D GridSpan((Width - 1) * GridCellSize, (Height - 1) * GridCellSize);
		const FVector2D Center = (Start + End) * 0.5;
		const FVector2D GridOrigin = Center - GridSpan * 0.5;
		auto ToGrid = [GridOrigin, Width, Height](const FVector2D& Position)
		{
			return FIntPoint(
				FMath::Clamp(FMath::RoundToInt32((Position.X - GridOrigin.X) / GridCellSize), 0, Width - 1),
				FMath::Clamp(FMath::RoundToInt32((Position.Y - GridOrigin.Y) / GridCellSize), 0, Height - 1));
		};

		const FIntPoint StartGrid = ToGrid(Start);
		const FIntPoint EndGrid = ToGrid(End);
		const int32 StartIndex = GridIndex(StartGrid.X, StartGrid.Y, Width);
		const int32 EndIndex = GridIndex(EndGrid.X, EndGrid.Y, Width);
		const int32 CellCount = Width * Height;
		auto GetPositionForIndex = [Start, End, StartIndex, EndIndex, Width, GridOrigin](const int32 Index)
		{
			if (Index == StartIndex)
			{
				return Start;
			}
			if (Index == EndIndex)
			{
				return End;
			}
			return GridOrigin + FVector2D(GridCoordinates(Index, Width)) * GridCellSize;
		};

		TArray<double> GroundHeights;
		GroundHeights.Init(0.0, CellCount);
		TArray<ETerrainHeightSampleState> GroundHeightStates;
		GroundHeightStates.Init(ETerrainHeightSampleState::NotSampled, CellCount);
		auto TryGetGridHeight = [&Terrain, &GroundHeights, &GroundHeightStates](
			const int32 Index,
			const FVector2D& Position,
			double& OutHeight)
		{
			if (GroundHeightStates[Index] == ETerrainHeightSampleState::NotSampled)
			{
				GroundHeightStates[Index] = Terrain.TryGetGroundHeight(Position, GroundHeights[Index])
					? ETerrainHeightSampleState::Valid
					: ETerrainHeightSampleState::Invalid;
			}
			OutHeight = GroundHeights[Index];
			return GroundHeightStates[Index] == ETerrainHeightSampleState::Valid;
		};

		TArray<double> Costs;
		Costs.Init(TNumericLimits<double>::Max(), CellCount);
		TArray<int32> Parents;
		Parents.Init(INDEX_NONE, CellCount);
		TArray<bool> Closed;
		Closed.Init(false, CellCount);
		TArray<int32> Open;
		Open.Add(StartIndex);
		Costs[StartIndex] = 0.0;
		double StartHeight = 0.0;
		if (not TryGetGridHeight(StartIndex, Start, StartHeight))
		{
			return {};
		}

		while (not Open.IsEmpty())
		{
			int32 BestOpenIndex = 0;
			double BestEstimate = TNumericLimits<double>::Max();
			for (int32 OpenIndex = 0; OpenIndex < Open.Num(); ++OpenIndex)
			{
				const FVector2D OpenPosition = GetPositionForIndex(Open[OpenIndex]);
				const double Estimate = Costs[Open[OpenIndex]]
					+ FVector2D::Distance(OpenPosition, End);
				if (Estimate < BestEstimate)
				{
					BestEstimate = Estimate;
					BestOpenIndex = OpenIndex;
				}
			}

			const int32 CurrentIndex = Open[BestOpenIndex];
			Open.RemoveAtSwap(BestOpenIndex, 1, EAllowShrinking::No);
			if (Closed[CurrentIndex])
			{
				continue;
			}
			if (CurrentIndex == EndIndex)
			{
				TArray<FVector2D> Path = ReconstructGridPath(Parents, EndIndex, Width, GridOrigin);
				Path[0] = Start;
				Path.Last() = End;
				return Path;
			}
			Closed[CurrentIndex] = true;

			const FIntPoint Current = GridCoordinates(CurrentIndex, Width);
			const FVector2D CurrentPosition = GetPositionForIndex(CurrentIndex);
			double CurrentHeight = 0.0;
			if (not TryGetGridHeight(CurrentIndex, CurrentPosition, CurrentHeight))
			{
				continue;
			}
			for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
			{
				for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
				{
					if (OffsetX == 0 && OffsetY == 0)
					{
						continue;
					}
					const FIntPoint Neighbor(Current.X + OffsetX, Current.Y + OffsetY);
					if (Neighbor.X < 0 || Neighbor.X >= Width || Neighbor.Y < 0 || Neighbor.Y >= Height)
					{
						continue;
					}

					const int32 NeighborIndex = GridIndex(Neighbor.X, Neighbor.Y, Width);
					const FVector2D NeighborPosition = GetPositionForIndex(NeighborIndex);
					if (Closed[NeighborIndex]
						|| (NeighborIndex != EndIndex && Exclusions.IsPositionExcluded(NeighborPosition, Clearance)))
					{
						continue;
					}

					double NeighborHeight = 0.0;
					double StepCost = 0.0;
					if (not TryGetGridHeight(NeighborIndex, NeighborPosition, NeighborHeight)
						|| not Terrain.TryGetTraversalCost(
							CurrentPosition, CurrentHeight, NeighborPosition, NeighborHeight, StepCost))
					{
						continue;
					}

					const double NewCost = Costs[CurrentIndex] + StepCost;
					if (NewCost >= Costs[NeighborIndex])
					{
						continue;
					}
					Costs[NeighborIndex] = NewCost;
					Parents[NeighborIndex] = CurrentIndex;
					Open.Add(NeighborIndex);
				}
			}
		}
		return {};
	}

	bool TryBuildCumulativeTerrainCosts(
		const TArray<FVector2D>& Path,
		const FTerrainRouteTester& Terrain,
		TArray<double>& OutCumulativeCosts)
	{
		OutCumulativeCosts.Init(0.0, Path.Num());
		for (int32 PointIndex = 1; PointIndex < Path.Num(); ++PointIndex)
		{
			double SegmentCost = 0.0;
			if (not Terrain.TryGetSegmentCost(Path[PointIndex - 1], Path[PointIndex], SegmentCost))
			{
				return false;
			}
			OutCumulativeCosts[PointIndex] = OutCumulativeCosts[PointIndex - 1] + SegmentCost;
		}
		return true;
	}

	TArray<FVector2D> SimplifyPath(
		const TArray<FVector2D>& Path,
		const FExclusionTester& Exclusions,
		const FTerrainRouteTester& Terrain,
		const double Clearance)
	{
		if (Path.Num() < 3)
		{
			return Path;
		}
		TArray<double> CumulativeTerrainCosts;
		if (not TryBuildCumulativeTerrainCosts(Path, Terrain, CumulativeTerrainCosts))
		{
			return {};
		}

		TArray<FVector2D> Simplified;
		Simplified.Add(Path[0]);
		int32 AnchorIndex = 0;
		while (AnchorIndex < Path.Num() - 1)
		{
			int32 FurthestVisible = AnchorIndex + 1;
			for (int32 CandidateIndex = Path.Num() - 1; CandidateIndex > AnchorIndex + 1; --CandidateIndex)
			{
				double DirectCost = 0.0;
				const double ExistingCost = CumulativeTerrainCosts[CandidateIndex]
					- CumulativeTerrainCosts[AnchorIndex];
				if (Exclusions.IsSegmentClear(Path[AnchorIndex], Path[CandidateIndex], Clearance)
					&& Terrain.TryGetSegmentCost(Path[AnchorIndex], Path[CandidateIndex], DirectCost)
					&& DirectCost <= ExistingCost * TerrainSimplificationCostTolerance)
				{
					FurthestVisible = CandidateIndex;
					break;
				}
			}
			Simplified.Add(Path[FurthestVisible]);
			AnchorIndex = FurthestVisible;
		}
		return Simplified;
	}

	double PathLength(const TArray<FVector2D>& Path)
	{
		double Length = 0.0;
		for (int32 PointIndex = 1; PointIndex < Path.Num(); ++PointIndex)
		{
			Length += FVector2D::Distance(Path[PointIndex - 1], Path[PointIndex]);
		}
		return Length;
	}

	TArray<FVector2D> DensifyPath(const TArray<FVector2D>& Path)
	{
		TArray<FVector2D> DensePath;
		for (int32 SegmentIndex = 1; SegmentIndex < Path.Num(); ++SegmentIndex)
		{
			const FVector2D SegmentStart = Path[SegmentIndex - 1];
			const FVector2D SegmentEnd = Path[SegmentIndex];
			const int32 NumSamples = FMath::Max(1,
				FMath::CeilToInt32(FVector2D::Distance(SegmentStart, SegmentEnd) / GroundSampleSpacing));
			const int32 FirstSample = SegmentIndex == 1 ? 0 : 1;
			for (int32 SampleIndex = FirstSample; SampleIndex <= NumSamples; ++SampleIndex)
			{
				DensePath.Add(FMath::Lerp(SegmentStart, SegmentEnd,
					static_cast<double>(SampleIndex) / NumSamples));
			}
		}
		return DensePath;
	}

	TArray<FVector2D> AddDecorativeCurves(
		const TArray<FVector2D>& BasePath,
		const float Curvature,
		const float CurvesPer1000Units,
		const int32 Seed,
		const FExclusionTester& Exclusions,
		const FTerrainRouteTester& Terrain,
		const double Clearance)
	{
		TArray<FVector2D> DensePath = DensifyPath(BasePath);
		const double TotalLength = PathLength(DensePath);
		if (DensePath.Num() < 3 || Curvature <= 0.0f || CurvesPer1000Units <= 0.0f)
		{
			return DensePath;
		}

		const double CurveCount = FMath::Max(0.5, TotalLength * CurvesPer1000Units / 1000.0);
		FRandomStream Random(Seed);
		const double Phase = Random.FRandRange(0.0f, 2.0f * UE_PI);
		double Amplitude = FMath::Min(DecorativeAmplitudePerCurvature * Curvature, TotalLength * 0.12);

		for (int32 AttemptIndex = 0; AttemptIndex < DecorativeAttempts; ++AttemptIndex)
		{
			TArray<FVector2D> CurvedPath;
			CurvedPath.Reserve(DensePath.Num());
			double DistanceAlongPath = 0.0;
			for (int32 PointIndex = 0; PointIndex < DensePath.Num(); ++PointIndex)
			{
				if (PointIndex > 0)
				{
					DistanceAlongPath += FVector2D::Distance(DensePath[PointIndex - 1], DensePath[PointIndex]);
				}
				const double Alpha = TotalLength > UE_DOUBLE_SMALL_NUMBER ? DistanceAlongPath / TotalLength : 0.0;
				const FVector2D Previous = DensePath[FMath::Max(0, PointIndex - 1)];
				const FVector2D Next = DensePath[FMath::Min(DensePath.Num() - 1, PointIndex + 1)];
				const FVector2D Tangent = (Next - Previous).GetSafeNormal();
				const FVector2D Normal(-Tangent.Y, Tangent.X);
				const double EndpointEnvelope = FMath::Square(FMath::Sin(UE_PI * Alpha));
				const double Wave = FMath::Sin(2.0 * UE_PI * CurveCount * Alpha + Phase);
				CurvedPath.Add(DensePath[PointIndex] + Normal * Amplitude * EndpointEnvelope * Wave);
			}

			if (Exclusions.IsPathClear(CurvedPath, Clearance) && Terrain.IsPathTraversable(CurvedPath))
			{
				return CurvedPath;
			}
			Amplitude *= 0.5;
		}
		return DensePath;
	}

	bool TryBuildBasePath(
		const FVector2D& Start,
		const FVector2D& End,
		const FExclusionTester& Exclusions,
		const FTerrainRouteTester& Terrain,
		const double Clearance,
		TArray<FVector2D>& OutPath)
	{
		if (FVector2D::Distance(Start, End) < GridCellSize)
		{
			OutPath = {Start, End};
			return Exclusions.IsSegmentClear(Start, End, Clearance) && Terrain.IsPathTraversable(OutPath);
		}

		OutPath = SimplifyPath(
			FindGridPath(Start, End, Exclusions, Terrain, Clearance), Exclusions, Terrain, Clearance);
		if (OutPath.Num() >= 2)
		{
			return true;
		}

		// Grid discretization must not reject a straight corridor that passes the full terrain and exclusion checks.
		OutPath = {Start, End};
		if (Exclusions.IsSegmentClear(Start, End, Clearance) && Terrain.IsPathTraversable(OutPath))
		{
			return true;
		}
		OutPath.Reset();
		return false;
	}

	FVector2D EvaluateCubicBezier(
		const FVector2D& Start,
		const FVector2D& StartControl,
		const FVector2D& EndControl,
		const FVector2D& End,
		const double Alpha)
	{
		const double OneMinusAlpha = 1.0 - Alpha;
		return Start * (OneMinusAlpha * OneMinusAlpha * OneMinusAlpha)
			+ StartControl * (3.0 * FMath::Square(OneMinusAlpha) * Alpha)
			+ EndControl * (3.0 * OneMinusAlpha * FMath::Square(Alpha))
			+ End * (Alpha * Alpha * Alpha);
	}

	void AppendOrphanApproach(
		TArray<FVector2D>& InOutRoute,
		const FVector2D& EndPosition,
		const FVector2D& EndOutwardDirection,
		const double ApproachLength)
	{
		const FVector2D ApproachStart = InOutRoute.Last();
		const FVector2D IncomingDirection = InOutRoute.Num() >= 2
			? (ApproachStart - InOutRoute[InOutRoute.Num() - 2]).GetSafeNormal()
			: -EndOutwardDirection;
		const double HandleLength = ApproachLength * OrphanApproachHandleFactor;
		const FVector2D StartControl = ApproachStart + IncomingDirection * HandleLength;
		const FVector2D EndControl = EndPosition + EndOutwardDirection * HandleLength;
		const int32 SampleCount = FMath::Max(3, FMath::CeilToInt32(ApproachLength / OrphanApproachSampleSpacing));

		for (int32 SampleIndex = 1; SampleIndex <= SampleCount; ++SampleIndex)
		{
			const double Alpha = static_cast<double>(SampleIndex) / SampleCount;
			InOutRoute.Add(EvaluateCubicBezier(
				ApproachStart, StartControl, EndControl, EndPosition, Alpha));
		}
	}

	bool TryBuildOrphanRoute(
		const FEndpoint& Start,
		const FEndpoint& End,
		const UPCGCreateReinforcementRoadsSettings& Settings,
		const FExclusionTester& Exclusions,
		const FTerrainRouteTester& Terrain,
		const double Clearance,
		const int32 Seed,
		TArray<FVector2D>& OutRoute)
	{
		const FVector2D StartPosition(Start.Position);
		const FVector2D EndPosition(End.Position);
		const double DirectDistance = FVector2D::Distance(StartPosition, EndPosition);
		const double MaximumUsefulApproach = FMath::Max(MinimumMeshLength, DirectDistance * 0.6);
		const double PreferredApproachLength = FMath::Min(
			MaximumUsefulApproach,
			FMath::Clamp(DirectDistance * OrphanApproachDistanceFactor + Clearance,
				MinimumOrphanApproachLength, MaximumOrphanApproachLength));

		for (const double AttemptScale : OrphanApproachAttemptScales)
		{
			const double ApproachLength = PreferredApproachLength * AttemptScale;
			const FVector2D ApproachStart = EndPosition + End.Forward * ApproachLength;
			TArray<FVector2D> BasePath;
			if (not TryBuildBasePath(StartPosition, ApproachStart, Exclusions, Terrain, Clearance, BasePath)
				|| PathLength(BasePath) > FVector2D::Distance(StartPosition, ApproachStart) * MaximumDetourRatio)
			{
				continue;
			}

			OutRoute = AddDecorativeCurves(
				BasePath, Settings.Curvature, Settings.AmountCurvesPer1000Units,
				Seed, Exclusions, Terrain, Clearance);
			AppendOrphanApproach(OutRoute, EndPosition, End.Forward, ApproachLength);
			if (Exclusions.IsPathClear(OutRoute, Clearance) && Terrain.IsPathTraversable(OutRoute))
			{
				return true;
			}
		}
		OutRoute.Reset();
		return false;
	}

	bool TryBuildRoute2D(
		UWorld& World,
		const FEndpoint& Start,
		const FEndpoint& End,
		const EConnectionType ConnectionType,
		const UPCGCreateReinforcementRoadsSettings& Settings,
		const FExclusionTester& Exclusions,
		const double Clearance,
		const int32 Seed,
		TArray<FVector2D>& OutRoute)
	{
		const FVector2D StartPosition(Start.Position);
		const FVector2D EndPosition(End.Position);
		const double DirectDistance = FVector2D::Distance(StartPosition, EndPosition);
		if (DirectDistance < MinimumMeshLength)
		{
			return false;
		}
		const FTerrainRouteTester Terrain(World, Start.Position, End.Position, Settings);
		if (ConnectionType == EConnectionType::Orphan)
		{
			return TryBuildOrphanRoute(
				Start, End, Settings, Exclusions, Terrain, Clearance, Seed, OutRoute);
		}

		TArray<FVector2D> BasePath;
		if (not TryBuildBasePath(StartPosition, EndPosition, Exclusions, Terrain, Clearance, BasePath)
			|| PathLength(BasePath) > DirectDistance * MaximumDetourRatio)
		{
			return false;
		}

		OutRoute = AddDecorativeCurves(
			BasePath, Settings.Curvature, Settings.AmountCurvesPer1000Units,
			Seed, Exclusions, Terrain, Clearance);
		return OutRoute.Num() >= 2
			&& Exclusions.IsPathClear(OutRoute, Clearance)
			&& Terrain.IsPathTraversable(OutRoute);
	}

	FVector ProjectReinforcementRoadToGround(
		UWorld& World,
		const FVector& Position,
		const FCollisionQueryParams& TraceParams)
	{
		FHitResult Hit;
		const FVector TraceStart = Position + FVector::UpVector * ReinforcementGroundTraceUp;
		const FVector TraceEnd = Position - FVector::UpVector * ReinforcementGroundTraceDown;
		if (World.LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
		{
			return Hit.ImpactPoint + FVector::UpVector * ReinforcementRoadZOffset;
		}
		return Position + FVector::UpVector * ReinforcementRoadZOffset;
	}

	TArray<FVector> GroundAndGradeRoute(
		UWorld& World,
		const TArray<FVector2D>& Route2D,
		const FVector& Start,
		const FVector& End,
		const TArray<AActor*>& ActorsToIgnore)
	{
		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActors(ActorsToIgnore);
		TArray<FVector> Grounded;
		Grounded.Reserve(Route2D.Num());
		for (int32 PointIndex = 0; PointIndex < Route2D.Num(); ++PointIndex)
		{
			const double Alpha = Route2D.Num() > 1
				? static_cast<double>(PointIndex) / (Route2D.Num() - 1)
				: 0.0;
			Grounded.Add(ProjectReinforcementRoadToGround(
				World, FVector(Route2D[PointIndex], FMath::Lerp(Start.Z, End.Z, Alpha)), TraceParams));
		}

		for (int32 PointIndex = 1; PointIndex < Grounded.Num(); ++PointIndex)
		{
			const double HorizontalDistance = FVector2D::Distance(FVector2D(Grounded[PointIndex - 1]), FVector2D(Grounded[PointIndex]));
			Grounded[PointIndex].Z = FMath::Clamp(
				Grounded[PointIndex].Z,
				Grounded[PointIndex - 1].Z - HorizontalDistance * MaximumRoadGrade,
				Grounded[PointIndex - 1].Z + HorizontalDistance * MaximumRoadGrade);
		}
		for (int32 PointIndex = Grounded.Num() - 2; PointIndex >= 0; --PointIndex)
		{
			const double HorizontalDistance = FVector2D::Distance(FVector2D(Grounded[PointIndex + 1]), FVector2D(Grounded[PointIndex]));
			Grounded[PointIndex].Z = FMath::Clamp(
				Grounded[PointIndex].Z,
				Grounded[PointIndex + 1].Z - HorizontalDistance * MaximumRoadGrade,
				Grounded[PointIndex + 1].Z + HorizontalDistance * MaximumRoadGrade);
		}
		return Grounded;
	}

	double GetReinforcementRouteLength(const TArray<FVector>& Points)
	{
		double Length = 0.0;
		for (int32 PointIndex = 1; PointIndex < Points.Num(); ++PointIndex)
		{
			Length += FVector::Distance(Points[PointIndex - 1], Points[PointIndex]);
		}
		return Length;
	}

	bool TrySampleReinforcementRoute(
		const TArray<FVector>& Points,
		const double Distance,
		FVector& OutPosition,
		FVector& OutDirection)
	{
		if (Points.Num() < 2)
		{
			return false;
		}

		double TraversedDistance = 0.0;
		for (int32 PointIndex = 1; PointIndex < Points.Num(); ++PointIndex)
		{
			const FVector Segment = Points[PointIndex] - Points[PointIndex - 1];
			const double SegmentLength = Segment.Length();
			if (TraversedDistance + SegmentLength < Distance && PointIndex < Points.Num() - 1)
			{
				TraversedDistance += SegmentLength;
				continue;
			}

			const double SegmentAlpha = SegmentLength > UE_DOUBLE_SMALL_NUMBER
				? FMath::Clamp((Distance - TraversedDistance) / SegmentLength, 0.0, 1.0)
				: 0.0;
			OutPosition = FMath::Lerp(Points[PointIndex - 1], Points[PointIndex], SegmentAlpha);
			OutDirection = FVector(Segment.X, Segment.Y, 0.0).GetSafeNormal();
			return not OutDirection.IsNearlyZero();
		}
		return false;
	}

	TArray<int32> GetConfiguredReinforcementPoleIndices(const FReinforcementRoadPoleCategory& Category)
	{
		TArray<int32> ConfiguredIndices;
		for (int32 PoleIndex = 0; PoleIndex < Category.Poles.Num(); ++PoleIndex)
		{
			if (not Category.Poles[PoleIndex].PoleActor.IsNull())
			{
				ConfiguredIndices.Add(PoleIndex);
			}
		}
		return ConfiguredIndices;
	}

	const FReinforcementRoadPoleCategory* SelectReinforcementPoleCategory(
		const UPCGCreateReinforcementRoadsSettings& Settings,
		FRandomStream& Random)
	{
		TArray<const FReinforcementRoadPoleCategory*> EligibleCategories;
		if (not GetConfiguredReinforcementPoleIndices(Settings.LargeElectricPoles).IsEmpty())
		{
			EligibleCategories.Add(&Settings.LargeElectricPoles);
		}
		if (not GetConfiguredReinforcementPoleIndices(Settings.SmallElectricPoles).IsEmpty())
		{
			EligibleCategories.Add(&Settings.SmallElectricPoles);
		}
		return EligibleCategories.IsEmpty()
			? nullptr
			: EligibleCategories[Random.RandRange(0, EligibleCategories.Num() - 1)];
	}

	FVector ProjectReinforcementPoleToGround(
		UWorld& World,
		const FVector& Position,
		const TArray<AActor*>& ActorsToIgnore)
	{
		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActors(ActorsToIgnore);
		FHitResult Hit;
		const FVector TraceStart = Position + FVector::UpVector * ReinforcementGroundTraceUp;
		const FVector TraceEnd = Position - FVector::UpVector * ReinforcementGroundTraceDown;
		return World.LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams)
			? Hit.ImpactPoint
			: Position;
	}

	FRotator GetReinforcementPoleRotation(const FVector& RouteDirection, const FVector& PoleOrientation)
	{
		FVector2D LocalOrientation(PoleOrientation.X, PoleOrientation.Y);
		if (LocalOrientation.IsNearlyZero())
		{
			LocalOrientation = FVector2D::UnitX();
		}
		const double RouteYaw = FMath::Atan2(RouteDirection.Y, RouteDirection.X);
		const double LocalYaw = FMath::Atan2(LocalOrientation.Y, LocalOrientation.X);
		return FRotator(0.0, FMath::RadiansToDegrees(RouteYaw - LocalYaw), 0.0);
	}

	AActor* SpawnReinforcementPoleActor(
		UWorld& World,
		const FReinforcementRoadPoleEntry& PoleEntry,
		const FVector& RoutePosition,
		const FVector& RouteDirection,
		const double SideMultiplier,
		const TArray<AActor*>& ActorsToIgnore,
		const int32 RoadIndex,
		const int32 PoleIndex)
	{
		UClass* PoleClass = PoleEntry.PoleActor.LoadSynchronous();
		if (not IsValid(PoleClass) || PoleClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return nullptr;
		}

		const FVector RoadNormal(-RouteDirection.Y, RouteDirection.X, 0.0);
		const FVector OffsetPosition = RoutePosition
			+ RoadNormal * SideMultiplier * PoleEntry.OffsetFromRoad;
		const FVector GroundPosition = ProjectReinforcementPoleToGround(World, OffsetPosition, ActorsToIgnore);
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* PoleActor = World.SpawnActor<AActor>(
			PoleClass, GroundPosition, GetReinforcementPoleRotation(RouteDirection, PoleEntry.Orientation), SpawnParams);
		if (not IsValid(PoleActor))
		{
			return nullptr;
		}

		PoleActor->Tags.Add(PCGHelpers::DefaultPCGActorTag);
#if WITH_EDITOR
		PoleActor->SetActorLabel(FString::Printf(TEXT("PCG_ReinforcementPole_%d_%d"), RoadIndex, PoleIndex));
#endif
		return PoleActor;
	}

	void ConnectAdjacentReinforcementPoles(
		UDestructibleWire* ParentWireComponent,
		UDestructibleWire* ChildWireComponent,
		const int32 ParentPreferredWires,
		const int32 ChildPreferredWires,
		UStaticMesh* WireMesh,
		const FVector& WireMeshScale)
	{
		if (not IsValid(ParentWireComponent) || not IsValid(ChildWireComponent) || not IsValid(WireMesh))
		{
			return;
		}

		const int32 WireCount = FMath::Max(1, FMath::Min(ParentPreferredWires, ChildPreferredWires));
		ParentWireComponent->SetupWireConnection(
			ChildWireComponent, WireCount, WireMesh, WireMeshScale);
	}

	void SpawnReinforcementPoleChain(
		UWorld& World,
		const TArray<FVector>& RoutePoints,
		const UPCGCreateReinforcementRoadsSettings& Settings,
		UStaticMesh* WireMesh,
		const int32 Seed,
		const int32 RoadIndex,
		TArray<AActor*>& InOutSpawnedActors)
	{
		FRandomStream Random(Seed);
		const FReinforcementRoadPoleCategory* Category = SelectReinforcementPoleCategory(Settings, Random);
		if (Category == nullptr)
		{
			return;
		}

		const TArray<int32> ConfiguredPoleIndices = GetConfiguredReinforcementPoleIndices(*Category);
		const double MinimumSpacing = FMath::Max(
			MinimumPoleSpacing, FMath::Min(Category->MinimumDistanceBetweenPoles, Category->MaximumDistanceBetweenPoles));
		const double MaximumSpacing = FMath::Max(
			MinimumSpacing, FMath::Max(Category->MinimumDistanceBetweenPoles, Category->MaximumDistanceBetweenPoles));
		const double RouteLength = GetReinforcementRouteLength(RoutePoints);
		const double SideMultiplier = Random.RandRange(0, 1) == 0 ? -1.0 : 1.0;
		double PoleDistance = 0.0;
		UDestructibleWire* PreviousWireComponent = nullptr;
		int32 PreviousPreferredWires = 0;
		int32 PoleIndex = 0;

		while (PoleDistance <= RouteLength)
		{
			FVector RoutePosition;
			FVector RouteDirection;
			if (not TrySampleReinforcementRoute(RoutePoints, PoleDistance, RoutePosition, RouteDirection))
			{
				break;
			}

			const int32 EntryIndex = ConfiguredPoleIndices[Random.RandRange(0, ConfiguredPoleIndices.Num() - 1)];
			const FReinforcementRoadPoleEntry& PoleEntry = Category->Poles[EntryIndex];
			AActor* PoleActor = SpawnReinforcementPoleActor(
				World, PoleEntry, RoutePosition, RouteDirection, SideMultiplier,
				InOutSpawnedActors, RoadIndex, PoleIndex);
			UDestructibleWire* WireComponent = IsValid(PoleActor)
				? PoleActor->FindComponentByClass<UDestructibleWire>()
				: nullptr;
			if (IsValid(PoleActor))
			{
				InOutSpawnedActors.Add(PoleActor);
			}
			ConnectAdjacentReinforcementPoles(
				PreviousWireComponent, WireComponent, PreviousPreferredWires,
				PoleEntry.PreferredAmountWires, WireMesh, Settings.WireMeshScale);

			PreviousWireComponent = WireComponent;
			PreviousPreferredWires = PoleEntry.PreferredAmountWires;
			PoleDistance += Random.FRandRange(MinimumSpacing, MaximumSpacing);
			++PoleIndex;
		}
	}

	AActor* SpawnRoadActor(
		UWorld& World,
		const TArray<FVector>& Points,
		UStaticMesh* RoadMesh,
		UMaterialInterface* OverrideMaterial,
		const double MeshLength,
		const int32 RoadIndex,
		TArray<FBox>& OutRoadMeshBounds)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* RoadActor = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform(Points[0]), SpawnParams);
		if (not IsValid(RoadActor))
		{
			return nullptr;
		}
		RoadActor->Tags.Add(PCGHelpers::DefaultPCGActorTag);
#if WITH_EDITOR
		RoadActor->SetActorLabel(FString::Printf(TEXT("PCG_ReinforcementRoad_%d"), RoadIndex));
#endif

		USplineComponent* Spline = NewObject<USplineComponent>(RoadActor);
		RoadActor->SetRootComponent(Spline);
		RoadActor->AddInstanceComponent(Spline);
		Spline->RegisterComponent();
		Spline->ClearSplinePoints(false);
		for (const FVector& Point : Points)
		{
			Spline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
		}
		for (int32 PointIndex = 0; PointIndex < Spline->GetNumberOfSplinePoints(); ++PointIndex)
		{
			Spline->SetSplinePointType(PointIndex, ESplinePointType::CurveClamped, false);
		}
		Spline->UpdateSpline();

		if (RoadMesh == nullptr)
		{
			return RoadActor;
		}
		const double SplineLength = Spline->GetSplineLength();
		const int32 NumMeshes = FMath::Max(1, FMath::RoundToInt32(SplineLength / MeshLength));
		const double SegmentLength = SplineLength / NumMeshes;
		for (int32 MeshIndex = 0; MeshIndex < NumMeshes; ++MeshIndex)
		{
			const double StartDistance = MeshIndex * SegmentLength;
			const double EndDistance = (MeshIndex + 1) * SegmentLength;
			USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(RoadActor);
			SplineMesh->SetMobility(EComponentMobility::Static);
			SplineMesh->SetStaticMesh(RoadMesh);
			SplineMesh->SetForwardAxis(ESplineMeshAxis::X, false);
			if (OverrideMaterial != nullptr)
			{
				SplineMesh->SetMaterial(0, OverrideMaterial);
			}
			SplineMesh->AttachToComponent(Spline, FAttachmentTransformRules::KeepRelativeTransform);
			RoadActor->AddInstanceComponent(SplineMesh);

			const FVector StartPosition = Spline->GetLocationAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local);
			const FVector EndPosition = Spline->GetLocationAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local);
			const FVector StartTangent = Spline->GetTangentAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local)
				.GetSafeNormal() * SegmentLength;
			const FVector EndTangent = Spline->GetTangentAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local)
				.GetSafeNormal() * SegmentLength;
			SplineMesh->SetStartAndEnd(StartPosition, StartTangent, EndPosition, EndTangent, false);
			SplineMesh->RegisterComponent();
			SplineMesh->UpdateBounds();
			const FBox MeshBounds = SplineMesh->Bounds.GetBox();
			if (MeshBounds.IsValid)
			{
				OutRoadMeshBounds.Add(MeshBounds);
			}
		}
		return RoadActor;
	}

	void MarkEndpointUsed(
		const FRouteCandidate& Candidate,
		TArray<bool>& UsedStarts,
		TArray<bool>& UsedOrphans,
		TArray<bool>& UsedBackups)
	{
		switch (Candidate.Type)
		{
		case EConnectionType::Orphan:
			UsedOrphans[Candidate.Endpoint.SourceIndex] = true;
			break;
		case EConnectionType::ReinforcementStart:
			UsedStarts[Candidate.Endpoint.SourceIndex] = true;
			break;
		case EConnectionType::Backup:
			UsedBackups[Candidate.Endpoint.SourceIndex] = true;
			break;
		default:
			break;
		}
	}

	bool TryConnectCandidates(
		UWorld& World,
		const FEndpoint& Start,
		TArray<FRouteCandidate>& Candidates,
		const UPCGCreateReinforcementRoadsSettings& Settings,
		const FExclusionTester& Exclusions,
		const double Clearance,
		const int32 BaseSeed,
		const TArray<AActor*>& SpawnedActors,
		FGeneratedRoute& OutRoute,
		FRouteCandidate& OutCandidate)
	{
		Candidates.Sort([](const FRouteCandidate& First, const FRouteCandidate& Second)
		{
			return First.Score < Second.Score;
		});
		for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
		{
			TArray<FVector2D> Route2D;
			if (not TryBuildRoute2D(World, Start, Candidates[CandidateIndex].Endpoint, Candidates[CandidateIndex].Type,
				Settings, Exclusions,
				Clearance, PCGHelpers::ComputeSeed(BaseSeed, CandidateIndex), Route2D))
			{
				continue;
			}
			OutRoute.Points = GroundAndGradeRoute(
				World, Route2D, Start.Position, Candidates[CandidateIndex].Endpoint.Position, SpawnedActors);
			OutRoute.Type = Candidates[CandidateIndex].Type;
			OutCandidate = Candidates[CandidateIndex];
			return OutRoute.Points.Num() >= 2;
		}
		return false;
	}

	void EmitRoadPoints(FPCGContext& Context, const TArray<FGeneratedRoute>& Routes)
	{
		UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(&Context);
		PointData->InitializeFromData(nullptr);
		FPCGMetadataAttribute<int32>* RoadIndex = PointData->Metadata->CreateAttribute<int32>(RoadIndexAttribute, INDEX_NONE, false, false);
		FPCGMetadataAttribute<int32>* PointIndex = PointData->Metadata->CreateAttribute<int32>(PointIndexAttribute, INDEX_NONE, false, false);
		FPCGMetadataAttribute<int32>* ConnectionType = PointData->Metadata->CreateAttribute<int32>(ConnectionTypeAttribute, INDEX_NONE, false, false);

		TArray<FPCGPoint>& OutputPoints = PointData->GetMutablePoints();
		for (int32 RouteIndex = 0; RouteIndex < Routes.Num(); ++RouteIndex)
		{
			for (int32 RoutePointIndex = 0; RoutePointIndex < Routes[RouteIndex].Points.Num(); ++RoutePointIndex)
			{
				FPCGPoint& Point = OutputPoints.Emplace_GetRef();
				const TArray<FVector>& RoutePoints = Routes[RouteIndex].Points;
				const FVector Previous = RoutePoints[FMath::Max(0, RoutePointIndex - 1)];
				const FVector Next = RoutePoints[FMath::Min(RoutePoints.Num() - 1, RoutePointIndex + 1)];
				Point.Transform = FTransform((Next - Previous).Rotation(), RoutePoints[RoutePointIndex]);
				Point.Density = 1.0f;
				Point.Seed = PCGHelpers::ComputeSeed(RouteIndex, RoutePointIndex);
				PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
				RoadIndex->SetValue(Point.MetadataEntry, RouteIndex);
				PointIndex->SetValue(Point.MetadataEntry, RoutePointIndex);
				ConnectionType->SetValue(Point.MetadataEntry, static_cast<int32>(Routes[RouteIndex].Type));
			}
		}

		FPCGTaggedData& Output = Context.OutputData.TaggedData.Emplace_GetRef();
		Output.Pin = RoadPointsPin;
		Output.Data = PointData;
	}

	void EmitRoadBoundsVolumes(FPCGContext& Context, const TArray<FBox>& RoadMeshBounds)
	{
		for (const FBox& MeshBounds : RoadMeshBounds)
		{
			if (not MeshBounds.IsValid)
			{
				continue;
			}

			UPCGVolumeData* VolumeData = FPCGContext::NewObject_AnyThread<UPCGVolumeData>(&Context);
			VolumeData->Initialize(MeshBounds);
			FPCGTaggedData& Output = Context.OutputData.TaggedData.Emplace_GetRef();
			Output.Pin = RoadBoundsVolumePin;
			Output.Data = VolumeData;
		}
	}
}

#if WITH_EDITOR
FName UPCGCreateReinforcementRoadsSettings::GetDefaultNodeName() const
{
	return TEXT("CreateReinforcementRoads");
}

FText UPCGCreateReinforcementRoadsSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Create Reinforcement Roads");
}

FText UPCGCreateReinforcementRoadsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Creates one-use, exclusion-safe reinforcement road connections. Primary orphan/start endpoints are preferred; "
		"backup ends are used only when no natural primary route can be planned. Roads avoid hills and steep terrain, "
		"can spawn connected electric-pole chains, and output their spline-mesh bounds as exclusion volumes.");
}
#endif

TArray<FPCGPinProperties> UPCGCreateReinforcementRoadsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(ReinforcementRoadConstants::ReinforcementStartsPin, EPCGDataType::Point).SetRequiredPin();
	Pins.Emplace(ReinforcementRoadConstants::OrpahnedRoadPointsPin, EPCGDataType::Point);
	Pins.Emplace(ReinforcementRoadConstants::BackupRoadEndsPin, EPCGDataType::Point);
	Pins.Emplace(ReinforcementRoadConstants::ExcludedVolumesPin, EPCGDataType::Spatial);
	return Pins;
}

TArray<FPCGPinProperties> UPCGCreateReinforcementRoadsSettings::OutputPinProperties() const
{
	return {
		FPCGPinProperties(ReinforcementRoadConstants::RoadPointsPin, EPCGDataType::Point),
		FPCGPinProperties(ReinforcementRoadConstants::RoadBoundsVolumePin, EPCGDataType::Volume)
	};
}

FPCGElementPtr UPCGCreateReinforcementRoadsSettings::CreateElement() const
{
	return MakeShared<FPCGCreateReinforcementRoadsElement>();
}

bool FPCGCreateReinforcementRoadsElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	const UPCGCreateReinforcementRoadsSettings* Settings = Context->GetInputSettings<UPCGCreateReinforcementRoadsSettings>();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (Settings == nullptr || not IsValid(SourceComponent) || not IsValid(SourceComponent->GetWorld()))
	{
		return true;
	}

	TArray<FEndpoint> Starts = CollectEndpoints(*Context, ReinforcementRoadConstants::ReinforcementStartsPin);
	const TArray<FEndpoint> Orphans = CollectEndpoints(*Context, ReinforcementRoadConstants::OrpahnedRoadPointsPin);
	const TArray<FEndpoint> Backups = CollectEndpoints(*Context, ReinforcementRoadConstants::BackupRoadEndsPin);
	if (Starts.IsEmpty())
	{
		EmitRoadPoints(*Context, {});
		EmitRoadBoundsVolumes(*Context, {});
		return true;
	}

	UStaticMesh* RoadMesh = Settings->RoadMesh.LoadSynchronous();
	UStaticMesh* WireMesh = Settings->WireMesh.LoadSynchronous();
	UMaterialInterface* OverrideMaterial = Settings->OverrideMaterial.LoadSynchronous();
	const double Clearance = ComputeRoadClearance(RoadMesh) + SplineInterpolationSafetyMargin;
	const double MeshLength = ComputeMeshLength(RoadMesh);
	const FExclusionTester Exclusions = BuildExclusionTester(*Context);

	TArray<bool> UsedStarts;
	UsedStarts.Init(false, Starts.Num());
	TArray<bool> UsedOrphans;
	UsedOrphans.Init(false, Orphans.Num());
	TArray<bool> UsedBackups;
	UsedBackups.Init(false, Backups.Num());
	TArray<FGeneratedRoute> Routes;
	TArray<AActor*> SpawnedActors;
	TArray<FBox> RoadMeshBounds;
	UWorld& World = *SourceComponent->GetWorld();

	for (int32 StartIndex = 0; StartIndex < Starts.Num(); ++StartIndex)
	{
		if (UsedStarts[StartIndex])
		{
			continue;
		}
		UsedStarts[StartIndex] = true;

		TArray<FRouteCandidate> Candidates;
		AddPrimaryCandidates(Starts[StartIndex], Orphans, UsedOrphans, Starts, UsedStarts, Candidates);
		FGeneratedRoute GeneratedRoute;
		FRouteCandidate SelectedCandidate;
		const int32 RouteSeed = PCGHelpers::ComputeSeed(Context->GetSeed(), StartIndex);
		bool bConnected = TryConnectCandidates(World, Starts[StartIndex], Candidates, *Settings, Exclusions,
			Clearance, RouteSeed, SpawnedActors, GeneratedRoute, SelectedCandidate);

		if (not bConnected)
		{
			Candidates.Reset();
			AddBackupCandidates(Starts[StartIndex], Backups, UsedBackups, Candidates);
			bConnected = TryConnectCandidates(World, Starts[StartIndex], Candidates, *Settings, Exclusions,
				Clearance, RouteSeed, SpawnedActors, GeneratedRoute, SelectedCandidate);
		}
		if (not bConnected)
		{
			continue;
		}

		MarkEndpointUsed(SelectedCandidate, UsedStarts, UsedOrphans, UsedBackups);
		AActor* RoadActor = SpawnRoadActor(
			World, GeneratedRoute.Points, RoadMesh, OverrideMaterial,
			MeshLength, Routes.Num(), RoadMeshBounds);
		if (IsValid(RoadActor))
		{
			SpawnedActors.Add(RoadActor);
		}
		SpawnReinforcementPoleChain(
			World, GeneratedRoute.Points, *Settings, WireMesh,
			PCGHelpers::ComputeSeed(RouteSeed, Routes.Num()), Routes.Num(), SpawnedActors);
		Routes.Add(MoveTemp(GeneratedRoute));
	}

	if (not SpawnedActors.IsEmpty())
	{
		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(SourceComponent);
		for (AActor* SpawnedActor : SpawnedActors)
		{
			ManagedActors->GeneratedActors.Add(SpawnedActor);
		}
		SourceComponent->AddToManagedResources(ManagedActors);
	}
	EmitRoadPoints(*Context, Routes);
	EmitRoadBoundsVolumes(*Context, RoadMeshBounds);
	return true;
}

#undef LOCTEXT_NAMESPACE
