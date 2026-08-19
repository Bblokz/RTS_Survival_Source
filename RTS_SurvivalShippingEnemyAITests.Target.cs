// Copyright (C) Bas Blokzijl - All rights reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/**
 * Separate shipping target for validating the Enemy AI system on UT_EnemyAI.
 *
 * Test-only code is compiled behind RTS_WITH_ENEMY_AI_SHIPPING_TESTS. The definition is absent from
 * every regular game, editor, and shipping target, so the harness and its observability hooks are
 * removed by the preprocessor outside this target.
 */
public class RTS_SurvivalShippingEnemyAITestsTarget : TargetRules
{
	public RTS_SurvivalShippingEnemyAITestsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "RTS_Survival" });

		GlobalDefinitions.Add("RTS_WITH_ENEMY_AI_SHIPPING_TESTS=1");
		BuildEnvironment = TargetBuildEnvironment.Unique;

		if (Configuration == UnrealTargetConfiguration.Shipping)
		{
			bUseChecksInShipping = false;
			bUseLoggingInShipping = true;
		}
	}
}
