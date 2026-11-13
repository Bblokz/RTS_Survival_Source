// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalWaveMarkerTracker.h"

#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Enemy/Formation/FormationData.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/UnitGroupMiniMapMarker.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"

namespace SurvivalWaveMarkerConstants
{
	// Same tolerance the wave director uses to recognise a group's formation by its last waypoint.
	constexpr float FinalWaypointMatchTolerance = 1.f;
	constexpr float DebugKillDamage = 1.0e8f;
}

void USurvivalWaveMarkerTracker::InitTracker(AEnemyController* EnemyController, const float FormationBindTimeoutSeconds)
{
	M_EnemyController = EnemyController;
	M_FormationBindTimeoutSeconds = FormationBindTimeoutSeconds;
}

void USurvivalWaveMarkerTracker::OnGroupLaunched(const FName GroupKey, const FVector& ExpectedFinalWaypoint,
                                                 const TArray<FVector>& SpawnPoints, const FVector& BaseLocation)
{
	if (SpawnPoints.IsEmpty() or not GetWorld())
	{
		return;
	}
	if (FSurvivalWaveMarker* SameGroup = M_Markers.FindByPredicate([GroupKey](const FSurvivalWaveMarker& Each)
	{
		return Each.GroupKey == GroupKey;
	}))
	{
		SameGroup->PendingFinalWaypoints.Add(ExpectedFinalWaypoint);
		return;
	}
	FSurvivalWaveMarker Marker;
	Marker.IconId = FName(*FString::Printf(TEXT("SurvivalWave_%d"), M_NextMarkerIndex++));
	Marker.GroupKey = GroupKey;
	Marker.PendingFinalWaypoints.Add(ExpectedFinalWaypoint);
	for (const FVector& SpawnPoint : SpawnPoints)
	{
		Marker.Location += SpawnPoint;
	}
	Marker.Location /= SpawnPoints.Num();
	Marker.HeadingYaw = (BaseLocation - Marker.Location).GetSafeNormal2D().Rotation().Yaw;
	Marker.HeadingAnchor = Marker.Location;
	Marker.LaunchTime = GetWorld()->GetTimeSeconds();
	AddMarkerIcons(Marker);
	M_Markers.Add(Marker);
	UE_LOG(LogTemp, Log, TEXT("Survival wave marker %s added at %s heading %.0f."), *Marker.IconId.ToString(),
	       *Marker.Location.ToString(), Marker.HeadingYaw);
	EnsureUpdateTimer();
}

void USurvivalWaveMarkerTracker::StopTracking()
{
	for (const FSurvivalWaveMarker& Marker : M_Markers)
	{
		RemoveMarkerIcons(Marker);
	}
	M_Markers.Reset();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_UpdateTimerHandle);
	}
}

void USurvivalWaveMarkerTracker::EnsureUpdateTimer()
{
	UWorld* World = GetWorld();
	if (not World or World->GetTimerManager().IsTimerActive(M_UpdateTimerHandle))
	{
		return;
	}
	World->GetTimerManager().SetTimer(M_UpdateTimerHandle, this, &USurvivalWaveMarkerTracker::UpdateMarkers,
	                                  RTSUnitGroupMiniMapMarker::UpdateIntervalSeconds, true);
}

void USurvivalWaveMarkerTracker::UpdateMarkers()
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	BindMarkersToFormations();
	const double Now = World->GetTimeSeconds();
	for (int32 Index = M_Markers.Num() - 1; Index >= 0; --Index)
	{
		if (UpdateMarker(M_Markers[Index], Now))
		{
			continue;
		}
		UE_LOG(LogTemp, Log, TEXT("Survival wave marker %s removed (%s)."), *M_Markers[Index].IconId.ToString(),
		       M_Markers[Index].bHasBoundUnits ? TEXT("all units gone") : TEXT("formation never appeared"));
		RemoveMarkerIcons(M_Markers[Index]);
		M_Markers.RemoveAt(Index);
	}
	if (M_Markers.IsEmpty())
	{
		World->GetTimerManager().ClearTimer(M_UpdateTimerHandle);
	}
}

void USurvivalWaveMarkerTracker::BindMarkersToFormations()
{
	const bool bHasUnboundMarker = M_Markers.ContainsByPredicate([](const FSurvivalWaveMarker& Marker)
	{
		return not Marker.PendingFinalWaypoints.IsEmpty();
	});
	if (not bHasUnboundMarker or not GetIsValidEnemyController())
	{
		return;
	}
	const UEnemyFormationController* FormationController = M_EnemyController->GetEnemyFormationController();
	if (not IsValid(FormationController))
	{
		return;
	}
	TArray<FFormationData> Formations;
	FormationController->GetActiveFormationData(Formations);
	for (FSurvivalWaveMarker& Marker : M_Markers)
	{
		BindMarkerToFormations(Marker, Formations);
	}
}

