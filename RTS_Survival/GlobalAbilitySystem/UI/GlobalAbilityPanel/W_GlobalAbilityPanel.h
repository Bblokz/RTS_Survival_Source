// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_GlobalAbilityPanel.generated.h"

class UGlobalAbilitiesManager;
class UW_GA_Description;
class UGlobalAbility;
class UW_GA_Item;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_GlobalAbilityPanel : public UUserWidget
{
	GENERATED_BODY()
	
	public:
	void InitWithLoadedAbilities(TArray<TObjectPtr<UGlobalAbility>> LoadedAbilities,
		UGlobalAbilitiesManager* Manager);
	
	UW_GA_Description* GetDescription() const {return Description;}
	TArray<UW_GA_Item*> GetAbilityItems() const;

	/**
	 * @brief Hotkey entry point: clicks the slot's ability button only when the player could click it themselves.
	 * @param SlotIndex Zero-based slot from the top of the sidebar (IA_GlobalAbility1 = 0).
	 * @return True when the ability was asked to activate.
	 */
	bool TryActivateAbilityFromHotkey(const int32 SlotIndex) const;

	protected:
	virtual void NativeOnInitialized() override;
	
	UPROPERTY( blueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_GA_Item> GlobalAbility_1;
	UPROPERTY( blueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_GA_Item> GlobalAbility_2;
	UPROPERTY( blueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_GA_Item> GlobalAbility_3;
	UPROPERTY( blueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_GA_Item> GlobalAbility_4;
	UPROPERTY( blueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_GA_Item> GlobalAbility_5;
	UPROPERTY( blueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UW_GA_Item> GlobalAbility_6;
	UPROPERTY( blueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UW_GA_Item> GlobalAbility_7;
	UPROPERTY( blueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UW_GA_Item> GlobalAbility_8;

	UPROPERTY( blueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_GA_Description> Description;
	
};
