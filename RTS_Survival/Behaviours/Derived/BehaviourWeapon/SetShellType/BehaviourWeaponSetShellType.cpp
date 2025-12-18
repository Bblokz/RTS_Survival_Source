// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehaviourWeaponSetShellType.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

UBehaviourWeaponSetShellType::UBehaviourWeaponSetShellType()
{
	BehaviourStackRule = EBehaviourStackRule::Exclusive;
}

void UBehaviourWeaponSetShellType::OnAdded(AActor* BehaviourOwner)
{
	// Make sure to call the bp event.
	Super::OnAdded(BehaviourOwner);
}

void UBehaviourWeaponSetShellType::OnRemoved(AActor* BehaviourOwner)
{
	// Make sure to call the bp event.
	Super::OnRemoved(BehaviourOwner);
}

void UBehaviourWeaponSetShellType::ApplyBehaviourToWeapon(UWeaponState* WeaponState)
{
	if (WeaponState == nullptr)
	{
		return;
	}

	TryChangeShellType(WeaponState);
	UBehaviourWeapon::ApplyBehaviourToWeapon(WeaponState);
}

void UBehaviourWeaponSetShellType::RemoveBehaviourFromWeapon(UWeaponState* WeaponState)
{
	if (WeaponState == nullptr)
	{
		return;
	}

	UBehaviourWeapon::RemoveBehaviourFromWeapon(WeaponState);
	RestorePreviousShellType(WeaponState);
}

bool UBehaviourWeaponSetShellType::TryChangeShellType(UWeaponState* WeaponState)
{
	if (WeaponState == nullptr)
	{
		return false;
	}

	if (TargetShellType == EWeaponShellType::Shell_None)
	{
		return false;
	}

	const TWeakObjectPtr<UWeaponState> WeaponPtr = WeaponState;
	if (M_PreviousShellTypes.Contains(WeaponPtr))
	{
		return false;
	}

	const EWeaponShellType PreviousShellType = WeaponState->GetRawWeaponData().ShellType;
	if (PreviousShellType == TargetShellType)
	{
		return false;
	}

	if (not WeaponState->ChangeWeaponShellType(TargetShellType))
	{
		return false;
	}

	CachePreviousShellType(WeaponPtr, PreviousShellType);
	return true;
}

void UBehaviourWeaponSetShellType::CachePreviousShellType(const TWeakObjectPtr<UWeaponState>& WeaponPtr,
                                                          const EWeaponShellType PreviousShellType)
{
	if (not WeaponPtr.IsValid())
	{
		return;
	}

	M_PreviousShellTypes.Add(WeaponPtr, PreviousShellType);
}

void UBehaviourWeaponSetShellType::RestorePreviousShellType(UWeaponState* WeaponState)
{
	if (WeaponState == nullptr)
	{
		return;
	}

	const TWeakObjectPtr<UWeaponState> WeaponPtr = WeaponState;
	EWeaponShellType PreviousShellType;
	if (not TryGetCachedShellType(WeaponPtr, PreviousShellType))
	{
		return;
	}

	WeaponState->ChangeWeaponShellType(PreviousShellType);
	M_PreviousShellTypes.Remove(WeaponPtr);
}

bool UBehaviourWeaponSetShellType::TryGetCachedShellType(const TWeakObjectPtr<UWeaponState>& WeaponPtr,
                                                         EWeaponShellType& OutShellType) const
{
	if (not WeaponPtr.IsValid())
	{
		return false;
	}

	const EWeaponShellType* const FoundShellType = M_PreviousShellTypes.Find(WeaponPtr);
	if (FoundShellType == nullptr)
	{
		return false;
	}

	OutShellType = *FoundShellType;
	return true;
}
