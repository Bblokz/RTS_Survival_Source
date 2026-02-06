// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_WorldPerkGraph.generated.h"

class UWidgetSwitcher;
struct FPlayerPerkProgress;
class UW_PerkGraphItem;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_WorldPerkGraph : public UUserWidget
{
	GENERATED_BODY()
public:
	void OnPerkOverView(FPlayerPerkProgress PlayerProgress);

protected:
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_PerkGraphItem> M_PerkGraphItem_0;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_PerkGraphItem> M_PerkGraphItem_1;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_PerkGraphItem> M_PerkGraphItem_2;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_PerkGraphItem> M_PerkGraphItem_3;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_PerkGraphItem> M_PerkGraphItem_4;
	

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnOpenPerkOverview(FPlayerPerkProgress PlayerProgress);
};
