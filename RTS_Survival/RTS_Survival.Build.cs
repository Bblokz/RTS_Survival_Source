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
			//// Saviour-added:
			//"GameFeatures",
			//"GameplayAbilities",
			//"CustomizableObject"
		});

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
