// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only: adds unit-scale armour boxes to a tank Blueprint's armour setup (see Docs/Units/Vehicles/AddOnArmorPanels.md,
// "Skirt boxes"). The legacy scaled front/rear boxes swallow side shots at the skirts, so the add-on zone never takes
// damage; a unit-scale box is tested before every scaled box, so a literal side box over the skirts fixes that.
// RTS.AddOnArmor.AddArmorBoxes Blueprint=<asset> [Graph=SetupArmor]
//   Boxes=<Plate>:<Value>:<MinX>/<MinY>/<MinZ>/<MaxX>/<MaxY>/<MaxZ>|...  [-DryRun]
// Each box becomes a literal Make ArmorSettings node (identity transform, box in hull-mesh space) on a new pin of the
// Make Array that already collects the graph's Make ArmorSettings nodes. A box that is already there is skipped, so the
// command can run again. Compiles and saves the Blueprint.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "AddOnArmorPanelAuthoringShared.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAddOnArmorBoxes, Log, All);

namespace AddOnArmorPanelArmorBoxes
{
	using namespace AddOnArmorPanelAuthoring;

	const FName ArmorTypePinName(GET_MEMBER_NAME_CHECKED(FArmorSettings, ArmorType));
	const FName ArmorValuePinName(GET_MEMBER_NAME_CHECKED(FArmorSettings, ArmorValue));
	const FName ArmorBoxPinName(GET_MEMBER_NAME_CHECKED(FArmorSettings, ArmorBox));
	const FName ArmorBoxTransformPinName(GET_MEMBER_NAME_CHECKED(FArmorSettings, ArmorBoxTransform));
	const FName AddOnArmorValuePinName(GET_MEMBER_NAME_CHECKED(FArmorSettings, AddOnArmorValue));
	constexpr int32 BoxCoordinateCount = 6;
	constexpr int32 NodeSpacingY = 260;
	constexpr int32 NodeOffsetX = -420;
	const TCHAR* const IdentityTransformText = TEXT("0.000000,0.000000,0.000000|0.000000,0.000000,0.000000|1.000000,1.000000,1.000000");

	struct FBoxRequest
	{
		FString PlateName;
		FString ArmorValue;
		FVector Min = FVector::ZeroVector;
		FVector Max = FVector::ZeroVector;

		FString GetBoxText() const
		{
			return FString::Printf(TEXT("(Min=(X=%f,Y=%f,Z=%f),Max=(X=%f,Y=%f,Z=%f),IsValid=True)"), Min.X, Min.Y, Min.Z,
			                       Max.X, Max.Y, Max.Z);
		}
	};

	struct FBoxSettings
	{
		FString BlueprintPath;
		FString GraphName = TEXT("SetupArmor");
		TArray<FBoxRequest> Boxes;
		bool bDryRun = false;
	};

	bool ParseBox(const FString& Entry, FBoxRequest& OutBox)
	{
		TArray<FString> Parts;
		Entry.ParseIntoArray(Parts, TEXT(":"));
		if (Parts.Num() != 3)
		{
			return false;
		}
		TArray<FString> Coordinates;
		Parts[2].ParseIntoArray(Coordinates, TEXT("/"));
		if (Coordinates.Num() != BoxCoordinateCount)
		{
			return false;
		}
		OutBox.PlateName = Parts[0].TrimStartAndEnd();
		OutBox.ArmorValue = Parts[1].TrimStartAndEnd();
		OutBox.Min = FVector(FCString::Atod(*Coordinates[0]), FCString::Atod(*Coordinates[1]), FCString::Atod(*Coordinates[2]));
		OutBox.Max = FVector(FCString::Atod(*Coordinates[3]), FCString::Atod(*Coordinates[4]), FCString::Atod(*Coordinates[5]));
		return true;
	}

	FBoxSettings ParseSettings(const FString& Joined)
	{
		FBoxSettings Settings;
		FParse::Value(*Joined, TEXT("Blueprint="), Settings.BlueprintPath);
		FParse::Value(*Joined, TEXT("Graph="), Settings.GraphName);
		FString BoxList;
		FParse::Value(*Joined, TEXT("Boxes="), BoxList, false);
		TArray<FString> Entries;
		BoxList.ParseIntoArray(Entries, TEXT("|"));
		for (const FString& Entry : Entries)
		{
			FBoxRequest Box;
			if (ParseBox(Entry, Box))
			{
				Settings.Boxes.Add(Box);
				continue;
			}
			UE_LOG(LogRTSAddOnArmorBoxes, Error, TEXT("RTS_ADDON_ARMOR_BOXES FAIL bad box entry %s"), *Entry);
		}
		Settings.bDryRun = FParse::Param(*Joined, TEXT("DryRun"));
		return Settings;
	}

