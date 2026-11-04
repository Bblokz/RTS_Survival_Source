// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only authoring of the add-on armour break test assets (test content, never shipped).
// RTS.AddOnArmorTest.BuildCollection: GC_TestSkirtSide, one skirt side (5 panels x 8 shards) as a flat collection,
//   every leaf anchored, so the root stays kinematic on the hull until a leaf is unanchored and broken off.
// RTS.AddOnArmorTest.BuildNiagara: NS_TestSkirtDebris, a CPU mesh-particle burst whose pieces start exactly where
//   the broken shards were (arrays filled per hit) and whose meshes are User.DebrisMesh0..7, so any armour kind can
//   pass its own pieces. Its motion is custom HLSL from DebrisHlsl/*.hlsl: gravity, drag, spin, a bounce on the
//   ground plane, settling flat, and a shrink-out matched to the geometry collection's removal.
// Run both headless: UnrealEditor-Cmd <uproject> /Engine/Maps/Entry
//   -ExecCmds="RTS.AddOnArmorTest.BuildCollection, RTS.AddOnArmorTest.BuildNiagara, QUIT_EDITOR" -unattended -nullrhi

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "AddOnArmorBreakTestTypes.h"
#include "AddOnArmorBreakAuthoringShared.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_Niagara.h"
#include "Engine/StaticMesh.h"
#include "GeometryCollection/Facades/CollectionAnchoringFacade.h"
#include "GeometryCollection/Facades/CollectionRemoveOnBreakFacade.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NiagaraDataInterfaceArrayFloat.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAddOnArmorAuthoring, Log, All);

namespace AddOnArmorBreakAuthoring
{
	using namespace AddOnArmorBreakTest;

	// Leaves break only from the test's release strain, never from contact.
	constexpr float LeafDamageThreshold = 1.0e6f;
	// Pieces are separate convex plates that touch at their cut edges.
	constexpr float ConnectionBoundsMargin = 1.f;
	constexpr float CollisionSampleFraction = 0.1f;

	const TCHAR* const TemplateEmitterPath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst.UpwardMeshBurst");
	const TCHAR* const DebrisEmitterName = TEXT("SkirtDebris");
	const TCHAR* const SpawnNodeName = TEXT("SkirtDebrisSpawn");
	const TCHAR* const UpdateNodeName = TEXT("SkirtDebrisUpdate");
	const TCHAR* const InputsNodeComment = TEXT("SkirtDebrisInputs");
	const TCHAR* const HlslFolder = TEXT("Source/RTS_Survival/Utils/RTSMapTests/AddOnArmorBreak/DebrisHlsl");
	// Modules of the template that stay on; the custom HLSL replaces everything else.
	const TArray<FString> KeptModules = {TEXT("EmitterState"), TEXT("SpawnBurst_Instantaneous"),
	                                     TEXT("InitializeParticle"), TEXT("ParticleState")};
	const TCHAR* const OnceLoopEnumValue = TEXT("NewEnumerator1");
	constexpr uint32 MaxPoolSize = 256;
	constexpr uint32 PoolPrimeSize = 64;

	const FName MeshIndexAttribute = TEXT("Particles.MeshIndex");
	const FName SpinAxisAttribute = TEXT("Particles.SpinAxis");
	const FName SpinSpeedAttribute = TEXT("Particles.SpinSpeed");
	const FName RestingAttribute = TEXT("Particles.Resting");

	using namespace AddOnArmorBreakAuthoringShared;

	// ------------------------------------------------------------------------------------------------------------
	// Geometry collection
	// ------------------------------------------------------------------------------------------------------------

	bool AppendSidePieces(UGeometryCollection& CollectionAsset, const TArray<UStaticMesh*>& Meshes,
	                      const TArray<FVector>& Centroids)
	{
		constexpr bool bReindexMaterials = false;
		constexpr bool bAddInternalMaterials = false;
		constexpr bool bSplitComponents = false;
		for (int32 PanelIndex = 0; PanelIndex < PanelsPerSide; ++PanelIndex)
		{
			for (int32 ShardIndex = 0; ShardIndex < ShardCount; ++ShardIndex)
			{
				const UStaticMesh* const Mesh = Meshes[ShardIndex];
				TArray<UMaterialInterface*> Materials;
				for (const FStaticMaterial& StaticMaterial : Mesh->GetStaticMaterials())
				{
					Materials.Add(StaticMaterial.MaterialInterface);
				}
				const FTransform PieceTransform(GetPanelTopCentre(PanelIndex) + Centroids[ShardIndex]);
				if (not FGeometryCollectionEngineConversion::AppendStaticMesh(
					Mesh, Materials, PieceTransform, &CollectionAsset, bReindexMaterials, bAddInternalMaterials, bSplitComponents))
				{
					return false;
				}
			}
		}
		return true;
	}

