// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#include "PCGFindEnemyDefensePoints.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"

#define LOCTEXT_NAMESPACE "PCGFindEnemyDefensePoints"

namespace FindEnemyDefensePointsConstants
{
	const FName CandidatePointsPin = TEXT("CandidatePoints");
	const FName ExclusionBoundsPin = TEXT("ExclusionBounds");
	const FName EnemyBasePin = TEXT("EnemyBase");
	const FName DefensePointsPin = TEXT("DefensePoints");
	const FName NotDefenseNotExcludedPin = TEXT("NotDefenseNotExcluded");

	constexpr double SymmetryLocationTolerance = 1.0;
	constexpr double SymmetryLocationToleranceSquared =
		SymmetryLocationTolerance * SymmetryLocationTolerance;
}

namespace
{
	using namespace FindEnemyDefensePointsConstants;

	struct FDefenseCandidate
	{
		FPCGPoint Point;
		FVector2D Position = FVector2D::ZeroVector;
		int32 SourceIndex = INDEX_NONE;
		double BaseDistanceSquared = 0.0;
	};

	struct FDefensePointOutputs
	{
		TArray<FPCGPoint> DefensePoints;
		TArray<FPCGPoint> RemainingPoints;
	};

	FBox GetPointWorldBounds(const FPCGPoint& Point)
	{
		return FBox(Point.BoundsMin, Point.BoundsMax).TransformBy(Point.Transform);
	}

	void AddSpatialBounds(const UPCGSpatialData& SpatialData, TArray<FBox>& OutBounds)
	{
		if (const UPCGPointData* PointData = Cast<UPCGPointData>(&SpatialData))
		{
			for (const FPCGPoint& Point : PointData->GetPoints())
			{
				const FBox PointBounds = GetPointWorldBounds(Point);
				if (PointBounds.IsValid)
				{
					OutBounds.Add(PointBounds);
				}
			}
			return;
		}

		const FBox SpatialBounds = SpatialData.GetBounds();
		if (SpatialBounds.IsValid)
		{
			OutBounds.Add(SpatialBounds);
		}
	}

	TArray<FBox> CollectBounds(const FPCGContext& Context, const FName Pin)
	{
		TArray<FBox> Bounds;
		for (const FPCGTaggedData& Input : Context.InputData.GetInputsByPin(Pin))
		{
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
			if (SpatialData == nullptr)
			{
				continue;
			}
			AddSpatialBounds(*SpatialData, Bounds);
		}
		return Bounds;
	}

