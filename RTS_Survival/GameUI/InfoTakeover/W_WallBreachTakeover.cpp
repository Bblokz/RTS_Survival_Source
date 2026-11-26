// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_WallBreachTakeover.h"

#include "Engine/Texture2D.h"

namespace WallBreachTakeoverPrivate
{
	const TCHAR* const ClipAtlasPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/WallBreach/T_WallBreach_ClipAtlas.T_WallBreach_ClipAtlas");
	const TCHAR* const RunOverClipAtlasPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/WallBreach/T_RunOver_ClipAtlas.T_RunOver_ClipAtlas");
	// The run-over atlas grid as Tools/InfoTakeover/VulturesAt/make_clip_atlas.py wrote it (clip_atlas.json in
	// Saved/InfoTakeover/RunOver).
	constexpr int32 RunOverClipColumns = 8;
	constexpr int32 RunOverClipRows = 10;
	constexpr int32 RunOverClipFrameCount = 80;
	constexpr float RunOverClipFramesPerSecond = 12.f;
	constexpr float RunOverClipEndHoldSeconds = 1.f;
}

UW_WallBreachTakeover::UW_WallBreachTakeover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	using namespace WallBreachTakeoverPrivate;
	M_ClipAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(ClipAtlasPath));
	M_RunOverClip.Atlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(RunOverClipAtlasPath));
	M_RunOverClip.Columns = RunOverClipColumns;
	M_RunOverClip.Rows = RunOverClipRows;
	M_RunOverClip.FrameCount = RunOverClipFrameCount;
	M_RunOverClip.FramesPerSecond = RunOverClipFramesPerSecond;
	M_RunOverClip.EndHoldSeconds = RunOverClipEndHoldSeconds;
}

FText UW_WallBreachTakeover::GetWallBreachTitle()
{
	return NSLOCTEXT("WallBreachTakeover", "Title", "Crushing Walls");
}

FText UW_WallBreachTakeover::GetWallBreachSummary()
{
	return NSLOCTEXT("WallBreachTakeover", "ArchiveSummary",
	                 "Make a way through: you can crush most walls with heavy vehicles; they are damaged by driving "
	                 "through them. Order a move to the far side; the tank plans its route through the wall.\n\n"
	                 "Size dependent: not all vehicles can crush all walls; some can only be destroyed by the "
	                 "heaviest class of tanks.\n\n"
	                 "The gap stays open: your infantry can follow, but so can the enemy, and shells and sight pass "
	                 "through it.\n\n"
	                 "Run them down: your vehicles crush the enemy soldiers they drive over. Soldiers who see a "
	                 "vehicle coming try to jump aside.");
}

void UW_WallBreachTakeover::SetupWallBreach()
{
	BuildClipBody();
	ApplyClipPageTexts(static_cast<int32>(EWallBreachTakeoverPage::CrushingWalls));
}

void UW_WallBreachTakeover::ApplyClipPageTexts(const int32 PageIndex)
{
	const bool bIsLastPage = PageIndex + 1 >= GetPageCount();
	SetConfirmButtonLabel(bIsLastPage
		                      ? NSLOCTEXT("InfoTakeover", "Understood", "UNDERSTOOD")
		                      : NSLOCTEXT("WallBreachTakeover", "Continue", "CONTINUE"));
	if (PageIndex == static_cast<int32>(EWallBreachTakeoverPage::RunOverInfantry))
	{
		SetKindLabel(NSLOCTEXT("WallBreachTakeover", "RunOverKindLabel", "VEHICLES  -  ENEMY INFANTRY"));
		SetTitleText(NSLOCTEXT("WallBreachTakeover", "RunOverTitle", "Running Over Infantry"));
		SetDescriptionText(NSLOCTEXT("WallBreachTakeover", "RunOverDescription",
		                             "Your vehicles crush the enemy soldiers they drive over."));
		return;
	}
	SetKindLabel(NSLOCTEXT("WallBreachTakeover", "KindLabel", "HEAVY VEHICLES  -  SOVIET WALLS"));
	SetTitleText(GetWallBreachTitle());
	SetDescriptionText(NSLOCTEXT("WallBreachTakeover", "Description",
	                             "Tanks drive straight through Soviet walls. Order a move to the far side; the tank "
	                             "plans its route through the wall."));
}

FClipTakeoverClip UW_WallBreachTakeover::GetPageClip(const int32 PageIndex) const
{
	return PageIndex == static_cast<int32>(EWallBreachTakeoverPage::RunOverInfantry)
		       ? M_RunOverClip
		       : Super::GetPageClip(PageIndex);
}

TArray<FClipTakeoverFact> UW_WallBreachTakeover::MakePageFacts(const int32 PageIndex) const
{
	if (PageIndex != static_cast<int32>(EWallBreachTakeoverPage::RunOverInfantry))
	{
		return MakeFacts();
	}
	// From URTSVehicleCrushSubsystem (2026-11-25, memory vehicle-crush-infantry): a moving vehicle kills hostile
	// soldiers under its hull; soldiers in its path may react and step aside (URTSVehicleCrushSettings::DodgeChance).
	// One short section only, as the owner asked.
	TArray<FClipTakeoverFact> Facts;
	FClipTakeoverFact& RunDownFact = Facts.AddDefaulted_GetRef();
	RunDownFact.Title = NSLOCTEXT("WallBreachTakeover", "RunDownTitle", "Run them down");
	RunDownFact.Body = NSLOCTEXT("WallBreachTakeover", "RunDownBody",
	                             "Drive into enemy squads in the open; soldiers who see you coming try to jump aside.");
	return Facts;
}

TArray<FClipTakeoverFact> UW_WallBreachTakeover::MakeFacts() const
{
	// The owner's words (2026-11-25): two sections, a title and a line each, no labels.
	TArray<FClipTakeoverFact> Facts;
	FClipTakeoverFact& WayThroughFact = Facts.AddDefaulted_GetRef();
	WayThroughFact.Title = NSLOCTEXT("WallBreachTakeover", "WayThroughTitle", "Make a way through");
	WayThroughFact.Body = NSLOCTEXT("WallBreachTakeover", "WayThroughBody",
	                                "You can crush most walls with heavy vehicles; they are damaged by driving through "
	                                "them.");

	FClipTakeoverFact& SizeFact = Facts.AddDefaulted_GetRef();
	SizeFact.Title = NSLOCTEXT("WallBreachTakeover", "SizeTitle", "Size dependent");
	SizeFact.Body = NSLOCTEXT("WallBreachTakeover", "SizeBody",
	                          "Not all vehicles can crush all walls; some can only be destroyed by the heaviest class "
	                          "of tanks.");
	return Facts;
}

FText UW_WallBreachTakeover::GetClipCaption() const
{
	return NSLOCTEXT("WallBreachTakeover", "ClipCaption",
	                 "A Tiger drives through a Soviet concrete wall; the slab over its hull falls and the gap stays "
	                 "open.");
}

FText UW_WallBreachTakeover::GetPageClipCaption(const int32 PageIndex) const
{
	if (PageIndex == static_cast<int32>(EWallBreachTakeoverPage::RunOverInfantry))
	{
		return NSLOCTEXT("WallBreachTakeover", "RunOverClipCaption",
		                 "A Tiger drives through a Soviet rifle squad that stays in its path.");
	}
	return Super::GetPageClipCaption(PageIndex);
}
