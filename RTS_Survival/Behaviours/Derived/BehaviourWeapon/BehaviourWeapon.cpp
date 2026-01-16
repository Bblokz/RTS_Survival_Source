// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehaviourWeapon.h"

#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "Engine/World.h"
#include "RTS_Survival/Weapons/BombComponent/BombComponent.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

namespace BehaviourWeaponConstants
{
	constexpr float NextFrameDelaySeconds = 1.f;
	constexpr int32 DefaultWeaponStackCount = 10;
}

UBehaviourWeapon::UBehaviourWeapon()
{
	BehaviourStackRule = EBehaviourStackRule::Stack;
	M_MaxStackCount = BehaviourWeaponConstants::DefaultWeaponStackCount;
}

const FBehaviourWeaponMultipliers& UBehaviourWeapon::GetBehaviourWeaponMultipliers() const
{
	return BehaviourWeaponMultipliers;
}

void UBehaviourWeapon::OnAdded(AActor* BehaviourOwner)
{
	if (bM_HasInitializedPostBeginPlayLogic)
	{
		return;
	}

	SetupInitializationForOwner();
	// Make sure to call the bp event.
	Super::OnAdded(BehaviourOwner);
}

void UBehaviourWeapon::OnRemoved(AActor* BehaviourOwner)
{
	RemoveBehaviourFromTrackedWeapons();
	ClearTimers();
	// Make sure to call the bp event.
	Super::OnRemoved(BehaviourOwner);
}

void UBehaviourWeapon::OnStack(UBehaviour* StackedBehaviour)
{
	static_cast<void>(StackedBehaviour);

	TArray<UWeaponState*> Weapons;
	if (not TryGetAircraftWeapons(Weapons) && not TryGetTankWeapons(Weapons) && not TryGetSquadWeapons(Weapons))
	{
		return;
	}

	for (UWeaponState* WeaponState : Weapons)
	{
		if (WeaponState == nullptr)
		{
			continue;
		}

		if (not CheckRequirement(WeaponState))
		{
			continue;
		}

		OnWeaponBehaviourStack(WeaponState);
	}
}

bool UBehaviourWeapon::CheckRequirement(UWeaponState* WeaponState) const
{
	return WeaponState != nullptr;
}

void UBehaviourWeapon::ApplyBehaviourToWeapon(UWeaponState* WeaponState)
{
	if (WeaponState == nullptr)
	{
		return;
	}

	const FBehaviourWeaponAttributes CalculatedAttributes = CalculateBehaviourAttributesWithMultipliers(WeaponState);
	WeaponState->Upgrade(CalculatedAttributes);
	CacheAppliedAttributes(WeaponState, CalculatedAttributes);
}

void UBehaviourWeapon::RemoveBehaviourFromWeapon(UWeaponState* WeaponState)
{
	if (WeaponState == nullptr)
	{
		return;
	}

	FBehaviourWeaponAttributes CachedAttributes;
	if (not TryGetCachedAttributes(WeaponState, CachedAttributes))
	{
		return;
	}

	WeaponState->Upgrade(CachedAttributes, false);
	ClearCachedAttributesForWeapon(WeaponState);
}

