#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "CardUnitStructure.generated.h"

USTRUCT(BlueprintType)
struct FCardUnitTypeSelector
{
    GENERATED_BODY()

    FCardUnitTypeSelector();

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    ESquadSubtype SquadSubtype = ESquadSubtype::Squad_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    ENomadicSubtype NomadicSubtype = ENomadicSubtype::Nomadic_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    ETankSubtype TankSubtype  = ETankSubtype::Tank_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    EAircraftSubtype AircraftSubtype = EAircraftSubtype::Aircarft_None;

    // Equality operator for TMap key usage
    bool operator==(const FCardUnitTypeSelector& Other) const
    {
        return SquadSubtype == Other.SquadSubtype &&
               NomadicSubtype == Other.NomadicSubtype &&
               TankSubtype == Other.TankSubtype &&
                   AircraftSubtype == Other.AircraftSubtype;
    }
};

FORCEINLINE uint32 GetTypeHash(const FCardUnitTypeSelector& Selector)
{
    uint32 Hash = 0;
    Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Selector.SquadSubtype)));
    Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Selector.NomadicSubtype)));
    Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Selector.TankSubtype)));
    Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Selector.AircraftSubtype)));
    return Hash;
}
