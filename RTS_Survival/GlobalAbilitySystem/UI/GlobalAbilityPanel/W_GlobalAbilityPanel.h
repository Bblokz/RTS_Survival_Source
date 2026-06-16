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
	
	UPROPERTY( blueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_GA_Description> Description;
	
};
