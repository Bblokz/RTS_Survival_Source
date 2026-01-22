// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "W_StartGameWidget.h"

#include "Components/Button.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_StartGameWidget::InitStartGameWidget(UPlayerStartGameControl* StartGameControlComponent)
{
	M_StartGameControlComponent = StartGameControlComponent;
}

void UW_StartGameWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (not GetIsValidStartGameButton())
	{
		return;
	}

	M_StartGameButton->OnClicked.AddDynamic(this, &UW_StartGameWidget::OnStartGameButtonClicked);
}

void UW_StartGameWidget::OnStartGameButtonClicked()
{
	if (not GetIsValidStartGameControlComponent())
	{
		return;
	}

	M_StartGameControlComponent->PlayerStartedGame();
}

bool UW_StartGameWidget::GetIsValidStartGameButton() const
{
	if (IsValid(M_StartGameButton))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_StartGameButton",
		"GetIsValidStartGameButton",
		this
	);
	return false;
}

bool UW_StartGameWidget::GetIsValidStartGameControlComponent() const
{
	if (M_StartGameControlComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_StartGameControlComponent",
		"GetIsValidStartGameControlComponent",
		this
	);
	return false;
}