	UEdGraph* FindGraph(const UBlueprint& Blueprint, const FString& GraphName)
	{
		TArray<UEdGraph*> Graphs;
		Blueprint.GetAllGraphs(Graphs);
		for (UEdGraph* const Graph : Graphs)
		{
			if (Graph != nullptr && Graph->GetName() == GraphName)
			{
				return Graph;
			}
		}
		return nullptr;
	}

	UEdGraphPin* FindStructOutputPin(const UK2Node_MakeStruct& Node)
	{
		for (UEdGraphPin* const Pin : Node.Pins)
		{
			if (Pin != nullptr && Pin->Direction == EGPD_Output)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	bool IsArmorSettingsNode(const UEdGraphNode* const Node)
	{
		const UK2Node_MakeStruct* const MakeStruct = Cast<UK2Node_MakeStruct>(Node);
		return MakeStruct != nullptr && MakeStruct->StructType == FArmorSettings::StaticStruct();
	}

	/** @return The Make Array whose inputs are linked to the most Make ArmorSettings nodes of the graph. */
	UK2Node_MakeArray* FindArmorSettingsArray(const UEdGraph& Graph)
	{
		TArray<UK2Node_MakeArray*> Arrays;
		Graph.GetNodesOfClass(Arrays);
		UK2Node_MakeArray* Best = nullptr;
		int32 BestCount = 0;
		for (UK2Node_MakeArray* const ArrayNode : Arrays)
		{
			int32 Count = 0;
			for (const UEdGraphPin* const Pin : ArrayNode->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Input)
				{
					continue;
				}
				for (const UEdGraphPin* const Linked : Pin->LinkedTo)
				{
					Count += Linked != nullptr && IsArmorSettingsNode(Linked->GetOwningNode()) ? 1 : 0;
				}
			}
			if (Count > BestCount)
			{
				Best = ArrayNode;
				BestCount = Count;
			}
		}
		return Best;
	}

	bool PinDefaultIs(const UK2Node_MakeStruct& Node, const FName PinName, const FString& Expected)
	{
		const UEdGraphPin* const Pin = Node.FindPin(PinName);
		return Pin != nullptr && Pin->LinkedTo.Num() == 0 && Pin->DefaultValue == Expected;
	}

	bool HasBox(const UEdGraph& Graph, const FBoxRequest& Box)
	{
		TArray<UK2Node_MakeStruct*> Nodes;
		Graph.GetNodesOfClass(Nodes);
		for (const UK2Node_MakeStruct* const Node : Nodes)
		{
			if (IsArmorSettingsNode(Node) && PinDefaultIs(*Node, ArmorTypePinName, Box.PlateName)
				&& PinDefaultIs(*Node, ArmorBoxPinName, Box.GetBoxText()))
			{
				return true;
			}
		}
		return false;
	}

	void SetPinDefault(UK2Node_MakeStruct& Node, const FName PinName, const FString& Value)
	{
		UEdGraphPin* const Pin = Node.FindPin(PinName);
		if (Pin == nullptr)
		{
			UE_LOG(LogRTSAddOnArmorBoxes, Error, TEXT("RTS_ADDON_ARMOR_BOXES FAIL no pin %s"), *PinName.ToString());
			return;
		}
		GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Pin, Value);
	}

	int32 CountInputPins(const UK2Node_MakeArray& ArrayNode)
	{
		int32 Count = 0;
		for (const UEdGraphPin* const Pin : ArrayNode.Pins)
		{
			Count += Pin != nullptr && Pin->Direction == EGPD_Input ? 1 : 0;
		}
		return Count;
	}

