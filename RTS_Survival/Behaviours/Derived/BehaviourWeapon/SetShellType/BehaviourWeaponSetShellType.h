// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once
#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"

#include "BehaviourWeaponSetShellType.generated.h"


class UWeaponState;
enum class EWeaponShellType : uint8;

/**
 * @brief Weapon behaviour that forces weapons to use a designer-selected shell type while active.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UBehaviourWeaponSetShellType : public UBehaviourWeapon
{
	GENERATED_BODY()

public:
	UBehaviourWeaponSetShellType();

protected:
	virtual void ApplyBehaviourToWeapon(UWeaponState* WeaponState) override;
	virtual void RemoveBehaviourFromWeapon(UWeaponState* WeaponState) override;

	/** Shell type to apply while this behaviour is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour|ShellType")
	EWeaponShellType TargetShellType = EWeaponShellType::Shell_None;

private:
	bool TryChangeShellType(UWeaponState* WeaponState);
	void CachePreviousShellType(const TWeakObjectPtr<UWeaponState>& WeaponPtr, const EWeaponShellType PreviousShellType);
	void RestorePreviousShellType(UWeaponState* WeaponState);
	bool TryGetCachedShellType(const TWeakObjectPtr<UWeaponState>& WeaponPtr, EWeaponShellType& OutShellType) const;

	UPROPERTY()
	TMap<TWeakObjectPtr<UWeaponState>, EWeaponShellType> M_PreviousShellTypes;
};
