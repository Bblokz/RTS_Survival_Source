// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/TechWheel/TechWheelLabelPlacement.h"

namespace TechWheelLabelPlacementPrivate
{
	// A hexagon with a corner pointing right: its flat edges are at radius * sin 60, and a slanted edge moves
	// 1 / tan 60 sideways per unit of height.
	constexpr float HexagonFlatEdgeScale = 0.8660254f;
	constexpr float HexagonSlantRunPerRise = 0.5773503f;
	constexpr float HexagonFlatEdgeHalfWidthScale = 0.5f;
	constexpr int32 HexagonSlices = 6;

	// Space between a node's outline and its label.
	constexpr float LabelGap = 3.f;
	// The slanted sides put the label's centre at this fraction of the hexagon's half height.
	constexpr float SlantedSideHeightScale = 0.62f;

	// Obstacles and other labels are grown by this much, so a label does not touch them either.
	constexpr float ObstacleMargin = 2.f;
	// Labels stay this far inside the ring's line.
	constexpr float RingInset = 4.f;
	constexpr int32 RingSampleColumns = 8;
	constexpr int32 RingSampleRows = 3;

	// Each step down the side order has to save this much covered area, so a label stays close to its node.
	constexpr float SidePreferenceCost = 6.f;
	// Covered area below this counts as free: the tip of a pip is no reason to move a label.
	constexpr float FreeAreaTolerance = 20.f;

	// The strip right under a node is where its own name is expected; another node's label there reads as this
	// node's name, so it costs as much as covering half of it.
	constexpr float NameSlotWeight = 0.5f;
	constexpr float NameSlotHalfWidthScale = 0.75f;
	constexpr float NameSlotExtraHeight = 2.f;

	// The labels are re-chosen against each other until nothing moves; this stops two labels swapping for ever.
	constexpr int32 MaxRefinePasses = 8;

	FTechWheelLabelRect InflateRect(const FTechWheelLabelRect& Rect, const float Margin)
	{
		FTechWheelLabelRect Inflated;
		Inflated.Min = Rect.Min - FVector2D(Margin, Margin);
		Inflated.Max = Rect.Max + FVector2D(Margin, Margin);
		return Inflated;
	}

	float GetRectArea(const FTechWheelLabelRect& Rect)
	{
		const FVector2D Size = Rect.GetSize();
		return static_cast<float>(FMath::Max(Size.X, 0.0) * FMath::Max(Size.Y, 0.0));
	}

	FTechWheelLabelRect GetNameSlot(const FTechWheelLabelRequest& Request)
	{
		const float HalfHeight = Request.OutlineRadius * HexagonFlatEdgeScale;
		const float HalfWidth = Request.OutlineRadius * NameSlotHalfWidthScale;
		const float SlotHeight = LabelGap + static_cast<float>(Request.LabelSize.Y) + NameSlotExtraHeight;
		FTechWheelLabelRect Slot;
		Slot.Min = Request.NodeCentre + FVector2D(-HalfWidth, HalfHeight);
		Slot.Max = Request.NodeCentre + FVector2D(HalfWidth, HalfHeight + SlotHeight);
		return Slot;
	}

	/** The obstacles as the cost function uses them: grown by the margin, plus every node's name slot. */
	TArray<FTechWheelLabelObstacle> PrepareObstacles(const TArray<FTechWheelLabelRequest>& Requests,
	                                                 const TArray<FTechWheelLabelObstacle>& Obstacles)
	{
		TArray<FTechWheelLabelObstacle> Prepared;
		Prepared.Reserve(Obstacles.Num() + Requests.Num());
		for (const FTechWheelLabelObstacle& Obstacle : Obstacles)
		{
			FTechWheelLabelObstacle& Grown = Prepared.Add_GetRef(Obstacle);
			Grown.Rect = InflateRect(Obstacle.Rect, ObstacleMargin);
		}
		for (int32 LabelIndex = 0; LabelIndex < Requests.Num(); ++LabelIndex)
		{
			FTechWheelLabelObstacle& NameSlot = Prepared.AddDefaulted_GetRef();
			NameSlot.Rect = GetNameSlot(Requests[LabelIndex]);
			NameSlot.Weight = NameSlotWeight;
			NameSlot.IgnoredByLabelIndex = LabelIndex;
		}
		return Prepared;
	}

