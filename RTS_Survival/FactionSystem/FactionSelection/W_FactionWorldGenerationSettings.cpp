// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_FactionWorldGenerationSettings.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/RichTextBlock.h"
#include "Components/SpinBox.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "RTS_Survival/Game/RTSGameInstance/GameInstCampaignGenerationSettings/GameInstCampaignGenerationSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "UObject/EnumProperty.h"

namespace FactionWorldGenerationSettingsConstants
{
	constexpr int32 MinSeedValue = 0;
	constexpr int32 MaxSeedValue = MAX_int32;
}

void UW_FactionWorldGenerationSettings::SetFactionPlayerController(AFactionPlayerController* FactionPlayerController)
{
	M_FactionPlayerController = FactionPlayerController;
}

void UW_FactionWorldGenerationSettings::SetDifficultyText(const ERTSGameDifficulty SelectedDifficulty)
{
	if (not GetIsValidDifficultyText())
	{
		return;
	}

	const FString DifficultyName = UEnum::GetDisplayValueAsText(SelectedDifficulty).ToString();
	const FString DisplayText = FString::Printf(TEXT("Difficulty: %s"), *DifficultyName);
	const FString RichText = FRTSRichTextConverter::MakeRTSRich(DisplayText, ERTSRichText::Text_Armor);
	M_DifficultyText->SetText(FText::FromString(RichText));
}

void UW_FactionWorldGenerationSettings::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (not GetIsValidBackButton()
		or not GetIsValidGenerateButton()
		or not GetIsValidShowAdvancedSettingsCheckBox()
		or not GetIsValidExtraDifficultyBox()
		or not GetIsValidExtraDifficultyCheckBox()
		or not GetIsValidPersonalityBox()
		or not GetIsValidPersonalityComboBox()
		or not GetIsValidDifficultyText()
		or not GetIsValidSeedSpinBox())
	{
		return;
	}

	M_Back->OnClicked.AddDynamic(this, &UW_FactionWorldGenerationSettings::HandleBackClicked);
	M_Generate->OnClicked.AddDynamic(this, &UW_FactionWorldGenerationSettings::HandleGenerateClicked);
	M_ShowAdvancedSettings->OnCheckStateChanged.AddDynamic(
		this,
		&UW_FactionWorldGenerationSettings::HandleShowAdvancedSettingsChanged
	);

	M_ShowAdvancedSettings->SetIsChecked(false);
	M_ExtraDifficulty->SetIsChecked(false);

	SetupPersonalityComboBox();
	UpdateAdvancedSettingsVisibility(false);

	M_SeedSpinBox->SetMinValue(FactionWorldGenerationSettingsConstants::MinSeedValue);
	M_SeedSpinBox->SetMaxValue(FactionWorldGenerationSettingsConstants::MaxSeedValue);
	M_SeedSpinBox->SetValue(FactionWorldGenerationSettingsConstants::MinSeedValue);
}

void UW_FactionWorldGenerationSettings::HandleBackClicked()
{
	if (not GetIsValidFactionPlayerController())
	{
		return;
	}

	M_FactionPlayerController->HandleWorldGenerationBackRequested();
}

void UW_FactionWorldGenerationSettings::HandleGenerateClicked()
{
	if (not GetIsValidFactionPlayerController())
	{
		return;
	}

	if (not GetIsValidExtraDifficultyCheckBox())
	{
		return;
	}

	FCampaignGenerationSettings Settings;
	Settings.GenerationSeed = GetSelectedSeed();
	Settings.bUsesExtraDifficultyPercentage = M_ExtraDifficulty->IsChecked();
	Settings.EnemyWorldPersonality = GetSelectedPersonality();

	M_FactionPlayerController->HandleWorldGenerationSettingsGenerated(Settings);
}

void UW_FactionWorldGenerationSettings::HandleShowAdvancedSettingsChanged(const bool bIsChecked)
{
	UpdateAdvancedSettingsVisibility(bIsChecked);
}

void UW_FactionWorldGenerationSettings::SetupPersonalityComboBox()
{
	if (not GetIsValidPersonalityComboBox())
	{
		return;
	}

	UEnum* PersonalityEnum = StaticEnum<EEnemyWorldPersonality>();
	if (PersonalityEnum == nullptr)
	{
		RTSFunctionLibrary::ReportError("Failed to load EEnemyWorldPersonality enum.");
		return;
	}

	M_PersonalityComboBox->ClearOptions();
	const int32 EnumCount = PersonalityEnum->NumEnums();
	for (int32 EnumIndex = 0; EnumIndex < EnumCount; ++EnumIndex)
	{
		const FString DisplayName = PersonalityEnum->GetDisplayNameTextByIndex(EnumIndex).ToString();
		M_PersonalityComboBox->AddOption(DisplayName);
	}

	const FString BalancedOption = UEnum::GetDisplayValueAsText(EEnemyWorldPersonality::Balanced).ToString();
	M_PersonalityComboBox->SetSelectedOption(BalancedOption);
	M_PersonalityComboBox->SetIsEnabled(false);
}

