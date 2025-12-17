// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Player/Abilities.h"
#include "W_ItemActionUI.generated.h"

class UActionUIManager;
class UTextBlock;
class ACPPController;
enum class EAbilityID : uint8;
class UImage;
class UButton;
class USizeBox;
/**
 * @brief Widget representing a single action button in the action UI bar.
 * @note UpdateItemActionUI: call to refresh the displayed ability data for this slot.
 */
UCLASS()
class RTS_SURVIVAL_API UW_ItemActionUI : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Sets this action UI item to use the new provided ability.
	 * Will adjust slate in derived blueprint.
	 * @param NewAbility The ability to use for this action UI item.
	 * @param CoolDownRemaining
	 * @param CooldownTotalDuration
	 */
        void UpdateItemActionUI(const EAbilityID NewAbility, const int32 CustomType, const int32 CoolDownRemaining, const int32 CooldownTotalDuration);

	void InitActionUIElement(
		ACPPController* PlayerController,
		const int32 IndexActionUIElm, UActionUIManager* ActionUIManager);

protected:
	/**
	 * @brief Update the slate of this action UI button in blueprints.
	 * @param NewAbility The ability to use.
	 * @param CooldownRemaining
	 * @param CooldownDuration
	 */
	UFUNCTION(BlueprintImplementableEvent, Category ="UpdateActionUI")
        void OnUpdateActionUI(const EAbilityID NewAbility, const int32 CustomType, const int32 CooldownRemaining, const int32 CooldownDuration);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnActionUIClicked();
	
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnActionUIHover(const bool bIsHover) const;

	// The style for the button.
	UPROPERTY(EditAnywhere, Category = "Style")
	USlateWidgetStyleAsset* ButtonStyleAsset;


	// The size box that is the parent of all elements in this widget.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	USizeBox* M_ActionItemSizeBox;

	// The button of this widget that is underneath the size box in the hierarchy.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UButton* M_ActionItemButton;

	// The image of this widget that is a child of the button.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UImage* M_ActionItemImage;

	// The image of this widget that is a child of the button.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UTextBlock* M_ActionItemHotKeyText;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void UpdateButtonWithGlobalSlateStyle();


private:
        EAbilityID M_Ability;
        int32 M_CustomType = 0;

	UPROPERTY()
	TObjectPtr<ACPPController> M_PlayerController;

	int32 M_Index;

	UPROPERTY()
	TObjectPtr<UActionUIManager> M_ActionUIManager;

	/** @return Whether the player controller reference is valid.
	 * Will atempt to reset the reference if not valid. */
	bool GetIsValidPlayerController();
	bool GetIsValidActionUIManager() const;
};