	float GetObstacleCost(const FTechWheelLabelRect& Rect, const int32 LabelIndex,
	                      const TArray<FTechWheelLabelObstacle>& PreparedObstacles)
	{
		float Cost = 0.f;
		for (const FTechWheelLabelObstacle& Obstacle : PreparedObstacles)
		{
			if (Obstacle.IgnoredByLabelIndex == LabelIndex)
			{
				continue;
			}
			Cost += TechWheelLabelPlacement::GetOverlapArea(Rect, Obstacle.Rect) * Obstacle.Weight;
		}
		return Cost;
	}

	float GetOtherLabelsCost(const FTechWheelLabelRect& Rect, const int32 LabelIndex,
	                         const TArray<FTechWheelLabelPlacementResult>& Placements)
	{
		float Cost = 0.f;
		for (int32 OtherIndex = 0; OtherIndex < Placements.Num(); ++OtherIndex)
		{
			if (OtherIndex == LabelIndex)
			{
				continue;
			}
			Cost += TechWheelLabelPlacement::GetOverlapArea(
				Rect, InflateRect(Placements[OtherIndex].Rect, ObstacleMargin));
		}
		return Cost;
	}

	/** Everything one choice of side is weighed against. */
	struct FLabelCostInput
	{
		const TArray<FTechWheelLabelObstacle>& PreparedObstacles;
		const FTechWheelLabelRing& Ring;
		// Null while the labels are first chosen against the obstacles only.
		const TArray<FTechWheelLabelPlacementResult>* OtherLabels;
	};

	float GetSideCost(const FTechWheelLabelRequest& Request, const int32 LabelIndex, const ETechWheelLabelSide Side,
	                  const FLabelCostInput& Input)
	{
		const FTechWheelLabelRect Rect = TechWheelLabelPlacement::GetCandidateRect(Request, Side);
		float Cost = GetObstacleCost(Rect, LabelIndex, Input.PreparedObstacles);
		Cost += TechWheelLabelPlacement::GetAreaOutsideRing(Rect, Input.Ring);
		if (Input.OtherLabels != nullptr)
		{
			Cost += GetOtherLabelsCost(Rect, LabelIndex, *Input.OtherLabels);
		}
		return Cost;
	}

	/** The first side in the order of preference that no later side beats by more than the tolerance. */
	ETechWheelLabelSide PickSide(const FTechWheelLabelRequest& Request, const int32 LabelIndex,
	                             const FLabelCostInput& Input)
	{
		ETechWheelLabelSide BestSide = ETechWheelLabelSide::Below;
		float BestCost = GetSideCost(Request, LabelIndex, BestSide, Input);
		const int32 SideCount = static_cast<int32>(ETechWheelLabelSide::Count);
		for (int32 SideIndex = 1; SideIndex < SideCount; ++SideIndex)
		{
			const ETechWheelLabelSide Side = static_cast<ETechWheelLabelSide>(SideIndex);
			const float Cost = GetSideCost(Request, LabelIndex, Side, Input) + SidePreferenceCost * SideIndex;
			if (Cost < BestCost - FreeAreaTolerance)
			{
				BestSide = Side;
				BestCost = Cost;
			}
		}
		return BestSide;
	}

	/** The labels while they are being moved, with what they are weighed against. */
	struct FLabelPlacementState
	{
		const TArray<FTechWheelLabelRequest>& Requests;
		const TArray<FTechWheelLabelObstacle>& PreparedObstacles;
		const FTechWheelLabelRing& Ring;
		TArray<FTechWheelLabelPlacementResult>& Placements;
	};

	void SetLabelSide(const FLabelPlacementState& State, const int32 LabelIndex, const ETechWheelLabelSide Side)
	{
		State.Placements[LabelIndex].Side = Side;
		State.Placements[LabelIndex].Rect = TechWheelLabelPlacement::GetCandidateRect(State.Requests[LabelIndex], Side);
	}

	/** What a label covers where it is now, the other labels included. */
	float GetPlacedCost(const FLabelPlacementState& State, const int32 LabelIndex)
	{
		const FLabelCostInput Input = {State.PreparedObstacles, State.Ring, &State.Placements};
		return GetSideCost(State.Requests[LabelIndex], LabelIndex, State.Placements[LabelIndex].Side, Input);
	}

	/** The placed cost plus the price of the side, the number PickSide compares. */
	float GetPlacedScore(const FLabelPlacementState& State, const int32 LabelIndex)
	{
		const int32 SideIndex = static_cast<int32>(State.Placements[LabelIndex].Side);
		return GetPlacedCost(State, LabelIndex) + SidePreferenceCost * SideIndex;
	}

