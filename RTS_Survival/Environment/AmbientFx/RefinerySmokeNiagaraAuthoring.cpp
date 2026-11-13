// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only authoring step for the German refinery's chimney smoke (NS_RadixiteSmoke1 on Ger_Refinery_Truck).
// The console command RTS.AmbientFx.PatchRefinerySmoke [<target system> [<source system>]] makes the plume smaller
// and lets it lean in the ambient wind:
// - writes the emitter's constant inputs (sprite size, rise speed, spawn rate, sprite rotation range);
// - rewrites the size and opacity curves so puffs grow less and are gone before they get large;
// - inserts the custom HLSL node SmokeWind at the end of particle update (code in RefinerySmokeHlsl/SmokeWind.hlsl).
// With a source system the target is first made as a copy of it (for side-by-side look-dev copies); without
// arguments the live system is patched in place. Running it again gives the same result.
// Run it in a headless editor while no other editor has the system loaded (the save fails on a locked file).

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_Niagara.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NiagaraDataInterfaceCurve.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "RTS_Survival/Environment/AmbientFx/RTSAmbientFxSettings.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSRefinerySmokeAuthoring, Log, All);

namespace RefinerySmokeNiagaraAuthoring
{
	const TCHAR* const DefaultSystemPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerEco/OpelBlitz_Ref/Refinery_Meshes/FX/NS_RadixiteSmoke1");
	const TCHAR* const EmitterName = TEXT("P_SmokeRibbons2a");
	const TCHAR* const WindNodeName = TEXT("SmokeWind");
	const TCHAR* const HlslFile = TEXT("Source/RTS_Survival/Environment/AmbientFx/RefinerySmokeHlsl/SmokeWind.hlsl");
	const TCHAR* const GeneratedHeader = TEXT(
		"// Written by the console command RTS.AmbientFx.PatchRefinerySmoke from RefinerySmokeHlsl/SmokeWind.hlsl; edits here are overwritten.\n");
	constexpr int32 HlslNodeOffsetX = 250;

	// Puffs used to start 6-9 m wide and double in size; they now start 3.5-5 m wide (times the size curve's 0.2 at
	// birth, so the plume is narrow at the chimney).
	constexpr float SpriteSizeMinimum = 350.f;
	constexpr float SpriteSizeMaximum = 500.f;
	// Rise speed at the chimney in cm/s; the wind node then bleeds it off, so the plume tops out at about 6 m.
	const FVector3f StartVelocityMinimum(-20.f, -20.f, 220.f);
	const FVector3f StartVelocityMaximum(20.f, 20.f, 280.f);
	// Puff life in seconds (was 4-5): a bit longer, so the faint tail stretches downwind as a trail.
	constexpr float LifetimeMinimumSeconds = 5.f;
	constexpr float LifetimeMaximumSeconds = 6.5f;
	// The colour is a random pick between Radix_Green and Radix_Yellow from the shared NIAPC_Radixite collection, so it
	// stays untouched; ScaleColor's RGB (was 1) brightens it on this emitter only, so the radixite green reads in the core.
	const FVector3f ScaleColorRgb(1.8f, 1.8f, 1.8f);
	// Smaller, fainter puffs need more of them to read as one continuous plume.
	constexpr float SpawnRatePerSecond = 12.f;
	// Random start angle of the ribbon flipbook, so neighbouring puffs never repeat the same shape.
	constexpr float SpriteRotationMaximumDegrees = 360.f;

	struct FCurveKey
	{
		float Time;
		float Value;
	};

