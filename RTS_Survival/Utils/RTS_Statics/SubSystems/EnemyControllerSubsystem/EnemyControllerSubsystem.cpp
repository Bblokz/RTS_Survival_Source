// EnemyControllerSubsystem.cpp

#include "EnemyControllerSubsystem.h"
#include "Engine/World.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UEnemyControllerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // Ensure every player slot starts empty
    M_EnemyControllersByPlayer.Reset();
    EnsurePlayerSlots();
}

void UEnemyControllerSubsystem::Deinitialize()
{
    // Clear cache on world teardown
    M_EnemyControllersByPlayer.Reset();
    Super::Deinitialize();
}

AEnemyController* UEnemyControllerSubsystem::GetEnemyController() const
{
    if (AEnemyController* FirstAIPlayerController = GetEnemyControllerForPlayer(RTSPlayers::FirstAIPlayer))
    {
        return FirstAIPlayerController;
    }
    // A map whose only controller plays a later AI player still serves the callers that ask for "the" controller.
    for (int32 Player = RTSPlayers::FirstAIPlayer + 1; Player <= RTSPlayers::MaxPlayers; ++Player)
    {
        if (AEnemyController* EnemyController = GetEnemyControllerForPlayer(Player))
        {
            return EnemyController;
        }
    }
    return nullptr;
}

AEnemyController* UEnemyControllerSubsystem::GetEnemyControllerForPlayer(const int32 Player) const
{
    if (not FRTSPlayers::IsValidAIPlayer(Player) || not M_EnemyControllersByPlayer.IsValidIndex(Player))
    {
        return nullptr;
    }
    return M_EnemyControllersByPlayer[Player].Get();
}

TArray<AEnemyController*> UEnemyControllerSubsystem::GetAllEnemyControllers() const
{
    TArray<AEnemyController*> EnemyControllers;
    for (int32 Player = RTSPlayers::FirstAIPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
    {
        if (AEnemyController* EnemyController = GetEnemyControllerForPlayer(Player))
        {
            EnemyControllers.Add(EnemyController);
        }
    }
    return EnemyControllers;
}

void UEnemyControllerSubsystem::SetEnemyController(AEnemyController* NewEnemyController)
{
    if (!IsValid(NewEnemyController))
    {
        RTSFunctionLibrary::ReportError(TEXT("Invalid Enemy Controller provided to EnemyControllerSubsystem."));
        return;
    }
    const int32 ControlledPlayer = NewEnemyController->GetControlledPlayer();
    if (not FRTSPlayers::IsValidAIPlayer(ControlledPlayer))
    {
        RTSFunctionLibrary::ReportError(
            "Enemy controller " + NewEnemyController->GetName() + " plays player " +
            FString::FromInt(ControlledPlayer) + ", which is no AI player; it is not registered.");
        return;
    }
    EnsurePlayerSlots();
    const AEnemyController* RegisteredController = M_EnemyControllersByPlayer[ControlledPlayer].Get();
    if (RegisteredController == NewEnemyController)
    {
        return;
    }
    if (IsValid(RegisteredController))
    {
        RTSFunctionLibrary::ReportError(
            "Two enemy controllers play player " + FString::FromInt(ControlledPlayer) + ": " +
            RegisteredController->GetName() + " stays registered, " + NewEnemyController->GetName() +
            " is ignored. Give each controller in the map its own ControlledPlayer.");
        return;
    }
    M_EnemyControllersByPlayer[ControlledPlayer] = NewEnemyController;
}

void UEnemyControllerSubsystem::EnsurePlayerSlots()
{
    if (M_EnemyControllersByPlayer.Num() < RTSPlayers::PlayerArraySize)
    {
        M_EnemyControllersByPlayer.SetNum(RTSPlayers::PlayerArraySize);
    }
}
