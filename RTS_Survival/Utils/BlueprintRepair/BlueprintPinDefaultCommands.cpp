// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only: sets literal pin defaults on Blueprint graph nodes, which Python cannot reach.
// Made for Blueprint duplicates whose graphs hold hard-coded assets (the metal resources set their mesh, collection
// and worth multiplier with literals in the construction script), so a duplicate can be retargeted without opening it.
// The console command RTS.Blueprint.SetPinDefaults <JsonFile> reads
//   [{"blueprint": "/Game/Path/BP", "edits": [{"graph": "UserConstructionScript", "node": "K2Node_CallFunction_8",
//      "pin": "NewMesh", "value": "/Game/Path/Mesh.Mesh"}]}]
// applies every edit, and only when all of them took, compiles and saves that Blueprint.
// Run it in a headless editor while no other editor has the Blueprint loaded.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSBlueprintPinDefaults, Log, All);

namespace RTSBlueprintPinDefaults
{
	UEdGraph* FindGraphByName(const UBlueprint& Blueprint, const FString& GraphName)
	{
		TArray<UEdGraph*> AllGraphs;
		Blueprint.GetAllGraphs(AllGraphs);
		for (UEdGraph* const Graph : AllGraphs)
		{
			if (IsValid(Graph) && Graph->GetName() == GraphName)
			{
				return Graph;
			}
		}
		return nullptr;
	}

	UEdGraphNode* FindNodeByName(const UEdGraph& Graph, const FString& NodeName)
	{
		for (UEdGraphNode* const Node : Graph.Nodes)
		{
			if (IsValid(Node) && Node->GetName() == NodeName)
			{
				return Node;
			}
		}
		return nullptr;
	}

	bool GetIsObjectPin(const UEdGraphPin& Pin)
	{
		const FName Category = Pin.PinType.PinCategory;
		return Category == UEdGraphSchema_K2::PC_Object || Category == UEdGraphSchema_K2::PC_Class
			|| Category == UEdGraphSchema_K2::PC_Interface;
	}

	/**
	 * @brief Hard object pins take a loaded object, every other pin takes its value as text.
	 * @param Pin Input pin of the node.
	 * @param Value Object path for object pins, else the literal as the Blueprint editor would show it.
	 * @return Whether the pin holds the requested default afterwards.
	 */
	bool SetPinDefault(UEdGraphPin& Pin, const FString& Value)
	{
		const UEdGraphSchema_K2* const Schema = GetDefault<UEdGraphSchema_K2>();
		if (not GetIsObjectPin(Pin))
		{
			Schema->TrySetDefaultValue(Pin, Value);
			return Pin.GetDefaultAsString() == Value;
		}
		UObject* const NewDefaultObject = StaticLoadObject(UObject::StaticClass(), nullptr, *Value);
		if (not IsValid(NewDefaultObject))
		{
			UE_LOG(LogRTSBlueprintPinDefaults, Error, TEXT("RTS_PIN_DEFAULT could not load object %s"), *Value);
			return false;
		}
		Schema->TrySetDefaultObject(Pin, NewDefaultObject);
		return Pin.DefaultObject == NewDefaultObject;
	}

