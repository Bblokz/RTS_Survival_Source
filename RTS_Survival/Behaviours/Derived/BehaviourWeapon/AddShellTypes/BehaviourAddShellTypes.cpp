// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehaviourAddShellTypes.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

void UBehaviourAddShellTypes::ApplyBehaviourToWeapon(UWeaponState* WeaponState)
{
	if (WeaponState == nullptr)
	{
		return;
	}

	AddShellTypesToWeapon(WeaponState);
}

void UBehaviourAddShellTypes::RemoveBehaviourFromWeapon(UWeaponState* WeaponState)
{
	if (WeaponState == nullptr)
	{
		return;
	}

	RemoveGrantedShellTypesFromWeapon(WeaponState);
}

void UBehaviourAddShellTypes::AddShellTypesToWeapon(UWeaponState* WeaponState)
{
	if (WeaponState == nullptr)
	{
		return;
	}

	FWeaponData* WeaponData = WeaponState->GetWeaponDataToUpgrade();
	if (WeaponData == nullptr)
	{
		return;
	}

	const TWeakObjectPtr<UWeaponState> WeaponStatePtr = WeaponState;
	TArray<EWeaponShellType>& GrantedShellTypes = M_GrantedShellTypesPerWeapon.FindOrAdd(WeaponStatePtr);

	for (const EWeaponShellType ShellTypeToGrant : ShellTypesToGrant)
	{
		if (ShellTypeToGrant == EWeaponShellType::Shell_None)
		{
			continue;
		}

		if (WeaponData->ShellTypes.Contains(ShellTypeToGrant))
		{
			continue;
		}

		WeaponState->UpgradeWeaponWithExtraShellType(ShellTypeToGrant);
		GrantedShellTypes.AddUnique(ShellTypeToGrant);
	}

	if (GrantedShellTypes.Num() == 0)
	{
		M_GrantedShellTypesPerWeapon.Remove(WeaponStatePtr);
	}
}

void UBehaviourAddShellTypes::RemoveGrantedShellTypesFromWeapon(UWeaponState* WeaponState)
{
	const TWeakObjectPtr<UWeaponState> WeaponStatePtr = WeaponState;
	TArray<EWeaponShellType> GrantedShellTypes;
	if (not TryGetGrantedShellTypes(WeaponStatePtr, GrantedShellTypes))
	{
		return;
	}

	FWeaponData* WeaponData = WeaponState->GetWeaponDataToUpgrade();
	if (WeaponData == nullptr)
	{
		return;
	}

	for (const EWeaponShellType ShellTypeToRemove : GrantedShellTypes)
	{
		if (not WeaponData->ShellTypes.Contains(ShellTypeToRemove))
		{
			continue;
		}

		if (WeaponData->ShellType == ShellTypeToRemove)
		{
			EWeaponShellType ReplacementShellType = EWeaponShellType::Shell_None;
			if (not TryGetReplacementShellType(WeaponState, ShellTypeToRemove, ReplacementShellType))
			{
				continue;
			}

			if (not WeaponState->ChangeWeaponShellType(ReplacementShellType))
			{
				continue;
			}
		}

		WeaponData->ShellTypes.RemoveSingle(ShellTypeToRemove);
	}

	M_GrantedShellTypesPerWeapon.Remove(WeaponStatePtr);
}

bool UBehaviourAddShellTypes::TryGetGrantedShellTypes(const TWeakObjectPtr<UWeaponState>& WeaponStatePtr,
	TArray<EWeaponShellType>& OutGrantedShellTypes) const
{
	if (not WeaponStatePtr.IsValid())
	{
		return false;
	}

	const TArray<EWeaponShellType>* FoundGrantedShellTypes = M_GrantedShellTypesPerWeapon.Find(WeaponStatePtr);
	if (FoundGrantedShellTypes == nullptr)
	{
		return false;
	}

	OutGrantedShellTypes = *FoundGrantedShellTypes;
	return true;
}

bool UBehaviourAddShellTypes::TryGetReplacementShellType(const UWeaponState* WeaponState,
	const EWeaponShellType ShellTypeToRemove,
	EWeaponShellType& OutReplacementShellType) const
{
	if (WeaponState == nullptr)
	{
		return false;
	}

	const FWeaponData& WeaponData = WeaponState->GetRawWeaponData();
	for (const EWeaponShellType AvailableShellType : WeaponData.ShellTypes)
	{
		if (AvailableShellType == ShellTypeToRemove)
		{
			continue;
		}

		OutReplacementShellType = AvailableShellType;
		return true;
	}

	return false;
}
