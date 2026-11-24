// Copyright (C) Bas Blokzijl - All rights reserved.

#include "DestroyEnemyItemSiteMission.h"

#include "EngineUtils.h"
#include "RTS_Survival/Enemy/ItemSites/EnemyItemSiteDirector.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace DestroyEnemyItemSiteMissionConstants
{
	// Backup poll of the mission widget tracking; the win itself is decided every tick.
	constexpr float TrackingValidityCheckSeconds = 1.f;
}

UDestroyEnemyItemSiteMission::UDestroyEnemyItemSiteMission()
{
	MissionState.bTickOnMission = true;
	// The tracking only shows progress: a dying building is already dead here, before its actor is destroyed.
	bCompleteMissionWhenTrackingDone = false;
}

void UDestroyEnemyItemSiteMission::OnMissionStartedNative()
{
	Super::OnMissionStartedNative();
	(void)TryFindSiteDirector();
}

void UDestroyEnemyItemSiteMission::TickMission(const float DeltaTime)
{
	Super::TickMission(DeltaTime);
	if (MissionState.bIsMissionComplete || not TryFindSiteDirector())
	{
		return;
	}

	const AEnemyItemSiteDirector* SiteDirector = M_SiteDirector.Get();
	const EEnemyItemSiteObjectiveState ObjectiveState = EnemyItemSiteRules::GetObjectiveState(
		SiteDirector->GetIsSiteBuilt(), SiteDirector->GetSpawnedVictoryTargetCount(),
		SiteDirector->GetVictoryTargets().Num());
	switch (ObjectiveState)
	{
	case EEnemyItemSiteObjectiveState::NoTargets:
		ReportSiteWithoutTargets(*SiteDirector);
		break;
	case EEnemyItemSiteObjectiveState::InProgress:
		StartTrackingTargets(*SiteDirector);
		break;
	case EEnemyItemSiteObjectiveState::AllTargetsDestroyed:
		OnMissionComplete();
		break;
	default:
		break;
	}
}

bool UDestroyEnemyItemSiteMission::TryFindSiteDirector()
{
	if (M_SiteDirector.IsValid())
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}

	TActorIterator<AEnemyItemSiteDirector> DirectorIterator(World);
	if (DirectorIterator)
	{
		M_SiteDirector = *DirectorIterator;
		return true;
	}

	if (not bM_HasReportedNoDirector)
	{
		bM_HasReportedNoDirector = true;
		RTSFunctionLibrary::ReportError(
			"DestroyEnemyItemSiteMission " + GetName() + " found no AEnemyItemSiteDirector in the level; "
			"it cannot complete.");
	}

	return false;
}

void UDestroyEnemyItemSiteMission::StartTrackingTargets(const AEnemyItemSiteDirector& SiteDirector)
{
	if (bM_HasStartedTracking || not bTrackTargets)
	{
		return;
	}

	bM_HasStartedTracking = true;
	TrackActorsWithMiniMapProgress(EMissionTrackingType::TrackOnActorsDestroyed, SiteDirector.GetVictoryTargets(),
	                               DestroyEnemyItemSiteMissionConstants::TrackingValidityCheckSeconds,
	                               TrackingPrefix, TEXT("/"), FString(), ERTSRichText::Text_SubTitle);
}

void UDestroyEnemyItemSiteMission::ReportSiteWithoutTargets(const AEnemyItemSiteDirector& SiteDirector)
{
	if (bM_HasReportedNoTargets)
	{
		return;
	}

	bM_HasReportedNoTargets = true;
	const FString Remedy = SiteDirector.GetSiteContext().bIsDivisionOperation
		                       ? TEXT("The division army spawned no unit; see the earlier division army errors.")
		                       : TEXT("Give the map signature slots with a victory target for this item type.");
	RTSFunctionLibrary::ReportError(
		"DestroyEnemyItemSiteMission " + GetName() + ": the site spawned no victory target, so this mission cannot "
		"complete. " + Remedy);
}

void UDestroyEnemyItemSiteMission::OnMissionComplete()
{
	if (MissionState.bIsMissionComplete)
	{
		return;
	}

	Super::OnMissionComplete();
	if (bTriggerVictoryOnComplete)
	{
		ScheduleVictory();
	}
}

void UDestroyEnemyItemSiteMission::ScheduleVictory()
{
	AMissionManager* MissionManager = GetMissionManagerChecked();
	UWorld* World = GetWorld();
	if (not IsValid(MissionManager))
	{
		return;
	}

	if (not IsValid(World) || VictoryDelaySeconds <= 0.f)
	{
		MissionManager->TriggerVictory(false);
		return;
	}

	// The timer holds the manager, not this mission: a completed mission may be released before it fires.
	TWeakObjectPtr<AMissionManager> WeakMissionManager(MissionManager);
	FTimerHandle VictoryDelayTimerHandle;
	World->GetTimerManager().SetTimer(VictoryDelayTimerHandle, FTimerDelegate::CreateLambda([WeakMissionManager]()
	{
		if (WeakMissionManager.IsValid())
		{
			WeakMissionManager->TriggerVictory(false);
		}
	}), VictoryDelaySeconds, false);
}
