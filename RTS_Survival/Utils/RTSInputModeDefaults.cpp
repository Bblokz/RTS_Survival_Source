#include "RTS_Survival/Utils/RTSInputModeDefaults.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace RTSInputModeDefaults
{
	namespace RTSInputModeDefaultsInternal
	{
		UWidget* GetWidgetToFocusForRegularGameInputMode(ACPPController* PlayerController, UMainGameUI*& OutMainGameUI)
		{
			OutMainGameUI = nullptr;
			if (not IsValid(PlayerController))
			{
				return nullptr;
			}

			UMainGameUI* const MainMenuUI = PlayerController->GetMainMenuUI();
			if (
				not IsValid(MainMenuUI))
			{
				return nullptr;
			}

			OutMainGameUI = MainMenuUI;
			return MainMenuUI;
		}
	}

	void ApplyRegularGameInputMode(ACPPController* PlayerController)
	{
		if (not IsValid(PlayerController))
		{
			RTSFunctionLibrary::ReportError("Invalid player controller in ApplyRegularGameInputMode.");
			return;
		}
	UMainGameUI* MainGameUI;
		// When the main menu is active again, restore focus to it so hover/click handling resumes immediately.
		UWidget* const WidgetToFocus =
			RTSInputModeDefaultsInternal::GetWidgetToFocusForRegularGameInputMode(PlayerController, MainGameUI);
		// const EMouseLockMode MouseLockMode = WidgetToFocus != nullptr
		// 	                                     ? EMouseLockMode::DoNotLock
		// 	                                     : EMouseLockMode::LockAlways;
		const EMouseLockMode MouseLockMode = EMouseLockMode::LockAlways;

		UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(
			PlayerController,
			WidgetToFocus,
			MouseLockMode,
			false,
			false
		);
		PlayerController->bShowMouseCursor = true;
		PlayerController->bEnableClickEvents = true;
		PlayerController->bEnableMouseOverEvents = true;
		if(MainGameUI)
		{
			MainGameUI->SetFocus();
		}
	}
}