	/** @return true when a label changed side. */
	bool RefinePlacements(const FLabelPlacementState& State)
	{
		const FLabelCostInput Input = {State.PreparedObstacles, State.Ring, &State.Placements};
		bool bHasChanged = false;
		for (int32 LabelIndex = 0; LabelIndex < State.Requests.Num(); ++LabelIndex)
		{
			const ETechWheelLabelSide Side = PickSide(State.Requests[LabelIndex], LabelIndex, Input);
			if (Side == State.Placements[LabelIndex].Side)
			{
				continue;
			}
			SetLabelSide(State, LabelIndex, Side);
			bHasChanged = true;
		}
		return bHasChanged;
	}

	/** Whether the other label, where it is now, takes room from any side of the label. */
	bool GetIsLabelInTheWay(const FLabelPlacementState& State, const int32 LabelIndex, const int32 OtherIndex)
	{
		const FTechWheelLabelRect OtherRect = InflateRect(State.Placements[OtherIndex].Rect, ObstacleMargin);
		const int32 SideCount = static_cast<int32>(ETechWheelLabelSide::Count);
		for (int32 SideIndex = 0; SideIndex < SideCount; ++SideIndex)
		{
			const FTechWheelLabelRect Rect = TechWheelLabelPlacement::GetCandidateRect(
				State.Requests[LabelIndex], static_cast<ETechWheelLabelSide>(SideIndex));
			if (TechWheelLabelPlacement::GetOverlapArea(Rect, OtherRect) > 0.f)
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * @brief Tries every pair of sides for two labels and keeps the pair that covers the least together. One label
	 * at a time cannot get out when another label sits on its only free side; moving both at once can.
	 * @param State The labels being placed.
	 * @param FirstIndex The label that is in trouble.
	 * @param SecondIndex A label in its way.
	 * @return true when one of the two changed side.
	 */
	bool TryMovePair(const FLabelPlacementState& State, const int32 FirstIndex, const int32 SecondIndex)
	{
		const ETechWheelLabelSide FirstStartSide = State.Placements[FirstIndex].Side;
		const ETechWheelLabelSide SecondStartSide = State.Placements[SecondIndex].Side;
		ETechWheelLabelSide BestFirstSide = FirstStartSide;
		ETechWheelLabelSide BestSecondSide = SecondStartSide;
		float BestScore = GetPlacedScore(State, FirstIndex) + GetPlacedScore(State, SecondIndex);
		const int32 SideCount = static_cast<int32>(ETechWheelLabelSide::Count);
		for (int32 PairIndex = 0; PairIndex < SideCount * SideCount; ++PairIndex)
		{
			const ETechWheelLabelSide FirstSide = static_cast<ETechWheelLabelSide>(PairIndex / SideCount);
			const ETechWheelLabelSide SecondSide = static_cast<ETechWheelLabelSide>(PairIndex % SideCount);
			SetLabelSide(State, FirstIndex, FirstSide);
			SetLabelSide(State, SecondIndex, SecondSide);
			const float Score = GetPlacedScore(State, FirstIndex) + GetPlacedScore(State, SecondIndex);
			if (Score < BestScore - FreeAreaTolerance)
			{
				BestFirstSide = FirstSide;
				BestSecondSide = SecondSide;
				BestScore = Score;
			}
		}
		SetLabelSide(State, FirstIndex, BestFirstSide);
		SetLabelSide(State, SecondIndex, BestSecondSide);
		return BestFirstSide != FirstStartSide || BestSecondSide != SecondStartSide;
	}

	/** @return true when a label changed side. */
	bool MoveLabelsInTheWayOf(const FLabelPlacementState& State, const int32 LabelIndex)
	{
		bool bHasChanged = false;
		for (int32 OtherIndex = 0; OtherIndex < State.Requests.Num(); ++OtherIndex)
		{
			if (OtherIndex == LabelIndex || not GetIsLabelInTheWay(State, LabelIndex, OtherIndex))
			{
				continue;
			}
			bHasChanged |= TryMovePair(State, LabelIndex, OtherIndex);
		}
		return bHasChanged;
	}

	/** Helps every label that still covers something by moving it together with a label in its way. */
	bool MoveBlockedPairs(const FLabelPlacementState& State)
	{
		bool bHasChanged = false;
		for (int32 LabelIndex = 0; LabelIndex < State.Requests.Num(); ++LabelIndex)
		{
			if (GetPlacedCost(State, LabelIndex) <= FreeAreaTolerance)
			{
				continue;
			}
			bHasChanged |= MoveLabelsInTheWayOf(State, LabelIndex);
		}
		return bHasChanged;
	}

	/** Offset of the label's centre from the node's centre for one side. */
	FVector2D GetSideOffset(const FTechWheelLabelRequest& Request, const ETechWheelLabelSide Side)
	{
		const float Radius = Request.OutlineRadius;
		const float HalfLabelWidth = static_cast<float>(Request.LabelSize.X) * 0.5f;
		const float HalfLabelHeight = static_cast<float>(Request.LabelSize.Y) * 0.5f;
		const float HexagonHalfHeight = Radius * HexagonFlatEdgeScale;
		// Below and above: clear of the flat edges.
		const float FlatY = HexagonHalfHeight + LabelGap + HalfLabelHeight;
		// Right and left: clear of the corners.
		const float CornerX = Radius + LabelGap + HalfLabelWidth;
		// Beside a slanted edge: as close as the edge allows at the label's inner corner.
		const float SlantY = HexagonHalfHeight * SlantedSideHeightScale;
		const float SlantX = TechWheelLabelPlacement::GetHexagonHalfWidthAt(Radius, SlantY - HalfLabelHeight)
			+ LabelGap + HalfLabelWidth;
		// Beside the end of a flat edge: the label starts where the edge ends.
		const float FlatEndX = Radius * HexagonFlatEdgeHalfWidthScale + HalfLabelWidth;

		switch (Side)
		{
		case ETechWheelLabelSide::Below:
			return FVector2D(0.f, FlatY);
		case ETechWheelLabelSide::Above:
			return FVector2D(0.f, -FlatY);
		case ETechWheelLabelSide::Right:
			return FVector2D(CornerX, 0.f);
		case ETechWheelLabelSide::Left:
			return FVector2D(-CornerX, 0.f);
		case ETechWheelLabelSide::LowerRight:
			return FVector2D(SlantX, SlantY);
		case ETechWheelLabelSide::LowerLeft:
			return FVector2D(-SlantX, SlantY);
		case ETechWheelLabelSide::UpperRight:
			return FVector2D(SlantX, -SlantY);
		case ETechWheelLabelSide::UpperLeft:
			return FVector2D(-SlantX, -SlantY);
		case ETechWheelLabelSide::BelowRight:
			return FVector2D(FlatEndX, FlatY);
		case ETechWheelLabelSide::BelowLeft:
			return FVector2D(-FlatEndX, FlatY);
		case ETechWheelLabelSide::AboveRight:
			return FVector2D(FlatEndX, -FlatY);
		case ETechWheelLabelSide::AboveLeft:
			return FVector2D(-FlatEndX, -FlatY);
		case ETechWheelLabelSide::Count:
			break;
		}
		return FVector2D(0.f, FlatY);
	}
}

float TechWheelLabelPlacement::GetOverlapArea(const FTechWheelLabelRect& First, const FTechWheelLabelRect& Second)
{
	const double OverlapWidth = FMath::Min(First.Max.X, Second.Max.X) - FMath::Max(First.Min.X, Second.Min.X);
	const double OverlapHeight = FMath::Min(First.Max.Y, Second.Max.Y) - FMath::Max(First.Min.Y, Second.Min.Y);
	if (OverlapWidth <= 0.0 || OverlapHeight <= 0.0)
	{
		return 0.f;
	}
	return static_cast<float>(OverlapWidth * OverlapHeight);
}

float TechWheelLabelPlacement::GetAreaOutsideRing(const FTechWheelLabelRect& Rect, const FTechWheelLabelRing& Ring)
{
	using namespace TechWheelLabelPlacementPrivate;
	const float AllowedRadius = Ring.Radius - RingInset;
	const double AllowedRadiusSquared = static_cast<double>(AllowedRadius) * AllowedRadius;
	const FVector2D Size = Rect.GetSize();
	int32 SamplesOutside = 0;
	for (int32 SampleIndex = 0; SampleIndex < RingSampleColumns * RingSampleRows; ++SampleIndex)
	{
		const double AlphaX = static_cast<double>(SampleIndex % RingSampleColumns) / (RingSampleColumns - 1);
		const double AlphaY = static_cast<double>(SampleIndex / RingSampleColumns) / (RingSampleRows - 1);
		const FVector2D SamplePoint = Rect.Min + FVector2D(Size.X * AlphaX, Size.Y * AlphaY);
		SamplesOutside += FVector2D::DistSquared(SamplePoint, Ring.Centre) > AllowedRadiusSquared ? 1 : 0;
	}
	const float OutsideFraction = static_cast<float>(SamplesOutside) / (RingSampleColumns * RingSampleRows);
	return OutsideFraction * GetRectArea(Rect);
}

float TechWheelLabelPlacement::GetHexagonHalfWidthAt(const float Radius, const float OffsetY)
{
	using namespace TechWheelLabelPlacementPrivate;
	const float Height = FMath::Abs(OffsetY);
	if (Height > Radius * HexagonFlatEdgeScale)
	{
		return 0.f;
	}
	return Radius - Height * HexagonSlantRunPerRise;
}

void TechWheelLabelPlacement::AppendHexagonObstacles(const FVector2D& Centre, const float Radius,
                                                     TArray<FTechWheelLabelObstacle>& OutObstacles,
                                                     const int32 IgnoredByLabelIndex)
{
	using namespace TechWheelLabelPlacementPrivate;
	const float HalfHeight = Radius * HexagonFlatEdgeScale;
	const float SliceHeight = HalfHeight * 2.f / HexagonSlices;
	for (int32 SliceIndex = 0; SliceIndex < HexagonSlices; ++SliceIndex)
	{
		const float SliceTop = -HalfHeight + SliceHeight * SliceIndex;
		const float SliceBottom = SliceTop + SliceHeight;
		// As wide as the slice's widest end, so the stack never falls inside the hexagon.
		const float HalfWidth = GetHexagonHalfWidthAt(
			Radius, FMath::Min(FMath::Abs(SliceTop), FMath::Abs(SliceBottom)));
		FTechWheelLabelObstacle& Slice = OutObstacles.AddDefaulted_GetRef();
		Slice.Rect.Min = Centre + FVector2D(-HalfWidth, SliceTop);
		Slice.Rect.Max = Centre + FVector2D(HalfWidth, SliceBottom);
		Slice.IgnoredByLabelIndex = IgnoredByLabelIndex;
	}
}

FTechWheelLabelRect TechWheelLabelPlacement::GetCandidateRect(const FTechWheelLabelRequest& Request,
                                                             const ETechWheelLabelSide Side)
{
	const FVector2D LabelCentre = Request.NodeCentre + TechWheelLabelPlacementPrivate::GetSideOffset(Request, Side);
	return FTechWheelLabelRect::MakeCentred(LabelCentre, Request.LabelSize * 0.5);
}

void TechWheelLabelPlacement::PlaceLabels(const TArray<FTechWheelLabelRequest>& Requests,
                                          const TArray<FTechWheelLabelObstacle>& Obstacles,
                                          const FTechWheelLabelRing& Ring,
                                          TArray<FTechWheelLabelPlacementResult>& OutPlacements)
{
	using namespace TechWheelLabelPlacementPrivate;
	OutPlacements.Reset();
	const TArray<FTechWheelLabelObstacle> PreparedObstacles = PrepareObstacles(Requests, Obstacles);

	// First against the nodes only, so every label starts from its own best side.
	const FLabelCostInput ObstaclesOnly = {PreparedObstacles, Ring, nullptr};
	for (int32 LabelIndex = 0; LabelIndex < Requests.Num(); ++LabelIndex)
	{
		FTechWheelLabelPlacementResult& Placement = OutPlacements.AddDefaulted_GetRef();
		Placement.Side = PickSide(Requests[LabelIndex], LabelIndex, ObstaclesOnly);
		Placement.Rect = GetCandidateRect(Requests[LabelIndex], Placement.Side);
	}

	// Then against each other, one at a time and in pairs, until no label moves any more.
	const FLabelPlacementState State = {Requests, PreparedObstacles, Ring, OutPlacements};
	for (int32 PassIndex = 0; PassIndex < MaxRefinePasses; ++PassIndex)
	{
		const bool bHasRefined = RefinePlacements(State);
		const bool bHasMovedPairs = MoveBlockedPairs(State);
		if (not bHasRefined && not bHasMovedPairs)
		{
			return;
		}
	}
}
