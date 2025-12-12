// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_EnumerateUnitSelection.generated.h"

class URichTextBlock;
class UButton;
class UW_SelectionPanel;

/**
 * @brief Page selector button used by the two-row selection panel.
 */
UCLASS()
class RTS_SURVIVAL_API UW_EnumerateUnitSelection : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitOwnerAndIndex(UW_SelectionPanel* InOwner, const int32 InPageIndex);
	void SetLabel(const FText& InText) const;
	int32 GetPageIndex() const { return M_PageIndex; }

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* EnumerationButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	URichTextBlock* EnumerationText;

	UFUNCTION(BlueprintCallable)
	void OnClickedEnumerate();

private:
	UPROPERTY()
	TWeakObjectPtr<UW_SelectionPanel> M_OwnerPanel;

	int32 M_PageIndex = 0;
};