	/** @return The leaf transform indices once every piece hangs directly under a single root. */
	TArray<int32> FlattenUnderRoot(FGeometryCollection& Collection)
	{
		if (FGeometryCollectionClusteringUtility::ContainsMultipleRootBones(&Collection))
		{
			FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(&Collection);
		}
		TArray<int32> RootBones;
		FGeometryCollectionClusteringUtility::GetRootBones(&Collection, RootBones);
		if (RootBones.Num() == 1)
		{
			FGeometryCollectionClusteringUtility::UpdateHierarchyLevelOfChildren(&Collection, RootBones[0]);
		}
		TArray<int32> Leaves;
		for (int32 TransformIndex = 0; TransformIndex < Collection.NumElements(FGeometryCollection::TransformGroup); ++TransformIndex)
		{
			if (Collection.Children[TransformIndex].IsEmpty())
			{
				Leaves.Add(TransformIndex);
			}
		}
		return Leaves;
	}

	void AnchorAndScheduleRemoval(FGeometryCollection& Collection, const TArray<int32>& Leaves)
	{
		Chaos::Facades::FCollectionAnchoringFacade Anchoring(Collection);
		Anchoring.AddAnchoredAttribute();
		Anchoring.SetAnchored(Leaves, true);
		GeometryCollection::Facades::FCollectionRemoveOnBreakFacade RemoveOnBreak(Collection);
		RemoveOnBreak.DefineSchema();
		constexpr bool bEnableRemoval = true;
		constexpr bool bClusterCrumbling = false;
		const GeometryCollection::Facades::FRemoveOnBreakData RemovalData(
			bEnableRemoval, FVector2f(DebrisSecondsBeforeRemoval, DebrisSecondsBeforeRemoval), bClusterCrumbling,
			FVector2f(DebrisRemovalSeconds, DebrisRemovalSeconds));
		RemoveOnBreak.SetFromIndexArray(Leaves, RemovalData);
	}

	void ApplyCollectionSettings(UGeometryCollection& CollectionAsset)
	{
		CollectionAsset.EnableNanite = false;
		CollectionAsset.EnableClustering = true;
		CollectionAsset.DamageModel = EDamageModelTypeEnum::Chaos_Damage_Model_UserDefined_Damage_Threshold;
		CollectionAsset.bUseSizeSpecificDamageThreshold = false;
		CollectionAsset.DamageThreshold = {LeafDamageThreshold, LeafDamageThreshold};
		// On by default with factor 1: one released piece would pass its strain to all its neighbours.
		CollectionAsset.DamagePropagationData.bEnabled = false;
		CollectionAsset.ClusterConnectionType = EClusterConnectionTypeEnum::Chaos_BoundsOverlapFilteredDelaunayTriangulation;
		CollectionAsset.ConnectionGraphBoundsFilteringMargin = ConnectionBoundsMargin;
		// Removal is per piece, a fixed time after its break (RemoveOnBreak), the same as the Niagara debris.
		CollectionAsset.bRemoveOnMaxSleep = false;
		CollectionAsset.bScaleOnRemoval = true;
		if (CollectionAsset.SizeSpecificData.IsEmpty())
		{
			CollectionAsset.SizeSpecificData.AddDefaulted();
		}
		for (FGeometryCollectionSizeSpecificData& SizeData : CollectionAsset.SizeSpecificData)
		{
			for (FGeometryCollectionCollisionTypeData& CollisionShape : SizeData.CollisionShapes)
			{
				CollisionShape.CollisionParticles.CollisionParticlesFraction = CollisionSampleFraction;
				// Thin plates: a box keeps the full 0.8 cm thickness, a convex hull loses part of it to the margin,
				// and the margin lets released pieces tunnel into the landscape.
				CollisionShape.ImplicitType = EImplicitTypeEnum::Chaos_Implicit_Box;
				CollisionShape.CollisionMarginFraction = 0.f;
				CollisionShape.CollisionObjectReductionPercentage = 0.f;
			}
		}
	}

