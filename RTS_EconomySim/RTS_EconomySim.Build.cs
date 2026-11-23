// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

using UnrealBuildTool;

/// <summary>
/// Player economy simulation (test code only): lays out a player base with resource fields on UT_EconomySim, runs a
/// scripted harvester / refinery / converter build order through the real training and payment paths against the
/// default enemy AI, and writes income and enemy production time series to Saved/EconomySim.
/// Only the editor target builds this module (RTS_SurvivalEditor.Target.cs ExtraModuleNames) and nothing loads it
/// automatically: the runner passes -ExecCmds="Module Load RTS_EconomySim", so game and shipping builds never
/// contain it.
/// </summary>
public class RTS_EconomySim : ModuleRules
{
	public RTS_EconomySim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"RTS_Survival"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AIModule",
			"NavigationSystem",
			"UMG",
			"Slate",
			"SlateCore",
			"GameplayTags",
			"DeveloperSettings",
			"Json",
			"JsonUtilities",
			// Game headers include these (private dependencies of RTS_Survival, so they do not propagate).
			"Niagara",
			"AsyncTickPhysics",
			"ApplicationCore",
			"ProtoAnimatedText",
			"RenderCore",
			"RHI",
			"EnhancedInput",
			"ChaosVehicles",
			"GeometryCollectionEngine",
			"PhysicsCore",
			"Chaos",
			"PCG",
			"Landscape",
			"FieldSystemEngine"
		});
	}
}