	TArray<FPCGPoint> CollectCandidatePoints(const FPCGContext& Context)
	{
		TArray<FPCGPoint> Points;
		for (const FPCGTaggedData& Input : Context.InputData.GetInputsByPin(CandidatePointsPin))
		{
			const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data);
			if (PointData == nullptr)
			{
				continue;
			}
			Points.Append(PointData->GetPoints());
		}
		return Points;
	}

	bool DoesCircleOverlapBox(const FVector2D& Center, const double Radius, const FBox& Box)
	{
		if (not Box.IsValid)
		{
			return false;
		}

		const double ClosestX = FMath::Clamp(Center.X, Box.Min.X, Box.Max.X);
		const double ClosestY = FMath::Clamp(Center.Y, Box.Min.Y, Box.Max.Y);
		return FVector2D::DistSquared(Center, FVector2D(ClosestX, ClosestY))
			<= FMath::Square(Radius);
	}

	bool DoesCircleOverlapAnyBounds(
		const FVector2D& Center,
		const double Radius,
		const TArray<FBox>& Bounds)
	{
		for (const FBox& Box : Bounds)
		{
			if (DoesCircleOverlapBox(Center, Radius, Box))
			{
				return true;
			}
		}
		return false;
	}

	double DistanceSquaredToBoxXY(const FVector2D& Position, const FBox& Box)
	{
		const double ClosestX = FMath::Clamp(Position.X, Box.Min.X, Box.Max.X);
		const double ClosestY = FMath::Clamp(Position.Y, Box.Min.Y, Box.Max.Y);
		return FVector2D::DistSquared(Position, FVector2D(ClosestX, ClosestY));
	}

	TArray<FDefenseCandidate> BuildEligibleDefenseCandidates(
		const TArray<FPCGPoint>& CandidatePoints,
		const TArray<FBox>& ExclusionBounds,
		const FBox& EnemyBaseBounds,
		const double Radius)
	{
		TArray<FDefenseCandidate> Candidates;
		for (int32 CandidateIndex = 0; CandidateIndex < CandidatePoints.Num(); ++CandidateIndex)
		{
			const FVector Location = CandidatePoints[CandidateIndex].Transform.GetLocation();
			const FVector2D Position(Location);
			if (DoesCircleOverlapAnyBounds(Position, Radius, ExclusionBounds)
				|| DoesCircleOverlapBox(Position, Radius, EnemyBaseBounds))
			{
				continue;
			}

			FDefenseCandidate& Candidate = Candidates.Emplace_GetRef();
			Candidate.Point = CandidatePoints[CandidateIndex];
			Candidate.Position = Position;
			Candidate.SourceIndex = CandidateIndex;
			Candidate.BaseDistanceSquared = DistanceSquaredToBoxXY(Position, EnemyBaseBounds);
		}

		Candidates.Sort([](const FDefenseCandidate& Left, const FDefenseCandidate& Right)
		{
			if (not FMath::IsNearlyEqual(Left.BaseDistanceSquared, Right.BaseDistanceSquared))
			{
				return Left.BaseDistanceSquared < Right.BaseDistanceSquared;
			}
			return Left.SourceIndex < Right.SourceIndex;
		});
		return Candidates;
	}

	int32 FindCandidateAtPosition(
		const TArray<FDefenseCandidate>& Candidates,
		const FVector2D& TargetPosition,
		const TArray<bool>& UnavailableCandidates)
	{
		int32 BestIndex = INDEX_NONE;
		double BestDistanceSquared = SymmetryLocationToleranceSquared;
		for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
		{
			if (UnavailableCandidates[CandidateIndex])
			{
				continue;
			}

			const double DistanceSquared = FVector2D::DistSquared(
				Candidates[CandidateIndex].Position, TargetPosition);
			if (DistanceSquared <= BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				BestIndex = CandidateIndex;
			}
		}
		return BestIndex;
	}

	bool AddUniqueGroupCandidate(const int32 CandidateIndex, TArray<int32>& OutGroup)
	{
		if (CandidateIndex == INDEX_NONE)
		{
			return false;
		}
		OutGroup.AddUnique(CandidateIndex);
		return true;
	}

	TArray<int32> BuildFourWaySymmetryGroup(
		const int32 SeedIndex,
		const TArray<FDefenseCandidate>& Candidates,
		const FVector2D& BaseCenter,
		const TArray<bool>& UnavailableCandidates)
	{
		const FVector2D Offset = Candidates[SeedIndex].Position - BaseCenter;
		const double AbsoluteX = FMath::Abs(Offset.X);
		const double AbsoluteY = FMath::Abs(Offset.Y);
		const FVector2D Targets[] =
		{
			BaseCenter + FVector2D(AbsoluteX, AbsoluteY),
			BaseCenter + FVector2D(-AbsoluteX, AbsoluteY),
			BaseCenter + FVector2D(AbsoluteX, -AbsoluteY),
			BaseCenter + FVector2D(-AbsoluteX, -AbsoluteY)
		};

		TArray<int32> Group;
		for (const FVector2D& Target : Targets)
		{
			const int32 CandidateIndex = FindCandidateAtPosition(Candidates, Target, UnavailableCandidates);
			if (not AddUniqueGroupCandidate(CandidateIndex, Group))
			{
				return {};
			}
		}
		return Group;
	}

	TArray<int32> BuildOppositeSymmetryGroup(
		const int32 SeedIndex,
		const TArray<FDefenseCandidate>& Candidates,
		const FVector2D& BaseCenter,
		const TArray<bool>& UnavailableCandidates)
	{
		TArray<int32> Group;
		Group.Add(SeedIndex);
		const FVector2D OppositePosition = BaseCenter * 2.0 - Candidates[SeedIndex].Position;
		const int32 OppositeIndex = FindCandidateAtPosition(Candidates, OppositePosition, UnavailableCandidates);
		if (not AddUniqueGroupCandidate(OppositeIndex, Group) || Group.Num() != 2)
		{
			return {};
		}
		return Group;
	}

	bool DoesGroupOverlapSelection(
		const TArray<int32>& Group,
		const TArray<FDefenseCandidate>& Candidates,
		const TArray<int32>& SelectedCandidateIndices,
		const double Radius)
	{
		const double MinimumDistanceSquared = FMath::Square(Radius * 2.0);
		for (int32 GroupPosition = 0; GroupPosition < Group.Num(); ++GroupPosition)
		{
			const FVector2D& Position = Candidates[Group[GroupPosition]].Position;
			for (int32 OtherGroupPosition = GroupPosition + 1; OtherGroupPosition < Group.Num(); ++OtherGroupPosition)
			{
				if (FVector2D::DistSquared(Position, Candidates[Group[OtherGroupPosition]].Position)
					<= MinimumDistanceSquared)
				{
					return true;
				}
			}

			for (const int32 SelectedIndex : SelectedCandidateIndices)
			{
				if (FVector2D::DistSquared(Position, Candidates[SelectedIndex].Position)
					<= MinimumDistanceSquared)
				{
					return true;
				}
			}
		}
		return false;
	}

	template <typename GroupBuilderType>
	void SelectSymmetryGroups(
		const TArray<FDefenseCandidate>& Candidates,
		const FVector2D& BaseCenter,
		const int32 TargetAmount,
		const double Radius,
		GroupBuilderType BuildGroup,
		TArray<bool>& InOutUnavailableCandidates,
		TArray<int32>& OutSelectedCandidateIndices)
	{
		for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
		{
			if (OutSelectedCandidateIndices.Num() >= TargetAmount
				|| InOutUnavailableCandidates[CandidateIndex])
			{
				continue;
			}

			const TArray<int32> Group = BuildGroup(
				CandidateIndex, Candidates, BaseCenter, InOutUnavailableCandidates);
			if (Group.IsEmpty() || OutSelectedCandidateIndices.Num() + Group.Num() > TargetAmount
				|| DoesGroupOverlapSelection(Group, Candidates, OutSelectedCandidateIndices, Radius))
			{
				continue;
			}

			for (const int32 GroupCandidateIndex : Group)
			{
				InOutUnavailableCandidates[GroupCandidateIndex] = true;
				OutSelectedCandidateIndices.Add(GroupCandidateIndex);
			}
		}
	}

	TArray<int32> SelectRectangularSymmetricalCandidates(
		const TArray<FDefenseCandidate>& Candidates,
		const FBox& EnemyBaseBounds,
		const int32 TargetAmount,
		const double Radius)
	{
		TArray<bool> UnavailableCandidates;
		UnavailableCandidates.Init(false, Candidates.Num());
		TArray<int32> SelectedCandidateIndices;
		const FVector BaseCenter3D = EnemyBaseBounds.GetCenter();
		const FVector2D BaseCenter(BaseCenter3D);

		SelectSymmetryGroups(Candidates, BaseCenter, TargetAmount, Radius,
			BuildFourWaySymmetryGroup, UnavailableCandidates, SelectedCandidateIndices);
		SelectSymmetryGroups(Candidates, BaseCenter, TargetAmount, Radius,
			BuildOppositeSymmetryGroup, UnavailableCandidates, SelectedCandidateIndices);
		return SelectedCandidateIndices;
	}

	TArray<int32> SelectDefenseCandidates(
		const EDefensePointStrategy Strategy,
		const TArray<FDefenseCandidate>& Candidates,
		const FBox& EnemyBaseBounds,
		const int32 TargetAmount,
		const double Radius)
	{
		switch (Strategy)
		{
		case EDefensePointStrategy::RectangularSymmetrical:
			return SelectRectangularSymmetricalCandidates(Candidates, EnemyBaseBounds, TargetAmount, Radius);
		default:
			return {};
		}
	}

	void SetDefensePointBounds(FPCGPoint& Point, const float Radius)
	{
		Point.BoundsMin.X = -Radius;
		Point.BoundsMin.Y = -Radius;
		Point.BoundsMax.X = Radius;
		Point.BoundsMax.Y = Radius;
	}

	void BuildDefensePointOutput(
		const TArray<FDefenseCandidate>& EligibleCandidates,
		const TArray<int32>& SelectedCandidateIndices,
		const float Radius,
		TArray<bool>& OutIsDefenseSource,
		TArray<FVector2D>& OutDefensePositions,
		TArray<FPCGPoint>& OutDefensePoints)
	{
		for (const int32 SelectedIndex : SelectedCandidateIndices)
		{
			FPCGPoint DefensePoint = EligibleCandidates[SelectedIndex].Point;
			SetDefensePointBounds(DefensePoint, Radius);
			OutDefensePoints.Add(MoveTemp(DefensePoint));
			OutDefensePositions.Add(EligibleCandidates[SelectedIndex].Position);
			OutIsDefenseSource[EligibleCandidates[SelectedIndex].SourceIndex] = true;
		}
	}

	TArray<FPCGPoint> BuildRemainingPointOutput(
		const TArray<FPCGPoint>& CandidatePoints,
		const TArray<FBox>& ExclusionBounds,
		const TArray<bool>& IsDefenseSource,
		const TArray<FVector2D>& DefensePositions,
		const double Radius)
	{
		TArray<FPCGPoint> RemainingPoints;
		const double MinimumDefenseDistanceSquared = FMath::Square(Radius * 2.0);
		for (int32 CandidateIndex = 0; CandidateIndex < CandidatePoints.Num(); ++CandidateIndex)
		{
			const FVector Location = CandidatePoints[CandidateIndex].Transform.GetLocation();
			const FVector2D Position(Location);
			if (IsDefenseSource[CandidateIndex]
				|| DoesCircleOverlapAnyBounds(Position, Radius, ExclusionBounds))
			{
				continue;
			}

			bool bOverlapsDefensePoint = false;
			for (const FVector2D& DefensePosition : DefensePositions)
			{
				if (FVector2D::DistSquared(Position, DefensePosition) <= MinimumDefenseDistanceSquared)
				{
					bOverlapsDefensePoint = true;
					break;
				}
			}
			if (not bOverlapsDefensePoint)
			{
				RemainingPoints.Add(CandidatePoints[CandidateIndex]);
			}
		}
		return RemainingPoints;
	}

	FDefensePointOutputs BuildOutputs(
		const TArray<FPCGPoint>& CandidatePoints,
		const TArray<FDefenseCandidate>& EligibleCandidates,
		const TArray<int32>& SelectedCandidateIndices,
		const TArray<FBox>& ExclusionBounds,
		const double Radius)
	{
		FDefensePointOutputs Outputs;
		TArray<bool> IsDefenseSource;
		IsDefenseSource.Init(false, CandidatePoints.Num());
		TArray<FVector2D> DefensePositions;
		BuildDefensePointOutput(EligibleCandidates, SelectedCandidateIndices, static_cast<float>(Radius),
			IsDefenseSource, DefensePositions, Outputs.DefensePoints);
		Outputs.RemainingPoints = BuildRemainingPointOutput(
			CandidatePoints, ExclusionBounds, IsDefenseSource, DefensePositions, Radius);
		return Outputs;
	}

	void AddOutput(FPCGContext& Context, const FName Pin, const TArray<FPCGPoint>& Points)
	{
		UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(&Context);
		PointData->InitializeFromData(nullptr);
		PointData->SetPoints(Points);

		FPCGTaggedData& Output = Context.OutputData.TaggedData.Emplace_GetRef();
		Output.Pin = Pin;
		Output.Data = PointData;
		Context.OutputData.DataCrcs.Add(PointData->GetOrComputeCrc(true));
	}
}

