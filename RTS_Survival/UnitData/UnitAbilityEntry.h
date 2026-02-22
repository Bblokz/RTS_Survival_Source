#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/ApplyBehaviourAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/BehaviourAbilityTypes/BehaviourAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityTypes/AimAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedWeaponAbilityComponent/AttachWeaponAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/TurretSwapComponent/TurretSwapAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ModeAbilityComponent/ModeAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/RocketAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/FieldConstructionAbilityComponent/FieldConstructionAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/FieldConstructionAbilityComponent/FieldConstructionTypes/FieldConstructionTypes.h"
#include "RTS_Survival/Units/Squads/Reinforcement/SquadReinforcementComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UnitAbilityEntry.generated.h"

class UGrenadeComponent;
enum class EGrenadeAbilityType : uint8;
class USquadReinforcementComponent;
class UAimAbilityComponent;
class UAttachedWeaponAbilityComponent;
class UTurretSwapComp;
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

	inline USquadReinforcementComponent* GetReinforcementAbilityComp(AActor* Actor)
	{
		if (not IsValid(Actor))
		{
			return nullptr;
		}
		TArray<USquadReinforcementComponent*> ReinCompArray;
		Actor->GetComponents<USquadReinforcementComponent>(ReinCompArray);
		if (ReinCompArray.Num() > 0)
		{
			return ReinCompArray[0];
		}
		return nullptr;
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
		for (FUnitAbilityEntry AbilityEntry : UnitAbilities)
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

	inline bool GetHasFieldConstructionAbility(
		const TArray<FUnitAbilityEntry>& UnitAbilities,
		const EFieldConstructionType FieldConstructionType,
		FUnitAbilityEntry& OutAbilityOfFieldConstruction)
	{
		const int32 CustomDataForFieldConstruction = static_cast<int32>(FieldConstructionType);
		for (const FUnitAbilityEntry& AbilityEntry : UnitAbilities)
		{
			if (AbilityEntry.AbilityId == EAbilityID::IdFieldConstruction &&
				AbilityEntry.CustomType == CustomDataForFieldConstruction)
			{
				OutAbilityOfFieldConstruction = AbilityEntry;
				return true;
			}
		}
		return false;
	}

	inline UFieldConstructionAbilityComponent* GetHasFieldConstructionAbility(
		AActor* const Actor,
		const EFieldConstructionType FieldConstructionType)
	{
		if (not IsValid(Actor))
		{
			return nullptr;
		}

		TArray<UFieldConstructionAbilityComponent*> ConstructionComponents;
		Actor->GetComponents<UFieldConstructionAbilityComponent>(ConstructionComponents);

		for (UFieldConstructionAbilityComponent* const EachComp : ConstructionComponents)
		{
			if (not IsValid(EachComp))
			{
				continue;
			}

			if (EachComp->GetConstructionType() == FieldConstructionType)
			{
				return EachComp;
			}
		}

		return nullptr;
	}


	inline bool GetHasModeAbility(const TArray<FUnitAbilityEntry>& UnitAbilities,
	                              const EModeAbilityType ModeAbility,
	                              FUnitAbilityEntry& OutAbilityOfMode)
	{
		const int32 CustomDataForMode = static_cast<int32>(ModeAbility);
		for (const FUnitAbilityEntry& AbilityEntry : UnitAbilities)
		{
			const bool bIsModeAbility = AbilityEntry.AbilityId == EAbilityID::IdActivateMode
				|| AbilityEntry.AbilityId == EAbilityID::IdDisableMode;
			if (bIsModeAbility && AbilityEntry.CustomType == CustomDataForMode)
			{
				OutAbilityOfMode = AbilityEntry;
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

	inline UFieldConstructionAbilityComponent* GetFieldConstructionAbilityCompOfType(
		const EFieldConstructionType Type, const AActor* Actor)
	{
		if (not IsValid(Actor))
		{
			return nullptr;
		}

		TArray<UFieldConstructionAbilityComponent*> FieldComps;
		Actor->GetComponents<UFieldConstructionAbilityComponent>(FieldComps);
		for (UFieldConstructionAbilityComponent* FieldComp : FieldComps)
		{
			if (not IsValid(FieldComp))
			{
				continue;
			}

			if (FieldComp->GetConstructionType() == Type)
			{
				return FieldComp;
			}
		}

		return nullptr;
	}

	inline UModeAbilityComponent* GetModeAbilityCompOfType(const EModeAbilityType Type, AActor* Actor)
	{
		if (not IsValid(Actor))
		{
			return nullptr;
		}

		TArray<UModeAbilityComponent*> ModeComps;
		Actor->GetComponents<UModeAbilityComponent>(ModeComps);
		for (UModeAbilityComponent* ModeComp : ModeComps)
		{
			if (not IsValid(ModeComp))
			{
				continue;
			}

			if (ModeComp->GetModeAbilityType() == Type)
			{
				return ModeComp;
			}
		}

		return nullptr;
	}

	UGrenadeComponent* GetGrenadeAbilityCompOfType(const EGrenadeAbilityType Type, const AActor* Actor);
	UAimAbilityComponent* GetHasAimAbilityComponent(const EAimAbilityType Type, const AActor* Actor);
	UAttachedWeaponAbilityComponent* GetAttachedWeaponAbilityComponent(const EAttachWeaponAbilitySubType Type,
	                                                                   const AActor* Actor);
	UTurretSwapComp* GetTurretSwapAbilityComponent(const ETurretSwapAbility Type, const AActor* Actor);


	inline bool GetHasGrenadeAbility(const TArray<FUnitAbilityEntry>& UnitAbilities,
	                                 const EAbilityID GrenadeAbilityId,
	                                 const EGrenadeAbilityType GrenadeAbility,
	                                 FUnitAbilityEntry& OutAbilityOfGrenade)
	{
		const int32 CustomDataForGrenade = static_cast<int32>(GrenadeAbility);
		for (const FUnitAbilityEntry& AbilityEntry : UnitAbilities)
		{
			if (AbilityEntry.AbilityId == GrenadeAbilityId && AbilityEntry.CustomType == CustomDataForGrenade)
			{
				OutAbilityOfGrenade = AbilityEntry;
				return true;
			}
		}
		return false;
	}

	inline bool GetHasAimAbility(const TArray<FUnitAbilityEntry>& UnitAbilities,
	                             const EAbilityID AimAbilityId,
	                             const EAimAbilityType AimAbilityType,
	                             FUnitAbilityEntry& OutAbilityOfAim)
	{
		const int32 CustomDataForAim = static_cast<int32>(AimAbilityType);
		for (const FUnitAbilityEntry& AbilityEntry : UnitAbilities)
		{
			if (AbilityEntry.AbilityId == AimAbilityId && AbilityEntry.CustomType == CustomDataForAim)
			{
				OutAbilityOfAim = AbilityEntry;
				return true;
			}
		}
		return false;
	}

	inline bool GetHasAttachedWeaponAbility(const TArray<FUnitAbilityEntry>& UnitAbilities,
	                                        const EAbilityID AttachedWeaponAbilityId,
	                                        const EAttachWeaponAbilitySubType AttachedWeaponAbilityType,
	                                        FUnitAbilityEntry& OutAbilityOfAttachedWeapon)
	{
		const int32 CustomDataForAttachedWeapon = static_cast<int32>(AttachedWeaponAbilityType);
		for (const FUnitAbilityEntry& AbilityEntry : UnitAbilities)
		{
			if (AbilityEntry.AbilityId == AttachedWeaponAbilityId
				&& AbilityEntry.CustomType == CustomDataForAttachedWeapon)
			{
				OutAbilityOfAttachedWeapon = AbilityEntry;
				return true;
			}
		}
		return false;
	}

	inline bool GetHasTurretSwapAbility(
		const TArray<FUnitAbilityEntry>& UnitAbilities,
		const ETurretSwapAbility TurretSwapAbilityType,
		FUnitAbilityEntry& OutAbilityOfTurretSwap)
	{
		const int32 CustomDataForSwap = static_cast<int32>(TurretSwapAbilityType);
		for (const FUnitAbilityEntry& AbilityEntry : UnitAbilities)
		{
			if (AbilityEntry.AbilityId == EAbilityID::IdSwapTurret && AbilityEntry.CustomType == CustomDataForSwap)
			{
				OutAbilityOfTurretSwap = AbilityEntry;
				return true;
			}
		}

		return false;
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