FBehaviourWeaponAttributes UBehaviourWeapon::CalculateBehaviourAttributesWithMultipliers(
	UWeaponState* WeaponState) const
{
	FBehaviourWeaponAttributes CalculatedAttributes = BehaviourWeaponAttributes;
	if (WeaponState == nullptr)
	{
		return CalculatedAttributes;
	}

	const FWeaponData* WeaponData = WeaponState->GetWeaponDataToUpgrade();
	if (WeaponData == nullptr)
	{
		return CalculatedAttributes;
	}

	const auto ApplyFloatMultiplier = [](const float BaseValue, const float Multiplier, float& AttributeToUpgrade)
	{
		if (FMath::IsNearlyZero(Multiplier))
		{
			return;
		}

		const float PercentageDelta = BaseValue * (Multiplier - 1.0f);
		AttributeToUpgrade += PercentageDelta;
	};

	const auto ApplyIntMultiplier = [](const float BaseValue, const float Multiplier, int32& AttributeToUpgrade)
	{
		if (FMath::IsNearlyZero(Multiplier))
		{
			return;
		}

		const float PercentageDelta = BaseValue * (Multiplier - 1.0f);
		AttributeToUpgrade += FMath::CeilToInt(PercentageDelta);
	};

	ApplyFloatMultiplier(WeaponData->BaseDamage, BehaviourWeaponMultipliers.DamageMlt, CalculatedAttributes.Damage);
	ApplyFloatMultiplier(WeaponData->Range, BehaviourWeaponMultipliers.RangeMlt, CalculatedAttributes.Range);
	ApplyFloatMultiplier(WeaponData->BaseCooldown, BehaviourWeaponMultipliers.BaseCooldownMlt,
	                     CalculatedAttributes.BaseCooldown);
	ApplyFloatMultiplier(WeaponData->ReloadSpeed, BehaviourWeaponMultipliers.ReloadSpeedMlt,
	                     CalculatedAttributes.ReloadSpeed);
	ApplyIntMultiplier(WeaponData->Accuracy, BehaviourWeaponMultipliers.AccuracyMlt, CalculatedAttributes.Accuracy);
	ApplyIntMultiplier(WeaponData->MagCapacity, BehaviourWeaponMultipliers.MagSizeMlt, CalculatedAttributes.MagSize);
	ApplyFloatMultiplier(WeaponData->ArmorPen, BehaviourWeaponMultipliers.ArmorPenetrationMlt,
	                     CalculatedAttributes.ArmorPenetration);
	ApplyFloatMultiplier(WeaponData->ArmorPenMaxRange, BehaviourWeaponMultipliers.ArmorPenetrationMlt,
	                     CalculatedAttributes.ArmorPenetrationMaxRange);
	ApplyFloatMultiplier(WeaponData->TNTExplosiveGrams, BehaviourWeaponMultipliers.TnTGramsMlt,
	                     CalculatedAttributes.TnTGrams);
	ApplyFloatMultiplier(WeaponData->ShrapnelRange, BehaviourWeaponMultipliers.ShrapnelRangeMlt,
	                     CalculatedAttributes.ShrapnelRange);
	ApplyFloatMultiplier(WeaponData->ShrapnelDamage, BehaviourWeaponMultipliers.ShrapnelDamageMlt,
	                     CalculatedAttributes.ShrapnelDamage);
	ApplyIntMultiplier(static_cast<float>(WeaponData->ShrapnelParticles),
	                   BehaviourWeaponMultipliers.ShrapnelParticlesMlt,
	                   CalculatedAttributes.ShrapnelParticles);
	ApplyFloatMultiplier(WeaponData->ShrapnelPen, BehaviourWeaponMultipliers.ShrapnelPenMlt,
	                     CalculatedAttributes.ShrapnelPen);
	ApplyFloatMultiplier(CalculatedAttributes.FlameAngle, BehaviourWeaponMultipliers.FlameAngleMlt,
	                     CalculatedAttributes.FlameAngle);
	ApplyIntMultiplier(CalculatedAttributes.DamageTicks, BehaviourWeaponMultipliers.DamageTicksMlt,
	                   CalculatedAttributes.DamageTicks);

	return CalculatedAttributes;
}

