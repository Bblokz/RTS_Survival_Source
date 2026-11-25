// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/ItemSites/EnemyItemSiteTypes.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "DestroyEnemyItemSiteMission.generated.h"

class AEnemyItemSiteDirector;

/**
 * @brief The win condition of a handmade enemy map: add it to the map's Missions. It completes when the
 * AEnemyItemSiteDirector's victory targets are dead (signature buildings, division army, convoy trucks) and calls
 * TriggerVictory; it fails with TriggerDefeat when a convoy truck escapes. See EnemyItemMaps_Plan.md (D4, 5e).
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UDestroyEnemyItemSiteMission : public UMissionBase
{
	GENERATED_BODY()

public:
	UDestroyEnemyItemSiteMission();

	virtual void TickMission(float DeltaTime) override;
	virtual void OnMissionComplete() override;

	// Off for a mission Blueprint that wants to play lines first and call TriggerVictory itself.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Enemy Item Site")
	bool bTriggerVictoryOnComplete = true;

	// Off for a mission Blueprint that wants to react to an escaped convoy itself (the mission still fails).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Enemy Item Site")
	bool bTriggerDefeatOnEnemyEscape = true;

	// Time between the last target dying and the victory screen, so the last explosion is seen.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Enemy Item Site", meta = (ClampMin = "0.0"))
	float VictoryDelaySeconds = 3.f;

	// Shows "<prefix>destroyed/total" in the mission widget and the count on each target on the minimap.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Enemy Item Site")
	bool bTrackTargets = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Enemy Item Site",
		meta = (EditCondition = "bTrackTargets"))
	FString TrackingPrefix;

protected:
	virtual void OnMissionStartedNative() override;

private:
	UPROPERTY()
	TWeakObjectPtr<AEnemyItemSiteDirector> M_SiteDirector;

	bool bM_HasStartedTracking = false;
	bool bM_HasReportedNoTargets = false;
	bool bM_HasReportedNoDirector = false;

	bool TryFindSiteDirector();
	void StartTrackingTargets(const AEnemyItemSiteDirector& SiteDirector);
	void ReportSiteWithoutTargets(const AEnemyItemSiteDirector& SiteDirector);
	void ScheduleVictory();
	void FailOnEnemyEscape();
};
