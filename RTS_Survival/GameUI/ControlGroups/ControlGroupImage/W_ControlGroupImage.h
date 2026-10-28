// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "W_ControlGroupImage.generated.h"

class UBorder;
class UButton;
class UImage;
class UMaterialInstanceDynamic;
class UTextBlock;
class UW_HotKey;
class URTSHotkeyProviderSubsystem;
class ULocalPlayer;
struct FTrainingOption;
enum class EAllUnitType : uint8;

/**
 * @brief How the glass pane (M_UI_GlassPane) over a control group slot reacts; tweak in the blueprint's Class Defaults.
 * The hover lives entirely in the shader: the widget only ramps the Hover, HoverSweep and Pressed scalars.
 */
USTRUCT(BlueprintType)
struct FControlGroupGlassSettings
{
	GENERATED_BODY()

	// Seconds for the hover glow to fade in.
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.01"))
	float HoverInSeconds = 0.12f;

	// Seconds for the hover glow to fade out.
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.01"))
	float HoverOutSeconds = 0.22f;

	// Seconds the light band takes to cross the pane once per mouse enter.
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.01"))
	float SweepSeconds = 0.26f;

	// Seconds for the pressed shade to come and go.
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.01"))
	float PressSeconds = 0.05f;

	// Hover strength the glass jumps to when the group's hotkey recalls it, fading out over RecallFlashSeconds.
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RecallFlashHover = 0.35f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.01"))
	float RecallFlashSeconds = 1.2f;

	// An empty group's glass and hover are weaker, so the occupied groups stand out.
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EmptyPaneOpacity = 0.8f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EmptyHoverStrength = 0.6f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EmptyHotkeyOpacity = 0.45f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OccupiedHotkeyOpacity = 0.85f;
};

/** Hover, sweep and press values last written to the glass pane's dynamic material. */
USTRUCT()
struct FControlGroupGlassAnimState
{
	GENERATED_BODY()

	// Linear 0..1 hover ramp; the shader gets it eased.
	float M_HoverRamp = 0.0f;

	// 0..1 progress of the light band; 1 when no sweep is playing.
	float M_SweepProgress = 1.0f;

	float M_PressRamp = 0.0f;

	// 1 right after a recall, fading to 0.
	float M_RecallFlash = 0.0f;

	bool bM_WasHovered = false;

	// Last values sent to the material, so idle slots skip the parameter writes.
	float M_SentHover = -1.0f;
	float M_SentSweep = -1.0f;
	float M_SentPressed = -1.0f;
};

/**
 * @brief One slot of the control group bar: the most frequent unit of the group under a scratched glass pane, with
 * the group's hotkey and unit count. Each slot gets its own wear on the glass and a shader-driven hover (no sound).
 * Clicking the slot selects the group like its hotkey (shift adds, control assigns).
 * Glass, icon, count, hotkey tab and button are optional bindings (M_GlassPane, M_TrainingItemImage,
 * M_UnitCountText, M_HotkeyTab, M_ControlGroupButton); without them the slot still shows its hotkey.
 * @note OnUpdateControlGroup: implement in blueprint to show the unit's icon (only called for occupied groups).
 */
UCLASS()
class RTS_SURVIVAL_API UW_ControlGroupImage : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Initializes the control group image and hotkey subscription for this slot.
	 * @param OwningLocalPlayer Local player used to access the hotkey provider subsystem.
	 * @param GroupIndex Index of this control group widget in the UI.
	 */
	void InitControlGroupImage(ULocalPlayer* OwningLocalPlayer, const int32 GroupIndex);

	/**
	 * @brief Switches the slot between empty and occupied and shows the group's size.
	 * @param MostFrequentUnitInGroup None for an empty group.
	 * @param UnitCount Number of units in the group.
	 */
	void UpdateControlGroup(const FTrainingOption& MostFrequentUnitInGroup, const int32 UnitCount);

	int32 GetGroupIndex() const { return M_GroupIndex; }

	// The group's hotkey selected it: the glass glows briefly (part of the hover look, no light band).
	void PlayRecallFlash();

	UFUNCTION(BlueprintImplementableEvent)
	void OnUpdateControlGroup(const FTrainingOption MostFrequentUnitInGroup);

#if !UE_BUILD_SHIPPING
	// Captures (UISnapshot) hold the light band of hovered slots at this progress; negative plays it normally.
	static float Debug_FrozenSweepProgress;
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, Category = "ControlGroup|Glass")
	FControlGroupGlassSettings M_GlassSettings;

	// Optional glass pane (M_UI_GlassPane instance) above the icon; gets a dynamic instance with this slot's wear.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UImage> M_GlassPane;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UImage> M_TrainingItemImage;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UTextBlock> M_UnitCountText;

	// Dark tab behind the hotkey digit so it reads over the unit icon; transparent on empty slots.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UBorder> M_HotkeyTab;

	// Clicking it uses the group like its hotkey; its hover and pressed state show in the glass shader.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UButton> M_ControlGroupButton;

private:
	int32 M_GroupIndex = 0;

	// Units in the group at the last update; 0 = empty slot.
	int32 M_UnitCount = 0;

	UPROPERTY()
	TWeakObjectPtr<URTSHotkeyProviderSubsystem> M_HotkeyProviderSubsystem;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_GlassMaterial;

	FControlGroupGlassAnimState M_GlassAnim;

	FDelegateHandle M_ControlGroupHotkeyHandle;

	void CacheHotkeyProviderSubsystem(ULocalPlayer* OwningLocalPlayer);
	void BindHotkeyUpdateDelegate();
	void UnbindHotkeyUpdateDelegate();
	void UpdateControlGroupHotkey();
	void HandleControlGroupHotkeyUpdated(const int32 GroupIndex, const FText& HotkeyText);

	// Gives this slot's glass its own scratches, reflection and dust; the rim and inset stay shared.
	void ApplySlotWear();
	void BindButtonClick();

	UFUNCTION()
	void OnControlGroupButtonClicked();

	void ApplyOccupiedState(const int32 UnitCount);
	void TickGlassAnimation(const float DeltaSeconds);
	void SendGlassAnimToMaterial();

	UMaterialInstanceDynamic* GetGlassMaterial();
	bool GetIsValidControlGroupHotKey() const;
	bool GetIsValidHotkeyProviderSubsystem() const;

public:
	// The hotkey of this control group ui item.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UW_HotKey* M_ControlGroupHotKey;
};
