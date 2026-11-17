// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only authoring step for the aircraft strafe impact Niagara system (see UAircraftStrafeImpactBatcher).
// RTS.StrafeImpactVfx.BuildNiagaraSystem adds every layer emitter (Ground_<Layer> / Unit_<Layer>) from the Minimal
// template with SpawnPerFrame bound to User.<Emitter>_SpawnCount and its sprite renderer, then patches each into a CPU
// emitter in world space: it declares the particle attributes, inserts the custom HLSL nodes StrafeSpawn (end of
// particle spawn) and StrafeUpdate (end of particle update), wires them to the impact arrays and writes their code
// from StrafeImpactHlsl/*.hlsl. RTS.StrafeImpactVfx.PatchNiagaraSystem only rewrites the HLSL (after editing it).
// Same approach as SmallArmsMuzzleNiagaraAuthoring; see Docs/Units/Aircraft/AircraftStrafeGun.md.

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
#include "NiagaraEmitter.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraSpriteRendererProperties.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "RTS_Survival/Weapons/RTSWeaponVFXSettings/RTSWeaponVFXSettings.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSStrafeImpactAuthoring, Log, All);

namespace AircraftStrafeImpactNiagaraAuthoring
{
	const TCHAR* SpawnNodeName = TEXT("StrafeSpawn");
	const TCHAR* UpdateNodeName = TEXT("StrafeUpdate");
	// Marks the parameter map get nodes this step creates, so a second run replaces them instead of adding more.
	const TCHAR* InputsNodeComment = TEXT("StrafeInputs");
	// EmitterState's "Loop Behavior" set to Infinite and "Scalability Mode" set to System, as for the trace impacts.
	const TCHAR* InfiniteLoopEnumValue = TEXT("NewEnumerator0");
	const TCHAR* SystemScalabilityEnumValue = TEXT("NewEnumerator0");
	// Particles live in world space around the component at the origin; this box covers any map.
	constexpr double FixedBoundsHalfExtent = 2000000.0;
	constexpr int32 InputsNodeOffsetX = 350;
	constexpr int32 InputsNodeOffsetY = 250;
	constexpr int32 HlslNodeOffsetX = 250;

	const FName OriginAttribute = TEXT("Particles.StrafeOrigin");
	const FName AxisAttribute = TEXT("Particles.StrafeAxis");
	const FName ShapeAttribute = TEXT("Particles.StrafeShape");
	// Up axis of the custom-aligned spurt sprites (the column stays upright from any camera).
	const FName SpriteAlignmentAttribute = TEXT("Particles.SpriteAlignment");
	// Which of the 2x2 puffs of the smoke atlas the sprite shows (sprite renderer Sub Image Size 2x2).
	const FName SubImageAttribute = TEXT("Particles.SubImageIndex");

	const TCHAR* HlslFolder = TEXT("Source/RTS_Survival/Weapons/AircraftWeapon/AircraftStrafeGun/StrafeImpactHlsl");
	const TCHAR* SectionMarker = TEXT("// @section ");
	const TCHAR* GeneratedHeader = TEXT("// Written by RTS.StrafeImpactVfx.PatchNiagaraSystem from StrafeImpactHlsl/*.hlsl; edits here are overwritten.\n");

	enum class EStrafeSpriteFacing : uint8
	{
		// Faces the camera fully.
		Camera,
		// Turns only about its up axis (Particles.SpriteAlignment) towards the camera: an upright column.
		Upright
	};

	struct FStrafeLayer
	{
		// Emitter name in the system: <Group>_<Layer>.
		const TCHAR* EmitterName;
		// User parameter prefix of the group: <Group>Offset / <Group>Count.
		const TCHAR* GroupName;
		// HLSL file with the Shape and Place sections of this layer.
		const TCHAR* HlslFile;
		// Particles one impact of the group spawns on this layer.
		int32 ParticlesPerImpact;
		const TCHAR* MaterialPath;
		// Sub image grid of the sprite material (1 = no atlas).
		int32 SubImagesPerSide;
		EStrafeSpriteFacing Facing;
	};

