// Copyright (C) Bas Blokzijl - All rights reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/**
 * Shipping test target for the global abilities: the player gets every global ability that has a template in
 * DA_GlobalAbilitySystem, on every map, and no requirement can block one (commander kit, mission tier cap, mission
 * panel hiding, technology, required unit, costs, cooldown). The ability test runner
 * (GlobalAbilitySystem/Tests/AllGlobalAbilities, -RTSGlobalAbilityTest) drives them through the real panel and click
 * path and stress-tests the mission maps.
 *
 * Everything this target changes sits behind RTS_WITH_ALL_GLOBAL_ABILITIES, which no other target defines, so the
 * regular RTS_Survival Shipping build keeps every gate. See Source/Docs/Testing/RTS_SurvivalShippingGlobalAbilityTests.md.
 */
public class RTS_SurvivalShippingGlobalAbilityTestsTarget : TargetRules
{
	public RTS_SurvivalShippingGlobalAbilityTestsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "RTS_Survival" });

		// Unlocks every global ability for the player and compiles the ability test runner. Undefined in every other target.
		GlobalDefinitions.Add("RTS_WITH_ALL_GLOBAL_ABILITIES=1");
		// The test hooks other runners use (RTSMapTests.h); the plain RTS_Survival Shipping build compiles them out.
		GlobalDefinitions.Add("RTS_WITH_MAP_TESTS=1");
		// A Shipping build ignores the map URL on the command line (GameInstance.cpp); the tests start mission maps directly.
		GlobalDefinitions.Add("UE_ALLOW_MAP_OVERRIDE_IN_SHIPPING=1");
		// The added definitions change the global environment, so every configuration needs its own build environment.
		BuildEnvironment = TargetBuildEnvironment.Unique;

		if (Configuration == UnrealTargetConfiguration.Shipping)
		{
			// No bUseChecksInShipping: UE 5.5 asserts in RHIInit with checks on in Shipping (the immediate command
			// list has no graphics context there), so this target runs the same check-free code as the real game.
			bUseLoggingInShipping = true;
			// The console lets a tester page through the abilities (RTS.GlobalAbilities.Page) and toggle stress cvars.
			bUseConsoleInShipping = true;
		}
	}
}
