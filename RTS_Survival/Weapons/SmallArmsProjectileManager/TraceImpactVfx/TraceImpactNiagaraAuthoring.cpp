// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only authoring step for the trace impact Niagara system (see UTraceImpactVfxBatcher).
// The console command RTS.TraceImpactVfx.PatchNiagaraSystem wires every emitter named <ImpactType>_<Layer>
// to the impact arrays: it fills in the emitter's "PlaceAtTraceImpact" custom HLSL node, which moves each
// spawned particle to the impact it belongs to. Run it again after adding an emitter to the system; an emitter
// needs a SpawnPerFrame module bound to User.<EmitterName>_SpawnCount and an empty custom HLSL node named
// PlaceAtTraceImpact at the end of its particle spawn script.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_Niagara.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "NiagaraDataInterfaceArrayFloat.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "RTS_Survival/Weapons/RTSWeaponVFXSettings/RTSWeaponVFXSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSTraceImpactAuthoring, Log, All);

namespace TraceImpactNiagaraAuthoring
{
	const TCHAR* PlaceNodeName = TEXT("PlaceAtTraceImpact");
	// Marks the parameter map get node this step creates, so a second run replaces it instead of adding one.
	const TCHAR* InputsNodeComment = TEXT("TraceImpactInputs");
	const TCHAR* ImpactTypeNames[] = {
		TEXT("Sand"), TEXT("Metal"), TEXT("Flesh"), TEXT("Air"), TEXT("Stone"), TEXT("Bounce")
	};
	const FName PositionAndScalePinName = TEXT("ImpactPositionAndScale");
	const FName NormalPinName = TEXT("ImpactNormal");
	const FName TypeOffsetPinName = TEXT("TypeOffset");
	const FName TypeCountPinName = TEXT("TypeCount");
	const FName ExecCountPinName = TEXT("ExecCount");
	// EmitterState's "Loop Behavior" set to Infinite: the emitters must stay alive between impacts.
	const TCHAR* InfiniteLoopEnumValue = TEXT("NewEnumerator0");
	// EmitterState's "Scalability Mode" set to System: no emitter decides by itself to go to sleep.
	const TCHAR* SystemScalabilityEnumValue = TEXT("NewEnumerator0");
	constexpr int32 InputsNodeOffsetX = 350;
	constexpr int32 InputsNodeOffsetY = 250;

	// How many particles the impact systems this was built from spawned per impact; 1 for all others.
	int32 GetDefaultParticlesPerImpact(const FString& EmitterName)
	{
		if (EmitterName.EndsWith(TEXT("_Debris")))
		{
			return 150;
		}
		// The stone effect comes from a larger calibre; a bullet gets a few puffs of it.
		if (EmitterName.EndsWith(TEXT("_Smoke")))
		{
			return 6;
		}
		const bool bIsSmallBurst = EmitterName.EndsWith(TEXT("_Sparks")) || EmitterName.EndsWith(TEXT("_Blood"));
		return bIsSmallBurst ? 5 : 1;
	}

	FNiagaraTypeDefinition GetFloat4ArrayType()
	{
		return FNiagaraTypeDefinition(UNiagaraDataInterfaceArrayFloat4::StaticClass());
	}

	void EnsureUserParameter(UNiagaraSystem& System, const FNiagaraVariable& Parameter)
	{
		FNiagaraUserRedirectionParameterStore& UserParameters = System.GetExposedParameters();
		if (UserParameters.FindParameterOffset(Parameter) == nullptr)
		{
			UserParameters.AddParameter(Parameter, true);
		}
	}

	void EnsureUserIntParameter(UNiagaraSystem& System, const FString& Name, const int32 DefaultValue)
	{
		FNiagaraUserRedirectionParameterStore& UserParameters = System.GetExposedParameters();
		const FNiagaraVariable Parameter(FNiagaraTypeDefinition::GetIntDef(), *(TEXT("User.") + Name));
		if (UserParameters.FindParameterOffset(Parameter) == nullptr)
		{
			UserParameters.SetParameterValue<int32>(DefaultValue, Parameter, true);
		}
	}

