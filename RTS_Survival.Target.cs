// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class RTS_SurvivalTarget : TargetRules
{
	public RTS_SurvivalTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
		
		//Use these config options in shipping to enable logs, and to enable debugger.
		if (Configuration == UnrealTargetConfiguration.Shipping)
		{
			BuildEnvironment = TargetBuildEnvironment.Unique;
			// this causes crashes in shipping when set to true https://issues.unrealengine.com/issue/UE-171275
			// (was true by default)
			bUseChecksInShipping = false;
			bUseLoggingInShipping = true;
		}

		ExtraModuleNames.AddRange( new string[] { "RTS_Survival" } );
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
	}
}
