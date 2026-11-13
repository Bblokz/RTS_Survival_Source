// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UnitGroupMiniMapMarker.h"

#include "MinimapIconTypes.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace UnitGroupMiniMapMarkerConstants
{
	// The heading is re-measured once the group moved this far (units standing in combat jitter).
	constexpr float MinMoveForHeading = 500.f;
	// Share of a new heading taken per measurement; smooths the turn of a group that zigzags around obstacles.
	constexpr float HeadingBlend = 0.5f;
	// Icons are kept this share of the map's half size inside the minimap edge.
	constexpr float EdgeMarginFraction = 0.1f;
}

namespace
{
	/** @return The location pulled inside the minimap so an icon at the map edge is drawn whole. */
	FVector GetDrawLocation(const AFowManager* FowManager, const FVector& Location)
	{
		if (FowManager->GetMapExtent() <= 0.f)
		{
			return Location;
		}
		const FVector Center = FowManager->GetActorLocation();
		const float Limit = FowManager->GetMapExtent() * (1.f - UnitGroupMiniMapMarkerConstants::EdgeMarginFraction);
		return FVector(FMath::Clamp(Location.X, Center.X - Limit, Center.X + Limit),
		               FMath::Clamp(Location.Y, Center.Y - Limit, Center.Y + Limit), Location.Z);
	}
}

void RTSUnitGroupMiniMapMarker::AddIcons(const UObject* WorldContext, const FName IconId, const FVector& Location,
                                         const float HeadingYaw)
{
	AFowManager* FowManager = FRTS_Statics::GetFowManager(WorldContext);
	if (not IsValid(FowManager))
	{
		return;
	}
	const FMinimapIconTextPayload NoText{};
	const FVector DrawLocation = GetDrawLocation(FowManager, Location);
	FowManager->AddCustomMiniMapIcon(IconId, EMinimapIconType::IncomingWave, DrawLocation,
	                                 FRotator(0.f, HeadingYaw, 0.f), NoText);
	FowManager->AddCustomMiniMapIcon(GetBadgeIconId(IconId), EMinimapIconType::IncomingWaveBadge, DrawLocation,
	                                 FRotator::ZeroRotator, NoText);
}

void RTSUnitGroupMiniMapMarker::MoveIcons(const UObject* WorldContext, const FName IconId, const FVector& Location,
                                          const float HeadingYaw)
{
	AFowManager* FowManager = FRTS_Statics::GetFowManager(WorldContext);
	if (not IsValid(FowManager))
	{
		return;
	}
	const FVector DrawLocation = GetDrawLocation(FowManager, Location);
	FowManager->SetCustomMiniMapIconTransform(IconId, DrawLocation, FRotator(0.f, HeadingYaw, 0.f));
	FowManager->SetCustomMiniMapIconTransform(GetBadgeIconId(IconId), DrawLocation, FRotator::ZeroRotator);
}

void RTSUnitGroupMiniMapMarker::RemoveIcons(const UObject* WorldContext, const FName IconId)
{
	AFowManager* FowManager = FRTS_Statics::GetFowManager(WorldContext);
	if (not IsValid(FowManager))
	{
		return;
	}
	FowManager->RemoveCustomMiniMapIcon(IconId);
	FowManager->RemoveCustomMiniMapIcon(GetBadgeIconId(IconId));
}

FName RTSUnitGroupMiniMapMarker::GetBadgeIconId(const FName IconId)
{
	return FName(*(IconId.ToString() + TEXT("_Badge")));
}

bool RTSUnitGroupMiniMapMarker::UpdateFromUnits(TArray<TWeakObjectPtr<AActor>>& Units, FVector& Location,
                                                FVector& HeadingAnchor, float& HeadingYaw)
{
	using namespace UnitGroupMiniMapMarkerConstants;
	Units.RemoveAll([](const TWeakObjectPtr<AActor>& Unit)
	{
		return not RTSFunctionLibrary::RTSIsValid(Unit.Get());
	});
	if (Units.IsEmpty())
	{
		return false;
	}
	FVector Centroid = FVector::ZeroVector;
	for (const TWeakObjectPtr<AActor>& Unit : Units)
	{
		const ICommands* Commands = Cast<ICommands>(Unit.Get());
		Centroid += Commands ? Commands->GetOwnerLocation() : Unit->GetActorLocation();
	}
	Centroid /= Units.Num();
	const FVector Progress = Centroid - HeadingAnchor;
	if (Progress.Size2D() >= MinMoveForHeading)
	{
		const float MoveYaw = Progress.GetSafeNormal2D().Rotation().Yaw;
		HeadingYaw = FRotator::NormalizeAxis(HeadingYaw + FMath::FindDeltaAngleDegrees(HeadingYaw, MoveYaw) *
			HeadingBlend);
		HeadingAnchor = Centroid;
	}
	Location = Centroid;
	return true;
}
