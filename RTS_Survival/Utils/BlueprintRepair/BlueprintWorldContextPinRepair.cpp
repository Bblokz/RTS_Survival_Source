// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only repair for Blueprint function libraries whose function entry holds a user input named like the hidden
// world context pin (__WorldContext), which a function copy or merge can leave behind. Loading such a Blueprint fires
// the ensure in UK2Node_FunctionEntry::AllocateDefaultPins, and that logged error fails every cook.
// The console command RTS.Blueprint.RemoveUserWorldContextPins <BlueprintPath> [<BlueprintPath> ...] removes those
// user inputs (the engine adds its own hidden pin back), compiles each Blueprint and saves it.
// Run it in a headless editor while no other editor has the Blueprint loaded, e.g.
// UnrealEditor-Cmd.exe <uproject> /Engine/Maps/Entry -ExecCmds="RTS.Blueprint.RemoveUserWorldContextPins /Game/X, QUIT_EDITOR"

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSBlueprintRepair, Log, All);

namespace BlueprintWorldContextPinRepair
{
	// The name UK2Node_FunctionEntry gives the hidden world context pin of function library functions.
	const FName WorldContextPinName(TEXT("__WorldContext"));

	int32 RemoveUserWorldContextPins(UK2Node_FunctionEntry& FunctionEntry)
	{
		const int32 UserPinCount = FunctionEntry.UserDefinedPins.FilterByPredicate(
			[](const TSharedPtr<FUserPinInfo>& UserPin)
			{
				return UserPin.IsValid() && UserPin->PinName == WorldContextPinName;
			}).Num();
		if (UserPinCount == 0)
		{
			return 0;
		}
		FunctionEntry.Modify();
		// Removes the user pin description and the visible pin made from it; the reconstruct adds the hidden one.
		FunctionEntry.RemoveUserDefinedPinByName(WorldContextPinName);
		FunctionEntry.ReconstructNode();
		return UserPinCount;
	}

	int32 RepairFunctionGraphs(UBlueprint& Blueprint)
	{
		int32 RemovedCount = 0;
		for (UEdGraph* const FunctionGraph : Blueprint.FunctionGraphs)
		{
			if (not IsValid(FunctionGraph))
			{
				continue;
			}
			TArray<UK2Node_FunctionEntry*> FunctionEntries;
			FunctionGraph->GetNodesOfClass(FunctionEntries);
			for (UK2Node_FunctionEntry* const FunctionEntry : FunctionEntries)
			{
				const int32 Removed = IsValid(FunctionEntry) ? RemoveUserWorldContextPins(*FunctionEntry) : 0;
				if (Removed > 0)
				{
					UE_LOG(LogRTSBlueprintRepair, Display, TEXT("RTS_BLUEPRINT_REPAIR removed=%d function=%s"),
					       Removed, *FunctionGraph->GetName());
				}
				RemovedCount += Removed;
			}
		}
		return RemovedCount;
	}

	bool SaveBlueprint(UBlueprint& Blueprint)
	{
		UPackage* const Package = Blueprint.GetOutermost();
		Package->MarkPackageDirty();
		const FString FileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Blueprint, *FileName, SaveArgs);
	}

	void RepairBlueprint(const FString& BlueprintPath)
	{
		UBlueprint* const Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
		if (not IsValid(Blueprint))
		{
			UE_LOG(LogRTSBlueprintRepair, Error, TEXT("RTS_BLUEPRINT_REPAIR not_a_blueprint %s"), *BlueprintPath);
			return;
		}
		const int32 RemovedCount = RepairFunctionGraphs(*Blueprint);
		if (RemovedCount == 0)
		{
			UE_LOG(LogRTSBlueprintRepair, Display, TEXT("RTS_BLUEPRINT_REPAIR nothing_to_repair %s"), *BlueprintPath);
			return;
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		const bool bCompiled = Blueprint->Status != BS_Error;
		const bool bSaved = bCompiled && SaveBlueprint(*Blueprint);
		UE_LOG(LogRTSBlueprintRepair, Display, TEXT("RTS_BLUEPRINT_REPAIR RESULT %s removed=%d compiled=%d saved=%d"),
		       *BlueprintPath, RemovedCount, bCompiled, bSaved);
	}

	void RemoveUserWorldContextPinsCommand(const TArray<FString>& Arguments)
	{
		if (Arguments.IsEmpty())
		{
			UE_LOG(LogRTSBlueprintRepair, Error, TEXT("RTS_BLUEPRINT_REPAIR usage: <BlueprintPath> [<BlueprintPath> ...]"));
			return;
		}
		for (const FString& BlueprintPath : Arguments)
		{
			RepairBlueprint(BlueprintPath);
		}
	}

	FAutoConsoleCommand RemoveUserWorldContextPinsConsoleCommand(
		TEXT("RTS.Blueprint.RemoveUserWorldContextPins"),
		TEXT("Removes user inputs named __WorldContext from Blueprint function entries, then compiles and saves."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RemoveUserWorldContextPinsCommand));
}

#endif // WITH_EDITOR
