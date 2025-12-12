// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_AmmoButton.generated.h"

class UW_AmmoPicker;
enum class EWeaponShellType : uint8;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_AmmoButton : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitAmmoButton(const EWeaponShellType ShellType, UW_AmmoPicker* AmmoPicker);
	inline EWeaponShellType GetShellType() const { return M_ShellType; };

protected:
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnAmmoButtonClicked();

	// Set up visuals in derived blueprint.
	UFUNCTION(BlueprintImplementableEvent)
	void OnInitAmmoButton(const EWeaponShellType ShellType);

	EWeaponShellType M_ShellType;

	// The ammo picker widget that contains this button.
	UPROPERTY()
	UW_AmmoPicker* M_AmmoPicker;
};