void UW_FactionWorldGenerationSettings::UpdateAdvancedSettingsVisibility(const bool bIsVisible)
{
	if (not GetIsValidExtraDifficultyBox())
	{
		return;
	}

	if (not GetIsValidPersonalityBox())
	{
		return;
	}

	const ESlateVisibility DesiredVisibility = bIsVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	M_ExtraDifficultyBox->SetVisibility(DesiredVisibility);
	M_PersonalityBox->SetVisibility(DesiredVisibility);
}

EEnemyWorldPersonality UW_FactionWorldGenerationSettings::GetSelectedPersonality() const
{
	if (not GetIsValidPersonalityComboBox())
	{
		return EEnemyWorldPersonality::Balanced;
	}

	UEnum* PersonalityEnum = StaticEnum<EEnemyWorldPersonality>();
	if (PersonalityEnum == nullptr)
	{
		RTSFunctionLibrary::ReportError("Failed to load EEnemyWorldPersonality enum.");
		return EEnemyWorldPersonality::Balanced;
	}

	const FString SelectedOption = M_PersonalityComboBox->GetSelectedOption();
	const int32 EnumCount = PersonalityEnum->NumEnums();
	for (int32 EnumIndex = 0; EnumIndex < EnumCount; ++EnumIndex)
	{
		const FString DisplayName = PersonalityEnum->GetDisplayNameTextByIndex(EnumIndex).ToString();
		if (DisplayName == SelectedOption)
		{
			return static_cast<EEnemyWorldPersonality>(PersonalityEnum->GetValueByIndex(EnumIndex));
		}
	}

	return EEnemyWorldPersonality::Balanced;
}

int32 UW_FactionWorldGenerationSettings::GetSelectedSeed() const
{
	if (not GetIsValidSeedSpinBox())
	{
		return 0;
	}

	return M_SeedSpinBox->GetValue();
}

bool UW_FactionWorldGenerationSettings::GetIsValidFactionPlayerController() const
{
	if (M_FactionPlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_FactionPlayerController",
		"UW_FactionWorldGenerationSettings::GetIsValidFactionPlayerController",
		this
	);
	return false;
}

bool UW_FactionWorldGenerationSettings::GetIsValidBackButton() const
{
	if (IsValid(M_Back))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_Back",
		"UW_FactionWorldGenerationSettings::GetIsValidBackButton",
		this
	);
	return false;
}

bool UW_FactionWorldGenerationSettings::GetIsValidGenerateButton() const
{
	if (IsValid(M_Generate))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_Generate",
		"UW_FactionWorldGenerationSettings::GetIsValidGenerateButton",
		this
	);
	return false;
}

bool UW_FactionWorldGenerationSettings::GetIsValidShowAdvancedSettingsCheckBox() const
{
	if (IsValid(M_ShowAdvancedSettings))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_ShowAdvancedSettings",
		"UW_FactionWorldGenerationSettings::GetIsValidShowAdvancedSettingsCheckBox",
		this
	);
	return false;
}

bool UW_FactionWorldGenerationSettings::GetIsValidExtraDifficultyBox() const
{
	if (IsValid(M_ExtraDifficultyBox))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_ExtraDifficultyBox",
		"UW_FactionWorldGenerationSettings::GetIsValidExtraDifficultyBox",
		this
	);
	return false;
}

bool UW_FactionWorldGenerationSettings::GetIsValidExtraDifficultyCheckBox() const
{
	if (IsValid(M_ExtraDifficulty))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_ExtraDifficulty",
		"UW_FactionWorldGenerationSettings::GetIsValidExtraDifficultyCheckBox",
		this
	);
	return false;
}

bool UW_FactionWorldGenerationSettings::GetIsValidPersonalityBox() const
{
	if (IsValid(M_PersonalityBox))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_PersonalityBox",
		"UW_FactionWorldGenerationSettings::GetIsValidPersonalityBox",
		this
	);
	return false;
}

bool UW_FactionWorldGenerationSettings::GetIsValidPersonalityComboBox() const
{
	if (IsValid(M_PersonalityComboBox))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_PersonalityComboBox",
		"UW_FactionWorldGenerationSettings::GetIsValidPersonalityComboBox",
		this
	);
	return false;
}

bool UW_FactionWorldGenerationSettings::GetIsValidDifficultyText() const
{
	if (IsValid(M_DifficultyText))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_DifficultyText",
		"UW_FactionWorldGenerationSettings::GetIsValidDifficultyText",
		this
	);
	return false;
}

bool UW_FactionWorldGenerationSettings::GetIsValidSeedSpinBox() const
{
	if (IsValid(M_SeedSpinBox))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_SeedSpinBox",
		"UW_FactionWorldGenerationSettings::GetIsValidSeedSpinBox",
		this
	);
	return false;
}
