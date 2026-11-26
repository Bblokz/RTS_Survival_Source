// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RTS_Survival/WorldCampaign/BatteryFire/WorldBatteryFireRules.h"
#include "RTS_Survival/WorldCampaign/BatteryFire/WorldBatteryFireSettings.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionOwners.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionTankFormation.h"

namespace WorldBatteryFireTestData
{
	const FName BatteryNodeA = TEXT("Battery_A");
	const FName BatteryNodeB = TEXT("Battery_B");
	constexpr float Range = 3000.f;
	constexpr float InsideRange = 2000.f;
	constexpr float OutsideRange = 3500.f;
	constexpr int32 TurnThree = 3;
	constexpr int32 TurnFour = 4;
	constexpr float Tolerance = 0.01f;

	FWorldBatteryFireNumbers MakeNumbers()
	{
		FWorldBatteryFireNumbers Numbers;
		Numbers.Range = Range;
		Numbers.MissionsPerTurn = 1;
		Numbers.EntriesDestroyedPerMission = 2;
		return Numbers;
	}

	// A built battery at the origin that has not fired yet.
	FWorldBatteryFireBattery MakeReadyBattery()
	{
		FWorldBatteryFireBattery Battery;
		Battery.bIsKnownNode = true;
		Battery.NodeState = EWorldBuildNodeState::Built;
		Battery.Building = EWorldCampaignBuilding::Battery;
		Battery.Location = FVector::ZeroVector;
		Battery.MissionsFiredThisTurn = 0;
		return Battery;
	}