	// Size multiplier over the puff's life (was 0.1 to 2.0): narrow at the chimney, spreading downwind.
	const TArray<FCurveKey> SizeCurveKeys = {{0.f, 0.2f}, {1.f, 1.7f}};
	// Opacity multiplier over the puff's life (was 0, 0.7 at 20 %, 0.5 at 80 %, 0): a dense core near the chimney that
	// thins out early, so the spread-out puffs downwind read as a see-through trail rather than a solid clump. The core
	// is near full opacity, which is where the radixite green of the flipbook shows.
	const TArray<FCurveKey> OpacityCurveKeys = {{0.f, 0.f}, {0.05f, 1.f}, {0.3f, 0.45f}, {1.f, 0.f}};
	// The emitter has exactly two curve data interfaces; the key count tells them apart.
	constexpr int32 SizeCurveKeyCount = 2;
	constexpr int32 OpacityCurveKeyCount = 4;

	FString MakeConstantName(const FString& Input)
	{
		return FString::Printf(TEXT("Constants.%s.%s"), EmitterName, *Input);
	}

	/** @return Number of the emitter's scripts that hold the constant (spawn, update or GPU copies). */
	template <typename TValue>
	int32 SetConstantInput(FVersionedNiagaraEmitterData& EmitterData, const FString& Input,
	                       const FNiagaraTypeDefinition& Type, const TValue& Value)
	{
		const FNiagaraVariable Parameter(Type, FName(*MakeConstantName(Input)));
		TArray<UNiagaraScript*> Scripts;
		EmitterData.GetScripts(Scripts, false);
		int32 WrittenCount = 0;
		for (UNiagaraScript* Script : Scripts)
		{
			if (Script == nullptr || Script->RapidIterationParameters.IndexOf(Parameter) == INDEX_NONE)
			{
				continue;
			}
			Script->Modify();
			Script->RapidIterationParameters.SetParameterValue(Value, Parameter);
			++WrittenCount;
		}
		if (WrittenCount == 0)
		{
			UE_LOG(LogRTSRefinerySmokeAuthoring, Error, TEXT("RTS_REFINERY_SMOKE no constant %s"), *Parameter.GetName().ToString());
		}
		return WrittenCount;
	}

	int32 SetConstantInputs(FVersionedNiagaraEmitterData& EmitterData)
	{
		const FNiagaraTypeDefinition FloatType = FNiagaraTypeDefinition::GetFloatDef();
		const FNiagaraTypeDefinition VectorType = FNiagaraTypeDefinition::GetVec3Def();
		int32 MissingCount = 0;
		MissingCount += SetConstantInput(EmitterData, TEXT("ScaleColor.Scale RGB"), VectorType, ScaleColorRgb) == 0;
		// RandomRangeFloat is InitializeParticle's Lifetime.
		MissingCount += SetConstantInput(EmitterData, TEXT("RandomRangeFloat.Minimum"), FloatType, LifetimeMinimumSeconds) == 0;
		MissingCount += SetConstantInput(EmitterData, TEXT("RandomRangeFloat.Maximum"), FloatType, LifetimeMaximumSeconds) == 0;
		// RandomRangeFloat001 feeds Vector2DFromFloat, which is InitializeParticle's Sprite Size.
		MissingCount += SetConstantInput(EmitterData, TEXT("RandomRangeFloat001.Minimum"), FloatType, SpriteSizeMinimum) == 0;
		MissingCount += SetConstantInput(EmitterData, TEXT("RandomRangeFloat001.Maximum"), FloatType, SpriteSizeMaximum) == 0;
		// RandomRangeFloat002 is InitializeParticle's Sprite Rotation Angle.
		MissingCount += SetConstantInput(EmitterData, TEXT("RandomRangeFloat002.Minimum"), FloatType, 0.f) == 0;
		MissingCount += SetConstantInput(EmitterData, TEXT("RandomRangeFloat002.Maximum"), FloatType, SpriteRotationMaximumDegrees) == 0;
		// RandomRangeVector is AddVelocity's Velocity.
		MissingCount += SetConstantInput(EmitterData, TEXT("RandomRangeVector.Minimum"), VectorType, StartVelocityMinimum) == 0;
		MissingCount += SetConstantInput(EmitterData, TEXT("RandomRangeVector.Maximum"), VectorType, StartVelocityMaximum) == 0;
		MissingCount += SetConstantInput(EmitterData, TEXT("SpawnRate.SpawnRate"), FloatType, SpawnRatePerSecond) == 0;
		return MissingCount;
	}

