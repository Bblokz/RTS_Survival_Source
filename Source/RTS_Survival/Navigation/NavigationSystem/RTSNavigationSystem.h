// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NavigationSystem.h"
#include "RTSNavigationSystem.generated.h"
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API URTSNavigationSystem : public UNavigationSystemV1
{
	GENERATED_BODY()

public:
	virtual ANavigationData* GetNavDataForProps(const FNavAgentProperties& AgentProperties) override;

	// todo....
};