	void BuildCollection()
	{
		TArray<FVector> Centroids;
		TArray<UStaticMesh*> Meshes;
		UPackage* const Package = LoadShardCentroids(Centroids) && LoadShardMeshes(Meshes)
			                          ? PrepareFreshPackage(CollectionPackage)
			                          : nullptr;
		if (Package == nullptr)
		{
			UE_LOG(LogRTSAddOnArmorAuthoring, Error, TEXT("RTS_ADDON_ARMOR_AUTHORING collection RESULT FAIL inputs"));
			return;
		}
		UGeometryCollection* const CollectionAsset = NewObject<UGeometryCollection>(
			Package, *FPackageName::GetShortName(CollectionPackage), RF_Public | RF_Standalone | RF_Transactional);
		if (not AppendSidePieces(*CollectionAsset, Meshes, Centroids))
		{
			UE_LOG(LogRTSAddOnArmorAuthoring, Error, TEXT("RTS_ADDON_ARMOR_AUTHORING collection RESULT FAIL append"));
			return;
		}
		FGeometryCollection& Collection = *CollectionAsset->GetGeometryCollection();
		CollectionAsset->InitializeMaterials(false);
		GeometryCollectionAlgo::PrepareForSimulation(&Collection);
		const TArray<int32> Leaves = FlattenUnderRoot(Collection);
		AnchorAndScheduleRemoval(Collection, Leaves);
		ApplyCollectionSettings(*CollectionAsset);
		CollectionAsset->UpdateGeometryDependentProperties();
		CollectionAsset->InvalidateCollection();
		CollectionAsset->RebuildRenderData();
		FAssetRegistryModule::AssetCreated(CollectionAsset);
		const bool bSaved = SaveAsset(*CollectionAsset);
		UE_LOG(LogRTSAddOnArmorAuthoring, Display,
		       TEXT("RTS_ADDON_ARMOR_AUTHORING collection RESULT DONE transforms=%d leaves=%d geometry=%d saved=%d"),
		       Collection.NumElements(FGeometryCollection::TransformGroup), Leaves.Num(),
		       Collection.NumElements(FGeometryCollection::GeometryGroup), bSaved ? 1 : 0);
	}

	// ------------------------------------------------------------------------------------------------------------
	// Niagara system
	// ------------------------------------------------------------------------------------------------------------

	FNiagaraTypeDefinition GetFloat4ArrayType()
	{
		return FNiagaraTypeDefinition(UNiagaraDataInterfaceArrayFloat4::StaticClass());
	}

	FNiagaraTypeDefinition GetStaticMeshType()
	{
		return FNiagaraTypeDefinition(UStaticMesh::StaticClass());
	}

	FString MakeUserName(const FName ParameterName)
	{
		return TEXT("User.") + ParameterName.ToString();
	}

