// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/BuildSites/W_WorldBuildCard.h"
#include "W_WorldBuildSitePanel.generated.h"

class AWorldBuildNode;
class AWorldPlayerController;
class UBorder;
class UButton;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UWidget;
class UW_WorldBuildCard;
class UWorldBuildNodeManager;
class UWorldCampaignProgressManager;
struct FWorldCampaignUILook;

DECLARE_MULTICAST_DELEGATE(FOnWorldBuildSitePanelClosed);

/** What the panel shows for the open site. */
UENUM()
enum class EWorldBuildSitePanelContent : uint8
{
	Locked,
	ForSale,
	Built
};

/**
 * @brief The card row that opens above a clicked build site (UI sketch sheet 2 A): one card per building the site can
 * take, a Build button, and why the site is locked or what stands on it. Full-screen and click-through except for the
 * plate itself. The campaign HUD creates it, calls InitBuildSitePanel before adding it, and calls OpenForBuildNode
 * when a site is clicked; the plate follows the site on screen until SetAnchorScreenPosition takes over.
 */
UCLASS()
class RTS_SURVIVAL_API UW_WorldBuildSitePanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Builds the panel (closed) and listens to resources and build node changes.
	 * @param WorldPlayerController Controller that owns the build node and progress managers.
	 * @param Look Shared art of the campaign screens; owned by the HUD and must outlive this widget.
	 */
	void InitBuildSitePanel(AWorldPlayerController* WorldPlayerController, const FWorldCampaignUILook* Look);

	/** Opens the panel for a site, or refreshes it when it is already open; the plate follows the site. */
	void OpenForBuildNode(AWorldBuildNode* BuildNode);

	void ClosePanel();

	bool GetIsOpen() const { return bM_IsOpen; }

	AWorldBuildNode* GetOpenBuildNode() const;

	/** The map's cursor trace ignores widgets; the controller asks this before it lights a site behind the cards. */
	bool GetIsCursorOverPlate() const;

	/**
	 * @brief Places the plate above this point instead of above the site, kept on screen.
	 * @param AnchorPosition Point in this widget's local units (the slate units of a full-screen HUD canvas, not
	 * pixels); the plate stops following the site until the next OpenForBuildNode.
	 */
	void SetAnchorScreenPosition(const FVector2D& AnchorPosition);

	/** Called by a card: selects it and names its building on the Build button. */
	void HandleCardClicked(const EWorldCampaignBuilding Building);

	// Capture hooks: the same paths a card click and the Build button take.
	void SelectBuildingForCapture(const EWorldCampaignBuilding Building);
	bool BuildSelectedForCapture();

	FOnWorldBuildSitePanelClosed OnPanelClosed;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	void BuildWidgetTree_Plate();
	void BuildWidgetTree_Footer();
	void BindToCampaignManagers();
	void UnbindFromCampaignManagers();

	/** Re-reads the open site: its state, its options, the prices and what the player can pay. */
	void RefreshContent();
	void RefreshContent_Locked(const AWorldBuildNode& BuildNode);
	void RefreshContent_ForSale(const AWorldBuildNode& BuildNode);
	void RefreshContent_Built(const AWorldBuildNode& BuildNode);

	/**
	 * @brief Replaces the cards in the row.
	 * @param Buildings One card per building, in this order.
	 * @param CardMode For sale or built.
	 */
	void RebuildCards(const TArray<EWorldCampaignBuilding>& Buildings, EWorldBuildCardMode CardMode);

	/** Sizes the plate for what it shows, so a site with one built card is not a four-card plate. */
	void SetContentLayout(EWorldBuildSitePanelContent Content, int32 CardCount);

	/**
	 * @brief Places the title, the cards, the status text and the footer inside a plate of the given width.
	 * @param ContentWidth Width between the plate's side paddings.
	 * @param CardRowWidth Width the shown cards take.
	 */
	void LayOutPlateContent(float ContentWidth, float CardRowWidth);

	/** @return Width the title needs; a stand-in until the title has been laid out once. */
	float GetTitleWidth() const;

	/** The title's width is only known after a layout pass, so the plate is sized again when it turns out different. */
	void RefreshLayoutIfTitleWidthChanged();
	void RefreshAffordability();
	void ApplySelection();
	EWorldCampaignBuilding ChooseDefaultSelection(const TArray<EWorldCampaignBuilding>& Options) const;

	/** @return Place of the node in its country's unlock order (0 first), the same order the unlock rule uses. */
	int32 GetUnlockRankOfNode(const AWorldBuildNode& BuildNode) const;

	void PlacePlate(const FGeometry& MyGeometry);
	void PlaceStem(const FVector2D& PlateTopLeft);
	bool TryBuildSelected();

	UFUNCTION()
	void HandleBuildClicked();

	UFUNCTION()
	void HandleResourcesChanged(const FWorldMapResources& Resources);

	UFUNCTION()
	void HandleBuildingPlaced(FName BuildNodeId, EWorldCampaignBuilding Building);

	UFUNCTION()
	void HandleBuildNodeUnlocked(FName BuildNodeId, FName CountryId);

	// Takes every click on the plate, so a click between the cards never reaches the map under it.
	UFUNCTION()
	FEventReply HandlePlateMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	bool GetIsValidWorldPlayerController() const;
	bool GetIsValidBuildNodeManager() const;
	bool GetIsValidProgressManager() const;

	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_WorldPlayerController;

	// Owned by the HUD that created this widget.
	const FWorldCampaignUILook* M_Look = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UWorldBuildNodeManager> M_BuildNodeManager;

	UPROPERTY()
	TWeakObjectPtr<UWorldCampaignProgressManager> M_ProgressManager;

	UPROPERTY()
	TWeakObjectPtr<AWorldBuildNode> M_OpenBuildNode;

	EWorldBuildSitePanelContent M_Content = EWorldBuildSitePanelContent::Locked;

	EWorldCampaignBuilding M_SelectedBuilding = EWorldCampaignBuilding::None;

	bool bM_IsOpen = false;

	// True from OpenForBuildNode until SetAnchorScreenPosition: the plate is put above the projected site each tick.
	bool bM_FollowsBuildNode = true;

	bool bM_HasAnchor = false;

	// The site (or the HUD's point) in local units; the plate is placed relative to it.
	FVector2D M_AnchorPosition = FVector2D::ZeroVector;

	FVector2D M_PlateSize = FVector2D::ZeroVector;

	// What the plate was last sized for, to size it again when the title's laid-out width changes.
	float M_LaidOutTitleWidth = 0.f;

	int32 M_LaidOutCardCount = 0;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> M_RootCanvas;

	UPROPERTY()
	TObjectPtr<UBorder> M_Plate;

	// Opaque dark sheet under the bolted frame, so the map's country names do not read through the plate.
	UPROPERTY()
	TObjectPtr<UImage> M_PlateBacking;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> M_PlateCanvas;

	// Thin line from the plate to the site, like the sketch's leader line.
	UPROPERTY()
	TObjectPtr<UImage> M_Stem;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TitleText;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> M_CardRow;

	// Why a site is locked; shown where the cards would be.
	UPROPERTY()
	TObjectPtr<UTextBlock> M_StatusText;

	// Why the last build was refused; next to the Build button.
	UPROPERTY()
	TObjectPtr<UTextBlock> M_FailText;

	UPROPERTY()
	TObjectPtr<UWidget> M_BuildButtonBox;

	UPROPERTY()
	TObjectPtr<UButton> M_BuildButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_BuildLabel;

	UPROPERTY()
	TArray<TObjectPtr<UW_WorldBuildCard>> M_Cards;
};
