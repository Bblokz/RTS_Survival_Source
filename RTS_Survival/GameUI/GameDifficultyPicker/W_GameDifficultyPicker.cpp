// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_GameDifficultyPicker.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Game/Difficulty/GameDifficultySettings/RTSGameDifficultySettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

void UW_GameDifficultyPicker::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (not GetIsValidNewToRTSDifficultyButton()
		or not GetIsValidNormalDifficultyButton()
		or not GetIsValidHardDifficultyButton()
		or not GetIsValidBrutalDifficultyButton()
		or not GetIsValidIronmanDifficultyButton()
		or not GetIsValidDifficultyInfoBorder())
	{
		return;
	}

	M_NewToRTSDifficultyButton->OnHovered.AddDynamic(this, &UW_GameDifficultyPicker::OnNewToRTSDifficultyHovered);
	M_NormalDifficultyButton->OnHovered.AddDynamic(this, &UW_GameDifficultyPicker::OnNormalDifficultyHovered);
	M_HardDifficultyButton->OnHovered.AddDynamic(this, &UW_GameDifficultyPicker::OnHardDifficultyHovered);
	M_BrutalDifficultyButton->OnHovered.AddDynamic(this, &UW_GameDifficultyPicker::OnBrutalDifficultyHovered);
	M_IronmanDifficultyButton->OnHovered.AddDynamic(this, &UW_GameDifficultyPicker::OnIronmanDifficultyHovered);

	M_NewToRTSDifficultyButton->OnUnhovered.AddDynamic(this, &UW_GameDifficultyPicker::OnNewToRTSDifficultyUnhovered);
	M_NormalDifficultyButton->OnUnhovered.AddDynamic(this, &UW_GameDifficultyPicker::OnNormalDifficultyUnhovered);
	M_HardDifficultyButton->OnUnhovered.AddDynamic(this, &UW_GameDifficultyPicker::OnHardDifficultyUnhovered);
	M_BrutalDifficultyButton->OnUnhovered.AddDynamic(this, &UW_GameDifficultyPicker::OnBrutalDifficultyUnhovered);
	M_IronmanDifficultyButton->OnUnhovered.AddDynamic(this, &UW_GameDifficultyPicker::OnIronmanDifficultyUnhovered);

	M_NewToRTSDifficultyButton->OnClicked.AddDynamic(this, &UW_GameDifficultyPicker::OnNewToRTSDifficultyClicked);
	M_NormalDifficultyButton->OnClicked.AddDynamic(this, &UW_GameDifficultyPicker::OnNormalDifficultyClicked);
	M_HardDifficultyButton->OnClicked.AddDynamic(this, &UW_GameDifficultyPicker::OnHardDifficultyClicked);
	M_BrutalDifficultyButton->OnClicked.AddDynamic(this, &UW_GameDifficultyPicker::OnBrutalDifficultyClicked);
	M_IronmanDifficultyButton->OnClicked.AddDynamic(this, &UW_GameDifficultyPicker::OnIronmanDifficultyClicked);

	SetDifficultyInfoBorderVisible(false);
}

void UW_GameDifficultyPicker::OnNewToRTSDifficultyHovered()
{
	HandleDifficultyHovered(ERTSGameDifficulty::NewToRTS);
}

void UW_GameDifficultyPicker::OnNormalDifficultyHovered()
{
	HandleDifficultyHovered(ERTSGameDifficulty::Normal);
}

void UW_GameDifficultyPicker::OnHardDifficultyHovered()
{
	HandleDifficultyHovered(ERTSGameDifficulty::Hard);
}

void UW_GameDifficultyPicker::OnBrutalDifficultyHovered()
{
	HandleDifficultyHovered(ERTSGameDifficulty::Brutal);
}

void UW_GameDifficultyPicker::OnIronmanDifficultyHovered()
{
	HandleDifficultyHovered(ERTSGameDifficulty::Ironman);
}

void UW_GameDifficultyPicker::OnNewToRTSDifficultyUnhovered()
{
	HandleDifficultyUnhovered();
}

void UW_GameDifficultyPicker::OnNormalDifficultyUnhovered()
{
	HandleDifficultyUnhovered();
}

void UW_GameDifficultyPicker::OnHardDifficultyUnhovered()
{
	HandleDifficultyUnhovered();
}

void UW_GameDifficultyPicker::OnBrutalDifficultyUnhovered()
{
	HandleDifficultyUnhovered();
}

void UW_GameDifficultyPicker::OnIronmanDifficultyUnhovered()
{
	HandleDifficultyUnhovered();
}

void UW_GameDifficultyPicker::OnNewToRTSDifficultyClicked()
{
	HandleDifficultyClicked(ERTSGameDifficulty::NewToRTS);
}

void UW_GameDifficultyPicker::OnNormalDifficultyClicked()
{
	HandleDifficultyClicked(ERTSGameDifficulty::Normal);
}

void UW_GameDifficultyPicker::OnHardDifficultyClicked()
{
	HandleDifficultyClicked(ERTSGameDifficulty::Hard);
}

