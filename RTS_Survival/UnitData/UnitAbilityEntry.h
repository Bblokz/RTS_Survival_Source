#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Player/Abilities.h"
#include "UnitAbilityEntry.generated.h"

/**
 * @brief Ability entry containing metadata such as cooldown and custom type.
 */
USTRUCT(BlueprintType)
struct FUnitAbilityEntry
{
        GENERATED_BODY()

        UPROPERTY(EditAnywhere, BlueprintReadWrite)
        EAbilityID AbilityId = EAbilityID::IdNoAbility;

        UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 CustomType = 0;

        UPROPERTY(EditAnywhere, BlueprintReadWrite)
        float CooldownDuration = 0.0f;

        UPROPERTY(EditAnywhere, BlueprintReadWrite)
        float CooldownRemaining = 0.0f;
};

inline FUnitAbilityEntry CreateAbilityEntryFromId(const EAbilityID AbilityId)
{
        FUnitAbilityEntry AbilityEntry;
        AbilityEntry.AbilityId = AbilityId;
        return AbilityEntry;
}

inline TArray<FUnitAbilityEntry> ConvertAbilityIdsToEntries(const TArray<EAbilityID>& AbilityIds)
{
        TArray<FUnitAbilityEntry> AbilityEntries;
        AbilityEntries.Reserve(AbilityIds.Num());
        for (const EAbilityID AbilityId : AbilityIds)
        {
                AbilityEntries.Add(CreateAbilityEntryFromId(AbilityId));
        }
        return AbilityEntries;
}

inline TArray<EAbilityID> ExtractAbilityIdsFromEntries(const TArray<FUnitAbilityEntry>& AbilityEntries,
        const bool bExcludeNoAbility = false)
{
        TArray<EAbilityID> AbilityIds;
        AbilityIds.Reserve(AbilityEntries.Num());
        for (const FUnitAbilityEntry& AbilityEntry : AbilityEntries)
        {
                if (bExcludeNoAbility && AbilityEntry.AbilityId == EAbilityID::IdNoAbility)
                {
                        continue;
                }
                AbilityIds.Add(AbilityEntry.AbilityId);
        }
        return AbilityIds;
}