	// Lit smoke on the 2x2 puff atlas (the muzzle smoke material): dirt spurts, clods and dust take sun and shadow.
	const TCHAR* DirtMaterialPath = TEXT("/Game/RTS_Survival/ImpactSfxVfx/MuzzleVfx/M_SmallArmsMuzzleSmoke.M_SmallArmsMuzzleSmoke");
	const TCHAR* SparkMaterialPath = TEXT("/Game/RTS_Survival/ImpactSfxVfx/MuzzleVfx/M_SmallArmsMuzzleGlow.M_SmallArmsMuzzleGlow");

	const FStrafeLayer Layers[] = {
		{TEXT("Ground_Spurt"), TEXT("Ground"), TEXT("Spurt.hlsl"), 3, DirtMaterialPath, 2, EStrafeSpriteFacing::Upright},
		{TEXT("Ground_Debris"), TEXT("Ground"), TEXT("Debris.hlsl"), 4, DirtMaterialPath, 2, EStrafeSpriteFacing::Camera},
		{TEXT("Ground_Dust"), TEXT("Ground"), TEXT("Dust.hlsl"), 1, DirtMaterialPath, 2, EStrafeSpriteFacing::Camera},
		{TEXT("Ground_Flash"), TEXT("Ground"), TEXT("Flash.hlsl"), 1, SparkMaterialPath, 1, EStrafeSpriteFacing::Camera},
		{TEXT("Unit_Spark"), TEXT("Unit"), TEXT("Spark.hlsl"), 3, SparkMaterialPath, 1, EStrafeSpriteFacing::Camera}
	};

	const FStrafeLayer* FindLayer(const FString& EmitterName)
	{
		for (const FStrafeLayer& Layer : Layers)
		{
			if (EmitterName == Layer.EmitterName)
			{
				return &Layer;
			}
		}
		return nullptr;
	}

