// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Player/Abilities.h"
#include "W_ItemActionUI.generated.h"

class UW_ActionUIDescription;
class UW_HotKey;
class UActionUIManager;
class UTextBlock;
class ACPPController;
class URTSHotkeyProviderSubsystem;
enum class EAbilityID : uint8;
class UImage;
class UButton;
class USizeBox;
class UMaterialInstanceDynamic;

/**
 * @brief Smooth cooldown fill of one action button; the ability entry only counts down in whole seconds,
 * so the widget anchors an estimated end time and animates the glass pane's CooldownRemaining between updates.
 */
USTRUCT()
struct FActionItemCooldownFillState
{
	GENERATED_BODY()

	// World time at which the shown cooldown ends; kept at the earliest estimate so mid-second UI updates
	// do not make the fill jump backwards.
	double CooldownEndWorldTime = 0.0;

	float CooldownDuration = 0.f;

	// Last value pushed to the material, so the parameter is only written when it changes.
	float AppliedRemainingFraction = 0.f;

	bool bIsCoolingDown = false;
};

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
	 * @param ActionUIDescription
	 */
	void InitActionUIElement(
		ACPPController* PlayerController,
		const int32 IndexActionUIElm, UActionUIManager* ActionUIManager,
		UW_ActionUIDescription* ActionUIDescription);

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

	// Glass pane over the icon (M_UI_GlassPane); its CooldownRemaining parameter shows the cooldown fill.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	UImage* M_GlassPane;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void UpdateButtonWithGlobalSlateStyle();

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	
	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	UW_ActionUIDescription* GetActionUIDescription() const;

private:
	EAbilityID M_Ability = EAbilityID::IdNoAbility;
	int32 M_CustomType = 0;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;
	
	UPROPERTY()
	TWeakObjectPtr<UW_ActionUIDescription> M_ActionUIDescription;

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

	// Stops a texture-less brush (empty slot) from being drawn as a solid white quad.
	void HideActionImageWithoutIcon() const;

	FActionItemCooldownFillState M_CooldownFill;

	// Dynamic instance of the glass pane material; created on the first cooldown.
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_GlassPaneMaterial;

	/**
	 * @brief Re-anchors the smooth fill on the whole-second cooldown the ability entry reports.
	 * @param bIsSameAbility Whether this slot showed the same ability before the update; a new ability always re-anchors.
	 * @param CooldownRemaining Whole seconds left on the cooldown.
	 * @param CooldownDuration Full cooldown length in seconds.
	 */
	void UpdateCooldownFill(const bool bIsSameAbility, const int32 CooldownRemaining, const int32 CooldownDuration);
	void StopCooldownFill();
	void ApplyCooldownFillAtTime(const double WorldTimeSeconds);
	void SetGlassPaneCooldownRemaining(const float RemainingFraction);
	UMaterialInstanceDynamic* GetGlassPaneMaterial();

	bool GetIsValidActionItemImage() const;
	bool GetIsValidActionItemHotKey() const;
	bool GetIsValidHotkeyProviderSubsystem() const;

	/** @return Whether the player controller reference is valid.
	 * Will atempt to reset the reference if not valid. */
	bool GetIsValidPlayerController();
	bool GetIsValidActionUIManager() const;
};
