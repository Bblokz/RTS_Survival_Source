// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MissionTrigger.generated.h"

class UMissionBase;

UENUM(BlueprintType)
enum class EMissionTriggerType : uint8
{
    None        UMETA(DisplayName = "None"),
    KillActors  UMETA(DisplayName = "Kill Actors")
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

private:
    TWeakObjectPtr<UMissionBase> M_Mission;

    bool EnsureMissionIsValid() const;
};
