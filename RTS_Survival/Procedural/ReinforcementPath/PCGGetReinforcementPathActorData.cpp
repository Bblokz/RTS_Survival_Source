// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#include "PCGGetReinforcementPathActorData.h"

#include "ReinforcementPathActor.h"

#include "PCGComponent.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "PCGGetReinforcementPathActorData"

namespace ReinforcementPathActorDataConstants
{
	const FName OutputPin = TEXT("ReinforcementStarts");
}

namespace
{
	TArray<AReinforcementPathActor*> FindReinforcementPathActors(UWorld& World)
	{
		TArray<AReinforcementPathActor*> Actors;
		for (TActorIterator<AReinforcementPathActor> ActorIterator(&World); ActorIterator; ++ActorIterator)
		{
			AReinforcementPathActor* Actor = *ActorIterator;
			if (not IsValid(Actor))
			{
				continue;
			}

			Actors.Add(Actor);
		}

		Actors.Sort([](const AReinforcementPathActor& First, const AReinforcementPathActor& Second)
		{
			return First.GetPathName() < Second.GetPathName();
		});
		return Actors;
	}

	void EmitActorPoints(FPCGContext& Context, const TArray<AReinforcementPathActor*>& Actors)
	{
		UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(&Context);
		PointData->InitializeFromData(nullptr);

		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
		Points.Reserve(Actors.Num());
		for (const AReinforcementPathActor* Actor : Actors)
		{
			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = Actor->GetActorTransform();
			Point.Density = 1.0f;
			Point.Seed = static_cast<int32>(GetTypeHash(Actor->GetPathName()));
		}

		FPCGTaggedData& Output = Context.OutputData.TaggedData.Emplace_GetRef();
		Output.Pin = ReinforcementPathActorDataConstants::OutputPin;
		Output.Data = PointData;
	}
}

#if WITH_EDITOR
FName UPCGGetReinforcementPathActorDataSettings::GetDefaultNodeName() const
{
	return TEXT("GetReinforcementPathActorData");
}

FText UPCGGetReinforcementPathActorDataSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Get Reinforcement Path Actor Data");
}

FText UPCGGetReinforcementPathActorDataSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Outputs one point at the transform of every Reinforcement Path Actor, including derived actor classes, in the current world.");
}

void UPCGGetReinforcementPathActorDataSettings::GetStaticTrackedKeys(
	FPCGSelectionKeyToSettingsMap& OutKeysToSettings,
	TArray<TObjectPtr<const UPCGGraph>>& /*OutVisitedGraphs*/) const
{
	const FPCGSelectionKey ReinforcementActorKey(AReinforcementPathActor::StaticClass());
	OutKeysToSettings.FindOrAdd(ReinforcementActorKey).Emplace(this, false);
}
#endif

TArray<FPCGPinProperties> UPCGGetReinforcementPathActorDataSettings::InputPinProperties() const
{
	return {};
}

TArray<FPCGPinProperties> UPCGGetReinforcementPathActorDataSettings::OutputPinProperties() const
{
	return {FPCGPinProperties(ReinforcementPathActorDataConstants::OutputPin, EPCGDataType::Point)};
}

FPCGElementPtr UPCGGetReinforcementPathActorDataSettings::CreateElement() const
{
	return MakeShared<FPCGGetReinforcementPathActorDataElement>();
}

bool FPCGGetReinforcementPathActorDataElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (not IsValid(SourceComponent))
	{
		return true;
	}

	UWorld* World = SourceComponent->GetWorld();
	if (not IsValid(World))
	{
		return true;
	}

	EmitActorPoints(*Context, FindReinforcementPathActors(*World));
	return true;
}

#undef LOCTEXT_NAMESPACE
