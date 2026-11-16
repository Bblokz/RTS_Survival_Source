// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_WallBreachTakeover.h"

#include "Engine/Texture2D.h"

namespace WallBreachTakeoverPrivate
{
	const TCHAR* const ClipAtlasPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/WallBreach/T_WallBreach_ClipAtlas.T_WallBreach_ClipAtlas");
}

UW_WallBreachTakeover::UW_WallBreachTakeover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	M_ClipAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(WallBreachTakeoverPrivate::ClipAtlasPath));
}

FText UW_WallBreachTakeover::GetWallBreachTitle()
{
	return NSLOCTEXT("WallBreachTakeover", "Title", "Crushing Walls");
}

FText UW_WallBreachTakeover::GetWallBreachSummary()
{
	return NSLOCTEXT("WallBreachTakeover", "ArchiveSummary",
	                 "Your medium and heavy tanks can drive straight through Soviet concrete walls. Order a move to "
	                 "the far side; the tank plans its route through the wall.\n\n"
	                 "Which tanks: only medium and heavy tanks break walls. Light tanks and infantry go round, or "
	                 "through a gap that is already open.\n\n"
	                 "The push: while the hull pushes through, the tank slows down (a heavy tank less than a medium "
	                 "one). The slab over the hull falls and the edges crumble.\n\n"
	                 "The gap: the wall loses health for what fell, and the gap stays open. Your infantry and light "
	                 "tanks can follow, but so can the enemy, and shells and sight pass through it.");
}

void UW_WallBreachTakeover::SetupWallBreach()
{
	SetKindLabel(NSLOCTEXT("WallBreachTakeover", "KindLabel", "MEDIUM AND HEAVY TANKS  -  SOVIET WALLS"));
	SetTitleText(GetWallBreachTitle());
	SetDescriptionText(NSLOCTEXT("WallBreachTakeover", "Description",
	                             "Medium and heavy tanks drive straight through Soviet concrete walls. Order a move "
	                             "to the far side; the tank plans its route through the wall."));
	BuildClipBody();
}

TArray<FClipTakeoverFact> UW_WallBreachTakeover::MakeFacts() const
{
	// From UWallTankBreachComponent / URTSWallGateNavQueryFilter (2026-11-16, WallChunkDamage.md "Tank breaches"):
	// only player 1's medium and heavy tanks break the AI side's breachable walls and plan through them; a pushing
	// hull is held to 40 % (medium) / 60 % (heavy) travel speed; the slab over the hull drops 0.45 s after the first
	// break, the edges crumble to a V; the wall loses removed share x max health; fallen breaches are open to every
	// unit and stop blocking projectiles and sight. Kept qualitative where a balance pass would make a number wrong.
	TArray<FClipTakeoverFact> Facts;
	FClipTakeoverFact& WhoFact = Facts.AddDefaulted_GetRef();
	WhoFact.Label = NSLOCTEXT("WallBreachTakeover", "WhoLabel", "WHICH TANKS");
	WhoFact.Title = NSLOCTEXT("WallBreachTakeover", "WhoTitle", "Medium and heavy only");
	WhoFact.Body = NSLOCTEXT("WallBreachTakeover", "WhoBody",
	                         "Only medium and heavy tanks break walls. Light tanks and infantry go round, or through a "
	                         "gap that is already open.");

	FClipTakeoverFact& PushFact = Facts.AddDefaulted_GetRef();
	PushFact.Label = NSLOCTEXT("WallBreachTakeover", "PushLabel", "THE PUSH");
	PushFact.Title = NSLOCTEXT("WallBreachTakeover", "PushTitle", "Slow going through");
	PushFact.Body = NSLOCTEXT("WallBreachTakeover", "PushBody",
	                          "While the hull pushes through, the tank slows down; a heavy tank less than a medium "
	                          "one. The slab over the hull falls and the edges crumble.");

	FClipTakeoverFact& GapFact = Facts.AddDefaulted_GetRef();
	GapFact.Label = NSLOCTEXT("WallBreachTakeover", "GapLabel", "THE GAP");
	GapFact.Title = NSLOCTEXT("WallBreachTakeover", "GapTitle", "Open to everyone");
	GapFact.Body = NSLOCTEXT("WallBreachTakeover", "GapBody",
	                         "The wall loses health for what fell. Your infantry can follow through the gap, but so can "
	                         "the enemy, and shells and sight pass through it.");
	return Facts;
}

FText UW_WallBreachTakeover::GetClipCaption() const
{
	return NSLOCTEXT("WallBreachTakeover", "ClipCaption",
	                 "A Tiger drives through a Soviet concrete wall; the slab over its hull falls and the gap stays "
	                 "open.");
}