	UEdGraphPin* AddArrayInputPin(UK2Node_MakeArray& ArrayNode)
	{
		TSet<UEdGraphPin*> Before(ArrayNode.Pins);
		ArrayNode.AddInputPin();
		for (UEdGraphPin* const Pin : ArrayNode.Pins)
		{
			if (Pin != nullptr && Pin->Direction == EGPD_Input && not Before.Contains(Pin))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	/** @return True when the box node was created and linked into the armour array. */
	bool AddBoxNode(UEdGraph& Graph, UK2Node_MakeArray& ArrayNode, const FBoxRequest& Box, const int32 Index)
	{
		FGraphNodeCreator<UK2Node_MakeStruct> Creator(Graph);
		UK2Node_MakeStruct* const Node = Creator.CreateNode();
		Node->StructType = FArmorSettings::StaticStruct();
		Node->NodePosX = ArrayNode.NodePosX + NodeOffsetX;
		Node->NodePosY = ArrayNode.NodePosY + NodeSpacingY * (Index + 1);
		Creator.Finalize();
		SetPinDefault(*Node, ArmorBoxTransformPinName, IdentityTransformText);
		SetPinDefault(*Node, ArmorBoxPinName, Box.GetBoxText());
		SetPinDefault(*Node, ArmorValuePinName, Box.ArmorValue);
		SetPinDefault(*Node, ArmorTypePinName, Box.PlateName);
		SetPinDefault(*Node, AddOnArmorValuePinName, TEXT("0.0"));
		UEdGraphPin* const ArrayPin = AddArrayInputPin(ArrayNode);
		UEdGraphPin* const StructPin = FindStructOutputPin(*Node);
		if (ArrayPin == nullptr || StructPin == nullptr)
		{
			return false;
		}
		return GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(StructPin, ArrayPin);
	}

	void AddArmorBoxes(const TArray<FString>& Args)
	{
		const FBoxSettings Settings = ParseSettings(FString::Join(Args, TEXT(" ")));
		UBlueprint* const Blueprint = LoadObject<UBlueprint>(nullptr, *Settings.BlueprintPath);
		UEdGraph* const Graph = Blueprint != nullptr ? FindGraph(*Blueprint, Settings.GraphName) : nullptr;
		UK2Node_MakeArray* const ArrayNode = Graph != nullptr ? FindArmorSettingsArray(*Graph) : nullptr;
		if (ArrayNode == nullptr || Settings.Boxes.Num() == 0)
		{
			UE_LOG(LogRTSAddOnArmorBoxes, Error, TEXT("RTS_ADDON_ARMOR_BOXES FAIL blueprint=%d graph=%d array=%d boxes=%d"),
			       Blueprint != nullptr ? 1 : 0, Graph != nullptr ? 1 : 0, ArrayNode != nullptr ? 1 : 0, Settings.Boxes.Num());
			return;
		}
		UE_LOG(LogRTSAddOnArmorBoxes, Display, TEXT("RTS_ADDON_ARMOR_BOXES array %s inputs=%d"), *ArrayNode->GetName(),
		       CountInputPins(*ArrayNode));
		int32 Added = 0;
		Blueprint->Modify();
		Graph->Modify();
		for (const FBoxRequest& Box : Settings.Boxes)
		{
			if (HasBox(*Graph, Box))
			{
				UE_LOG(LogRTSAddOnArmorBoxes, Display, TEXT("RTS_ADDON_ARMOR_BOXES present %s %s"), *Box.PlateName, *Box.GetBoxText());
				continue;
			}
			UE_LOG(LogRTSAddOnArmorBoxes, Display, TEXT("RTS_ADDON_ARMOR_BOXES %s %s value=%s %s"),
			       Settings.bDryRun ? TEXT("would add") : TEXT("add"), *Box.PlateName, *Box.ArmorValue, *Box.GetBoxText());
			if (Settings.bDryRun)
			{
				continue;
			}
			if (not AddBoxNode(*Graph, *ArrayNode, Box, Added))
			{
				UE_LOG(LogRTSAddOnArmorBoxes, Error, TEXT("RTS_ADDON_ARMOR_BOXES FAIL could not link %s"), *Box.PlateName);
				return;
			}
			++Added;
		}
		if (Settings.bDryRun || Added == 0)
		{
			UE_LOG(LogRTSAddOnArmorBoxes, Display, TEXT("RTS_ADDON_ARMOR_BOXES RESULT %s added=0"),
			       Settings.bDryRun ? TEXT("DRYRUN") : TEXT("UNCHANGED"));
			return;
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		const bool bSaved = Blueprint->Status != BS_Error && SaveAsset(*Blueprint);
		UE_LOG(LogRTSAddOnArmorBoxes, Display, TEXT("RTS_ADDON_ARMOR_BOXES RESULT %s added=%d inputs=%d"),
		       bSaved ? TEXT("SAVED") : TEXT("FAIL"), Added, CountInputPins(*ArrayNode));
	}

	FAutoConsoleCommand AddArmorBoxesCommand(
		TEXT("RTS.AddOnArmor.AddArmorBoxes"),
		TEXT("Adds unit-scale literal armour boxes to a tank Blueprint's armour setup. Args: Blueprint= [Graph=SetupArmor] "
			"Boxes=Plate:Value:MinX/MinY/MinZ/MaxX/MaxY/MaxZ|... [-DryRun]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&AddArmorBoxes));
}

#endif // WITH_EDITOR
