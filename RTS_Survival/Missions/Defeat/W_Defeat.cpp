#include "RTS_Survival/Missions/Defeat/W_Defeat.h"

#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_Defeat::BP_OnRestartMap()
{
	const UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("Defeat widget failed to restart map because world is invalid.");
		return;
	}

	const FName CurrentLevelName = FName(*UGameplayStatics::GetCurrentLevelName(this, true));
	UGameplayStatics::OpenLevel(this, CurrentLevelName);
}

void UW_Defeat::NativeConstruct()
{
	Super::NativeConstruct();
	BindButtonCallbacks();
}

void UW_Defeat::BindButtonCallbacks()
{
	BindQuitButton();
	BindRestartButton();
}

void UW_Defeat::BindQuitButton()
{
	const FString FunctionName = TEXT("UW_Defeat::BindQuitButton");
	if (not EnsureButtonIsValid(M_Quit, TEXT("M_Quit"), FunctionName))
	{
		return;
	}

	M_Quit->OnClicked.RemoveAll(this);
	M_Quit->OnClicked.AddDynamic(this, &UW_Defeat::HandleQuitClicked);
}

void UW_Defeat::BindRestartButton()
{
	const FString FunctionName = TEXT("UW_Defeat::BindRestartButton");
	if (not EnsureButtonIsValid(M_Restart, TEXT("M_Restart"), FunctionName))
	{
		return;
	}

	M_Restart->OnClicked.RemoveAll(this);
	M_Restart->OnClicked.AddDynamic(this, &UW_Defeat::HandleRestartClicked);
}

bool UW_Defeat::EnsureButtonIsValid(const UButton* ButtonToCheck, const FString& ButtonName, const FString& FunctionName) const
{
	if (ButtonToCheck)
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, ButtonName, FunctionName, this);
	return false;
}

void UW_Defeat::HandleQuitClicked()
{
	APlayerController* OwningPlayerController = GetOwningPlayer();
	if (not IsValid(OwningPlayerController))
	{
		RTSFunctionLibrary::ReportError("Defeat widget failed to quit game because owning player controller is invalid.");
		return;
	}

	UKismetSystemLibrary::QuitGame(this, OwningPlayerController, EQuitPreference::Quit, true);
}

void UW_Defeat::HandleRestartClicked()
{
	BP_OnRestartMap();
}
