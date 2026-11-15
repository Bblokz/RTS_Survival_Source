// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_DigInTakeover.h"

#include "Engine/Texture2D.h"

namespace DigInTakeoverPrivate
{
	const TCHAR* const ClipAtlasPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/DigIn/T_DigIn_ClipAtlas.T_DigIn_ClipAtlas");
}

UW_DigInTakeover::UW_DigInTakeover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	M_ClipAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(DigInTakeoverPrivate::ClipAtlasPath));
}

FText UW_DigInTakeover::GetDigInTitle()
{
	return NSLOCTEXT("DigInTakeover", "Title", "Dig In");
}

FText UW_DigInTakeover::GetDigInSummary()
{
	return NSLOCTEXT("DigInTakeover", "ArchiveSummary",
	                 "Dig In has the crew pile a sandbag wall around the tank's hull.\n\n"
	                 "The wall: shells that hit the sandbags do not hurt the tank, until the wall is shot away. Heavier "
	                 "tanks build a stronger wall but take longer.\n\n"
	                 "Building: while the wall goes up the tank cannot move or turn and its guns stay silent. The wall "
	                 "starts weak and grows to full strength.\n\n"
	                 "The catch: a dug-in tank stays where it is and only fires at what comes into range. Use Break "
	                 "Cover to move again; that removes the wall. If the wall is destroyed, the tank breaks cover on its "
	                 "own.");
}

void UW_DigInTakeover::SetupDigIn()
{
	SetKindLabel(NSLOCTEXT("DigInTakeover", "KindLabel", "TANKS  -  VEHICLE ABILITY"));
	SetTitleText(GetDigInTitle());
	SetDescriptionText(NSLOCTEXT("DigInTakeover", "Description",
	                             "The crew piles sandbags around the tank's hull. Use the Dig In card before the enemy "
	                             "arrives."));
	BuildClipBody();
}

TArray<FClipTakeoverFact> UW_DigInTakeover::MakeFacts() const
{
	// From UDigInComponent / ADigInWall / ATrackedTankMaster (2026-11-15): the wall spawns around the hull, starts
	// at a third of its health and grows once a second; light / medium / heavy tank walls take 8 / 12 / 16 s and get
	// 320 / 560 / 960 health. While it builds Move, Reverse Move and Rotate Towards are removed and the turrets are off;
	// once built the turrets auto-engage but the tank does not aggro. A destroyed wall forces Break Cover. Kept
	// qualitative where a balance pass would make a number wrong.
	TArray<FClipTakeoverFact> Facts;
	FClipTakeoverFact& WallFact = Facts.AddDefaulted_GetRef();
	WallFact.Label = NSLOCTEXT("DigInTakeover", "WallLabel", "THE WALL");
	WallFact.Title = NSLOCTEXT("DigInTakeover", "WallTitle", "Sandbags take the hits");
	WallFact.Body = NSLOCTEXT("DigInTakeover", "WallBody",
	                          "Shells that hit the sandbags do not hurt the tank, until the wall is shot away. Heavier "
	                          "tanks build a stronger wall.");

	FClipTakeoverFact& BuildFact = Facts.AddDefaulted_GetRef();
	BuildFact.Label = NSLOCTEXT("DigInTakeover", "BuildLabel", "BUILDING");
	BuildFact.Title = NSLOCTEXT("DigInTakeover", "BuildTitle", "Dig in before the fight");
	BuildFact.Body = NSLOCTEXT("DigInTakeover", "BuildBody",
	                           "While the wall goes up the tank cannot move or turn and its guns stay silent. The wall "
	                           "starts weak and grows to full strength.");

	FClipTakeoverFact& CatchFact = Facts.AddDefaulted_GetRef();
	CatchFact.Label = NSLOCTEXT("DigInTakeover", "CatchLabel", "THE CATCH");
	CatchFact.Title = NSLOCTEXT("DigInTakeover", "CatchTitle", "Stuck until you break cover");
	CatchFact.Body = NSLOCTEXT("DigInTakeover", "CatchBody",
	                           "A dug-in tank stays put. Use Break Cover to move again. If the wall is destroyed, the "
	                           "tank breaks cover on its own.");
	return Facts;
}

FText UW_DigInTakeover::GetClipCaption() const
{
	return NSLOCTEXT("DigInTakeover", "ClipCaption",
	                 "A Panzer IV digs in; when two T-26s open fire, their shells hit the sandbags, not the tank.");
}