#if WITH_EDITOR
FName UPCGFindEnemyDefensePointsSettings::GetDefaultNodeName() const
{
	return FName(TEXT("FindEnemyDefensePoints"));
}

FText UPCGFindEnemyDefensePointsSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Find Enemy Defense Points");
}

FText UPCGFindEnemyDefensePointsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Selects non-overlapping candidate points in a symmetric formation around an enemy base. "
		"Candidates blocked by exclusions are removed; other points clear of the selected formation "
		"are emitted separately.");
}
#endif

FPCGElementPtr UPCGFindEnemyDefensePointsSettings::CreateElement() const
{
	return MakeShared<FPCGFindEnemyDefensePointsElement>();
}

TArray<FPCGPinProperties> UPCGFindEnemyDefensePointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(FindEnemyDefensePointsConstants::CandidatePointsPin, EPCGDataType::Point).SetRequiredPin();
	Pins.Emplace(FindEnemyDefensePointsConstants::ExclusionBoundsPin, EPCGDataType::Spatial);
	Pins.Emplace_GetRef(FindEnemyDefensePointsConstants::EnemyBasePin, EPCGDataType::Spatial).SetRequiredPin();
	return Pins;
}

TArray<FPCGPinProperties> UPCGFindEnemyDefensePointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(FindEnemyDefensePointsConstants::DefensePointsPin, EPCGDataType::Point);
	Pins.Emplace(FindEnemyDefensePointsConstants::NotDefenseNotExcludedPin, EPCGDataType::Point);
	return Pins;
}

bool FPCGFindEnemyDefensePointsElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	const UPCGFindEnemyDefensePointsSettings* Settings =
		Context->GetInputSettings<UPCGFindEnemyDefensePointsSettings>();
	if (Settings == nullptr)
	{
		return false;
	}

	const TArray<FPCGPoint> CandidatePoints = CollectCandidatePoints(*Context);
	const TArray<FBox> EnemyBaseBoundsInputs = CollectBounds(*Context, FindEnemyDefensePointsConstants::EnemyBasePin);
	if (EnemyBaseBoundsInputs.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingEnemyBase", "EnemyBase requires valid spatial data."));
		return false;
	}

	FBox EnemyBaseBounds(EForceInit::ForceInit);
	for (const FBox& Bounds : EnemyBaseBoundsInputs)
	{
		EnemyBaseBounds += Bounds;
	}
	const TArray<FBox> ExclusionBounds =
		CollectBounds(*Context, FindEnemyDefensePointsConstants::ExclusionBoundsPin);
	const double Radius = FMath::Max(0.0, static_cast<double>(Settings->DefensePointRadius));
	const int32 TargetAmount = FMath::Max(0, Settings->AmountOfDefensePointsToAimFor);

	const TArray<FDefenseCandidate> EligibleCandidates = BuildEligibleDefenseCandidates(
		CandidatePoints, ExclusionBounds, EnemyBaseBounds, Radius);
	const TArray<int32> SelectedCandidateIndices = SelectDefenseCandidates(
		Settings->DefenseStrategy, EligibleCandidates, EnemyBaseBounds, TargetAmount, Radius);
	const FDefensePointOutputs Outputs = BuildOutputs(
		CandidatePoints, EligibleCandidates, SelectedCandidateIndices, ExclusionBounds, Radius);

	Context->OutputData.TaggedData.Reset();
	Context->OutputData.DataCrcs.Reset();
	AddOutput(*Context, FindEnemyDefensePointsConstants::DefensePointsPin, Outputs.DefensePoints);
	AddOutput(*Context, FindEnemyDefensePointsConstants::NotDefenseNotExcludedPin, Outputs.RemainingPoints);
	return true;
}

#undef LOCTEXT_NAMESPACE
