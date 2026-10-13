// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_TrainingQuickNavButton.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingMenuManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_TrainingQuickNavButton::InitQuickNavButton(UTrainingMenuManager* TrainingMenuManager)
{
	M_TrainingMenuManager = TrainingMenuManager;
}

void UW_TrainingQuickNavButton::SetupForBuilding(const ENomadicSubtype Building, const FSlateBrush* IconBrush)
{
	M_Building = Building;
	const FString BuildingDisplayName = Global_GetNomadicDisplayName(Building);
	SetToolTipText(FText::FromString(BuildingDisplayName));
	if (IconBrush)
	{
		ShowIcon(*IconBrush);
		return;
	}
	ShowInitialsInsteadOfIcon(BuildingDisplayName);
}

void UW_TrainingQuickNavButton::ShowIcon(const FSlateBrush& IconBrush)
{
	// Keep the size the designer gave the icon; only the image itself comes from the building.
	FSlateBrush SizedIconBrush = IconBrush;
	SizedIconBrush.ImageSize = M_NavIcon->GetBrush().ImageSize;
	M_NavIcon->SetBrush(SizedIconBrush);
	M_NavIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (M_NavFallbackText)
	{
		M_NavFallbackText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UW_TrainingQuickNavButton::ShowInitialsInsteadOfIcon(const FString& BuildingDisplayName)
{
	M_NavIcon->SetVisibility(ESlateVisibility::Collapsed);
	if (not M_NavFallbackText)
	{
		return;
	}
	TArray<FString> Words;
	BuildingDisplayName.ParseIntoArray(Words, TEXT(" "));
	FString Initials;
	for (const FString& Word : Words)
	{
		Initials += Word.Left(1).ToUpper();
	}
	// A one word name such as HQ reads better in full than as a single letter.
	constexpr int32 MaxLettersOfSingleWordName = 3;
	if (Words.Num() == 1)
	{
		Initials = BuildingDisplayName.Left(MaxLettersOfSingleWordName).ToUpper();
	}
	M_NavFallbackText->SetText(FText::FromString(Initials));
	M_NavFallbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_TrainingQuickNavButton::UpdateQuickNavButton(const bool bIsBuildingAvailable, const bool bIsBuildingShown)
{
	SetVisibility(bIsBuildingAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	M_NavButton->SetBackgroundColor(bIsBuildingShown ? ShownBuildingButtonColor : OtherBuildingButtonColor);
}

void UW_TrainingQuickNavButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	M_NavButton->OnClicked.AddDynamic(this, &UW_TrainingQuickNavButton::OnNavButtonClicked);
}

bool UW_TrainingQuickNavButton::GetIsValidTrainingMenuManager() const
{
	if (M_TrainingMenuManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_TrainingMenuManager",
		"GetIsValidTrainingMenuManager",
		this
	);

	return false;
}

void UW_TrainingQuickNavButton::OnNavButtonClicked()
{
	if (not GetIsValidTrainingMenuManager())
	{
		return;
	}
	M_TrainingMenuManager->OnQuickNavButtonClicked(M_Building);
}
