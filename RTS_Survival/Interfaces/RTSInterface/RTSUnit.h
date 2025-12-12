// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RTSUnit.generated.h"

// This class does not need to be modified.
UINTERFACE()
class URTSUnit : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class RTS_SURVIVAL_API IRTSUnit
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/**
	 * @brief Used when the unit is spawned to disable it unit it is ready to be used.
	 * @param bSetDisabled Whether to enable or disable the unit.
	 * @param TimeNotSelectable The time in seconds the unit is not selectable.
	 * @param MoveTo The location to move to.
	 */
	virtual void OnRTSUnitSpawned(
		const bool bSetDisabled,
		const float TimeNotSelectable = 0.f,
		const FVector MoveTo = FVector::ZeroVector) = 0;

	
};
