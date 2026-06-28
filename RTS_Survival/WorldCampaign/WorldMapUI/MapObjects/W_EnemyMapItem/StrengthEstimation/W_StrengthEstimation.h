#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DataAndUtils/RTSStrengthEstimationTypes.h"
#include "W_StrengthEstimation.generated.h"

class URichTextBlock;

UCLASS()
class RTS_SURVIVAL_API UW_StrengthEstimation : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "RTS|Strength Estimation")
	void SetupStrengthEstimation(const FRTSStrengthEstimationRichTextMessage& StrengthEstimationMessage);
	void ShowVisibleAnimation();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "World Campaign|Map Item")
	void BP_ShowVisibleAnimation();

	// By default contains <Text_NewBad>20%</>
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	URichTextBlock* M_EstimationsRichText = nullptr;

	// By default contains <Text_BadTitle>Total: </><Text_NewBad>45%</>
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	URichTextBlock* M_TotalRichText = nullptr;

private:
	bool GetIsValidEstimationsRichText() const;
	bool GetIsValidTotalRichText() const;
};
