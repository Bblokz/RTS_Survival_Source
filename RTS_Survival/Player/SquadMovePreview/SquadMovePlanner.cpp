#include "SquadMovePlanner.h"

namespace SquadMovePlannerPrivate
{
	// How many times a regular slot is pushed back one row to get clear of an already planned position.
	constexpr int32 MaximumSlotPushBacks = 4;

	/** Axes and shared bookkeeping for one BuildPlan call. */
	struct FPlanContext
	{
		FVector Facing = FVector::ForwardVector;
		FVector Right = FVector::RightVector;
		TSet<int64> UsedCoverPointIds;
		TArray<FVector> OccupiedLocations;
	};

	/** Where one squad stands inside the selected formation shape, relative to the cursor and the facing. */
	struct FSquadSlot
	{
		float LateralOffset = 0.0f;
		float DepthBehindAnchor = 0.0f;
		float YawDegrees = 0.0f;
		int32 RowIndex = 0;
	};

	/** Columns and rows of the block a number of soldiers forms in the open. */
	struct FSquadBlockShape
	{
		int32 Columns = 1;
		int32 Rows = 1;
	};

	/** A wide rectangle with rows as even as possible, so seven men stand 4 + 3 instead of 5 + 2 or in one line. */
	FSquadBlockShape GetSquadBlockShape(const FSquadMovePlannerSettings& Settings, const int32 UnitCount)
	{
		FSquadBlockShape Shape;
		if (UnitCount <= 0)
		{
			return Shape;
		}
		const int32 MaximumColumns = FMath::Max(1, Settings.MaximumRegularUnitsPerRow);
		const int32 WidestColumns = FMath::Clamp(
			FMath::CeilToInt32(FMath::Sqrt(static_cast<float>(UnitCount) * Settings.RegularBlockWidthToDepthRatio)),
			1,
			MaximumColumns);
		Shape.Rows = FMath::DivideAndRoundUp(UnitCount, WidestColumns);
		Shape.Columns = FMath::DivideAndRoundUp(UnitCount, Shape.Rows);
		return Shape;
	}

	TArray<int32> CountUnitsPerSquad(const FSquadMovePlanRequest& Request)
	{
		TArray<int32> UnitCounts;
		UnitCounts.SetNumZeroed(FMath::Max(0, Request.SquadCount));
		for (const FSquadMovePlannerUnit& Unit : Request.Units)
		{
			if (UnitCounts.IsValidIndex(Unit.SquadIndex))
			{
				++UnitCounts[Unit.SquadIndex];
			}
		}
		return UnitCounts;
	}

	/** Centre-to-centre distance of neighbouring squads: the largest squad's block plus the gap between blocks. */
	FVector2D GetSquadSlotSpacing(const FSquadMovePlanRequest& Request)
	{
		const FSquadMovePlannerSettings& Settings = Request.Settings;
		FSquadBlockShape LargestShape;
		for (const int32 UnitCount : CountUnitsPerSquad(Request))
		{
			const FSquadBlockShape Shape = GetSquadBlockShape(Settings, UnitCount);
			LargestShape.Columns = FMath::Max(LargestShape.Columns, Shape.Columns);
			LargestShape.Rows = FMath::Max(LargestShape.Rows, Shape.Rows);
		}
		return FVector2D(
			static_cast<float>(LargestShape.Columns - 1) * Settings.RegularUnitSpacing + Settings.SquadGap,
			static_cast<float>(LargestShape.Rows - 1) * Settings.RegularRowSpacing + Settings.SquadGap);
	}

	int32 GetSquadsInFormationRow(const FSquadMovePlanRequest& Request, const int32 RowIndex)
	{
		const FSquadMovePlannerSettings& Settings = Request.Settings;
		switch (Request.Formation)
		{
		case EFormation::SpearFormation:
			return FMath::Min(RowIndex + 1, FMath::Max(1, Settings.SpearSquadsPerRow));
		case EFormation::ThinSpearFormation:
			return FMath::Min(RowIndex + 1, FMath::Max(1, Settings.ThinSpearSquadsPerRow));
		case EFormation::SemiCircleFormation:
			return FMath::Max(1, Settings.SemiCircleFirstArcSquads) + 2 * RowIndex;
		case EFormation::RectangleFormation:
		default:
			return FMath::Max(1, FMath::CeilToInt32(FMath::Sqrt(static_cast<float>(Request.SquadCount))));
		}
	}

