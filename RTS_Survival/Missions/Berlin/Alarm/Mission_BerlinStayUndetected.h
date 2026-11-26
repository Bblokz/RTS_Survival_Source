// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/Portrait/RTSPortraitTypes.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"

#include "Mission_BerlinStayUndetected.generated.h"

class USoundBase;

/** What raised the Berlin alarm; picks the Soviet radio line and where the patrols go. */
UENUM(BlueprintType)
enum class EBerlinAlarmCause : uint8
{
	None,
	// A player unit entered a detection zone.
	Spotted,
	// A fleeing guard reached the Reichstag forecourt.
	RunnerArrived,
	// Player infantry entered the relay bunker at the Zoo tower.
	RelayBunker
};

/** Soviet rifleman radio (Russian, English subtitles) that plays once when the alarm goes off, one line per cause. */
USTRUCT(BlueprintType)
struct FBerlinAlarmLines
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Alarm Lines")
	ERTSPortraitTypes Portrait = ERTSPortraitTypes::SovietMosin;

	// Berlin_Alarm_Spotted.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alarm Lines")
	TObjectPtr<USoundBase> SpottedLine = nullptr;

	// Berlin_Alarm_RunnerArrived.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alarm Lines")
	TObjectPtr<USoundBase> RunnerArrivedLine = nullptr;

	// Berlin_Alarm_RelayBunker.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alarm Lines")
	TObjectPtr<USoundBase> RelayBunkerLine = nullptr;
};

/**
 * @brief C++ parent of Mission_Berlin_StayUndetected (the stage whose failure is the alarm). The Blueprint keeps its
 * graph; natively this class remembers the last alarm trigger (cause and where the player was seen) and, when the
 * Blueprint fails the stage, plays the cause's Soviet radio line and sends every patrol to attack the player HQ
 * (AEnemyWaveSpawner::ConvergeAllPatrolsOnAlarm). Fights in the first PatrolCombatCheckDelaySeconds do not count; after
 * that a group returns to its route PatrolCombatSecondsBeforeReturn after its first fight.
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UMission_BerlinStayUndetected : public UMissionBase
{
	GENERATED_BODY()

public:
	virtual void OnMissionFailed() override;

	EBerlinAlarmCause GetAlarmCause() const { return M_AlarmCause; }
	bool GetHasRespondedToAlarm() const { return bM_HasRespondedToAlarm; }

protected:
	virtual void OnTriggerAreaCallback(AActor* OverlappingActor, const int32 TriggerID,
	                                   ATriggerArea* TriggerVolume) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alarm")
	FBerlinAlarmLines AlarmLines;

	// True: the patrols attack-move to the player HQ. False (or no HQ): to where the alarm unit was seen.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alarm")
	bool bPatrolsAttackPlayerHQ = true;

	// Seconds after a patrol group first fights before it breaks off and returns to its route.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alarm", meta = (ClampMin = "0.0"))
	float PatrolCombatSecondsBeforeReturn = 20.f;

	// Seconds after the alarm before fights count for the return above (owner 2026-11-25: by then the patrols are
	// near the player); fights on the way to the HQ before that do not turn a group back.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alarm", meta = (ClampMin = "0.0"))
	float PatrolCombatCheckDelaySeconds = 80.f;

	// Trigger ids the Blueprint gives its areas (BerlinMissionScripting.md, StayUndetected).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alarm")
	int32 AlarmRelayBunkerTriggerId = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alarm")
	int32 AlarmForecourtTriggerId = 2;

	// Every detection zone uses this id or higher.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alarm")
	int32 AlarmFirstZoneTriggerId = 100;

private:
	// The last trigger that can raise the alarm; the forecourt raises it only after a grace wait, so a later zone
	// entry in that window wins (rare; the line then names the zone).
	EBerlinAlarmCause M_AlarmCause = EBerlinAlarmCause::None;
	FVector M_AlarmLocation = FVector::ZeroVector;
	bool bM_HasRespondedToAlarm = false;

	void RecordAlarmCandidate(const AActor* OverlappingActor, const int32 TriggerID);
	USoundBase* GetLineForCause(const EBerlinAlarmCause Cause) const;
	void RespondToAlarm();

	/** @return The location of the player unit closest to the runner, or the runner's own location if none. */
	FVector GetClosestPlayerUnitLocation(const FVector& From) const;

	/** @return Where the patrols attack-move to: the player HQ, or the alarm spot without an HQ. */
	FVector GetPatrolTargetLocation() const;
};
