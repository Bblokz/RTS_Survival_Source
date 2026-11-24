// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_RattleFeedTakeover.h"

#include "Engine/Texture2D.h"

namespace RattleFeedTakeoverPrivate
{
	const TCHAR* const ClipAtlasPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/RattleFeed/T_RattleFeed_ClipAtlas.T_RattleFeed_ClipAtlas");
}

UW_RattleFeedTakeover::UW_RattleFeedTakeover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	M_ClipAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(RattleFeedTakeoverPrivate::ClipAtlasPath));
}

FText UW_RattleFeedTakeover::GetRattleFeedTitle()
{
	return NSLOCTEXT("RattleFeedTakeover", "Title", "Rattle-Feed");
}

FText UW_RattleFeedTakeover::GetRattleFeedSummary()
{
	return NSLOCTEXT("RattleFeedTakeover", "ArchiveSummary",
	                 "Rattle-Feed lets the crew of a Panzer II, Luchs or 20 mm armoured car run their autocannon flat "
	                 "out for a short burst.\n\n"
	                 "Burst: for 15 seconds the gun barely pauses. Bursts follow each other quickly, magazines hold "
	                 "twice as many rounds and reload almost at once, so about three times as many rounds go out.\n\n"
	                 "Weapons: only guns of 20 mm and up speed up; machine guns fire as normal. Each hit does the same "
	                 "damage and pierces the same armour as before, so it adds volume, not punch.\n\n"
	                 "Cooldown: the crew needs well over a minute before they can do it again, so save it for the "
	                 "moment targets crowd in front of the gun.");
}

void UW_RattleFeedTakeover::SetupRattleFeed()
{
	SetKindLabel(NSLOCTEXT("RattleFeedTakeover", "KindLabel", "PANZER II  -  VEHICLE ABILITY"));
	SetTitleText(GetRattleFeedTitle());
	SetDescriptionText(NSLOCTEXT("RattleFeedTakeover", "Description",
	                             "The crew runs the 20 mm autocannon flat out for a short burst. Select the Panzer II "
	                             "and use the Rattle-Feed card; it works while the tank keeps fighting."));
	BuildClipBody();
}

TArray<FClipTakeoverFact> UW_RattleFeedTakeover::MakeFacts() const
{
	// From BP_Beh_RapidFire_Autocannon (2026-11-15): 15 s, pause between bursts x0.33, reload x0.1, magazine x2,
	// weapons of 20 mm and up only; the rounds inside a burst keep their spacing. The ability cooldown is 100 s on the
	// Panzer II F, Luchs, Sd.Kfz. 231 and 140 (30 s on the Jaguar). The clip capture measured the Panzer II F at about
	// 1 shot/s over a magazine and its reload, and 3.4 shots/s with Rattle-Feed. Kept qualitative where a balance pass
	// would make a number wrong.
	TArray<FClipTakeoverFact> Facts;
	FClipTakeoverFact& BurstFact = Facts.AddDefaulted_GetRef();
	BurstFact.Label = NSLOCTEXT("RattleFeedTakeover", "BurstLabel", "BURST");
	BurstFact.Title = NSLOCTEXT("RattleFeedTakeover", "BurstTitle", "Bursts back to back");
	BurstFact.Body = NSLOCTEXT("RattleFeedTakeover", "BurstBody",
	                           "For 15 seconds the gun barely pauses: bursts follow each other quickly, magazines hold "
	                           "twice as many rounds and reload almost at once.");

	FClipTakeoverFact& WeaponFact = Facts.AddDefaulted_GetRef();
	WeaponFact.Label = NSLOCTEXT("RattleFeedTakeover", "WeaponLabel", "WEAPONS");
	WeaponFact.Title = NSLOCTEXT("RattleFeedTakeover", "WeaponTitle", "Volume, not punch");
	WeaponFact.Body = NSLOCTEXT("RattleFeedTakeover", "WeaponBody",
	                            "Only the autocannon speeds up; machine guns fire as normal. Each hit does the same "
	                            "damage and pierces the same armour as before.");

	FClipTakeoverFact& CooldownFact = Facts.AddDefaulted_GetRef();
	CooldownFact.Label = NSLOCTEXT("RattleFeedTakeover", "CooldownLabel", "COOLDOWN");
	CooldownFact.Title = NSLOCTEXT("RattleFeedTakeover", "CooldownTitle", "Pick the moment");
	CooldownFact.Body = NSLOCTEXT("RattleFeedTakeover", "CooldownBody",
	                              "The crew needs well over a minute before they can do it again. Save it for when "
	                              "targets crowd in front of the gun.");
	return Facts;
}

FText UW_RattleFeedTakeover::GetClipCaption() const
{
	return NSLOCTEXT("RattleFeedTakeover", "ClipCaption",
	                 "A Panzer II fires a magazine and sits out its long reload; with Rattle-Feed the gun keeps going.");
}
