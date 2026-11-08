// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only authoring of the add-on armour panel debris (see AGENTS.md in this folder).
// RTS.AddOnArmor.BuildDebrisSystem: NS_AddOnArmorPanelDebris, a pooled CPU mesh-particle burst, one particle per
//   shot-off panel, starting exactly where the panel hung. Its mesh is the user parameter User.DebrisMesh, which the
//   panels component sets per burst to the hull's debris panel. Motion is custom HLSL from DebrisHlsl/*.hlsl
//   (gravity, drag, spin, bounce on a ground plane, settling flat, shrink-out), as in the break test it comes from.
// RTS.AddOnArmor.SetPanelDebrisMesh: stores a modelled debris panel on a panel hull's layout.
// RTS.AddOnArmor.ExtractPanelDebris: cuts one panel out of a built panel hull as the debris panel, for hulls whose
//   panels were modelled before the debris rule.
// Run headless: UnrealEditor-Cmd <uproject> -ExecCmds="RTS.AddOnArmor.BuildDebrisSystem, QUIT_EDITOR" -unattended -nullrhi

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "AddOnArmorPanelAuthoringShared.h"
#include "AddOnArmorPanelDebris.h"
#include "AddOnArmorPanelLayout.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_Niagara.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
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
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAddOnArmorDebrisAuthoring, Log, All);

namespace AddOnArmorPanelDebrisAuthoring
{
	using namespace AddOnArmorPanelDebris;
	using namespace AddOnArmorPanelMask;
	using AddOnArmorPanelAuthoring::PrepareFreshPackage;
	using AddOnArmorPanelAuthoring::SaveAsset;

	const TCHAR* const TemplateEmitterPath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst.UpwardMeshBurst");
	const TCHAR* const DebrisEmitterName = TEXT("PanelDebris");
	const TCHAR* const SpawnNodeName = TEXT("PanelDebrisSpawn");
	const TCHAR* const UpdateNodeName = TEXT("PanelDebrisUpdate");
	const TCHAR* const InputsNodeComment = TEXT("PanelDebrisInputs");
	const TCHAR* const HlslFolder = TEXT("Source/RTS_Survival/RTSComponents/ArmorCalculationComponent/AddOnArmorPanels/DebrisHlsl");
	// Shown in the renderer until a burst sets User.DebrisMesh.
	const TCHAR* const PlaceholderMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	// Modules of the template that stay on; the custom HLSL replaces everything else.
	const TArray<FString> KeptModules = {TEXT("EmitterState"), TEXT("SpawnBurst_Instantaneous"),
	                                     TEXT("InitializeParticle"), TEXT("ParticleState")};
	const TCHAR* const OnceLoopEnumValue = TEXT("NewEnumerator1");
	constexpr uint32 MaxPoolSize = 64;
	constexpr uint32 PoolPrimeSize = 8;
	const TCHAR* const DebrisMeshSuffix = TEXT("_Debris");

	const FName PieceScaleAttribute = TEXT("Particles.PieceScale");
	const FName SpinAxisAttribute = TEXT("Particles.SpinAxis");
	const FName SpinSpeedAttribute = TEXT("Particles.SpinSpeed");
	const FName RestingAttribute = TEXT("Particles.Resting");

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

