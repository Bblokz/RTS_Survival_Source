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

		if (Configuration == UnrealTargetConfiguration.Shipping)
		{
			BuildEnvironment = TargetBuildEnvironment.Unique;
			bUseChecksInShipping = false;
			bUseLoggingInShipping = true;
		}
	}
}
