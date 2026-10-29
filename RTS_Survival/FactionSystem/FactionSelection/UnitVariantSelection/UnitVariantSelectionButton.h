// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"

#include "UnitVariantSelectionButton.generated.h"

/**
 * @brief Every button of the unit variant selection screen: it never takes keyboard focus, so the screen keeps the
 * arrow keys, Enter and Space for its own navigation instead of a clicked button swallowing them.
 */
UCLASS()
class RTS_SURVIVAL_API UUnitVariantSelectionButton : public UButton
{
	GENERATED_BODY()

public:
	UUnitVariantSelectionButton(const FObjectInitializer& ObjectInitializer);
};
