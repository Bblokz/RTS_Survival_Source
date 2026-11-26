// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechWheel.h"
#include "W_WorldTechWheel.generated.h"

class AWorldPlayerController;
class UButton;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UTechWheelNodeButton;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UW_TechWheelEffectPanel;
class UW_TechWheelPaintLayer;
class UWorldCampaignTechManager;
struct FSlateFontInfo;
struct FTechWheelLabelObstacle;
struct FTechWheelPaintList;
struct FTechWheelPaintPass;
struct FWorldCampaignUILook;

DECLARE_MULTICAST_DELEGATE(FOnWorldTechWheelCloseRequested);

/** The widgets that show one node of the wheel; the painted parts live in the paint layers. */
USTRUCT()
struct FTechWheelNodeWidgets
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UTechWheelNodeButton> Button;

	// Technology nodes only.
	UPROPERTY()
	TObjectPtr<UImage> Icon;

	// Technology nodes only.
	UPROPERTY()
	TObjectPtr<UTextBlock> Label;

	// Nodes with a cross-branch key only: the round marker that turns green once the key is researched.
	UPROPERTY()
	TObjectPtr<UImage> KeyMarker;
};

/** The texts and parts of the detail panel on the right. */
USTRUCT()
struct FTechWheelDetailWidgets
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UTextBlock> KindText;

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UImage> Icon;

	UPROPERTY()
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> ReachedGroup;

	UPROPERTY()
	TObjectPtr<UTextBlock> ReachedText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> KeyGroup;

	UPROPERTY()
	TObjectPtr<UTextBlock> KeyNameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> KeyStatusText;

	UPROPERTY()
	TObjectPtr<UTextBlock> KeyBranchText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> PriceGroup;

	UPROPERTY()
	TObjectPtr<UImage> PriceIcon;

	UPROPERTY()
	TObjectPtr<UTextBlock> PriceText;

	UPROPERTY()
	TObjectPtr<UTextBlock> HoldText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ReasonText;

	UPROPERTY()
	TObjectPtr<UButton> ResearchButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> ResearchLabel;
};

/** The campaign's tech progress at one moment; every part of the screen is drawn from the same copy. */
struct FTechWheelProgressSnapshot
{
	TArray<FName> BoughtNodeIds;
	TArray<ETechnology> ResearchedTechnologies;
	FWorldMapResources Resources;
};

/**
 * @brief The full-screen TECHTREE page of the world map (UI sketch sheet 13): the tech wheel on the left, the
 * selected node's details and the research button on the right. Created by the campaign HUD, which calls
 * InitTechWheel once, RefreshFromCampaign whenever the page is shown, and switches pages on OnCloseRequested.
 * Builds its own widget tree; buying goes through UWorldCampaignTechManager::TryBuyWheelNode.
 */
UCLASS()
class RTS_SURVIVAL_API UW_WorldTechWheel : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Builds the screen for the campaign's wheel and shows its current state.
	 * @param WorldPlayerController Controller that owns the campaign progress and tech managers.
	 * @param Look Shared art of the campaign screens; owned by the HUD and must outlive this widget.
	 */
	void InitTechWheel(AWorldPlayerController* WorldPlayerController, const FWorldCampaignUILook* Look);

	/** Reads the campaign again and redraws every node, the counters and the detail panel. */
	void RefreshFromCampaign();

	/** BACK TO MAP was pressed; the HUD switches back to the map. */
	FOnWorldTechWheelCloseRequested& OnCloseRequested() { return M_OnCloseRequested; }

	// Hooks for the automated capture: they go through the same paths as a click on a node or on RESEARCH.
	void SelectNodeForCapture(FName NodeId);
	bool BuyNodeForCapture(FName NodeId);
	FName GetSelectedNodeId() const { return M_SelectedNodeId; }

