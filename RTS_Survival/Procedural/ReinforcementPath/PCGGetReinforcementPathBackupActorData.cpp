// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#include "PCGGetReinforcementPathBackupActorData.h"

#include "ReinforcementPathBackupActor.h"

#include "PCGComponent.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "PCGGetReinforcementPathBackupActorData"

namespace ReinforcementPathBackupActorDataConstants
{
	const FName OutputPin = TEXT("BackupRoadEnds");
}

namespace
{
	TArray<AReinforcementPathBackupActor*> FindReinforcementPathBackupActors(UWorld& World)
	{
		TArray<AReinforcementPathBackupActor*> Actors;
		for (TActorIterator<AReinforcementPathBackupActor> ActorIterator(&World); ActorIterator; ++ActorIterator)
		{
			AReinforcementPathBackupActor* Actor = *ActorIterator;
			if (not IsValid(Actor))
			{
				continue;
			}

			Actors.Add(Actor);
		}

		Actors.Sort([](const AReinforcementPathBackupActor& First, const AReinforcementPathBackupActor& Second)
		{
			return First.GetPathName() < Second.GetPathName();
		});
		return Actors;
	}

	void EmitBackupActorPoints(FPCGContext& Context, const TArray<AReinforcementPathBackupActor*>& Actors)
	{
		UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(&Context);
		PointData->InitializeFromData(nullptr);

		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
		Points.Reserve(Actors.Num());
		for (const AReinforcementPathBackupActor* Actor : Actors)
		{
			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = Actor->GetActorTransform();
			Point.Density = 1.0f;
			Point.Seed = static_cast<int32>(GetTypeHash(Actor->GetPathName()));
		}

		FPCGTaggedData& Output = Context.OutputData.TaggedData.Emplace_GetRef();
		Output.Pin = ReinforcementPathBackupActorDataConstants::OutputPin;
		Output.Data = PointData;
	}
}

#if WITH_EDITOR
FName UPCGGetReinforcementPathBackupActorDataSettings::GetDefaultNodeName() const
{
	return TEXT("GetReinforcementPathBackupActorData");
}

FText UPCGGetReinforcementPathBackupActorDataSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Get Reinforcement Path Backup Actor Data");
}

FText UPCGGetReinforcementPathBackupActorDataSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Outputs one point at the transform of every Reinforcement Path Backup Actor, including derived actor classes, in the current world.");
}

void UPCGGetReinforcementPathBackupActorDataSettings::GetStaticTrackedKeys(
	FPCGSelectionKeyToSettingsMap& OutKeysToSettings,
	TArray<TObjectPtr<const UPCGGraph>>& /*OutVisitedGraphs*/) const
{
	const FPCGSelectionKey BackupActorKey(AReinforcementPathBackupActor::StaticClass());
	OutKeysToSettings.FindOrAdd(BackupActorKey).Emplace(this, false);
}
#endif

TArray<FPCGPinProperties> UPCGGetReinforcementPathBackupActorDataSettings::InputPinProperties() const
{
	return {};
}

TArray<FPCGPinProperties> UPCGGetReinforcementPathBackupActorDataSettings::OutputPinProperties() const
{
	return {FPCGPinProperties(ReinforcementPathBackupActorDataConstants::OutputPin, EPCGDataType::Point)};
}

FPCGElementPtr UPCGGetReinforcementPathBackupActorDataSettings::CreateElement() const
{
	return MakeShared<FPCGGetReinforcementPathBackupActorDataElement>();
}

bool FPCGGetReinforcementPathBackupActorDataElement::ExecuteInternal(FPCGContext* Context) const
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

	EmitBackupActorPoints(*Context, FindReinforcementPathBackupActors(*World));
	return true;
}

#undef LOCTEXT_NAMESPACE