	/**
	 * @brief Reads one .hlsl file of StrafeImpactHlsl and splits it at its section markers.
	 * @param FileName File name inside the HLSL folder, for example Spurt.hlsl.
	 * @param OutSections Section name to the code below its marker, up to the next marker.
	 * @return False when the file could not be read.
	 */
	bool ReadHlslSections(const FString& FileName, TMap<FString, FString>& OutSections)
	{
		const FString FilePath = FPaths::Combine(FPaths::ProjectDir(), HlslFolder, FileName);
		FString FileText;
		if (not FFileHelper::LoadFileToString(FileText, *FilePath))
		{
			UE_LOG(LogRTSStrafeImpactAuthoring, Error, TEXT("RTS_STRAFE_IMPACT_AUTHORING cannot read %s"), *FilePath);
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

	struct FStrafeHlsl
	{
		FString Spawn;
		FString Update;
	};

	bool BuildLayerHlsl(const FStrafeLayer& Layer, FStrafeHlsl& OutHlsl)
	{
		TMap<FString, FString> Common;
		TMap<FString, FString> LayerSections;
		if (not ReadHlslSections(TEXT("Common.hlsl"), Common) || not ReadHlslSections(Layer.HlslFile, LayerSections))
		{
			return false;
		}
		for (const TCHAR* Section : {TEXT("SpawnBegin"), TEXT("SpawnEnd"), TEXT("UpdateBegin"), TEXT("ReadImpact")})
		{
			if (not Common.Contains(Section))
			{
				UE_LOG(LogRTSStrafeImpactAuthoring, Error, TEXT("RTS_STRAFE_IMPACT_AUTHORING Common.hlsl has no section %s"), Section);
				return false;
			}
		}
		if (not LayerSections.Contains(TEXT("Shape")) || not LayerSections.Contains(TEXT("Place")))
		{
			UE_LOG(LogRTSStrafeImpactAuthoring, Error, TEXT("RTS_STRAFE_IMPACT_AUTHORING %s needs sections Shape and Place"), Layer.HlslFile);
			return false;
		}
		OutHlsl.Spawn = GeneratedHeader + Common[TEXT("SpawnBegin")] + LayerSections[TEXT("Shape")]
			+ Common[TEXT("SpawnEnd")] + Common[TEXT("ReadImpact")] + LayerSections[TEXT("Place")];
		OutHlsl.Update = GeneratedHeader + Common[TEXT("UpdateBegin")] + Common[TEXT("ReadImpact")]
			+ LayerSections[TEXT("Place")];
		return true;
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

	// bOverwrite: also sets the value of an existing parameter (the per-shot counts follow the Layers table).
	void SetUserIntParameter(UNiagaraSystem& System, const FString& Name, const int32 Value, const bool bOverwrite)
	{
		FNiagaraUserRedirectionParameterStore& UserParameters = System.GetExposedParameters();
		const FNiagaraVariable Parameter(FNiagaraTypeDefinition::GetIntDef(), *(TEXT("User.") + Name));
		if (UserParameters.FindParameterOffset(Parameter) == nullptr || bOverwrite)
		{
			UserParameters.SetParameterValue<int32>(Value, Parameter, true);
		}
	}

	void EnsureSystemUserParameters(UNiagaraSystem& System)
	{
		EnsureUserParameter(System, FNiagaraVariable(GetFloat4ArrayType(), TEXT("User.StrafePositionAndCalibre")));
		EnsureUserParameter(System, FNiagaraVariable(GetFloat4ArrayType(), TEXT("User.StrafeNormalAndSurface")));
		for (const TCHAR* GroupName : {TEXT("Ground"), TEXT("Unit")})
		{
			SetUserIntParameter(System, FString(GroupName) + TEXT("Offset"), 0, false);
			SetUserIntParameter(System, FString(GroupName) + TEXT("Count"), 0, false);
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
	 * @param ParameterName Full name of the parameter to read, for example User.FlashCount.
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

	/**
	 * @brief Gives the HLSL node its data inputs in the order the code expects and writes the code.
	 * @param Graph The emitter graph.
	 * @param HlslNode The StrafeSpawn or StrafeUpdate node.
	 * @param Layer The layer of the emitter; its group names the offset and count parameters.
	 * @param Hlsl The code to write into the node.
	 * @return False when the node could not be wired.
	 */
	bool WireHlslNode(UEdGraph& Graph, UNiagaraNodeCustomHlsl& HlslNode, const FStrafeLayer& Layer, const FString& Hlsl)
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
		const FNiagaraTypeDefinition IntType = FNiagaraTypeDefinition::GetIntDef();
		const FString GroupName = Layer.GroupName;
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("StrafePositionAndCalibre"), TEXT("User.StrafePositionAndCalibre"), GetFloat4ArrayType());
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("StrafeNormalAndSurface"), TEXT("User.StrafeNormalAndSurface"), GetFloat4ArrayType());
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("TypeOffset"), TEXT("User.") + GroupName + TEXT("Offset"), IntType);
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("TypeCount"), TEXT("User.") + GroupName + TEXT("Count"), IntType);
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("ExecCount"), TEXT("Engine.ExecutionCount"), IntType);
		// Particle attributes the update reads but never writes must come in through a parameter map read: a token
		// read inside custom HLSL alone does not make the script load the attribute, so it would read zero.
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("StrafeOriginIn"), OriginAttribute.ToString(), FNiagaraTypeDefinition::GetVec3Def());
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("StrafeAxisIn"), AxisAttribute.ToString(), FNiagaraTypeDefinition::GetVec4Def());
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("StrafeShapeIn"), ShapeAttribute.ToString(), FNiagaraTypeDefinition::GetVec4Def());
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

	// The emitters must stay alive for the whole game: they loop forever and are never culled. The system sits at
	// the world origin, so culling by its distance or visibility would switch off impacts all over the map.
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
		if (GetHasAttributeAssignment(Graph, OriginAttribute))
		{
			return;
		}
		const TArray<FNiagaraVariable> Attributes = {
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), OriginAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), AxisAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), ShapeAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), SubImageAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), SpriteAlignmentAttribute)
		};
		const TArray<FString> DefaultValues = {TEXT("0,0,0"), TEXT("0,0,1,1"), TEXT("0,0,0,0"), TEXT("0"), TEXT("0,0,1")};
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
		// A handful of particles per impact: cheaper on the CPU than a GPU dispatch per emitter, and the impact arrays
		// are read in the frame they were written (the batcher clears them on the next frame).
		EmitterData.SimTarget = ENiagaraSimTarget::CPUSim;
		EmitterData.bDeterminism = false;
		EmitterData.CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
		EmitterData.FixedBounds = FBox(FVector(-FixedBoundsHalfExtent), FVector(FixedBoundsHalfExtent));
		// Attribute trimming does not see the custom HLSL read these, and would drop them from the particle data.
		for (const FName& Attribute : {OriginAttribute, AxisAttribute, ShapeAttribute, SubImageAttribute, SpriteAlignmentAttribute})
		{
			EmitterData.AttributesToPreserve.AddUnique(Attribute.ToString());
		}
	}

	bool PatchEmitter(UNiagaraSystem& System, const FNiagaraEmitterHandle& EmitterHandle)
	{
		const FString EmitterName = EmitterHandle.GetName().ToString();
		const FStrafeLayer* Layer = FindLayer(EmitterName);
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
		UNiagaraScriptSource* ScriptSource = EmitterData != nullptr ? Cast<UNiagaraScriptSource>(EmitterData->GraphSource) : nullptr;
		UNiagaraGraph* Graph = ScriptSource != nullptr ? ScriptSource->NodeGraph : nullptr;
		UNiagaraNodeOutput* SpawnOutputNode = Graph != nullptr
			? Graph->FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleSpawnScript)
			: nullptr;
		if (Layer == nullptr || SpawnOutputNode == nullptr)
		{
			UE_LOG(LogRTSStrafeImpactAuthoring, Warning, TEXT("RTS_STRAFE_IMPACT_AUTHORING skipped emitter=%s layer=%d graph=%d"),
			       *EmitterName, Layer != nullptr ? 1 : 0, SpawnOutputNode != nullptr ? 1 : 0);
			return false;
		}
		Graph->Modify();
		SetEmitterProperties(*EmitterData);
		MakeEmitterLoopForever(*Graph);
		SetUserIntParameter(System, EmitterName + TEXT("_SpawnCount"), 0, false);
		SetUserIntParameter(System, EmitterName + TEXT("_PerImpact"), Layer->ParticlesPerImpact, true);
		EnsureAttributesDeclared(*Graph, *SpawnOutputNode);

		UNiagaraNodeCustomHlsl* SpawnNode = FindOrInsertHlslNode(*Graph, ENiagaraScriptUsage::ParticleSpawnScript, SpawnNodeName);
		UNiagaraNodeCustomHlsl* UpdateNode = FindOrInsertHlslNode(*Graph, ENiagaraScriptUsage::ParticleUpdateScript, UpdateNodeName);
		FStrafeHlsl Hlsl;
		const bool bHasHlsl = BuildLayerHlsl(*Layer, Hlsl);
		const bool bIsSpawnWired = bHasHlsl && SpawnNode != nullptr && WireHlslNode(*Graph, *SpawnNode, *Layer, Hlsl.Spawn);
		const bool bIsUpdateWired = bHasHlsl && UpdateNode != nullptr && WireHlslNode(*Graph, *UpdateNode, *Layer, Hlsl.Update);
		Graph->NotifyGraphChanged();
		UE_LOG(LogRTSStrafeImpactAuthoring, Display, TEXT("RTS_STRAFE_IMPACT_AUTHORING emitter=%s group=%s spawn=%d update=%d"),
		       *EmitterName, Layer->GroupName, bIsSpawnWired ? 1 : 0, bIsUpdateWired ? 1 : 0);
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
				// The translated code, for checking what the custom HLSL turned into: Saved/StrafeImpactVfx/Hlsl/.
				const FString DumpPath = FPaths::ProjectSavedDir() / TEXT("StrafeImpactVfx/Hlsl")
					/ (EmitterHandle.GetName().ToString() + TEXT("_") + Script->GetName() + TEXT(".hlsl"));
				FFileHelper::SaveStringToFile(ExecutableData.LastHlslTranslation, *DumpPath);
				UE_LOG(LogRTSStrafeImpactAuthoring, Display,
				       TEXT("RTS_STRAFE_IMPACT_AUTHORING compiled emitter=%s script=%s status=%d errors=%s"),
				       *EmitterHandle.GetName().ToString(), *Script->GetName(),
				       static_cast<int32>(ExecutableData.LastCompileStatus), *ExecutableData.ErrorMsg);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------
	// Building the emitters (RTS.StrafeImpactVfx.BuildNiagaraSystem): adds every layer that is missing from the Minimal
	// emitter template, spawns it per frame from User.<Emitter>_SpawnCount and gives it its renderer.
	// ------------------------------------------------------------------------------------------------------------

	const TCHAR* MinimalEmitterTemplatePath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal");
	const TCHAR* SpawnPerFrameModulePath = TEXT("/Niagara/Modules/Emitter/SpawnPerFrame.SpawnPerFrame");

	const FNiagaraEmitterHandle* FindEmitterHandle(const UNiagaraSystem& System, const FString& EmitterName)
	{
		for (const FNiagaraEmitterHandle& EmitterHandle : System.GetEmitterHandles())
		{
			if (EmitterHandle.GetName().ToString() == EmitterName)
			{
				return &EmitterHandle;
			}
		}
		return nullptr;
	}

	// The template spawns by rate or burst; the batcher decides the count per frame, so those modules are switched off.
	void DisableTemplateSpawnModules(UEdGraph& Graph)
	{
		for (UEdGraphNode* Node : Graph.Nodes)
		{
			UNiagaraNodeFunctionCall* ModuleNode = Cast<UNiagaraNodeFunctionCall>(Node);
			if (ModuleNode == nullptr)
			{
				continue;
			}
			const FString ModuleName = ModuleNode->GetFunctionName();
			const bool bIsTemplateSpawn = ModuleName.StartsWith(TEXT("SpawnRate")) || ModuleName.StartsWith(TEXT("SpawnBurst"));
			if (bIsTemplateSpawn)
			{
				FNiagaraStackGraphUtilities::SetModuleIsEnabled(*ModuleNode, false);
			}
		}
	}

	bool AddSpawnPerFrameBoundToUser(UNiagaraNodeOutput& EmitterUpdateOutputNode, const FString& EmitterName)
	{
		UNiagaraScript* SpawnPerFrameScript = LoadObject<UNiagaraScript>(nullptr, SpawnPerFrameModulePath);
		if (SpawnPerFrameScript == nullptr)
		{
			return false;
		}
		UNiagaraNodeFunctionCall* SpawnNode = FNiagaraStackGraphUtilities::AddScriptModuleToStack(
			SpawnPerFrameScript, EmitterUpdateOutputNode);
		if (SpawnNode == nullptr)
		{
			return false;
		}
		const FNiagaraParameterHandle SpawnCountInput = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(
			FNiagaraParameterHandle(TEXT("Module.Spawn Count")), SpawnNode);
		UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
			*SpawnNode, SpawnCountInput, FNiagaraTypeDefinition::GetIntDef(), FGuid(), FGuid());
		const FNiagaraParameterHandle UserSpawnCount(FName(*(TEXT("User.") + EmitterName + TEXT("_SpawnCount"))));
		FNiagaraStackGraphUtilities::SetLinkedValueHandleForFunctionInput(OverridePin, UserSpawnCount, TSet<FNiagaraVariable>());
		return true;
	}

	UNiagaraRendererProperties* CreateLayerRenderer(UNiagaraEmitter& Emitter, const FStrafeLayer& Layer)
	{
		UNiagaraSpriteRendererProperties* SpriteRenderer = NewObject<UNiagaraSpriteRendererProperties>(
			&Emitter, NAME_None, RF_Transactional);
		SpriteRenderer->Material = LoadObject<UMaterialInterface>(nullptr, Layer.MaterialPath);
		SpriteRenderer->SubImageSize = FVector2D(Layer.SubImagesPerSide, Layer.SubImagesPerSide);
		// Always explicit: the defaults are Automatic, which use every declared facing / alignment attribute; with
		// both SpriteFacing and SpriteAlignment declared as up, every quad collapsed to nothing.
		SpriteRenderer->FacingMode = ENiagaraSpriteFacingMode::FaceCamera;
		SpriteRenderer->Alignment = Layer.Facing == EStrafeSpriteFacing::Upright
			                            ? ENiagaraSpriteAlignment::CustomAlignment
			                            : ENiagaraSpriteAlignment::Unaligned;
		return SpriteRenderer;
	}

	void ReplaceRenderers(const FNiagaraEmitterHandle& EmitterHandle, const FStrafeLayer& Layer)
	{
		const FVersionedNiagaraEmitter VersionedEmitter = EmitterHandle.GetInstance();
		UNiagaraEmitter* Emitter = VersionedEmitter.Emitter;
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
		if (Emitter == nullptr || EmitterData == nullptr)
		{
			return;
		}
		const TArray<UNiagaraRendererProperties*> OldRenderers = EmitterData->GetRenderers();
		for (UNiagaraRendererProperties* OldRenderer : OldRenderers)
		{
			Emitter->RemoveRenderer(OldRenderer, VersionedEmitter.Version);
		}
		Emitter->AddRenderer(CreateLayerRenderer(*Emitter, Layer), VersionedEmitter.Version);
	}

	bool AddLayerEmitter(UNiagaraSystem& System, const FStrafeLayer& Layer)
	{
		UNiagaraEmitter* Template = LoadObject<UNiagaraEmitter>(nullptr, MinimalEmitterTemplatePath);
		if (Template == nullptr)
		{
			UE_LOG(LogRTSStrafeImpactAuthoring, Error, TEXT("RTS_STRAFE_IMPACT_AUTHORING no emitter template %s"), MinimalEmitterTemplatePath);
			return false;
		}
		const FGuid HandleId = FNiagaraEditorUtilities::AddEmitterToSystem(System, *Template, Template->GetExposedVersion().VersionGuid);
		FNiagaraEmitterHandle* NewHandle = System.GetEmitterHandles().FindByPredicate(
			[&HandleId](const FNiagaraEmitterHandle& EmitterHandle) { return EmitterHandle.GetId() == HandleId; });
		if (NewHandle == nullptr)
		{
			return false;
		}
		NewHandle->SetName(Layer.EmitterName, System);
		SetUserIntParameter(System, FString(Layer.EmitterName) + TEXT("_SpawnCount"), 0, false);

		FVersionedNiagaraEmitterData* EmitterData = NewHandle->GetEmitterData();
		UNiagaraScriptSource* ScriptSource = EmitterData != nullptr ? Cast<UNiagaraScriptSource>(EmitterData->GraphSource) : nullptr;
		UNiagaraGraph* Graph = ScriptSource != nullptr ? ScriptSource->NodeGraph : nullptr;
		UNiagaraNodeOutput* EmitterUpdateOutput = Graph != nullptr
			? Graph->FindEquivalentOutputNode(ENiagaraScriptUsage::EmitterUpdateScript)
			: nullptr;
		if (EmitterUpdateOutput == nullptr)
		{
			return false;
		}
		Graph->Modify();
		DisableTemplateSpawnModules(*Graph);
		const bool bHasSpawn = AddSpawnPerFrameBoundToUser(*EmitterUpdateOutput, Layer.EmitterName);
		ReplaceRenderers(*NewHandle, Layer);
		Graph->NotifyGraphChanged();
		UE_LOG(LogRTSStrafeImpactAuthoring, Display, TEXT("RTS_STRAFE_IMPACT_AUTHORING added emitter=%s spawn_per_frame=%d"),
		       Layer.EmitterName, bHasSpawn ? 1 : 0);
		return bHasSpawn;
	}

	// An emitter that is no longer in the Layers table is only reported: removing an emitter handle by hand leaves the
	// system graph's emitter nodes stale (the system then runs without emitters). Rebuild the system instead:
	// strafe_impact_vfx_job.py rebuild deletes it and builds it again from the table.
	void ReportEmittersNotInLayers(const UNiagaraSystem& System)
	{
		for (const FNiagaraEmitterHandle& EmitterHandle : System.GetEmitterHandles())
		{
			if (FindLayer(EmitterHandle.GetName().ToString()) == nullptr)
			{
				UE_LOG(LogRTSStrafeImpactAuthoring, Warning,
				       TEXT("RTS_STRAFE_IMPACT_AUTHORING emitter=%s is not in the layer table; rebuild the system"),
				       *EmitterHandle.GetName().ToString());
			}
		}
	}

	// Adds the layers the system does not have yet and gives the others the renderer of the Layers table again.
	void AddMissingLayerEmitters(UNiagaraSystem& System)
	{
		for (const FStrafeLayer& Layer : Layers)
		{
			const FNiagaraEmitterHandle* ExistingHandle = FindEmitterHandle(System, Layer.EmitterName);
			if (ExistingHandle == nullptr)
			{
				AddLayerEmitter(System, Layer);
				continue;
			}
			ReplaceRenderers(*ExistingHandle, Layer);
		}
	}

	void PatchStrafeImpactNiagaraSystem(const bool bAddMissingEmitters);

	void BuildStrafeImpactNiagaraSystem()
	{
		PatchStrafeImpactNiagaraSystem(true);
	}

	void PatchStrafeImpactNiagaraSystemOnly()
	{
		PatchStrafeImpactNiagaraSystem(false);
	}

	void PatchStrafeImpactNiagaraSystem(const bool bAddMissingEmitters)
	{
		const URTSWeaponVFXSettings* Settings = URTSWeaponVFXSettings::Get();
		UNiagaraSystem* System = Settings != nullptr ? Settings->AircraftStrafeImpactNiagaraSystem.LoadSynchronous() : nullptr;
		if (System == nullptr)
		{
			UE_LOG(LogRTSStrafeImpactAuthoring, Error, TEXT("RTS_STRAFE_IMPACT_AUTHORING RESULT FAIL system not found"));
			return;
		}
		System->Modify();
		EnsureSystemUserParameters(*System);
		if (bAddMissingEmitters)
		{
			ReportEmittersNotInLayers(*System);
			AddMissingLayerEmitters(*System);
		}
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
		UE_LOG(LogRTSStrafeImpactAuthoring, Display, TEXT("RTS_STRAFE_IMPACT_AUTHORING RESULT DONE patched_emitters=%d of %d saved=%d"),
		       PatchedEmitterCount, System->GetEmitterHandles().Num(), bIsSaved ? 1 : 0);
	}

	FAutoConsoleCommand PatchStrafeImpactNiagaraSystemCommand(
		TEXT("RTS.StrafeImpactVfx.PatchNiagaraSystem"),
		TEXT("Rewrites the custom HLSL of every Ground_/Unit_ emitter of the aircraft strafe impact Niagara system from StrafeImpactHlsl and saves it."),
		FConsoleCommandDelegate::CreateStatic(&PatchStrafeImpactNiagaraSystemOnly));

	FAutoConsoleCommand BuildStrafeImpactNiagaraSystemCommand(
		TEXT("RTS.StrafeImpactVfx.BuildNiagaraSystem"),
		TEXT("Adds every missing Ground_/Unit_ layer emitter (Minimal template, SpawnPerFrame bound to its user spawn count, sprite renderer), then patches and saves the aircraft strafe impact Niagara system."),
		FConsoleCommandDelegate::CreateStatic(&BuildStrafeImpactNiagaraSystem));
}

#endif // WITH_EDITOR
