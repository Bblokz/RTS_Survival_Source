// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_Behaviour.generated.h"

struct FBehaviourUIData;
class UButton;
class UImage;
class UW_BehaviourContainer;
class UMainGameUI;


/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_Behaviour : public UUserWidget
{
	GENERATED_BODY()

void InitBehaviourWidget(UW_BehaviourContainer* InBehaviourContainer);
void SetupBehaviourWidget(const FBehaviourUIData& InBehaviourUIData );
protected:
	UPROPERTY(meta = (BindWidget))
	UImage* BehaviourImage;

	UPROPERTY(meta = (BindWidget))
	UButton* BehaviourButton;


private:
	void OnHoveredButton();
	void OnUnHoverButton();
	
};
