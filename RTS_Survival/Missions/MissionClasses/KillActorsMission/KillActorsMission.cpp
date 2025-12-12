#include "KillActorsMission.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UKillActorsMission::UKillActorsMission()
{
	MissionState.bTickOnMission = true;
}

void UKillActorsMission::TickMission(float DeltaTime)
{
	if (MissionState.bIsMissionComplete)
	{
		return;
	}

	bool bAllDestroyed = true;
	for (AActor* Target : TargetActors)
	{
		// Check if unit is valid and not dying.
		if (RTSFunctionLibrary::RTSIsValid(Target))
		{
			bAllDestroyed = false;
			break;
		}
	}

	if (bAllDestroyed)
	{
		OnMissionComplete();
	}
}

void UKillActorsMission::OnMissionComplete()
{
	DebugMission("All actors killed. Mission complete.");
	Super::OnMissionComplete();
	
}

