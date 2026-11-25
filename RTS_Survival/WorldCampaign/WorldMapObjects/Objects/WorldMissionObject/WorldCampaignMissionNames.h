// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"

struct FWorldCampaignPinnedPlace;
struct FWorldMissionNameDefinition;

/**
 * @brief The name a mission carries on the campaign map: on its map tag and as the title of its briefing panel.
 * Kept as pure functions over the map content settings so the rule is unit tested.
 */
namespace WorldCampaignMissionNames
{
	/**
	 * @brief Looks a mission's name up: its own entry first, else the name of the pinned place that holds it.
	 * @param Mission Mission to name.
	 * @param MissionNames Explicit names from the map content settings.
	 * @param PinnedPlaces Pinned places from the map content settings.
	 * @return The name; empty when the mission has neither, so the caller keeps what it had.
	 */
	RTS_SURVIVAL_API FText FindMissionDisplayName(EMapMission Mission,
	                                              const TArray<FWorldMissionNameDefinition>& MissionNames,
	                                              const TArray<FWorldCampaignPinnedPlace>& PinnedPlaces);

	/** @return FindMissionDisplayName on the project's map content settings. */
	RTS_SURVIVAL_API FText GetMissionDisplayName(EMapMission Mission);

	/**
	 * @brief Puts a mission name into the rich text style of the title it replaces, so the briefing panel keeps the
	 * look its Blueprint authored.
	 * @param AuthoredTitle Title on the mission object, for example "<Text_LBadTitle>Clear the road</>".
	 * @param MissionName Name to show instead.
	 * @return The name inside the authored title's style tag; the plain name when the title has no tag.
	 */
	RTS_SURVIVAL_API FText MakeMissionTitleText(const FText& AuthoredTitle, const FText& MissionName);
}
