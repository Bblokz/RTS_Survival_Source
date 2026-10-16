// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class RTS_SurvivalShippingMapTestsTarget : TargetRules
{
	public RTS_SurvivalShippingMapTestsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "RTS_Survival" });

		GlobalDefinitions.Add("RTS_WITH_SHIPPING_MAP_TESTS=1");
		// The added definition changes the global environment, so every configuration (also a DebugGame
		// package for debugging the tests) needs its own build environment.
		BuildEnvironment = TargetBuildEnvironment.Unique;

		if (Configuration == UnrealTargetConfiguration.Shipping)
		{
			bUseChecksInShipping = false;
			bUseLoggingInShipping = true;
		}
	}
}