	void AddUserParameters(UNiagaraSystem& System, const TArray<UStaticMesh*>& Meshes)
	{
		FNiagaraUserRedirectionParameterStore& UserParameters = System.GetExposedParameters();
		for (const FName ArrayName : {PieceStartParameter, PieceVelocityParameter, PieceSpinParameter, PieceOrientationParameter})
		{
			UserParameters.AddParameter(FNiagaraVariable(GetFloat4ArrayType(), *MakeUserName(ArrayName)), true);
		}
		UserParameters.SetParameterValue<int32>(0, FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), *MakeUserName(PieceCountParameter)), true);
		UserParameters.SetParameterValue<float>(0.f, FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), *MakeUserName(GroundHeightParameter)), true);
		UserParameters.SetParameterValue<float>(DebrisSecondsBeforeRemoval + DebrisRemovalSeconds,
		                                        FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.DebrisLifetime")), true);
		UserParameters.SetParameterValue<float>(DebrisRemovalSeconds,
		                                        FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.DebrisShrinkSeconds")), true);
		for (int32 ShardIndex = 0; ShardIndex < Meshes.Num(); ++ShardIndex)
		{
			const FNiagaraVariable MeshVariable(GetStaticMeshType(), *MakeUserName(GetDebrisMeshParameter(ShardIndex)));
			UserParameters.AddParameter(MeshVariable, true);
			UserParameters.SetUObject(Meshes[ShardIndex], MeshVariable);
		}
	}

	FNiagaraEmitterHandle* AddTemplateEmitter(UNiagaraSystem& System)
	{
		UNiagaraEmitter* const Template = LoadObject<UNiagaraEmitter>(nullptr, TemplateEmitterPath);
		if (not IsValid(Template))
		{
			return nullptr;
		}
		UNiagaraEmitter* const Staging = DuplicateObject<UNiagaraEmitter>(Template, GetTransientPackage(), DebrisEmitterName);
		const FGuid HandleId = FNiagaraEditorUtilities::AddEmitterToSystem(System, *Staging, Template->GetExposedVersion().VersionGuid);
		for (FNiagaraEmitterHandle& Handle : System.GetEmitterHandles())
		{
			if (Handle.GetId() == HandleId)
			{
				// The staging copy is transient; as a parent it would make the save fail.
				Handle.GetEmitterData()->RemoveParent();
				return &Handle;
			}
		}
		return nullptr;
	}

	bool GetIsParameterMapPin(const UEdGraphPin& Pin)
	{
		return Pin.PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct();
	}

	UEdGraphPin* FindParameterMapPin(UEdGraphNode& Node, const EEdGraphPinDirection Direction)
	{
		for (UEdGraphPin* const Pin : Node.Pins)
		{
			if (Pin->Direction == Direction && GetIsParameterMapPin(*Pin))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	void SetPinDefaultWhenPresent(UEdGraphNode& Node, const TCHAR* PinName, const TCHAR* DefaultValue)
	{
		if (UEdGraphPin* const Pin = Node.FindPin(PinName, EGPD_Input))
		{
			Pin->DefaultValue = DefaultValue;
		}
	}

	/** @brief Keeps the emitter state, the burst, Initialize Particle and Particle State; the HLSL does the rest. */
	void ConfigureTemplateModules(UNiagaraGraph& Graph, UNiagaraSystem& System)
	{
		for (UEdGraphNode* const Node : TArray<UEdGraphNode*>(Graph.Nodes))
		{
			UNiagaraNodeFunctionCall* const FunctionNode = Cast<UNiagaraNodeFunctionCall>(Node);
			// Stack modules pass the parameter map on; dynamic inputs (curves, randoms) feed a module's input instead.
			const bool bIsStackModule = FunctionNode != nullptr && FindParameterMapPin(*Node, EGPD_Input) != nullptr
				&& FindParameterMapPin(*Node, EGPD_Output) != nullptr;
			if (not bIsStackModule || Cast<UNiagaraNodeAssignment>(Node) != nullptr)
			{
				continue;
			}
			const FString FunctionName = FunctionNode->GetFunctionName();
			const bool bIsKept = KeptModules.ContainsByPredicate([&FunctionName](const FString& Kept)
			{
				return FunctionName.StartsWith(Kept);
			});
			UE_LOG(LogRTSAddOnArmorAuthoring, Display, TEXT("RTS_ADDON_ARMOR_AUTHORING module %s kept=%d"), *FunctionName, bIsKept ? 1 : 0);
			if (not bIsKept)
			{
				FNiagaraStackGraphUtilities::SetModuleIsEnabled(*FunctionNode, false);
				continue;
			}
			if (FunctionName.StartsWith(TEXT("EmitterState")))
			{
				// One burst per activation; the pooled component completes when the last piece is gone.
				SetPinDefaultWhenPresent(*Node, TEXT("Loop Behavior"), OnceLoopEnumValue);
				SetPinDefaultWhenPresent(*Node, TEXT("Enable Distance Culling"), TEXT("false"));
				SetPinDefaultWhenPresent(*Node, TEXT("Enable Visibility Culling"), TEXT("false"));
			}
			if (FunctionName.StartsWith(TEXT("SpawnBurst_Instantaneous")))
			{
				const FNiagaraParameterHandle CountHandle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(
					FNiagaraParameterHandle::CreateModuleParameterHandle(TEXT("Spawn Count")), FunctionNode);
				UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
					*FunctionNode, CountHandle, FNiagaraTypeDefinition::GetIntDef(), FGuid(), FGuid());
				const FNiagaraVariable CountVariable(FNiagaraTypeDefinition::GetIntDef(), *MakeUserName(PieceCountParameter));
				FNiagaraStackGraphUtilities::SetLinkedValueHandleForFunctionInput(
					OverridePin, FNiagaraParameterHandle(CountVariable.GetName()), TSet<FNiagaraVariable>{CountVariable});
				UE_LOG(LogRTSAddOnArmorAuthoring, Display, TEXT("RTS_ADDON_ARMOR_AUTHORING burst count bound to %s"),
				       *CountVariable.GetName().ToString());
			}
		}
	}

	/** @return A custom HLSL node with a parameter map in and out, run after every module of the script. */
	UNiagaraNodeCustomHlsl* InsertHlslNodeBeforeOutput(UEdGraph& Graph, UNiagaraNodeOutput& OutputNode, const TCHAR* NodeName)
	{
		UEdGraphPin* const OutputMapPin = FindParameterMapPin(OutputNode, EGPD_Input);
		if (OutputMapPin == nullptr || OutputMapPin->LinkedTo.IsEmpty())
		{
			return nullptr;
		}
		UEdGraphPin* const PreviousMapPin = OutputMapPin->LinkedTo[0];
		UNiagaraNodeCustomHlsl* const HlslNode = NewObject<UNiagaraNodeCustomHlsl>(&Graph, NAME_None, RF_Transactional);
		const FNiagaraVariable MapVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("Map"));
		HlslNode->Signature.Inputs.Add(MapVariable);
		HlslNode->Signature.Outputs.Add(MapVariable);
		Graph.AddNode(HlslNode, false, false);
		HlslNode->CreateNewGuid();
		HlslNode->PostPlacedNewNode();
		HlslNode->AllocateDefaultPins();
		HlslNode->OnRenameNode(NodeName);
		UEdGraphPin* const HlslInputMapPin = FindParameterMapPin(*HlslNode, EGPD_Input);
		UEdGraphPin* const HlslOutputMapPin = FindParameterMapPin(*HlslNode, EGPD_Output);
		if (HlslInputMapPin == nullptr || HlslOutputMapPin == nullptr)
		{
			Graph.RemoveNode(HlslNode);
			return nullptr;
		}
		OutputMapPin->BreakLinkTo(PreviousMapPin);
		PreviousMapPin->MakeLinkTo(HlslInputMapPin);
		HlslOutputMapPin->MakeLinkTo(OutputMapPin);
		return HlslNode;
	}

	/**
	 * @brief Adds an output to a parameter map get node the way the Niagara editor does, with the hidden default
	 * pin the node keeps for every output.
	 * @param InputsNode The parameter map get node.
	 * @param ParameterName Full name of the parameter to read, for example User.PieceCount.
	 * @param Type Type of the parameter.
	 * @return The output pin that carries the parameter.
	 */
	UEdGraphPin* AddParameterOutput(UEdGraphNode& InputsNode, const FName ParameterName, const FNiagaraTypeDefinition& Type)
	{
		FEdGraphPinType PinType = UEdGraphSchema_Niagara::TypeDefinitionToPinType(Type);
		PinType.PinSubCategory = TEXT("ParameterPin");
		UEdGraphPin* const OutputPin = InputsNode.CreatePin(EGPD_Output, PinType, ParameterName);
		OutputPin->PersistentGuid = FGuid::NewGuid();
		UEdGraphPin* const DefaultPin = InputsNode.CreatePin(EGPD_Input, PinType, NAME_None);
		DefaultPin->PersistentGuid = FGuid::NewGuid();
		DefaultPin->bNotConnectable = true;
		DefaultPin->bDefaultValueIsIgnored = true;
		const FMapProperty* const DefaultPinMapProperty = FindFProperty<FMapProperty>(
			InputsNode.GetClass(), TEXT("PinOutputToPinDefaultPersistentId"));
		if (DefaultPinMapProperty != nullptr)
		{
			TMap<FGuid, FGuid>* const DefaultPinMap = DefaultPinMapProperty->ContainerPtrToValuePtr<TMap<FGuid, FGuid>>(&InputsNode);
			DefaultPinMap->Add(OutputPin->PersistentGuid, DefaultPin->PersistentGuid);
		}
		return OutputPin;
	}

	struct FHlslInput
	{
		const TCHAR* PinName;
		FString ParameterName;
		FNiagaraTypeDefinition Type;
	};

	/**
	 * @brief Feeds the HLSL node its inputs through a parameter map get node and writes its code.
	 * @param Graph The emitter graph.
	 * @param HlslNode The node to wire.
	 * @param Inputs Pins in the order the HLSL uses them, each with the parameter that feeds it.
	 * @param Hlsl The code.
	 * @return False when the graph lacks the expected pins.
	 */
	bool WireHlslNode(UEdGraph& Graph, UNiagaraNodeCustomHlsl& HlslNode, const TArray<FHlslInput>& Inputs, const FString& Hlsl)
	{
		UClass* const ParameterMapGetClass = FindObject<UClass>(nullptr, TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"));
		UEdGraphPin* const InputMapPin = FindParameterMapPin(HlslNode, EGPD_Input);
		if (ParameterMapGetClass == nullptr || InputMapPin == nullptr || InputMapPin->LinkedTo.IsEmpty())
		{
			return false;
		}
		UEdGraphNode* const InputsNode = NewObject<UEdGraphNode>(&Graph, ParameterMapGetClass, NAME_None, RF_Transactional);
		Graph.AddNode(InputsNode, false, false);
		InputsNode->CreateNewGuid();
		InputsNode->PostPlacedNewNode();
		InputsNode->AllocateDefaultPins();
		InputsNode->NodeComment = FString(InputsNodeComment) + HlslNode.GetFunctionName();
		UEdGraphPin* const SourcePin = InputsNode->FindPin(TEXT("Source"), EGPD_Input);
		if (SourcePin == nullptr)
		{
			return false;
		}
		SourcePin->MakeLinkTo(InputMapPin->LinkedTo[0]);
		for (const FHlslInput& Input : Inputs)
		{
			UEdGraphPin* const InputPin = HlslNode.CreatePin(EGPD_Input, UEdGraphSchema_Niagara::TypeDefinitionToPinType(Input.Type), Input.PinName);
			UEdGraphPin* const ParameterPin = AddParameterOutput(*InputsNode, FName(*Input.ParameterName), Input.Type);
			ParameterPin->MakeLinkTo(InputPin);
			HlslNode.Signature.Inputs.Add(FNiagaraVariable(Input.Type, Input.PinName));
		}
		FStrProperty* const HlslProperty = FindFProperty<FStrProperty>(HlslNode.GetClass(), TEXT("CustomHlsl"));
		if (HlslProperty == nullptr)
		{
			return false;
		}
		HlslProperty->SetPropertyValue_InContainer(&HlslNode, Hlsl);
		FPropertyChangedEvent HlslChangedEvent(HlslProperty);
		HlslNode.PostEditChangeProperty(HlslChangedEvent);
		return true;
	}

	bool ReadHlsl(const TCHAR* FileName, FString& OutHlsl)
	{
		const FString FilePath = FPaths::Combine(FPaths::ProjectDir(), HlslFolder, FileName);
		if (FFileHelper::LoadFileToString(OutHlsl, *FilePath))
		{
			return true;
		}
		UE_LOG(LogRTSAddOnArmorAuthoring, Error, TEXT("RTS_ADDON_ARMOR_AUTHORING cannot read %s"), *FilePath);
		return false;
	}

	void DeclareDebrisAttributes(UNiagaraNodeOutput& SpawnOutputNode)
	{
		const TArray<FNiagaraVariable> Attributes = {
			FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), MeshIndexAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), SpinAxisAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), SpinSpeedAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), RestingAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(), TEXT("Particles.MeshOrientation"))
		};
		const TArray<FString> DefaultValues = {TEXT("0"), TEXT("0,0,1"), TEXT("0"), TEXT("0"), TEXT("0,0,0,1")};
		FNiagaraStackGraphUtilities::AddParameterModuleToStack(Attributes, SpawnOutputNode, INDEX_NONE, DefaultValues);
	}

	bool WireDebrisHlsl(UNiagaraGraph& Graph)
	{
		UNiagaraNodeOutput* const SpawnOutput = Graph.FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleSpawnScript);
		UNiagaraNodeOutput* const UpdateOutput = Graph.FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleUpdateScript);
		FString SpawnHlsl;
		FString UpdateHlsl;
		if (SpawnOutput == nullptr || UpdateOutput == nullptr || not ReadHlsl(TEXT("Spawn.hlsl"), SpawnHlsl)
			|| not ReadHlsl(TEXT("Update.hlsl"), UpdateHlsl))
		{
			return false;
		}
		DeclareDebrisAttributes(*SpawnOutput);
		UNiagaraNodeCustomHlsl* const SpawnNode = InsertHlslNodeBeforeOutput(Graph, *SpawnOutput, SpawnNodeName);
		UNiagaraNodeCustomHlsl* const UpdateNode = InsertHlslNodeBeforeOutput(Graph, *UpdateOutput, UpdateNodeName);
		if (SpawnNode == nullptr || UpdateNode == nullptr)
		{
			return false;
		}
		const FNiagaraTypeDefinition FloatType = FNiagaraTypeDefinition::GetFloatDef();
		const TArray<FHlslInput> SpawnInputs = {
			{TEXT("PieceStart"), MakeUserName(PieceStartParameter), GetFloat4ArrayType()},
			{TEXT("PieceVelocity"), MakeUserName(PieceVelocityParameter), GetFloat4ArrayType()},
			{TEXT("PieceSpin"), MakeUserName(PieceSpinParameter), GetFloat4ArrayType()},
			{TEXT("PieceOrientation"), MakeUserName(PieceOrientationParameter), GetFloat4ArrayType()},
			{TEXT("PieceCount"), MakeUserName(PieceCountParameter), FNiagaraTypeDefinition::GetIntDef()},
			{TEXT("DebrisLifetime"), TEXT("User.DebrisLifetime"), FloatType}
		};
		// Attributes the update only reads come in through the map read; a token in the HLSL alone may not load them.
		const TArray<FHlslInput> UpdateInputs = {
			{TEXT("GroundHeight"), MakeUserName(GroundHeightParameter), FloatType},
			{TEXT("ShrinkSeconds"), TEXT("User.DebrisShrinkSeconds"), FloatType},
			{TEXT("SpinAxisIn"), SpinAxisAttribute.ToString(), FNiagaraTypeDefinition::GetVec3Def()},
			{TEXT("AgeIn"), TEXT("Particles.Age"), FloatType},
			{TEXT("LifetimeIn"), TEXT("Particles.Lifetime"), FloatType}
		};
		return WireHlslNode(Graph, *SpawnNode, SpawnInputs, SpawnHlsl) && WireHlslNode(Graph, *UpdateNode, UpdateInputs, UpdateHlsl);
	}

	bool ConfigureMeshRenderer(FVersionedNiagaraEmitterData& EmitterData, const TArray<UStaticMesh*>& Meshes)
	{
		for (UNiagaraRendererProperties* const Renderer : EmitterData.GetRenderers())
		{
			UNiagaraMeshRendererProperties* const MeshRenderer = Cast<UNiagaraMeshRendererProperties>(Renderer);
			if (MeshRenderer == nullptr)
			{
				continue;
			}
			MeshRenderer->Modify();
			MeshRenderer->Meshes.SetNum(Meshes.Num());
			for (int32 ShardIndex = 0; ShardIndex < Meshes.Num(); ++ShardIndex)
			{
				FNiagaraMeshRendererMeshProperties& Slot = MeshRenderer->Meshes[ShardIndex];
				Slot.Mesh = Meshes[ShardIndex];
				const FNiagaraVariableBase MeshParameter(GetStaticMeshType(),
				                                         *MakeUserName(GetDebrisMeshParameter(ShardIndex)));
				Slot.MeshParameterBinding.ResolvedParameter = MeshParameter;
				Slot.MeshParameterBinding.AliasedParameter = MeshParameter;
			}
			MeshRenderer->bOverrideMaterials = false;
			FProperty* const MeshesProperty = FindFProperty<FProperty>(UNiagaraMeshRendererProperties::StaticClass(), TEXT("Meshes"));
			FPropertyChangedEvent MeshesChanged(MeshesProperty);
			MeshRenderer->PostEditChangeProperty(MeshesChanged);
			return true;
		}
		return false;
	}

	void SetSystemPoolSizes(UNiagaraSystem& System)
	{
		for (const TPair<const TCHAR*, uint32>& PoolSetting : {TPair<const TCHAR*, uint32>(TEXT("MaxPoolSize"), MaxPoolSize),
		                                                      TPair<const TCHAR*, uint32>(TEXT("PoolPrimeSize"), PoolPrimeSize)})
		{
			FNumericProperty* const Property = CastField<FNumericProperty>(UNiagaraSystem::StaticClass()->FindPropertyByName(PoolSetting.Key));
			if (Property != nullptr)
			{
				Property->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(&System), static_cast<uint64>(PoolSetting.Value));
			}
		}
	}

	int32 LogCompileResults(const UNiagaraSystem& System)
	{
		int32 FailedScripts = 0;
		for (const FNiagaraEmitterHandle& EmitterHandle : System.GetEmitterHandles())
		{
			const FVersionedNiagaraEmitterData* const EmitterData = EmitterHandle.GetEmitterData();
			if (EmitterData == nullptr)
			{
				continue;
			}
			TArray<UNiagaraScript*> Scripts;
			EmitterData->GetScripts(Scripts, true);
			for (const UNiagaraScript* const Script : Scripts)
			{
				const FNiagaraVMExecutableData& ExecutableData = Script->GetVMExecutableData();
				const bool bFailed = ExecutableData.LastCompileStatus == ENiagaraScriptCompileStatus::NCS_Error;
				FailedScripts += bFailed ? 1 : 0;
				UE_LOG(LogRTSAddOnArmorAuthoring, Display, TEXT("RTS_ADDON_ARMOR_AUTHORING compiled script=%s status=%d errors=%s"),
				       *Script->GetName(), static_cast<int32>(ExecutableData.LastCompileStatus), *ExecutableData.ErrorMsg);
			}
		}
		return FailedScripts;
	}

	void BuildNiagara()
	{
		TArray<UStaticMesh*> Meshes;
		UPackage* const Package = LoadShardMeshes(Meshes) ? PrepareFreshPackage(NiagaraSystemPackage) : nullptr;
		if (Package == nullptr)
		{
			UE_LOG(LogRTSAddOnArmorAuthoring, Error, TEXT("RTS_ADDON_ARMOR_AUTHORING niagara RESULT FAIL inputs"));
			return;
		}
		UNiagaraSystem* const System = NewObject<UNiagaraSystem>(
			Package, *FPackageName::GetShortName(NiagaraSystemPackage), RF_Public | RF_Standalone | RF_Transactional);
		UNiagaraSystemFactoryNew::InitializeSystem(System, true);
		AddUserParameters(*System, Meshes);
		FNiagaraEmitterHandle* const Handle = AddTemplateEmitter(*System);
		FVersionedNiagaraEmitterData* const EmitterData = Handle != nullptr ? Handle->GetEmitterData() : nullptr;
		UNiagaraScriptSource* const Source = EmitterData != nullptr ? Cast<UNiagaraScriptSource>(EmitterData->GraphSource) : nullptr;
		UNiagaraGraph* const Graph = Source != nullptr ? Source->NodeGraph : nullptr;
		if (Graph == nullptr)
		{
			UE_LOG(LogRTSAddOnArmorAuthoring, Error, TEXT("RTS_ADDON_ARMOR_AUTHORING niagara RESULT FAIL template emitter"));
			return;
		}
		Graph->Modify();
		EmitterData->SimTarget = ENiagaraSimTarget::CPUSim;
		EmitterData->bLocalSpace = false;
		EmitterData->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Dynamic;
		for (const FName& Attribute : {MeshIndexAttribute, SpinAxisAttribute, SpinSpeedAttribute, RestingAttribute})
		{
			EmitterData->AttributesToPreserve.AddUnique(Attribute.ToString());
		}
		ConfigureTemplateModules(*Graph, *System);
		const bool bIsWired = WireDebrisHlsl(*Graph);
		const bool bHasRenderer = ConfigureMeshRenderer(*EmitterData, Meshes);
		Graph->NotifyGraphChanged();
		SetSystemPoolSizes(*System);
		System->RequestCompile(true);
		System->WaitForCompilationComplete();
		const int32 FailedScripts = LogCompileResults(*System);
		FAssetRegistryModule::AssetCreated(System);
		const bool bSaved = SaveAsset(*System);
		UE_LOG(LogRTSAddOnArmorAuthoring, Display,
		       TEXT("RTS_ADDON_ARMOR_AUTHORING niagara RESULT %s wired=%d renderer=%d failed_scripts=%d saved=%d"),
		       bIsWired && bHasRenderer && FailedScripts == 0 && bSaved ? TEXT("DONE") : TEXT("FAIL"),
		       bIsWired ? 1 : 0, bHasRenderer ? 1 : 0, FailedScripts, bSaved ? 1 : 0);
	}

	FAutoConsoleCommand BuildCollectionCommand(
		TEXT("RTS.AddOnArmorTest.BuildCollection"),
		TEXT("Builds the add-on armour break test's skirt geometry collection (test content) and saves it."),
		FConsoleCommandDelegate::CreateStatic(&BuildCollection));

	FAutoConsoleCommand BuildNiagaraCommand(
		TEXT("RTS.AddOnArmorTest.BuildNiagara"),
		TEXT("Builds the add-on armour break test's Niagara debris system (test content) and saves it."),
		FConsoleCommandDelegate::CreateStatic(&BuildNiagara));
}

#endif // WITH_EDITOR
