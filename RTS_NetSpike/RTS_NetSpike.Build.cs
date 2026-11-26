// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

using UnrealBuildTool;

/// <summary>
/// LAN multiplayer spike: one listen-server host running the normal game, one joining client that orders a
/// replicated tank to move. Only the editor target builds this module (RTS_SurvivalEditor.Target.cs
/// ExtraModuleNames) and nothing loads it automatically: Source/Tools/NetSpike/run_net_spike.ps1 passes
/// -ExecCmds="Module Load RTS_NetSpike", so the game, the editor and packaged builds never run any of it.
/// The module changes no class of RTS_Survival; it only calls its exported functions.
/// </summary>
public class RTS_NetSpike : ModuleRules
{
	public RTS_NetSpike(ReadOnlyTargetRules Target) : base(Target)
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
			// Replication macros and the listen port of the net driver.
			"NetCore",
			"Sockets",
			"AIModule",
			"NavigationSystem",
			"UMG",
			"Slate",
			"SlateCore",
			"GameplayTags",
			"DeveloperSettings",
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
