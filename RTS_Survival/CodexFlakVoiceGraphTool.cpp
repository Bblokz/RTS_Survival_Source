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

	static FAutoConsoleCommand DumpCommand(
		TEXT("RTS.Codex.DumpFlakVoiceGraphs"),
		TEXT("Dump the Flak and Scavenger voice setup Blueprint graphs."),
		FConsoleCommandDelegate::CreateStatic(&DumpFlakVoiceGraphs));
}

#endif
