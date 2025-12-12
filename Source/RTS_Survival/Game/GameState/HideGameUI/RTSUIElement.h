// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RTSUIElement.generated.h"

// This class does not need to be modified.
UINTERFACE()
class URTSUIElement : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class RTS_SURVIVAL_API IRTSUIElement
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	virtual void OnHideAllGameUI(const bool bHide);
};