	/** Squads on the wings of a semi circle turn outward and stand a little further back. */
	void ApplySemiCircleFan(const FSquadMovePlannerSettings& Settings, const float LateralIndex, FSquadSlot& InOutSlot)
	{
		constexpr float SagHalfAngleRatio = 0.5f;
		InOutSlot.YawDegrees = FMath::Clamp(
			LateralIndex * Settings.SemiCircleFanDegreesPerSquad,
			-Settings.MaximumSemiCircleFanDegrees,
			Settings.MaximumSemiCircleFanDegrees);
		InOutSlot.DepthBehindAnchor += FMath::Abs(InOutSlot.LateralOffset) * FMath::Tan(
			FMath::DegreesToRadians(FMath::Abs(InOutSlot.YawDegrees)) * SagHalfAngleRatio);
	}

	/**
	 * One slot per squad in the selected shape, front row first and left to right inside a row. The rectangle is
	 * centred on the cursor; the spear and the semi circle have their front row on it, like the mixed formation.
	 */
	TArray<FSquadSlot> BuildSquadSlots(const FSquadMovePlanRequest& Request)
	{
		TArray<FSquadSlot> Slots;
		const FVector2D SlotSpacing = GetSquadSlotSpacing(Request);
		int32 RowIndex = 0;
		while (Slots.Num() < Request.SquadCount)
		{
			const int32 SquadsInRow = FMath::Min(
				GetSquadsInFormationRow(Request, RowIndex),
				Request.SquadCount - Slots.Num());
			for (int32 ColumnIndex = 0; ColumnIndex < SquadsInRow; ++ColumnIndex)
			{
				const float LateralIndex = static_cast<float>(ColumnIndex) - static_cast<float>(SquadsInRow - 1) * 0.5f;
				FSquadSlot& Slot = Slots.AddDefaulted_GetRef();
				Slot.RowIndex = RowIndex;
				Slot.LateralOffset = LateralIndex * SlotSpacing.X;
				Slot.DepthBehindAnchor = static_cast<float>(RowIndex) * SlotSpacing.Y;
				if (Request.Formation == EFormation::SemiCircleFormation)
				{
					ApplySemiCircleFan(Request.Settings, LateralIndex, Slot);
				}
			}
			++RowIndex;
		}
		if (Request.Formation == EFormation::RectangleFormation)
		{
			const float HalfDepth = static_cast<float>(RowIndex - 1) * 0.5f * SlotSpacing.Y;
			for (FSquadSlot& Slot : Slots)
			{
				Slot.DepthBehindAnchor -= HalfDepth;
			}
		}
		return Slots;
	}

	/** A cover point that is in reach of one squad, with the distance-like cost used to rank it. */
	struct FCoverCandidate
	{
		int32 CoverPointIndex = INDEX_NONE;
		float Score = 0.0f;
	};

	bool GetIsLocationFree(const FPlanContext& Context, const FVector& Location, const float MinimumSpacing)
	{
		const float MinimumSpacingSquared = FMath::Square(MinimumSpacing);
		for (const FVector& OccupiedLocation : Context.OccupiedLocations)
		{
			if (FVector::DistSquared2D(OccupiedLocation, Location) < MinimumSpacingSquared)
			{
				return false;
			}
		}
		return true;
	}

	ESquadPlannedPositionType GetPlannedTypeForCover(const ERTSCoverType CoverType)
	{
		return CoverType == ERTSCoverType::Crouch
			? ESquadPlannedPositionType::CrouchCover
			: ESquadPlannedPositionType::StandingCover;
	}