	// Removes the spawn parameters of emitters that are no longer in the system.
	void RemoveStaleEmitterParameters(UNiagaraSystem& System)
	{
		FNiagaraUserRedirectionParameterStore& UserParameterStore = System.GetExposedParameters();
		TArray<FNiagaraVariable> UserParameters;
		UserParameterStore.GetUserParameters(UserParameters);
		for (const FNiagaraVariable& UserParameter : UserParameters)
		{
			FString EmitterName = UserParameter.GetName().ToString();
			const bool bIsEmitterParameter = EmitterName.RemoveFromEnd(TEXT("_PerImpact"))
				|| EmitterName.RemoveFromEnd(TEXT("_SpawnCount"));
			const bool bHasEmitter = System.GetEmitterHandles().ContainsByPredicate(
				[&EmitterName](const FNiagaraEmitterHandle& EmitterHandle)
				{
					return EmitterHandle.GetName().ToString() == EmitterName;
				});
			if (bIsEmitterParameter && not bHasEmitter)
			{
				UserParameterStore.RemoveParameter(FNiagaraVariable(
					UserParameter.GetType(), FName(*(TEXT("User.") + UserParameter.GetName().ToString()))));
			}
		}
	}

	bool GetIsParameterMapPin(const UEdGraphPin& Pin)
	{
		return Pin.PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct();
	}

	UNiagaraNodeCustomHlsl* FindPlaceNode(UEdGraph& Graph)
	{
		for (UEdGraphNode* Node : Graph.Nodes)
		{
			UNiagaraNodeCustomHlsl* HlslNode = Cast<UNiagaraNodeCustomHlsl>(Node);
			if (HlslNode != nullptr && HlslNode->GetFunctionName() == PlaceNodeName)
			{
				return HlslNode;
			}
		}
		return nullptr;
	}

	// Whether a module before the place node in the spawn script gives the particles a velocity.
	bool GetSpawnScriptSetsVelocity(const UEdGraphPin& PlaceNodeInputMapPin)
	{
		const UEdGraphPin* UpstreamPin = PlaceNodeInputMapPin.LinkedTo.IsEmpty() ? nullptr : PlaceNodeInputMapPin.LinkedTo[0];
		while (UpstreamPin != nullptr)
		{
			const UEdGraphNode* UpstreamNode = UpstreamPin->GetOwningNode();
			const UNiagaraNodeFunctionCall* FunctionNode = Cast<UNiagaraNodeFunctionCall>(UpstreamNode);
			if (FunctionNode != nullptr && FunctionNode->GetFunctionName().Contains(TEXT("Velocity")))
			{
				return true;
			}
			UpstreamPin = nullptr;
			for (const UEdGraphPin* Pin : UpstreamNode->Pins)
			{
				if (Pin->Direction == EGPD_Input && GetIsParameterMapPin(*Pin) && not Pin->LinkedTo.IsEmpty())
				{
					UpstreamPin = Pin->LinkedTo[0];
					break;
				}
			}
		}
		return false;
	}

