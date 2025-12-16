// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Behaviours/UI/BehaviourUIData.h"
#include "W_Behaviour.generated.h"

struct FBehaviourUIData;
class UButton;
class UImage;
class UW_BehaviourContainer;
class UBehaviourButtonSettings;

/**
 * @brief Widget representing a single behaviour entry in the behaviour container.
 */
UCLASS()
class RTS_SURVIVAL_API UW_Behaviour : public UUserWidget
{
        GENERATED_BODY()

public:
        void InitBehaviourWidget(UW_BehaviourContainer* InBehaviourContainer);
        void SetupBehaviourWidget(const FBehaviourUIData& InBehaviourUIData);

protected:
        UPROPERTY(meta = (BindWidget))
        UImage* BehaviourImage;

        UPROPERTY(meta = (BindWidget))
        UButton* BehaviourButton;

private:
        UPROPERTY()
        TWeakObjectPtr<UW_BehaviourContainer> M_BehaviourContainer;

        FBehaviourUIData M_BehaviourUIData;

        bool GetIsValidBehaviourContainer() const;

        static const UBehaviourButtonSettings* GetBehaviourButtonSettings();

        void ApplyBehaviourIcon();

        UFUNCTION()
        void OnHoveredButton();

        UFUNCTION()
        void OnUnHoverButton();
};
