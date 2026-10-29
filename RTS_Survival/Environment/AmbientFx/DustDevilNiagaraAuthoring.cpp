// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only authoring step for the dust devil Niagara system (see ULandscapeAmbientFxSubsystem).
// The console command RTS.AmbientFx.PatchNiagaraSystem turns every emitter named DustDevil_<Layer> into a GPU emitter
// that draws one layer of every dust devil: it declares the particle attributes, inserts the custom HLSL nodes
// DustDevilSpawn (end of particle spawn) and DustDevilUpdate (end of particle update), wires them to the devil
// arrays and writes their code. Layers: Column (the twisting column), Skirt (dust rolling over the ground at its
// foot) and Grit (dark specks whirling inside). An emitter needs a SpawnRate module whose rate is bound to
// User.<EmitterName>_SpawnRate, and a sprite renderer. The code is read from DustDevilHlsl/*.hlsl; run the command
// again after changing those files.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_Niagara.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NiagaraDataInterfaceArrayFloat.h"
#include "NiagaraDataInterfaceArrayInt.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "RTS_Survival/Environment/AmbientFx/RTSAmbientFxSettings.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSDustDevilAuthoring, Log, All);

namespace DustDevilNiagaraAuthoring
{
	const TCHAR* EmitterPrefix = TEXT("DustDevil_");
	const TCHAR* SpawnNodeName = TEXT("DustDevilSpawn");
	const TCHAR* UpdateNodeName = TEXT("DustDevilUpdate");
	// Marks the parameter map get nodes this step creates, so a second run replaces them instead of adding more.
	const TCHAR* InputsNodeComment = TEXT("DustDevilInputs");
	// EmitterState's "Loop Behavior" set to Infinite and "Scalability Mode" set to System, as for the trace impacts.
	const TCHAR* InfiniteLoopEnumValue = TEXT("NewEnumerator0");
	const TCHAR* SystemScalabilityEnumValue = TEXT("NewEnumerator0");
	// Particles live in world space around the component at the origin; this box covers any map.
	constexpr double FixedBoundsHalfExtent = 2000000.0;
	constexpr int32 InputsNodeOffsetX = 350;
	constexpr int32 InputsNodeOffsetY = 250;
	constexpr int32 HlslNodeOffsetX = 250;

	const FName SlotAttribute = TEXT("Particles.DevilSlot");
	const FName ShapeAttribute = TEXT("Particles.DevilShape");
	// Which of the 2x2 dust puffs of T_AmbientDust_Puffs the sprite shows (sprite renderer Sub Image Size 2x2).
	const FName SubImageAttribute = TEXT("Particles.SubImageIndex");

	// The HLSL lives in .hlsl files next to this one (Common + one per layer), split into "// @section <Name>" parts,
	// so the look can be tuned and re-patched without rebuilding the editor.
	const TCHAR* HlslFolder = TEXT("Source/RTS_Survival/Environment/AmbientFx/DustDevilHlsl");
	const TCHAR* SectionMarker = TEXT("// @section ");
	const TCHAR* GeneratedHeader = TEXT("// Written by the console command RTS.AmbientFx.PatchNiagaraSystem from DustDevilHlsl/*.hlsl; edits here are overwritten.\n");

	struct FDustDevilLayer
	{
		const TCHAR* Name;
		// Particles per second for one devil at full strength.
		float PerDevilRate;
	};

	// Column: the twisting column. Skirt: dust rolling over the ground at its foot. Grit: dark specks whirling inside.
	const FDustDevilLayer Layers[] = {
		{TEXT("Column"), 70.f},
		{TEXT("Skirt"), 22.f},
		{TEXT("Grit"), 40.f}
	};

	const FDustDevilLayer* FindLayer(const FString& LayerName)
	{
		for (const FDustDevilLayer& Layer : Layers)
		{
			if (LayerName == Layer.Name)
			{
				return &Layer;
			}
		}
		return nullptr;
	}

