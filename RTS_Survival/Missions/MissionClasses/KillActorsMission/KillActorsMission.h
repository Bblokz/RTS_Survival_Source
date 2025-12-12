#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "KillActorsMission.generated.h"

/**
 * A mission that completes when a list of actors have been destroyed.
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UKillActorsMission : public UMissionBase
{
	GENERATED_BODY()

public:
	UKillActorsMission();

	/** List of target actors to be destroyed for mission completion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
	TArray<AActor*> TargetActors;
	

	/** Check mission status each tick. */
	virtual void TickMission(float DeltaTime) override;

	virtual void OnMissionComplete() override;
};