void UBehaviourWeapon::CacheAppliedAttributes(UWeaponState* WeaponState,
                                              const FBehaviourWeaponAttributes& AppliedAttributes)
{
	if (WeaponState == nullptr)
	{
		return;
	}

	const TWeakObjectPtr<UWeaponState> WeaponPtr = WeaponState;
	if (FBehaviourWeaponAttributes* CachedAttributes = M_AppliedAttributesPerWeapon.Find(WeaponPtr))
	{
		CachedAttributes->Damage += AppliedAttributes.Damage;
		CachedAttributes->Range += AppliedAttributes.Range;
		CachedAttributes->BaseCooldown += AppliedAttributes.BaseCooldown;
		CachedAttributes->ReloadSpeed += AppliedAttributes.ReloadSpeed;
		CachedAttributes->Accuracy += AppliedAttributes.Accuracy;
		CachedAttributes->MagSize += AppliedAttributes.MagSize;
		CachedAttributes->ArmorPenetration += AppliedAttributes.ArmorPenetration;
		CachedAttributes->TnTGrams += AppliedAttributes.TnTGrams;
		CachedAttributes->ShrapnelRange += AppliedAttributes.ShrapnelRange;
		CachedAttributes->ShrapnelDamage += AppliedAttributes.ShrapnelDamage;
		CachedAttributes->ShrapnelParticles += AppliedAttributes.ShrapnelParticles;
		CachedAttributes->ShrapnelPen += AppliedAttributes.ShrapnelPen;
		CachedAttributes->FlameAngle += AppliedAttributes.FlameAngle;
		CachedAttributes->DamageTicks += AppliedAttributes.DamageTicks;
		return;
	}

	M_AppliedAttributesPerWeapon.Add(WeaponPtr, AppliedAttributes);
}

bool UBehaviourWeapon::TryGetCachedAttributes(UWeaponState* WeaponState,
                                              FBehaviourWeaponAttributes& OutCachedAttributes) const
{
	if (WeaponState == nullptr)
	{
		return false;
	}

	const TWeakObjectPtr<UWeaponState> WeaponPtr = WeaponState;
	const FBehaviourWeaponAttributes* CachedAttributes = M_AppliedAttributesPerWeapon.Find(WeaponPtr);
	if (CachedAttributes == nullptr)
	{
		return false;
	}

	OutCachedAttributes = *CachedAttributes;
	return true;
}

void UBehaviourWeapon::ClearCachedAttributesForWeapon(UWeaponState* WeaponState)
{
	if (WeaponState == nullptr)
	{
		return;
	}

	const TWeakObjectPtr<UWeaponState> WeaponPtr = WeaponState;
	M_AppliedAttributesPerWeapon.Remove(WeaponPtr);
}

void UBehaviourWeapon::OnWeaponBehaviourStack(UWeaponState* WeaponState)
{
	ApplyBehaviourToWeapon(WeaponState);
}

void UBehaviourWeapon::PostBeginPlayLogicInitialized()
{
	if (bM_HasInitializedPostBeginPlayLogic)
	{
		return;
	}

	bM_HasInitializedPostBeginPlayLogic = true;
	TArray<UWeaponState*> Weapons;
	if (TryGetAircraftWeapons(Weapons) || TryGetTankWeapons(Weapons) || TryGetSquadWeapons(Weapons))
	{
		ApplyBehaviourToMountedWeapons(Weapons);
	}
}

void UBehaviourWeapon::ApplyBehaviourToMountedWeapons(const TArray<UWeaponState*>& Weapons)
{
	for (UWeaponState* WeaponState : Weapons)
	{
		if (WeaponState == nullptr)
		{
			continue;
		}

		if (not CheckRequirement(WeaponState))
		{
			continue;
		}

		ApplyBehaviourToWeapon(WeaponState);
		M_AffectedWeapons.AddUnique(WeaponState);
	}
}

void UBehaviourWeapon::RemoveBehaviourFromTrackedWeapons()
{
	for (const TWeakObjectPtr<UWeaponState>& WeaponPtr : M_AffectedWeapons)
	{
		if (not WeaponPtr.IsValid())
		{
			M_AppliedAttributesPerWeapon.Remove(WeaponPtr);
			continue;
		}

		RemoveBehaviourFromWeapon(WeaponPtr.Get());
	}

	M_AffectedWeapons.Empty();
}

void UBehaviourWeapon::SetupInitializationForOwner()
{
	if (bM_HasInitializedPostBeginPlayLogic)
	{
		return;
	}

	ASquadController* SquadController = nullptr;
	if (TryGetSquadController(SquadController))
	{
		SetupInitializationForSquadController();
		return;
	}

	SchedulePostBeginPlayLogic();
}

