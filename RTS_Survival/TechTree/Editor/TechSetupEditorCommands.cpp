// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only setup step for technologies. RTS.TechSetup.AddMakeMapEntries <Blueprint> <Graph> <Key=Value>...
// adds key/value pairs to the single Make Map node of a Blueprint function graph, compiles and saves the Blueprint.
// It exists because the controller's GetPlayerTechs function returns the ETechnology -> TEBP map as a Make Map
// literal, which Python and the UECP bridge cannot grow. Keys that are already in the map are left untouched, so
// running it twice is safe. Run it in a headless editor while no other editor has the Blueprint loaded
// (Source/Tools/TechSetup/run_tech_setup.sh).

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_MakeMap.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSTechSetup, Log, All);

namespace TechSetupEditorCommands
{
	UEdGraph* FindFunctionGraph(UBlueprint& Blueprint, const FString& GraphName)
	{
		for (UEdGraph* const Graph : Blueprint.FunctionGraphs)
		{
			if (IsValid(Graph) && Graph->GetName() == GraphName)
			{
				return Graph;
			}
		}
		return nullptr;
	}

	UK2Node_MakeMap* FindSingleMakeMapNode(UEdGraph& Graph)
	{
		TArray<UK2Node_MakeMap*> MakeMapNodes;
		Graph.GetNodesOfClass(MakeMapNodes);
		return MakeMapNodes.Num() == 1 ? MakeMapNodes[0] : nullptr;
	}

	// Input pins of a Make Map node alternate key, value, key, value...
	void CollectKeyValuePins(const UK2Node_MakeMap& MakeMapNode, TArray<UEdGraphPin*>& OutKeyPins,
	                         TArray<UEdGraphPin*>& OutValuePins)
	{
		TArray<UEdGraphPin*> InputPins;
		for (UEdGraphPin* const Pin : MakeMapNode.Pins)
		{
			if (Pin->Direction == EGPD_Input)
			{
				InputPins.Add(Pin);
			}
		}
		for (int32 PinIndex = 0; PinIndex + 1 < InputPins.Num(); PinIndex += 2)
		{
			OutKeyPins.Add(InputPins[PinIndex]);
			OutValuePins.Add(InputPins[PinIndex + 1]);
		}
	}

	bool AddEntry(UK2Node_MakeMap& MakeMapNode, const FString& Key, const FString& Value)
	{
		TArray<UEdGraphPin*> KeyPinsBefore;
		TArray<UEdGraphPin*> ValuePinsBefore;
		CollectKeyValuePins(MakeMapNode, KeyPinsBefore, ValuePinsBefore);
		for (const UEdGraphPin* const KeyPin : KeyPinsBefore)
		{
			if (KeyPin->DefaultValue == Key)
			{
				UE_LOG(LogRTSTechSetup, Display, TEXT("RTS_TECH_SETUP key %s already present"), *Key);
				return true;
			}
		}
		MakeMapNode.AddInputPin();
		TArray<UEdGraphPin*> KeyPins;
		TArray<UEdGraphPin*> ValuePins;
		CollectKeyValuePins(MakeMapNode, KeyPins, ValuePins);
		if (KeyPins.Num() != KeyPinsBefore.Num() + 1)
		{
			UE_LOG(LogRTSTechSetup, Error, TEXT("RTS_TECH_SETUP AddInputPin did not add a key/value pair"));
			return false;
		}
		const UEdGraphSchema_K2* const Schema = GetDefault<UEdGraphSchema_K2>();
		Schema->TrySetDefaultValue(*KeyPins.Last(), Key);
		Schema->TrySetDefaultValue(*ValuePins.Last(), Value);
		const bool bIsSet = KeyPins.Last()->DefaultValue == Key && not ValuePins.Last()->DefaultValue.IsEmpty();
		UE_LOG(LogRTSTechSetup, Display, TEXT("RTS_TECH_SETUP added %s -> %s (key pin '%s', value pin '%s')"), *Key,
		       *Value, *KeyPins.Last()->DefaultValue, *ValuePins.Last()->DefaultValue);
		return bIsSet;
	}

	void LogExistingEntries(const UK2Node_MakeMap& MakeMapNode)
	{
		TArray<UEdGraphPin*> KeyPins;
		TArray<UEdGraphPin*> ValuePins;
		CollectKeyValuePins(MakeMapNode, KeyPins, ValuePins);
		for (int32 EntryIndex = 0; EntryIndex < KeyPins.Num(); ++EntryIndex)
		{
			UE_LOG(LogRTSTechSetup, Display, TEXT("RTS_TECH_SETUP existing '%s' -> '%s'"),
			       *KeyPins[EntryIndex]->DefaultValue, *ValuePins[EntryIndex]->DefaultValue);
		}
	}

	void AddMakeMapEntries(const TArray<FString>& Args)
	{
		if (Args.Num() < 3)
		{
			UE_LOG(LogRTSTechSetup, Error,
			       TEXT("RTS_TECH_SETUP RESULT FAIL usage: <Blueprint path> <function graph> <Key=Value>..."));
			return;
		}
		UBlueprint* const Blueprint = LoadObject<UBlueprint>(nullptr, *Args[0]);
		UEdGraph* const Graph = IsValid(Blueprint) ? FindFunctionGraph(*Blueprint, Args[1]) : nullptr;
		UK2Node_MakeMap* const MakeMapNode = Graph ? FindSingleMakeMapNode(*Graph) : nullptr;
		if (not MakeMapNode)
		{
			UE_LOG(LogRTSTechSetup, Error, TEXT("RTS_TECH_SETUP RESULT FAIL no single Make Map node in %s %s"),
			       *Args[0], *Args[1]);
			return;
		}
		LogExistingEntries(*MakeMapNode);
		int32 AddedCount = 0;
		for (int32 ArgIndex = 2; ArgIndex < Args.Num(); ++ArgIndex)
		{
			FString Key;
			FString Value;
			if (not Args[ArgIndex].Split(TEXT("="), &Key, &Value))
			{
				UE_LOG(LogRTSTechSetup, Error, TEXT("RTS_TECH_SETUP cannot read pair %s"), *Args[ArgIndex]);
				continue;
			}
			AddedCount += AddEntry(*MakeMapNode, Key, Value) ? 1 : 0;
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		const bool bIsSaved = UEditorLoadingAndSavingUtils::SavePackages({Blueprint->GetPackage()}, false);
		UE_LOG(LogRTSTechSetup, Display, TEXT("RTS_TECH_SETUP RESULT %s entries ok=%d of %d saved=%d"),
		       bIsSaved ? TEXT("DONE") : TEXT("FAIL"), AddedCount, Args.Num() - 2, bIsSaved ? 1 : 0);
	}

	FAutoConsoleCommand AddMakeMapEntriesCommand(
		TEXT("RTS.TechSetup.AddMakeMapEntries"),
		TEXT("Adds Key=Value pairs to the Make Map node of a Blueprint function graph, compiles and saves it."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&AddMakeMapEntries));
}

#endif // WITH_EDITOR
