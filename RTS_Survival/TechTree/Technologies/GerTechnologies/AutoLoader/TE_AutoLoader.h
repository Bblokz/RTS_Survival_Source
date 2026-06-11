// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_AutoLoader.generated.h"

/**
 * @brief Adds the configured Auto Loader behaviour to supported German vehicle subtypes.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_AutoLoader : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_AutoLoader();
};
