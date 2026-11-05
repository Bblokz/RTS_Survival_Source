// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only conversion of a tank Blueprint to add-on armour panels (see Docs/Units/Vehicles/AddOnArmorPanels.md).
// RTS.AddOnArmor.ConvertBlueprint Blueprint=<asset> [Hull=<panel hull mesh>] [HullComponent=Hull]
//   [Armor=Plate_SideLeft:30|Plate_SideLowerLeft:20|...] [-DryRun]
// Swaps the hull component's mesh, sets the armour value pin of every SetArmorOfPlateType node of the listed plates,
// adds the AddOnArmorPanels component when the Blueprint has none, then compiles and saves. -DryRun only lists the
// SetArmorOfPlateType nodes and the hull component.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "AddOnArmorPanelAuthoringShared.h"
#include "AddOnArmorPanelsComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAddOnArmorBlueprintConversion, Log, All);

namespace AddOnArmorPanelBlueprintConversion
{
	using namespace AddOnArmorPanelAuthoring;

	const FName SetArmorFunctionName = GET_FUNCTION_NAME_CHECKED(UArmorCalculation, SetArmorOfPlateType);
	const FName PlateTypePinName(TEXT("PlateType"));
	const FName ArmorValuePinName(TEXT("NewArmorValue"));
	const FName PanelsComponentName(TEXT("AddOnArmorPanels"));

	struct FConversionSettings
	{
		FString BlueprintPath;
		FString HullMeshPath;
		FName HullComponentName = TEXT("Hull");
		// Plate enumerator name (e.g. Plate_SideLeft) -> new structural armour value.
		TMap<FString, FString> ArmorValues;
		bool bDryRun = false;
	};

	FConversionSettings ParseSettings(const FString& Joined)
	{
		FConversionSettings Settings;
		FParse::Value(*Joined, TEXT("Blueprint="), Settings.BlueprintPath);
		FParse::Value(*Joined, TEXT("Hull="), Settings.HullMeshPath);
		FString HullComponent;
		if (FParse::Value(*Joined, TEXT("HullComponent="), HullComponent))
		{
			Settings.HullComponentName = FName(*HullComponent);
		}
		FString ArmorList;
		FParse::Value(*Joined, TEXT("Armor="), ArmorList);
		TArray<FString> Entries;
		ArmorList.ParseIntoArray(Entries, TEXT("|"));
		for (const FString& Entry : Entries)
		{
			FString Plate;
			FString Value;
			if (Entry.Split(TEXT(":"), &Plate, &Value))
			{
				Settings.ArmorValues.Add(Plate.TrimStartAndEnd(), Value.TrimStartAndEnd());
			}
		}
		Settings.bDryRun = FParse::Param(*Joined, TEXT("DryRun"));
		return Settings;
	}

	TArray<UK2Node_CallFunction*> FindSetArmorNodes(const UBlueprint& Blueprint)
	{
		TArray<UEdGraph*> Graphs;
		Blueprint.GetAllGraphs(Graphs);
		TArray<UK2Node_CallFunction*> Nodes;
		for (const UEdGraph* const Graph : Graphs)
		{
			TArray<UK2Node_CallFunction*> GraphNodes;
			Graph->GetNodesOfClass(GraphNodes);
			for (UK2Node_CallFunction* const Node : GraphNodes)
			{
				if (Node->FunctionReference.GetMemberName() == SetArmorFunctionName)
				{
					Nodes.Add(Node);
				}
			}
		}
		return Nodes;
	}

	/** @return The plate enumerator name a node sets, or empty when its plate pin is wired instead of a literal. */
	FString GetNodePlateName(const UK2Node_CallFunction& Node)
	{
		const UEdGraphPin* const PlatePin = Node.FindPin(PlateTypePinName);
		if (PlatePin == nullptr || PlatePin->LinkedTo.Num() > 0)
		{
			return FString();
		}
		return PlatePin->DefaultValue;
	}

	/** @return The number of SetArmorOfPlateType nodes whose armour value changed; reports plates it could not set. */
	int32 ApplyArmorValues(const UBlueprint& Blueprint, const FConversionSettings& Settings)
	{
		const UEdGraphSchema_K2* const Schema = GetDefault<UEdGraphSchema_K2>();
		TSet<FString> AppliedPlates;
		int32 ChangedNodes = 0;
		for (UK2Node_CallFunction* const Node : FindSetArmorNodes(Blueprint))
		{
			const FString PlateName = GetNodePlateName(*Node);
			UEdGraphPin* const ValuePin = Node->FindPin(ArmorValuePinName);
			const FString* const NewValue = Settings.ArmorValues.Find(PlateName);
			if (NewValue == nullptr || ValuePin == nullptr || ValuePin->LinkedTo.Num() > 0)
			{
				continue;
			}
			UE_LOG(LogRTSAddOnArmorBlueprintConversion, Display, TEXT("RTS_ADDON_PANEL_CONVERT armor %s %s -> %s in %s"),
			       *PlateName, *ValuePin->DefaultValue, **NewValue, *GetNameSafe(Node->GetGraph()));
			Schema->TrySetDefaultValue(*ValuePin, *NewValue);
			AppliedPlates.Add(PlateName);
			++ChangedNodes;
		}
		for (const TPair<FString, FString>& Requested : Settings.ArmorValues)
		{
			if (not AppliedPlates.Contains(Requested.Key))
			{
				UE_LOG(LogRTSAddOnArmorBlueprintConversion, Error,
				       TEXT("RTS_ADDON_PANEL_CONVERT FAIL no literal SetArmorOfPlateType node for %s"), *Requested.Key);
			}
		}
		return ChangedNodes;
	}

