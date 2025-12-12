// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RTSNavAgent.generated.h"

// This class does not need to be modified.
UINTERFACE()
class URTSNavAgent : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class RTS_SURVIVAL_API IRTSNavAgent
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual float GetAgentRadius() = 0;
	virtual float GetAgentQueryRange() = 0;
	virtual bool ChangeActivePathPoint(const FVector& NewPathPoint) = 0;

	
};
