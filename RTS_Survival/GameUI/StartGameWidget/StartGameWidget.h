// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "StartGameWidget.generated.h"

/**
 * @brief Added to the viewport to gate gameplay until the player confirms the start.
 */
UCLASS()
class RTS_SURVIVAL_API UStartGameWidget : public UUserWidget
{
	GENERATED_BODY()
};