	FString BuildPlaceHlsl(const bool bRotatesVelocity)
	{
		FString Hlsl =
			TEXT("// Written by the console command RTS.TraceImpactVfx.PatchNiagaraSystem; edits here are overwritten.\n")
			TEXT("// Every impact of this emitter's type gets the same amount of the particles spawned this frame.\n")
			TEXT("int ImpactIndex = TypeOffset + (ExecIndex() * TypeCount) / max(ExecCount, 1);\n")
			TEXT("// Left without a start value on purpose: the VM compiler refuses a constant as an out argument.\n")
			TEXT("float4 ImpactPositionScale;\n")
			TEXT("float4 ImpactNormalValue;\n")
			TEXT("ImpactPositionAndScale.Get(ImpactIndex, ImpactPositionScale);\n")
			TEXT("ImpactNormal.Get(ImpactIndex, ImpactNormalValue);\n")
			TEXT("// The effect is authored pointing up; turn it so that up follows the surface normal.\n")
			TEXT("float3 NormalAxis = normalize(ImpactNormalValue.xyz + float3(0.0, 0.0, 0.0001));\n")
			TEXT("float UseUpAsReference = abs(NormalAxis.z) < 0.999 ? 1.0 : 0.0;\n")
			TEXT("float3 ReferenceAxis = float3(1.0 - UseUpAsReference, 0.0, UseUpAsReference);\n")
			TEXT("float3 TangentAxis = normalize(cross(ReferenceAxis, NormalAxis));\n")
			TEXT("float3 BitangentAxis = cross(NormalAxis, TangentAxis);\n")
			TEXT("float3 LocalOffset = Particles.Position;\n")
			TEXT("Particles.Position = ImpactPositionScale.xyz + (TangentAxis * LocalOffset.x + BitangentAxis * LocalOffset.y + NormalAxis * LocalOffset.z) * ImpactPositionScale.w;\n");
		if (bRotatesVelocity)
		{
			Hlsl +=
				TEXT("float3 LocalVelocity = Particles.Velocity;\n")
				TEXT("Particles.Velocity = (TangentAxis * LocalVelocity.x + BitangentAxis * LocalVelocity.y + NormalAxis * LocalVelocity.z) * ImpactPositionScale.w;\n");
		}
		return Hlsl;
	}

