// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/PinnedPlaces/WorldCampaignPinnedPlaces.h"

namespace WorldCampaignPinnedPlacesTestData
{
	const FName BerlinPinId(TEXT("Berlin"));
	const FName HarzPinId(TEXT("Harz"));
	const FName UralPinId(TEXT("UralShield"));
	const FName SvarogPinId(TEXT("SvarogLine"));
	constexpr int32 MinSpacingHops = 2;
	constexpr int32 MaxSpacingHops = 4;
	constexpr int32 FarHopDistance = 9;
	constexpr float MinSpacingXY = 100.f;
	constexpr float MaxSpacingXY = 500.f;
	constexpr float FarXYDistance = 5000.f;

	FWorldCampaignPinnedPlace MakePlace(const FName PinId, const EWorldPinnedPlaceRole Role,
	                                    const EMapMission Mission = EMapMission::None)
	{
		FWorldCampaignPinnedPlace Place;
		Place.PinId = PinId;
		Place.Role = Role;
		Place.Mission = Mission;
		return Place;
	}

	FWorldCampaignResolvedPin MakePin(const FName PinId, const EWorldPinnedPlaceRole Role,
	                                  const EMapMission Mission = EMapMission::None)
	{
		FWorldCampaignResolvedPin Pin;
		Pin.PinId = PinId;
		Pin.Role = Role;
		Pin.Mission = Mission;
		Pin.AnchorKey = WorldCampaignPinnedPlaces::BuildPinnedAnchorKey(PinId);
		return Pin;
	}

