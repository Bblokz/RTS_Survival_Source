// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_VulturesAtTakeover.h"

#include "Engine/Texture2D.h"

namespace VulturesAtTakeoverPrivate
{
	const TCHAR* const ClipAtlasPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/VulturesAt/T_VulturesAt_ClipAtlas.T_VulturesAt_ClipAtlas");
}

UW_VulturesAtTakeover::UW_VulturesAtTakeover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	M_ClipAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(VulturesAtTakeoverPrivate::ClipAtlasPath));
}

FText UW_VulturesAtTakeover::GetVulturesAtTitle()
{
	return NSLOCTEXT("VulturesAtTakeover", "Title", "AT Stick Grenades");
}

FText UW_VulturesAtTakeover::GetVulturesAtSummary()
{
	return NSLOCTEXT("VulturesAtTakeover", "ArchiveSummary",
	                 "The Hazmat Vultures carry M24 stick grenades rigged to cling to armour.\n\n"
	                 "Volley: all four soldiers throw at once. The grenades stick to the hull and go off a moment "
	                 "later, so even a tank on the move takes the whole volley.\n\n"
	                 "Targets: one volley wrecks most light tanks and badly hurts medium ones. The blast always strikes "
	                 "the tank's thinnest plate, so the side it faces does not matter, but thick armour still soaks "
	                 "up much of it.\n\n"
	                 "Reload: the squad needs a long while before it can throw again, so pick the moment. The throw is "
	                 "short: the squad first walks up to the target, into its guns.");
}

void UW_VulturesAtTakeover::SetupVulturesAt()
{
	SetKindLabel(NSLOCTEXT("VulturesAtTakeover", "KindLabel", "VULTURES  -  SQUAD ABILITY"));
	SetTitleText(GetVulturesAtTitle());
	SetDescriptionText(NSLOCTEXT("VulturesAtTakeover", "Description",
	                             "The Vultures carry stick grenades rigged to cling to armour. Select the squad, use "
	                             "the AT Stick Grenades card and click an enemy vehicle."));
	BuildClipBody();
}

TArray<FClipTakeoverFact> UW_VulturesAtTakeover::MakeFacts() const
{
	// From the ATStickGrenade component (GrenadeQuality.md): four throwers, sticks to vehicles, 1 s fuse, 20 s
	// cooldown, 800 range; damage is dealt against rear armour (FRTS_AOE::DealDamageVsRearArmorInRadiusAsync) and an
	// out-of-range order walks the squad into range first. Kept qualitative so balance passes do not make it wrong.
	TArray<FClipTakeoverFact> Facts;
	FClipTakeoverFact& VolleyFact = Facts.AddDefaulted_GetRef();
	VolleyFact.Label = NSLOCTEXT("VulturesAtTakeover", "VolleyLabel", "VOLLEY");
	VolleyFact.Title = NSLOCTEXT("VulturesAtTakeover", "VolleyTitle", "Four grenades at once");
	VolleyFact.Body = NSLOCTEXT("VulturesAtTakeover", "VolleyBody",
	                            "Every Vulture throws. The grenades stick to the hull and go off a moment later, even "
	                            "on a tank that is driving.");

	FClipTakeoverFact& TargetFact = Facts.AddDefaulted_GetRef();
	TargetFact.Label = NSLOCTEXT("VulturesAtTakeover", "TargetLabel", "TARGETS");
	TargetFact.Title = NSLOCTEXT("VulturesAtTakeover", "TargetTitle", "Light and medium tanks");
	TargetFact.Body = NSLOCTEXT("VulturesAtTakeover", "TargetBody",
	                            "One volley wrecks most light tanks and badly hurts medium ones. The blast always "
	                            "strikes the thinnest plate, from any side; thick armour still soaks up much of it.");

	FClipTakeoverFact& ReloadFact = Facts.AddDefaulted_GetRef();
	ReloadFact.Label = NSLOCTEXT("VulturesAtTakeover", "ReloadLabel", "RELOAD");
	ReloadFact.Title = NSLOCTEXT("VulturesAtTakeover", "ReloadTitle", "Pick the moment");
	ReloadFact.Body = NSLOCTEXT("VulturesAtTakeover", "ReloadBody",
	                            "The squad needs a long while before it can throw again. The throw is short: the "
	                            "squad walks up to the target first, into its guns.");
	return Facts;
}

FText UW_VulturesAtTakeover::GetClipCaption() const
{
	return NSLOCTEXT("VulturesAtTakeover", "ClipCaption", "Vultures stick their grenades onto an advancing T-26.");
}
