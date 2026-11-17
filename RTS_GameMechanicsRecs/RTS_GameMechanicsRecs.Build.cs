// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

using UnrealBuildTool;

/// <summary>
/// Game mechanics recorder: films scripted gameplay on real maps with Movie Render Queue from a headless
/// UnrealEditor-Cmd -game process. Only the editor target builds this module (RTS_SurvivalEditor.Target.cs
/// ExtraModuleNames) and nothing loads it automatically: the render launcher passes
/// -ExecCmds="Module Load RTS_GameMechanicsRecs", so packaged and shipping builds never contain it.
/// </summary>
public class RTS_GameMechanicsRecs : ModuleRules
{
	public RTS_GameMechanicsRecs(ReadOnlyTargetRules Target) : base(Target)
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
			"Json",
			"JsonUtilities",
			"AudioMixer",
			"AudioMixerCore",
			"MovieScene",
			"MovieSceneTracks",
			"LevelSequence",
			"MovieRenderPipelineCore",
			"MovieRenderPipelineRenderPasses",
			"MovieRenderPipelineSettings",
			"CinematicCamera",
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