	/**
	 * @brief Reads one .hlsl file of DustDevilHlsl and splits it at its section markers.
	 * @param FileName File name inside the HLSL folder, for example Column.hlsl.
	 * @param OutSections Section name to the code below its marker, up to the next marker.
	 * @return False when the file could not be read.
	 */
	bool ReadHlslSections(const FString& FileName, TMap<FString, FString>& OutSections)
	{
		const FString FilePath = FPaths::Combine(FPaths::ProjectDir(), HlslFolder, FileName);
		FString FileText;
		if (not FFileHelper::LoadFileToString(FileText, *FilePath))
		{
			UE_LOG(LogRTSDustDevilAuthoring, Error, TEXT("RTS_DUST_DEVIL_AUTHORING cannot read %s"), *FilePath);
			return false;
		}
		TArray<FString> Lines;
		FileText.ParseIntoArrayLines(Lines, false);
		FString* CurrentSection = nullptr;
		for (const FString& Line : Lines)
		{
			if (Line.StartsWith(SectionMarker))
			{
				CurrentSection = &OutSections.Add(Line.RightChop(FCString::Strlen(SectionMarker)).TrimStartAndEnd());
				continue;
			}
			if (CurrentSection != nullptr)
			{
				*CurrentSection += Line + TEXT("\n");
			}
		}
		return true;
	}

	struct FDustDevilHlsl
	{
		FString Spawn;
		FString Update;
	};

	bool BuildLayerHlsl(const FDustDevilLayer& Layer, FDustDevilHlsl& OutHlsl)
	{
		TMap<FString, FString> Common;
		TMap<FString, FString> LayerSections;
		if (not ReadHlslSections(TEXT("Common.hlsl"), Common)
			|| not ReadHlslSections(FString(Layer.Name) + TEXT(".hlsl"), LayerSections))
		{
			return false;
		}
		for (const TCHAR* Section : {TEXT("SpawnBegin"), TEXT("SpawnEnd"), TEXT("UpdateBegin"), TEXT("ReadDevil")})
		{
			if (not Common.Contains(Section))
			{
				UE_LOG(LogRTSDustDevilAuthoring, Error, TEXT("RTS_DUST_DEVIL_AUTHORING Common.hlsl has no section %s"), Section);
				return false;
			}
		}
		if (not LayerSections.Contains(TEXT("Shape")) || not LayerSections.Contains(TEXT("Place")))
		{
			UE_LOG(LogRTSDustDevilAuthoring, Error, TEXT("RTS_DUST_DEVIL_AUTHORING %s.hlsl needs sections Shape and Place"), Layer.Name);
			return false;
		}
		OutHlsl.Spawn = GeneratedHeader + Common[TEXT("SpawnBegin")] + LayerSections[TEXT("Shape")]
			+ Common[TEXT("SpawnEnd")] + Common[TEXT("ReadDevil")] + LayerSections[TEXT("Place")];
		OutHlsl.Update = GeneratedHeader + Common[TEXT("UpdateBegin")] + Common[TEXT("ReadDevil")]
			+ LayerSections[TEXT("Place")];
		return true;
	}

	FNiagaraTypeDefinition GetFloat4ArrayType()
	{
		return FNiagaraTypeDefinition(UNiagaraDataInterfaceArrayFloat4::StaticClass());
	}

	FNiagaraTypeDefinition GetIntArrayType()
	{
		return FNiagaraTypeDefinition(UNiagaraDataInterfaceArrayInt32::StaticClass());
	}

	void EnsureUserParameter(UNiagaraSystem& System, const FNiagaraVariable& Parameter)
	{
		FNiagaraUserRedirectionParameterStore& UserParameters = System.GetExposedParameters();
		if (UserParameters.FindParameterOffset(Parameter) == nullptr)
		{
			UserParameters.AddParameter(Parameter, true);
		}
	}

	void EnsureUserFloatParameter(UNiagaraSystem& System, const FString& Name, const float DefaultValue)
	{
		FNiagaraUserRedirectionParameterStore& UserParameters = System.GetExposedParameters();
		const FNiagaraVariable Parameter(FNiagaraTypeDefinition::GetFloatDef(), *(TEXT("User.") + Name));
		if (UserParameters.FindParameterOffset(Parameter) == nullptr)
		{
			UserParameters.SetParameterValue<float>(DefaultValue, Parameter, true);
		}
	}

