#include "OnArchiveLoadedCallbacks.h"
#include "RTS_Survival/GameUI/Archive/W_Archive/W_Archive.h"
#include "RTS_Survival/GameUI/MainGameUI.h"

void FOnArchiveLoadedCallbacks::CallbackOnArchiveReady(TFunction<void()> Callback,
                                                       TWeakObjectPtr<UObject> WeakCallbackOwner)
{
	if(!EnsureMainGameUIIsValid() || !WeakCallbackOwner.IsValid())
	{
		return;
	}
	// If Archive already valid invoke immediately.
	if (IsValid(M_MainGameUI.Get()->M_Archive))
	{
		Callback();
		return;
	}
	// Not loaded yet; save the callback for later execution.
	OnArchiveReady.AddLambda([WeakCallbackOwner, Callback]()->void
	{
		if (WeakCallbackOwner.IsValid())
		{
			Callback();
		}
	});
}

void FOnArchiveLoadedCallbacks::SetMainMenu(UMainGameUI* MainMenu)
{
	M_MainGameUI = MainMenu;
}

bool FOnArchiveLoadedCallbacks::EnsureMainGameUIIsValid() const
{
	if( M_MainGameUI.IsValid() )
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("MainGameUI is not valid in FOnArchiveLoadedCallbacks");
	return false;
}
