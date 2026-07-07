// Copyright (C) Bas Blokzijl - All rights reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/**
 * Separate shipping target that exercises the campaign map backtracking generator across many seeds.
 *
 * Mirrors RTS_SurvivalShippingMapTestsTarget: it is an isolated Game target that only differs from the
 * regular shipping build by a single GlobalDefinition. All test-only code is compiled behind
 * RTS_WITH_CAMPAIGN_MAP_TESTS, so building any other target (RTS_Survival, RTS_SurvivalEditor,
 * RTS_SurvivalShippingMapTests) produces byte-identical game code - the macro is undefined there and the
 * harness translation units collapse to empty.
 */
public class RTS_SurvivalShippingCampaignMapTestsTarget : TargetRules
{
	public RTS_SurvivalShippingCampaignMapTestsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "RTS_Survival" });

		// Enables the campaign map determinism test harness. Undefined in every other target.
		GlobalDefinitions.Add("RTS_WITH_CAMPAIGN_MAP_TESTS=1");

		if (Configuration == UnrealTargetConfiguration.Shipping)
		{
			BuildEnvironment = TargetBuildEnvironment.Unique;
			bUseChecksInShipping = false;
			bUseLoggingInShipping = true;
		}
	}
}