void USurvivalWaveMarkerTracker::BindMarkerToFormations(FSurvivalWaveMarker& Marker,
                                                        const TArray<FFormationData>& Formations) const
{
	for (int32 PendingIndex = Marker.PendingFinalWaypoints.Num() - 1; PendingIndex >= 0; --PendingIndex)
	{
		const FVector& FinalWaypoint = Marker.PendingFinalWaypoints[PendingIndex];
		const FFormationData* Formation = Formations.FindByPredicate([&FinalWaypoint](const FFormationData& Each)
		{
			return not Each.FormationWaypoints.IsEmpty() and Each.FormationWaypoints.Last().Equals(
				FinalWaypoint, SurvivalWaveMarkerConstants::FinalWaypointMatchTolerance);
		});
		if (not Formation)
		{
			continue;
		}
		for (const FFormationUnitData& FormationUnit : Formation->FormationUnits)
		{
			AActor* UnitActor = FormationUnit.IsValidFormationUnit() ? FormationUnit.Unit->GetOwnerActor() : nullptr;
			if (RTSFunctionLibrary::RTSIsValid(UnitActor))
			{
				Marker.Units.AddUnique(UnitActor);
			}
		}
		Marker.PendingFinalWaypoints.RemoveAt(PendingIndex);
		Marker.bHasBoundUnits = not Marker.Units.IsEmpty();
		UE_LOG(LogTemp, Log, TEXT("Survival wave marker %s follows %d units."), *Marker.IconId.ToString(),
		       Marker.Units.Num());
	}
}

bool USurvivalWaveMarkerTracker::UpdateMarker(FSurvivalWaveMarker& Marker, const double Now) const
{
	if (not RTSUnitGroupMiniMapMarker::UpdateFromUnits(Marker.Units, Marker.Location, Marker.HeadingAnchor,
	                                                   Marker.HeadingYaw))
	{
		// Still waiting for (another) formation of the group to appear: keep the marker where it is.
		const bool bWaitingForFormation = not Marker.PendingFinalWaypoints.IsEmpty();
		return bWaitingForFormation and Now - Marker.LaunchTime < M_FormationBindTimeoutSeconds;
	}
	RTSUnitGroupMiniMapMarker::MoveIcons(this, Marker.IconId, Marker.Location, Marker.HeadingYaw);
	return true;
}

void USurvivalWaveMarkerTracker::AddMarkerIcons(const FSurvivalWaveMarker& Marker) const
{
	RTSUnitGroupMiniMapMarker::AddIcons(this, Marker.IconId, Marker.Location, Marker.HeadingYaw);
}

void USurvivalWaveMarkerTracker::RemoveMarkerIcons(const FSurvivalWaveMarker& Marker) const
{
	RTSUnitGroupMiniMapMarker::RemoveIcons(this, Marker.IconId);
}

bool USurvivalWaveMarkerTracker::GetIsValidEnemyController() const
{
	if (M_EnemyController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_EnemyController",
	                                                             "GetIsValidEnemyController", this);
	return false;
}

#if RTS_WITH_MAP_TESTS
int32 USurvivalWaveMarkerTracker::DebugKillMarkerUnits(const FName IconId) const
{
	const FSurvivalWaveMarker* Marker = M_Markers.FindByPredicate([IconId](const FSurvivalWaveMarker& Each)
	{
		return Each.IconId == IconId;
	});
	if (not Marker)
	{
		return 0;
	}
	int32 Hit = 0;
	const FDamageEvent DamageEvent;
	for (const TWeakObjectPtr<AActor>& Unit : Marker->Units)
	{
		ASquadController* Squad = Cast<ASquadController>(Unit.Get());
		if (not Squad)
		{
			if (Unit.IsValid())
			{
				Unit->TakeDamage(SurvivalWaveMarkerConstants::DebugKillDamage, DamageEvent, nullptr, nullptr);
				++Hit;
			}
			continue;
		}
		for (ASquadUnit* SquadUnit : Squad->GetSquadUnitsChecked())
		{
			SquadUnit->TakeDamage(SurvivalWaveMarkerConstants::DebugKillDamage, DamageEvent, nullptr, nullptr);
			++Hit;
		}
	}
	return Hit;
}
#endif
