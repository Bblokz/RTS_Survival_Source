// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_BehaviourContainer.generated.h"

class UW_Behaviour;
class UBehaviourComp;
class UActionUIManager;
class UW_BehaviourDescription;
class UMainGameUI;
class UButton;
class UImage;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_BehaviourContainer : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitBehaviourContainer(UW_BehaviourDescription* InBehaviourDescription, UActionUIManager* InActionUIManger);

	void OnAmmoPickerVisiblityChange(const bool bIsVisible);

	void SetupBehaviourContainerForSelectedUnit(UBehaviourComp* PrimarySelectedBehaviourComp);

private:
	UPROPERTY()
	UActionUIManager* M_ActionUIManager = nullptr;
	bool GetIsValidActionUIManager()const;
	
	UPROPERTY()
	UW_BehaviourDescription* M_BehaviourDescription = nullptr;
	bool GetIsValidBehaviourDescription()const;

	UPROPERTY()
	TWeakObjectPtr<UBehaviourComp> M_PrimaryBehaviourComponent;

	// Only visible if behaviour component is valid and there are any active behaviours.
	bool NeedsVisibility() const;

	UPROPERTY()
	TArray<UW_Behaviour> M_BehaviourWidgets;
};
