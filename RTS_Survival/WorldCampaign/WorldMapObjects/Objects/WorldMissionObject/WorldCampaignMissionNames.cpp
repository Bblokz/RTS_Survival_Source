// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldCampaignMissionNames.h"

#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignMapContentSettings.h"

namespace WorldCampaignMissionNamesPrivate
{
	const TCHAR StyleTagOpen = TEXT('<');
	const TCHAR StyleTagClose = TEXT('>');
	const TCHAR StyleTagEndMarker = TEXT('/');

	/** @return The style name of a title that starts with "<Style>"; empty when it does not. */
	FString GetLeadingStyleTag(const FString& RichText)
	{
		int32 CloseIndex = INDEX_NONE;
		if (not RichText.StartsWith(FString::Chr(StyleTagOpen)) || not RichText.FindChar(StyleTagClose, CloseIndex))
		{
			return FString();
		}

		const FString StyleTag = RichText.Mid(1, CloseIndex - 1);
		// "</>" closes a style; it is not one.
		return StyleTag.StartsWith(FString::Chr(StyleTagEndMarker)) ? FString() : StyleTag;
	}
}

FText WorldCampaignMissionNames::FindMissionDisplayName(const EMapMission Mission,
                                                        const TArray<FWorldMissionNameDefinition>& MissionNames,
                                                        const TArray<FWorldCampaignPinnedPlace>& PinnedPlaces)
{
	if (Mission == EMapMission::None)
	{
		return FText::GetEmpty();
	}

	for (const FWorldMissionNameDefinition& MissionName : MissionNames)
	{
		if (MissionName.Mission == Mission && not MissionName.DisplayName.IsEmpty())
		{
			return MissionName.DisplayName;
		}
	}

	for (const FWorldCampaignPinnedPlace& PinnedPlace : PinnedPlaces)
	{
		const bool bHoldsMission = PinnedPlace.Role == EWorldPinnedPlaceRole::Mission && PinnedPlace.Mission == Mission;
		if (bHoldsMission && not PinnedPlace.DisplayName.IsEmpty())
		{
			return PinnedPlace.DisplayName;
		}
	}

	return FText::GetEmpty();
}

FText WorldCampaignMissionNames::GetMissionDisplayName(const EMapMission Mission)
{
	const UWorldCampaignMapContentSettings* const MapContentSettings = UWorldCampaignMapContentSettings::Get();
	if (MapContentSettings == nullptr)
	{
		return FText::GetEmpty();
	}

	return FindMissionDisplayName(Mission, MapContentSettings->MissionNames, MapContentSettings->PinnedPlaces);
}

FText WorldCampaignMissionNames::MakeMissionTitleText(const FText& AuthoredTitle, const FText& MissionName)
{
	const FString StyleTag = WorldCampaignMissionNamesPrivate::GetLeadingStyleTag(AuthoredTitle.ToString());
	if (StyleTag.IsEmpty())
	{
		return MissionName;
	}

	return FText::Format(INVTEXT("<{0}>{1}</>"), FText::FromString(StyleTag), MissionName);
}