protected:
	virtual void NativeOnInitialized() override;
	// The page covers the map; a click on it never reaches the map.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void BuildWidgetTree();
	void BuildScreen();
	void BuildScreen_TopStrip();
	void BuildScreen_TopStrip_Counters();
	void BuildScreen_BranchLabels();
	void BuildScreen_Nodes();
	void BuildScreen_Node(const FWorldTechWheelNode& Node);
	// Runs after every node is built: a name is placed against all nodes and badges, not only the earlier ones.
	void BuildScreen_NodeLabels();

	/**
	 * @brief Adds one technology name on its dark plate and keeps its text with the node's widgets.
	 * @param NodeId Node the name belongs to.
	 * @param LabelText The technology's display name.
	 * @param LabelFont Font the name was measured with.
	 * @param LabelCentre Centre of the plate in screen canvas units, from TechWheelLabelPlacement.
	 */
	void BuildScreen_NodeLabel(FName NodeId, const FText& LabelText, const FSlateFontInfo& LabelFont,
	                           const FVector2D& LabelCentre);

	/**
	 * @brief Lists what the technology names must not cover of one node: its widest outline and key badge, or its pip.
	 * @param Node Node to list.
	 * @param OwnLabelIndex Index of the node's own name among the placed names; INDEX_NONE when it has none.
	 * @param OutObstacles Receives the rectangles; not emptied.
	 */
	void AddLabelObstaclesOfNode(const FWorldTechWheelNode& Node, int32 OwnLabelIndex,
	                             TArray<FTechWheelLabelObstacle>& OutObstacles) const;
	void BuildScreen_StartLabel(const FWorldTechWheelNode& Node);
	void BuildScreen_KeyBadge(const FWorldTechWheelNode& Node, FTechWheelNodeWidgets& NodeWidgets);
	void BuildScreen_DetailPanel();
	void BuildScreen_DetailPanel_Rows(UVerticalBox& Column);
	void BuildScreen_DetailPanel_Price(UVerticalBox& Column);
	void BuildScreen_DetailPanel_Button(UVerticalBox& Column);
	UTextBlock* AddDetailLabel(UVerticalBox& Column, const FText& Label, float TopPadding);
	void LoadTechnologyIcons();

	/** @return false when no campaign is running; the snapshot is then empty. */
	bool ReadProgressSnapshot(FTechWheelProgressSnapshot& OutSnapshot) const;

	void RefreshStrip(const FTechWheelProgressSnapshot& Snapshot);
	void RefreshNodes(const FTechWheelProgressSnapshot& Snapshot);
	void RefreshDetailPanel(const FTechWheelProgressSnapshot& Snapshot);
	void RefreshDetailPanel_Header(const FWorldTechWheelNode& Node);
	void RefreshDetailPanel_Effect(const FWorldTechWheelNode& Node);
	void RefreshDetailPanel_Reached(const FWorldTechWheelNode& Node, const FTechWheelProgressSnapshot& Snapshot);
	void RefreshDetailPanel_Key(const FWorldTechWheelNode& Node, const FTechWheelProgressSnapshot& Snapshot);
	void RefreshDetailPanel_Price(const FWorldTechWheelNode& Node, const FTechWheelProgressSnapshot& Snapshot);
	void RefreshDetailPanel_Button(const FWorldTechWheelNode& Node, const FTechWheelProgressSnapshot& Snapshot);
	void RefreshPaintLayers(const FTechWheelProgressSnapshot& Snapshot);

	void AddPaint_Sectors(FTechWheelPaintPass& Pass) const;
	void AddPaint_Paths(FTechWheelPaintPass& Pass, const FTechWheelProgressSnapshot& Snapshot) const;
	// Draws each link of the node once: only towards neighbours that come later in the wheel's node list.
	void AddPaint_PathsOfNode(FTechWheelPaintPass& Pass, int32 NodeIndex,
	                          const FTechWheelProgressSnapshot& Snapshot) const;
	void AddPaint_NodeOutlines(FTechWheelPaintPass& Pass, const FTechWheelProgressSnapshot& Snapshot) const;
	void AddPaint_TechnologyOutlines(FTechWheelPaintPass& Pass, const FWorldTechWheelNode& Node,
	                                 EWorldTechWheelNodeState State) const;
	void AddPaint_SelectionAndChord(FTechWheelPaintPass& Pass) const;
	void AddPaint_StartAndPips(FTechWheelPaintPass& Pass, const FTechWheelProgressSnapshot& Snapshot) const;
	void AddPaint_KeyMarks(FTechWheelPaintPass& Pass, const FTechWheelProgressSnapshot& Snapshot) const;

	void SelectNode(FName NodeId);
	void HandleNodeClicked(FName NodeId);

	/** @return true when the selected node was bought; OutFailReason says why not otherwise. */
	bool TryBuySelectedNode(FText& OutFailReason);

	UFUNCTION()
	void HandleResearchClicked();

	UFUNCTION()
	void HandleBackClicked();

	FVector2D GetNodePosition(const FWorldTechWheelNode& Node) const;
	FSlateBrush MakeTechnologyBrush(ETechnology Technology, const FVector2D& DrawSize) const;
	FText GetNodeDisplayName(const FWorldTechWheelNode& Node) const;
	const FWorldTechWheel& GetWheel() const;

	bool GetIsValidWorldPlayerController() const;
	bool GetIsValidScreenCanvas() const;
	bool GetIsValidUnderPaintLayer() const;
	bool GetIsValidOverPaintLayer() const;
	UWorldCampaignTechManager* GetTechManager() const;

	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_WorldPlayerController;

	// Owned by the HUD that created this widget.
	const FWorldCampaignUILook* M_Look = nullptr;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> M_ScreenCanvas;

	// Affected units and weapon changes of the selected technology; fills the free height of the detail panel.
	UPROPERTY()
	TObjectPtr<UW_TechWheelEffectPanel> M_EffectPanel;

	UPROPERTY()
	TObjectPtr<UW_TechWheelPaintLayer> M_UnderPaintLayer;

	UPROPERTY()
	TObjectPtr<UW_TechWheelPaintLayer> M_OverPaintLayer;

	UPROPERTY()
	TMap<FName, FTechWheelNodeWidgets> M_NodeWidgets;

	// Loaded from the soft references of the wheel presentation; kept so the icons showing them stay drawn.
	UPROPERTY()
	TMap<ETechnology, TObjectPtr<UTexture2D>> M_LoadedIcons;

	UPROPERTY()
	FTechWheelDetailWidgets M_Detail;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_RadixiteText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_BlueprintText;

	UPROPERTY()
	TObjectPtr<UButton> M_BackButton;

	FName M_SelectedNodeId;

	// Set once BuildScreen ran, so InitTechWheel can be called again without building a second screen.
	bool bM_IsScreenBuilt = false;

	FOnWorldTechWheelCloseRequested M_OnCloseRequested;
};
