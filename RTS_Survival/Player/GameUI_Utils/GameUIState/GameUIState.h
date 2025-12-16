#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

#include "GameUIState.generated.h"

// A simple struct to save the last state that was used to update the action UI.
USTRUCT()
struct FGameUIState
{
	GENERATED_BODY()

public:
	FGameUIState():
	PrimarySelectedUnitType(EAllUnitType::UNType_None),
	PrimarySelectedUnit(nullptr)
	{}

        UPROPERTY()
        EAllUnitType PrimarySelectedUnitType;
        UPROPERTY()
        AActor* PrimarySelectedUnit;
        UPROPERTY()
        FActionUIParameters ActionUIParameters;
        UPROPERTY()
        TArray<FUnitAbilityEntry> PrimaryUnitAbilities;
};