	float GetCoverSnapRadius(const FSquadMovePlannerSettings& Settings, const int32 SquadUnitCount)
	{
		return FMath::Min(
			Settings.MaximumCoverSnapRadius,
			Settings.CoverSnapBaseRadius + Settings.CoverSnapRadiusPerUnit * static_cast<float>(SquadUnitCount));
	}

	FVector GetSquadCentre(const FSquadMovePlanRequest& Request, const int32 SquadIndex)
	{
		FVector LocationSum = FVector::ZeroVector;
		int32 UnitCount = 0;
		for (const FSquadMovePlannerUnit& Unit : Request.Units)
		{
			if (Unit.SquadIndex == SquadIndex)
			{
				LocationSum += Unit.Location;
				++UnitCount;
			}
		}
		return UnitCount > 0 ? LocationSum / static_cast<float>(UnitCount) : FVector::ZeroVector;
	}

	/**
	 * Decides which squad takes which slot (the result is parallel to Slots): the squads that already stand
	 * furthest forward take the front rows, and inside a row they keep their left-to-right order, so squads do
	 * not cross each other on the way.
	 */
	TArray<int32> AssignSquadsToSlots(
		const FSquadMovePlanRequest& Request,
		const FPlanContext& Context,
		const TArray<FSquadSlot>& Slots)
	{
		TArray<FVector> SquadCentres;
		SquadCentres.Reserve(Request.SquadCount);
		TArray<int32> SquadPerSlot;
		SquadPerSlot.Reserve(Request.SquadCount);
		for (int32 SquadIndex = 0; SquadIndex < Request.SquadCount; ++SquadIndex)
		{
			SquadCentres.Add(GetSquadCentre(Request, SquadIndex));
			SquadPerSlot.Add(SquadIndex);
		}
		SquadPerSlot.Sort([&SquadCentres, &Context](const int32 FirstSquad, const int32 SecondSquad)
		{
			return FVector::DotProduct(SquadCentres[FirstSquad], Context.Facing) >
				FVector::DotProduct(SquadCentres[SecondSquad], Context.Facing);
		});
		int32 RowStart = 0;
		while (RowStart < Slots.Num())
		{
			int32 RowEnd = RowStart;
			while (RowEnd < Slots.Num() && Slots[RowEnd].RowIndex == Slots[RowStart].RowIndex)
			{
				++RowEnd;
			}
			TArrayView<int32> SquadsOfRow(SquadPerSlot.GetData() + RowStart, RowEnd - RowStart);
			SquadsOfRow.Sort([&SquadCentres, &Context](const int32 FirstSquad, const int32 SecondSquad)
			{
				return FVector::DotProduct(SquadCentres[FirstSquad], Context.Right) <
					FVector::DotProduct(SquadCentres[SecondSquad], Context.Right);
			});
			RowStart = RowEnd;
		}
		return SquadPerSlot;
	}

	/**
	 * Ranks the cover in reach of one squad. Distance to the squad's anchor decides; cover facing away from the
	 * squad's facing costs extra, and is dropped entirely once the player picked the facing with the arrow.
	 */
	TArray<FCoverCandidate> GatherCoverCandidates(
		const FSquadMovePlanRequest& Request,
		const FPlanContext& Context,
		const FVector& SquadAnchor,
		const FVector& SquadFacing,
		const int32 SquadUnitCount)
	{
		const FSquadMovePlannerSettings& Settings = Request.Settings;
		const float SnapRadiusSquared = FMath::Square(GetCoverSnapRadius(Settings, SquadUnitCount));
		TArray<FCoverCandidate> Candidates;
		for (int32 CoverPointIndex = 0; CoverPointIndex < Request.CoverPoints.Num(); ++CoverPointIndex)
		{
			const FRTSCoverPoint& CoverPoint = Request.CoverPoints[CoverPointIndex];
			const float DistanceSquared = FVector::DistSquared2D(CoverPoint.Location, SquadAnchor);
			if (DistanceSquared > SnapRadiusSquared || Context.UsedCoverPointIds.Contains(CoverPoint.PointId))
			{
				continue;
			}
			// The normal points from the cover to the soldier, so the soldier looks against it.
			const float FacingAlignment = FVector::DotProduct(
				-CoverPoint.CoverNormal.GetSafeNormal2D(),
				SquadFacing);
			if (Request.bFacingChosenByPlayer && FacingAlignment < Settings.MinimumChosenFacingAlignment)
			{
				continue;
			}
			FCoverCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.CoverPointIndex = CoverPointIndex;
			Candidate.Score = FMath::Sqrt(DistanceSquared)
				+ (1.0f - FacingAlignment) * 0.5f * Settings.FacingMisalignmentPenalty
				- (Request.PreviousCoverPointIds.Contains(CoverPoint.PointId) ? Settings.PreviousPlanBonus : 0.0f);
		}
		Candidates.Sort([](const FCoverCandidate& Left, const FCoverCandidate& Right)
		{
			return Left.Score < Right.Score;
		});
		return Candidates;
	}

