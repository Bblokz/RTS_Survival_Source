// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CommanderPanel/WorldCommanderPanelRules.h"

namespace WorldCommanderPanelTestData
{
	constexpr int32 ThreeTurns = 3;
	const FName BuildNodeId(TEXT("Germany_02"));
	const FVector ClickLocation(1200.0, -340.0, 15.0);

	FWorldCommanderPickCandidate MakeCandidate(const EWorldCommanderPickedThing Thing)
	{
		FWorldCommanderPickCandidate Candidate;
		Candidate.Thing = Thing;
		Candidate.AnchorKey = FGuid::NewGuid();
		Candidate.DivisionKey = FGuid::NewGuid();
		Candidate.BuildNodeId = BuildNodeId;
		Candidate.WorldLocation = ClickLocation;
		Candidate.bHasWorldLocation = Thing != EWorldCommanderPickedThing::Nothing;
		return Candidate;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCommanderPanelButtonStateTest, "RTS.WorldCampaign.UI.Commander.ButtonState",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCommanderPanelButtonStateTest::RunTest(const FString& Parameters)
{
	using namespace WorldCommanderPanelRules;
	TestEqual(TEXT("No ability is unavailable, even while cooling down"),
	          GetAbilityButtonState(false, WorldCommanderPanelTestData::ThreeTurns, false),
	          EWorldCommanderAbilityButtonState::Unavailable);
	TestEqual(TEXT("No cooldown is ready"), GetAbilityButtonState(true, 0, false),
	          EWorldCommanderAbilityButtonState::Ready);
	TestEqual(TEXT("A cooldown wins over picking"),
	          GetAbilityButtonState(true, WorldCommanderPanelTestData::ThreeTurns, true),
	          EWorldCommanderAbilityButtonState::CoolingDown);
	TestEqual(TEXT("Ready and picking is picking"), GetAbilityButtonState(true, 0, true),
	          EWorldCommanderAbilityButtonState::PickingTarget);

	TestEqual(TEXT("Cooling down shows the turns left"),
	          GetAbilityButtonLabel(EWorldCommanderAbilityButtonState::CoolingDown,
	                                WorldCommanderPanelTestData::ThreeTurns).ToString(), FString(TEXT("3")));
	TestTrue(TEXT("Ready shows the glyph, no number"),
	         GetAbilityButtonLabel(EWorldCommanderAbilityButtonState::Ready, 0).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCommanderPanelStateLineTest, "RTS.WorldCampaign.UI.Commander.StateLine",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCommanderPanelStateLineTest::RunTest(const FString& Parameters)
{
	using namespace WorldCommanderPanelRules;
	using EState = EWorldCommanderAbilityButtonState;
	const int32 ThreeTurns = WorldCommanderPanelTestData::ThreeTurns;
	TestEqual(TEXT("Ready with a cooldown names it"), GetAbilityStateLine(EState::Ready, 0, ThreeTurns).ToString(),
	          FString(TEXT("READY · cooldown 3 turns")));
	TestEqual(TEXT("A cooldown of one turn is singular"), GetAbilityStateLine(EState::Ready, 0, 1).ToString(),
	          FString(TEXT("READY · cooldown 1 turn")));
	TestEqual(TEXT("Ready without a cooldown"), GetAbilityStateLine(EState::Ready, 0, 0).ToString(),
	          FString(TEXT("READY")));
	TestEqual(TEXT("Picking reads like ready"), GetAbilityStateLine(EState::PickingTarget, 0, ThreeTurns).ToString(),
	          GetAbilityStateLine(EState::Ready, 0, ThreeTurns).ToString());
	TestEqual(TEXT("Cooling down counts the turns"),
	          GetAbilityStateLine(EState::CoolingDown, 2, ThreeTurns).ToString(), FString(TEXT("READY IN 2 TURNS")));
	TestEqual(TEXT("One turn left is singular"), GetAbilityStateLine(EState::CoolingDown, 1, ThreeTurns).ToString(),
	          FString(TEXT("READY IN 1 TURN")));
	TestTrue(TEXT("No ability, no state line"), GetAbilityStateLine(EState::Unavailable, 0, ThreeTurns).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCommanderPanelPromptTest, "RTS.WorldCampaign.UI.Commander.TargetPrompt",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCommanderPanelPromptTest::RunTest(const FString& Parameters)
{
	using namespace WorldCommanderPanelRules;
	TestTrue(TEXT("No target, no prompt"), GetTargetPromptText(EWorldCommanderAbilityTargetKind::None).IsEmpty());
	TestEqual(TEXT("Anchor prompt"), GetTargetPromptText(EWorldCommanderAbilityTargetKind::Anchor).ToString(),
	          FString(TEXT("Pick a target: an enemy target")));
	TestEqual(TEXT("Division prompt"), GetTargetPromptText(EWorldCommanderAbilityTargetKind::Division).ToString(),
	          FString(TEXT("Pick a target: an enemy division")));
	TestEqual(TEXT("Build node prompt"), GetTargetPromptText(EWorldCommanderAbilityTargetKind::BuildNode).ToString(),
	          FString(TEXT("Pick a target: a build site")));
	TestEqual(TEXT("Location prompt"),
	          GetTargetPromptText(EWorldCommanderAbilityTargetKind::WorldLocation).ToString(),
	          FString(TEXT("Pick a target: a place on the map")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCommanderPanelPickTest, "RTS.WorldCampaign.UI.Commander.PickTarget",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCommanderPanelPickTest::RunTest(const FString& Parameters)
{
	using namespace WorldCommanderPanelRules;
	using namespace WorldCommanderPanelTestData;
	using EKind = EWorldCommanderAbilityTargetKind;
	using EThing = EWorldCommanderPickedThing;
	FWorldCommanderMapAbilityTarget Target;
	FText FailReason;

	const FWorldCommanderPickCandidate Operation = MakeCandidate(EThing::OperationTarget);
	TestTrue(TEXT("A mission or enemy item is an anchor target"),
	         BuildTargetFromPick(EKind::Anchor, Operation, Target, FailReason));
	TestEqual(TEXT("Anchor key is copied"), Target.AnchorKey, Operation.AnchorKey);
	TestFalse(TEXT("Only the anchor field is set"), Target.DivisionKey.IsValid() || Target.bHasWorldLocation);

	TestFalse(TEXT("A player or cleared object is no anchor target"),
	          BuildTargetFromPick(EKind::Anchor, MakeCandidate(EThing::OtherMapObject), Target, FailReason));
	TestEqual(TEXT("The reason names what to pick"), FailReason.ToString(),
	          FString(TEXT("That is not an enemy target.")));
	TestFalse(TEXT("A division is no anchor target"),
	          BuildTargetFromPick(EKind::Anchor, MakeCandidate(EThing::Division), Target, FailReason));

	const FWorldCommanderPickCandidate Division = MakeCandidate(EThing::Division);
	TestTrue(TEXT("A division is a division target"),
	         BuildTargetFromPick(EKind::Division, Division, Target, FailReason));
	TestEqual(TEXT("Division key is copied"), Target.DivisionKey, Division.DivisionKey);
	TestFalse(TEXT("The ground is no division"),
	          BuildTargetFromPick(EKind::Division, MakeCandidate(EThing::Ground), Target, FailReason));

	TestTrue(TEXT("A build node is a build node target"),
	         BuildTargetFromPick(EKind::BuildNode, MakeCandidate(EThing::BuildNode), Target, FailReason));
	TestEqual(TEXT("Build node id is copied"), Target.BuildNodeId, BuildNodeId);
	TestFalse(TEXT("A mission is no build node"),
	          BuildTargetFromPick(EKind::BuildNode, Operation, Target, FailReason));

	TestTrue(TEXT("Any hit is a place on the map"),
	         BuildTargetFromPick(EKind::WorldLocation, MakeCandidate(EThing::OtherMapObject), Target, FailReason));
	TestTrue(TEXT("A location target says it was aimed"), Target.bHasWorldLocation);
	TestEqual(TEXT("Location is copied"), Target.WorldLocation, ClickLocation);
	TestFalse(TEXT("A click on nothing is no place"),
	          BuildTargetFromPick(EKind::WorldLocation, MakeCandidate(EThing::Nothing), Target, FailReason));

	TestTrue(TEXT("An ability without a target takes any click"),
	         BuildTargetFromPick(EKind::None, MakeCandidate(EThing::Nothing), Target, FailReason));
	return true;
}

#endif
