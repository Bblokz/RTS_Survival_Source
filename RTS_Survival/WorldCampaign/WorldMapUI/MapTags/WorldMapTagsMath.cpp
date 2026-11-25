// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/MapTags/WorldMapTagsMath.h"

namespace WorldMapTagsMathPrivate
{
	FVector2D GetXY(const FVector& Location)
	{
		return FVector2D(Location.X, Location.Y);
	}

	/**
	 * One Liang-Barsky edge test: narrows the [Enter, Exit] range of the segment parameter that lies inside the edge.
	 * @return false when the segment lies wholly outside this edge.
	 */
	bool ClipAgainstEdge(const double Denominator, const double Numerator, double& InOutEnter, double& InOutExit)
	{
		if (FMath::IsNearlyZero(Denominator))
		{
			// Parallel to the edge: inside or outside along its whole length.
			return Numerator >= 0.0;
		}

		const double EdgeParameter = Numerator / Denominator;
		if (Denominator < 0.0)
		{
			if (EdgeParameter > InOutExit)
			{
				return false;
			}
			InOutEnter = FMath::Max(InOutEnter, EdgeParameter);
			return true;
		}

		if (EdgeParameter < InOutEnter)
		{
			return false;
		}
		InOutExit = FMath::Min(InOutExit, EdgeParameter);
		return true;
	}

	constexpr double HalfOfSize = 0.5;
	// Tags this little short of the gap still count as clear, so a tag nudged to exactly the gap is not seen as
	// overlapping again because of rounding.
	constexpr double TagGapTolerance = 0.01;

	/** @return Index of the first kept tag the given tag comes too close to; INDEX_NONE when it is free. */
	int32 FindBlockingTagIndex(const FWorldMapTagPlacement& Tag, const TArray<FWorldMapTagPlacement>& KeptTags,
	                           const float Gap)
	{
		for (int32 KeptIndex = 0; KeptIndex < KeptTags.Num(); ++KeptIndex)
		{
			const FWorldMapTagPlacement& KeptTag = KeptTags[KeptIndex];
			if (WorldMapTagsMath::GetDoTagsOverlap(Tag.Center, Tag.Size, KeptTag.Center, KeptTag.Size, Gap))
			{
				return KeptIndex;
			}
		}
		return INDEX_NONE;
	}

	/**
	 * Finds room for one tag among the tags already kept, nudging it past the first one in its way.
	 * @return false when the tag has to be hidden.
	 */
	bool TryPlaceTag(FWorldMapTagPlacement& InOutTag, const TArray<FWorldMapTagPlacement>& KeptTags, const float Gap,
	                 const float TopBarHeight)
	{
		if (not WorldMapTagsMath::GetIsTagClearOfTopBar(InOutTag.Center, InOutTag.Size, TopBarHeight))
		{
			return false;
		}

		const int32 BlockingTagIndex = FindBlockingTagIndex(InOutTag, KeptTags, Gap);
		if (BlockingTagIndex == INDEX_NONE)
		{
			return true;
		}

		// Straight up or down, whichever side of the kept tag this one is already on.
		const FWorldMapTagPlacement& BlockingTag = KeptTags[BlockingTagIndex];
		const double ClearDistance = (InOutTag.Size.Y + BlockingTag.Size.Y) * HalfOfSize + Gap;
		const bool bMovesUp = InOutTag.Center.Y <= BlockingTag.Center.Y;
		InOutTag.Center.Y = BlockingTag.Center.Y + (bMovesUp ? -ClearDistance : ClearDistance);
		return WorldMapTagsMath::GetIsTagClearOfTopBar(InOutTag.Center, InOutTag.Size, TopBarHeight)
			&& FindBlockingTagIndex(InOutTag, KeptTags, Gap) == INDEX_NONE;
	}
}

float WorldMapTagsMath::GetRemainingPathLengthXY(const FVector& CurrentLocation, const TArray<FVector>& PathPoints,
                                                 const int32 NextPathPointIndex)
{
	using namespace WorldMapTagsMathPrivate;
	if (not PathPoints.IsValidIndex(NextPathPointIndex))
	{
		return 0.f;
	}

	double RemainingLength = FVector2D::Distance(GetXY(CurrentLocation), GetXY(PathPoints[NextPathPointIndex]));
	for (int32 PathPointIndex = NextPathPointIndex + 1; PathPointIndex < PathPoints.Num(); ++PathPointIndex)
	{
		RemainingLength += FVector2D::Distance(GetXY(PathPoints[PathPointIndex - 1]), GetXY(PathPoints[PathPointIndex]));
	}
	return static_cast<float>(RemainingLength);
}

int32 WorldMapTagsMath::GetTurnsAway(const float RemainingDistance, const float DistancePerTurn)
{
	if (RemainingDistance <= KINDA_SMALL_NUMBER)
	{
		return 0;
	}

	if (DistancePerTurn <= KINDA_SMALL_NUMBER)
	{
		return INDEX_NONE;
	}

	return FMath::Max(1, FMath::CeilToInt(RemainingDistance / DistancePerTurn));
}

