#include "RTS_Survival/Missions/Victory/W_Victory.h"

#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_Victory::TriggerVictory(TSoftObjectPtr<UWorld> MapToLoadOnContinueVictory,
                                TSoftObjectPtr<UWorld> MenuLevelToLoad,
                                const bool bShowBackToMenu)
{
	M_MapToLoadOnContinueVictory = MapToLoadOnContinueVictory;
	M_MenuLevelToLoad = MenuLevelToLoad;
	bM_ShowBackToMenu = bShowBackToMenu;
	ApplyBackToMainMenuVisibility();
}

void UW_Victory::NativeConstruct()
{
	Super::NativeConstruct();
	BindButtonCallbacks();
	ApplyBackToMainMenuVisibility();
}

void UW_Victory::BindButtonCallbacks()
{
	BindContinueButton();
	BindBackToMainMenuButton();
}

void UW_Victory::BindContinueButton()
{
	const FString FunctionName = TEXT("UW_Victory::BindContinueButton");
	if (not GetIsValidContinueButton(FunctionName))
	{
		return;
	}

	M_Continue->OnClicked.RemoveAll(this);
	M_Continue->OnClicked.AddDynamic(this, &UW_Victory::HandleContinueClicked);
}

void UW_Victory::BindBackToMainMenuButton()
{
	const FString FunctionName = TEXT("UW_Victory::BindBackToMainMenuButton");
	if (not GetIsValidBackToMainMenuButton(FunctionName))
	{
		return;
	}

	M_BackToMainMenu->OnClicked.RemoveAll(this);
	M_BackToMainMenu->OnClicked.AddDynamic(this, &UW_Victory::HandleBackToMainMenuClicked);
}

void UW_Victory::ApplyBackToMainMenuVisibility() const
{
	if (not GetIsValidBackToMainMenuButton(TEXT("UW_Victory::ApplyBackToMainMenuVisibility")))
	{
		return;
	}

	M_BackToMainMenu->SetVisibility(bM_ShowBackToMenu ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UW_Victory::OpenConfiguredLevel(const TSoftObjectPtr<UWorld>& LevelToLoad, const FString& ActionName) const
{
	if (not EnsureLevelIsValid(LevelToLoad, ActionName))
	{
		return;
	}

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, LevelToLoad, true, FString());
}

bool UW_Victory::GetIsValidContinueButton(const FString& FunctionName) const
{
	if (IsValid(M_Continue))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_Continue"), FunctionName, this);
	return false;
}

bool UW_Victory::GetIsValidBackToMainMenuButton(const FString& FunctionName) const
{
	if (IsValid(M_BackToMainMenu))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_BackToMainMenu"), FunctionName, this);
	return false;
}

bool UW_Victory::EnsureLevelIsValid(const TSoftObjectPtr<UWorld>& LevelToLoad, const FString& ActionName) const
{
	if (not LevelToLoad.IsNull())
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Victory widget failed to " + ActionName + " because no level was configured.");
	return false;
}

void UW_Victory::HandleContinueClicked()
{
	OpenConfiguredLevel(M_MapToLoadOnContinueVictory, TEXT("continue victory"));
}

void UW_Victory::HandleBackToMainMenuClicked()
{
	OpenConfiguredLevel(M_MenuLevelToLoad, TEXT("load main menu"));
}
