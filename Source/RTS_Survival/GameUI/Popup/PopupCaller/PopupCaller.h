// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PopupCaller.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UPopupCaller : public UInterface
{
	GENERATED_BODY()
};

/**
 * Handles all return logic from a pop-up widget.
 */
class RTS_SURVIVAL_API IPopupCaller
{
	GENERATED_BODY()

	friend class RTS_SURVIVAL_API UW_RTSPopup;

protected:
	virtual void OnPopupOK();
	virtual void OnPopupCancel();
	virtual void OnPopupBack();
};
