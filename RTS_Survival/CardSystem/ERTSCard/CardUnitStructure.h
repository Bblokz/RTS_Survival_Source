#pragma once

#include "CoreMinimal.h"
#include "CardUnitStructure.generated.h"

enum class EAircraftSubtype : uint8;
enum class ETankSubtype : uint8;
enum class ENomadicSubtype : uint8;
enum class ESquadSubtype : uint8;

USTRUCT(BlueprintType)
struct FCardUnitTypeSelector
{
    GENERATED_BODY()

    FCardUnitTypeSelector();

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    ESquadSubtype SquadSubtype;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    ENomadicSubtype NomadicSubtype;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    ETankSubtype TankSubtype;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    EAircraftSubtype AircraftSubtype;

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
