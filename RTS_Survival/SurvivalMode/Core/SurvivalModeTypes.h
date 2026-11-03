// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "SurvivalModeTypes.generated.h"

/**
 * @brief The phases of a survival match, in order; the director only ever moves forward.
 */
UENUM(BlueprintType)
enum class ESurvivalPhase : uint8
{
	None,
	// The player drags units from the faction roster into the army slots.
	ArmySelection,
	// The player cycles the start locations and confirms one.
	StartLocationSelection,
	// Aircraft fly the army in; the HQ lands first.
	Deploying,
	// Countdown before the first Soviet wave.
	GracePeriod,
	// Soviet waves attack; the survival clock runs.
	Waves,
	// The player lost; the result screen shows the time survived.
	Defeated
};

static inline FString Global_GetSurvivalPhaseAsString(const ESurvivalPhase Phase)
{
	switch (Phase)
	{
	case ESurvivalPhase::None:
		return TEXT("None");
	case ESurvivalPhase::ArmySelection:
		return TEXT("ArmySelection");
	case ESurvivalPhase::StartLocationSelection:
		return TEXT("StartLocationSelection");
	case ESurvivalPhase::Deploying:
		return TEXT("Deploying");
	case ESurvivalPhase::GracePeriod:
		return TEXT("GracePeriod");
	case ESurvivalPhase::Waves:
		return TEXT("Waves");
	case ESurvivalPhase::Defeated:
		return TEXT("Defeated");
	}
	return TEXT("Unknown");
}

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSurvivalPhaseChanged, ESurvivalPhase /*OldPhase*/, ESurvivalPhase /*NewPhase*/);
