// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CargoOwner.generated.h"

class ASquadController;
// This class does not need to be modified.
UINTERFACE()
class UCargoOwner : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class RTS_SURVIVAL_API ICargoOwner
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void OnSquadRegistered(ASquadController* SquadController) = 0;
	virtual void OnCargoEmpty() = 0;
};
