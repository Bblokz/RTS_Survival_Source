// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class RTS_Survival : ModuleRules
{
	public RTS_Survival(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"AIModule",
			"NavigationSystem",
			"ChaosVehicles",
			"GeometryCollectionEngine",
			"GameplayTasks",
			"Navmesh",
			"MoviePlayer",
			"Chaos",
			"Landscape",
			"FieldSystemEngine",
			"PhysicsCore",
			"PCG",
			"DeveloperSettings",
			"GameplayTags",
			"MovieScene",
			"UMG",
			// Needed for UGameMapsSettings (EngineSettings/GameMapsSettings.h) used to read the project's default map when exiting to main menu.
			"EngineSettings",
			//// Saviour runtime headers expose JSON types and use JsonObjectConverter.
			//"Json",
			//"JsonUtilities"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Niagara",
			"AsyncTickPhysics",
			"Slate",
			"SlateCore",
			"ProtoAnimatedText",
			"Json",
			"RenderCore",
			"RHI",
			"MovieSceneCapture",
			"AudioMixer",
			"EnhancedInput",
			// Shorts creator (Source/RTS_Survival/Shorts): drives Movie Render Queue from a -game process and
			// reads its Job.json with the JSON object converter.
			"MovieRenderPipelineCore",
			"MovieRenderPipelineRenderPasses",
			// UI renderer pass: floating combat texts are screen-space widgets.
			"MovieRenderPipelineSettings",
			"LevelSequence",
			"MovieSceneTracks",
			"CinematicCamera",
			"JsonUtilities",
			//// Saviour-added:
			//"GameFeatures",
			//"GameplayAbilities",
			//"CustomizableObject"
		});

		if (Target.bBuildEditor)
		{
			// Landscape height editing updates any foliage attached to the modified terrain.
			PrivateDependencyModuleNames.Add("Foliage");
			// NiagaraEditor: the authoring step of the trace impact Niagara system edits emitter graphs.
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "BlueprintGraph", "NiagaraEditor" });
			// Sequencer: NiagaraEditorUtilities.h (impact Niagara system authoring) includes Sequencer headers.
			PrivateDependencyModuleNames.Add("Sequencer");
			// PlanarCut and Voronoi: RTS.Aircraft.BuildCrashCollections fractures the aircraft into their crash pieces.
			PrivateDependencyModuleNames.AddRange(new string[] { "PlanarCut", "Voronoi" });
		}

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
