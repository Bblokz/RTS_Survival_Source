// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignMapContentSettings.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldCampaignMissionNames.h"

namespace WorldCampaignMissionNamesTestData
{
	FWorldCampaignPinnedPlace MakePin(const EWorldPinnedPlaceRole Role, const EMapMission Mission, const TCHAR* Name)
	{
		FWorldCampaignPinnedPlace PinnedPlace;
		PinnedPlace.Role = Role;
		PinnedPlace.Mission = Mission;
		PinnedPlace.DisplayName = FText::FromString(Name);
		return PinnedPlace;
	}

	FWorldMissionNameDefinition MakeName(const EMapMission Mission, const TCHAR* Name)
	{
		FWorldMissionNameDefinition MissionName;
		MissionName.Mission = Mission;
		MissionName.DisplayName = FText::FromString(Name);
		return MissionName;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignMissionNameLookupTest, "RTS.WorldCampaign.MissionNames.Lookup",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignMissionNameLookupTest::RunTest(const FString& Parameters)
{
	using namespace WorldCampaignMissionNamesTestData;
	const TArray<FWorldCampaignPinnedPlace> PinnedPlaces = {
		MakePin(EWorldPinnedPlaceRole::PlayerHQ, EMapMission::None, TEXT("Harz Mountains")),
		MakePin(EWorldPinnedPlaceRole::Mission, EMapMission::Berlin, TEXT("Berlin")),
		MakePin(EWorldPinnedPlaceRole::Mission, EMapMission::OderWarthe, TEXT("Oder-Warthe Line")),
		// A place whose role is not Mission never names a mission, whatever its mission field says.
		MakePin(EWorldPinnedPlaceRole::EnemyWall, EMapMission::FrozenLake, TEXT("Svarog Line"))
	};
	const TArray<FWorldMissionNameDefinition> MissionNames = {
		MakeName(EMapMission::OderWarthe, TEXT("Operation Peilung")),
		MakeName(EMapMission::HetzerMission, TEXT("Hetzer Works")),
		MakeName(EMapMission::Berlin, TEXT(""))
	};

	TestEqual(TEXT("A pinned mission is named after its place"),
	          WorldCampaignMissionNames::FindMissionDisplayName(EMapMission::Berlin, {}, PinnedPlaces).ToString(),
	          FString(TEXT("Berlin")));
	TestEqual(TEXT("An explicit name wins over the place"),
	          WorldCampaignMissionNames::FindMissionDisplayName(EMapMission::OderWarthe, MissionNames, PinnedPlaces)
	          .ToString(), FString(TEXT("Operation Peilung")));
	TestEqual(TEXT("A mission on no pin takes its explicit name"),
	          WorldCampaignMissionNames::FindMissionDisplayName(EMapMission::HetzerMission, MissionNames, PinnedPlaces)
	          .ToString(), FString(TEXT("Hetzer Works")));
	TestEqual(TEXT("An empty explicit name falls back to the place"),
	          WorldCampaignMissionNames::FindMissionDisplayName(EMapMission::Berlin, MissionNames, PinnedPlaces)
	          .ToString(), FString(TEXT("Berlin")));
	TestTrue(TEXT("A mission without a name stays unnamed"),
	         WorldCampaignMissionNames::FindMissionDisplayName(EMapMission::SturmtigerDam, MissionNames, PinnedPlaces)
	         .IsEmpty());
	TestTrue(TEXT("A non-mission place names no mission"),
	         WorldCampaignMissionNames::FindMissionDisplayName(EMapMission::FrozenLake, MissionNames, PinnedPlaces)
	         .IsEmpty());
	TestTrue(TEXT("None is never named"),
	         WorldCampaignMissionNames::FindMissionDisplayName(EMapMission::None, MissionNames, PinnedPlaces).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignMissionTitleTextTest, "RTS.WorldCampaign.MissionNames.TitleText",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignMissionTitleTextTest::RunTest(const FString& Parameters)
{
	const FText MissionName = FText::FromString(TEXT("Berlin"));

	TestEqual(TEXT("The name takes the authored title's style"),
	          WorldCampaignMissionNames::MakeMissionTitleText(
		          FText::FromString(TEXT("<Text_LBadTitle>Clear the road</>")), MissionName).ToString(),
	          FString(TEXT("<Text_LBadTitle>Berlin</>")));
	TestEqual(TEXT("A plain title gives a plain name"),
	          WorldCampaignMissionNames::MakeMissionTitleText(FText::FromString(TEXT("Clear the road")), MissionName)
	          .ToString(), FString(TEXT("Berlin")));
	TestEqual(TEXT("An empty title gives a plain name"),
	          WorldCampaignMissionNames::MakeMissionTitleText(FText::GetEmpty(), MissionName).ToString(),
	          FString(TEXT("Berlin")));
	TestEqual(TEXT("A closing tag alone is not a style"),
	          WorldCampaignMissionNames::MakeMissionTitleText(FText::FromString(TEXT("</>Road")), MissionName)
	          .ToString(), FString(TEXT("Berlin")));
	return true;
}

#endif
