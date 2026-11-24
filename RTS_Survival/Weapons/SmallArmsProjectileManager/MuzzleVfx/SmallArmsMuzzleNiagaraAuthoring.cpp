// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only authoring step for the small arms muzzle Niagara system (see USmallArmsMuzzleVfxBatcher).
// The console command RTS.MuzzleVfx.PatchNiagaraSystem turns every emitter named Flash_<Layer> / Smoke_<Layer> into a
// CPU emitter in world space that draws one layer of every shot of its group: it declares the particle attributes,
// inserts the custom HLSL nodes MuzzleSpawn (end of particle spawn) and MuzzleUpdate (end of particle update), wires
// them to the shot arrays and writes their code from MuzzleHlsl/*.hlsl. It also adds the user parameters the batcher
// writes (<Emitter>_SpawnCount) and reads (<Emitter>_PerShot). An emitter needs a SpawnPerFrame module whose Spawn
// Count is bound to User.<Emitter>_SpawnCount, and a sprite or mesh renderer. Run the command again after changing the
// .hlsl files; see Docs/VFX/SmallArmsMuzzleVfx.md.

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
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraMeshRendererProperties.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogRTSMuzzleVfxAuthoring, Log, All);

namespace SmallArmsMuzzleNiagaraAuthoring
{
	const TCHAR* SpawnNodeName = TEXT("MuzzleSpawn");
	const TCHAR* UpdateNodeName = TEXT("MuzzleUpdate");
	// Marks the parameter map get nodes this step creates, so a second run replaces them instead of adding more.
	const TCHAR* InputsNodeComment = TEXT("MuzzleInputs");
	// EmitterState's "Loop Behavior" set to Infinite and "Scalability Mode" set to System, as for the trace impacts.
	const TCHAR* InfiniteLoopEnumValue = TEXT("NewEnumerator0");
	const TCHAR* SystemScalabilityEnumValue = TEXT("NewEnumerator0");
	// Particles live in world space around the component at the origin; this box covers any map.
	constexpr double FixedBoundsHalfExtent = 2000000.0;
	constexpr int32 InputsNodeOffsetX = 350;
	constexpr int32 InputsNodeOffsetY = 250;
	constexpr int32 HlslNodeOffsetX = 250;

	const FName OriginAttribute = TEXT("Particles.MuzzleOrigin");
	const FName AxisAttribute = TEXT("Particles.MuzzleAxis");
	const FName ShapeAttribute = TEXT("Particles.MuzzleShape");
	// Which of the 2x2 puffs of the smoke atlas the sprite shows (sprite renderer Sub Image Size 2x2).
	const FName SubImageAttribute = TEXT("Particles.SubImageIndex");
	const FName MeshOrientationAttribute = TEXT("Particles.MeshOrientation");
	const FName ScaleAttribute = TEXT("Particles.Scale");

	const TCHAR* HlslFolder = TEXT("Source/RTS_Survival/Weapons/SmallArmsProjectileManager/MuzzleVfx/MuzzleHlsl");
	const TCHAR* SectionMarker = TEXT("// @section ");
	const TCHAR* GeneratedHeader = TEXT("// Written by the console command RTS.MuzzleVfx.PatchNiagaraSystem from MuzzleHlsl/*.hlsl; edits here are overwritten.\n");

	struct FMuzzleLayer
	{
		// Emitter name in the system: <Group>_<Layer>.
		const TCHAR* EmitterName;
		// User parameter prefix of the group: <Group>Offset / <Group>Count.
		const TCHAR* GroupName;
		// HLSL file with the Shape and Place sections of this layer.
		const TCHAR* HlslFile;
		// Particles one shot of the group spawns on this layer.
		int32 ParticlesPerShot;
		// Mesh of a mesh renderer; null for a camera-facing sprite renderer.
		const TCHAR* MeshPath;
		const TCHAR* MaterialPath;
		// Sub image grid of the sprite material (1 = no atlas).
		int32 SubImagesPerSide;
	};

