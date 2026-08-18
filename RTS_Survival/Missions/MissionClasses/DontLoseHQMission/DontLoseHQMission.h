#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "DontLoseHQMission.generated.h"

class ANomadicVehicle;

/**
 * @brief Tracks the player's HQ so its destruction can focus the camera and end the mission.
 * It retries HQ lookup during startup because profile-driven spawning may finish after mission loading.
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UDontLoseHQMission : public UMissionBase
{
	GENERATED_BODY()

protected:
	virtual void OnMissionStart() override;
	virtual void OnMissionComplete() override;
	virtual void OnMissionFailed() override;

private:
	void ClearMissionTimers();
	void TryFindAndBindPlayerHQ();
	void HandleNoPlayerHQFound();
	void BindToPlayerHQDeath();
	void HandlePlayerHQDied();
	void StartHQLossCameraPan();
	void TriggerHQLossAnnouncerVoiceLine() const;
	void ScheduleMissionDefeat();

	UFUNCTION()
	void HandleRetryFindPlayerHQ();

	UFUNCTION()
	void HandleTriggerDefeatDelayed();

	bool GetIsValidPlayerHQ() const;

private:
	// Used to track the player HQ lifetime without owning it.
	UPROPERTY()
	TWeakObjectPtr<ANomadicVehicle> M_PlayerHQ;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|HQ", meta=(AllowPrivateAccess = "true"))
	float M_CameraPanTimeToHQDeathLocation = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|HQ", meta=(AllowPrivateAccess = "true"))
	float M_DefeatDelayAfterHQLost = 5.0f;

	UPROPERTY()
	FVector M_LastKnownHQPosition = FVector::ZeroVector;

	UPROPERTY()
	FTimerHandle M_RetryFindHQHandle;

	UPROPERTY()
	FTimerHandle M_DefeatAfterHQLostHandle;

	int32 M_HQFindAttempts = 0;
	bool bM_HasTriggeredHQLossFlow = false;
};
