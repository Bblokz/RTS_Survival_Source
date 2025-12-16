#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/ApplyBehaviourAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/BehaviourAbilityTypes/BehaviourAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/RocketAbilityTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
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
	int32 CooldownDuration = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CooldownRemaining = 0;
};

UENUM()
enum class EAttachedRocketAbilityType
{
	// The large hetzer rockets.
	Default,
	SmallRockets,
};


namespace FAbilityHelpers
{
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

	inline bool GetHasBehaviourAbility(const TArray<FUnitAbilityEntry>& UnitAbilities,
	                                   const EBehaviourAbilityType BehaviourAbility,
	                                   FUnitAbilityEntry& OutAbilityOfBehaviour)
	{
		const int32 CustomDataForBehaviour = static_cast<int32>(BehaviourAbility);
		for (const FUnitAbilityEntry& AbilityEntry : UnitAbilities)
		{
			if (AbilityEntry.AbilityId == EAbilityID::IdApplyBehaviour &&
				AbilityEntry.CustomType == CustomDataForBehaviour)
			{
				OutAbilityOfBehaviour = AbilityEntry;
				return true;
			}
		}
		return false;
	}

	inline UApplyBehaviourAbilityComponent* GetBehaviourAbilityCompOfType(
		const EBehaviourAbilityType Type, AActor* Actor)
	{
		if (not IsValid(Actor))
		{
			return nullptr;
		}
		TArray<UApplyBehaviourAbilityComponent*> BehComps;
		Actor->GetComponents<UApplyBehaviourAbilityComponent>(BehComps);
		for (UApplyBehaviourAbilityComponent* BehComp : BehComps)
		{
			if (not IsValid(BehComp))
			{
				continue;
			}
			if (BehComp->GetBehaviourAbilityType() == Type)
			{
				return BehComp;
			}
		}
		return nullptr;
	}


	inline FUnitAbilityEntry GetRocketAbilityEntry(const EAttachedRocketAbilityType RocketType)
	{
		FUnitAbilityEntry RocketAbilityEntry;
		RocketAbilityEntry.AbilityId = EAbilityID::IdFireRockets;
		RocketAbilityEntry.CustomType = static_cast<int32>(RocketType);
		return RocketAbilityEntry;
	}

	inline FUnitAbilityEntry GetRocketAbilityForRocketType(const ERocketAbility RocketAbility)
	{
		FUnitAbilityEntry DefaultRocketAbility;
		DefaultRocketAbility.AbilityId = EAbilityID::IdFireRockets;
		switch (RocketAbility)
		{
		case ERocketAbility::None:
			RTSFunctionLibrary::ReportError(
				TEXT("FAbilityHelpers::GetRocketAbilityForRocketType: Rocket ability is None!"));
			return DefaultRocketAbility;
		case ERocketAbility::HetzerRockets:
			return DefaultRocketAbility;
		case ERocketAbility::JagdPantherRockets:
			return GetRocketAbilityEntry(EAttachedRocketAbilityType::SmallRockets);
		};
		RTSFunctionLibrary::ReportError(
			TEXT("FAbilityHelpers::GetRocketAbilityForRocketType: Rocket ability type not handled!"));
		return DefaultRocketAbility;
	}

	inline TArray<FUnitAbilityEntry> SwapAtIdForNewEntry(const TArray<FUnitAbilityEntry>& OldAbilityArray,
	                                                     const EAbilityID AbilityIdToSwap,
	                                                     const FUnitAbilityEntry& NewAbilityEntry)
	{
		TArray<FUnitAbilityEntry> NewAbilityArray = OldAbilityArray;
		for (int32 Index = 0; Index < NewAbilityArray.Num(); Index++)
		{
			if (NewAbilityArray[Index].AbilityId == AbilityIdToSwap)
			{
				NewAbilityArray[Index] = NewAbilityEntry;
				break;
			}
		}
		return NewAbilityArray;
	}
}
