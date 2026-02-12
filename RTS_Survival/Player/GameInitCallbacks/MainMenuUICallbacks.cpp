#include "MainMenuUICallbacks.h"

#include "RTS_Survival/Player/CPPController.h"

void FMainMenuUICallbacks::SetPlayerController(ACPPController* Controller)
{
	PlayerController = Controller;
}

void FMainMenuUICallbacks::CallbackOnMenuReady(TFunction<void()> Callback, TWeakObjectPtr<UObject> WeakCallbackOwner)
{
	if (!EnsurePlayerControllerIsValid() || not WeakCallbackOwner.IsValid())
	{
		return;
	}
	// If menu already valid invoke immediately.
	if (IsValid(PlayerController.Get()->GetMainMenuUI()))
	{
		Callback();
		return;
	}
	// Otherwise, add the lambda to the delegate for later execution.
	OnMainMenuUIReady.AddLambda([WeakCallbackOwner, Callback]()->void
	{
		if (not WeakCallbackOwner.IsValid())
		{
			return;
		}

		UObject* StrongCallbackOwner = WeakCallbackOwner.Get();
		Callback();
	});
}


bool FMainMenuUICallbacks::EnsurePlayerControllerIsValid() const
{
	if (PlayerController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("PlayerController is not valid in FMainMenuUICallbacks");
	return false;
}
