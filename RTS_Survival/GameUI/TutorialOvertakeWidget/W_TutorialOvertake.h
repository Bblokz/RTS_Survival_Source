// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_TutorialOvertake.generated.h"

class ACPPController;
class UMainGameUI;
/**
 * Use OverTakeUI to make only this widget display and force pause the game.
 * call OnClickedContinue to continue the game and remove this widget.
 */
UCLASS()
class RTS_SURVIVAL_API UW_TutorialOvertake : public UUserWidget 
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OvertakeUI(const UObject* WorldContextObject);

protected:
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnClickedContinue();

private:
	TWeakObjectPtr<UMainGameUI> M_MainGameUI;
	bool EnsureMainGameUIIsValid()const;
	TWeakObjectPtr<ACPPController> M_PlayerController;
	bool EnsurePlayerControllerIsValid() const;
};