	// A visible enemy division at the given distance east of the origin.
	FWorldBatteryFireTarget MakeEnemyTarget(const float Distance)
	{
		FWorldBatteryFireTarget Target;
		Target.DivisionKey = FGuid::NewGuid();
		Target.bExists = true;
		Target.OwningPlayer = WorldDivisionOwners::EnemyOwner;
		Target.Location = FVector(Distance, 0.f, 0.f);
		Target.bIsVisibleThroughFog = true;
		return Target;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBatteryFireRefusalsTest, "RTS.WorldCampaign.BatteryFire.Refusals",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBatteryFireRefusalsTest::RunTest(const FString& Parameters)
{
	using namespace WorldBatteryFireTestData;
	const FWorldBatteryFireNumbers Numbers = MakeNumbers();
	const FWorldBatteryFireTarget GoodTarget = MakeEnemyTarget(InsideRange);
	TestEqual(TEXT("A ready battery fires on a visible enemy in range"),
	          WorldBatteryFireRules::GetFireMissionResult(MakeReadyBattery(), GoodTarget, Numbers),
	          EWorldBatteryFireResult::Success);

	FWorldBatteryFireBattery UnknownNode = MakeReadyBattery();
	UnknownNode.bIsKnownNode = false;
	TestEqual(TEXT("Unknown node"), WorldBatteryFireRules::GetFireMissionResult(UnknownNode, GoodTarget, Numbers),
	          EWorldBatteryFireResult::UnknownBattery);

	FWorldBatteryFireBattery Airfield = MakeReadyBattery();
	Airfield.Building = EWorldCampaignBuilding::Airfield;
	TestEqual(TEXT("Another building is no battery"),
	          WorldBatteryFireRules::GetFireMissionResult(Airfield, GoodTarget, Numbers),
	          EWorldBatteryFireResult::NotABattery);

	FWorldBatteryFireBattery EmptySite = MakeReadyBattery();
	EmptySite.NodeState = EWorldBuildNodeState::Unlocked;
	EmptySite.Building = EWorldCampaignBuilding::None;
	TestEqual(TEXT("An empty site is no battery"),
	          WorldBatteryFireRules::GetFireMissionResult(EmptySite, GoodTarget, Numbers),
	          EWorldBatteryFireResult::NotABattery);

	FWorldBatteryFireBattery SpentBattery = MakeReadyBattery();
	SpentBattery.MissionsFiredThisTurn = 1;
	TestEqual(TEXT("One mission per turn"),
	          WorldBatteryFireRules::GetFireMissionResult(SpentBattery, GoodTarget, Numbers),
	          EWorldBatteryFireResult::NoMissionsLeft);

	FWorldBatteryFireTarget MissingTarget = GoodTarget;
	MissingTarget.bExists = false;
	TestEqual(TEXT("Missing target"),
	          WorldBatteryFireRules::GetFireMissionResult(MakeReadyBattery(), MissingTarget, Numbers),
	          EWorldBatteryFireResult::UnknownTarget);

	FWorldBatteryFireTarget OwnPiece = GoodTarget;
	OwnPiece.OwningPlayer = WorldDivisionOwners::PlayerOwner;
	TestEqual(TEXT("Only enemy divisions"),
	          WorldBatteryFireRules::GetFireMissionResult(MakeReadyBattery(), OwnPiece, Numbers),
	          EWorldBatteryFireResult::TargetNotEnemy);

	TestEqual(TEXT("Out of range"),
	          WorldBatteryFireRules::GetFireMissionResult(MakeReadyBattery(), MakeEnemyTarget(OutsideRange), Numbers),
	          EWorldBatteryFireResult::TargetOutOfRange);

	FWorldBatteryFireTarget HiddenTarget = GoodTarget;
	HiddenTarget.bIsVisibleThroughFog = false;
	TestEqual(TEXT("Hidden by fog"),
	          WorldBatteryFireRules::GetFireMissionResult(MakeReadyBattery(), HiddenTarget, Numbers),
	          EWorldBatteryFireResult::TargetHiddenByFog);

	FWorldBatteryFireTarget HighTarget = MakeEnemyTarget(InsideRange);
	HighTarget.Location.Z = OutsideRange;
	TestEqual(TEXT("Range is measured on the map plane"),
	          WorldBatteryFireRules::GetFireMissionResult(MakeReadyBattery(), HighTarget, Numbers),
	          EWorldBatteryFireResult::Success);

	TestTrue(TEXT("Success has no refusal text"),
	         WorldBatteryFireRules::GetFireResultText(EWorldBatteryFireResult::Success).IsEmpty());
	TestFalse(TEXT("A refusal has a text"),
	          WorldBatteryFireRules::GetFireResultText(EWorldBatteryFireResult::NoMissionsLeft).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBatteryFireTargetsTest, "RTS.WorldCampaign.BatteryFire.ValidTargets",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBatteryFireTargetsTest::RunTest(const FString& Parameters)
{
	using namespace WorldBatteryFireTestData;
	const FWorldBatteryFireNumbers Numbers = MakeNumbers();
	const FWorldBatteryFireTarget NearTarget = MakeEnemyTarget(InsideRange);
	const FWorldBatteryFireTarget FarTarget = MakeEnemyTarget(OutsideRange);
	FWorldBatteryFireTarget HiddenTarget = MakeEnemyTarget(InsideRange);
	HiddenTarget.bIsVisibleThroughFog = false;
	const TArray<FWorldBatteryFireTarget> Targets = {FarTarget, NearTarget, HiddenTarget};

	const TArray<FGuid> ValidKeys = WorldBatteryFireRules::GetValidTargetKeys(MakeReadyBattery(), Targets, Numbers);
	TestEqual(TEXT("Only the visible target in range is valid"), ValidKeys.Num(), 1);
	TestTrue(TEXT("The valid target is the near one"), ValidKeys.Contains(NearTarget.DivisionKey));
	TestEqual(TEXT("A battery with a target is ready"),
	          WorldBatteryFireRules::GetBatteryReadiness(MakeReadyBattery(), Targets, Numbers),
	          EWorldBatteryFireResult::Success);

	const TArray<FWorldBatteryFireTarget> NoGoodTargets = {FarTarget, HiddenTarget};
	TestEqual(TEXT("No target in range"),
	          WorldBatteryFireRules::GetBatteryReadiness(MakeReadyBattery(), NoGoodTargets, Numbers),
	          EWorldBatteryFireResult::NoTargetInRange);

	FWorldBatteryFireBattery SpentBattery = MakeReadyBattery();
	SpentBattery.MissionsFiredThisTurn = 1;
	TestEqual(TEXT("A spent battery says so before it looks for targets"),
	          WorldBatteryFireRules::GetBatteryReadiness(SpentBattery, Targets, Numbers),
	          EWorldBatteryFireResult::NoMissionsLeft);
	TestEqual(TEXT("A spent battery has no valid targets"),
	          WorldBatteryFireRules::GetValidTargetKeys(SpentBattery, Targets, Numbers).Num(), 0);

	FWorldBatteryFireNumbers TwoPerTurn = Numbers;
	TwoPerTurn.MissionsPerTurn = 2;
	TestEqual(TEXT("The missions per turn setting is honoured"),
	          WorldBatteryFireRules::GetBatteryReadiness(SpentBattery, Targets, TwoPerTurn),
	          EWorldBatteryFireResult::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBatteryFireTurnStateTest, "RTS.WorldCampaign.BatteryFire.TurnState",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBatteryFireTurnStateTest::RunTest(const FString& Parameters)
{
	using namespace WorldBatteryFireTestData;
	FWorldBatteryFireTurnState TurnState;
	TestEqual(TEXT("Nothing fired yet"),
	          WorldBatteryFireRules::GetMissionsFiredThisTurn(TurnState, BatteryNodeA, TurnThree), 0);

	WorldBatteryFireRules::RecordMissionFired(TurnState, BatteryNodeA, TurnThree);
	TestEqual(TEXT("A fired"), WorldBatteryFireRules::GetMissionsFiredThisTurn(TurnState, BatteryNodeA, TurnThree), 1);
	TestEqual(TEXT("B did not fire"),
	          WorldBatteryFireRules::GetMissionsFiredThisTurn(TurnState, BatteryNodeB, TurnThree), 0);
	WorldBatteryFireRules::RecordMissionFired(TurnState, BatteryNodeA, TurnThree);
	TestEqual(TEXT("A fired twice"),
	          WorldBatteryFireRules::GetMissionsFiredThisTurn(TurnState, BatteryNodeA, TurnThree), 2);
	TestEqual(TEXT("One record per node"), TurnState.Records.Num(), 1);

	// A save written before the turn start work ran still holds last turn's records.
	TestEqual(TEXT("Records of an older turn do not count"),
	          WorldBatteryFireRules::GetMissionsFiredThisTurn(TurnState, BatteryNodeA, TurnFour), 0);
	WorldBatteryFireRules::RecordMissionFired(TurnState, BatteryNodeB, TurnFour);
	TestEqual(TEXT("Recording in a new turn drops the old records"), TurnState.Records.Num(), 1);
	TestEqual(TEXT("B fired in the new turn"),
	          WorldBatteryFireRules::GetMissionsFiredThisTurn(TurnState, BatteryNodeB, TurnFour), 1);

	WorldBatteryFireRules::ResetForNewTurn(TurnState, TurnFour + 1);
	TestEqual(TEXT("A new turn clears the records"), TurnState.Records.Num(), 0);
	TestEqual(TEXT("A new turn stamps the state"), TurnState.Turn, TurnFour + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBatteryFireCasualtiesTest, "RTS.WorldCampaign.BatteryFire.Casualties",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBatteryFireCasualtiesTest::RunTest(const FString& Parameters)
{
	const FWorldBatteryFireCasualties FromFour = WorldBatteryFireRules::GetCasualties(4, 2);
	TestEqual(TEXT("Two of four die"), FromFour.DestroyedCount, 2);
	TestEqual(TEXT("They are the last two slots"), FromFour.FirstDestroyedSlot, 2);
	TestFalse(TEXT("Two are left"), FromFour.bIsDivisionEmptied);

	const FWorldBatteryFireCasualties FromTwo = WorldBatteryFireRules::GetCasualties(2, 2);
	TestEqual(TEXT("Both die"), FromTwo.DestroyedCount, 2);
	TestTrue(TEXT("The division is emptied"), FromTwo.bIsDivisionEmptied);

	const FWorldBatteryFireCasualties FromOne = WorldBatteryFireRules::GetCasualties(1, 2);
	TestEqual(TEXT("No more than the roster holds"), FromOne.DestroyedCount, 1);
	TestEqual(TEXT("The only slot"), FromOne.FirstDestroyedSlot, 0);
	TestTrue(TEXT("Emptied"), FromOne.bIsDivisionEmptied);

	const FWorldBatteryFireCasualties NoRoster = WorldBatteryFireRules::GetCasualties(INDEX_NONE, 2);
	TestEqual(TEXT("A piece without a roster has no casualties"), NoRoster.DestroyedCount, 0);
	TestFalse(TEXT("And is not emptied"), NoRoster.bIsDivisionEmptied);

	// Five shells, two casualties: the second and the fifth shell.
	TestEqual(TEXT("First casualty"), WorldBatteryFireRules::GetShellIndexForCasualty(0, 2, 5), 1);
	TestEqual(TEXT("Last casualty dies with the last shell"), WorldBatteryFireRules::GetShellIndexForCasualty(1, 2, 5), 4);
	TestEqual(TEXT("One casualty dies with the last shell"), WorldBatteryFireRules::GetShellIndexForCasualty(0, 1, 5), 4);
	TestEqual(TEXT("More casualties than shells share shells"), WorldBatteryFireRules::GetShellIndexForCasualty(0, 4, 2), 0);
	TestEqual(TEXT("The last of many still gets the last shell"),
	          WorldBatteryFireRules::GetShellIndexForCasualty(3, 4, 2), 1);
	TestEqual(TEXT("No shells"), WorldBatteryFireRules::GetShellIndexForCasualty(0, 2, 0), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Index past the casualties"), WorldBatteryFireRules::GetShellIndexForCasualty(2, 2, 5),
	          static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBatteryFireArcTest, "RTS.WorldCampaign.BatteryFire.ArcAndTiming",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBatteryFireArcTest::RunTest(const FString& Parameters)
{
	using namespace WorldBatteryFireTestData;
	const FVector Start(0.f, 0.f, 20.f);
	const FVector End(1000.f, 0.f, 0.f);
	constexpr float ArcHeightFactor = 0.25f;
	TestTrue(TEXT("The arc starts at the muzzle"),
	         WorldBatteryFireRules::GetShellLocation(Start, End, 0.f, ArcHeightFactor).Equals(Start, Tolerance));
	TestTrue(TEXT("The arc ends on the target"),
	         WorldBatteryFireRules::GetShellLocation(Start, End, 1.f, ArcHeightFactor).Equals(End, Tolerance));
	const FVector Top = WorldBatteryFireRules::GetShellLocation(Start, End, 0.5f, ArcHeightFactor);
	TestEqual(TEXT("Half way along the map"), static_cast<float>(Top.X), 500.f, Tolerance);
	// Half way between the end heights (10) plus a quarter of the 1000 unit distance.
	TestEqual(TEXT("Peak height"), static_cast<float>(Top.Z), 260.f, Tolerance);
	TestTrue(TEXT("Alpha is clamped"),
	         WorldBatteryFireRules::GetShellLocation(Start, End, 2.f, ArcHeightFactor).Equals(End, Tolerance));

	TestEqual(TEXT("First shell leaves after the lead-in"), WorldBatteryFireRules::GetShellLaunchSeconds(0, 0.4f, 0.35f),
	          0.4f, Tolerance);
	TestEqual(TEXT("Shells are staggered"), WorldBatteryFireRules::GetShellLaunchSeconds(4, 0.4f, 0.35f), 1.8f,
	          Tolerance);
	TestEqual(TEXT("The default show takes about four seconds"),
	          WorldBatteryFireRules::GetShowDurationSeconds(5, 0.4f, 0.35f, 1.6f, 0.6f), 4.f, Tolerance);

	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	TestNotNull(TEXT("Settings exist"), Settings);
	if (Settings == nullptr)
	{
		return false;
	}
	TestTrue(TEXT("Every Soviet division tank has a piece mesh path"), Settings->TankPieceMeshes.Num() >= 8);
	TestEqual(TEXT("Wreck paths match the piece paths"), Settings->TankWreckMeshes.Num(), Settings->TankPieceMeshes.Num());
	TestTrue(TEXT("An unset asset loads as nothing"),
	         UWorldBatteryFireSettings::LoadOptionalAsset(FSoftObjectPath(), TEXT("TestUnset")) == nullptr);
	TestTrue(TEXT("A missing asset loads as nothing"),
	         UWorldBatteryFireSettings::LoadOptionalAsset(
		         FSoftObjectPath(TEXT("/Game/RTS_Survival/WorldCampaign/NoSuchFolder/SM_NoSuchMesh.SM_NoSuchMesh")),
		         TEXT("TestMissing")) == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldDivisionFormationLayoutTest, "RTS.WorldCampaign.BatteryFire.FormationLayout",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldDivisionFormationLayoutTest::RunTest(const FString& Parameters)
{
	using namespace WorldBatteryFireTestData;
	constexpr int32 MaxPerRow = 3;
	constexpr float ColumnSpacing = 18.f;
	constexpr float RowSpacing = 28.f;
	TestEqual(TEXT("No slots"), WorldDivisionFormationLayout::GetSlotOffsets(0, MaxPerRow, ColumnSpacing, RowSpacing).Num(),
	          0);

	const TArray<FVector2D> One = WorldDivisionFormationLayout::GetSlotOffsets(1, MaxPerRow, ColumnSpacing, RowSpacing);
	TestTrue(TEXT("One tank stands on the division"), One.Num() == 1 && One[0].Equals(FVector2D::ZeroVector, Tolerance));

	const TArray<FVector2D> Four = WorldDivisionFormationLayout::GetSlotOffsets(4, MaxPerRow, ColumnSpacing, RowSpacing);
	TestEqual(TEXT("Four slots"), Four.Num(), 4);
	if (Four.Num() != 4)
	{
		return false;
	}
	// Two balanced rows of two: front row ahead of the centre, back row behind it.
	TestTrue(TEXT("Slot 0 front left"), Four[0].Equals(FVector2D(14.f, -9.f), Tolerance));
	TestTrue(TEXT("Slot 1 front right"), Four[1].Equals(FVector2D(14.f, 9.f), Tolerance));
	TestTrue(TEXT("Slot 2 back left"), Four[2].Equals(FVector2D(-14.f, -9.f), Tolerance));
	TestTrue(TEXT("Slot 3 back right"), Four[3].Equals(FVector2D(-14.f, 9.f), Tolerance));

	const TArray<FVector2D> Five = WorldDivisionFormationLayout::GetSlotOffsets(5, MaxPerRow, ColumnSpacing, RowSpacing);
	TestEqual(TEXT("Five slots"), Five.Num(), 5);
	if (Five.Num() != 5)
	{
		return false;
	}
	TestTrue(TEXT("Three in the front row"), Five[2].Equals(FVector2D(14.f, 18.f), Tolerance));
	TestTrue(TEXT("The short back row is centred"), Five[3].Equals(FVector2D(-14.f, -9.f), Tolerance));

	const FVector2D FootprintHalfSize = WorldDivisionFormationLayout::GetFootprintHalfSize(Five);
	TestTrue(TEXT("Footprint covers every slot"), FootprintHalfSize.Equals(FVector2D(14.f, 18.f), Tolerance));
	// Six tanks must stay inside a build site plate (58 wide) plus the tanks' own size.
	const FVector2D SixHalfSize = WorldDivisionFormationLayout::GetFootprintHalfSize(
		WorldDivisionFormationLayout::GetSlotOffsets(6, MaxPerRow, ColumnSpacing, RowSpacing));
	TestTrue(TEXT("Six tanks fit the division's footprint"), SixHalfSize.X <= 30.f && SixHalfSize.Y <= 30.f);
	TestEqual(TEXT("A max per row below one counts as one"),
	          WorldDivisionFormationLayout::GetSlotOffsets(3, 0, ColumnSpacing, RowSpacing).Num(), 3);
	return true;
}

#endif
