// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/CampaignHUD/W_WorldCampaignHUD.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/BatteryFire/W_WorldBatteryFirePrompt.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/BuildSites/W_WorldBuildSitePanel.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CampaignHUD/W_WorldCampaignBar.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CommanderPanel/W_WorldCommanderPanel.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapTags/W_WorldMapTags.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

namespace WorldCampaignHUDLayout
{
	// Draw order on the map: tags under everything, the build cards over everything.
	constexpr int32 MapTagsZOrder = 0;
	constexpr int32 CommanderZOrder = 1;
	constexpr int32 BarZOrder = 2;
	constexpr int32 BuildSiteZOrder = 3;
	constexpr int32 BatteryFirePromptZOrder = 4;
	// The prompt sits at the bottom centre, this far above the screen edge.
	constexpr float BatteryFirePromptBottomMargin = 96.f;

	void FillCanvas(UCanvasPanel& Canvas, UWidget* Widget, const int32 ZOrder)
	{
		UCanvasPanelSlot* const WidgetSlot = Canvas.AddChildToCanvas(Widget);
		WidgetSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		WidgetSlot->SetOffsets(FMargin(0.f));
		WidgetSlot->SetZOrder(ZOrder);
	}
}

void UW_WorldCampaignHUD::InitCampaignHUD(AWorldPlayerController* WorldPlayerController)
{
	if (not IsValid(WorldPlayerController))
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_WorldCampaignHUD::InitCampaignHUD was given no world player controller."));
		return;
	}

	TArray<FString> MissingTextures;
	M_Look.Build(MissingTextures);
	for (const FString& MissingTexture : MissingTextures)
	{
		UE_LOG(LogTemp, Warning, TEXT("WORLDCAMPAIGNHUD missing texture %s"), *MissingTexture);
	}

	BuildWidgetTree(WorldPlayerController);
}

void UW_WorldCampaignHUD::BuildWidgetTree(AWorldPlayerController* WorldPlayerController)
{
	using namespace WorldCampaignHUDLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}

	M_RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	M_RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = M_RootCanvas;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	M_MapTags = CreateWidget<UW_WorldMapTags>(this, UW_WorldMapTags::StaticClass());
	if (IsValid(M_MapTags))
	{
		M_MapTags->InitMapTags(WorldPlayerController, &M_Look);
		FillCanvas(*M_RootCanvas, M_MapTags, MapTagsZOrder);
	}

	M_CommanderPanel = CreateWidget<UW_WorldCommanderPanel>(this, UW_WorldCommanderPanel::StaticClass());
	if (IsValid(M_CommanderPanel))
	{
		M_CommanderPanel->InitCommanderPanel(WorldPlayerController, &M_Look);
		UCanvasPanelSlot* const CommanderSlot = M_RootCanvas->AddChildToCanvas(M_CommanderPanel);
		CommanderSlot->SetAnchors(FAnchors(0.f, 1.f));
		CommanderSlot->SetAlignment(FVector2D(0.f, 1.f));
		CommanderSlot->SetPosition(FVector2D::ZeroVector);
		CommanderSlot->SetAutoSize(true);
		CommanderSlot->SetZOrder(CommanderZOrder);
	}

	M_CampaignBar = CreateWidget<UW_WorldCampaignBar>(this, UW_WorldCampaignBar::StaticClass());
	if (IsValid(M_CampaignBar))
	{
		M_CampaignBar->InitCampaignBar(WorldPlayerController, &M_Look);
		FillCanvas(*M_RootCanvas, M_CampaignBar, BarZOrder);
	}

	M_BuildSitePanel = CreateWidget<UW_WorldBuildSitePanel>(this, UW_WorldBuildSitePanel::StaticClass());
	if (IsValid(M_BuildSitePanel))
	{
		M_BuildSitePanel->InitBuildSitePanel(WorldPlayerController, &M_Look);
		FillCanvas(*M_RootCanvas, M_BuildSitePanel, BuildSiteZOrder);
	}

	M_BatteryFirePrompt = CreateWidget<UW_WorldBatteryFirePrompt>(this, UW_WorldBatteryFirePrompt::StaticClass());
	if (IsValid(M_BatteryFirePrompt))
	{
		M_BatteryFirePrompt->InitBatteryFirePrompt(WorldPlayerController, &M_Look);
		UCanvasPanelSlot* const PromptSlot = M_RootCanvas->AddChildToCanvas(M_BatteryFirePrompt);
		PromptSlot->SetAnchors(FAnchors(0.5f, 1.f));
		PromptSlot->SetAlignment(FVector2D(0.5f, 1.f));
		PromptSlot->SetPosition(FVector2D(0.f, -BatteryFirePromptBottomMargin));
		PromptSlot->SetAutoSize(true);
		PromptSlot->SetZOrder(BatteryFirePromptZOrder);
	}
}

void UW_WorldCampaignHUD::SetMapWidgetsVisible(const bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (IsValid(M_MapTags))
	{
		M_MapTags->SetTagsVisible(bVisible);
	}

	if (bVisible)
	{
		return;
	}

	CloseBuildSite();
	if (IsValid(M_CommanderPanel))
	{
		M_CommanderPanel->CancelTargetPicking();
	}

	if (IsValid(M_BatteryFirePrompt))
	{
		M_BatteryFirePrompt->CancelPicking();
	}
}

void UW_WorldCampaignHUD::OpenBuildSite(AWorldBuildNode* BuildNode)
{
	if (not IsValid(M_BuildSitePanel) || not IsValid(BuildNode))
	{
		return;
	}

	M_BuildSitePanel->OpenForBuildNode(BuildNode);
}

void UW_WorldCampaignHUD::CloseBuildSite()
{
	if (IsValid(M_BuildSitePanel) && M_BuildSitePanel->GetIsOpen())
	{
		M_BuildSitePanel->ClosePanel();
	}
}

AWorldBuildNode* UW_WorldCampaignHUD::GetOpenBuildSite() const
{
	const bool bIsOpen = IsValid(M_BuildSitePanel) && M_BuildSitePanel->GetIsOpen();
	return bIsOpen ? M_BuildSitePanel->GetOpenBuildNode() : nullptr;
}

bool UW_WorldCampaignHUD::GetIsCursorOverBuildSitePanel() const
{
	return IsValid(M_BuildSitePanel) && M_BuildSitePanel->GetIsCursorOverPlate();
}

void UW_WorldCampaignHUD::SetIsInputBlocked(const bool bIsBlocked)
{
	if (IsValid(M_CommanderPanel))
	{
		M_CommanderPanel->SetIsInputBlocked(bIsBlocked);
	}

	if (not bIsBlocked)
	{
		return;
	}

	CloseBuildSite();
	if (IsValid(M_BatteryFirePrompt))
	{
		M_BatteryFirePrompt->CancelPicking();
	}
}

void UW_WorldCampaignHUD::RefreshAtTurnStart()
{
	if (IsValid(M_CommanderPanel))
	{
		M_CommanderPanel->RefreshFromCampaign();
	}

	if (IsValid(M_CampaignBar))
	{
		M_CampaignBar->RefreshFromCampaign();
	}
}
