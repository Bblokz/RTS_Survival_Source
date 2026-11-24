// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/WorldUIFocusState/WorldUIFocusState.h"
#include "W_WorldCampaignBar.generated.h"

class AWorldPlayerController;
class UButton;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UWidget;
struct FSlateColor;
struct FWorldCampaignUILook;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldCampaignBarNavigate, EWorldUIFocusState /*RequestedFocus*/);

/**
 * @brief The thin strip across the top of the world map (UI sketch sheet 7): turn, rank, ability tier, missions to
 * the next tier, the enemy's freeze progress, radixite, blueprints and the buttons to the full-screen pages.
 * Builds its own widget tree; created and placed by UW_WorldCampaignHUD.
 */
UCLASS()
class RTS_SURVIVAL_API UW_WorldCampaignBar : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Builds the strip and starts showing the campaign's numbers.
	 * @param WorldPlayerController Controller that owns the campaign managers.
	 * @param Look Shared art of the campaign screens; owned by the HUD and must outlive this widget.
	 */
	void InitCampaignBar(AWorldPlayerController* WorldPlayerController, const FWorldCampaignUILook* Look);

	/** Reads every number again; also runs a few times per second so no event can be missed. */
	void RefreshFromCampaign();

	/** A page button was pressed; the HUD passes it on to the world menu. */
	FOnWorldCampaignBarNavigate& OnNavigateRequested() { return M_OnNavigateRequested; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildWidgetTree();
	void BuildWidgetTree_LeftGroup(UHorizontalBox& Row);
	void BuildWidgetTree_TierCell(UHorizontalBox& Row);
	void BuildWidgetTree_FreezeBar(UHorizontalBox& Row);
	void BuildWidgetTree_FreezeTicks();
	void BuildWidgetTree_RightGroup(UHorizontalBox& Row);

	/** The shoulder board shown left of the rank text, sized to the value row of a cell. */
	UWidget* BuildRankInsignia();

	/**
	 * @brief Adds a small caps label with a value under it, the repeating cell of the strip.
	 * @param Row Row to add the cell to.
	 * @param Label Caption above the value.
	 * @param OutValue Receives the value text block.
	 * @param IconBrush Optional icon left of the value.
	 * @param OutSecondary Optional second, smaller text right of the value (income per turn).
	 * @param LeadingWidget Optional widget at the start of the value row (the rank insignia).
	 */
	void AddCell(UHorizontalBox& Row, const FText& Label, TObjectPtr<UTextBlock>& OutValue,
	             const struct FSlateBrush* IconBrush = nullptr, TObjectPtr<UTextBlock>* OutSecondary = nullptr,
	             UWidget* LeadingWidget = nullptr);

	UTextBlock* MakeCellLabel(const FText& Label, const FSlateColor& Color);

	/**
	 * @brief Puts a label and any value widget in the bolted metal frame, so every cell of the strip has the same
	 * frame, height and value centre line whatever it shows (text, pips, the freeze track).
	 * @param Row Row to add the cell to.
	 * @param Label Caption above the value.
	 * @param ValueContent Widget shown under the caption, left aligned and vertically centred in the value row.
	 */
	void AddFramedCell(UHorizontalBox& Row, UTextBlock* Label, UWidget* ValueContent);
	void AddGap(UHorizontalBox& Row, float Width) const;
	void AddPageButton(UHorizontalBox& Row, const FText& Label, TObjectPtr<UButton>& OutButton);

	void RefreshRankAndTier();
	void RefreshFreeze();
	void RefreshResources();

	UFUNCTION()
	void HandleArmyClicked();

	UFUNCTION()
	void HandleTechClicked();

	UFUNCTION()
	void HandleArchiveClicked();

	bool GetIsValidWorldPlayerController() const;

	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_WorldPlayerController;

	// Owned by the HUD that created this widget.
	const FWorldCampaignUILook* M_Look = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TurnText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_RankText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TierText;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> M_TierPips;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_NextTierText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_FreezeLabel;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> M_FreezeCanvas;

	UPROPERTY()
	TObjectPtr<UImage> M_FreezeFill;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_RadixiteText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_RadixiteIncomeText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_BlueprintText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_BlueprintIncomeText;

	UPROPERTY()
	TObjectPtr<UButton> M_ArmyButton;

	UPROPERTY()
	TObjectPtr<UButton> M_TechButton;

	UPROPERTY()
	TObjectPtr<UButton> M_ArchiveButton;

	FOnWorldCampaignBarNavigate M_OnNavigateRequested;
	float M_SecondsUntilRefresh = 0.f;
};
