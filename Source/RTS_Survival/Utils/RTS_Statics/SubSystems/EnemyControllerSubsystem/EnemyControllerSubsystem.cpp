// EnemyControllerSubsystem.cpp

#include "EnemyControllerSubsystem.h"
#include "Engine/World.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UEnemyControllerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // Ensure pointer starts null
    M_EnemyController = nullptr;
}

void UEnemyControllerSubsystem::Deinitialize()
{
    // Clear cache on world teardown
    M_EnemyController = nullptr;
    Super::Deinitialize();
}

void UEnemyControllerSubsystem::SetEnemyController(AEnemyController* NewEnemyController)
{
    if (!IsValid(NewEnemyController))
    {
        RTSFunctionLibrary::ReportError(TEXT("Invalid Enemy Controller provided to EnemyControllerSubsystem."));
        return;
    }
    M_EnemyController = NewEnemyController;
}
