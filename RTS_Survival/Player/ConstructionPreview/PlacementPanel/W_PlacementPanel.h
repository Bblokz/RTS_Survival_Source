// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlacementPanelTypes.h"

#include "W_PlacementPanel.generated.h"

class UBorder;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UVerticalBox;

/**
 * @brief Screen-space panel shown while the player places a building, field defense or trench: what is placed,
 * whether a click places it (or why not), its numbers and the keys. Builds itself in the Frontstahl HUD style.
 * Owners call SetContent and UpdateDocking every tick; the panel docks beside the projected bounds.
 */
UCLASS()
class RTS_SURVIVAL_API UW_PlacementPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** @brief Adds the panel to the viewport if needed and fades it in at its docked spot. */
	void ShowPanel();
	void HidePanel();
	bool GetIsPanelShown() const { return bM_IsShown; }

	void SetContent(const FPlacementPanelContent& Content);

	/**
	 * @brief Docks the panel beside what is placed. Owners call it every tick right after moving their preview, so
	 * the panel moves in the same frame (and in map tests, where widgets may not tick).
	 * @param AvoidWorldBounds What is being placed; the panel keeps off its screen rectangle. Invalid: above the HUD.
	 * @param DeltaTime Frame time of the follow slide.
	 */
	void UpdateDocking(const FBox& AvoidWorldBounds, const float DeltaTime);

	/** @return The screen rectangle the panel last kept off, in widget units; invalid when nothing was projected. */
	FBox2D GetLastAvoidScreenRect() const { return M_LastAvoidScreenRect; }

	/** @return Where the panel is drawn now, in widget units. */
	FBox2D GetPanelScreenRect() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> M_TitleText;

	UPROPERTY()
	TObjectPtr<UImage> M_StatusLamp;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_StatusText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> M_StatsBox;

	UPROPERTY()
	TObjectPtr<UImage> M_HintsGroove;

	UPROPERTY()
	TObjectPtr<UVerticalBox> M_HintsBox;

	// Label and value texts of the stat rows built so far; rows beyond the shown count are collapsed, not removed.
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> M_StatLabels;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> M_StatValues;

	UPROPERTY()
	TArray<TObjectPtr<UHorizontalBox>> M_StatRows;

	// The hints are rebuilt only when their keys or actions change; this is what the shown ones were built from.
	FString M_ShownHintsSignature;

	// What the owner last asked the panel to keep off; projected again on every docking as the camera moves.
	FBox M_AvoidWorldBounds = FBox(ForceInit);
	FBox2D M_LastAvoidScreenRect = FBox2D(ForceInit);
	FVector2D M_CurrentTopLeft = FVector2D::ZeroVector;
	EPlacementPanelDock M_CurrentDock = EPlacementPanelDock::Right;
	// The first docking after ShowPanel jumps to its spot instead of sliding in from the old one.
	bool bM_HasDockedSinceShown = false;
	bool bM_IsShown = false;
	float M_FadeAlpha = 0.f;

	void BuildWidgetTree();
	UVerticalBox* BuildPlate();
	void BuildHeader(UVerticalBox& Content);
	UImage* AddGroove(UVerticalBox& Content);
	void AddStatRow();
	void SetStats(const TArray<FPlacementPanelStatRow>& Stats);
	void SetKeyHints(const TArray<FPlacementPanelKeyHint>& KeyHints);
	UHorizontalBox* MakeKeyHint(const FPlacementPanelKeyHint& KeyHint);
	UBorder* MakeKeyCap(const FText& KeyText);
	UTextBlock* MakeText(const int32 FontSize, const FName Typeface, const FLinearColor& Color,
	                     const int32 LetterSpacing = 0);

	bool TryProjectAvoidRect(FBox2D& OutAvoidRect) const;
	FVector2D GetViewportSizeInWidgetUnits() const;
	void UpdateFade(const float DeltaTime);
};
