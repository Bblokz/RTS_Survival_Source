// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_DirectionFindingTakeover.h"

#include "Engine/Texture2D.h"

namespace DirectionFindingTakeoverPrivate
{
	const TCHAR* const ClipAtlasPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/DirectionFinding/T_DirectionFinding_ClipAtlas."
		"T_DirectionFinding_ClipAtlas");
	// Must match make_clip_atlas.py's output for the clip (Source/Tools/InfoTakeover/DirectionFinding/README.md).
	constexpr int32 ClipColumns = 8;
	constexpr int32 ClipRows = 12;
	constexpr int32 ClipFrameCount = 96;
}

UW_DirectionFindingTakeover::UW_DirectionFindingTakeover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	using namespace DirectionFindingTakeoverPrivate;
	M_ClipAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(ClipAtlasPath));
	M_ClipColumns = ClipColumns;
	M_ClipRows = ClipRows;
	M_ClipFrameCount = ClipFrameCount;
}

FText UW_DirectionFindingTakeover::GetDirectionFindingTitle()
{
	return NSLOCTEXT("DirectionFindingTakeover", "Title", "Direction Finding");
}

FText UW_DirectionFindingTakeover::GetDirectionFindingSummary()
{
	return NSLOCTEXT("DirectionFindingTakeover", "ArchiveSummary",
	                 "Shield's command post hides in one of the five bunker groups. Its jammer gives no bearing, but "
	                 "its own orders have to cut through it, and our listening posts can hear them.\n\n"
	                 "Listen: every listening post you hold draws a wedge on the minimap and the tactical map. The "
	                 "command post is somewhere inside it. A lost post keeps its wedge, grey, and continues once you "
	                 "retake it.\n\n"
	                 "Narrow: every transmission narrows the wedges. Attacks are ordered by radio, so when they attack, "
	                 "they talk. Hit an outpost and the command post answers.\n\n"
	                 "Cross: two wedges overlap on a rough area. Three, from different angles, find the command post "
	                 "and reveal its targets.");
}

void UW_DirectionFindingTakeover::SetupDirectionFinding()
{
	SetKindLabel(NSLOCTEXT("DirectionFindingTakeover", "KindLabel", "ODER-WARTHE  -  RADIO DIRECTION FINDING"));
	SetTitleText(GetDirectionFindingTitle());
	SetDescriptionText(NSLOCTEXT("DirectionFindingTakeover", "Description",
	                             "The command post hides in one of five bunker groups. Its jammer gives no bearing, "
	                             "but its own orders cut through, and the listening posts hear them."));
	BuildClipBody();
}

TArray<FClipTakeoverFact> UW_DirectionFindingTakeover::MakeFacts() const
{
	// From ADirectionFindingManager (2026-11-24): a held station opens a wedge, a lost one keeps it (grey) and stops
	// narrowing; routine, reply and attack-order bursts narrow every held wedge; three held wedges around one standing
	// Werk (or all five stations) fix the command post and show its targets. Kept free of the tuning numbers.
	TArray<FClipTakeoverFact> Facts;
	FClipTakeoverFact& ListenFact = Facts.AddDefaulted_GetRef();
	ListenFact.Label = NSLOCTEXT("DirectionFindingTakeover", "ListenLabel", "LISTEN");
	ListenFact.Title = NSLOCTEXT("DirectionFindingTakeover", "ListenTitle", "Hold a listening post");
	ListenFact.Body = NSLOCTEXT("DirectionFindingTakeover", "ListenBody",
	                            "Each post you hold draws a wedge on the map: the command post is inside it. A lost "
	                            "post keeps its wedge, grey, until you retake it.");

	FClipTakeoverFact& NarrowFact = Facts.AddDefaulted_GetRef();
	NarrowFact.Label = NSLOCTEXT("DirectionFindingTakeover", "NarrowLabel", "NARROW");
	NarrowFact.Title = NSLOCTEXT("DirectionFindingTakeover", "NarrowTitle", "When they attack, they talk");
	NarrowFact.Body = NSLOCTEXT("DirectionFindingTakeover", "NarrowBody",
	                            "Every transmission narrows the wedges. Attacks are ordered by radio. Hit an outpost "
	                            "and the command post answers.");

	FClipTakeoverFact& CrossFact = Facts.AddDefaulted_GetRef();
	CrossFact.Label = NSLOCTEXT("DirectionFindingTakeover", "CrossLabel", "CROSS");
	CrossFact.Title = NSLOCTEXT("DirectionFindingTakeover", "CrossTitle", "Cross the bearings");
	CrossFact.Body = NSLOCTEXT("DirectionFindingTakeover", "CrossBody",
	                           "Two wedges overlap on a rough area. Three, from different angles, find the command "
	                           "post and reveal its targets.");
	return Facts;
}

FText UW_DirectionFindingTakeover::GetClipCaption() const
{
	return NSLOCTEXT("DirectionFindingTakeover", "ClipCaption",
	                 "The pump house takes a bearing; an attack order narrows it, and a second post crosses it.");
}
