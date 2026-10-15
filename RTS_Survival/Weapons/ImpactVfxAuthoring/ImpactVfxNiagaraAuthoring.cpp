// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only authoring step for impact Niagara systems that are composed from layers of existing explosion systems
// (the howitzer impacts in ImpactSfxVfx/5Artillery/Howitzer). The console command
// RTS.ImpactVfx.BuildFromRecipe <recipe.json> builds every system the recipe lists and saves it:
// - "target": the system to (re)create, as a copy of "base";
// - "add": emitters copied in from other systems ({"from", "emitter", "name"}), without a parent link to them;
// - "rename" / "disable": emitter names to change / emitters to switch off (they are not cooked);
// - "spawn_scale": per emitter spawn count scale, written as a scalability override for every platform;
// - "materials": renderer materials to replace ({"/Game/Old.Old": "/Game/New.New"});
// - "user": user parameter values (number, bool, or a 3 / 4 number array for vectors and colours).
// Every Collision module of every emitter is disabled: impacts never collide.
// Recipes live in Source/Tools/ImpactVfx. Run it in a headless editor (see Tools/ImpactVfx/build_systems.sh) while
// the user's editor does not have the target packages loaded.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEffectType.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSImpactVfxAuthoring, Log, All);

namespace ImpactVfxNiagaraAuthoring
{
	const TCHAR* const CollisionModuleFolder = TEXT("/Modules/Collision/");
	const TCHAR* const UserNamespacePrefix = TEXT("User.");

	FNiagaraEmitterHandle* FindEmitterHandle(UNiagaraSystem& System, const FString& EmitterName)
	{
		for (FNiagaraEmitterHandle& Handle : System.GetEmitterHandles())
		{
			if (Handle.GetName().ToString() == EmitterName)
			{
				return &Handle;
			}
		}
		return nullptr;
	}

	FNiagaraEmitterHandle* FindEmitterHandleById(UNiagaraSystem& System, const FGuid& HandleId)
	{
		for (FNiagaraEmitterHandle& Handle : System.GetEmitterHandles())
		{
			if (Handle.GetId() == HandleId)
			{
				return &Handle;
			}
		}
		return nullptr;
	}

	FName MakeUniqueEmitterName(UNiagaraSystem& System, const FString& BaseName)
	{
		FString Candidate = BaseName;
		for (int32 Suffix = 1; FindEmitterHandle(System, Candidate) != nullptr
		     || StaticFindObjectFast(nullptr, &System, FName(*Candidate)) != nullptr; ++Suffix)
		{
			Candidate = FString::Printf(TEXT("%s_%d"), *BaseName, Suffix);
		}
		return FName(*Candidate);
	}

	FString GetUserParameterShortName(const FNiagaraVariable& Parameter)
	{
		FString Name = Parameter.GetName().ToString();
		Name.RemoveFromStart(UserNamespacePrefix);
		return Name;
	}

	const FNiagaraVariable* FindUserParameter(const TArray<FNiagaraVariable>& Parameters, const FString& ShortName)
	{
		return Parameters.FindByPredicate([&ShortName](const FNiagaraVariable& Parameter)
		{
			return GetUserParameterShortName(Parameter) == ShortName;
		});
	}

	/** Emitters copied from another system read that system's user parameters; adds the ones the target lacks. */
	void CopyMissingUserParameters(const UNiagaraSystem& SourceSystem, UNiagaraSystem& TargetSystem)
	{
		const FNiagaraUserRedirectionParameterStore& SourceStore = SourceSystem.GetExposedParameters();
		FNiagaraUserRedirectionParameterStore& TargetStore = TargetSystem.GetExposedParameters();
		TArray<FNiagaraVariable> SourceParameters;
		SourceStore.GetParameters(SourceParameters);
		TArray<FNiagaraVariable> TargetParameters;
		TargetStore.GetParameters(TargetParameters);
		for (const FNiagaraVariable& SourceParameter : SourceParameters)
		{
			if (FindUserParameter(TargetParameters, GetUserParameterShortName(SourceParameter)) != nullptr)
			{
				continue;
			}
			TargetStore.AddParameter(SourceParameter, true);
			if (SourceParameter.IsDataInterface())
			{
				UNiagaraDataInterface* const SourceInterface = SourceStore.GetDataInterface(SourceParameter);
				if (SourceInterface != nullptr)
				{
					TargetStore.SetDataInterface(DuplicateObject(SourceInterface, &TargetSystem), SourceParameter);
				}
				continue;
			}
			const uint8* const SourceData = SourceStore.GetParameterData(SourceParameter);
			if (SourceData != nullptr)
			{
				TargetStore.SetParameterData(SourceData, SourceParameter);
			}
		}
	}

