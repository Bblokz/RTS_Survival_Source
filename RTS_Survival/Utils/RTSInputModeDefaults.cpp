#include "RTS_Survival/Utils/RTSInputModeDefaults.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace RTSInputModeDefaults
{
	void ApplyRegularGameInputMode(ACPPController* PlayerController)
	{
		if (not IsValid(PlayerController))
		{
			RTSFunctionLibrary::ReportError("Invalid player controller in ApplyRegularGameInputMode.");
			return;
		}

		UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(
			PlayerController,
			nullptr,
			EMouseLockMode::LockAlways,
			false,
			false
		);
		PlayerController->bShowMouseCursor = true;
		PlayerController->bEnableClickEvents = true;
		PlayerController->bEnableMouseOverEvents = true;
	}
}
