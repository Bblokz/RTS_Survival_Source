// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RTS_Survival/WorldCampaign/CommanderMapKit/WorldCommanderMapKitRules.h"

namespace WorldCommanderMapKitTestData
{
	constexpr uint8 FirstCommanderValue = static_cast<uint8>(ERTSCommander::BalancedCommander);
	constexpr uint8 LastCommanderValue = static_cast<uint8>(ERTSCommander::SkyDropQuartermaster);
	constexpr int32 CooldownTurns = 3;
	constexpr int32 DurationTurns = 2;

	FWorldCommanderMapKitDefinition MakeKit(const EWorldCommanderMapAbility AbilityType)
	{
		FWorldCommanderMapKitDefinition Kit;
		Kit.Ability.Type = AbilityType;
		Kit.Ability.CooldownTurns = CooldownTurns;
		Kit.Ability.DurationTurns = DurationTurns;
		return Kit;
	}

	FWorldCommanderMapState MakeState()
	{
		FWorldCommanderMapState MapState;
		MapState.Commander = ERTSCommander::BalancedCommander;
		return MapState;
	}

	FWorldCommanderMapAbilityTarget MakeAnchorTarget()
	{
		FWorldCommanderMapAbilityTarget Target;
		Target.AnchorKey = FGuid::NewGuid();
		return Target;
	}