	USCS_Node* FindScsNodeInHierarchy(const UBlueprint& Blueprint, const FName VariableName, bool& bOutIsInherited)
	{
		bOutIsInherited = false;
		for (const UBlueprint* Current = &Blueprint; Current != nullptr;
		     Current = UBlueprint::GetBlueprintFromClass(Current->ParentClass))
		{
			USCS_Node* const Node = Current->SimpleConstructionScript != nullptr
				                        ? Current->SimpleConstructionScript->FindSCSNode(VariableName)
				                        : nullptr;
			if (Node != nullptr)
			{
				return Node;
			}
			bOutIsInherited = true;
		}
		return nullptr;
	}

	/** @return The hull template this Blueprint owns: its own SCS template, or an override of an inherited one. */
	UStaticMeshComponent* GetEditableHullTemplate(UBlueprint& Blueprint, const FName HullComponentName)
	{
		bool bIsInherited = false;
		USCS_Node* const Node = FindScsNodeInHierarchy(Blueprint, HullComponentName, bIsInherited);
		if (Node == nullptr)
		{
			return nullptr;
		}
		if (not bIsInherited)
		{
			return Cast<UStaticMeshComponent>(Node->ComponentTemplate);
		}
		UInheritableComponentHandler* const Handler = Blueprint.GetInheritableComponentHandler(true);
		const FComponentKey Key(Node);
		UActorComponent* const Override = Handler->GetOverridenComponentTemplate(Key);
		return Cast<UStaticMeshComponent>(Override != nullptr ? Override : Handler->CreateOverridenComponentTemplate(Key));
	}

	/** @return The hull template the Blueprint spawns: the nearest override of an inherited template, else the template. */
	const UStaticMeshComponent* GetEffectiveHullTemplate(UBlueprint& Blueprint, USCS_Node* const HullNode,
	                                                     const bool bIsInherited)
	{
		if (HullNode == nullptr)
		{
			return nullptr;
		}
		const FComponentKey Key(HullNode);
		const UObject* const DefiningBlueprint = Key.GetComponentOwner()->ClassGeneratedBy;
		for (UBlueprint* Current = bIsInherited ? &Blueprint : nullptr; Current != nullptr && Current != DefiningBlueprint;
		     Current = UBlueprint::GetBlueprintFromClass(Current->ParentClass))
		{
			const UInheritableComponentHandler* const Handler = Current->GetInheritableComponentHandler(false);
			const UActorComponent* const Override = Handler != nullptr ? Handler->GetOverridenComponentTemplate(Key) : nullptr;
			if (Override != nullptr)
			{
				return Cast<UStaticMeshComponent>(Override);
			}
		}
		return Cast<UStaticMeshComponent>(HullNode->ComponentTemplate);
	}

	bool HasPanelsComponent(const UBlueprint& Blueprint)
	{
		for (const UBlueprint* Current = &Blueprint; Current != nullptr;
		     Current = UBlueprint::GetBlueprintFromClass(Current->ParentClass))
		{
			if (Current->SimpleConstructionScript == nullptr)
			{
				continue;
			}
			for (const USCS_Node* const Node : Current->SimpleConstructionScript->GetAllNodes())
			{
				if (Node->ComponentClass != nullptr && Node->ComponentClass->IsChildOf<UAddOnArmorPanelsComponent>())
				{
					return true;
				}
			}
		}
		return false;
	}

	void AddPanelsComponent(const UBlueprint& Blueprint)
	{
		if (HasPanelsComponent(Blueprint))
		{
			UE_LOG(LogRTSAddOnArmorBlueprintConversion, Display, TEXT("RTS_ADDON_PANEL_CONVERT panels component present"));
			return;
		}
		USimpleConstructionScript* const Construction = Blueprint.SimpleConstructionScript;
		USCS_Node* const Node = Construction->CreateNode(UAddOnArmorPanelsComponent::StaticClass(), PanelsComponentName);
		Construction->AddNode(Node);
		UE_LOG(LogRTSAddOnArmorBlueprintConversion, Display, TEXT("RTS_ADDON_PANEL_CONVERT added %s"),
		       *Node->GetVariableName().ToString());
	}

