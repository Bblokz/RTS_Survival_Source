// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_BottomCenter_ChildPanel.generated.h"

class UW_BottomCenterUI;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_BottomCenter_ChildPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetParentWidget(UW_BottomCenterUI* NewParent);

	UFUNCTION(BlueprintImplementableEvent)
	void OnMainGameUIConstruct();
	

protected:
	TWeakObjectPtr<UW_BottomCenterUI> M_ParentWidget;
	bool EnsureValidParent()const;
	
	
	
};
