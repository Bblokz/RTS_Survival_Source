// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Player/Abilities.h"
#include "W_ItemActionUI.generated.h"

class UW_HotKey;
class UActionUIManager;
class UTextBlock;
class ACPPController;
class URTSHotkeyProviderSubsystem;
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
	void UpdateItemActionUI(const EAbilityID NewAbility, const int32 CustomType, const int32 CoolDownRemaining,
	                        const int32 CooldownTotalDuration);

	/**
	 * @brief Caches the owning references and pulls the current hotkey text for this slot.
	 * @param PlayerController Owning player controller for input mapping access.
	 * @param IndexActionUIElm Index of this action UI slot.
	 * @param ActionUIManager Manager that owns the action UI elements.
	 */
	void InitActionUIElement(
		ACPPController* PlayerController,
		const int32 IndexActionUIElm, UActionUIManager* ActionUIManager);

	void SetActionButtonHotkeyHidden(const bool bHideActionButtonHotkey) const;

protected:
	/**
	 * @brief Update the slate of this action UI button in blueprints.
	 * @param NewAbility The ability to use.
	 * @param CooldownRemaining
	 * @param CooldownDuration
	 */
	UFUNCTION(BlueprintImplementableEvent, Category ="UpdateActionUI")
	void OnUpdateActionUI(const EAbilityID NewAbility, const int32 CustomType, const int32 CooldownRemaining,
	                      const int32 CooldownDuration);

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

	// The hotkey of this action ui item.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UW_HotKey* M_ActionItemHotKey;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void UpdateButtonWithGlobalSlateStyle();

	virtual void NativeDestruct() override;

private:
	EAbilityID M_Ability;
	int32 M_CustomType = 0;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	int32 M_Index = 0;

	UPROPERTY()
	TWeakObjectPtr<UActionUIManager> M_ActionUIManager;

	UPROPERTY()
	TWeakObjectPtr<URTSHotkeyProviderSubsystem> M_HotkeyProviderSubsystem;

	FDelegateHandle M_ActionSlotHotkeyHandle;

	void CacheHotkeyProviderSubsystem();
	void BindHotkeyUpdateDelegate();
	void UnbindHotkeyUpdateDelegate();
	void UpdateHotkeyFromProvider();
	void HandleActionSlotHotkeyUpdated(const int32 ActionSlotIndex, const FText& HotkeyText);

	bool GetIsValidActionItemHotKey() const;
	bool GetIsValidHotkeyProviderSubsystem() const;

	/** @return Whether the player controller reference is valid.
	 * Will atempt to reset the reference if not valid. */
	bool GetIsValidPlayerController();
	bool GetIsValidActionUIManager() const;
};
