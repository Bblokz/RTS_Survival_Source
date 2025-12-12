// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "W_OpenTechTree.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_OpenTechTree : public UUserWidget
{
	GENERATED_BODY()

protected:
	UFUNCTION(BlueprintCallable)
	void OnButtonClicked();

	virtual void NativeConstruct() override;

private:
	UPROPERTY()
	UMainGameUI* M_MainGameUI;

	bool GetIsValidMainGameUI();
};