	bool ApplyEdit(const UBlueprint& Blueprint, const FJsonObject& Edit)
	{
		const FString GraphName = Edit.GetStringField(TEXT("graph"));
		const FString NodeName = Edit.GetStringField(TEXT("node"));
		const FString PinName = Edit.GetStringField(TEXT("pin"));
		const FString Value = Edit.GetStringField(TEXT("value"));
		const UEdGraph* const Graph = FindGraphByName(Blueprint, GraphName);
		UEdGraphNode* const Node = Graph != nullptr ? FindNodeByName(*Graph, NodeName) : nullptr;
		UEdGraphPin* const Pin = Node != nullptr ? Node->FindPin(FName(*PinName), EGPD_Input) : nullptr;
		if (Pin == nullptr)
		{
			UE_LOG(LogRTSBlueprintPinDefaults, Error, TEXT("RTS_PIN_DEFAULT missing graph=%d node=%d pin=%d for %s.%s.%s"),
			       Graph != nullptr, Node != nullptr, Pin != nullptr, *GraphName, *NodeName, *PinName);
			return false;
		}
		const FString OldValue = Pin->GetDefaultAsString();
		Node->Modify();
		const bool bSet = SetPinDefault(*Pin, Value);
		UE_LOG(LogRTSBlueprintPinDefaults, Display, TEXT("RTS_PIN_DEFAULT %s %s.%s.%s: '%s' -> '%s'"),
		       bSet ? TEXT("set") : TEXT("FAILED"), *GraphName, *NodeName, *PinName, *OldValue, *Pin->GetDefaultAsString());
		return bSet;
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

	void ApplyBlueprintEdits(const FJsonObject& BlueprintEdits)
	{
		const FString BlueprintPath = BlueprintEdits.GetStringField(TEXT("blueprint"));
		UBlueprint* const Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
		if (not IsValid(Blueprint))
		{
			UE_LOG(LogRTSBlueprintPinDefaults, Error, TEXT("RTS_PIN_DEFAULT not_a_blueprint %s"), *BlueprintPath);
			return;
		}
		const TArray<TSharedPtr<FJsonValue>>* Edits = nullptr;
		if (not BlueprintEdits.TryGetArrayField(TEXT("edits"), Edits) || Edits == nullptr)
		{
			UE_LOG(LogRTSBlueprintPinDefaults, Error, TEXT("RTS_PIN_DEFAULT no_edits %s"), *BlueprintPath);
			return;
		}
		int32 FailedCount = 0;
		for (const TSharedPtr<FJsonValue>& EditValue : *Edits)
		{
			const TSharedPtr<FJsonObject> Edit = EditValue.IsValid() ? EditValue->AsObject() : nullptr;
			const bool bApplied = Edit.IsValid() && ApplyEdit(*Blueprint, *Edit);
			FailedCount += bApplied ? 0 : 1;
		}
		if (FailedCount > 0)
		{
			UE_LOG(LogRTSBlueprintPinDefaults, Error, TEXT("RTS_PIN_DEFAULT RESULT %s failed=%d not saved"),
			       *BlueprintPath, FailedCount);
			return;
		}
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		const bool bCompiled = Blueprint->Status != BS_Error;
		const bool bSaved = bCompiled && SaveBlueprint(*Blueprint);
		UE_LOG(LogRTSBlueprintPinDefaults, Display, TEXT("RTS_PIN_DEFAULT RESULT %s edits=%d compiled=%d saved=%d"),
		       *BlueprintPath, Edits->Num(), bCompiled, bSaved);
	}

	void SetPinDefaultsCommand(const TArray<FString>& Arguments)
	{
		FString JsonText;
		if (Arguments.IsEmpty() || not FFileHelper::LoadFileToString(JsonText, *Arguments[0]))
		{
			UE_LOG(LogRTSBlueprintPinDefaults, Error, TEXT("RTS_PIN_DEFAULT usage: <JsonFile> (could not read it)"));
			return;
		}
		TArray<TSharedPtr<FJsonValue>> BlueprintEntries;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (not FJsonSerializer::Deserialize(Reader, BlueprintEntries))
		{
			UE_LOG(LogRTSBlueprintPinDefaults, Error, TEXT("RTS_PIN_DEFAULT %s is not a JSON array"), *Arguments[0]);
			return;
		}
		for (const TSharedPtr<FJsonValue>& Entry : BlueprintEntries)
		{
			const TSharedPtr<FJsonObject> BlueprintEdits = Entry.IsValid() ? Entry->AsObject() : nullptr;
			if (BlueprintEdits.IsValid())
			{
				ApplyBlueprintEdits(*BlueprintEdits);
			}
		}
	}

	FAutoConsoleCommand SetPinDefaultsConsoleCommand(
		TEXT("RTS.Blueprint.SetPinDefaults"),
		TEXT("Sets literal pin defaults on Blueprint graph nodes from a JSON file, then compiles and saves. Args: <JsonFile>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SetPinDefaultsCommand));
}

#endif // WITH_EDITOR
