#include "HealthBarWidgetCallbacks.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Components/WidgetComponent.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"

void FHealthBarWidgetCallbacks::CallbackOnHealthBarReady(
    TFunction<void(UHealthComponent* HealthComp)> Callback,
    TWeakObjectPtr<UObject> WeakCallbackOwner)
{
    if (!EnsureHealthComponentIsValid() || !WeakCallbackOwner.IsValid())
    {
        return;
    }

    UHealthComponent* HealthComp = M_HealthComponent.Get();
    if (!IsValid(HealthComp))
    {
        return;
    }

    // If the widget already exists, invoke immediately with the component ref.
    if (IsValid(HealthComp->GetHealthBarWidgetComp()))
    {
        Callback(HealthComp);
        return;
    }

    // Otherwise, register to be called later (use the param from the delegate, don't capture a strong ptr).
    OnHealthBarWidgetReady.AddLambda([WeakCallbackOwner, Callback](UHealthComponent* InHealthComp)
    {
        if (WeakCallbackOwner.IsValid() && IsValid(InHealthComp))
        {
            Callback(InHealthComp);
        }
    });
}

void FHealthBarWidgetCallbacks::SetOwningHealthComponent(UHealthComponent* InHealthComp)
{
	M_HealthComponent = InHealthComp;
}


bool FHealthBarWidgetCallbacks::EnsureHealthComponentIsValid() const
{
	if (M_HealthComponent.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("UHealthComponent is not valid in FHealthBarWidgetCallbacks");
	return false;
}
