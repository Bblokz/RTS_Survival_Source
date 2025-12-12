#include "ExplosionManagerSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionManager.h"

class ACPPGameState;

void UExplosionManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

}

void UExplosionManagerSubsystem::Deinitialize()
{
    ExplosionManager = nullptr;

    Super::Deinitialize();
}

void UExplosionManagerSubsystem::SetExplosionManager(UGameExplosionsManager* NewExplosionManager)
{
    if(not IsValid(NewExplosionManager))
    {
        RTSFunctionLibrary::ReportError("Invalid Explosion Manager provided for ExplosionManagerSubsystem.");
        return;
    }
    ExplosionManager = NewExplosionManager;
}
