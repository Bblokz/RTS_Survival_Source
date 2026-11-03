// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_GlobalAbilityPanel.h"

#include "Components/Widget.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/UI/W_GA_Description/W_GA_Description.h"
#include "RTS_Survival/GlobalAbilitySystem/UI/W_GA_Item/W_GA_Item.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_GlobalAbilityPanel::InitWithLoadedAbilities(TArray<TObjectPtr<UGlobalAbility>> LoadedAbilities,
                                                    UGlobalAbilitiesManager* Manager)
{
	TArray<UW_GA_Item*> AbilityItems = GetAbilityItems();
	if (LoadedAbilities.Num() > AbilityItems.Num())
	{
		RTSFunctionLibrary::ReportError(TEXT("More global abilities loaded than there are ability panel widgets."));
	}

	for (int32 ItemIndex = 0; ItemIndex < AbilityItems.Num(); ++ItemIndex)
	{
		UW_GA_Item* AbilityItem = AbilityItems[ItemIndex];
		if (not IsValid(AbilityItem))
		{
			continue;
		}
		if (not LoadedAbilities.IsValidIndex(ItemIndex) || not IsValid(LoadedAbilities[ItemIndex]))
		{
			AbilityItem->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		AbilityItem->SetVisibility(ESlateVisibility::Visible);
		AbilityItem->SetupGa_Item(LoadedAbilities[ItemIndex], Manager);
	}
	if (IsValid(Description))
	{
		Description->HideDescription();
	}
}

TArray<UW_GA_Item*> UW_GlobalAbilityPanel::GetAbilityItems() const
{
	TArray<UW_GA_Item*> AbilityItems = {
		GlobalAbility_1.Get(), GlobalAbility_2.Get(), GlobalAbility_3.Get(), GlobalAbility_4.Get(),
		GlobalAbility_5.Get()
	};
	// Optional slots: a panel without them must report too many abilities instead of dropping them silently.
	for (UW_GA_Item* OptionalItem : {GlobalAbility_6.Get(), GlobalAbility_7.Get(), GlobalAbility_8.Get()})
	{
		if (IsValid(OptionalItem))
		{
			AbilityItems.Add(OptionalItem);
		}
	}
	return AbilityItems;
}

void UW_GlobalAbilityPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (IsValid(Description))
	{
		Description->HideDescription();
	}
}
