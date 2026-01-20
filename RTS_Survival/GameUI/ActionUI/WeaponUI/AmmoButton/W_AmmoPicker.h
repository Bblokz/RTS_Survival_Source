// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_AmmoPicker.generated.h"

class UActionUIManager;
enum class EWeaponShellType : uint8;
class UW_WeaponItem;
class UW_AmmoButton;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_AmmoPicker : public UUserWidget
{
	GENERATED_BODY()

public:

/**
	 * @brief Sets up the ammo picker for the weapon item.
	 * @param WeaponItem The weapon item that was clicked.
	 * @param ShellTypesToPickFrom The shell types that the weapon item can use.
	 */
	void OnClickedWeaponItemToAmmoPick(UW_WeaponItem* WeaponItem, const TArray<EWeaponShellType>& ShellTypesToPickFrom);

	void InitActionUIMngr(UActionUIManager* ActionUIManager);

	/**
	 * @brief Attempts to set the selected ammo type on the weapon item.
	 * @param SelectedShellType The selected shell type.
	 */
	void OnShellTypeSelected(const EWeaponShellType SelectedShellType);

	void OnAmmoButtonHovered(const EWeaponShellType HoveredShellType, const bool bIsHovering) const;

	void SetAmmoPickerVisibility(const bool bVisible);
protected:
	/** @brief Called on BeginPlay, loads the ammo buttons into the picker and sets up each button with a type. */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitAmmoPicker(const TArray<UW_AmmoButton*>& AmmoButtons);

private:
	UPROPERTY()
	TArray<UW_AmmoButton*> M_AmmoButtons;

	// The weapon item that was clicked and populated the ammo picker.
	UPROPERTY()
	UW_WeaponItem* M_ActiveWeaponItem;

	UPROPERTY()
	TWeakObjectPtr<UActionUIManager> M_ActionUIManager;
	
	bool GetIsValidActionUIManager()const;
	bool GetIsValidActiveWeaponItem()const;
};