	/** Takes the best-ranked cover for as many soldiers as possible while keeping planned positions apart. */
	void PickCoverPositions(
		const FSquadMovePlanRequest& Request,
		const TArray<FCoverCandidate>& Candidates,
		const int32 WantedCount,
		FPlanContext& InOutContext,
		TArray<FSquadUnitPlannedPosition>& OutPositions)
	{
		for (const FCoverCandidate& Candidate : Candidates)
		{
			if (OutPositions.Num() >= WantedCount)
			{
				return;
			}
			const FRTSCoverPoint& CoverPoint = Request.CoverPoints[Candidate.CoverPointIndex];
			if (not GetIsLocationFree(InOutContext, CoverPoint.Location, Request.Settings.MinimumSlotSpacing))
			{
				continue;
			}
			FSquadUnitPlannedPosition& Position = OutPositions.AddDefaulted_GetRef();
			Position.Location = CoverPoint.Location;
			Position.Facing = -CoverPoint.CoverNormal.GetSafeNormal2D();
			Position.Type = GetPlannedTypeForCover(CoverPoint.CoverType);
			Position.CoverPoint = CoverPoint;
			InOutContext.UsedCoverPointIds.Add(CoverPoint.PointId);
			InOutContext.OccupiedLocations.Add(CoverPoint.Location);
		}
	}

	/**
	 * Where the front row of the soldiers without cover stands: a block centred on the squad's anchor when
	 * nobody found cover, otherwise rows behind the men in cover so they are not left in front of the wall.
	 */
	void GetRegularFormationFrame(
		const FSquadMovePlanRequest& Request,
		const FVector& SquadAnchor,
		const FVector& SquadFacing,
		const int32 RegularCount,
		const TArray<FSquadUnitPlannedPosition>& CoverPositions,
		FVector& OutOrigin,
		FVector& OutForward)
	{
		OutForward = SquadFacing;
		if (CoverPositions.IsEmpty())
		{
			const FSquadBlockShape Shape = GetSquadBlockShape(Request.Settings, RegularCount);
			OutOrigin = SquadAnchor
				+ SquadFacing * (static_cast<float>(Shape.Rows - 1) * 0.5f * Request.Settings.RegularRowSpacing);
			return;
		}
		FVector CoverCentre = FVector::ZeroVector;
		FVector CoverFacingSum = FVector::ZeroVector;
		for (const FSquadUnitPlannedPosition& CoverPosition : CoverPositions)
		{
			CoverCentre += CoverPosition.Location;
			CoverFacingSum += CoverPosition.Facing;
		}
		CoverCentre /= static_cast<float>(CoverPositions.Num());
		const FVector CoverFacing = CoverFacingSum.GetSafeNormal2D();
		if (not CoverFacing.IsNearlyZero())
		{
			OutForward = CoverFacing;
		}
		OutOrigin = CoverCentre - OutForward * Request.Settings.RegularBehindCoverDistance;
	}

