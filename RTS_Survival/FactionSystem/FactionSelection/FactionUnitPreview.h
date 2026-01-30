// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FactionUnitPreview.generated.h"

/**
 * @brief Placement actor used in the faction selection map to mark where preview units should spawn.
 */
UCLASS()
class RTS_SURVIVAL_API AFactionUnitPreview : public AActor
{
	GENERATED_BODY()

public:
	AFactionUnitPreview();
};