bool WorldMapTagsMath::ClipSegmentToRect(FVector2D& InOutStart, FVector2D& InOutEnd, const FVector2D& RectMin,
                                         const FVector2D& RectMax)
{
	using namespace WorldMapTagsMathPrivate;
	const FVector2D Delta = InOutEnd - InOutStart;
	double Enter = 0.0;
	double Exit = 1.0;
	const bool bIsInside = ClipAgainstEdge(-Delta.X, InOutStart.X - RectMin.X, Enter, Exit)
		&& ClipAgainstEdge(Delta.X, RectMax.X - InOutStart.X, Enter, Exit)
		&& ClipAgainstEdge(-Delta.Y, InOutStart.Y - RectMin.Y, Enter, Exit)
		&& ClipAgainstEdge(Delta.Y, RectMax.Y - InOutStart.Y, Enter, Exit);
	if (not bIsInside || Enter > Exit)
	{
		return false;
	}

	const FVector2D OriginalStart = InOutStart;
	InOutStart = OriginalStart + Delta * Enter;
	InOutEnd = OriginalStart + Delta * Exit;
	return true;
}

void WorldMapTagsMath::BuildDashSegments(const FVector2D& Start, const FVector2D& End, const float DashLength,
                                         const float GapLength, const int32 MaxDashCount,
                                         TArray<FWorldMapDashSegment>& OutDashes)
{
	OutDashes.Reset();
	const double LineLength = FVector2D::Distance(Start, End);
	if (DashLength <= KINDA_SMALL_NUMBER || LineLength <= KINDA_SMALL_NUMBER || MaxDashCount <= 0)
	{
		return;
	}

	const FVector2D Direction = (End - Start) / LineLength;
	const double Stride = DashLength + FMath::Max(0.f, GapLength);
	for (double DashStart = 0.0; DashStart < LineLength && OutDashes.Num() < MaxDashCount; DashStart += Stride)
	{
		FWorldMapDashSegment Dash;
		Dash.Start = Start + Direction * DashStart;
		Dash.End = Start + Direction * FMath::Min(DashStart + DashLength, LineLength);
		OutDashes.Add(Dash);
	}
}

bool WorldMapTagsMath::TryGetCountryLabelPoint(const TArray<FVector>& AnchorLocations, FVector& OutLabelPoint)
{
	if (AnchorLocations.Num() == 0)
	{
		return false;
	}

	FVector LocationSum = FVector::ZeroVector;
	for (const FVector& AnchorLocation : AnchorLocations)
	{
		LocationSum += AnchorLocation;
	}
	OutLabelPoint = LocationSum / static_cast<double>(AnchorLocations.Num());
	return true;
}

bool WorldMapTagsMath::GetIsTagClearOfTopBar(const FVector2D& TagCenter, const FVector2D& TagSize,
                                             const float TopBarHeight)
{
	const double TagTop = TagCenter.Y - TagSize.Y * WorldMapTagsMathPrivate::HalfOfSize;
	return TagTop >= TopBarHeight;
}

bool WorldMapTagsMath::GetDoTagsOverlap(const FVector2D& CenterA, const FVector2D& SizeA, const FVector2D& CenterB,
                                        const FVector2D& SizeB, const float Gap)
{
	using namespace WorldMapTagsMathPrivate;
	const double NeededGap = Gap - TagGapTolerance;
	const double ClearDistanceX = (SizeA.X + SizeB.X) * HalfOfSize + NeededGap;
	const double ClearDistanceY = (SizeA.Y + SizeB.Y) * HalfOfSize + NeededGap;
	return FMath::Abs(CenterA.X - CenterB.X) < ClearDistanceX && FMath::Abs(CenterA.Y - CenterB.Y) < ClearDistanceY;
}

FVector2D WorldMapTagsMath::GetTagCenterClearOfMarker(const FVector2D& TagCenter, const FVector2D& TagSize,
                                                      const FVector2D& MarkerCenter, const FVector2D& MarkerHalfSize,
                                                      const float Gap)
{
	using namespace WorldMapTagsMathPrivate;
	const FVector2D MarkerSize = MarkerHalfSize / HalfOfSize;
	if (not GetDoTagsOverlap(TagCenter, TagSize, MarkerCenter, MarkerSize, Gap))
	{
		return TagCenter;
	}

	const double MarkerTop = MarkerCenter.Y - MarkerHalfSize.Y;
	return FVector2D(TagCenter.X, MarkerTop - Gap - TagSize.Y * HalfOfSize);
}

void WorldMapTagsMath::ResolveTagOverlaps(TArray<FWorldMapTagPlacement>& InOutTags, const float Gap,
                                          const float TopBarHeight)
{
	// A copy of every tag that stays, so later tags are tested against the nudged positions.
	TArray<FWorldMapTagPlacement> KeptTags;
	KeptTags.Reserve(InOutTags.Num());
	for (FWorldMapTagPlacement& Tag : InOutTags)
	{
		if (not Tag.bIsVisible)
		{
			continue;
		}

		Tag.bIsVisible = WorldMapTagsMathPrivate::TryPlaceTag(Tag, KeptTags, Gap, TopBarHeight);
		if (Tag.bIsVisible)
		{
			KeptTags.Add(Tag);
		}
	}
}