	FWorldCampaignResolvedPins MakeStoryPins()
	{
		FWorldCampaignResolvedPins Pins;
		Pins.AddPin(MakePin(HarzPinId, EWorldPinnedPlaceRole::PlayerHQ));
		Pins.AddPin(MakePin(UralPinId, EWorldPinnedPlaceRole::EnemyHQ));
		Pins.AddPin(MakePin(SvarogPinId, EWorldPinnedPlaceRole::EnemyWall));
		Pins.AddPin(MakePin(BerlinPinId, EWorldPinnedPlaceRole::Mission, EMapMission::Berlin));
		return Pins;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignPinsDeterministicKeyTest, "RTS.WorldCampaign.Pins.DeterministicKey",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignPinsDeterministicKeyTest::RunTest(const FString& Parameters)
{
	using namespace WorldCampaignPinnedPlacesTestData;
	const FGuid BerlinKey = WorldCampaignPinnedPlaces::BuildPinnedAnchorKey(BerlinPinId);
	TestTrue(TEXT("A pin id gives a valid key"), BerlinKey.IsValid());
	TestTrue(TEXT("The key is the same on every call"),
	         WorldCampaignPinnedPlaces::BuildPinnedAnchorKey(BerlinPinId) == BerlinKey);
	TestTrue(TEXT("The key ignores pin id casing"),
	         WorldCampaignPinnedPlaces::BuildPinnedAnchorKey(FName(TEXT("BERLIN"))) == BerlinKey);
	TestTrue(TEXT("Different pin ids give different keys"),
	         WorldCampaignPinnedPlaces::BuildPinnedAnchorKey(HarzPinId) != BerlinKey);
	TestFalse(TEXT("NAME_None gives an invalid key"),
	          WorldCampaignPinnedPlaces::BuildPinnedAnchorKey(NAME_None).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignPinsUsablePlacesTest, "RTS.WorldCampaign.Pins.UsablePlaces",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignPinsUsablePlacesTest::RunTest(const FString& Parameters)
{
	using namespace WorldCampaignPinnedPlacesTestData;
	TArray<FWorldCampaignPinnedPlace> Configured;
	Configured.Add(MakePlace(HarzPinId, EWorldPinnedPlaceRole::PlayerHQ));
	Configured.Add(MakePlace(BerlinPinId, EWorldPinnedPlaceRole::Mission, EMapMission::Berlin));
	Configured.Add(MakePlace(BerlinPinId, EWorldPinnedPlaceRole::Mission, EMapMission::OderWarthe));
	Configured.Add(MakePlace(UralPinId, EWorldPinnedPlaceRole::PlayerHQ));
	Configured.Add(MakePlace(SvarogPinId, EWorldPinnedPlaceRole::Mission, EMapMission::None));
	Configured.Add(MakePlace(NAME_None, EWorldPinnedPlaceRole::EnemyHQ));

	TArray<FString> Warnings;
	const TArray<FWorldCampaignPinnedPlace> Usable =
		WorldCampaignPinnedPlaces::BuildUsablePinnedPlaces(true, Configured, Warnings);
	TestEqual(TEXT("Only the HQ and the first Berlin pin are usable"), Usable.Num(), 2);
	TestEqual(TEXT("Every dropped entry gives a warning"), Warnings.Num(), 4);
	if (Usable.Num() == 2)
	{
		TestTrue(TEXT("Config order is kept"), Usable[0].PinId == HarzPinId);
		TestTrue(TEXT("The first duplicate wins"), Usable[1].Mission == EMapMission::Berlin);
	}

	TArray<FString> DisabledWarnings;
	TestEqual(TEXT("Disabled pins give an empty list"),
	          WorldCampaignPinnedPlaces::BuildUsablePinnedPlaces(false, Configured, DisabledWarnings).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignPinsPlanFilterTest, "RTS.WorldCampaign.Pins.PlanFilter",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignPinsPlanFilterTest::RunTest(const FString& Parameters)
{
	using namespace WorldCampaignPinnedPlacesTestData;
	const TArray<EMapMission> OriginalPlan = {
		EMapMission::HetzerMission, EMapMission::ME410UnlockMission, EMapMission::Berlin, EMapMission::FrozenLake
	};

	TArray<EMapMission> UnpinnedPlan = OriginalPlan;
	WorldCampaignPinnedPlaces::RemovePinnedMissionsFromPlan(FWorldCampaignResolvedPins(), UnpinnedPlan);
	TestTrue(TEXT("No pins leaves the plan unchanged"), UnpinnedPlan == OriginalPlan);

	FWorldCampaignResolvedPins Pins = MakeStoryPins();
	Pins.AddPin(MakePin(FName(TEXT("Pripyat")), EWorldPinnedPlaceRole::Mission, EMapMission::ME410UnlockMission));
	TArray<EMapMission> PinnedPlan = OriginalPlan;
	WorldCampaignPinnedPlaces::RemovePinnedMissionsFromPlan(Pins, PinnedPlan);
	const TArray<EMapMission> ExpectedPlan = {EMapMission::HetzerMission, EMapMission::FrozenLake};
	TestTrue(TEXT("Pinned missions are removed and the order of the rest is kept"), PinnedPlan == ExpectedPlan);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignPinsPreSeedTest, "RTS.WorldCampaign.Pins.PreSeed",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignPinsPreSeedTest::RunTest(const FString& Parameters)
{
	using namespace WorldCampaignPinnedPlacesTestData;
	const FWorldCampaignResolvedPins Pins = MakeStoryPins();
	const FGuid BerlinKey = WorldCampaignPinnedPlaces::BuildPinnedAnchorKey(BerlinPinId);

	TMap<FGuid, EMapMission> MissionsByAnchorKey;
	const int32 Written = WorldCampaignPinnedPlaces::ApplyPinnedMissionPreSeed(
		Pins, [](const FGuid&) { return true; }, MissionsByAnchorKey);
	TestEqual(TEXT("Only mission pins are written"), Written, 1);
	TestTrue(TEXT("Berlin sits on its pinned anchor"), MissionsByAnchorKey.FindRef(BerlinKey) == EMapMission::Berlin);

	const int32 WrittenAgain = WorldCampaignPinnedPlaces::ApplyPinnedMissionPreSeed(
		Pins, [](const FGuid&) { return true; }, MissionsByAnchorKey);
	TestEqual(TEXT("Re-applying the pre-seed is a no-op"), WrittenAgain, 0);

	TMap<FGuid, EMapMission> MissingAnchorMissions;
	WorldCampaignPinnedPlaces::ApplyPinnedMissionPreSeed(
		Pins, [](const FGuid&) { return false; }, MissingAnchorMissions);
	TestEqual(TEXT("Pins without an anchor in the graph are skipped"), MissingAnchorMissions.Num(), 0);

	TestTrue(TEXT("Berlin is a story mission anchor"), Pins.GetIsPinnedMissionAnchorKey(BerlinKey));
	TestFalse(TEXT("The player HQ pin is not a story mission"),
	          Pins.GetIsPinnedMissionAnchorKey(WorldCampaignPinnedPlaces::BuildPinnedAnchorKey(HarzPinId)));
	FGuid FoundKey;
	TestTrue(TEXT("Pin id lookup works"), Pins.TryGetAnchorKeyForPinId(BerlinPinId, FoundKey));
	TestTrue(TEXT("Pin id lookup returns the derived key"), FoundKey == BerlinKey);
	TestTrue(TEXT("Key lookup returns the pin id"), Pins.GetPinIdForAnchorKey(BerlinKey) == BerlinPinId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignPinsCandidateRestrictionTest, "RTS.WorldCampaign.Pins.CandidateRestriction",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignPinsCandidateRestrictionTest::RunTest(const FString& Parameters)
{
	using namespace WorldCampaignPinnedPlacesTestData;
	const FWorldCampaignResolvedPins Pins = MakeStoryPins();
	const FGuid FreeKeyA = FGuid::NewDeterministicGuid(TEXT("PinsTest.FreeA"));
	const FGuid FreeKeyB = FGuid::NewDeterministicGuid(TEXT("PinsTest.FreeB"));
	const FGuid HarzKey = Pins.GetAnchorKeyForRole(EWorldPinnedPlaceRole::PlayerHQ);
	const FGuid UralKey = Pins.GetAnchorKeyForRole(EWorldPinnedPlaceRole::EnemyHQ);
	const TArray<FGuid> SourceKeys = {FreeKeyA, UralKey, HarzKey, FreeKeyB};

	const TArray<int32> PinnedPositions =
		WorldCampaignPinnedPlaces::SelectCandidatePositionsForRole(SourceKeys, HarzKey, Pins.GetPinnedAnchorKeys());
	TestEqual(TEXT("A pinned role keeps exactly one candidate"), PinnedPositions.Num(), 1);
	if (PinnedPositions.Num() == 1)
	{
		TestEqual(TEXT("The kept candidate is the pin"), PinnedPositions[0], 2);
	}

	const TArray<int32> UnpinnedPositions =
		WorldCampaignPinnedPlaces::SelectCandidatePositionsForRole(SourceKeys, FGuid(), Pins.GetPinnedAnchorKeys());
	const TArray<int32> ExpectedUnpinned = {0, 3};
	TestTrue(TEXT("An unpinned role skips every pinned anchor"), UnpinnedPositions == ExpectedUnpinned);

	const TArray<int32> MissingPinPositions = WorldCampaignPinnedPlaces::SelectCandidatePositionsForRole(
		{FreeKeyA, FreeKeyB}, HarzKey, Pins.GetPinnedAnchorKeys());
	TestEqual(TEXT("A pin missing from the graph never falls back to another anchor"), MissingPinPositions.Num(), 0);

	const TArray<int32> NoPinPositions =
		WorldCampaignPinnedPlaces::SelectCandidatePositionsForRole(SourceKeys, FGuid(), TSet<FGuid>());
	TestEqual(TEXT("Without pins every candidate is kept"), NoPinPositions.Num(), SourceKeys.Num());
	TestTrue(TEXT("The wall pin is reserved"), Pins.GetIsPinnedAnchorKey(
		         WorldCampaignPinnedPlaces::BuildPinnedAnchorKey(SvarogPinId)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignPinsMissionSpacingTest, "RTS.WorldCampaign.Pins.MissionSpacing",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignPinsMissionSpacingTest::RunTest(const FString& Parameters)
{
	using namespace WorldCampaignPinnedPlacesTestData;
	using WorldCampaignPinnedPlaces::PassesMissionHopSpacingBand;
	using WorldCampaignPinnedPlaces::PassesMissionXYSpacingBand;

	TestTrue(TEXT("Seeded mission inside the hop band passes"),
	         PassesMissionHopSpacingBand(MinSpacingHops, MinSpacingHops, MaxSpacingHops, false));
	TestFalse(TEXT("Seeded mission beyond max hops fails"),
	          PassesMissionHopSpacingBand(FarHopDistance, MinSpacingHops, MaxSpacingHops, false));
	TestFalse(TEXT("Unreachable seeded mission fails"),
	          PassesMissionHopSpacingBand(INDEX_NONE, MinSpacingHops, MaxSpacingHops, false));
	TestTrue(TEXT("Pinned mission beyond max hops is ignored"),
	         PassesMissionHopSpacingBand(FarHopDistance, MinSpacingHops, MaxSpacingHops, true));
	TestTrue(TEXT("Unreachable pinned mission is ignored"),
	         PassesMissionHopSpacingBand(INDEX_NONE, MinSpacingHops, MaxSpacingHops, true));
	TestFalse(TEXT("Pinned mission still enforces min hops"),
	          PassesMissionHopSpacingBand(MinSpacingHops - 1, MinSpacingHops, MaxSpacingHops, true));

	TestFalse(TEXT("Seeded mission beyond max XY fails"),
	          PassesMissionXYSpacingBand(FarXYDistance, MinSpacingXY, MaxSpacingXY, false));
	TestTrue(TEXT("Pinned mission beyond max XY is ignored"),
	         PassesMissionXYSpacingBand(FarXYDistance, MinSpacingXY, MaxSpacingXY, true));
	TestFalse(TEXT("Pinned mission still enforces min XY"),
	          PassesMissionXYSpacingBand(MinSpacingXY * 0.5f, MinSpacingXY, MaxSpacingXY, true));
	return true;
}

#endif
