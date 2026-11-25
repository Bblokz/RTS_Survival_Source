// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/ItemSites/EnemyItemSiteTypes.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "DestroyEnemyItemSiteMission.generated.h"

class AEnemyItemSiteDirector;

/**
 * @brief The win condition of a handmade enemy map: add it to the map's Missions; it completes when every victory
 * target the AEnemyItemSiteDirector spawned is dead (the signature buildings of an item map, the division army of a
 * field map) and then calls TriggerVictory, which reports the win to the world map. See EnemyItemMaps_Plan.md (D4).
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
};
