// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Navigation/RTSNavAgents/ERTSNavAgents.h"
#include "UObject/Interface.h"

#include "IRTSNavAI.generated.h"

UINTERFACE(MinimalAPI)
class URTSNavAIInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * @brief Interface for objects (typically AIControllers or Pawns) 
 * that can declare which RTSNavAgents type they should use.
 */
class RTS_SURVIVAL_API IRTSNavAIInterface
{
	GENERATED_BODY()

public:
	virtual void SetDefaultQueryFilter(const TSubclassOf<UNavigationQueryFilter> NewDefaultFilter) = 0;
};