	UNiagaraSystem* CreateSystemFromBase(const FString& TargetPackageName, UNiagaraSystem& BaseSystem)
	{
		const FString AssetName = FPackageName::GetShortName(TargetPackageName);
		UPackage* const Package = CreatePackage(*TargetPackageName);
		Package->FullyLoad();
		if (UObject* const Existing = StaticFindObject(UObject::StaticClass(), Package, *AssetName))
		{
			// A rebuild replaces the system: the old one is moved out of the package and left to the garbage collector.
			Existing->ClearFlags(RF_Public | RF_Standalone);
			Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			Existing->MarkAsGarbage();
		}
		UNiagaraSystem* const NewSystem = DuplicateObject<UNiagaraSystem>(&BaseSystem, Package, FName(*AssetName));
		NewSystem->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(NewSystem);
		return NewSystem;
	}

	bool AddEmitterFromSystem(UNiagaraSystem& System, const TSharedPtr<FJsonObject>& AddEntry)
	{
		const FString SourcePath = AddEntry->GetStringField(TEXT("from"));
		const FString SourceEmitterName = AddEntry->GetStringField(TEXT("emitter"));
		const FString NewName = AddEntry->HasField(TEXT("name")) ? AddEntry->GetStringField(TEXT("name")) : SourceEmitterName;
		UNiagaraSystem* const SourceSystem = LoadObject<UNiagaraSystem>(nullptr, *SourcePath);
		if (SourceSystem != nullptr)
		{
			// A freshly loaded system may still be compiling; its emitters are copied only once that is done.
			SourceSystem->WaitForCompilationComplete();
		}
		FNiagaraEmitterHandle* const SourceHandle = SourceSystem != nullptr
			                                            ? FindEmitterHandle(*SourceSystem, SourceEmitterName)
			                                            : nullptr;
		if (SourceHandle == nullptr || SourceHandle->GetInstance().Emitter == nullptr)
		{
			UE_LOG(LogRTSImpactVfxAuthoring, Error, TEXT("RTS_IMPACT_VFX_AUTHORING no emitter %s in %s"),
			       *SourceEmitterName, *SourcePath);
			return false;
		}
		const FVersionedNiagaraEmitter SourceInstance = SourceHandle->GetInstance();
		// The copy is created as an object named after the emitter it is made from, and an object of that name
		// already in the system is overwritten (emitter object names need not match their handle names). A staging
		// copy carries a name no handle or object in the system uses.
		const FName UniqueName = MakeUniqueEmitterName(System, NewName);
		UNiagaraEmitter* const StagingEmitter = DuplicateObject<UNiagaraEmitter>(
			SourceInstance.Emitter, GetTransientPackage(), UniqueName);
		const FGuid NewHandleId = FNiagaraEditorUtilities::AddEmitterToSystem(
			System, *StagingEmitter, SourceInstance.Version);
		FNiagaraEmitterHandle* const NewHandle = FindEmitterHandleById(System, NewHandleId);
		if (NewHandle == nullptr || NewHandle->GetEmitterData() == nullptr)
		{
			return false;
		}
		// The staging copy must not stay referenced as a parent: it is transient and would not be saved.
		NewHandle->GetEmitterData()->RemoveParent();
		CopyMissingUserParameters(*SourceSystem, System);
		System.WaitForCompilationComplete();
		UE_LOG(LogRTSImpactVfxAuthoring, Display, TEXT("RTS_IMPACT_VFX_AUTHORING added %s from %s:%s"), *NewName,
		       *FPackageName::GetShortName(SourcePath), *SourceEmitterName);
		return true;
	}

