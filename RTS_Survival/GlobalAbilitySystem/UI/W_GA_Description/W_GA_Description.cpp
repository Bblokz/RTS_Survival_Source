// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_GA_Description.h"

#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityCostState.h"

void UW_GA_Description::ShowDescription(const FGlobalAbilityUISettings& UISettings,
                                         const FGLobalAbilityCostState& CostState,
                                         const FText& OverrideDescription)
{
	SetVisibility(ESlateVisibility::Visible);

	if (IsValid(Title))
	{
		Title->SetText(UISettings.Title);
	}
	if (IsValid(Description))
	{
		const FText DescriptionText = OverrideDescription.IsEmpty() ? UISettings.Description : OverrideDescription;
		Description->SetText(DescriptionText);
	}
	if (IsValid(Cooldown))
	{
		Cooldown->SetText(FText::FromString(FString::Printf(TEXT("Cooldown: %d"), CostState.CoolDownTime)));
	}
	if (IsValid(CostDisplay))
	{
		CostDisplay->SetupCost(CostState.Costs.ResourceCosts);
	}
	if (IsValid(AbilityIcon))
	{
		AbilityIcon->SetBrushFromTexture(UISettings.AbilityIcon);
	}
}

void UW_GA_Description::HideDescription()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
