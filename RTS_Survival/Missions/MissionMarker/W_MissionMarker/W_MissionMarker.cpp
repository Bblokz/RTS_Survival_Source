#include "W_MissionMarker.h"

#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"

void UW_MissionMarker::UpdateMarkerScreenPosition(const FVector2D& NewScreenPosition)
{
    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
    {
        CanvasSlot->SetPosition(NewScreenPosition);
    }
    else
    {
        // fallback: use UserWidget helper
        SetPositionInViewport(NewScreenPosition, /*bRemoveDPIScale=*/false);
    }
}

