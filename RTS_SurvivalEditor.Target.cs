// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class RTS_SurvivalEditorTarget : TargetRules
{
	public RTS_SurvivalEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;

		ExtraModuleNames.AddRange( new string[] { "RTS_Survival" } );
		// Player economy simulation (Source/RTS_EconomySim): editor builds only and never loaded by itself;
		// Source/Tools/EconomySim/run_economy_sim.ps1 loads it with -ExecCmds="Module Load RTS_EconomySim".
		ExtraModuleNames.Add("RTS_EconomySim");
		// Game mechanics recorder (Source/RTS_GameMechanicsRecs): editor builds only and never loaded by itself;
		// the headless render process loads it with -ExecCmds="Module Load RTS_GameMechanicsRecs".
		ExtraModuleNames.Add("RTS_GameMechanicsRecs");
	}
}
