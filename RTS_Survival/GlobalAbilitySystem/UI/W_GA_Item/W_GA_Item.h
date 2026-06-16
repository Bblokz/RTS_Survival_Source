// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_GA_Item.generated.h"

class UGlobalAbility;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_GA_Item : public UUserWidget
{
	GENERATED_BODY()
	
	public:
	void SetupGa_Item(TWeakObjectPtr<UGlobalAbility> GlobalAbility);
	
private:
	UPROPERTY()
	TWeakObjectPtr<UGlobalAbility> M_GlobalAbility;
	[[nodiscard]] bool EnsureIsValidAbility();
	
};