void UBehaviourWeapon::SetupInitializationForSquadController()
{
	if (bM_HasInitializedPostBeginPlayLogic)
	{
		return;
	}

	ASquadController* SquadController = nullptr;
	if (not TryGetSquadController(SquadController))
	{
		return;
	}

	if (SquadController->GetIsSquadFullyLoaded())
	{
		SchedulePostBeginPlayLogic();
		return;
	}

	RegisterSquadFullyLoadedCallback(SquadController);
}

void UBehaviourWeapon::SchedulePostBeginPlayLogic()
{
	if (bM_HasInitializedPostBeginPlayLogic)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	FTimerDelegate TimerDel;
	TimerDel.BindUObject(this, &UBehaviourWeapon::PostBeginPlayLogicInitialized);
	World->GetTimerManager().SetTimerForNextTick(TimerDel);
	//         &UBehaviourWeapon::PostBeginPlayLogicInitialized)
	// World->GetTimerManager().SetTimer(M_PostBeginPlayTimerHandle, this,
	//         &UBehaviourWeapon::PostBeginPlayLogicInitialized, BehaviourWeaponConstants::NextFrameDelaySeconds, false);
}

bool UBehaviourWeapon::TryGetAircraftWeapons(TArray<UWeaponState*>& OutWeapons) const
{
	AAircraftMaster* AircraftMaster = nullptr;
	if (not TryGetAircraftMaster(AircraftMaster))
	{
		return false;
	}

	UBombComponent* BombComponent = nullptr;
	OutWeapons = FRTSWeaponHelpers::GetWeaponsMountedOnAircraft(AircraftMaster, BombComponent);
	return true;
}

bool UBehaviourWeapon::TryGetTankWeapons(TArray<UWeaponState*>& OutWeapons) const
{
	ATankMaster* TankMaster = nullptr;
	if (not TryGetTankMaster(TankMaster))
	{
		return false;
	}

	OutWeapons = FRTSWeaponHelpers::GetWeaponsMountedOnTank(TankMaster);
	return true;
}

bool UBehaviourWeapon::TryGetSquadWeapons(TArray<UWeaponState*>& OutWeapons) const
{
	ASquadController* SquadController = nullptr;
	if (not TryGetSquadController(SquadController))
	{
		return false;
	}

	OutWeapons = SquadController->GetWeaponsOfSquad();
	return SquadController->GetIsSquadFullyLoaded();
}

bool UBehaviourWeapon::TryGetAircraftMaster(AAircraftMaster*& OutAircraftMaster) const
{
	OutAircraftMaster = Cast<AAircraftMaster>(GetOwningActor());
	return OutAircraftMaster != nullptr;
}

bool UBehaviourWeapon::TryGetTankMaster(ATankMaster*& OutTankMaster) const
{
	OutTankMaster = Cast<ATankMaster>(GetOwningActor());
	return OutTankMaster != nullptr;
}

bool UBehaviourWeapon::TryGetSquadController(ASquadController*& OutSquadController) const
{
	OutSquadController = Cast<ASquadController>(GetOwningActor());
	return OutSquadController != nullptr;
}

void UBehaviourWeapon::ClearTimers()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (TimerManager.TimerExists(M_PostBeginPlayTimerHandle))
	{
		TimerManager.ClearTimer(M_PostBeginPlayTimerHandle);
	}
}

void UBehaviourWeapon::RegisterSquadFullyLoadedCallback(ASquadController* SquadController)
{
	if (bM_HasInitializedPostBeginPlayLogic)
	{
		return;
	}

	if (SquadController == nullptr)
	{
		return;
	}

	const TFunction<void()> SquadLoadedCallback = [this]()
	{
		if (bM_HasInitializedPostBeginPlayLogic)
		{
			return;
		}

		ASquadController* LocalSquadController = nullptr;
		if (not TryGetSquadController(LocalSquadController))
		{
			return;
		}

		if (not LocalSquadController->GetIsSquadFullyLoaded())
		{
			return;
		}

		SchedulePostBeginPlayLogic();
	};

	SquadController->SquadDataCallbacks.CallbackOnSquadDataLoaded(SquadLoadedCallback, this);
}
