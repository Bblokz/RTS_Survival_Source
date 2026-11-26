// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldFow/WorldFogHintRules.h"

namespace WorldFogHintRuleConstants
{
	// Guards the divisions below; no road piece or mesh is this small.
	constexpr float MinLength = 1.f;
	constexpr float HalfValue = 0.5f;
}

bool WorldFogHintRules::GetShowsUnknownMarker(const EMapItemType ItemType, const EWorldMapFowState FowState,
                                              const bool bIsFogOpenAtItem, const bool bIsClearedByPlayer,
                                              const bool bShowUnknownMissionMarkers)
{
	if (not bShowUnknownMissionMarkers || bIsClearedByPlayer || bIsFogOpenAtItem)
	{
		return false;
	}

	// Explorable, Visible and POIVisible already show the real piece; only full fog is replaced.
	return ItemType == EMapItemType::Mission && FowState == EWorldMapFowState::NotVisible;
}

EWorldMissionFogPresentation WorldFogHintRules::GetMissionFogPresentation(
	const EMapItemType ItemType, const EWorldMapFowState FowState, const bool bIsFogOpenAtItem,
	const bool bIsClearedByPlayer, const bool bShowUnknownMissionMarkers,
	const EWorldUnknownMissionLook UnknownMissionLook, const bool bIsHiddenEnemyPieceInsideOpening)
{
	if (not GetShowsUnknownMarker(ItemType, FowState, bIsFogOpenAtItem, bIsClearedByPlayer, bShowUnknownMissionMarkers))
	{
		return EWorldMissionFogPresentation::AsFogState;
	}

	// Scouting must keep its value: an opening that would show a hidden enemy piece is not made.
	if (UnknownMissionLook == EWorldUnknownMissionLook::RealPiece && not bIsHiddenEnemyPieceInsideOpening)
	{
		return EWorldMissionFogPresentation::RealPieceThroughFog;
	}

	return EWorldMissionFogPresentation::PlainMarker;
}

float WorldFogHintRules::GetMissionOpeningExposureRadius(const float OpeningRadius, const float OpeningFalloff,
                                                         const float MaskPixelSize, const float EnemyPieceHalfWidth)
{
	return FMath::Max(OpeningRadius, 0.f) + FMath::Max(OpeningFalloff, 0.f) + FMath::Max(MaskPixelSize, 0.f)
		+ FMath::Max(EnemyPieceHalfWidth, 0.f);
}

bool WorldFogHintRules::GetIsAnyPieceInsideRadius(const FVector& Center, const TConstArrayView<FVector> PieceLocations,
                                                  const float Radius)
{
	const float RadiusSquared = FMath::Square(FMath::Max(Radius, 0.f));
	for (const FVector& PieceLocation : PieceLocations)
	{
		if (FVector::DistSquared2D(Center, PieceLocation) <= RadiusSquared)
		{
			return true;
		}
	}
	return false;
}

bool WorldFogHintRules::GetShowsRoadUnderFog(const EWorldMapFowState ConnectionState, const bool bIsFogOpenAtSegment,
                                             const bool bShowRoadsUnderFog)
{
	if (not bShowRoadsUnderFog || bIsFogOpenAtSegment)
	{
		return false;
	}

	return ConnectionState == EWorldMapFowState::NotVisible;
}

FLinearColor WorldFogHintRules::GetFogRoadColor(const FLinearColor& RoadColor, const float Brightness,
                                                const float Opacity)
{
	const float ClampedBrightness = FMath::Clamp(Brightness, 0.f, 1.f);
	const float ClampedOpacity = FMath::Clamp(Opacity, 0.f, 1.f);
	return FLinearColor(RoadColor.R * ClampedBrightness, RoadColor.G * ClampedBrightness,
	                    RoadColor.B * ClampedBrightness, RoadColor.A * ClampedOpacity);
}

void WorldFogHintRules::BuildFogRoadPath(const TArray<FVector>& RoadPath, const float PathHeight,
                                         const float MaxSegmentLength, TArray<FVector>& OutFogRoadPath)
{
	OutFogRoadPath.Reset();
	if (RoadPath.Num() < 2)
	{
		return;
	}

	const float SafeSegmentLength = FMath::Max(MaxSegmentLength, WorldFogHintRuleConstants::MinLength);
	OutFogRoadPath.Add(FVector(RoadPath[0].X, RoadPath[0].Y, PathHeight));
	for (int32 PointIndex = 1; PointIndex < RoadPath.Num(); ++PointIndex)
	{
		const FVector SegmentStart(RoadPath[PointIndex - 1].X, RoadPath[PointIndex - 1].Y, PathHeight);
		const FVector SegmentEnd(RoadPath[PointIndex].X, RoadPath[PointIndex].Y, PathHeight);
		const int32 PieceCount = FMath::Max(1, FMath::CeilToInt(FVector::Dist(SegmentStart, SegmentEnd) / SafeSegmentLength));
		for (int32 PieceIndex = 1; PieceIndex <= PieceCount; ++PieceIndex)
		{
			const float Alpha = static_cast<float>(PieceIndex) / static_cast<float>(PieceCount);
			OutFogRoadPath.Add(FMath::Lerp(SegmentStart, SegmentEnd, Alpha));
		}
	}
}

FVector WorldFogHintRules::GetUnknownMarkerScale(const float Diameter, const float Height, const float MeshSize)
{
	const float SafeMeshSize = FMath::Max(MeshSize, WorldFogHintRuleConstants::MinLength);
	const float WidthScale = FMath::Max(Diameter, 0.f) / SafeMeshSize;
	return FVector(WidthScale, WidthScale, FMath::Max(Height, 0.f) / SafeMeshSize);
}

float WorldFogHintRules::GetUnknownMarkerCenterZ(const float FogTopZ, const float HeightAboveFog,
                                                 const float MarkerHeight)
{
	return FogTopZ + FMath::Max(HeightAboveFog, 0.f) + FMath::Max(MarkerHeight, 0.f) * WorldFogHintRuleConstants::HalfValue;
}
