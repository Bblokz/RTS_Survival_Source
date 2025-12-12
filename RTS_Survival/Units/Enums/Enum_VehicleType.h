// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum EVehicleOrderType
{
    VHType_Civilian UMETA(DisplayName = "Civilian"),
    VHType_Transport UMETA(DisplayName = "Transport"),
    VHType_Military  UMETA(DisplayName = "Military"),

};

