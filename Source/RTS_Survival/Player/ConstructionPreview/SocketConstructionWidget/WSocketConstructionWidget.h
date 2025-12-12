#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/RichTextBlock.h"
#include "WSocketConstructionWidget.generated.h"

/**
 * @brief Base for socket-indicator widgets. 
 */
UCLASS(Abstract, Blueprintable)
class RTS_SURVIVAL_API UWSocketConstructionWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    UPROPERTY(meta = (BindWidget))
    URichTextBlock* RichTextBlock_Content;
};
