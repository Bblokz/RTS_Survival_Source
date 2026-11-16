// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/GameUI/GlassButtons/GlassHoverAnimator.h"

#include "W_TacticalMapMissionCard.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UOverlay;
class URichTextBlock;
class UVerticalBox;
class UW_Mission;

/**
 * @brief One active mission in the tactical map's mission panel: the mission's title and full description on a pane
 * of the faction button glass (the shared M_UI_GlassPane shader). Hovering lights the glass, sends one light band
 * across it and brightens the accent bar on the left. The texts copy the rich text styles of the mission widget they
 * mirror, so tags such as <Text_Armor> look the same as in the HUD. UW_FullMapView pools these; builds its own tree.
 */
UCLASS()
class RTS_SURVIVAL_API UW_TacticalMapMissionCard : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Gives the card its own glass scratches, so stacked cards do not look copied.
	 * @param CardIndex Position of the card in the panel.
	 */
	void SetupMissionCard(const int32 CardIndex);

	/**
	 * @brief Shows the mission a HUD mission widget shows; copies that widget's text styles on the first call.
	 * @param MissionWidget An in-use mission widget of the mission widget manager.
	 */
	void ShowMission(const UW_Mission& MissionWidget);

	/**
	 * @brief Captures and tests hover the card without a mouse.
	 * @param bForceHover Lights the glass as if the mouse were over the card.
	 * @param FrozenSweepProgress Holds the light band at this progress (0..1); negative lets it run.
	 */
	void SetForcedHover(const bool bForceHover, const float FrozenSweepProgress = -1.f);

	/** @return The hover strength last sent to the glass (0 idle, 1 fully lit). */
	float GetGlassHoverAmount() const { return M_GlassAnimator.GetHover(); }

	/** @return The plain title text, for the capture log. */
	FString GetShownTitle() const { return M_ShownTitle.ToString(); }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UImage> M_GlassPane;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_GlassMaterial;

	// Thin bar left of the texts; brightens with the hover.
	UPROPERTY()
	TObjectPtr<UImage> M_AccentBar;

	UPROPERTY()
	TObjectPtr<UVerticalBox> M_TextColumn;

	// Created on the first ShowMission with the class and styles of the mission widget's text blocks.
	UPROPERTY()
	TObjectPtr<URichTextBlock> M_TitleText;

	UPROPERTY()
	TObjectPtr<URichTextBlock> M_DescriptionText;

	FGlassHoverAnimator M_GlassAnimator;
	FGlassHoverTimings M_GlassTimings;

	FText M_ShownTitle;
	FText M_ShownDescription;

	bool bM_IsForcedHover = false;

	// Rim, streak and tint last sent, so an idle card skips the parameter writes.
	float M_SentRimHover = -1.f;

	void BuildWidgetTree();
	void BuildGlassPane(UOverlay& CardOverlay);
	void BuildContent(UOverlay& CardOverlay);

	/**
	 * @brief Creates a text block of the source's class with the source's style set, decorators and default style.
	 * @param Source The mission widget's title or description block.
	 * @param bIsTitle Adds the gap below the title on the description only.
	 * @return The new block in the text column, or null when the column is missing.
	 */
	URichTextBlock* CreateTextLike(const URichTextBlock& Source, const bool bIsTitle);

	static void CopyRichTextLook(const URichTextBlock& Source, URichTextBlock& Target);

	void SetTextIfChanged(URichTextBlock* TextBlock, FText& InOutShownText, const FText& NewText) const;

	void TickGlass(const float DeltaSeconds);
};