	void WriteCurve(UNiagaraDataInterfaceCurve& CurveInterface, const TArray<FCurveKey>& Keys)
	{
		CurveInterface.Modify();
		CurveInterface.Curve.Reset();
		for (const FCurveKey& Key : Keys)
		{
			const FKeyHandle Handle = CurveInterface.Curve.AddKey(Key.Time, Key.Value);
			CurveInterface.Curve.SetKeyInterpMode(Handle, RCIM_Cubic);
			CurveInterface.Curve.SetKeyTangentMode(Handle, RCTM_Auto);
		}
		CurveInterface.Curve.AutoSetTangents();
		// Rebuilds the lookup table the scripts sample.
		CurveInterface.PostEditChange();
	}

	/** Rewrites every curve data interface in the system: graph inputs and the compiled scripts' cached copies. */
	void WriteCurves(UNiagaraSystem& System, int32& OutSizeCurveCount, int32& OutOpacityCurveCount)
	{
		OutSizeCurveCount = 0;
		OutOpacityCurveCount = 0;
		TArray<UObject*> Objects;
		GetObjectsWithOuter(&System, Objects, true);
		for (UObject* const Object : Objects)
		{
			UNiagaraDataInterfaceCurve* const CurveInterface = Cast<UNiagaraDataInterfaceCurve>(Object);
			if (CurveInterface == nullptr)
			{
				continue;
			}
			const int32 KeyCount = CurveInterface->Curve.GetNumKeys();
			if (KeyCount == SizeCurveKeyCount)
			{
				WriteCurve(*CurveInterface, SizeCurveKeys);
				++OutSizeCurveCount;
			}
			else if (KeyCount == OpacityCurveKeyCount)
			{
				WriteCurve(*CurveInterface, OpacityCurveKeys);
				++OutOpacityCurveCount;
			}
			else
			{
				UE_LOG(LogRTSRefinerySmokeAuthoring, Warning, TEXT("RTS_REFINERY_SMOKE left curve %s with %d keys"),
				       *CurveInterface->GetName(), KeyCount);
			}
		}
	}

