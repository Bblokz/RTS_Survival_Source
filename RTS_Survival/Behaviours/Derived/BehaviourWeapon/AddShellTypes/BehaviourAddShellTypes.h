// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"

#include "BehaviourAddShellTypes.generated.h"

class UWeaponState;
enum class EWeaponShellType : uint8;

USTRUCT()
struct FBehaviourAddShellTypesGrantedShellRecord
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<EWeaponShellType> GrantedShellTypes;
};

/**
 * @brief Weapon behaviour that grants extra shell options to eligible mounted weapons.
 * The shell list is additive while active and restores only behaviour-added shell types on removal.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UBehaviourAddShellTypes : public UBehaviourWeapon
{
	GENERATED_BODY()

protected:
	virtual void ApplyBehaviourToWeapon(UWeaponState* WeaponState) override;
	virtual void RemoveBehaviourFromWeapon(UWeaponState* WeaponState) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour|ShellTypes")
	TArray<EWeaponShellType> ShellTypesToGrant;

private:
	void AddShellTypesToWeapon(UWeaponState* WeaponState);
	void RemoveGrantedShellTypesFromWeapon(UWeaponState* WeaponState);
	bool TryGetGrantedShellTypes(const TWeakObjectPtr<UWeaponState>& WeaponStatePtr,
		TArray<EWeaponShellType>& OutGrantedShellTypes) const;
	bool TryGetReplacementShellType(const UWeaponState* WeaponState,
		EWeaponShellType ShellTypeToRemove,
		EWeaponShellType& OutReplacementShellType) const;

	// Cache of shell types this behaviour instance added per weapon so only those grants are reverted later.
	UPROPERTY()
	TMap<TWeakObjectPtr<UWeaponState>, FBehaviourAddShellTypesGrantedShellRecord> M_GrantedShellTypesPerWeapon;
};
