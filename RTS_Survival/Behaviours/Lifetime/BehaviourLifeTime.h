// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "BehaviourLifeTime.generated.h"

UENUM()
enum class EBehaviourLifeTime : uint8
{
None,
Permanent,
Timed
};