	bool GetIsParameterMapPin(const UEdGraphPin& Pin)
	{
		return Pin.PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryType
			&& Pin.PinType.PinSubCategoryObject == FNiagaraTypeDefinition::GetParameterMapStruct();
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

	UNiagaraNodeCustomHlsl* FindHlslNode(UEdGraph& Graph)
	{
		for (UEdGraphNode* const Node : Graph.Nodes)
		{
			UNiagaraNodeCustomHlsl* const HlslNode = Cast<UNiagaraNodeCustomHlsl>(Node);
			if (HlslNode != nullptr && HlslNode->GetFunctionName() == WindNodeName)
			{
				return HlslNode;
			}
		}
		return nullptr;
	}

	/** Puts a custom HLSL node with only parameter map pins between the update's last module and its output. */
	UNiagaraNodeCustomHlsl* InsertHlslNodeBeforeOutput(UEdGraph& Graph, UNiagaraNodeOutput& OutputNode)
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
		HlslNode->OnRenameNode(WindNodeName);
		HlslNode->NodePosX = OutputNode.NodePosX - HlslNodeOffsetX;
		HlslNode->NodePosY = OutputNode.NodePosY;

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

	bool BuildWindHlsl(FString& OutHlsl)
	{
		const FString FilePath = FPaths::Combine(FPaths::ProjectDir(), HlslFile);
		FString FileText;
		if (not FFileHelper::LoadFileToString(FileText, *FilePath))
		{
			UE_LOG(LogRTSRefinerySmokeAuthoring, Error, TEXT("RTS_REFINERY_SMOKE cannot read %s"), *FilePath);
			return false;
		}
		const URTSAmbientFxSettings* const Settings = URTSAmbientFxSettings::Get();
		const float WindYawRadians = FMath::DegreesToRadians(Settings != nullptr ? Settings->WindYawDegrees : 0.f);
		FileText.ReplaceInline(TEXT("{WindX}"), *FString::SanitizeFloat(FMath::Cos(WindYawRadians)));
		FileText.ReplaceInline(TEXT("{WindY}"), *FString::SanitizeFloat(FMath::Sin(WindYawRadians)));
		OutHlsl = GeneratedHeader + FileText;
		return true;
	}

	bool SetHlsl(UNiagaraNodeCustomHlsl& HlslNode, const FString& Hlsl)
	{
		FStrProperty* const HlslProperty = FindFProperty<FStrProperty>(HlslNode.GetClass(), TEXT("CustomHlsl"));
		if (HlslProperty == nullptr)
		{
			return false;
		}
		HlslNode.Modify();
		HlslProperty->SetPropertyValue_InContainer(&HlslNode, Hlsl);
		// Tells the node its code changed, which marks the script for a recompile.
		FPropertyChangedEvent HlslChangedEvent(HlslProperty);
		HlslNode.PostEditChangeProperty(HlslChangedEvent);
		return true;
	}

	bool WriteWindNode(UNiagaraGraph& Graph)
	{
		UNiagaraNodeCustomHlsl* HlslNode = FindHlslNode(Graph);
		if (HlslNode == nullptr)
		{
			UNiagaraNodeOutput* const OutputNode = Graph.FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleUpdateScript);
			HlslNode = OutputNode != nullptr ? InsertHlslNodeBeforeOutput(Graph, *OutputNode) : nullptr;
		}
		FString Hlsl;
		return HlslNode != nullptr && BuildWindHlsl(Hlsl) && SetHlsl(*HlslNode, Hlsl);
	}

	UNiagaraSystem* CopySystem(const FString& SourcePath, const FString& TargetPath)
	{
		UNiagaraSystem* const SourceSystem = LoadObject<UNiagaraSystem>(nullptr, *SourcePath);
		if (SourceSystem == nullptr)
		{
			return nullptr;
		}
		SourceSystem->WaitForCompilationComplete();
		const FString AssetName = FPackageName::GetShortName(TargetPath);
		UPackage* const Package = CreatePackage(*TargetPath);
		Package->FullyLoad();
		if (UObject* const Existing = StaticFindObject(UObject::StaticClass(), Package, *AssetName))
		{
			// A rebuild replaces the copy: the old one is moved out of the package and left to the garbage collector.
			Existing->ClearFlags(RF_Public | RF_Standalone);
			Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			Existing->MarkAsGarbage();
		}
		UNiagaraSystem* const NewSystem = DuplicateObject<UNiagaraSystem>(SourceSystem, Package, FName(*AssetName));
		NewSystem->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(NewSystem);
		return NewSystem;
	}

	UNiagaraSystem* LoadTargetSystem(const TArray<FString>& Args)
	{
		const FString TargetPath = Args.Num() > 0 ? Args[0] : FString(DefaultSystemPath);
		if (Args.Num() > 1)
		{
			return CopySystem(Args[1], TargetPath);
		}
		return LoadObject<UNiagaraSystem>(nullptr, *TargetPath);
	}

	void LogCompileResults(const FVersionedNiagaraEmitterData& EmitterData)
	{
		TArray<UNiagaraScript*> Scripts;
		EmitterData.GetScripts(Scripts, true);
		for (const UNiagaraScript* const Script : Scripts)
		{
			const FNiagaraVMExecutableData& ExecutableData = Script->GetVMExecutableData();
			UE_LOG(LogRTSRefinerySmokeAuthoring, Display, TEXT("RTS_REFINERY_SMOKE compiled script=%s status=%d errors=%s"),
			       *Script->GetName(), static_cast<int32>(ExecutableData.LastCompileStatus), *ExecutableData.ErrorMsg);
		}
	}

