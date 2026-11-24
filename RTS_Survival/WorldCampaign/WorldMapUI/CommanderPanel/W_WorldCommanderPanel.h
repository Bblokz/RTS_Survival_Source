// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "RTS_Survival/GlobalAbilitySystem/RTSCommanders/RTSCommander.h"
#include "RTS_Survival/WorldCampaign/CommanderMapKit/WorldCommanderMapKitSettings.h"
#include "RTS_Survival/WorldCampaign/CommanderMapKit/WorldCommanderMapKitTypes.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CommanderPanel/WorldCommanderPanelRules.h"

#include "W_WorldCommanderPanel.generated.h"

class AActor;
class AWorldPlayerController;
class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UWorldCommanderMapKit;
class UWidget;
struct FWorldCampaignUILook;

/** Why the hover plates of the panel are shown. */
struct FWorldCommanderPanelPlateState
{
	bool bIsAbilityHovered = false;
	bool bIsPassiveHovered = false;
	// Set by the capture hook; keeps the ability plate open without a mouse.
	bool bIsAbilityPlatePinned = false;
	// A failed use keeps the ability plate open for a moment to show why.
	bool bIsShowingFailReason = false;
};

/**
 * @brief The campaign commander in the bottom-left corner of the world map: portrait, name, one round map ability
 * button and the passive. The campaign HUD creates it, calls InitCommanderPanel once and RefreshFromCampaign at the
 * start of every player turn and after a load. While it picks a target the controller routes map clicks to it.
 */
UCLASS()
class RTS_SURVIVAL_API UW_WorldCommanderPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Builds the widget tree with the shared look and binds to the commander map kit of the controller.
	 * @param Controller The world player controller that owns the commander map kit.
	 * @param Look The campaign HUD's loaded look; only read here, not kept.
	 */
	void InitCommanderPanel(AWorldPlayerController* Controller, const FWorldCampaignUILook* Look);

	/** Reads commander, kit and cooldown again; collapses the panel when the campaign has no commander. */
	void RefreshFromCampaign();

	// --- Target picking ---

	bool GetIsPickingTarget() const { return bM_IsPickingTarget; }

	EWorldCommanderAbilityTargetKind GetPickingTargetKind() const { return M_PickingTargetKind; }

	void CancelTargetPicking();

	/**
	 * @brief The controller hands every map click here while the panel picks a target. A wrong kind of click keeps
	 * the panel picking and shows the reason on the prompt.
	 * @param ClickedActor Actor under the cursor; null when nothing was hit (no location either).
	 * @param ClickedWorldLocation Hit location; read only when ClickedActor is valid.
	 * @param OutFailReason Player-facing reason when the ability was not used.
	 * @return true when the ability was used and picking ended.
	 */
	bool TryUseAbilityOnPickedActor(AActor* ClickedActor, const FVector& ClickedWorldLocation, FText& OutFailReason);

	/** While blocked (enemy turn, cinematic) the ability button ignores clicks and picking ends. */
	void SetIsInputBlocked(const bool bIsBlocked);

	// --- Capture hooks for the automated screenshot run ---

	void ShowAbilityTooltipForCapture(const bool bShow);

	/** Uses the ability on Target as a click would, without picking; a failure shows on the plate. */
	bool UseAbilityForCapture(const FWorldCommanderMapAbilityTarget& Target);

