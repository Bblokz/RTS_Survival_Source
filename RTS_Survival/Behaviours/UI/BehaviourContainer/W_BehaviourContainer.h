// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_BehaviourContainer.generated.h"

class UW_Behaviour;
class UBehaviour;
class UBehaviourComp;
class UActionUIManager;
class UW_BehaviourDescription;
class UMainGameUI;
class UButton;
class UImage;
struct FBehaviourUIData;
/**
 * @brief Widget that groups behaviour entries and manages description visibility.
 */
UCLASS()
class RTS_SURVIVAL_API UW_BehaviourContainer : public UUserWidget
{
        GENERATED_BODY()

public:
	void InitBehaviourContainer(UW_BehaviourDescription* InBehaviourDescription, UActionUIManager* InActionUIManger);

	void OnAmmoPickerVisiblityChange(const bool bIsVisible);

        void SetupBehaviourContainerForSelectedUnit(UBehaviourComp* PrimarySelectedBehaviourComp);

        void OnBehaviourHovered(const bool bIsHovering, const FBehaviourUIData& BehaviourUIData);

        
protected:
        UFUNCTION(BlueprintCallable , NotBlueprintable)
        void SetBehaviourWidgets(TArray<UW_Behaviour*> Widgets);

private:
        UPROPERTY()
        TWeakObjectPtr<UActionUIManager> M_ActionUIManager;
        bool GetIsValidActionUIManager() const;

        UPROPERTY()
        TWeakObjectPtr<UW_BehaviourDescription> M_BehaviourDescription;
        bool GetIsValidBehaviourDescription() const;

        UPROPERTY()
        TWeakObjectPtr<UBehaviourComp> M_PrimaryBehaviourComponent;
        bool GetIsValidPrimaryBehaviourComponent() const;

        // Only visible if behaviour component is valid and there are any active behaviours.
        bool NeedsVisibility() const;

        UPROPERTY()
        TArray<TObjectPtr<UW_Behaviour>> M_BehaviourWidgets;


        void HideUnusedBehaviourWidgets(const int32 StartIndex);

        void SetupBehavioursOnWidgets(const TArray<UBehaviour*>& Behaviours);
};