	void LogBlueprintState(UBlueprint& Blueprint, const FConversionSettings& Settings)
	{
		for (const UK2Node_CallFunction* const Node : FindSetArmorNodes(Blueprint))
		{
			const UEdGraphPin* const ValuePin = Node->FindPin(ArmorValuePinName);
			UE_LOG(LogRTSAddOnArmorBlueprintConversion, Display, TEXT("RTS_ADDON_PANEL_CONVERT node graph=%s plate=%s value=%s"),
			       *GetNameSafe(Node->GetGraph()), *GetNodePlateName(*Node),
			       ValuePin != nullptr && ValuePin->LinkedTo.Num() == 0 ? *ValuePin->DefaultValue : TEXT("(wired)"));
		}
		bool bIsInherited = false;
		USCS_Node* const HullNode = FindScsNodeInHierarchy(Blueprint, Settings.HullComponentName, bIsInherited);
		const UStaticMeshComponent* const HullTemplate = GetEffectiveHullTemplate(Blueprint, HullNode, bIsInherited);
		UE_LOG(LogRTSAddOnArmorBlueprintConversion, Display,
		       TEXT("RTS_ADDON_PANEL_CONVERT hull component=%s found=%d inherited=%d mesh=%s panels_component=%d"),
		       *Settings.HullComponentName.ToString(), HullNode != nullptr ? 1 : 0, bIsInherited ? 1 : 0,
		       HullTemplate != nullptr ? *GetPathNameSafe(HullTemplate->GetStaticMesh()) : TEXT("none"),
		       HasPanelsComponent(Blueprint) ? 1 : 0);
	}

	bool SwapHullMesh(UBlueprint& Blueprint, const FConversionSettings& Settings)
	{
		if (Settings.HullMeshPath.IsEmpty())
		{
			return true;
		}
		UStaticMesh* const HullMesh = LoadObject<UStaticMesh>(nullptr, *Settings.HullMeshPath);
		UStaticMeshComponent* const HullTemplate = GetEditableHullTemplate(Blueprint, Settings.HullComponentName);
		if (HullMesh == nullptr || HullTemplate == nullptr)
		{
			UE_LOG(LogRTSAddOnArmorBlueprintConversion, Error,
			       TEXT("RTS_ADDON_PANEL_CONVERT FAIL hull mesh=%d component=%d"), HullMesh != nullptr ? 1 : 0,
			       HullTemplate != nullptr ? 1 : 0);
			return false;
		}
		UE_LOG(LogRTSAddOnArmorBlueprintConversion, Display, TEXT("RTS_ADDON_PANEL_CONVERT hull %s -> %s"),
		       *GetPathNameSafe(HullTemplate->GetStaticMesh()), *GetPathNameSafe(HullMesh));
		HullTemplate->Modify();
		HullTemplate->SetStaticMesh(HullMesh);
		return true;
	}

	bool CompileAndSave(UBlueprint& Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		if (Blueprint.Status == BS_Error)
		{
			UE_LOG(LogRTSAddOnArmorBlueprintConversion, Error, TEXT("RTS_ADDON_PANEL_CONVERT FAIL compile errors, not saved"));
			return false;
		}
		return SaveAsset(Blueprint);
	}

	void ConvertBlueprint(const TArray<FString>& Args)
	{
		const FConversionSettings Settings = ParseSettings(FString::Join(Args, TEXT(" ")));
		UBlueprint* const Blueprint = LoadObject<UBlueprint>(nullptr, *Settings.BlueprintPath);
		if (Blueprint == nullptr || Blueprint->SimpleConstructionScript == nullptr)
		{
			UE_LOG(LogRTSAddOnArmorBlueprintConversion, Error, TEXT("RTS_ADDON_PANEL_CONVERT FAIL no actor Blueprint %s"),
			       *Settings.BlueprintPath);
			return;
		}
		LogBlueprintState(*Blueprint, Settings);
		if (Settings.bDryRun)
		{
			UE_LOG(LogRTSAddOnArmorBlueprintConversion, Display, TEXT("RTS_ADDON_PANEL_CONVERT RESULT DRYRUN"));
			return;
		}
		Blueprint->Modify();
		const int32 ChangedArmorNodes = ApplyArmorValues(*Blueprint, Settings);
		const bool bHullSwapped = SwapHullMesh(*Blueprint, Settings);
		AddPanelsComponent(*Blueprint);
		const bool bSaved = bHullSwapped && CompileAndSave(*Blueprint);
		LogBlueprintState(*Blueprint, Settings);
		UE_LOG(LogRTSAddOnArmorBlueprintConversion, Display, TEXT("RTS_ADDON_PANEL_CONVERT RESULT %s armor_nodes=%d"),
		       bSaved ? TEXT("SAVED") : TEXT("FAIL"), ChangedArmorNodes);
	}

	FAutoConsoleCommand ConvertBlueprintCommand(
		TEXT("RTS.AddOnArmor.ConvertBlueprint"),
		TEXT("Converts a tank Blueprint to add-on armour panels. Args: Blueprint= [Hull=] [HullComponent=Hull] "
			"[Armor=Plate:Value|...] [-DryRun]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ConvertBlueprint));
}

#endif // WITH_EDITOR
