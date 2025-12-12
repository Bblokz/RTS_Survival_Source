#pragma once
#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "W_MissionMarker.generated.h"
class UImage;

UCLASS()
class UW_MissionMarker : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 
     * Move the widget to the given screen-space position (in pixels, top-left origin).
     * This is called every tick by the marker actor. 
     */
	void UpdateMarkerScreenPosition(const FVector2D& NewScreenPosition);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> M_MarkerImage;
};
