#include "KillActorsMissionTrigger.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

bool UKillActorsMissionTrigger::CheckTrigger_Implementation() const
{
    // If any actor is still valid, the trigger condition is not yet met.
    for (AActor* Actor : ActorsToKill)
    {
        if (RTSFunctionLibrary::RTSIsValid(Actor))
        {
            return false;
        }
    }
    return true;
}
