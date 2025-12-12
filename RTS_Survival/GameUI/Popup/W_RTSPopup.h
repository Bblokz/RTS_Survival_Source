// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/RichTextBlock.h"
#include "W_RTSPopup.generated.h"

class IPopupCaller;
enum class ERTSPopup : uint8;
class UTextBlock;
class UButton;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_RTSPopup : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitPopupWidget(const TWeakInterfacePtr<IPopupCaller>& PopupCaller);

	void SetPopupType(const ERTSPopup NewPopup, const FText& NewTitle, const FText& NewMessage);

protected:
	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	URichTextBlock* RichPopupText;

	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	UButton* LeftBtn;

	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	UButton* MiddleBtn;

	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	UButton* RightBtn;

	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	UTextBlock* TitleText;

	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	UTextBlock* LeftText;

	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	UTextBlock* RightText;

	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	UTextBlock* MiddleText;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnClickLeftBtn();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnClickMiddleBtn();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnclickRightBtn();

private:
	UPROPERTY()
	ERTSPopup M_CurrentPopup;

	TWeakInterfacePtr<IPopupCaller> M_PopupCaller;

	bool GetIsValidRichText();

	void SetupOkBackPopup();
	void SetupOkCanclePopup();

	bool GetIsValidPopupCaller();
};
