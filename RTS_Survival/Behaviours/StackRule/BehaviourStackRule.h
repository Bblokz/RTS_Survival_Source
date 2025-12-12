// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "BehaviourStackRule.generated.h"

UENUM()
enum class EBehaviourStackRule : uint8
{
None,
Stack,
Refresh,
Exclusive
};
