// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_ArmyMenuLayout.generated.h"

class UW_WorldMenu;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_ArmyLayoutMenu : public UUserWidget
{
	GENERATED_BODY()
public:

	void InitArmyLayoutMenu(UW_WorldMenu* WorldMenu);


private:
	UPROPERTY()
	TWeakObjectPtr<UW_WorldMenu> M_WorldMenu;
	bool GetIsValidWorldMenu() const;
};