	const TCHAR* FlashMaterialPath = TEXT("/Game/RTS_Survival/ImpactSfxVfx/MuzzleVfx/M_SmallArmsMuzzleFlash.M_SmallArmsMuzzleFlash");
	const TCHAR* GlowMaterialPath = TEXT("/Game/RTS_Survival/ImpactSfxVfx/MuzzleVfx/M_SmallArmsMuzzleGlow.M_SmallArmsMuzzleGlow");
	// Lit smoke on the 2x2 puff atlas of the dust devils; the cheapest lit translucency, so it takes sun and shadow.
	const TCHAR* SmokeMaterialPath = TEXT("/Game/RTS_Survival/ImpactSfxVfx/MuzzleVfx/M_SmallArmsMuzzleSmoke.M_SmallArmsMuzzleSmoke");

	const FMuzzleLayer Layers[] = {
		{
			TEXT("Flash_Cone"), TEXT("Flash"), TEXT("Cone.hlsl"), 1,
			TEXT("/Game/RTS_Survival/SpecialEffects/sA_ShootingVfxPack/Meshes/SM_Muzzle_1.SM_Muzzle_1"), FlashMaterialPath, 1
		},
		{
			TEXT("Flash_Star"), TEXT("Flash"), TEXT("Star.hlsl"), 1,
			TEXT("/Game/RTS_Survival/SpecialEffects/sA_ShootingVfxPack/Meshes/SM_Muzzle_Part2.SM_Muzzle_Part2"), FlashMaterialPath, 1
		},
		{TEXT("Flash_Glow"), TEXT("Flash"), TEXT("Glow.hlsl"), 1, nullptr, GlowMaterialPath, 1},
		{TEXT("Smoke_Puff"), TEXT("Smoke"), TEXT("Puff.hlsl"), 2, nullptr, SmokeMaterialPath, 2}
	};

	const FMuzzleLayer* FindLayer(const FString& EmitterName)
	{
		for (const FMuzzleLayer& Layer : Layers)
		{
			if (EmitterName == Layer.EmitterName)
			{
				return &Layer;
			}
		}
		return nullptr;
	}

