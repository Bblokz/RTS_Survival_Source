// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_WespeBarrageTakeover.h"

#include "Engine/Texture2D.h"

namespace WespeBarrageTakeoverPrivate
{
	const TCHAR* const ClipAtlasPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/WespeBarrage/T_WespeBarrage_ClipAtlas."
		"T_WespeBarrage_ClipAtlas");
}

UW_WespeBarrageTakeover::UW_WespeBarrageTakeover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	M_ClipAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(WespeBarrageTakeoverPrivate::ClipAtlasPath));
}

FText UW_WespeBarrageTakeover::GetWespeBarrageTitle()
{
	return NSLOCTEXT("WespeBarrageTakeover", "Title", "Barrage");
}

FText UW_WespeBarrageTakeover::GetWespeBarrageSummary()
{
	return NSLOCTEXT("WespeBarrageTakeover", "ArchiveSummary",
	                 "Barrage turns the Wespe's 105 mm howitzer on one area instead of one target.\n\n"
	                 "Area: select the Wespe, use the Barrage card and click the ground. For 20 seconds the crew "
	                 "shells the marked area, spread across it, whether or not they can see anything there.\n\n"
	                 "Rate: the crew reloads faster than normal but aims worse, so the shells scatter wider.\n\n"
	                 "The catch: the Wespe cannot move while the barrage runs, and the crew needs two minutes before "
	                 "they can fire another. Several Wespes on one area hit far harder than one.");
}

void UW_WespeBarrageTakeover::SetupWespeBarrage()
{
	SetKindLabel(NSLOCTEXT("WespeBarrageTakeover", "KindLabel", "WESPE  -  ARTILLERY ABILITY"));
	SetTitleText(GetWespeBarrageTitle());
	SetDescriptionText(NSLOCTEXT("WespeBarrageTakeover", "Description",
	                             "The Wespe shells an area instead of one target. Select the Wespe, use the Barrage "
	                             "card and click the ground where the shells should fall."));
	BuildClipBody();
}

TArray<FClipTakeoverFact> UW_WespeBarrageTakeover::MakeFacts() const
{
	// From the Wespe's BarrageAimAbility (UTankAimAbilityComponent, HeArtilleryBarrage, 2026-11-15): behaviour
	// Beh_Aim_WespeBarrage for 20 s (reload x0.6, accuracy x0.5), Move and Reverse Move removed while it runs, target
	// radius 1500, cooldown 120 s. The leFH 18 reloads in 8 s, so one Wespe fires about four shells per barrage. Kept
	// qualitative where a balance pass would make a number wrong.
	TArray<FClipTakeoverFact> Facts;
	FClipTakeoverFact& AreaFact = Facts.AddDefaulted_GetRef();
	AreaFact.Label = NSLOCTEXT("WespeBarrageTakeover", "AreaLabel", "AREA");
	AreaFact.Title = NSLOCTEXT("WespeBarrageTakeover", "AreaTitle", "Shell the ground, not a unit");
	AreaFact.Body = NSLOCTEXT("WespeBarrageTakeover", "AreaBody",
	                          "For 20 seconds the crew fires at the marked area, even where they cannot see. Use it on "
	                          "positions, crossings and massed infantry.");

	FClipTakeoverFact& RateFact = Facts.AddDefaulted_GetRef();
	RateFact.Label = NSLOCTEXT("WespeBarrageTakeover", "RateLabel", "RATE");
	RateFact.Title = NSLOCTEXT("WespeBarrageTakeover", "RateTitle", "Faster and looser");
	RateFact.Body = NSLOCTEXT("WespeBarrageTakeover", "RateBody",
	                          "The crew reloads faster than normal but aims worse, so the shells scatter across the "
	                          "area. Several Wespes on one spot hit far harder.");

	FClipTakeoverFact& CatchFact = Facts.AddDefaulted_GetRef();
	CatchFact.Label = NSLOCTEXT("WespeBarrageTakeover", "CatchLabel", "THE CATCH");
	CatchFact.Title = NSLOCTEXT("WespeBarrageTakeover", "CatchTitle", "Dug in until it ends");
	CatchFact.Body = NSLOCTEXT("WespeBarrageTakeover", "CatchBody",
	                           "The Wespe cannot move while the barrage runs, and the crew needs two minutes before "
	                           "they can fire another.");
	return Facts;
}

FText UW_WespeBarrageTakeover::GetClipCaption() const
{
	return NSLOCTEXT("WespeBarrageTakeover", "ClipCaption",
	                 "Three Wespes fire a barrage; the shells fall on a T-26 and a rifle squad far away.");
}