	void RenameEmitters(UNiagaraSystem& System, const TSharedPtr<FJsonObject>& Recipe)
	{
		const TSharedPtr<FJsonObject>* Renames = nullptr;
		if (not Recipe->TryGetObjectField(TEXT("rename"), Renames))
		{
			return;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Rename : (*Renames)->Values)
		{
			FNiagaraEmitterHandle* const Handle = FindEmitterHandle(System, Rename.Key);
			if (Handle == nullptr)
			{
				UE_LOG(LogRTSImpactVfxAuthoring, Error, TEXT("RTS_IMPACT_VFX_AUTHORING rename: no emitter %s"), *Rename.Key);
				continue;
			}
			Handle->SetName(FName(*Rename.Value->AsString()), System);
		}
	}

	void DisableEmitters(UNiagaraSystem& System, const TSharedPtr<FJsonObject>& Recipe)
	{
		const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
		if (not Recipe->TryGetArrayField(TEXT("disable"), Names))
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& Name : *Names)
		{
			FNiagaraEmitterHandle* const Handle = FindEmitterHandle(System, Name->AsString());
			if (Handle == nullptr)
			{
				UE_LOG(LogRTSImpactVfxAuthoring, Error, TEXT("RTS_IMPACT_VFX_AUTHORING disable: no emitter %s"),
				       *Name->AsString());
				continue;
			}
			Handle->SetIsEnabled(false, System, false);
		}
	}

	void SetSpawnCountScale(FVersionedNiagaraEmitterData& EmitterData, const float SpawnCountScale)
	{
		// A platform set left at its default covers every platform and quality level.
		EmitterData.ScalabilityOverrides.Overrides.Reset();
		FNiagaraEmitterScalabilityOverride& Override = EmitterData.ScalabilityOverrides.Overrides.AddDefaulted_GetRef();
		Override.bOverrideSpawnCountScale = true;
		Override.bScaleSpawnCount = true;
		Override.SpawnCountScale = SpawnCountScale;
	}

	void ApplySpawnCountScales(UNiagaraSystem& System, const TSharedPtr<FJsonObject>& Recipe)
	{
		const TSharedPtr<FJsonObject>* Scales = nullptr;
		if (not Recipe->TryGetObjectField(TEXT("spawn_scale"), Scales))
		{
			return;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Scale : (*Scales)->Values)
		{
			FNiagaraEmitterHandle* const Handle = FindEmitterHandle(System, Scale.Key);
			if (Handle == nullptr || Handle->GetEmitterData() == nullptr)
			{
				UE_LOG(LogRTSImpactVfxAuthoring, Error, TEXT("RTS_IMPACT_VFX_AUTHORING spawn_scale: no emitter %s"),
				       *Scale.Key);
				continue;
			}
			SetSpawnCountScale(*Handle->GetEmitterData(), static_cast<float>(Scale.Value->AsNumber()));
		}
	}

	bool ReplaceMaterial(TObjectPtr<UMaterialInterface>& Material, const TMap<FString, UMaterialInterface*>& Replacements)
	{
		if (Material == nullptr)
		{
			return false;
		}
		UMaterialInterface* const* const Replacement = Replacements.Find(Material->GetPathName());
		if (Replacement == nullptr)
		{
			return false;
		}
		Material = *Replacement;
		return true;
	}

	int32 ReplaceRendererMaterials(UNiagaraRendererProperties& Renderer, const TMap<FString, UMaterialInterface*>& Replacements)
	{
		int32 ReplacedCount = 0;
		if (UNiagaraSpriteRendererProperties* const Sprite = Cast<UNiagaraSpriteRendererProperties>(&Renderer))
		{
			ReplacedCount += ReplaceMaterial(Sprite->Material, Replacements) ? 1 : 0;
		}
		else if (UNiagaraRibbonRendererProperties* const Ribbon = Cast<UNiagaraRibbonRendererProperties>(&Renderer))
		{
			ReplacedCount += ReplaceMaterial(Ribbon->Material, Replacements) ? 1 : 0;
		}
		else if (UNiagaraMeshRendererProperties* const Mesh = Cast<UNiagaraMeshRendererProperties>(&Renderer))
		{
			for (FNiagaraMeshMaterialOverride& Override : Mesh->OverrideMaterials)
			{
				ReplacedCount += ReplaceMaterial(Override.ExplicitMat, Replacements) ? 1 : 0;
			}
		}
		return ReplacedCount;
	}

	void ApplyMaterialReplacements(UNiagaraSystem& System, const TSharedPtr<FJsonObject>& Recipe)
	{
		const TSharedPtr<FJsonObject>* MaterialMap = nullptr;
		if (not Recipe->TryGetObjectField(TEXT("materials"), MaterialMap))
		{
			return;
		}
		TMap<FString, UMaterialInterface*> Replacements;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*MaterialMap)->Values)
		{
			UMaterialInterface* const NewMaterial = LoadObject<UMaterialInterface>(nullptr, *Entry.Value->AsString());
			if (NewMaterial == nullptr)
			{
				UE_LOG(LogRTSImpactVfxAuthoring, Error, TEXT("RTS_IMPACT_VFX_AUTHORING no material %s"),
				       *Entry.Value->AsString());
				continue;
			}
			Replacements.Add(Entry.Key, NewMaterial);
		}
		int32 ReplacedCount = 0;
		for (FNiagaraEmitterHandle& Handle : System.GetEmitterHandles())
		{
			const FVersionedNiagaraEmitterData* const EmitterData = Handle.GetEmitterData();
			if (EmitterData == nullptr)
			{
				continue;
			}
			for (UNiagaraRendererProperties* const Renderer : EmitterData->GetRenderers())
			{
				if (Renderer == nullptr)
				{
					continue;
				}
				Renderer->Modify();
				const int32 RendererReplacements = ReplaceRendererMaterials(*Renderer, Replacements);
				if (RendererReplacements > 0)
				{
					Renderer->PostEditChange();
				}
				ReplacedCount += RendererReplacements;
			}
		}
		UE_LOG(LogRTSImpactVfxAuthoring, Display, TEXT("RTS_IMPACT_VFX_AUTHORING replaced %d renderer materials"),
		       ReplacedCount);
	}

	bool SetUserParameterValue(FNiagaraUserRedirectionParameterStore& Store, const FNiagaraVariable& Parameter,
	                           const TSharedPtr<FJsonValue>& Value)
	{
		const FNiagaraTypeDefinition& Type = Parameter.GetType();
		if (Type == FNiagaraTypeDefinition::GetFloatDef())
		{
			return Store.SetParameterValue(static_cast<float>(Value->AsNumber()), Parameter);
		}
		if (Type == FNiagaraTypeDefinition::GetIntDef())
		{
			return Store.SetParameterValue(static_cast<int32>(Value->AsNumber()), Parameter);
		}
		if (Type == FNiagaraTypeDefinition::GetBoolDef())
		{
			FNiagaraBool BoolValue;
			BoolValue.SetValue(Value->AsBool());
			return Store.SetParameterValue(BoolValue, Parameter);
		}
		const TArray<TSharedPtr<FJsonValue>>& Numbers = Value->AsArray();
		auto GetNumber = [&Numbers](const int32 Index, const float Default)
		{
			return Numbers.IsValidIndex(Index) ? static_cast<float>(Numbers[Index]->AsNumber()) : Default;
		};
		if (Type == FNiagaraTypeDefinition::GetColorDef())
		{
			return Store.SetParameterValue(FLinearColor(GetNumber(0, 1.f), GetNumber(1, 1.f), GetNumber(2, 1.f), GetNumber(3, 1.f)),
			                               Parameter);
		}
		if (Type == FNiagaraTypeDefinition::GetVec3Def())
		{
			return Store.SetParameterValue(FVector3f(GetNumber(0, 0.f), GetNumber(1, 0.f), GetNumber(2, 0.f)), Parameter);
		}
		return false;
	}

	void ApplyUserParameters(UNiagaraSystem& System, const TSharedPtr<FJsonObject>& Recipe)
	{
		const TSharedPtr<FJsonObject>* UserValues = nullptr;
		if (not Recipe->TryGetObjectField(TEXT("user"), UserValues))
		{
			return;
		}
		FNiagaraUserRedirectionParameterStore& Store = System.GetExposedParameters();
		TArray<FNiagaraVariable> Parameters;
		Store.GetParameters(Parameters);
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*UserValues)->Values)
		{
			const FNiagaraVariable* const Parameter = FindUserParameter(Parameters, Entry.Key);
			const bool bIsSet = Parameter != nullptr && SetUserParameterValue(Store, *Parameter, Entry.Value);
			UE_LOG(LogRTSImpactVfxAuthoring, Display, TEXT("RTS_IMPACT_VFX_AUTHORING user %s set=%d"), *Entry.Key,
			       bIsSet ? 1 : 0);
		}
	}

	/** Impacts must never collide; disabled modules are compiled out, so they cost nothing. */
	int32 DisableCollisionModules(UNiagaraSystem& System)
	{
		int32 DisabledCount = 0;
		for (FNiagaraEmitterHandle& Handle : System.GetEmitterHandles())
		{
			const FVersionedNiagaraEmitterData* const EmitterData = Handle.GetEmitterData();
			const UNiagaraScriptSource* const ScriptSource = EmitterData != nullptr
				                                                 ? Cast<UNiagaraScriptSource>(EmitterData->GraphSource)
				                                                 : nullptr;
			if (ScriptSource == nullptr || ScriptSource->NodeGraph == nullptr)
			{
				continue;
			}
			for (UEdGraphNode* const Node : ScriptSource->NodeGraph->Nodes)
			{
				UNiagaraNodeFunctionCall* const FunctionCall = Cast<UNiagaraNodeFunctionCall>(Node);
				const bool bIsCollisionModule = FunctionCall != nullptr && FunctionCall->FunctionScript != nullptr
					&& FunctionCall->FunctionScript->GetPathName().Contains(CollisionModuleFolder);
				if (not bIsCollisionModule || not FunctionCall->IsNodeEnabled())
				{
					continue;
				}
				FNiagaraStackGraphUtilities::SetModuleIsEnabled(*FunctionCall, false);
				++DisabledCount;
			}
		}
		return DisabledCount;
	}

	void LogSystemLayout(const UNiagaraSystem& System)
	{
		for (const FNiagaraEmitterHandle& Handle : System.GetEmitterHandles())
		{
			const FVersionedNiagaraEmitterData* const EmitterData = Handle.GetEmitterData();
			if (EmitterData == nullptr)
			{
				continue;
			}
			FString Materials;
			for (const UNiagaraRendererProperties* const Renderer : EmitterData->GetRenderers())
			{
				if (const UNiagaraSpriteRendererProperties* const Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
				{
					Materials += TEXT(" ") + GetNameSafe(Sprite->Material);
				}
			}
			const float SpawnScale = EmitterData->ScalabilityOverrides.Overrides.IsEmpty()
				                         ? 1.f
				                         : EmitterData->ScalabilityOverrides.Overrides[0].SpawnCountScale;
			UE_LOG(LogRTSImpactVfxAuthoring, Display,
			       TEXT("RTS_IMPACT_VFX_AUTHORING   emitter %s enabled=%d gpu=%d local=%d spawn_scale=%.2f renderers=%d%s"),
			       *Handle.GetName().ToString(), Handle.GetIsEnabled() ? 1 : 0,
			       EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim ? 1 : 0, EmitterData->bLocalSpace ? 1 : 0,
			       SpawnScale, EmitterData->GetRenderers().Num(), *Materials);
		}
	}

	/** A script shared by two emitters gets no DDC handle for the second compile task and asserts in the DDC. */
	int32 LogSharedScripts(const UNiagaraSystem& System)
	{
		TMap<const UNiagaraScript*, FString> ScriptOwners;
		int32 SharedCount = 0;
		for (const FNiagaraEmitterHandle& Handle : System.GetEmitterHandles())
		{
			const FVersionedNiagaraEmitterData* const EmitterData = Handle.GetEmitterData();
			if (EmitterData == nullptr)
			{
				continue;
			}
			TArray<UNiagaraScript*> Scripts;
			EmitterData->GetScripts(Scripts, false, true);
			for (const UNiagaraScript* const Script : Scripts)
			{
				if (const FString* const Owner = ScriptOwners.Find(Script))
				{
					UE_LOG(LogRTSImpactVfxAuthoring, Warning, TEXT("RTS_IMPACT_VFX_AUTHORING shared script %s: %s and %s"),
					       *Script->GetPathName(), **Owner, *Handle.GetName().ToString());
					++SharedCount;
					continue;
				}
				ScriptOwners.Add(Script, Handle.GetName().ToString());
			}
		}
		return SharedCount;
	}

	bool SaveSystem(UNiagaraSystem& System)
	{
		UPackage* const Package = System.GetOutermost();
		Package->MarkPackageDirty();
		const FString FileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &System, *FileName, SaveArgs);
	}

	bool BuildSystem(const TSharedPtr<FJsonObject>& Recipe)
	{
		const FString TargetPath = Recipe->GetStringField(TEXT("target"));
		const FString BasePath = Recipe->GetStringField(TEXT("base"));
		UNiagaraSystem* const BaseSystem = LoadObject<UNiagaraSystem>(nullptr, *BasePath);
		if (BaseSystem == nullptr)
		{
			UE_LOG(LogRTSImpactVfxAuthoring, Error, TEXT("RTS_IMPACT_VFX_AUTHORING no base system %s"), *BasePath);
			return false;
		}
		BaseSystem->WaitForCompilationComplete();
		UNiagaraSystem* const System = CreateSystemFromBase(TargetPath, *BaseSystem);
		// The copy starts compiling right away; editing it while that runs and then forcing a new compile trips the
		// DDC (a pending task is replaced while it is being waited on).
		System->WaitForCompilationComplete();
		const TArray<TSharedPtr<FJsonValue>>* AddEntries = nullptr;
		if (Recipe->TryGetArrayField(TEXT("add"), AddEntries))
		{
			for (const TSharedPtr<FJsonValue>& AddEntry : *AddEntries)
			{
				AddEmitterFromSystem(*System, AddEntry->AsObject());
			}
		}
		RenameEmitters(*System, Recipe);
		DisableEmitters(*System, Recipe);
		ApplySpawnCountScales(*System, Recipe);
		ApplyMaterialReplacements(*System, Recipe);
		ApplyUserParameters(*System, Recipe);
		const int32 DisabledCollisionCount = DisableCollisionModules(*System);
		if (LogSharedScripts(*System) > 0)
		{
			UE_LOG(LogRTSImpactVfxAuthoring, Error, TEXT("RTS_IMPACT_VFX_AUTHORING %s not compiled: shared scripts"), *TargetPath);
			return false;
		}
		System->RequestCompile(false);
		System->WaitForCompilationComplete();
		LogSystemLayout(*System);
		const bool bIsSaved = SaveSystem(*System);
		UE_LOG(LogRTSImpactVfxAuthoring, Display,
		       TEXT("RTS_IMPACT_VFX_AUTHORING built %s from %s: emitters=%d collision_modules_disabled=%d saved=%d"),
		       *TargetPath, *FPackageName::GetShortName(BasePath), System->GetEmitterHandles().Num(),
		       DisabledCollisionCount, bIsSaved ? 1 : 0);
		return bIsSaved;
	}

	void BuildFromRecipe(const TArray<FString>& Args)
	{
		if (Args.IsEmpty())
		{
			UE_LOG(LogRTSImpactVfxAuthoring, Error, TEXT("RTS_IMPACT_VFX_AUTHORING RESULT FAIL usage: <recipe.json>"));
			return;
		}
		FString RecipeText;
		TSharedPtr<FJsonObject> Root;
		const bool bIsRead = FFileHelper::LoadFileToString(RecipeText, *Args[0])
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RecipeText), Root) && Root.IsValid();
		const TArray<TSharedPtr<FJsonValue>>* Systems = nullptr;
		if (not bIsRead || not Root->TryGetArrayField(TEXT("systems"), Systems))
		{
			UE_LOG(LogRTSImpactVfxAuthoring, Error, TEXT("RTS_IMPACT_VFX_AUTHORING RESULT FAIL cannot read %s"), *Args[0]);
			return;
		}
		int32 BuiltCount = 0;
		for (const TSharedPtr<FJsonValue>& SystemRecipe : *Systems)
		{
			BuiltCount += BuildSystem(SystemRecipe->AsObject()) ? 1 : 0;
		}
		UE_LOG(LogRTSImpactVfxAuthoring, Display, TEXT("RTS_IMPACT_VFX_AUTHORING RESULT DONE built=%d of %d"),
		       BuiltCount, Systems->Num());
	}

	FAutoConsoleCommand BuildFromRecipeCommand(
		TEXT("RTS.ImpactVfx.BuildFromRecipe"),
		TEXT("Builds the impact Niagara systems a JSON recipe lists from layers of existing systems, then saves them."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&BuildFromRecipe));
}

#endif // WITH_EDITOR
