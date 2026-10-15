// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CrewRepairProgressSettings.generated.h"

class UW_CrewRepairProgress;

/**
 * @brief Project settings for the bar a tank shows above its health bar while its crew repairs modules.
 * One widget class serves every tank, so no tank Blueprint needs its own setup.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Crew Repair Progress Bar"))
class RTS_SURVIVAL_API UCrewRepairProgressSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UCrewRepairProgressSettings();

	/** Blueprint class derived from UW_CrewRepairProgress (progress bar plus crew repair icon). */
	UPROPERTY(Config, EditAnywhere, Category="Crew Repair Progress Bar")
	TSoftClassPtr<UW_CrewRepairProgress> WidgetClass;

	/** Used when the tank has no health bar widget to anchor on. */
	UPROPERTY(Config, EditAnywhere, Category="Crew Repair Progress Bar")
	FVector FallbackRelativeOffset = FVector(0.f, 0.f, 300.f);
};
