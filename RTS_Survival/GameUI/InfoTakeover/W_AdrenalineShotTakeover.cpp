// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_AdrenalineShotTakeover.h"

#include "Engine/Texture2D.h"

namespace AdrenalineShotTakeoverPrivate
{
	const TCHAR* const ClipAtlasPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/AdrenalineShot/T_AdrenalineShot_ClipAtlas."
		"T_AdrenalineShot_ClipAtlas");
}

UW_AdrenalineShotTakeover::UW_AdrenalineShotTakeover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	M_ClipAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(AdrenalineShotTakeoverPrivate::ClipAtlasPath));
}

FText UW_AdrenalineShotTakeover::GetAdrenalineShotTitle()
{
	return NSLOCTEXT("AdrenalineShotTakeover", "Title", "Adrenaline Shot");
}

FText UW_AdrenalineShotTakeover::GetAdrenalineShotSummary()
{
	return NSLOCTEXT("AdrenalineShotTakeover", "ArchiveSummary",
	                 "Adrenaline Shot pushes the crew of a Panzer 38(t) to work the 37 mm gun as fast as they can.\n\n"
	                 "Reload: for 12 seconds the gun reloads in a third of the time, so it fires about three times "
	                 "as often.\n\n"
	                 "Heavier hits: every round bursts with more shrapnel over a wider area, which hurts infantry "
	                 "near the impact.\n\n"
	                 "The catch: the rushed crew aims worse, so more rounds miss at long range, and they need a few "
	                 "minutes before they can do it again.");
}

void UW_AdrenalineShotTakeover::SetupAdrenalineShot()
{
	SetKindLabel(NSLOCTEXT("AdrenalineShotTakeover", "KindLabel", "PANZER 38(T)  -  VEHICLE ABILITY"));
	SetTitleText(GetAdrenalineShotTitle());
	SetDescriptionText(NSLOCTEXT("AdrenalineShotTakeover", "Description",
	                             "The crew works the 37 mm gun as fast as they can. Select the Panzer 38(t) and use the "
	                             "Adrenaline Shot card; it works while the tank keeps fighting."));
	BuildClipBody();
}

TArray<FClipTakeoverFact> UW_AdrenalineShotTakeover::MakeFacts() const
{
	// From Beh_WeaponVFX_Pz38AdrenalineShot (2026-11-15): 12 s, reload x0.33 (3.3 s -> 1.1 s on the KwK 38(t)),
	// accuracy x0.67, calibre x2 (bigger impact, decal and bounce damage chance), +1 g TNT, +80 shrapnel range,
	// +15 shrapnel damage, +20 shrapnel particles, artillery impact effects; ability cooldown 180 s. Kept qualitative
	// where a balance pass would make a number wrong.
	TArray<FClipTakeoverFact> Facts;
	FClipTakeoverFact& ReloadFact = Facts.AddDefaulted_GetRef();
	ReloadFact.Label = NSLOCTEXT("AdrenalineShotTakeover", "ReloadLabel", "RELOAD");
	ReloadFact.Title = NSLOCTEXT("AdrenalineShotTakeover", "ReloadTitle", "Three shots for one");
	ReloadFact.Body = NSLOCTEXT("AdrenalineShotTakeover", "ReloadBody",
	                            "For 12 seconds the gun reloads in a third of the time, so it fires about three times "
	                            "as often.");

	FClipTakeoverFact& ShrapnelFact = Facts.AddDefaulted_GetRef();
	ShrapnelFact.Label = NSLOCTEXT("AdrenalineShotTakeover", "ShrapnelLabel", "SHRAPNEL");
	ShrapnelFact.Title = NSLOCTEXT("AdrenalineShotTakeover", "ShrapnelTitle", "Heavier hits");
	ShrapnelFact.Body = NSLOCTEXT("AdrenalineShotTakeover", "ShrapnelBody",
	                              "Every round bursts with more shrapnel over a wider area, which hurts infantry near "
	                              "the impact.");

	FClipTakeoverFact& CatchFact = Facts.AddDefaulted_GetRef();
	CatchFact.Label = NSLOCTEXT("AdrenalineShotTakeover", "CatchLabel", "THE CATCH");
	CatchFact.Title = NSLOCTEXT("AdrenalineShotTakeover", "CatchTitle", "Fast, not careful");
	CatchFact.Body = NSLOCTEXT("AdrenalineShotTakeover", "CatchBody",
	                           "The rushed crew aims worse, so more rounds miss at long range. They need a few "
	                           "minutes before they can do it again.");
	return Facts;
}

FText UW_AdrenalineShotTakeover::GetClipCaption() const
{
	return NSLOCTEXT("AdrenalineShotTakeover", "ClipCaption",
	                 "A Panzer 38(t) fires at its normal pace, then with Adrenaline Shot.");
}
