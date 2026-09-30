#if WITH_EDITOR

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeMap.h"
#include "K2Node_MakeStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"
#include "RTS_Survival/Audio/RTSVoiceLines/RTSVoicelines.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

namespace CodexFlakVoiceGraphTool
{
	static UEdGraph* FindFunctionGraph(UBlueprint* Blueprint, const FName GraphName)
	{
		if (Blueprint == nullptr)
		{
			return nullptr;
		}

		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph != nullptr && Graph->GetFName() == GraphName)
			{
				return Graph;
			}
		}
		return nullptr;
	}

	static FString DescribePinType(const FEdGraphPinType& PinType)
	{
		const FString SubCategoryObject = PinType.PinSubCategoryObject.IsValid()
			? PinType.PinSubCategoryObject->GetPathName()
			: TEXT("None");
		return FString::Printf(
			TEXT("category=%s subcategory=%s object=%s container=%d"),
			*PinType.PinCategory.ToString(),
			*PinType.PinSubCategory.ToString(),
			*SubCategoryObject,
			static_cast<int32>(PinType.ContainerType));
	}

	static void DumpGraph(const TCHAR* GraphName)
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(
			nullptr,
			TEXT("/Game/RTS_Survival/Blueprints/Player/RTS_BP_PlayerController.RTS_BP_PlayerController"));
		UEdGraph* Graph = FindFunctionGraph(Blueprint, FName(GraphName));
		if (Graph == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("CODEX_FLAK_DUMP: graph missing: %s"), GraphName);
			return;
		}

		UE_LOG(LogTemp, Display, TEXT("CODEX_FLAK_DUMP_BEGIN graph=%s nodes=%d"), GraphName, Graph->Nodes.Num());
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node == nullptr)
			{
				continue;
			}

			FString Extra;
			if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
			{
				Extra = FString::Printf(TEXT(" function=%s"), *Call->FunctionReference.GetMemberName().ToString());
			}
			else if (const UK2Node_MakeStruct* MakeStruct = Cast<UK2Node_MakeStruct>(Node))
			{
				Extra = FString::Printf(
					TEXT(" struct=%s"),
					MakeStruct->StructType ? *MakeStruct->StructType->GetPathName() : TEXT("None"));
			}
			else if (const UK2Node_MakeArray* MakeArray = Cast<UK2Node_MakeArray>(Node))
			{
				Extra = FString::Printf(TEXT(" num_inputs=%d"), MakeArray->NumInputs);
			}
			else if (const UK2Node_MakeMap* MakeMap = Cast<UK2Node_MakeMap>(Node))
			{
				Extra = FString::Printf(TEXT(" num_inputs=%d"), MakeMap->NumInputs);
			}

			UE_LOG(
				LogTemp,
				Display,
				TEXT("CODEX_FLAK_NODE graph=%s name=%s class=%s pos=(%d,%d) comment=\"%s\"%s"),
				GraphName,
				*Node->GetName(),
				*Node->GetClass()->GetName(),
				Node->NodePosX,
				Node->NodePosY,
				*Node->NodeComment.ReplaceCharWithEscapedChar(),
				*Extra);

			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin == nullptr)
				{
					continue;
				}

				FString Links;
				for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (LinkedPin != nullptr && LinkedPin->GetOwningNode() != nullptr)
					{
						if (!Links.IsEmpty())
						{
							Links += TEXT(",");
						}
						Links += LinkedPin->GetOwningNode()->GetName() + TEXT(".") + LinkedPin->PinName.ToString();
					}
				}

				UE_LOG(
					LogTemp,
					Display,
					TEXT("CODEX_FLAK_PIN node=%s name=%s dir=%d %s default=\"%s\" object=%s links=[%s]"),
					*Node->GetName(),
					*Pin->PinName.ToString(),
					static_cast<int32>(Pin->Direction),
					*DescribePinType(Pin->PinType),
					*Pin->DefaultValue.ReplaceCharWithEscapedChar(),
					Pin->DefaultObject ? *Pin->DefaultObject->GetPathName() : TEXT("None"),
					*Links);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("CODEX_FLAK_DUMP_END graph=%s"), GraphName);
	}

	static void DumpFlakVoiceGraphs()
	{
		DumpGraph(TEXT("SetupFlakVoiceLines"));
		DumpGraph(TEXT("SetupScavengerVoiceLines"));
	}

	static FString SoundPath(const FString& Folder, const FString& Stem, const int32 Index)
	{
		return FString::Printf(
			TEXT("/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Buildings/Flak/%s/%s_%d.%s_%d"),
			*Folder,
			*Stem,
			Index,
			*Stem,
			Index);
	}

	static TArray<FString> SoundSeries(
		const FString& Folder,
		const FString& Stem,
		const int32 Count)
	{
		TArray<FString> Paths;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Paths.Add(SoundPath(Folder, Stem, Index));
		}
		return Paths;
	}

	static TMap<FString, TArray<FString>> BuildFlakVoiceAssignments()
	{
		const TArray<FString> Confirm = SoundSeries(TEXT("confirm"), TEXT("S_Flak_Confirm"), 9);
		const TArray<FString> ConfirmStressed = SoundSeries(
			TEXT("confirmstressed"), TEXT("S_Flak_ConfirmStressed"), 7);
		const TArray<FString> Fire = SoundSeries(TEXT("Fire"), TEXT("S_Flak_Fire"), 7);

		TMap<FString, TArray<FString>> Assignments;
		Assignments.Add(TEXT("Select"), SoundSeries(TEXT("Selecction"), TEXT("S_Flak_Selection"), 11));
		Assignments.Add(TEXT("SelectExcited"), SoundSeries(TEXT("SelectionExcited"), TEXT("S_Flak_SelectionExcited"), 6));
		Assignments.Add(TEXT("SelectStressed"), SoundSeries(TEXT("selecetionStressed"), TEXT("S_Flak_SelectionStressed"), 13));
		Assignments.Add(TEXT("SelectNeedRepairs"), ConfirmStressed);
		Assignments.Add(TEXT("SelectAnnoyed"), SoundSeries(TEXT("SelectionAnnoyed"), TEXT("S_Flak_SelectionAnnoyed"), 3));
		Assignments.Add(TEXT("Move"), SoundSeries(TEXT("Move"), TEXT("S_Flak_Move"), 15));
		Assignments.Add(TEXT("MoveStressed"), SoundSeries(TEXT("MoveStressed"), TEXT("S_Flak_MoveStressed"), 2));
		Assignments.Add(TEXT("ReverseMove"), Confirm);
		Assignments.Add(TEXT("ReverseMoveStressed"), ConfirmStressed);
		Assignments.Add(TEXT("Stop"), SoundSeries(TEXT("stop"), TEXT("S_Flak_Stop"), 4));
		Assignments.Add(TEXT("StopStressed"), SoundSeries(TEXT("stopstressed"), TEXT("S_Flak_StopStressed"), 3));
		Assignments.Add(TEXT("Attack"), SoundSeries(TEXT("Attack"), TEXT("S_Flak_Attack"), 9));
		Assignments.Add(TEXT("AttackExcited"), SoundSeries(TEXT("AttackEx"), TEXT("S_Flak_AttackExcited"), 5));
		Assignments.Add(TEXT("AttackStressed"), SoundSeries(TEXT("attackstressed"), TEXT("S_Flak_AttackStressed"), 7));
		Assignments.Add(TEXT("Harvest"), Confirm);
		Assignments.Add(TEXT("Rotation"), SoundSeries(TEXT("rotation"), TEXT("S_Flak_Rotate"), 4));
		Assignments.Add(TEXT("RotationStressed"), SoundSeries(TEXT("rotationstressed"), TEXT("S_Flak_RotateStressed"), 4));
		Assignments.Add(TEXT("Confirm"), Confirm);
		Assignments.Add(TEXT("ConfirmStressed"), ConfirmStressed);
		Assignments.Add(TEXT("CommandCompleted"), Confirm);
		Assignments.Add(TEXT("Fortified"), Confirm);
		Assignments.Add(TEXT("ShotConnected"), SoundSeries(TEXT("RoundPens"), TEXT("S_Flak_RoundPens"), 6));
		Assignments.Add(TEXT("ShotBounced"), SoundSeries(TEXT("RoundBounces"), TEXT("S_Flak_RoundBounces"), 5));
		Assignments.Add(TEXT("Fire"), Fire);
		Assignments.Add(TEXT("EnemyDestroyed"), SoundSeries(TEXT("EnemyDestroyed"), TEXT("S_Flak_EnemyDestroyed"), 6));
		Assignments.Add(TEXT("LostEngine"), ConfirmStressed);
		Assignments.Add(TEXT("LostGun"), ConfirmStressed);
		Assignments.Add(TEXT("LostTrack"), ConfirmStressed);
		Assignments.Add(TEXT("LoadAP"), { SoundPath(TEXT("LoadingShellTypes"), TEXT("S_Flak_LoadingShellTypes"), 0) });
		Assignments.Add(TEXT("LoadHE"), { SoundPath(TEXT("LoadingShellTypes"), TEXT("S_Flak_LoadingShellTypes"), 1) });
		Assignments.Add(TEXT("LoadHEAT"), { SoundPath(TEXT("LoadingShellTypes"), TEXT("S_Flak_LoadingShellTypes"), 2) });
		Assignments.Add(TEXT("LoadRadixite"), { SoundPath(TEXT("LoadingShellTypes"), TEXT("S_Flak_LoadingShellTypes"), 3) });
		Assignments.Add(TEXT("LoadAPCR"), { SoundPath(TEXT("LoadingShellTypes"), TEXT("S_Flak_LoadingShellTypes"), 4) });
		Assignments.Add(TEXT("NeedHelp"), ConfirmStressed);
		Assignments.Add(TEXT("UnitPromoted"), SoundSeries(TEXT("UnitPromoted"), TEXT("S_Flak_UnitPromoted"), 3));
		Assignments.Add(TEXT("FireExtraWeapons"), Fire);
		Assignments.Add(TEXT("UnitDies"), SoundSeries(TEXT("Death"), TEXT("S_Flak_UnitDies"), 6));
		Assignments.Add(TEXT("SquadUnitLost"), SoundSeries(TEXT("OnSquadUnitLost"), TEXT("S_Flak_OnSquadUnitLost"), 5));
		Assignments.Add(TEXT("SquadFullyReinforced"), SoundSeries(TEXT("FullyReinforced"), TEXT("S_Flak_FullyReinforced"), 3));
		Assignments.Add(TEXT("OnUnitTrained"), SoundSeries(TEXT("Reinforcements"), TEXT("S_Flak_Reinforcements"), 5));
		return Assignments;
	}

	static UK2Node_MakeStruct* FindUpstreamVoiceLineStruct(
		UEdGraphNode* StartNode,
		UEdGraphNode* MapNode)
	{
		TArray<UEdGraphNode*> Pending { StartNode };
		TSet<UEdGraphNode*> Visited;
		while (!Pending.IsEmpty())
		{
			UEdGraphNode* Node = Pending.Pop(EAllowShrinking::No);
			if (Node == nullptr || Node == MapNode || Visited.Contains(Node))
			{
				continue;
			}
			Visited.Add(Node);
			if (UK2Node_MakeStruct* StructNode = Cast<UK2Node_MakeStruct>(Node))
			{
				if (StructNode->StructType == FVoiceLineData::StaticStruct())
				{
					return StructNode;
				}
			}

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Input)
				{
					continue;
				}
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (LinkedPin != nullptr)
					{
						Pending.Add(LinkedPin->GetOwningNode());
					}
				}
			}
		}
		return nullptr;
	}

	static UK2Node_MakeArray* FindVoiceArray(UK2Node_MakeStruct* StructNode)
	{
		if (StructNode == nullptr)
		{
			return nullptr;
		}
		UEdGraphPin* VoiceLinesPin = StructNode->FindPin(TEXT("VoiceLines"), EGPD_Input);
		if (VoiceLinesPin == nullptr || VoiceLinesPin->LinkedTo.IsEmpty())
		{
			return nullptr;
		}
		return Cast<UK2Node_MakeArray>(VoiceLinesPin->LinkedTo[0]->GetOwningNode());
	}

	static bool SetArrayAssets(
		UK2Node_MakeArray* ArrayNode,
		const TArray<FString>& AssetPaths,
		const UEdGraphSchema* Schema,
		FString& OutError)
	{
		if (ArrayNode == nullptr || Schema == nullptr || AssetPaths.IsEmpty())
		{
			OutError = TEXT("invalid array node, schema, or empty assignment");
			return false;
		}

		ArrayNode->Modify();
		while (ArrayNode->NumInputs < AssetPaths.Num())
		{
			ArrayNode->AddInputPin();
		}
		while (ArrayNode->NumInputs > AssetPaths.Num())
		{
			UEdGraphPin* LastPin = ArrayNode->FindPin(ArrayNode->GetPinName(ArrayNode->NumInputs - 1), EGPD_Input);
			if (LastPin == nullptr)
			{
				OutError = TEXT("failed to find trailing input pin while trimming");
				return false;
			}
			ArrayNode->RemoveInputPin(LastPin);
		}

		for (int32 Index = 0; Index < AssetPaths.Num(); ++Index)
		{
			USoundBase* Sound = LoadObject<USoundBase>(nullptr, *AssetPaths[Index]);
			UEdGraphPin* InputPin = ArrayNode->FindPin(ArrayNode->GetPinName(Index), EGPD_Input);
			if (Sound == nullptr || InputPin == nullptr)
			{
				OutError = FString::Printf(TEXT("missing sound or pin for %s"), *AssetPaths[Index]);
				return false;
			}
			Schema->TrySetDefaultObject(*InputPin, Sound);
		}
		return true;
	}

	static bool ReplaceStructVoiceArray(
		UEdGraph* Graph,
		UK2Node_MakeStruct* StructNode,
		const TArray<FString>& AssetPaths,
		FString& OutError)
	{
		if (Graph == nullptr || StructNode == nullptr || AssetPaths.IsEmpty())
		{
			OutError = TEXT("graph, voice-line struct, or assets missing");
			return false;
		}

		const UEdGraphSchema* Schema = Graph->GetSchema();
		UEdGraphPin* VoiceLinesPin = StructNode->FindPin(TEXT("VoiceLines"), EGPD_Input);
		if (Schema == nullptr || VoiceLinesPin == nullptr)
		{
			OutError = TEXT("schema or VoiceLines pin missing");
			return false;
		}

		Schema->BreakPinLinks(*VoiceLinesPin, true);
		FGraphNodeCreator<UK2Node_MakeArray> ArrayCreator(*Graph);
		UK2Node_MakeArray* ArrayNode = ArrayCreator.CreateNode();
		ArrayNode->NumInputs = AssetPaths.Num();
		ArrayNode->NodePosX = StructNode->NodePosX - 320;
		ArrayNode->NodePosY = StructNode->NodePosY;
		ArrayCreator.Finalize();

		if (!Schema->TryCreateConnection(ArrayNode->GetOutputPin(), VoiceLinesPin))
		{
			OutError = TEXT("failed to connect replacement voice array");
			return false;
		}
		return SetArrayAssets(ArrayNode, AssetPaths, Schema, OutError);
	}

	static bool CreateMapValueNodes(
		UEdGraph* Graph,
		UK2Node_MakeMap* MapNode,
		UEdGraphPin* ValuePin,
		const TArray<FString>& AssetPaths,
		const int32 LayoutIndex,
		FString& OutError)
	{
		if (Graph == nullptr || MapNode == nullptr || ValuePin == nullptr)
		{
			OutError = TEXT("graph, map, or value pin missing");
			return false;
		}
		const UEdGraphSchema* Schema = Graph->GetSchema();

		FGraphNodeCreator<UK2Node_MakeStruct> StructCreator(*Graph);
		UK2Node_MakeStruct* StructNode = StructCreator.CreateNode();
		StructNode->StructType = FVoiceLineData::StaticStruct();
		StructNode->NodePosX = MapNode->NodePosX - 480;
		StructNode->NodePosY = MapNode->NodePosY + 4800 + LayoutIndex * 240;
		StructCreator.Finalize();

		FGraphNodeCreator<UK2Node_MakeArray> ArrayCreator(*Graph);
		UK2Node_MakeArray* ArrayNode = ArrayCreator.CreateNode();
		ArrayNode->NumInputs = AssetPaths.Num();
		ArrayNode->NodePosX = StructNode->NodePosX - 320;
		ArrayNode->NodePosY = StructNode->NodePosY;
		ArrayCreator.Finalize();

		UEdGraphPin* StructOutput = StructNode->FindPinChecked(TEXT("VoiceLineData"), EGPD_Output);
		UEdGraphPin* StructVoiceLines = StructNode->FindPinChecked(TEXT("VoiceLines"), EGPD_Input);
		if (!Schema->TryCreateConnection(StructOutput, ValuePin) ||
			!Schema->TryCreateConnection(ArrayNode->GetOutputPin(), StructVoiceLines))
		{
			OutError = TEXT("failed to connect new map entry nodes");
			return false;
		}

		return SetArrayAssets(ArrayNode, AssetPaths, Schema, OutError);
	}

	static bool AddMapEntry(
		UEdGraph* Graph,
		UK2Node_MakeMap* MapNode,
		const FString& VoiceType,
		const TArray<FString>& AssetPaths,
		const int32 LayoutIndex,
		FString& OutError)
	{
		if (Graph == nullptr || MapNode == nullptr)
		{
			OutError = TEXT("graph or map missing");
			return false;
		}
		const UEdGraphSchema* Schema = Graph->GetSchema();
		MapNode->AddInputPin();

		TArray<UEdGraphPin*> KeyPins;
		TArray<UEdGraphPin*> ValuePins;
		MapNode->GetKeyAndValuePins(KeyPins, ValuePins);
		if (KeyPins.IsEmpty() || KeyPins.Num() != ValuePins.Num())
		{
			OutError = TEXT("map pins invalid after adding entry");
			return false;
		}
		UEdGraphPin* KeyPin = KeyPins.Last();
		UEdGraphPin* ValuePin = ValuePins.Last();
		Schema->TrySetDefaultValue(*KeyPin, VoiceType);
		return CreateMapValueNodes(Graph, MapNode, ValuePin, AssetPaths, LayoutIndex, OutError);
	}

	static bool VerifyAssignments(
		UEdGraph* Graph,
		const TMap<FString, TArray<FString>>& Assignments,
		FString& OutError)
	{
		UK2Node_MakeMap* MapNode = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_MakeMap* Candidate = Cast<UK2Node_MakeMap>(Node))
			{
				MapNode = Candidate;
				break;
			}
		}
		if (MapNode == nullptr)
		{
			OutError = TEXT("MakeMap node missing");
			return false;
		}

		TArray<UEdGraphPin*> KeyPins;
		TArray<UEdGraphPin*> ValuePins;
		MapNode->GetKeyAndValuePins(KeyPins, ValuePins);
		int32 VerifiedEntries = 0;
		for (int32 Index = 0; Index < KeyPins.Num(); ++Index)
		{
			const FString VoiceType = KeyPins[Index]->DefaultValue;
			const TArray<FString>* ExpectedPaths = Assignments.Find(VoiceType);
			if (ExpectedPaths == nullptr)
			{
				OutError = FString::Printf(TEXT("unexpected or unmapped key %s"), *VoiceType);
				return false;
			}
			if (ValuePins[Index]->LinkedTo.IsEmpty())
			{
				OutError = FString::Printf(TEXT("unlinked map value %s"), *VoiceType);
				return false;
			}
			UK2Node_MakeStruct* StructNode = FindUpstreamVoiceLineStruct(
				ValuePins[Index]->LinkedTo[0]->GetOwningNode(), MapNode);
			UK2Node_MakeArray* ArrayNode = FindVoiceArray(StructNode);
			if (ArrayNode == nullptr)
			{
				OutError = FString::Printf(
					TEXT("array node missing for %s (struct=%s)"),
					*VoiceType,
					StructNode ? *StructNode->GetName() : TEXT("None"));
				return false;
			}
			if (ArrayNode->NumInputs != ExpectedPaths->Num())
			{
				OutError = FString::Printf(
					TEXT("array count mismatch for %s: actual=%d expected=%d node=%s"),
					*VoiceType,
					ArrayNode->NumInputs,
					ExpectedPaths->Num(),
					*ArrayNode->GetName());
				return false;
			}
			for (int32 AssetIndex = 0; AssetIndex < ExpectedPaths->Num(); ++AssetIndex)
			{
				UEdGraphPin* InputPin = ArrayNode->FindPin(ArrayNode->GetPinName(AssetIndex), EGPD_Input);
				if (InputPin == nullptr || InputPin->DefaultObject == nullptr ||
					InputPin->DefaultObject->GetPathName() != (*ExpectedPaths)[AssetIndex])
				{
					OutError = FString::Printf(TEXT("asset mismatch for %s[%d]"), *VoiceType, AssetIndex);
					return false;
				}
			}
			++VerifiedEntries;
		}

		if (VerifiedEntries != Assignments.Num())
		{
			OutError = FString::Printf(
				TEXT("entry count mismatch: verified=%d expected=%d"), VerifiedEntries, Assignments.Num());
			return false;
		}
		UE_LOG(LogTemp, Display, TEXT("CODEX_FLAK_VERIFY: entries=%d empty_pins=0"), VerifiedEntries);
		return true;
	}

	static void SetupFlakVoiceGraph()
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(
			nullptr,
			TEXT("/Game/RTS_Survival/Blueprints/Player/RTS_BP_PlayerController.RTS_BP_PlayerController"));
		UEdGraph* Graph = FindFunctionGraph(Blueprint, FName(TEXT("SetupFlakVoiceLines")));
		if (Blueprint == nullptr || Graph == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("CODEX_FLAK_SETUP: Blueprint or graph missing"));
			return;
		}

		UK2Node_MakeMap* MapNode = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_MakeMap* Candidate = Cast<UK2Node_MakeMap>(Node))
			{
				MapNode = Candidate;
				break;
			}
		}
		if (MapNode == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("CODEX_FLAK_SETUP: MakeMap node missing"));
			return;
		}

		Blueprint->Modify();
		Graph->Modify();
		const TMap<FString, TArray<FString>> Assignments = BuildFlakVoiceAssignments();
		TArray<UEdGraphPin*> KeyPins;
		TArray<UEdGraphPin*> ValuePins;
		MapNode->GetKeyAndValuePins(KeyPins, ValuePins);
		TSet<FString> ExistingKeys;
		FString Error;

		for (int32 Index = 0; Index < KeyPins.Num(); ++Index)
		{
			const FString VoiceType = KeyPins[Index]->DefaultValue;
			ExistingKeys.Add(VoiceType);
			const TArray<FString>* AssetPaths = Assignments.Find(VoiceType);
			if (AssetPaths == nullptr)
			{
				UE_LOG(LogTemp, Error, TEXT("CODEX_FLAK_SETUP: unmapped key %s"), *VoiceType);
				return;
			}
			if (ValuePins[Index]->LinkedTo.IsEmpty())
			{
				if (!CreateMapValueNodes(Graph, MapNode, ValuePins[Index], *AssetPaths, Index, Error))
				{
					UE_LOG(LogTemp, Error, TEXT("CODEX_FLAK_SETUP: reconnect %s failed: %s"), *VoiceType, *Error);
					return;
				}
				UE_LOG(LogTemp, Display, TEXT("CODEX_FLAK_RECONNECTED: %s=%d"), *VoiceType, AssetPaths->Num());
				continue;
			}
			UK2Node_MakeStruct* StructNode = FindUpstreamVoiceLineStruct(
				ValuePins[Index]->LinkedTo[0]->GetOwningNode(), MapNode);
			if (!ReplaceStructVoiceArray(Graph, StructNode, *AssetPaths, Error))
			{
				UE_LOG(LogTemp, Error, TEXT("CODEX_FLAK_SETUP: %s: %s"), *VoiceType, *Error);
				return;
			}
			UE_LOG(LogTemp, Display, TEXT("CODEX_FLAK_SET: %s=%d"), *VoiceType, AssetPaths->Num());
		}

		int32 AddedIndex = 0;
		for (const TPair<FString, TArray<FString>>& Pair : Assignments)
		{
			if (ExistingKeys.Contains(Pair.Key))
			{
				continue;
			}
			if (!AddMapEntry(Graph, MapNode, Pair.Key, Pair.Value, AddedIndex++, Error))
			{
				UE_LOG(LogTemp, Error, TEXT("CODEX_FLAK_SETUP: add %s failed: %s"), *Pair.Key, *Error);
				return;
			}
			UE_LOG(LogTemp, Display, TEXT("CODEX_FLAK_ADDED: %s=%d"), *Pair.Key, Pair.Value.Num());
		}

		TArray<UEdGraphNode*> OrphanArrays;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_MakeArray* ArrayNode = Cast<UK2Node_MakeArray>(Node))
			{
				if (ArrayNode->GetOutputPin() == nullptr || ArrayNode->GetOutputPin()->LinkedTo.IsEmpty())
				{
					OrphanArrays.Add(ArrayNode);
				}
			}
		}
		for (UEdGraphNode* OrphanArray : OrphanArrays)
		{
			Graph->RemoveNode(OrphanArray);
		}
		UE_LOG(LogTemp, Display, TEXT("CODEX_FLAK_TRIMMED_ORPHANS: arrays=%d"), OrphanArrays.Num());

		if (!VerifyAssignments(Graph, Assignments, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("CODEX_FLAK_SETUP: precompile verification failed: %s"), *Error);
			return;
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (Blueprint->Status == BS_Error)
		{
			UE_LOG(LogTemp, Error, TEXT("CODEX_FLAK_SETUP: Blueprint compilation failed"));
			return;
		}

		if (!VerifyAssignments(Graph, Assignments, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("CODEX_FLAK_SETUP: verification failed: %s"), *Error);
			return;
		}

		UPackage* Package = Blueprint->GetOutermost();
		FString OriginalFilename;
		if (!FPackageName::TryConvertLongPackageNameToFilename(
			Package->GetName(), OriginalFilename, FPackageName::GetAssetPackageExtension()))
		{
			UE_LOG(LogTemp, Error, TEXT("CODEX_FLAK_SETUP: package filename conversion failed"));
			return;
		}
		const FString Filename = FPaths::Combine(
			FPaths::ProjectSavedDir(), TEXT("CodexFlakController.uasset"));
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		SaveArgs.Error = GError;
		const bool bSaved = UPackage::SavePackage(Package, Blueprint, *Filename, SaveArgs);
		if (bSaved)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("CODEX_FLAK_SETUP_COMPLETE: entries=%d added=%d staged=true file=%s"),
				Assignments.Num(),
				AddedIndex,
				*Filename);
		}
		else
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("CODEX_FLAK_SETUP_COMPLETE: entries=%d added=%d staged=false file=%s"),
				Assignments.Num(),
				AddedIndex,
				*Filename);
		}
	}

	static void VerifyFlakVoiceGraph()
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(
			nullptr,
			TEXT("/Game/RTS_Survival/Blueprints/Player/RTS_BP_PlayerController.RTS_BP_PlayerController"));
		UEdGraph* Graph = FindFunctionGraph(Blueprint, FName(TEXT("SetupFlakVoiceLines")));
		if (Blueprint == nullptr || Graph == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("CODEX_FLAK_VERIFY_COMPLETE: Blueprint or graph missing"));
			return;
		}

		const TMap<FString, TArray<FString>> Assignments = BuildFlakVoiceAssignments();
		FString Error;
		if (!VerifyAssignments(Graph, Assignments, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("CODEX_FLAK_VERIFY_COMPLETE: failed: %s"), *Error);
			return;
		}
		UE_LOG(
			LogTemp,
			Display,
			TEXT("CODEX_FLAK_VERIFY_COMPLETE: entries=%d status=%d"),
			Assignments.Num(),
			static_cast<int32>(Blueprint->Status));
	}

	static FAutoConsoleCommand DumpCommand(
		TEXT("RTS.Codex.DumpFlakVoiceGraphs"),
		TEXT("Dump the Flak and Scavenger voice setup Blueprint graphs."),
		FConsoleCommandDelegate::CreateStatic(&DumpFlakVoiceGraphs));

	static FAutoConsoleCommand SetupCommand(
		TEXT("RTS.Codex.SetupFlakVoiceGraph"),
		TEXT("Populate and validate the Flak voice setup Blueprint graph."),
		FConsoleCommandDelegate::CreateStatic(&SetupFlakVoiceGraph));

	static FAutoConsoleCommand VerifyCommand(
		TEXT("RTS.Codex.VerifyFlakVoiceGraph"),
		TEXT("Reload and validate the Flak voice setup Blueprint graph."),
		FConsoleCommandDelegate::CreateStatic(&VerifyFlakVoiceGraph));
}

#endif