	void AddUserParameters(UNiagaraSystem& System, UStaticMesh& PlaceholderMesh)
	{
		FNiagaraUserRedirectionParameterStore& UserParameters = System.GetExposedParameters();
		for (const FName ArrayName : {PieceStartParameter, PieceVelocityParameter, PieceSpinParameter, PieceOrientationParameter})
		{
			UserParameters.AddParameter(FNiagaraVariable(GetFloat4ArrayType(), *MakeUserName(ArrayName)), true);
		}
		const FNiagaraTypeDefinition FloatType = FNiagaraTypeDefinition::GetFloatDef();
		UserParameters.SetParameterValue<int32>(0, FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), *MakeUserName(PieceCountParameter)), true);
		UserParameters.SetParameterValue<float>(0.f, FNiagaraVariable(FloatType, *MakeUserName(GroundHeightParameter)), true);
		UserParameters.SetParameterValue<float>(DebrisSecondsBeforeShrink + DebrisShrinkSeconds,
		                                        FNiagaraVariable(FloatType, TEXT("User.DebrisLifetime")), true);
		UserParameters.SetParameterValue<float>(DebrisShrinkSeconds, FNiagaraVariable(FloatType, TEXT("User.DebrisShrinkSeconds")), true);
		const FNiagaraVariable MeshVariable(GetStaticMeshType(), *MakeUserName(DebrisMeshParameter));
		UserParameters.AddParameter(MeshVariable, true);
		UserParameters.SetUObject(&PlaceholderMesh, MeshVariable);
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

	UEdGraphPin* FindParameterMapPin(UEdGraphNode& Node, const EEdGraphPinDirection Direction)
	{
		for (UEdGraphPin* const Pin : Node.Pins)
		{
			if (Pin->Direction == Direction && Pin->PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct())
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

	void BindBurstCountToPieceCount(UNiagaraNodeFunctionCall& BurstNode)
	{
		const FNiagaraParameterHandle CountHandle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(
			FNiagaraParameterHandle::CreateModuleParameterHandle(TEXT("Spawn Count")), &BurstNode);
		UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
			BurstNode, CountHandle, FNiagaraTypeDefinition::GetIntDef(), FGuid(), FGuid());
		const FNiagaraVariable CountVariable(FNiagaraTypeDefinition::GetIntDef(), *MakeUserName(PieceCountParameter));
		FNiagaraStackGraphUtilities::SetLinkedValueHandleForFunctionInput(
			OverridePin, FNiagaraParameterHandle(CountVariable.GetName()), TSet<FNiagaraVariable>{CountVariable});
	}

	/** @brief Keeps the emitter state, the burst, Initialize Particle and Particle State; the HLSL does the rest. */
	void ConfigureTemplateModules(UNiagaraGraph& Graph)
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
				BindBurstCountToPieceCount(*FunctionNode);
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
		UE_LOG(LogRTSAddOnArmorDebrisAuthoring, Error, TEXT("RTS_ADDON_DEBRIS_AUTHORING cannot read %s"), *FilePath);
		return false;
	}

	void DeclareDebrisAttributes(UNiagaraNodeOutput& SpawnOutputNode)
	{
		const TArray<FNiagaraVariable> Attributes = {
			FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), PieceScaleAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), SpinAxisAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), SpinSpeedAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), RestingAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(), TEXT("Particles.MeshOrientation"))
		};
		const TArray<FString> DefaultValues = {TEXT("1"), TEXT("0,0,1"), TEXT("0"), TEXT("0"), TEXT("0,0,0,1")};
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
		// Every attribute the update reads comes in through the map read, also the ones it writes; a token in the HLSL
		// alone may not load it.
		const TArray<FHlslInput> UpdateInputs = {
			{TEXT("GroundHeight"), MakeUserName(GroundHeightParameter), FloatType},
			{TEXT("ShrinkSeconds"), TEXT("User.DebrisShrinkSeconds"), FloatType},
			{TEXT("SpinAxisIn"), SpinAxisAttribute.ToString(), FNiagaraTypeDefinition::GetVec3Def()},
			{TEXT("PieceScaleIn"), PieceScaleAttribute.ToString(), FloatType},
			{TEXT("OrientationIn"), TEXT("Particles.MeshOrientation"), FNiagaraTypeDefinition::GetQuatDef()},
			{TEXT("SpinSpeedIn"), SpinSpeedAttribute.ToString(), FloatType},
			{TEXT("RestingIn"), RestingAttribute.ToString(), FloatType},
			{TEXT("AgeIn"), TEXT("Particles.Age"), FloatType},
			{TEXT("LifetimeIn"), TEXT("Particles.Lifetime"), FloatType}
		};
		return WireHlslNode(Graph, *SpawnNode, SpawnInputs, SpawnHlsl) && WireHlslNode(Graph, *UpdateNode, UpdateInputs, UpdateHlsl);
	}

	/** @brief One mesh slot whose mesh is bound to User.DebrisMesh. */
	bool ConfigureMeshRenderer(FVersionedNiagaraEmitterData& EmitterData, UStaticMesh& PlaceholderMesh)
	{
		for (UNiagaraRendererProperties* const Renderer : EmitterData.GetRenderers())
		{
			UNiagaraMeshRendererProperties* const MeshRenderer = Cast<UNiagaraMeshRendererProperties>(Renderer);
			if (MeshRenderer == nullptr)
			{
				continue;
			}
			MeshRenderer->Modify();
			MeshRenderer->Meshes.SetNum(1);
			FNiagaraMeshRendererMeshProperties& Slot = MeshRenderer->Meshes[0];
			Slot.Mesh = &PlaceholderMesh;
			const FNiagaraVariableBase MeshParameter(GetStaticMeshType(), *MakeUserName(DebrisMeshParameter));
			Slot.MeshParameterBinding.ResolvedParameter = MeshParameter;
			Slot.MeshParameterBinding.AliasedParameter = MeshParameter;
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
				UE_LOG(LogRTSAddOnArmorDebrisAuthoring, Display, TEXT("RTS_ADDON_DEBRIS_AUTHORING compiled script=%s status=%d errors=%s"),
				       *Script->GetName(), static_cast<int32>(ExecutableData.LastCompileStatus), *ExecutableData.ErrorMsg);
			}
		}
		return FailedScripts;
	}

	void BuildDebrisSystem()
	{
		UStaticMesh* const PlaceholderMesh = LoadObject<UStaticMesh>(nullptr, PlaceholderMeshPath);
		UPackage* const Package = IsValid(PlaceholderMesh) ? PrepareFreshPackage(SystemPackage) : nullptr;
		if (Package == nullptr)
		{
			UE_LOG(LogRTSAddOnArmorDebrisAuthoring, Error, TEXT("RTS_ADDON_DEBRIS_AUTHORING system RESULT FAIL inputs"));
			return;
		}
		UNiagaraSystem* const System = NewObject<UNiagaraSystem>(
			Package, *FPackageName::GetShortName(SystemPackage), RF_Public | RF_Standalone | RF_Transactional);
		UNiagaraSystemFactoryNew::InitializeSystem(System, true);
		AddUserParameters(*System, *PlaceholderMesh);
		FNiagaraEmitterHandle* const Handle = AddTemplateEmitter(*System);
		FVersionedNiagaraEmitterData* const EmitterData = Handle != nullptr ? Handle->GetEmitterData() : nullptr;
		UNiagaraScriptSource* const Source = EmitterData != nullptr ? Cast<UNiagaraScriptSource>(EmitterData->GraphSource) : nullptr;
		UNiagaraGraph* const Graph = Source != nullptr ? Source->NodeGraph : nullptr;
		if (Graph == nullptr)
		{
			UE_LOG(LogRTSAddOnArmorDebrisAuthoring, Error, TEXT("RTS_ADDON_DEBRIS_AUTHORING system RESULT FAIL template emitter"));
			return;
		}
		Graph->Modify();
		EmitterData->SimTarget = ENiagaraSimTarget::CPUSim;
		EmitterData->bLocalSpace = false;
		EmitterData->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Dynamic;
		for (const FName& Attribute : {PieceScaleAttribute, SpinAxisAttribute, SpinSpeedAttribute, RestingAttribute})
		{
			EmitterData->AttributesToPreserve.AddUnique(Attribute.ToString());
		}
		ConfigureTemplateModules(*Graph);
		const bool bIsWired = WireDebrisHlsl(*Graph);
		const bool bHasRenderer = ConfigureMeshRenderer(*EmitterData, *PlaceholderMesh);
		Graph->NotifyGraphChanged();
		SetSystemPoolSizes(*System);
		System->RequestCompile(true);
		System->WaitForCompilationComplete();
		const int32 FailedScripts = LogCompileResults(*System);
		FAssetRegistryModule::AssetCreated(System);
		const bool bSaved = SaveAsset(*System);
		UE_LOG(LogRTSAddOnArmorDebrisAuthoring, Display,
		       TEXT("RTS_ADDON_DEBRIS_AUTHORING system RESULT %s wired=%d renderer=%d failed_scripts=%d saved=%d"),
		       bIsWired && bHasRenderer && FailedScripts == 0 && bSaved ? TEXT("DONE") : TEXT("FAIL"),
		       bIsWired ? 1 : 0, bHasRenderer ? 1 : 0, FailedScripts, bSaved ? 1 : 0);
	}

	// ------------------------------------------------------------------------------------------------------------
	// Debris panel mesh
	// ------------------------------------------------------------------------------------------------------------

	FString GetObjectPath(const FString& PackageOrObjectPath)
	{
		return PackageOrObjectPath.Contains(TEXT("."))
			? PackageOrObjectPath
			: PackageOrObjectPath + TEXT(".") + FPackageName::GetShortName(PackageOrObjectPath);
	}

	UStaticMesh* LoadMesh(const FString& PackageOrObjectPath)
	{
		const FString PackageName = FPackageName::ObjectPathToPackageName(GetObjectPath(PackageOrObjectPath));
		if (not FPackageName::DoesPackageExist(PackageName))
		{
			return nullptr;
		}
		return LoadObject<UStaticMesh>(nullptr, *GetObjectPath(PackageOrObjectPath));
	}

	/**
	 * @brief Mesh particles need their materials compiled for Niagara meshes; without that flag a cooked build draws
	 * the default material. Sets the flag on the base material of every slot and saves the changed ones.
	 * @return False when a base material could not be saved.
	 */
	bool EnsureNiagaraMeshUsage(const UStaticMesh& Mesh)
	{
		bool bAllSaved = true;
		for (const FStaticMaterial& StaticMaterial : Mesh.GetStaticMaterials())
		{
			UMaterial* const BaseMaterial = IsValid(StaticMaterial.MaterialInterface) ? StaticMaterial.MaterialInterface->GetMaterial() : nullptr;
			if (not IsValid(BaseMaterial) || BaseMaterial->GetUsageByFlag(MATUSAGE_NiagaraMeshParticles))
			{
				continue;
			}
			bool bNeedsRecompile = false;
			BaseMaterial->Modify();
			BaseMaterial->SetMaterialUsage(bNeedsRecompile, MATUSAGE_NiagaraMeshParticles);
			const bool bSaved = SaveAsset(*BaseMaterial);
			bAllSaved = bAllSaved && bSaved;
			UE_LOG(LogRTSAddOnArmorDebrisAuthoring, Display, TEXT("RTS_ADDON_DEBRIS_AUTHORING niagara mesh usage on %s saved=%d"),
			       *BaseMaterial->GetPathName(), bSaved ? 1 : 0);
		}
		return bAllSaved;
	}

	/**
	 * @brief Stores the debris panel on the hull's layout and saves the hull; the mesh itself is not rebuilt.
	 * @param Hull Panel hull with a layout.
	 * @param DebrisMesh The cut-out or modelled panel.
	 * @param OwnPanelIndex INDEX_NONE stores the shared debris panel; else that panel's own debris mesh.
	 * @return False when the hull has no layout or a material or the hull could not be saved.
	 */
	bool StoreDebrisMesh(UStaticMesh& Hull, UStaticMesh& DebrisMesh, const int32 OwnPanelIndex = INDEX_NONE)
	{
		UAddOnArmorPanelLayout* const Layout = Hull.GetAssetUserData<UAddOnArmorPanelLayout>();
		if (not IsValid(Layout) || not EnsureNiagaraMeshUsage(DebrisMesh))
		{
			return false;
		}
		Hull.Modify();
		Layout->Modify();
		if (OwnPanelIndex == INDEX_NONE)
		{
			Layout->SetDebrisMesh(&DebrisMesh);
		}
		else
		{
			Layout->SetOwnDebrisMesh(OwnPanelIndex, &DebrisMesh);
		}
		return SaveAsset(Hull);
	}

	void SetPanelDebrisMesh(const TArray<FString>& Args)
	{
		const FString Joined = FString::Join(Args, TEXT(" "));
		FString HullPath;
		FString DebrisPath;
		FParse::Value(*Joined, TEXT("Hull="), HullPath);
		FParse::Value(*Joined, TEXT("Debris="), DebrisPath);
		UStaticMesh* const Hull = LoadMesh(HullPath);
		UStaticMesh* const Debris = LoadMesh(DebrisPath);
		const bool bStored = IsValid(Hull) && IsValid(Debris) && StoreDebrisMesh(*Hull, *Debris);
		UE_LOG(LogRTSAddOnArmorDebrisAuthoring, Display, TEXT("RTS_ADDON_DEBRIS_AUTHORING set RESULT %s hull=%s debris=%s"),
		       bStored ? TEXT("DONE") : TEXT("FAIL"), *HullPath, *DebrisPath);
	}

	/** @return The left-side panel nearest the middle of the left side: a typical panel, not an end piece. */
	int32 FindTypicalLeftPanel(const UAddOnArmorPanelLayout& Layout)
	{
		TArray<int32> LeftPanels;
		const TArray<FAddOnArmorPanel>& Panels = Layout.GetPanels();
		for (int32 PanelIndex = 0; PanelIndex < Panels.Num(); ++PanelIndex)
		{
			if (Panels[PanelIndex].Side == EAddOnArmorPanelSide::Left)
			{
				LeftPanels.Add(PanelIndex);
			}
		}
		LeftPanels.Sort([&Panels](const int32 A, const int32 B)
		{
			return Panels[A].MeshSpaceCentre.X < Panels[B].MeshSpaceCentre.X;
		});
		return LeftPanels.IsEmpty() ? INDEX_NONE : LeftPanels[LeftPanels.Num() / 2];
	}

	bool GetIsPanelPolygon(const FMeshDescription& Description, const FPolygonID PolygonID, const int32 PanelIndex)
	{
		const FStaticMeshConstAttributes Attributes(Description);
		const FName SlotName = Attributes.GetPolygonGroupMaterialSlotNames()[Description.GetPolygonPolygonGroup(PolygonID)];
		if (not SlotName.ToString().StartsWith(PanelMaterialSlotName.ToString()))
		{
			return false;
		}
		const TVertexInstanceAttributesConstRef<FVector2f> Uvs = Attributes.GetVertexInstanceUVs();
		const FVertexInstanceID FirstCorner = Description.GetPolygonVertexInstances(PolygonID)[0];
		return FMath::RoundToInt32(Uvs.Get(FirstCorner, PanelIndexUvChannel).X) == PanelIndex;
	}

	/** @brief Deletes everything but the panel's polygons and moves the panel centre to the origin. */
	bool KeepOnlyPanel(FMeshDescription& Description, const int32 PanelIndex, const FVector& PanelCentre)
	{
		TArray<FPolygonID> RemovedPolygons;
		for (const FPolygonID PolygonID : Description.Polygons().GetElementIDs())
		{
			if (not GetIsPanelPolygon(Description, PolygonID, PanelIndex))
			{
				RemovedPolygons.Add(PolygonID);
			}
		}
		if (RemovedPolygons.Num() == Description.Polygons().Num())
		{
			return false;
		}
		TArray<FEdgeID> OrphanedEdges;
		TArray<FVertexInstanceID> OrphanedInstances;
		TArray<FPolygonGroupID> OrphanedGroups;
		for (const FPolygonID PolygonID : RemovedPolygons)
		{
			Description.DeletePolygon(PolygonID, &OrphanedEdges, &OrphanedInstances, &OrphanedGroups);
		}
		TArray<FVertexID> OrphanedVertices;
		for (const FVertexInstanceID Instance : OrphanedInstances)
		{
			Description.DeleteVertexInstance(Instance, &OrphanedVertices);
		}
		for (const FEdgeID Edge : OrphanedEdges)
		{
			Description.DeleteEdge(Edge, &OrphanedVertices);
		}
		for (const FVertexID Vertex : TSet<FVertexID>(OrphanedVertices))
		{
			if (Description.IsVertexOrphaned(Vertex))
			{
				Description.DeleteVertex(Vertex);
			}
		}
		for (const FPolygonGroupID Group : TSet<FPolygonGroupID>(OrphanedGroups))
		{
			Description.DeletePolygonGroup(Group);
		}
		FElementIDRemappings Remappings;
		Description.Compact(Remappings);
		TVertexAttributesRef<FVector3f> Positions = FStaticMeshAttributes(Description).GetVertexPositions();
		for (const FVertexID Vertex : Description.Vertices().GetElementIDs())
		{
			Positions[Vertex] -= FVector3f(PanelCentre);
		}
		return true;
	}

	/** @brief A new mesh from the description; slots keep the hull's panel materials, no collision, no lightmap. */
	UStaticMesh* WriteDebrisMesh(const UStaticMesh& Hull, FMeshDescription&& Description, const FString& TargetPath)
	{
		UPackage* const Package = PrepareFreshPackage(TargetPath);
		if (Package == nullptr)
		{
			return nullptr;
		}
		UStaticMesh* const Mesh = NewObject<UStaticMesh>(Package, *FPackageName::GetShortName(TargetPath),
		                                                 RF_Public | RF_Standalone | RF_Transactional);
		const FStaticMeshConstAttributes Attributes(Description);
		TArray<FStaticMaterial> Materials;
		for (const FPolygonGroupID Group : Description.PolygonGroups().GetElementIDs())
		{
			const FName SlotName = Attributes.GetPolygonGroupMaterialSlotNames()[Group];
			const int32 HullMaterialIndex = Hull.GetMaterialIndexFromImportedMaterialSlotName(SlotName);
			UMaterialInterface* const Material = Hull.GetMaterial(HullMaterialIndex);
			Materials.Add(FStaticMaterial(Material, SlotName, SlotName));
		}
		Mesh->SetStaticMaterials(Materials);
		FStaticMeshSourceModel& SourceModel = Mesh->AddSourceModel();
		SourceModel.BuildSettings.bRecomputeNormals = false;
		SourceModel.BuildSettings.bRecomputeTangents = false;
		SourceModel.BuildSettings.bGenerateLightmapUVs = false;
		int32 SectionIndex = 0;
		for (const FPolygonGroupID Group : Description.PolygonGroups().GetElementIDs())
		{
			FMeshSectionInfo Info(SectionIndex);
			Info.bEnableCollision = false;
			Info.bCastShadow = true;
			Mesh->GetSectionInfoMap().Set(0, SectionIndex++, Info);
		}
		Mesh->CreateMeshDescription(0, MoveTemp(Description));
		Mesh->CommitMeshDescription(0);
		Mesh->Build(false);
		Mesh->PostEditChange();
		FAssetRegistryModule::AssetCreated(Mesh);
		return SaveAsset(*Mesh) ? Mesh : nullptr;
	}

	void ExtractPanelDebris(const TArray<FString>& Args)
	{
		const FString Joined = FString::Join(Args, TEXT(" "));
		FString HullPath;
		FString TargetPath;
		int32 PanelIndex = INDEX_NONE;
		FParse::Value(*Joined, TEXT("Hull="), HullPath);
		FParse::Value(*Joined, TEXT("Target="), TargetPath);
		FParse::Value(*Joined, TEXT("Panel="), PanelIndex);
		// The panel's own debris mesh (any side, as it hangs) instead of the shared left panel.
		const bool bOwnPanel = FParse::Param(*Joined, TEXT("OwnPanel"));
		UStaticMesh* const Hull = LoadMesh(HullPath);
		const UAddOnArmorPanelLayout* const Layout = IsValid(Hull) ? Hull->GetAssetUserData<UAddOnArmorPanelLayout>() : nullptr;
		const FMeshDescription* const HullDescription = IsValid(Layout) ? Hull->GetMeshDescription(0) : nullptr;
		PanelIndex = PanelIndex == INDEX_NONE && IsValid(Layout) && not bOwnPanel ? FindTypicalLeftPanel(*Layout) : PanelIndex;
		if (HullDescription == nullptr || not Layout->GetPanels().IsValidIndex(PanelIndex))
		{
			UE_LOG(LogRTSAddOnArmorDebrisAuthoring, Error, TEXT("RTS_ADDON_DEBRIS_AUTHORING extract RESULT FAIL inputs (%s)"), *Joined);
			return;
		}
		const FString DefaultTarget = FPackageName::ObjectPathToPackageName(GetObjectPath(HullPath)) + DebrisMeshSuffix
			+ (bOwnPanel ? FString::Printf(TEXT("_Panel%d"), PanelIndex) : FString());
		TargetPath = TargetPath.IsEmpty() ? DefaultTarget : TargetPath;
		FMeshDescription Description = *HullDescription;
		const FAddOnArmorPanel& Panel = Layout->GetPanels()[PanelIndex];
		const bool bSideAllowed = bOwnPanel || Panel.Side == EAddOnArmorPanelSide::Left;
		if (not bSideAllowed || not KeepOnlyPanel(Description, PanelIndex, Panel.MeshSpaceCentre))
		{
			UE_LOG(LogRTSAddOnArmorDebrisAuthoring, Error,
			       TEXT("RTS_ADDON_DEBRIS_AUTHORING extract RESULT FAIL panel %d is not a left-side panel with geometry")
			       TEXT(" (pass -OwnPanel for a right panel's own mesh)"), PanelIndex);
			return;
		}
		const int32 TriangleCount = Description.Triangles().Num();
		UStaticMesh* const Debris = WriteDebrisMesh(*Hull, MoveTemp(Description), TargetPath);
		const bool bStored = IsValid(Debris) && StoreDebrisMesh(*Hull, *Debris, bOwnPanel ? PanelIndex : INDEX_NONE);
		UE_LOG(LogRTSAddOnArmorDebrisAuthoring, Display,
		       TEXT("RTS_ADDON_DEBRIS_AUTHORING extract RESULT %s panel=%d own=%d tris=%d bounds=%s target=%s"),
		       bStored ? TEXT("DONE") : TEXT("FAIL"), PanelIndex, bOwnPanel ? 1 : 0, TriangleCount,
		       IsValid(Debris) ? *Debris->GetBoundingBox().GetSize().ToCompactString() : TEXT("-"), *TargetPath);
	}

	FAutoConsoleCommand BuildDebrisSystemCommand(
		TEXT("RTS.AddOnArmor.BuildDebrisSystem"),
		TEXT("Builds NS_AddOnArmorPanelDebris, the pooled Niagara burst that throws shot-off add-on armour panels."),
		FConsoleCommandDelegate::CreateStatic(&BuildDebrisSystem));

	FAutoConsoleCommand SetPanelDebrisMeshCommand(
		TEXT("RTS.AddOnArmor.SetPanelDebrisMesh"),
		TEXT("Stores a modelled debris panel on a panel hull's layout. Args: Hull=<_AddOnPanels mesh> Debris=<panel mesh>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SetPanelDebrisMesh));

	FAutoConsoleCommand ExtractPanelDebrisCommand(
		TEXT("RTS.AddOnArmor.ExtractPanelDebris"),
		TEXT("Cuts one left-side panel out of a panel hull as its debris panel and stores it on the layout. "
			"Args: Hull=<_AddOnPanels mesh> [Panel=<layout index>] [Target=<package, default <hull>_Debris>] "
			"[-OwnPanel: store it as that panel's own debris mesh (any side, as it hangs; default target <hull>_Debris_Panel<i>)]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ExtractPanelDebris));
}

#endif // WITH_EDITOR