	/**
	 * @brief Reads one .hlsl file of MuzzleHlsl and splits it at its section markers.
	 * @param FileName File name inside the HLSL folder, for example Puff.hlsl.
	 * @param OutSections Section name to the code below its marker, up to the next marker.
	 * @return False when the file could not be read.
	 */
	bool ReadHlslSections(const FString& FileName, TMap<FString, FString>& OutSections)
	{
		const FString FilePath = FPaths::Combine(FPaths::ProjectDir(), HlslFolder, FileName);
		FString FileText;
		if (not FFileHelper::LoadFileToString(FileText, *FilePath))
		{
			UE_LOG(LogRTSMuzzleVfxAuthoring, Error, TEXT("RTS_MUZZLE_VFX_AUTHORING cannot read %s"), *FilePath);
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

	struct FMuzzleHlsl
	{
		FString Spawn;
		FString Update;
	};

	bool BuildLayerHlsl(const FMuzzleLayer& Layer, FMuzzleHlsl& OutHlsl)
	{
		TMap<FString, FString> Common;
		TMap<FString, FString> LayerSections;
		if (not ReadHlslSections(TEXT("Common.hlsl"), Common) || not ReadHlslSections(Layer.HlslFile, LayerSections))
		{
			return false;
		}
		for (const TCHAR* Section : {TEXT("SpawnBegin"), TEXT("SpawnEnd"), TEXT("UpdateBegin"), TEXT("ReadShot")})
		{
			if (not Common.Contains(Section))
			{
				UE_LOG(LogRTSMuzzleVfxAuthoring, Error, TEXT("RTS_MUZZLE_VFX_AUTHORING Common.hlsl has no section %s"), Section);
				return false;
			}
		}
		if (not LayerSections.Contains(TEXT("Shape")) || not LayerSections.Contains(TEXT("Place")))
		{
			UE_LOG(LogRTSMuzzleVfxAuthoring, Error, TEXT("RTS_MUZZLE_VFX_AUTHORING %s needs sections Shape and Place"), Layer.HlslFile);
			return false;
		}
		OutHlsl.Spawn = GeneratedHeader + Common[TEXT("SpawnBegin")] + LayerSections[TEXT("Shape")]
			+ Common[TEXT("SpawnEnd")] + Common[TEXT("ReadShot")] + LayerSections[TEXT("Place")];
		OutHlsl.Update = GeneratedHeader + Common[TEXT("UpdateBegin")] + Common[TEXT("ReadShot")]
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
		EnsureUserParameter(System, FNiagaraVariable(GetFloat4ArrayType(), TEXT("User.ShotPositionAndScale")));
		EnsureUserParameter(System, FNiagaraVariable(GetFloat4ArrayType(), TEXT("User.ShotDirection")));
		for (const TCHAR* GroupName : {TEXT("Flash"), TEXT("Smoke")})
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
	 * @param HlslNode The MuzzleSpawn or MuzzleUpdate node.
	 * @param Layer The layer of the emitter; its group names the offset and count parameters.
	 * @param Hlsl The code to write into the node.
	 * @return False when the node could not be wired.
	 */
	bool WireHlslNode(UEdGraph& Graph, UNiagaraNodeCustomHlsl& HlslNode, const FMuzzleLayer& Layer, const FString& Hlsl)
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
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("ShotPositionAndScale"), TEXT("User.ShotPositionAndScale"), GetFloat4ArrayType());
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("ShotDirection"), TEXT("User.ShotDirection"), GetFloat4ArrayType());
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("TypeOffset"), TEXT("User.") + GroupName + TEXT("Offset"), IntType);
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("TypeCount"), TEXT("User.") + GroupName + TEXT("Count"), IntType);
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("ExecCount"), TEXT("Engine.ExecutionCount"), IntType);
		// Particle attributes the update reads but never writes must come in through a parameter map read: a token
		// read inside custom HLSL alone does not make the script load the attribute, so it would read zero.
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("MuzzleOriginIn"), OriginAttribute.ToString(), FNiagaraTypeDefinition::GetVec3Def());
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("MuzzleAxisIn"), AxisAttribute.ToString(), FNiagaraTypeDefinition::GetVec4Def());
		AddHlslNodeInput(HlslNode, *InputsNode, TEXT("MuzzleShapeIn"), ShapeAttribute.ToString(), FNiagaraTypeDefinition::GetVec4Def());
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
	// the world origin, so culling by its distance or visibility would switch off muzzles all over the map.
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

	bool GetHasMeshRenderer(const FVersionedNiagaraEmitterData& EmitterData)
	{
		for (const UNiagaraRendererProperties* Renderer : EmitterData.GetRenderers())
		{
			if (Cast<UNiagaraMeshRendererProperties>(Renderer) != nullptr)
			{
				return true;
			}
		}
		return false;
	}

	// The HLSL can only write attributes the script already knows, so a Set Parameters module declares them first.
	void EnsureAttributesDeclared(UEdGraph& Graph, UNiagaraNodeOutput& SpawnOutputNode, const bool bHasMeshRenderer)
	{
		if (GetHasAttributeAssignment(Graph, OriginAttribute))
		{
			return;
		}
		TArray<FNiagaraVariable> Attributes = {
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), OriginAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), AxisAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), ShapeAttribute),
			FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), SubImageAttribute)
		};
		TArray<FString> DefaultValues = {TEXT("0,0,0"), TEXT("1,0,0,1"), TEXT("0,0,0,0"), TEXT("0")};
		if (bHasMeshRenderer)
		{
			Attributes.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(), MeshOrientationAttribute));
			DefaultValues.Add(TEXT("0,0,0,1"));
			Attributes.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), ScaleAttribute));
			DefaultValues.Add(TEXT("1,1,1"));
		}
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
		// A handful of particles per shot: cheaper on the CPU than a GPU dispatch per emitter, and the shot arrays are
		// read in the frame they were written (the batcher clears them on the next frame).
		EmitterData.SimTarget = ENiagaraSimTarget::CPUSim;
		EmitterData.bDeterminism = false;
		EmitterData.CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
		EmitterData.FixedBounds = FBox(FVector(-FixedBoundsHalfExtent), FVector(FixedBoundsHalfExtent));
		// Attribute trimming does not see the custom HLSL read these, and would drop them from the particle data.
		for (const FName& Attribute : {OriginAttribute, AxisAttribute, ShapeAttribute, SubImageAttribute})
		{
			EmitterData.AttributesToPreserve.AddUnique(Attribute.ToString());
		}
	}

	bool PatchEmitter(UNiagaraSystem& System, const FNiagaraEmitterHandle& EmitterHandle)
	{
		const FString EmitterName = EmitterHandle.GetName().ToString();
		const FMuzzleLayer* Layer = FindLayer(EmitterName);
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
		UNiagaraScriptSource* ScriptSource = EmitterData != nullptr ? Cast<UNiagaraScriptSource>(EmitterData->GraphSource) : nullptr;
		UNiagaraGraph* Graph = ScriptSource != nullptr ? ScriptSource->NodeGraph : nullptr;
		UNiagaraNodeOutput* SpawnOutputNode = Graph != nullptr
			? Graph->FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleSpawnScript)
			: nullptr;
		if (Layer == nullptr || SpawnOutputNode == nullptr)
		{
			UE_LOG(LogRTSMuzzleVfxAuthoring, Warning, TEXT("RTS_MUZZLE_VFX_AUTHORING skipped emitter=%s layer=%d graph=%d"),
			       *EmitterName, Layer != nullptr ? 1 : 0, SpawnOutputNode != nullptr ? 1 : 0);
			return false;
		}
		Graph->Modify();
		SetEmitterProperties(*EmitterData);
		MakeEmitterLoopForever(*Graph);
		SetUserIntParameter(System, EmitterName + TEXT("_SpawnCount"), 0, false);
		SetUserIntParameter(System, EmitterName + TEXT("_PerShot"), Layer->ParticlesPerShot, true);
		EnsureAttributesDeclared(*Graph, *SpawnOutputNode, GetHasMeshRenderer(*EmitterData));

		UNiagaraNodeCustomHlsl* SpawnNode = FindOrInsertHlslNode(*Graph, ENiagaraScriptUsage::ParticleSpawnScript, SpawnNodeName);
		UNiagaraNodeCustomHlsl* UpdateNode = FindOrInsertHlslNode(*Graph, ENiagaraScriptUsage::ParticleUpdateScript, UpdateNodeName);
		FMuzzleHlsl Hlsl;
		const bool bHasHlsl = BuildLayerHlsl(*Layer, Hlsl);
		const bool bIsSpawnWired = bHasHlsl && SpawnNode != nullptr && WireHlslNode(*Graph, *SpawnNode, *Layer, Hlsl.Spawn);
		const bool bIsUpdateWired = bHasHlsl && UpdateNode != nullptr && WireHlslNode(*Graph, *UpdateNode, *Layer, Hlsl.Update);
		Graph->NotifyGraphChanged();
		UE_LOG(LogRTSMuzzleVfxAuthoring, Display, TEXT("RTS_MUZZLE_VFX_AUTHORING emitter=%s group=%s spawn=%d update=%d mesh=%d"),
		       *EmitterName, Layer->GroupName, bIsSpawnWired ? 1 : 0, bIsUpdateWired ? 1 : 0,
		       GetHasMeshRenderer(*EmitterData) ? 1 : 0);
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
				// The translated code, for checking what the custom HLSL turned into: Saved/MuzzleVfx/Hlsl/.
				const FString DumpPath = FPaths::ProjectSavedDir() / TEXT("MuzzleVfx/Hlsl")
					/ (EmitterHandle.GetName().ToString() + TEXT("_") + Script->GetName() + TEXT(".hlsl"));
				FFileHelper::SaveStringToFile(ExecutableData.LastHlslTranslation, *DumpPath);
				UE_LOG(LogRTSMuzzleVfxAuthoring, Display,
				       TEXT("RTS_MUZZLE_VFX_AUTHORING compiled emitter=%s script=%s status=%d errors=%s"),
				       *EmitterHandle.GetName().ToString(), *Script->GetName(),
				       static_cast<int32>(ExecutableData.LastCompileStatus), *ExecutableData.ErrorMsg);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------
	// Building the emitters (RTS.MuzzleVfx.BuildNiagaraSystem): adds every layer that is missing from the Minimal
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

	UNiagaraRendererProperties* CreateLayerRenderer(UNiagaraEmitter& Emitter, const FMuzzleLayer& Layer)
	{
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Layer.MaterialPath);
		if (Layer.MeshPath != nullptr)
		{
			UNiagaraMeshRendererProperties* MeshRenderer = NewObject<UNiagaraMeshRendererProperties>(
				&Emitter, NAME_None, RF_Transactional);
			FNiagaraMeshRendererMeshProperties MeshProperties;
			MeshProperties.Mesh = LoadObject<UStaticMesh>(nullptr, Layer.MeshPath);
			MeshRenderer->Meshes = {MeshProperties};
			MeshRenderer->bOverrideMaterials = true;
			FNiagaraMeshMaterialOverride MaterialOverride;
			MaterialOverride.ExplicitMat = Material;
			MeshRenderer->OverrideMaterials = {MaterialOverride};
			MeshRenderer->bCastShadows = false;
			return MeshRenderer;
		}
		UNiagaraSpriteRendererProperties* SpriteRenderer = NewObject<UNiagaraSpriteRendererProperties>(
			&Emitter, NAME_None, RF_Transactional);
		SpriteRenderer->Material = Material;
		SpriteRenderer->SubImageSize = FVector2D(Layer.SubImagesPerSide, Layer.SubImagesPerSide);
		return SpriteRenderer;
	}

	void ReplaceRenderers(const FNiagaraEmitterHandle& EmitterHandle, const FMuzzleLayer& Layer)
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

	bool AddLayerEmitter(UNiagaraSystem& System, const FMuzzleLayer& Layer)
	{
		UNiagaraEmitter* Template = LoadObject<UNiagaraEmitter>(nullptr, MinimalEmitterTemplatePath);
		if (Template == nullptr)
		{
			UE_LOG(LogRTSMuzzleVfxAuthoring, Error, TEXT("RTS_MUZZLE_VFX_AUTHORING no emitter template %s"), MinimalEmitterTemplatePath);
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
		UE_LOG(LogRTSMuzzleVfxAuthoring, Display, TEXT("RTS_MUZZLE_VFX_AUTHORING added emitter=%s spawn_per_frame=%d"),
		       Layer.EmitterName, bHasSpawn ? 1 : 0);
		return bHasSpawn;
	}

	// Adds the layers the system does not have yet and gives the others the renderer of the Layers table again.
	void AddMissingLayerEmitters(UNiagaraSystem& System)
	{
		for (const FMuzzleLayer& Layer : Layers)
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

	void PatchMuzzleNiagaraSystem(const bool bAddMissingEmitters);

	void BuildMuzzleNiagaraSystem()
	{
		PatchMuzzleNiagaraSystem(true);
	}

	void PatchMuzzleNiagaraSystemOnly()
	{
		PatchMuzzleNiagaraSystem(false);
	}

	void PatchMuzzleNiagaraSystem(const bool bAddMissingEmitters)
	{
		const URTSWeaponVFXSettings* Settings = URTSWeaponVFXSettings::Get();
		UNiagaraSystem* System = Settings != nullptr ? Settings->SmallArmsMuzzleNiagaraSystem.LoadSynchronous() : nullptr;
		if (System == nullptr)
		{
			UE_LOG(LogRTSMuzzleVfxAuthoring, Error, TEXT("RTS_MUZZLE_VFX_AUTHORING RESULT FAIL system not found"));
			return;
		}
		System->Modify();
		EnsureSystemUserParameters(*System);
		if (bAddMissingEmitters)
		{
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
		UE_LOG(LogRTSMuzzleVfxAuthoring, Display, TEXT("RTS_MUZZLE_VFX_AUTHORING RESULT DONE patched_emitters=%d of %d saved=%d"),
		       PatchedEmitterCount, System->GetEmitterHandles().Num(), bIsSaved ? 1 : 0);
	}

	FAutoConsoleCommand PatchMuzzleNiagaraSystemCommand(
		TEXT("RTS.MuzzleVfx.PatchNiagaraSystem"),
		TEXT("Turns every Flash_<Layer> / Smoke_<Layer> emitter of the small arms muzzle Niagara system into a world-space CPU emitter wired to the shot arrays, and saves it."),
		FConsoleCommandDelegate::CreateStatic(&PatchMuzzleNiagaraSystemOnly));

	FAutoConsoleCommand BuildMuzzleNiagaraSystemCommand(
		TEXT("RTS.MuzzleVfx.BuildNiagaraSystem"),
		TEXT("Adds every missing Flash_/Smoke_ layer emitter (Minimal template, SpawnPerFrame bound to its user spawn count, renderer), then patches and saves the small arms muzzle Niagara system."),
		FConsoleCommandDelegate::CreateStatic(&BuildMuzzleNiagaraSystem));
}

#endif // WITH_EDITOR
