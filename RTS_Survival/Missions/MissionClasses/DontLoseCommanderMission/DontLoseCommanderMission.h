#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "DontLoseCommanderMission.generated.h"

class ATankMaster;

/**
 * @brief Mission that tracks the player's command vehicle and triggers defeat if it is destroyed.
 * It retries commander lookup on mission start and handles camera focus + announcer feedback on loss.
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UDontLoseCommanderMission : public UMissionBase
{
	GENERATED_BODY()

protected:
	virtual void OnMissionStart() override;
	virtual void OnMissionComplete() override;
	virtual void OnMissionFailed() override;

private:
	void ClearMissionTimers();
	void TryFindAndBindPlayerCommandVehicle();
	void HandleNoCommandVehicleFound();
	void BindToCommandVehicleDeath();
	void HandleCommandVehicleDied();
	void StartCommanderLossCameraPan();
	void TriggerCommanderLossAnnouncerVoiceLine() const;
	void ScheduleMissionDefeat();

	UFUNCTION()
	void HandleRetryFindCommandVehicle();

	UFUNCTION()
	void HandleTriggerDefeatDelayed();

	bool GetIsValidCommandVehicle() const;

private:
	// Used to track the command vehicle lifetime without owning it.
	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_CommandVehicle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Commander", meta=(AllowPrivateAccess = "true"))
	float M_CameraPanTimeToCommanderDeathLocation = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Commander", meta=(AllowPrivateAccess = "true"))
	float M_DefeatDelayAfterCommanderLost = 2.0f;

	UPROPERTY()
	FVector M_LastKnownCommanderVehiclePosition = FVector::ZeroVector;

	UPROPERTY()
	FTimerHandle M_RetryFindCommanderHandle;

	UPROPERTY()
	FTimerHandle M_DefeatAfterCommanderLostHandle;

	int32 M_CommandVehicleFindAttempts = 0;
	bool bM_HasTriggeredCommanderLossFlow = false;
};
