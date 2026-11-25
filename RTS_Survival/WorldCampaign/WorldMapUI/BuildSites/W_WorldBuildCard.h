// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "W_WorldBuildCard.generated.h"

class UBorder;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UW_WorldBuildSitePanel;
struct FWorldCampaignUILook;

/** Whether a build card offers its building for sale or shows the building that already stands on the site. */
UENUM()
enum class EWorldBuildCardMode : uint8
{
	ForSale,
	Built
};

/**
 * @brief One building card of the build site panel (UI sketch sheet 2 A): code in a picture well, name, one line of
 * effect and the price, or a built mark once the building stands. Created by UW_WorldBuildSitePanel, which calls SetupBuildCard before adding it to its row;
 * a left click selects the card on the panel.
 */
UCLASS()
class RTS_SURVIVAL_API UW_WorldBuildCard : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Builds the card's widget tree on first use and fills it.
	 * @param Panel Receives the click.
	 * @param Look Shared art of the campaign screens.
	 * @param Building Building the card shows.
	 * @param Cost Price shown on a card for sale.
	 * @param Mode For sale (price shown) or already built (price replaced by a built mark).
	 */
	void SetupBuildCard(UW_WorldBuildSitePanel* Panel, const FWorldCampaignUILook& Look,
	                    EWorldCampaignBuilding Building, const FWorldMapResources& Cost, EWorldBuildCardMode Mode);

	void SetIsSelected(const bool bIsSelected);

	/** A card the player cannot pay for is dimmed but can still be selected. */
	void SetCanAfford(const bool bCanAfford);

	EWorldCampaignBuilding GetBuilding() const { return M_Building; }

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void BuildWidgetTree(const FWorldCampaignUILook& Look);
	void BuildWidgetTree_PriceRow(const FWorldCampaignUILook& Look);
	void BuildWidgetTree_BuiltPlate(const FWorldCampaignUILook& Look);
	void FillPriceRow(const FWorldMapResources& Cost);
	bool GetIsValidPanel() const;

	UPROPERTY()
	TWeakObjectPtr<UW_WorldBuildSitePanel> M_Panel;

	EWorldCampaignBuilding M_Building = EWorldCampaignBuilding::None;

	EWorldBuildCardMode M_Mode = EWorldBuildCardMode::ForSale;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> M_Canvas;

	// Plate behind the card; its outline turns to the accent colour while the card is selected.
	UPROPERTY()
	TObjectPtr<UBorder> M_Background;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CodeText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_EffectText;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> M_PriceRow;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_RadixiteText;

	UPROPERTY()
	TObjectPtr<UImage> M_BlueprintIcon;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_BlueprintText;

	// The lit plate that takes the price's place on a card whose building already stands.
	UPROPERTY()
	TObjectPtr<UBorder> M_BuiltPlate;

	FSlateBrush M_NormalBackgroundBrush;
	FSlateBrush M_SelectedBackgroundBrush;
	// Outline in the state colour for a card whose building already stands.
	FSlateBrush M_BuiltBackgroundBrush;
};
