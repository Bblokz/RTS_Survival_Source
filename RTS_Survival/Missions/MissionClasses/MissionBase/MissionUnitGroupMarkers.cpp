// Copyright (C) Bas Blokzijl - All rights reserved.

// The unit-group minimap markers of UMissionBase: survival-style chevron + badge per group until its last unit is gone.

#include "MissionBase.h"

#include "RTS_Survival/GameUI/MiniMap/CustomIcons/UnitGroupMiniMapMarker.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"

void UMissionBase::TrackUnitGroupOnMiniMap(const FName GroupId, const TArray<AActor*>& Units, const FVector HeadingTarget)
{
	UnitGroupMarkers_RemoveGroup(GroupId);
	FMissionUnitGroupMarker Marker;
	Marker.GroupId = GroupId;
	Marker.IconId = FName(*FString::Printf(TEXT("MissionUnitGroup_%d"), M_NextUnitGroupMarkerIndex++));
	for (AActor* Unit : Units)
	{
		if (RTSFunctionLibrary::RTSIsValid(Unit))
		{
			Marker.Units.Add(Unit);
		}
	}
	// With no living unit the group stays without icons and is reported defeated on the next update.
	if (RTSUnitGroupMiniMapMarker::UpdateFromUnits(Marker.Units, Marker.Location, Marker.HeadingAnchor,
	                                               Marker.HeadingYaw))
	{
		Marker.HeadingAnchor = Marker.Location;
		Marker.HeadingYaw = (HeadingTarget - Marker.Location).GetSafeNormal2D().Rotation().Yaw;
		RTSUnitGroupMiniMapMarker::AddIcons(this, Marker.IconId, Marker.Location, Marker.HeadingYaw);
	}
	M_UnitGroupMarkers.Add(Marker);
	UnitGroupMarkers_EnsureUpdateTimer();
}

FVector UMissionBase::GetUnitGroupMarkerLocation(const FName GroupId) const
{
	const FMissionUnitGroupMarker* Marker = M_UnitGroupMarkers.FindByPredicate(
		[GroupId](const FMissionUnitGroupMarker& Each)
		{
			return Each.GroupId == GroupId;
		});
	return Marker ? Marker->Location : FVector::ZeroVector;
}

void UMissionBase::UnitGroupMarkers_EnsureUpdateTimer()
{
	UWorld* World = GetWorld();
	if (not World or World->GetTimerManager().IsTimerActive(M_UnitGroupMarkerTimerHandle))
	{
		return;
	}
	TWeakObjectPtr<UMissionBase> WeakThis(this);
	World->GetTimerManager().SetTimer(M_UnitGroupMarkerTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->UnitGroupMarkers_Update();
		}
	}), RTSUnitGroupMiniMapMarker::UpdateIntervalSeconds, true);
}

void UMissionBase::UnitGroupMarkers_Update()
{
	TArray<FName> DefeatedGroups;
	for (int32 Index = M_UnitGroupMarkers.Num() - 1; Index >= 0; --Index)
	{
		FMissionUnitGroupMarker& Marker = M_UnitGroupMarkers[Index];
		if (RTSUnitGroupMiniMapMarker::UpdateFromUnits(Marker.Units, Marker.Location, Marker.HeadingAnchor,
		                                               Marker.HeadingYaw))
		{
			RTSUnitGroupMiniMapMarker::MoveIcons(this, Marker.IconId, Marker.Location, Marker.HeadingYaw);
			continue;
		}
		RTSUnitGroupMiniMapMarker::RemoveIcons(this, Marker.IconId);
		DefeatedGroups.Add(Marker.GroupId);
		M_UnitGroupMarkers.RemoveAt(Index);
	}
	if (M_UnitGroupMarkers.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_UnitGroupMarkerTimerHandle);
		}
	}
	// Reported after the loop: a handler may complete the mission (clearing the markers) or track a new group.
	for (const FName& GroupId : DefeatedGroups)
	{
		BP_OnUnitGroupDefeated(GroupId);
	}
}

void UMissionBase::UnitGroupMarkers_RemoveGroup(const FName GroupId)
{
	for (int32 Index = M_UnitGroupMarkers.Num() - 1; Index >= 0; --Index)
	{
		if (M_UnitGroupMarkers[Index].GroupId != GroupId)
		{
			continue;
		}
		RTSUnitGroupMiniMapMarker::RemoveIcons(this, M_UnitGroupMarkers[Index].IconId);
		M_UnitGroupMarkers.RemoveAt(Index);
	}
}

void UMissionBase::UnitGroupMarkers_Clear()
{
	for (const FMissionUnitGroupMarker& Marker : M_UnitGroupMarkers)
	{
		RTSUnitGroupMiniMapMarker::RemoveIcons(this, Marker.IconId);
	}
	M_UnitGroupMarkers.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_UnitGroupMarkerTimerHandle);
	}
}