	FWorldMapResources MakeResources(const int32 Radixite, const int32 Blueprints)
	{
		FWorldMapResources Resources;
		Resources.Radixite = Radixite;
		Resources.Blueprints = Blueprints;
		return Resources;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCommanderDefaultTableTest, "RTS.WorldCampaign.Commander.DefaultTableIsComplete",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCommanderDefaultTableTest::RunTest(const FString& Parameters)
{
	using namespace WorldCommanderMapKitTestData;
	const FWorldCommanderMapKitTable Table;
	TestNull(TEXT("NoCommander has no kit"), Table.FindDefinition(ERTSCommander::NoCommander));
	for (uint8 Value = FirstCommanderValue; Value <= LastCommanderValue; ++Value)
	{
		const ERTSCommander Commander = static_cast<ERTSCommander>(Value);
		const FString CommanderName = UEnum::GetValueAsString(Commander);
		const FWorldCommanderMapKitDefinition* Kit = Table.FindDefinition(Commander);
		if (not TestNotNull(CommanderName + TEXT(" has a kit"), Kit))
		{
			continue;
		}
		TestTrue(CommanderName + TEXT(" has a passive"), Kit->Passive.Type != EWorldCommanderPassive::None);
		TestTrue(CommanderName + TEXT(" has a map ability"), Kit->Ability.Type != EWorldCommanderMapAbility::None);
		TestTrue(CommanderName + TEXT(" has a cooldown"), Kit->Ability.CooldownTurns > 0);
		TestFalse(CommanderName + TEXT(" has a passive name"), Kit->PassiveDisplayName.IsEmpty());
		TestFalse(CommanderName + TEXT(" has an ability name"), Kit->AbilityDisplayName.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCommanderCooldownTest, "RTS.WorldCampaign.Commander.CooldownTiming",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCommanderCooldownTest::RunTest(const FString& Parameters)
{
	using namespace WorldCommanderMapKitTestData;
	const FWorldCommanderMapKitDefinition Kit = MakeKit(EWorldCommanderMapAbility::GrantResources);
	const FWorldCommanderMapAbilityTarget NoTarget;
	const FWorldMapResources NoResources;
	FWorldCommanderMapState MapState = MakeState();

	TestEqual(TEXT("A fresh ability is ready"),
	          WorldCommanderMapKitRules::GetAbilityBlock(MapState, &Kit, NoTarget, NoResources),
	          EWorldCommanderAbilityBlock::None);

	// Used on turn N.
	WorldCommanderMapKitRules::RecordAbilityUse(MapState, Kit.Ability, NoTarget);
	TestEqual(TEXT("The use is counted"), MapState.MapAbilityUses, 1);
	TestEqual(TEXT("The cooldown starts at its full length"), MapState.MapAbilityCooldownTurnsRemaining,
	          CooldownTurns);
	TestEqual(TEXT("Not usable again in the same turn"),
	          WorldCommanderMapKitRules::GetAbilityBlock(MapState, &Kit, NoTarget, NoResources),
	          EWorldCommanderAbilityBlock::OnCooldown);

	for (int32 TurnsPassed = 1; TurnsPassed < CooldownTurns; ++TurnsPassed)
	{
		WorldCommanderMapKitRules::TickStartOfPlayerTurn(MapState);
		TestEqual(TEXT("Still cooling down before turn N + cooldown"),
		          WorldCommanderMapKitRules::GetAbilityBlock(MapState, &Kit, NoTarget, NoResources),
		          EWorldCommanderAbilityBlock::OnCooldown);
	}

	// Start of turn N + CooldownTurns.
	WorldCommanderMapKitRules::TickStartOfPlayerTurn(MapState);
	TestEqual(TEXT("Ready again on turn N + cooldown"),
	          WorldCommanderMapKitRules::GetAbilityBlock(MapState, &Kit, NoTarget, NoResources),
	          EWorldCommanderAbilityBlock::None);
	WorldCommanderMapKitRules::TickStartOfPlayerTurn(MapState);
	TestEqual(TEXT("The cooldown never goes below zero"), MapState.MapAbilityCooldownTurnsRemaining, 0);

	FWorldCommanderMapState NoCommanderState;
	TestEqual(TEXT("Without a commander nothing can be used"),
	          WorldCommanderMapKitRules::GetAbilityBlock(NoCommanderState, &Kit, NoTarget, NoResources),
	          EWorldCommanderAbilityBlock::NoCommander);
	TestEqual(TEXT("Without a kit nothing can be used"),
	          WorldCommanderMapKitRules::GetAbilityBlock(MapState, nullptr, NoTarget, NoResources),
	          EWorldCommanderAbilityBlock::NoAbility);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCommanderTimedEffectTest, "RTS.WorldCampaign.Commander.TimedEffects",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCommanderTimedEffectTest::RunTest(const FString& Parameters)
{
	using namespace WorldCommanderMapKitTestData;
	const FWorldCommanderMapKitDefinition Kit = MakeKit(EWorldCommanderMapAbility::WeakenTarget);
	const FWorldCommanderMapAbilityTarget Target = MakeAnchorTarget();
	FWorldCommanderMapState MapState = MakeState();

	WorldCommanderMapKitRules::RecordAbilityUse(MapState, Kit.Ability, Target);
	if (not TestEqual(TEXT("A lasting ability stores one timed effect"), MapState.ActiveTimedEffects.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("The effect keeps its target"), MapState.ActiveTimedEffects[0].Target.AnchorKey == Target.AnchorKey);

	TArray<FWorldCommanderTimedEffect> Expired = WorldCommanderMapKitRules::TickStartOfPlayerTurn(MapState);
	TestEqual(TEXT("A two-turn effect is still active on the next turn"), MapState.ActiveTimedEffects.Num(), 1);
	TestEqual(TEXT("Nothing expired yet"), Expired.Num(), 0);

	Expired = WorldCommanderMapKitRules::TickStartOfPlayerTurn(MapState);
	TestEqual(TEXT("The effect is gone after its two turns"), MapState.ActiveTimedEffects.Num(), 0);
	if (TestEqual(TEXT("The expired effect is reported"), Expired.Num(), 1))
	{
		TestEqual(TEXT("The expired effect is the weaken"), Expired[0].Ability, EWorldCommanderMapAbility::WeakenTarget);
	}

	FWorldCommanderMapAbilityDefinition InstantAbility = Kit.Ability;
	InstantAbility.DurationTurns = 0;
	WorldCommanderMapKitRules::RecordAbilityUse(MapState, InstantAbility, Target);
	TestEqual(TEXT("An instant ability stores no timed effect"), MapState.ActiveTimedEffects.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCommanderTargetTest, "RTS.WorldCampaign.Commander.TargetValidation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCommanderTargetTest::RunTest(const FString& Parameters)
{
	using namespace WorldCommanderMapKitTestData;
	const FWorldCommanderMapAbilityTarget EmptyTarget;
	const FWorldMapResources NoResources;
	const FWorldCommanderMapState MapState = MakeState();
	const FWorldCommanderMapKitDefinition AnchorKit = MakeKit(EWorldCommanderMapAbility::WeakenTarget);

	TestEqual(TEXT("An anchor ability needs an anchor"),
	          WorldCommanderMapKitRules::GetAbilityBlock(MapState, &AnchorKit, EmptyTarget, NoResources),
	          EWorldCommanderAbilityBlock::MissingTarget);
	TestEqual(TEXT("An anchor ability with an anchor is ready"),
	          WorldCommanderMapKitRules::GetAbilityBlock(MapState, &AnchorKit, MakeAnchorTarget(), NoResources),
	          EWorldCommanderAbilityBlock::None);

	FWorldCommanderMapAbilityTarget LocationTarget;
	LocationTarget.WorldLocation = FVector(100.f, 200.f, 0.f);
	TestFalse(TEXT("A location must be marked as aimed"),
	          WorldCommanderMapKitRules::GetIsTargetValidForAbility(EWorldCommanderMapAbility::RevealRegion,
	                                                                LocationTarget));
	LocationTarget.bHasWorldLocation = true;
	TestTrue(TEXT("An aimed location is valid"),
	         WorldCommanderMapKitRules::GetIsTargetValidForAbility(EWorldCommanderMapAbility::RevealRegion,
	                                                               LocationTarget));

	FWorldCommanderMapAbilityTarget DivisionTarget;
	TestFalse(TEXT("A division strike needs a division"),
	          WorldCommanderMapKitRules::GetIsTargetValidForAbility(EWorldCommanderMapAbility::DamageDivision,
	                                                                DivisionTarget));
	DivisionTarget.DivisionKey = FGuid::NewGuid();
	TestTrue(TEXT("A division strike with a division is valid"),
	         WorldCommanderMapKitRules::GetIsTargetValidForAbility(EWorldCommanderMapAbility::DamageDivision,
	                                                               DivisionTarget));

	FWorldCommanderMapAbilityTarget BuildNodeTarget;
	BuildNodeTarget.BuildNodeId = FName(TEXT("TestNode"));
	TestTrue(TEXT("A named build node is valid"),
	         WorldCommanderMapKitRules::GetIsTargetValidForAbility(EWorldCommanderMapAbility::ProtectBuilding,
	                                                               BuildNodeTarget));
	TestTrue(TEXT("A resource ability needs no target"),
	         WorldCommanderMapKitRules::GetIsTargetValidForAbility(EWorldCommanderMapAbility::GrantResources,
	                                                               EmptyTarget));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCommanderConversionTest, "RTS.WorldCampaign.Commander.Conversion",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCommanderConversionTest::RunTest(const FString& Parameters)
{
	using namespace WorldCommanderMapKitTestData;
	FWorldCommanderMapKitDefinition Kit = MakeKit(EWorldCommanderMapAbility::ConvertRadixiteToBlueprints);
	Kit.Ability.Magnitude = 100.f;
	Kit.Ability.Resources = MakeResources(0, 3);

	const FWorldCommanderConversion Partial = WorldCommanderMapKitRules::GetConversion(Kit.Ability, 250);
	TestEqual(TEXT("250 radixite buys two blueprints"), Partial.BlueprintsGained, 2);
	TestEqual(TEXT("Only the used radixite is spent"), Partial.RadixiteSpent, 200);

	const FWorldCommanderConversion Capped = WorldCommanderMapKitRules::GetConversion(Kit.Ability, 1000);
	TestEqual(TEXT("The per-use cap holds"), Capped.BlueprintsGained, 3);
	TestEqual(TEXT("The cap limits the spending"), Capped.RadixiteSpent, 300);

	const FWorldCommanderMapState MapState = MakeState();
	const FWorldCommanderMapAbilityTarget NoTarget;
	TestEqual(TEXT("Too little radixite blocks the conversion"),
	          WorldCommanderMapKitRules::GetAbilityBlock(MapState, &Kit, NoTarget, MakeResources(50, 0)),
	          EWorldCommanderAbilityBlock::CannotAfford);
	TestEqual(TEXT("Enough radixite allows it"),
	          WorldCommanderMapKitRules::GetAbilityBlock(MapState, &Kit, NoTarget, MakeResources(100, 0)),
	          EWorldCommanderAbilityBlock::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCommanderPassiveTest, "RTS.WorldCampaign.Commander.PassiveValues",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCommanderPassiveTest::RunTest(const FString& Parameters)
{
	using namespace WorldCommanderMapKitTestData;
	FWorldCommanderPassiveDefinition AirfieldRange;
	AirfieldRange.Type = EWorldCommanderPassive::BuildingRangeMultiplier;
	AirfieldRange.Magnitude = 1.5f;
	AirfieldRange.Building = EWorldCampaignBuilding::Airfield;

	TestEqual(TEXT("The passive scales its own building"),
	          WorldCommanderMapKitRules::GetPassiveBuildingValue(
		          AirfieldRange, EWorldCommanderPassive::BuildingRangeMultiplier, EWorldCampaignBuilding::Airfield,
		          1.f), 1.5f);
	TestEqual(TEXT("Another building stays neutral"),
	          WorldCommanderMapKitRules::GetPassiveBuildingValue(
		          AirfieldRange, EWorldCommanderPassive::BuildingRangeMultiplier, EWorldCampaignBuilding::WatchPost,
		          1.f), 1.f);
	TestEqual(TEXT("Another passive type stays neutral"),
	          WorldCommanderMapKitRules::GetPassiveBuildingValue(
		          AirfieldRange, EWorldCommanderPassive::BuildingCostMultiplier, EWorldCampaignBuilding::Airfield,
		          1.f), 1.f);

	const FWorldMapResources Base = MakeResources(100, 2);
	const FWorldMapResources RadixiteOnly = WorldCommanderMapKitRules::ScaleResources(
		Base, 1.5f, EWorldMapResourceScope::RadixiteOnly);
	TestEqual(TEXT("Radixite is scaled"), RadixiteOnly.Radixite, 150);
	TestEqual(TEXT("Blueprints are untouched"), RadixiteOnly.Blueprints, 2);

	FWorldCommanderPassiveDefinition SideMissionRewards;
	SideMissionRewards.Type = EWorldCommanderPassive::OperationRewardMultiplier;
	SideMissionRewards.Magnitude = 2.f;
	SideMissionRewards.OperationTarget = EWorldOperationTarget::Mission;
	SideMissionRewards.bExcludeStoryMissions = true;
	TestEqual(TEXT("A side mission pays more"),
	          WorldCommanderMapKitRules::GetScaledOperationReward(SideMissionRewards, Base,
	                                                              EWorldOperationTarget::Mission, false).Radixite, 200);
	TestEqual(TEXT("A story mission is excluded"),
	          WorldCommanderMapKitRules::GetScaledOperationReward(SideMissionRewards, Base,
	                                                              EWorldOperationTarget::Mission, true).Radixite, 100);
	TestEqual(TEXT("An enemy item is not a mission"),
	          WorldCommanderMapKitRules::GetScaledOperationReward(SideMissionRewards, Base,
	                                                              EWorldOperationTarget::EnemyItem, false).Radixite,
	          100);
	return true;
}

#endif