protected:
	// Builds only the root box and canvas; InitCommanderPanel fills them once it has the look.
	virtual void NativeOnInitialized() override;

	virtual void NativeDestruct() override;

	// Only the buttons and the pick prompt can be hit; a click on them never falls through to the map as a pick.
	// A right click there cancels picking like a right click on the map.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	// --- Build (InitCommanderPanel) ---
	void BuildWidgetTree(const FWorldCampaignUILook& Look);
	void BuildPortrait();
	void BuildNamePlate(const FWorldCampaignUILook& Look);
	void BuildAbilityButton(const FWorldCampaignUILook& Look);
	UWidget* BuildAbilityGlyph(const FWorldCampaignUILook& Look);
	void BuildPassivePlate(const FWorldCampaignUILook& Look);
	void BuildAbilityHoverPlate(const FWorldCampaignUILook& Look);
	void BuildPassiveHoverPlate(const FWorldCampaignUILook& Look);
	void BuildPickPrompt(const FWorldCampaignUILook& Look);
	/**
	 * @brief A collapsed plate of fixed width that grows upward from its bottom-left corner as its text wraps.
	 * @param BorderColor Outline of the plate.
	 * @param PlateWidth Width in UI units; the text inside wraps to it.
	 * @param BottomLeft Bottom-left corner on the panel canvas.
	 * @return The plate border to fill and show.
	 */
	UBorder* MakeHoverPlate(const FLinearColor& BorderColor, float PlateWidth, const FVector2D& BottomLeft);

	// --- Refresh ---
	void RefreshPortraitAndName();
	void RefreshPassive();
	void RefreshAbilityButton();
	void RefreshPlates();
	void RefreshAbilityHoverPlateTexts();

	EWorldCommanderAbilityButtonState GetAbilityButtonState() const;
	bool GetHasAbility() const;
	int32 GetCooldownTurnsRemaining() const;

	// --- Use ---
	void BeginTargetPicking(const EWorldCommanderAbilityTargetKind Kind);
	bool UseAbilityOnTarget(const FWorldCommanderMapAbilityTarget& Target, FText& OutFailReason);
	FWorldCommanderPickCandidate MakePickCandidate(AActor* ClickedActor, const FVector& ClickedWorldLocation) const;
	void ShowFailReason(const FText& FailReason);
	void HideFailReason();

	UFUNCTION()
	void OnAbilityButtonClicked();

	UFUNCTION()
	void OnAbilityButtonHovered();

	UFUNCTION()
	void OnAbilityButtonUnhovered();

	UFUNCTION()
	void OnPassiveHovered();

	UFUNCTION()
	void OnPassiveUnhovered();

	UFUNCTION()
	void OnCancelPickingClicked();

	UFUNCTION()
	void OnMapAbilityUsed(EWorldCommanderMapAbility Ability, const FWorldCommanderMapAbilityTarget& Target);

	UFUNCTION()
	void OnMapAbilityCooldownChanged(int32 TurnsRemaining);

	bool GetIsValidController() const;
	bool GetIsValidCommanderMapKit() const;
	bool GetIsValidCanvas() const;

	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_Controller;

	// Component on the controller; read on every refresh so a restored save is always shown.
	UPROPERTY()
	TWeakObjectPtr<UWorldCommanderMapKit> M_CommanderMapKit;

	// Copy of the commander's kit from the last refresh; valid only while bM_HasKit.
	UPROPERTY()
	FWorldCommanderMapKitDefinition M_Kit;

	bool bM_HasKit = false;

	ERTSCommander M_ShownCommander = ERTSCommander::NoCommander;

	bool bM_IsPickingTarget = false;

	EWorldCommanderAbilityTargetKind M_PickingTargetKind = EWorldCommanderAbilityTargetKind::None;

	bool bM_IsInputBlocked = false;

	FWorldCommanderPanelPlateState M_PlateState;

	FTimerHandle M_HideFailReasonTimer;

	// Set once InitCommanderPanel built the widget tree; every widget member below is valid from then on.
	bool bM_IsTreeBuilt = false;

	// The look's glass dome, kept because the ability button is restyled on every state change.
	UPROPERTY()
	FSlateBrush M_GlassDomeBrush;

	// Loaded from the commander settings; the portrait brush holds it too.
	UPROPERTY()
	TObjectPtr<UTexture2D> M_PortraitTexture;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> M_Canvas;

	UPROPERTY()
	TObjectPtr<UImage> M_PortraitImage;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_NameText;

	// Soft disc behind the ability button while it is lit.
	UPROPERTY()
	TObjectPtr<UImage> M_AbilityHalo;

	UPROPERTY()
	TObjectPtr<UButton> M_AbilityButton;

	UPROPERTY()
	TObjectPtr<UImage> M_AbilityGlyphRing;

	UPROPERTY()
	TObjectPtr<UImage> M_AbilityGlyphDot;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_AbilityCooldownText;

	UPROPERTY()
	TObjectPtr<USizeBox> M_PassivePlateBox;

	UPROPERTY()
	TObjectPtr<UButton> M_PassiveButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_PassiveNameText;

	UPROPERTY()
	TObjectPtr<UBorder> M_AbilityHoverPlate;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_AbilityHoverNameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_AbilityHoverDescriptionText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_AbilityHoverStateText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_AbilityHoverFailText;

	UPROPERTY()
	TObjectPtr<UBorder> M_PassiveHoverPlate;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_PassiveHoverNameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_PassiveHoverDescriptionText;

	UPROPERTY()
	TObjectPtr<UBorder> M_PickPromptPlate;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_PickPromptText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_PickFailText;

	UPROPERTY()
	TObjectPtr<UButton> M_CancelPickingButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CancelPickingLabel;
};