	void AddRegularPositions(
		const FSquadMovePlanRequest& Request,
		const FVector& Origin,
		const FVector& Forward,
		const int32 RegularCount,
		FPlanContext& InOutContext,
		TArray<FSquadUnitPlannedPosition>& OutPositions)
	{
		const FSquadMovePlannerSettings& Settings = Request.Settings;
		const FVector RowRight = FVector::CrossProduct(FVector::UpVector, Forward);
		const int32 UnitsPerRow = GetSquadBlockShape(Settings, RegularCount).Columns;
		for (int32 RegularIndex = 0; RegularIndex < RegularCount; ++RegularIndex)
		{
			const int32 RowIndex = RegularIndex / UnitsPerRow;
			const int32 UnitsInThisRow = FMath::Min(UnitsPerRow, RegularCount - RowIndex * UnitsPerRow);
			const float LateralIndex = static_cast<float>(RegularIndex % UnitsPerRow)
				- static_cast<float>(UnitsInThisRow - 1) * 0.5f;
			FVector SlotLocation = Origin
				+ RowRight * (LateralIndex * Settings.RegularUnitSpacing)
				- Forward * (static_cast<float>(RowIndex) * Settings.RegularRowSpacing);
			for (int32 PushBack = 0;
				PushBack < MaximumSlotPushBacks &&
				not GetIsLocationFree(InOutContext, SlotLocation, Settings.MinimumSlotSpacing);
				++PushBack)
			{
				SlotLocation -= Forward * Settings.RegularRowSpacing;
			}
			FSquadUnitPlannedPosition& Position = OutPositions.AddDefaulted_GetRef();
			Position.Location = SlotLocation;
			Position.Facing = Forward;
			Position.Type = ESquadPlannedPositionType::RegularStanding;
			InOutContext.OccupiedLocations.Add(SlotLocation);
		}
	}

	/**
	 * Hands the squad's positions to its soldiers left to right, matching how they stand now, which keeps
	 * their paths from crossing without solving a full assignment problem on every mouse move.
	 */
	void AssignPositionsToSquadUnits(
		const FSquadMovePlanRequest& Request,
		const FPlanContext& Context,
		const TArray<int32>& SquadUnitIndices,
		TArray<FSquadUnitPlannedPosition>& InOutSquadPositions,
		FSquadMovePlan& InOutPlan)
	{
		TArray<int32> UnitsLeftToRight = SquadUnitIndices;
		UnitsLeftToRight.Sort([&Request, &Context](const int32 Left, const int32 Right)
		{
			return FVector::DotProduct(Request.Units[Left].Location, Context.Right) <
				FVector::DotProduct(Request.Units[Right].Location, Context.Right);
		});
		InOutSquadPositions.Sort([&Context](const FSquadUnitPlannedPosition& Left, const FSquadUnitPlannedPosition& Right)
		{
			return FVector::DotProduct(Left.Location, Context.Right) <
				FVector::DotProduct(Right.Location, Context.Right);
		});
		const int32 AssignedCount = FMath::Min(UnitsLeftToRight.Num(), InOutSquadPositions.Num());
		for (int32 AssignmentIndex = 0; AssignmentIndex < AssignedCount; ++AssignmentIndex)
		{
			InOutPlan.UnitPositions[UnitsLeftToRight[AssignmentIndex]] = InOutSquadPositions[AssignmentIndex];
		}
	}