	void EnsureSystemUserParameters(UNiagaraSystem& System)
	{
		EnsureUserParameter(System, FNiagaraVariable(GetFloat4ArrayType(), TEXT("User.DevilPositionAndScale")));
		EnsureUserParameter(System, FNiagaraVariable(GetFloat4ArrayType(), TEXT("User.DevilMotion")));
		EnsureUserParameter(System, FNiagaraVariable(GetIntArrayType(), TEXT("User.AliveSlots")));
		EnsureUserParameter(System, FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("User.AliveCount")));
		FNiagaraUserRedirectionParameterStore& UserParameters = System.GetExposedParameters();
		const FNiagaraVariable DustColor(FNiagaraTypeDefinition::GetColorDef(), TEXT("User.DustColor"));
		if (UserParameters.FindParameterOffset(DustColor) == nullptr)
		{
			UserParameters.SetParameterValue<FLinearColor>(URTSAmbientFxSettings::Get()->DustColor, DustColor, true);
		}
	}

	bool GetIsParameterMapPin(const UEdGraphPin& Pin)
	{
		return Pin.PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct();
	}

	UEdGraphPin* FindParameterMapPin(UEdGraphNode& Node, const EEdGraphPinDirection Direction)
	{
		for (UEdGraphPin* Pin : Node.Pins)
		{
			if (Pin->Direction == Direction && GetIsParameterMapPin(*Pin))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	UNiagaraNodeCustomHlsl* FindHlslNode(UEdGraph& Graph, const TCHAR* NodeName)
	{
		for (UEdGraphNode* Node : Graph.Nodes)
		{
			UNiagaraNodeCustomHlsl* HlslNode = Cast<UNiagaraNodeCustomHlsl>(Node);
			if (HlslNode != nullptr && HlslNode->GetFunctionName() == NodeName)
			{
				return HlslNode;
			}
		}
		return nullptr;
	}

	/**
	 * @brief Puts a new custom HLSL node with a parameter map in and out between the last module of a script and
	 * its output node, so it runs after every module of that script.
	 * @param Graph The emitter graph.
	 * @param OutputNode The output node of the particle spawn or update script.
	 * @param NodeName Name the node and its HLSL function get.
	 * @return The new node, or null when the script has no module chain to attach to.
	 */
	UNiagaraNodeCustomHlsl* InsertHlslNodeBeforeOutput(UEdGraph& Graph, UNiagaraNodeOutput& OutputNode, const TCHAR* NodeName)
	{
		UEdGraphPin* OutputMapPin = FindParameterMapPin(OutputNode, EGPD_Input);
		if (OutputMapPin == nullptr || OutputMapPin->LinkedTo.IsEmpty())
		{
			return nullptr;
		}
		UEdGraphPin* PreviousMapPin = OutputMapPin->LinkedTo[0];
		UNiagaraNodeCustomHlsl* HlslNode = NewObject<UNiagaraNodeCustomHlsl>(&Graph, NAME_None, RF_Transactional);
		const FNiagaraVariable MapVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("Map"));
		HlslNode->Signature.Inputs.Add(MapVariable);
		HlslNode->Signature.Outputs.Add(MapVariable);
		Graph.AddNode(HlslNode, false, false);
		HlslNode->CreateNewGuid();
		HlslNode->PostPlacedNewNode();
		HlslNode->AllocateDefaultPins();
		HlslNode->OnRenameNode(NodeName);
		HlslNode->NodePosX = OutputNode.NodePosX - HlslNodeOffsetX;
		HlslNode->NodePosY = OutputNode.NodePosY;

		UEdGraphPin* HlslInputMapPin = FindParameterMapPin(*HlslNode, EGPD_Input);
		UEdGraphPin* HlslOutputMapPin = FindParameterMapPin(*HlslNode, EGPD_Output);
		if (HlslInputMapPin == nullptr || HlslOutputMapPin == nullptr)
		{
			Graph.RemoveNode(HlslNode);
			return nullptr;
		}
		OutputMapPin->BreakLinkTo(PreviousMapPin);
		PreviousMapPin->MakeLinkTo(HlslInputMapPin);
		HlslOutputMapPin->MakeLinkTo(OutputMapPin);
		UE_LOG(LogRTSDustDevilAuthoring, Display, TEXT("RTS_DUST_DEVIL_AUTHORING inserted node=%s pins=%d"),
		       NodeName, HlslNode->Pins.Num());
		return HlslNode;
	}

	// Removes what an earlier run made: the data pins of the HLSL node and the parameter map get node that fed them.
	void RemoveEarlierWiring(UEdGraph& Graph, UNiagaraNodeCustomHlsl& HlslNode)
	{
		for (int32 PinIndex = HlslNode.Pins.Num() - 1; PinIndex >= 0; --PinIndex)
		{
			UEdGraphPin* Pin = HlslNode.Pins[PinIndex];
			const bool bIsAddPin = Pin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryMisc;
			if (not GetIsParameterMapPin(*Pin) && not bIsAddPin)
			{
				Pin->BreakAllPinLinks();
				HlslNode.RemovePin(Pin);
			}
		}
		const FString NodeComment = FString(InputsNodeComment) + HlslNode.GetFunctionName();
		for (int32 NodeIndex = Graph.Nodes.Num() - 1; NodeIndex >= 0; --NodeIndex)
		{
			UEdGraphNode* Node = Graph.Nodes[NodeIndex];
			if (Node != nullptr && Node->NodeComment == NodeComment)
			{
				Node->BreakAllNodeLinks();
				Graph.RemoveNode(Node);
			}
		}
	}

	/**
	 * @brief Adds an output to the parameter map get node the way the Niagara editor does: with a hidden
	 * default pin that the node keeps for every output.
	 * @param InputsNode The parameter map get node.
	 * @param ParameterName Full name of the parameter to read, for example User.AliveCount.
	 * @param Type Type of the parameter.
	 * @return The output pin that carries the parameter.
	 */
	UEdGraphPin* AddParameterOutput(UEdGraphNode& InputsNode, const FName ParameterName, const FNiagaraTypeDefinition& Type)
	{
		FEdGraphPinType PinType = UEdGraphSchema_Niagara::TypeDefinitionToPinType(Type);
		PinType.PinSubCategory = TEXT("ParameterPin");
		UEdGraphPin* OutputPin = InputsNode.CreatePin(EGPD_Output, PinType, ParameterName);
		OutputPin->PersistentGuid = FGuid::NewGuid();

		UEdGraphPin* DefaultPin = InputsNode.CreatePin(EGPD_Input, PinType, NAME_None);
		DefaultPin->PersistentGuid = FGuid::NewGuid();
		DefaultPin->bNotConnectable = true;
		DefaultPin->bDefaultValueIsIgnored = true;

		const FMapProperty* DefaultPinMapProperty = FindFProperty<FMapProperty>(
			InputsNode.GetClass(), TEXT("PinOutputToPinDefaultPersistentId"));
		if (DefaultPinMapProperty != nullptr)
		{
			TMap<FGuid, FGuid>* DefaultPinMap =
				DefaultPinMapProperty->ContainerPtrToValuePtr<TMap<FGuid, FGuid>>(&InputsNode);
			DefaultPinMap->Add(OutputPin->PersistentGuid, DefaultPin->PersistentGuid);
		}
		return OutputPin;
	}

	UEdGraphNode* CreateInputsNode(UEdGraph& Graph, const UNiagaraNodeCustomHlsl& HlslNode)
	{
		UClass* ParameterMapGetClass = FindObject<UClass>(nullptr, TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"));
		if (ParameterMapGetClass == nullptr)
		{
			return nullptr;
		}
		UEdGraphNode* InputsNode = NewObject<UEdGraphNode>(&Graph, ParameterMapGetClass, NAME_None, RF_Transactional);
		Graph.AddNode(InputsNode, false, false);
		InputsNode->CreateNewGuid();
		InputsNode->PostPlacedNewNode();
		InputsNode->AllocateDefaultPins();
		InputsNode->NodeComment = FString(InputsNodeComment) + HlslNode.GetFunctionName();
		InputsNode->NodePosX = HlslNode.NodePosX - InputsNodeOffsetX;
		InputsNode->NodePosY = HlslNode.NodePosY + InputsNodeOffsetY;
		return InputsNode;
	}

	void AddHlslNodeInput(
		UNiagaraNodeCustomHlsl& HlslNode,
		UEdGraphNode& InputsNode,
		const FName PinName,
		const FString& ParameterName,
		const FNiagaraTypeDefinition& Type)
	{
		UEdGraphPin* InputPin = HlslNode.CreatePin(EGPD_Input, UEdGraphSchema_Niagara::TypeDefinitionToPinType(Type), PinName);
		UEdGraphPin* ParameterPin = AddParameterOutput(InputsNode, FName(*ParameterName), Type);
		ParameterPin->MakeLinkTo(InputPin);
		HlslNode.Signature.Inputs.Add(FNiagaraVariable(Type, PinName));
	}

	bool SetHlsl(UNiagaraNodeCustomHlsl& HlslNode, const FString& Hlsl)
	{
		FStrProperty* HlslProperty = FindFProperty<FStrProperty>(HlslNode.GetClass(), TEXT("CustomHlsl"));
		if (HlslProperty == nullptr)
		{
			return false;
		}
		HlslProperty->SetPropertyValue_InContainer(&HlslNode, Hlsl);
		// Tells the node its code changed, which marks the script for a recompile.
		FPropertyChangedEvent HlslChangedEvent(HlslProperty);
		HlslNode.PostEditChangeProperty(HlslChangedEvent);
		return true;
	}

	bool WireHlslNode(UEdGraph& Graph, UNiagaraNodeCustomHlsl& HlslNode, const FString& Hlsl)
	{
		RemoveEarlierWiring(Graph, HlslNode);
		UEdGraphPin* InputMapPin = FindParameterMapPin(HlslNode, EGPD_Input);
		UEdGraphNode* InputsNode = CreateInputsNode(Graph, HlslNode);
		if (InputMapPin == nullptr || InputMapPin->LinkedTo.IsEmpty() || InputsNode == nullptr)
		{
			return false;
		}
		UEdGraphPin* SourcePin = InputsNode->FindPin(TEXT("Source"), EGPD_Input);
		if (SourcePin == nullptr)
		{
			return false;
		}
		SourcePin->MakeLinkTo(InputMapPin->LinkedTo[0]);

		// Only the parameter map input stays; the data inputs are added again in the order the HLSL expects.
		HlslNode.Signature.Inputs.SetNum(1);
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("DevilPositionAndScale"), TEXT("User.DevilPositionAndScale"), GetFloat4ArrayType());
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("DevilMotion"), TEXT("User.DevilMotion"), GetFloat4ArrayType());
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("AliveSlots"), TEXT("User.AliveSlots"), GetIntArrayType());
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("AliveCount"), TEXT("User.AliveCount"), FNiagaraTypeDefinition::GetIntDef());
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("DustColor"), TEXT("User.DustColor"), FNiagaraTypeDefinition::GetColorDef());
		// Particle attributes the update reads but never writes must come in through a parameter map read: a token
		// read inside custom HLSL alone does not make the GPU script load the attribute, so it would read zero.
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("DevilSlotIn"), SlotAttribute.ToString(), FNiagaraTypeDefinition::GetIntDef());
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("DevilShapeIn"), ShapeAttribute.ToString(), FNiagaraTypeDefinition::GetVec4Def());
		return SetHlsl(HlslNode, Hlsl);
	}

	void SetPinDefaultWhenPresent(UEdGraphNode& Node, const TCHAR* PinName, const TCHAR* DefaultValue)
	{
		UEdGraphPin* Pin = Node.FindPin(PinName, EGPD_Input);
		if (Pin != nullptr)
		{
			Pin->DefaultValue = DefaultValue;
		}
	}

	// The emitters must stay alive while the component is active; the subsystem switches the component off when
	// no devil is left, so nothing is culled by distance or visibility here.
	void MakeEmitterLoopForever(UEdGraph& Graph)
	{
		for (UEdGraphNode* Node : Graph.Nodes)
		{
			const UNiagaraNodeFunctionCall* FunctionNode = Cast<UNiagaraNodeFunctionCall>(Node);
			if (FunctionNode == nullptr || FunctionNode->GetFunctionName() != TEXT("EmitterState"))
			{
				continue;
			}
			SetPinDefaultWhenPresent(*Node, TEXT("Loop Behavior"), InfiniteLoopEnumValue);
			SetPinDefaultWhenPresent(*Node, TEXT("Scalability Mode"), SystemScalabilityEnumValue);
			SetPinDefaultWhenPresent(*Node, TEXT("Enable Distance Culling"), TEXT("false"));
			SetPinDefaultWhenPresent(*Node, TEXT("Enable Visibility Culling"), TEXT("false"));
		}
	}

	bool GetHasAttributeAssignment(UEdGraph& Graph, const FName AttributeName)
	{
		for (UEdGraphNode* Node : Graph.Nodes)
		{
			const UNiagaraNodeAssignment* AssignmentNode = Cast<UNiagaraNodeAssignment>(Node);
			if (AssignmentNode == nullptr)
			{
				continue;
			}
			for (const FNiagaraVariable& Target : AssignmentNode->GetAssignmentTargets())
			{
				if (Target.GetName() == AttributeName)
				{
					return true;
				}
			}
		}
		return false;
	}

	// The HLSL can only write attributes the script already knows, so a Set Parameters module declares them first.
	void EnsureAttributesDeclared(UEdGraph& Graph, UNiagaraNodeOutput& SpawnOutputNode)
	{
		if (GetHasAttributeAssignment(Graph, SlotAttribute))
		{
			return;
		}
		const TArray<FNiagaraVariable> Attributes = {
			FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), SlotAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), ShapeAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), SubImageAttribute)
		};
		const TArray<FString> DefaultValues = {TEXT("0"), TEXT("0,0,0,0"), TEXT("0")};
		FNiagaraStackGraphUtilities::AddParameterModuleToStack(Attributes, SpawnOutputNode, INDEX_NONE, DefaultValues);
	}

	UNiagaraNodeCustomHlsl* FindOrInsertHlslNode(UNiagaraGraph& Graph, const ENiagaraScriptUsage Usage, const TCHAR* NodeName)
	{
		UNiagaraNodeCustomHlsl* HlslNode = FindHlslNode(Graph, NodeName);
		if (HlslNode != nullptr)
		{
			return HlslNode;
		}
		UNiagaraNodeOutput* OutputNode = Graph.FindEquivalentOutputNode(Usage);
		return OutputNode != nullptr ? InsertHlslNodeBeforeOutput(Graph, *OutputNode, NodeName) : nullptr;
	}

	void SetEmitterProperties(FVersionedNiagaraEmitterData& EmitterData)
	{
		EmitterData.bLocalSpace = false;
		EmitterData.SimTarget = ENiagaraSimTarget::GPUComputeSim;
		EmitterData.CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
		EmitterData.FixedBounds = FBox(FVector(-FixedBoundsHalfExtent), FVector(FixedBoundsHalfExtent));
		// Attribute trimming does not see the custom HLSL read these, and would drop them from the particle data:
		// every update would then read slot 0 and a flat shape.
		for (const FName& Attribute : {SlotAttribute, ShapeAttribute, SubImageAttribute})
		{
			EmitterData.AttributesToPreserve.AddUnique(Attribute.ToString());
		}
	}

	bool PatchEmitter(UNiagaraSystem& System, const FNiagaraEmitterHandle& EmitterHandle)
	{
		FString EmitterName = EmitterHandle.GetName().ToString();
		FString LayerName = EmitterName;
		const FDustDevilLayer* Layer = LayerName.RemoveFromStart(EmitterPrefix) ? FindLayer(LayerName) : nullptr;
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
		UNiagaraScriptSource* ScriptSource = EmitterData != nullptr ? Cast<UNiagaraScriptSource>(EmitterData->GraphSource) : nullptr;
		UNiagaraGraph* Graph = ScriptSource != nullptr ? ScriptSource->NodeGraph : nullptr;
		UNiagaraNodeOutput* SpawnOutputNode = Graph != nullptr
			? Graph->FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleSpawnScript)
			: nullptr;
		if (Layer == nullptr || SpawnOutputNode == nullptr)
		{
			UE_LOG(LogRTSDustDevilAuthoring, Warning, TEXT("RTS_DUST_DEVIL_AUTHORING skipped emitter=%s layer=%d graph=%d"),
			       *EmitterName, Layer != nullptr ? 1 : 0, SpawnOutputNode != nullptr ? 1 : 0);
			return false;
		}
		Graph->Modify();
		SetEmitterProperties(*EmitterData);
		MakeEmitterLoopForever(*Graph);
		EnsureUserFloatParameter(System, EmitterName + TEXT("_SpawnRate"), 0.f);
		EnsureUserFloatParameter(System, EmitterName + TEXT("_PerDevilRate"), Layer->PerDevilRate);
		EnsureAttributesDeclared(*Graph, *SpawnOutputNode);

		UNiagaraNodeCustomHlsl* SpawnNode = FindOrInsertHlslNode(*Graph, ENiagaraScriptUsage::ParticleSpawnScript, SpawnNodeName);
		UNiagaraNodeCustomHlsl* UpdateNode = FindOrInsertHlslNode(*Graph, ENiagaraScriptUsage::ParticleUpdateScript, UpdateNodeName);
		FDustDevilHlsl Hlsl;
		const bool bHasHlsl = BuildLayerHlsl(*Layer, Hlsl);
		const bool bIsSpawnWired = bHasHlsl && SpawnNode != nullptr && WireHlslNode(*Graph, *SpawnNode, Hlsl.Spawn);
		const bool bIsUpdateWired = bHasHlsl && UpdateNode != nullptr && WireHlslNode(*Graph, *UpdateNode, Hlsl.Update);
		Graph->NotifyGraphChanged();
		UE_LOG(LogRTSDustDevilAuthoring, Display, TEXT("RTS_DUST_DEVIL_AUTHORING emitter=%s layer=%s spawn=%d update=%d"),
		       *EmitterName, Layer->Name, bIsSpawnWired ? 1 : 0, bIsUpdateWired ? 1 : 0);
		return bIsSpawnWired && bIsUpdateWired;
	}

	void LogCompileResults(const UNiagaraSystem& System)
	{
		for (const FNiagaraEmitterHandle& EmitterHandle : System.GetEmitterHandles())
		{
			const FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
			if (EmitterData == nullptr)
			{
				continue;
			}
			TArray<UNiagaraScript*> Scripts;
			EmitterData->GetScripts(Scripts, true);
			for (const UNiagaraScript* Script : Scripts)
			{
				const FNiagaraVMExecutableData& ExecutableData = Script->GetVMExecutableData();
				// The translated code, for checking what the custom HLSL turned into: Saved/AmbientFx/Hlsl/.
				const FString DumpPath = FPaths::ProjectSavedDir() / TEXT("AmbientFx/Hlsl")
					/ (EmitterHandle.GetName().ToString() + TEXT("_") + Script->GetName() + TEXT(".hlsl"));
				FFileHelper::SaveStringToFile(ExecutableData.LastHlslTranslation + TEXT("\n// ---- GPU ----\n")
				                              + ExecutableData.LastHlslTranslationGPU, *DumpPath);
				UE_LOG(LogRTSDustDevilAuthoring, Display,
				       TEXT("RTS_DUST_DEVIL_AUTHORING compiled emitter=%s script=%s status=%d errors=%s"),
				       *EmitterHandle.GetName().ToString(), *Script->GetName(),
				       static_cast<int32>(ExecutableData.LastCompileStatus), *ExecutableData.ErrorMsg);
			}
		}
	}

	void PatchDustDevilNiagaraSystem()
	{
		const URTSAmbientFxSettings* Settings = URTSAmbientFxSettings::Get();
		UNiagaraSystem* System = Settings != nullptr ? Settings->DustDevilNiagaraSystem.LoadSynchronous() : nullptr;
		if (System == nullptr)
		{
			UE_LOG(LogRTSDustDevilAuthoring, Error, TEXT("RTS_DUST_DEVIL_AUTHORING RESULT FAIL system not found"));
			return;
		}
		System->Modify();
		EnsureSystemUserParameters(*System);
		System->bFixedBounds = true;
		System->SetFixedBounds(FBox(FVector(-FixedBoundsHalfExtent), FVector(FixedBoundsHalfExtent)));
		int32 PatchedEmitterCount = 0;
		for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
		{
			PatchedEmitterCount += PatchEmitter(*System, EmitterHandle) ? 1 : 0;
		}
		System->RequestCompile(true);
		System->WaitForCompilationComplete();
		LogCompileResults(*System);
		System->MarkPackageDirty();
		const bool bIsSaved = UEditorLoadingAndSavingUtils::SavePackages({System->GetOutermost()}, false);
		UE_LOG(LogRTSDustDevilAuthoring, Display, TEXT("RTS_DUST_DEVIL_AUTHORING RESULT DONE patched_emitters=%d of %d saved=%d"),
		       PatchedEmitterCount, System->GetEmitterHandles().Num(), bIsSaved ? 1 : 0);
	}

	FAutoConsoleCommand PatchDustDevilNiagaraSystemCommand(
		TEXT("RTS.AmbientFx.PatchNiagaraSystem"),
		TEXT("Turns every DustDevil_<Layer> emitter of the dust devil Niagara system into a GPU emitter wired to the devil arrays, and saves it."),
		FConsoleCommandDelegate::CreateStatic(&PatchDustDevilNiagaraSystem));
}

#endif // WITH_EDITOR
