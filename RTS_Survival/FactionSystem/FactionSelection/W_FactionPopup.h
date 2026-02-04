// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_FactionPopup.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFactionPopupAccepted);

/**
 * @brief Widget shown before the faction selection menu to gate player confirmation.
 */
UCLASS()
class RTS_SURVIVAL_API UW_FactionPopup : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FFactionPopupAccepted OnPopupAccepted;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_AcceptButton = nullptr;

private:
	UFUNCTION()
	void HandleAcceptClicked();

	bool GetIsValidAcceptButton() const;
};
