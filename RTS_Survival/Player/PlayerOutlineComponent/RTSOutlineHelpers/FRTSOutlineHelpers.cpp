#include "FRTSOutlineHelpers.h"

#include "RTS_Survival/Player/PlayerOutlineComponent/RTSOutlineTypes/RTSOutlineTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void FRTSOutlineHelpers::SetRTSOutLineOnComponent(UPrimitiveComponent* ValidComponent, const ERTSOutLineTypes OutlineType)
{
	int32 StencilValue = -1;
	switch (OutlineType) {
	case ERTSOutLineTypes::None:
		ValidComponent->SetRenderCustomDepth(false);
		return;
	case ERTSOutLineTypes::Radixite:
			StencilValue = 1;	
		break;
	case ERTSOutLineTypes::Metal:
		StencilValue = 3;
		break;
	case ERTSOutLineTypes::VehicleParts:
		StencilValue = 5;
		break;
	}
	if(StencilValue <= 0)
	{
		const FString OwnerName = ValidComponent->GetOwner() ? ValidComponent->GetOwner()->GetName() : "NULL Owner";
		RTSFunctionLibrary::ReportError("Invalid stencil value for component: " +
			ValidComponent->GetName() +
			"\nof actor: " + OwnerName);
		ValidComponent->SetRenderCustomDepth(false);
		return;
	}
	ValidComponent->SetRenderCustomDepth(true);
	ValidComponent->SetCustomDepthStencilValue(StencilValue);	
}
