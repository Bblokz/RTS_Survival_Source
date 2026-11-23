// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "MissionObjectivePlateButton.generated.h"

/**
 * @brief Mouse-only button of the mission objective plates (knob, title, OK / Next): clicking the mission panel
 * never takes keyboard focus, so the RTS hotkeys keep working. Built by UW_Mission in C++.
 */
UCLASS()
class RTS_SURVIVAL_API UMissionObjectivePlateButton : public UButton
{
	GENERATED_BODY()

public:
	UMissionObjectivePlateButton(const FObjectInitializer& ObjectInitializer);
};