	void PlanSquad(
		const FSquadMovePlanRequest& Request,
		const FVector& SquadAnchor,
		const FVector& SquadFacing,
		const TArray<int32>& SquadUnitIndices,
		FPlanContext& InOutContext,
		FSquadMovePlan& InOutPlan)
	{
		const int32 SquadUnitCount = SquadUnitIndices.Num();
		if (SquadUnitCount == 0)
		{
			return;
		}
		TArray<FSquadUnitPlannedPosition> SquadPositions;
		SquadPositions.Reserve(SquadUnitCount);
		const TArray<FCoverCandidate> Candidates = GatherCoverCandidates(
			Request,
			InOutContext,
			SquadAnchor,
			SquadFacing,
			SquadUnitCount);
		PickCoverPositions(Request, Candidates, SquadUnitCount, InOutContext, SquadPositions);

		const int32 RegularCount = SquadUnitCount - SquadPositions.Num();
		FVector RegularOrigin = FVector::ZeroVector;
		FVector RegularForward = FVector::ForwardVector;
		GetRegularFormationFrame(
			Request,
			SquadAnchor,
			SquadFacing,
			RegularCount,
			SquadPositions,
			RegularOrigin,
			RegularForward);
		AddRegularPositions(Request, RegularOrigin, RegularForward, RegularCount, InOutContext, SquadPositions);
		AssignPositionsToSquadUnits(Request, InOutContext, SquadUnitIndices, SquadPositions, InOutPlan);
	}
}

float FSquadMovePlanner::GetCoverQueryRadius(const FSquadMovePlanRequest& Request)
{
	float FurthestSlotDistanceSquared = 0.0f;
	for (const SquadMovePlannerPrivate::FSquadSlot& Slot : SquadMovePlannerPrivate::BuildSquadSlots(Request))
	{
		FurthestSlotDistanceSquared = FMath::Max(
			FurthestSlotDistanceSquared,
			FMath::Square(Slot.LateralOffset) + FMath::Square(Slot.DepthBehindAnchor));
	}
	return FMath::Sqrt(FurthestSlotDistanceSquared) + Request.Settings.MaximumCoverSnapRadius;
}

void FSquadMovePlanner::BuildPlan(const FSquadMovePlanRequest& Request, FSquadMovePlan& OutPlan)
{
	using namespace SquadMovePlannerPrivate;
	OutPlan.Reset();
	OutPlan.UnitPositions.SetNum(Request.Units.Num());
	OutPlan.SquadAnchors.Init(Request.Anchor, FMath::Max(0, Request.SquadCount));
	OutPlan.SquadFacings.Init(Request.Facing.GetSafeNormal2D(), FMath::Max(0, Request.SquadCount));
	if (Request.Units.IsEmpty() || Request.SquadCount <= 0)
	{
		return;
	}

	FPlanContext Context;
	Context.Facing = Request.Facing.GetSafeNormal2D();
	if (Context.Facing.IsNearlyZero())
	{
		Context.Facing = FVector::ForwardVector;
	}
	Context.Right = FVector::CrossProduct(FVector::UpVector, Context.Facing);

	TArray<TArray<int32>> UnitIndicesPerSquad;
	UnitIndicesPerSquad.SetNum(Request.SquadCount);
	for (int32 UnitIndex = 0; UnitIndex < Request.Units.Num(); ++UnitIndex)
	{
		const int32 SquadIndex = Request.Units[UnitIndex].SquadIndex;
		if (UnitIndicesPerSquad.IsValidIndex(SquadIndex))
		{
			UnitIndicesPerSquad[SquadIndex].Add(UnitIndex);
		}
	}

	const TArray<FSquadSlot> Slots = BuildSquadSlots(Request);
	const TArray<int32> SquadPerSlot = AssignSquadsToSlots(Request, Context, Slots);
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		const FSquadSlot& Slot = Slots[SlotIndex];
		const int32 SquadIndex = SquadPerSlot[SlotIndex];
		const FVector SquadAnchor = Request.Anchor
			+ Context.Right * Slot.LateralOffset
			- Context.Facing * Slot.DepthBehindAnchor;
		const FVector SquadFacing = Context.Facing.RotateAngleAxis(Slot.YawDegrees, FVector::UpVector);
		OutPlan.SquadAnchors[SquadIndex] = SquadAnchor;
		OutPlan.SquadFacings[SquadIndex] = SquadFacing;
		PlanSquad(Request, SquadAnchor, SquadFacing, UnitIndicesPerSquad[SquadIndex], Context, OutPlan);
	}
}
