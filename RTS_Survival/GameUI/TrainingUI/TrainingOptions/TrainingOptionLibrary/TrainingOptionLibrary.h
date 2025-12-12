
// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInType/DigInType.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h" 

#include "TrainingOptionLibrary.generated.h"

struct FCardUnitTypeSelector;
struct FTrainingOption;

UCLASS()
class RTS_SURVIVAL_API UTrainingOptionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** @return The Training Option name of this unit type and subtype. */
    UFUNCTION(BlueprintCallable, Category = "Training")
    static FString GetTrainingOptionName(EAllUnitType UnitType, uint8 SubtypeValue);

    static FString GetTrainingOptionDisplayName(EAllUnitType UnitType, uint8 SubtypeValue);

    /** Get the name of the subtype, based on the unit type. */
    UFUNCTION(BlueprintCallable, Category = "Training")
    static FString GetEnumValueName(EAllUnitType UnitType, uint8 SubtypeValue);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Training")
    static FTrainingOption GetTrainingOptionFromCardSelector(FCardUnitTypeSelector CardSelector);
};

// Function declarations
inline static FString Global_GetNomadicSubtypeEnumAsString(const ENomadicSubtype NomadicSubtype)
{
    return StaticEnum<ENomadicSubtype>()->GetNameStringByValue(static_cast<int64>(NomadicSubtype));
}
inline static FString Global_GetTankSubtypeEnumAsString(const ETankSubtype TankSubtype)
{
    return StaticEnum<ETankSubtype>()->GetNameStringByValue(static_cast<int64>(TankSubtype));
}
inline static FString Global_GetSquadSubtypeEnumAsString(const ESquadSubtype SquadSubtype)
{
    return StaticEnum<ESquadSubtype>()->GetNameStringByValue(static_cast<int64>(SquadSubtype));
}

inline static FString Global_GetDigInTypeEnumAsString(const EDigInType DigInType)
{
    return StaticEnum<EDigInType>()->GetNameStringByValue(static_cast<int64>(DigInType));
}