// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MissionTrigger.generated.h"

class UMissionBase;
class AMissionManager;

UENUM(BlueprintType)
enum class EMissionTriggerType : uint8
{
    None        UMETA(DisplayName = "None"),
    KillActors  UMETA(DisplayName = "Kill Actors"),
    MissionClassCompleted UMETA(DisplayName = "Mission Class Completed"),
    // A player unit came near a place (e.g. UForgeStationSightedTrigger).
    PlayerUnitsNear UMETA(DisplayName = "Player Units Near"),
    // An enemy unit started to flee to its rally point (UEnemyFleeingMissionTrigger, UEnemyFallbackSubsystem).
    EnemyFleeing UMETA(DisplayName = "Enemy Fleeing"),
    // The game has run for a number of seconds (UTimeElapsedMissionTrigger).
    TimeElapsed UMETA(DisplayName = "Time Elapsed")
};

UCLASS(Abstract, Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UMissionTrigger : public UObject
{
    GENERATED_BODY()

public:
    // Returns the type of the trigger. By default, it is None.
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mission Trigger")
    virtual EMissionTriggerType GetTriggerType() const { return EMissionTriggerType::None; }

    void InitTrigger(UMissionBase* AssociatedMission);

    void TickTrigger();

protected:
    // Check if the trigger condition is met.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mission Trigger")
    bool CheckTrigger() const;
    virtual bool CheckTrigger_Implementation() const { return false; }

    AMissionManager* GetMissionManagerCheckedFromTrigger() const;

private:
    UPROPERTY()
    TWeakObjectPtr<UMissionBase> M_Mission;

    bool GetIsValidMission() const;
};
