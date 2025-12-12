// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DigInUnit.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UDigInUnit : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class RTS_SURVIVAL_API IDigInUnit
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void OnStartDigIn() =0;
	virtual void OnDigInCompleted() = 0;
	virtual void OnBreakCoverCompleted()=0;
	virtual void WallGotDestroyedForceBreakCover() =0;
};