	// Removes what an earlier run made: the data pins of the place node and the node that fed them.
	void RemoveEarlierWiring(UEdGraph& Graph, UNiagaraNodeCustomHlsl& PlaceNode)
	{
		for (int32 PinIndex = PlaceNode.Pins.Num() - 1; PinIndex >= 0; --PinIndex)
		{
			UEdGraphPin* Pin = PlaceNode.Pins[PinIndex];
			const bool bIsAddPin = Pin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryMisc;
			if (not GetIsParameterMapPin(*Pin) && not bIsAddPin)
			{
				Pin->BreakAllPinLinks();
				PlaceNode.RemovePin(Pin);
			}
		}
		for (int32 NodeIndex = Graph.Nodes.Num() - 1; NodeIndex >= 0; --NodeIndex)
		{
			UEdGraphNode* Node = Graph.Nodes[NodeIndex];
			if (Node != nullptr && Node->NodeComment == InputsNodeComment)
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
	 * @param ParameterName Full name of the parameter to read, for example User.SandCount.
	 * @param Type Type of the parameter.
	 * @return The output pin that carries the parameter.
	 */
	UEdGraphPin* AddParameterOutput(
		UEdGraphNode& InputsNode,
		const FName ParameterName,
		const FNiagaraTypeDefinition& Type)
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

	UEdGraphNode* CreateInputsNode(UEdGraph& Graph, const UNiagaraNodeCustomHlsl& PlaceNode)
	{
		UClass* ParameterMapGetClass = FindObject<UClass>(
			nullptr, TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"));
		if (ParameterMapGetClass == nullptr)
		{
			return nullptr;
		}
		UEdGraphNode* InputsNode = NewObject<UEdGraphNode>(&Graph, ParameterMapGetClass, NAME_None, RF_Transactional);
		Graph.AddNode(InputsNode, false, false);
		InputsNode->CreateNewGuid();
		InputsNode->PostPlacedNewNode();
		InputsNode->AllocateDefaultPins();
		InputsNode->NodeComment = InputsNodeComment;
		InputsNode->NodePosX = PlaceNode.NodePosX - InputsNodeOffsetX;
		InputsNode->NodePosY = PlaceNode.NodePosY + InputsNodeOffsetY;
		return InputsNode;
	}

	/**
	 * @brief Gives the place node an input fed by a parameter of the inputs node.
	 * @param PlaceNode The custom HLSL node.
	 * @param InputsNode The parameter map get node.
	 * @param PinName Name the HLSL uses for the input.
	 * @param ParameterName Parameter that feeds it.
	 * @param Type Type of both.
	 */
	void AddPlaceNodeInput(
		UNiagaraNodeCustomHlsl& PlaceNode,
		UEdGraphNode& InputsNode,
		const FName PinName,
		const FString& ParameterName,
		const FNiagaraTypeDefinition& Type)
	{
		UEdGraphPin* InputPin = PlaceNode.CreatePin(
			EGPD_Input, UEdGraphSchema_Niagara::TypeDefinitionToPinType(Type), PinName);
		UEdGraphPin* ParameterPin = AddParameterOutput(InputsNode, FName(*ParameterName), Type);
		ParameterPin->MakeLinkTo(InputPin);
		PlaceNode.Signature.Inputs.Add(FNiagaraVariable(Type, PinName));
	}

	bool SetPlaceNodeHlsl(UNiagaraNodeCustomHlsl& PlaceNode, const FString& Hlsl)
	{
		FStrProperty* HlslProperty = FindFProperty<FStrProperty>(PlaceNode.GetClass(), TEXT("CustomHlsl"));
		if (HlslProperty == nullptr)
		{
			return false;
		}
		HlslProperty->SetPropertyValue_InContainer(&PlaceNode, Hlsl);
		// Tells the node its code changed, which marks the script for a recompile.
		FPropertyChangedEvent HlslChangedEvent(HlslProperty);
		PlaceNode.PostEditChangeProperty(HlslChangedEvent);
		return true;
	}

	void SetPinDefaultWhenPresent(UEdGraphNode& Node, const TCHAR* PinName, const TCHAR* DefaultValue)
	{
		UEdGraphPin* Pin = Node.FindPin(PinName, EGPD_Input);
		if (Pin != nullptr)
		{
			Pin->DefaultValue = DefaultValue;
		}
	}

	// The emitters must stay alive for the whole game: they loop forever and are never culled. The system sits
	// at the world origin, so culling by its distance or visibility would switch off impacts all over the map.
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

	bool WirePlaceNode(UEdGraph& Graph, UNiagaraNodeCustomHlsl& PlaceNode, const FString& ImpactTypeName)
	{
		RemoveEarlierWiring(Graph, PlaceNode);
		UEdGraphPin* InputMapPin = nullptr;
		for (UEdGraphPin* Pin : PlaceNode.Pins)
		{
			if (Pin->Direction == EGPD_Input && GetIsParameterMapPin(*Pin))
			{
				InputMapPin = Pin;
				break;
			}
		}
		UEdGraphNode* InputsNode = CreateInputsNode(Graph, PlaceNode);
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
		PlaceNode.Signature.Inputs.SetNum(1);
		const FNiagaraTypeDefinition IntType = FNiagaraTypeDefinition::GetIntDef();
		AddPlaceNodeInput(PlaceNode, *InputsNode, PositionAndScalePinName, TEXT("User.ImpactPositionAndScale"), GetFloat4ArrayType());
		AddPlaceNodeInput(PlaceNode, *InputsNode, NormalPinName, TEXT("User.ImpactNormal"), GetFloat4ArrayType());
		AddPlaceNodeInput(PlaceNode, *InputsNode, TypeOffsetPinName, TEXT("User.") + ImpactTypeName + TEXT("Offset"), IntType);
		AddPlaceNodeInput(PlaceNode, *InputsNode, TypeCountPinName, TEXT("User.") + ImpactTypeName + TEXT("Count"), IntType);
		AddPlaceNodeInput(PlaceNode, *InputsNode, ExecCountPinName, TEXT("Engine.ExecutionCount"), IntType);

		return SetPlaceNodeHlsl(PlaceNode, BuildPlaceHlsl(GetSpawnScriptSetsVelocity(*InputMapPin)));
	}

	bool PatchEmitter(UNiagaraSystem& System, const FNiagaraEmitterHandle& EmitterHandle)
	{
		const FString EmitterName = EmitterHandle.GetName().ToString();
		FString ImpactTypeName;
		FString LayerName;
		const bool bHasTypedName = EmitterName.Split(TEXT("_"), &ImpactTypeName, &LayerName)
			&& Algo::Find(ImpactTypeNames, ImpactTypeName) != nullptr;
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
		UNiagaraScriptSource* ScriptSource = EmitterData != nullptr
			? Cast<UNiagaraScriptSource>(EmitterData->GraphSource)
			: nullptr;
		UEdGraph* Graph = ScriptSource != nullptr ? ScriptSource->NodeGraph : nullptr;
		UNiagaraNodeCustomHlsl* PlaceNode = Graph != nullptr ? FindPlaceNode(*Graph) : nullptr;
		if (not bHasTypedName || PlaceNode == nullptr)
		{
			UE_LOG(LogRTSTraceImpactAuthoring, Warning,
			       TEXT("RTS_TRACE_IMPACT_AUTHORING skipped emitter=%s typed_name=%d place_node=%d"),
			       *EmitterName, bHasTypedName ? 1 : 0, PlaceNode != nullptr ? 1 : 0);
			return false;
		}
		Graph->Modify();
		PlaceNode->Modify();
		// The system sits at the world origin and the impacts come in as world locations.
		EmitterData->bLocalSpace = false;
		// A handful of particles per impact: cheaper on the CPU than a GPU dispatch per emitter, and the
		// impact arrays are then read in the frame they were written.
		EmitterData->SimTarget = ENiagaraSimTarget::CPUSim;
		MakeEmitterLoopForever(*Graph);
		EnsureUserIntParameter(System, EmitterName + TEXT("_SpawnCount"), 0);
		EnsureUserIntParameter(System, EmitterName + TEXT("_PerImpact"), GetDefaultParticlesPerImpact(EmitterName));
		const bool bIsWired = WirePlaceNode(*Graph, *PlaceNode, ImpactTypeName);
		Graph->NotifyGraphChanged();
		UE_LOG(LogRTSTraceImpactAuthoring, Display, TEXT("RTS_TRACE_IMPACT_AUTHORING emitter=%s type=%s wired=%d"),
		       *EmitterName, *ImpactTypeName, bIsWired ? 1 : 0);
		return bIsWired;
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
				UE_LOG(LogRTSTraceImpactAuthoring, Display,
				       TEXT("RTS_TRACE_IMPACT_AUTHORING compiled emitter=%s script=%s status=%d errors=%s"),
				       *EmitterHandle.GetName().ToString(),
				       *Script->GetName(),
				       static_cast<int32>(ExecutableData.LastCompileStatus),
				       *ExecutableData.ErrorMsg);
			}
		}
	}

	void PatchTraceImpactNiagaraSystem()
	{
		const URTSWeaponVFXSettings* VfxSettings = URTSWeaponVFXSettings::Get();
		UNiagaraSystem* System = VfxSettings != nullptr ? VfxSettings->TraceImpactNiagaraSystem.LoadSynchronous() : nullptr;
		if (System == nullptr)
		{
			UE_LOG(LogRTSTraceImpactAuthoring, Error, TEXT("RTS_TRACE_IMPACT_AUTHORING RESULT FAIL system not found"));
			return;
		}
		System->Modify();
		EnsureUserParameter(*System, FNiagaraVariable(GetFloat4ArrayType(), TEXT("User.ImpactPositionAndScale")));
		EnsureUserParameter(*System, FNiagaraVariable(GetFloat4ArrayType(), TEXT("User.ImpactNormal")));
		for (const TCHAR* ImpactTypeName : ImpactTypeNames)
		{
			EnsureUserIntParameter(*System, FString(ImpactTypeName) + TEXT("Offset"), 0);
			EnsureUserIntParameter(*System, FString(ImpactTypeName) + TEXT("Count"), 0);
		}
		RemoveStaleEmitterParameters(*System);
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
		UE_LOG(LogRTSTraceImpactAuthoring, Display,
		       TEXT("RTS_TRACE_IMPACT_AUTHORING RESULT DONE patched_emitters=%d of %d saved=%d"),
		       PatchedEmitterCount, System->GetEmitterHandles().Num(), bIsSaved ? 1 : 0);
	}

	FAutoConsoleCommand PatchTraceImpactNiagaraSystemCommand(
		TEXT("RTS.TraceImpactVfx.PatchNiagaraSystem"),
		TEXT("Wires every <ImpactType>_<Layer> emitter of the trace impact Niagara system to the impact arrays and saves it."),
		FConsoleCommandDelegate::CreateStatic(&PatchTraceImpactNiagaraSystem));
}

#endif // WITH_EDITOR
