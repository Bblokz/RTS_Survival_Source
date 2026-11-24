// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CampaignHUD/WorldCampaignUILook.h"
#include "W_WorldCampaignHUD.generated.h"

class AWorldBuildNode;
class AWorldPlayerController;
class UCanvasPanel;
class UW_WorldBuildSitePanel;
class UW_WorldCampaignBar;
class UW_WorldCommanderPanel;
class UW_WorldMapTags;

/**
 * @brief Everything the campaign draws over the world map: the top strip, the tags and arrows on the map, the
 * commander panel and the build site cards. Created by UW_WorldMenu, which hides it while a full-screen page is open.
 * Owns the shared look that all campaign screens draw with.
 */
UCLASS()
class RTS_SURVIVAL_API UW_WorldCampaignHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Loads the look and builds the four map widgets. Call once, before the HUD is added to the viewport.
	 * @param WorldPlayerController Controller that owns the campaign managers.
	 */
	void InitCampaignHUD(AWorldPlayerController* WorldPlayerController);

	/** The shared art; valid for the lifetime of the HUD. The tech wheel page uses it too. */
	const FWorldCampaignUILook* GetLook() const { return &M_Look; }

	/** Off while the deck builder, the archive or the tech wheel covers the map. */
	void SetMapWidgetsVisible(bool bVisible);

	void OpenBuildSite(AWorldBuildNode* BuildNode);
	void CloseBuildSite();

	/** @return The site whose cards are open, so its plate on the map stays lit; null while the cards are closed. */
	AWorldBuildNode* GetOpenBuildSite() const;

	/** @return true while the cursor is on the build cards, which cover the map sites behind them. */
	bool GetIsCursorOverBuildSitePanel() const;

	/** The enemy turn blocks the commander ability and closes whatever the player had open. */
	void SetIsInputBlocked(bool bIsBlocked);

	/** Re-reads cooldowns and numbers when a new player turn starts or a campaign was loaded. */
	void RefreshAtTurnStart();

	UW_WorldCampaignBar* GetCampaignBar() const { return M_CampaignBar; }
	UW_WorldCommanderPanel* GetCommanderPanel() const { return M_CommanderPanel; }
	UW_WorldBuildSitePanel* GetBuildSitePanel() const { return M_BuildSitePanel; }
	UW_WorldMapTags* GetMapTags() const { return M_MapTags; }

private:
	void BuildWidgetTree(AWorldPlayerController* WorldPlayerController);

	UPROPERTY()
	FWorldCampaignUILook M_Look;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> M_RootCanvas;

	UPROPERTY()
	TObjectPtr<UW_WorldCampaignBar> M_CampaignBar;

	UPROPERTY()
	TObjectPtr<UW_WorldCommanderPanel> M_CommanderPanel;

	UPROPERTY()
	TObjectPtr<UW_WorldBuildSitePanel> M_BuildSitePanel;

	UPROPERTY()
	TObjectPtr<UW_WorldMapTags> M_MapTags;
};