void UW_GameDifficultyPicker::OnBrutalDifficultyClicked()
{
	HandleDifficultyClicked(ERTSGameDifficulty::Brutal);
}

void UW_GameDifficultyPicker::OnIronmanDifficultyClicked()
{
	HandleDifficultyClicked(ERTSGameDifficulty::Ironman);
}

void UW_GameDifficultyPicker::HandleDifficultyHovered(const ERTSGameDifficulty HoveredDifficulty)
{
	SetDifficultyInfoBorderVisible(true);
	BP_OnDifficultyHovered(HoveredDifficulty);
}

void UW_GameDifficultyPicker::HandleDifficultyUnhovered()
{
	if (GetIsAnyDifficultyButtonHovered())
	{
		return;
	}

	SetDifficultyInfoBorderVisible(false);
}

void UW_GameDifficultyPicker::HandleDifficultyClicked(const ERTSGameDifficulty SelectedDifficulty)
{
	const URTSGameDifficultySettings* DifficultySettings = GetDefault<URTSGameDifficultySettings>();
	if (DifficultySettings == nullptr)
	{
		RTSFunctionLibrary::ReportError("Failed to get RTS game difficulty settings for selection.");
		return;
	}

	const int32* DifficultyPercentage = DifficultySettings->DifficultyPercentageByLevel.Find(SelectedDifficulty);
	if (DifficultyPercentage == nullptr)
	{
		const FString DifficultyAsString = UEnum::GetValueAsString(SelectedDifficulty);
		RTSFunctionLibrary::ReportError(
			"No difficulty percentage configured for: " + DifficultyAsString);
		return;
	}

	OnDifficultyChosen(*DifficultyPercentage, SelectedDifficulty);
}

void UW_GameDifficultyPicker::OnDifficultyChosen(const int32 DifficultyPercentage, const ERTSGameDifficulty SelectedDifficulty)
{
	AMissionManager* MissionManager = FRTS_Statics::GetGameMissionManager(this);
	if (not MissionManager)
	{
		return;
	}

	MissionManager->SetMissionDifficulty(DifficultyPercentage, SelectedDifficulty);
	RemoveFromParent();
	ConditionalBeginDestroy();
}

void UW_GameDifficultyPicker::SetDifficultyInfoBorderVisible(const bool bIsVisible)
{
	if (not GetIsValidDifficultyInfoBorder())
	{
		return;
	}

	M_DifficultyInfoBorder->SetVisibility(bIsVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

bool UW_GameDifficultyPicker::GetIsAnyDifficultyButtonHovered() const
{
	if (not GetIsValidNewToRTSDifficultyButton()
		or not GetIsValidNormalDifficultyButton()
		or not GetIsValidHardDifficultyButton()
		or not GetIsValidBrutalDifficultyButton()
		or not GetIsValidIronmanDifficultyButton())
	{
		return false;
	}

	return M_NewToRTSDifficultyButton->IsHovered()
		|| M_NormalDifficultyButton->IsHovered()
		|| M_HardDifficultyButton->IsHovered()
		|| M_BrutalDifficultyButton->IsHovered()
		|| M_IronmanDifficultyButton->IsHovered();
}

bool UW_GameDifficultyPicker::GetIsValidNewToRTSDifficultyButton() const
{
	if (IsValid(M_NewToRTSDifficultyButton))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_NewToRTSDifficultyButton", "UW_GameDifficultyPicker::GetIsValidNewToRTSDifficultyButton", this);
	return false;
}

bool UW_GameDifficultyPicker::GetIsValidNormalDifficultyButton() const
{
	if (IsValid(M_NormalDifficultyButton))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_NormalDifficultyButton", "UW_GameDifficultyPicker::GetIsValidNormalDifficultyButton", this);
	return false;
}

bool UW_GameDifficultyPicker::GetIsValidHardDifficultyButton() const
{
	if (IsValid(M_HardDifficultyButton))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_HardDifficultyButton", "UW_GameDifficultyPicker::GetIsValidHardDifficultyButton", this);
	return false;
}

bool UW_GameDifficultyPicker::GetIsValidBrutalDifficultyButton() const
{
	if (IsValid(M_BrutalDifficultyButton))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_BrutalDifficultyButton", "UW_GameDifficultyPicker::GetIsValidBrutalDifficultyButton", this);
	return false;
}

bool UW_GameDifficultyPicker::GetIsValidIronmanDifficultyButton() const
{
	if (IsValid(M_IronmanDifficultyButton))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_IronmanDifficultyButton", "UW_GameDifficultyPicker::GetIsValidIronmanDifficultyButton", this);
	return false;
}

bool UW_GameDifficultyPicker::GetIsValidDifficultyInfoBorder() const
{
	if (IsValid(M_DifficultyInfoBorder))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_DifficultyInfoBorder", "UW_GameDifficultyPicker::GetIsValidDifficultyInfoBorder", this);
	return false;
}
