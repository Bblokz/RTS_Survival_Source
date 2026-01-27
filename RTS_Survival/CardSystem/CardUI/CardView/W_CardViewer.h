// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_CardViewer.generated.h"

enum class ERTSCard : uint8;
class UW_RTSCard;
/**
 * @brief Shows a single card preview widget when card hover actions request it.
 */
UCLASS()
class RTS_SURVIVAL_API UW_CardViewer : public UUserWidget
{
	GENERATED_BODY()
public:
	void ViewCard(ERTSCard Card) const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TWeakObjectPtr<UW_RTSCard> RTSCard;

private:
	bool GetIsValidRTSCard() const;
};
