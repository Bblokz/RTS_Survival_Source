// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "RTS_Survival/Behaviours/Icons/BehaviourIcon.h"

#include "BehaviourUIData.generated.h"

USTRUCT()
struct FBehaviourUIData
{
GENERATED_BODY()

UPROPERTY()
EBehaviourIcon BehaviourIcon = EBehaviourIcon::None;

UPROPERTY()
FString DisplayText;
};
