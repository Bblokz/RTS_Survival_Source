// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "StartGameWidget.h"

#include "Components/Button.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Utils/RTSFunctionLibrary.h"

void UStartGameWidget::InitStartGameWidget(UPlayerStartGameControl* StartGameControlComponent)
{
	M_StartGameControlComponent = StartGameControlComponent;
}

void UStartGameWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (not GetIsValidStartGameButton())
	{
		return;
	}

	M_StartGameButton->OnClicked.AddDynamic(this, &UStartGameWidget::OnStartGameButtonClicked);
}

void UStartGameWidget::OnStartGameButtonClicked()
{
	if (not GetIsValidStartGameControlComponent())
	{
		return;
	}

	M_StartGameControlComponent->PlayerStartedGame();
}

bool UStartGameWidget::GetIsValidStartGameButton() const
{
	if (IsValid(M_StartGameButton))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_StartGameButton",
		"GetIsValidStartGameButton",
		this
	);
	return false;
}

bool UStartGameWidget::GetIsValidStartGameControlComponent() const
{
	if (M_StartGameControlComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_StartGameControlComponent",
		"GetIsValidStartGameControlComponent",
		this
	);
	return false;
}
