#include "RTS_Survival/Procedural/CustomNodes/WorldMissionContext/PCGWorldMissionContextData.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Engine/World.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"

#define LOCTEXT_NAMESPACE "PCGWorldMissionContextData"

namespace WorldMissionContextPCG
{
	const FName EnemyObjectTypeAttributeName = TEXT("EnemyObjectType");
	const FName TotalStrengthPercentageAttributeName = TEXT("TotalStrengthPercentage");

	bool TryGetWorldMissionContext(FPCGContext* Context, FWorldMissionContext& OutWorldMissionContext)
	{
		OutWorldMissionContext = FWorldMissionContext();
		if (Context == nullptr)
		{
			return false;
		}

		UPCGComponent* SourceComponent = Context->SourceComponent.Get();
		if (not IsValid(SourceComponent))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context,
				LOCTEXT("InvalidSourceComponent", "World mission context PCG node has no valid source component."));
			return false;
		}

		UWorld* World = SourceComponent->GetWorld();
		if (not IsValid(World))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context,
				LOCTEXT("InvalidWorld", "World mission context PCG node could not resolve its world."));
			return false;
		}

		const URTSGameInstance* RTSGameInstance = World->GetGameInstance<URTSGameInstance>();
		if (not IsValid(RTSGameInstance))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context,
				LOCTEXT("InvalidGameInstance", "World mission context PCG node could not resolve URTSGameInstance."));
			return false;
		}

		OutWorldMissionContext = RTSGameInstance->GetWorldMissionContext();
		if (not OutWorldMissionContext.GetIsValid())
		{
			PCGE_LOG_C(Error, GraphAndLog, Context,
				LOCTEXT("InvalidMissionContext", "No valid world mission context is loaded in URTSGameInstance."));
			return false;
		}

		return true;
	}

	template <typename AttributeType>
	bool AddAttributeSetOutput(FPCGContext* Context,
	                           const FName AttributeName,
	                           const AttributeType AttributeValue)
	{
		UPCGParamData* OutputParamData = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
		if (not IsValid(OutputParamData) || not IsValid(OutputParamData->Metadata))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context,
				LOCTEXT("InvalidOutputParamData", "Failed to allocate world mission context PCG output data."));
			return false;
		}

		FPCGMetadataAttribute<AttributeType>* OutputAttribute =
			OutputParamData->Metadata->CreateAttribute<AttributeType>(
				AttributeName,
				AttributeValue,
				false,
				false);
		if (OutputAttribute == nullptr)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context,
				LOCTEXT("AttributeCreationFailed", "Failed to create world mission context PCG attribute."));
			return false;
		}

		OutputParamData->Metadata->AddEntry();
		FPCGTaggedData& OutputData = Context->OutputData.TaggedData.Emplace_GetRef();
		OutputData.Data = OutputParamData;
		OutputData.Pin = PCGPinConstants::DefaultOutputLabel;
		return true;
	}
}

#if WITH_EDITOR
FName UPCGGetWorldMissionEnemyObjectSettings::GetDefaultNodeName() const
{
	return TEXT("GetWorldMissionEnemyObject");
}

FText UPCGGetWorldMissionEnemyObjectSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("EnemyObjectNodeTitle", "Get World Mission Enemy Object");
}

FText UPCGGetWorldMissionEnemyObjectSettings::GetNodeTooltipText() const
{
	return LOCTEXT(
		"EnemyObjectNodeTooltip",
		"Outputs the loaded EMapEnemyItem value as the int64 EnemyObjectType attribute for an enum PCG Switch.");
}
#endif

FPCGElementPtr UPCGGetWorldMissionEnemyObjectSettings::CreateElement() const
{
	return MakeShared<FPCGGetWorldMissionEnemyObjectElement>();
}

TArray<FPCGPinProperties> UPCGGetWorldMissionEnemyObjectSettings::InputPinProperties() const
{
	return {};
}

TArray<FPCGPinProperties> UPCGGetWorldMissionEnemyObjectSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);
	return Pins;
}

bool FPCGGetWorldMissionEnemyObjectElement::ExecuteInternal(FPCGContext* Context) const
{
	FWorldMissionContext WorldMissionContext;
	if (not WorldMissionContextPCG::TryGetWorldMissionContext(Context, WorldMissionContext))
	{
		return true;
	}

	if (WorldMissionContext.OperationType != EMapItemType::EnemyItem)
	{
		PCGE_LOG_C(Error, GraphAndLog, Context,
			LOCTEXT("ContextIsNotEnemyObject", "The loaded world mission context is not an enemy-object operation."));
		return true;
	}

	WorldMissionContextPCG::AddAttributeSetOutput<int64>(
		Context,
		WorldMissionContextPCG::EnemyObjectTypeAttributeName,
		static_cast<int64>(WorldMissionContext.EnemyObjectType));
	return true;
}

#if WITH_EDITOR
FName UPCGGetWorldMissionStrengthSettings::GetDefaultNodeName() const
{
	return TEXT("GetWorldMissionStrength");
}

FText UPCGGetWorldMissionStrengthSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("StrengthNodeTitle", "Get World Mission Strength");
}

FText UPCGGetWorldMissionStrengthSettings::GetNodeTooltipText() const
{
	return LOCTEXT(
		"StrengthNodeTooltip",
		"Outputs the signed sum of all loaded mission strength contributions as TotalStrengthPercentage.");
}
#endif

FPCGElementPtr UPCGGetWorldMissionStrengthSettings::CreateElement() const
{
	return MakeShared<FPCGGetWorldMissionStrengthElement>();
}

TArray<FPCGPinProperties> UPCGGetWorldMissionStrengthSettings::InputPinProperties() const
{
	return {};
}

TArray<FPCGPinProperties> UPCGGetWorldMissionStrengthSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);
	return Pins;
}

bool FPCGGetWorldMissionStrengthElement::ExecuteInternal(FPCGContext* Context) const
{
	FWorldMissionContext WorldMissionContext;
	if (not WorldMissionContextPCG::TryGetWorldMissionContext(Context, WorldMissionContext))
	{
		return true;
	}

	WorldMissionContextPCG::AddAttributeSetOutput<int32>(
		Context,
		WorldMissionContextPCG::TotalStrengthPercentageAttributeName,
		WorldMissionContext.GetTotalStrengthPercentage());
	return true;
}

#undef LOCTEXT_NAMESPACE