	bool PatchEmitter(UNiagaraSystem& System, FVersionedNiagaraEmitterData& EmitterData)
	{
		UNiagaraScriptSource* const ScriptSource = Cast<UNiagaraScriptSource>(EmitterData.GraphSource);
		UNiagaraGraph* const Graph = ScriptSource != nullptr ? ScriptSource->NodeGraph : nullptr;
		if (Graph == nullptr)
		{
			return false;
		}
		UE_LOG(LogRTSRefinerySmokeAuthoring, Display, TEXT("RTS_REFINERY_SMOKE emitter local_space=%d sim_target=%d"),
		       EmitterData.bLocalSpace ? 1 : 0, static_cast<int32>(EmitterData.SimTarget));
		Graph->Modify();
		const int32 MissingConstantCount = SetConstantInputs(EmitterData);
		int32 SizeCurveCount = 0;
		int32 OpacityCurveCount = 0;
		WriteCurves(System, SizeCurveCount, OpacityCurveCount);
		const bool bIsWindWritten = WriteWindNode(*Graph);
		Graph->NotifyGraphChanged();
		UE_LOG(LogRTSRefinerySmokeAuthoring, Display,
		       TEXT("RTS_REFINERY_SMOKE missing_constants=%d size_curves=%d opacity_curves=%d wind_node=%d"),
		       MissingConstantCount, SizeCurveCount, OpacityCurveCount, bIsWindWritten ? 1 : 0);
		return MissingConstantCount == 0 && SizeCurveCount > 0 && OpacityCurveCount > 0 && bIsWindWritten;
	}

	FVersionedNiagaraEmitterData* FindEmitterData(UNiagaraSystem& System)
	{
		for (const FNiagaraEmitterHandle& Handle : System.GetEmitterHandles())
		{
			if (Handle.GetName().ToString() == EmitterName)
			{
				return Handle.GetEmitterData();
			}
		}
		return nullptr;
	}

	void PatchRefinerySmoke(const TArray<FString>& Args)
	{
		UNiagaraSystem* const System = LoadTargetSystem(Args);
		if (System == nullptr)
		{
			UE_LOG(LogRTSRefinerySmokeAuthoring, Error, TEXT("RTS_REFINERY_SMOKE RESULT FAIL system not found"));
			return;
		}
		System->WaitForCompilationComplete();
		FVersionedNiagaraEmitterData* const EmitterData = FindEmitterData(*System);
		if (EmitterData == nullptr)
		{
			UE_LOG(LogRTSRefinerySmokeAuthoring, Error, TEXT("RTS_REFINERY_SMOKE RESULT FAIL no emitter %s"), EmitterName);
			return;
		}
		System->Modify();
		const bool bIsPatched = PatchEmitter(*System, *EmitterData);
		System->RequestCompile(true);
		System->WaitForCompilationComplete();
		LogCompileResults(*EmitterData);
		System->MarkPackageDirty();
		const bool bIsSaved = UEditorLoadingAndSavingUtils::SavePackages({System->GetOutermost()}, false);
		UE_LOG(LogRTSRefinerySmokeAuthoring, Display, TEXT("RTS_REFINERY_SMOKE RESULT %s system=%s saved=%d"),
		       bIsPatched ? TEXT("DONE") : TEXT("PARTIAL"), *System->GetPathName(), bIsSaved ? 1 : 0);
	}

	FAutoConsoleCommand PatchRefinerySmokeCommand(
		TEXT("RTS.AmbientFx.PatchRefinerySmoke"),
		TEXT("Makes the refinery chimney smoke smaller and lean in the ambient wind, and saves it. Args: [target system] [source system to copy first]."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&PatchRefinerySmoke));
}

#endif // WITH_EDITOR
