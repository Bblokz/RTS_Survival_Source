#include "FRTS_FactionFlags.h"

#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

int32 FRTS_FactionFlags::FlagPrimitiveDataIndex = 0;
int32 FRTS_FactionFlags::BreakThroughFactionAtlasIndex= 1;
int32 FRTS_FactionFlags::StrikeDivisionFactionAtlasIndex= 0;
int32 FRTS_FactionFlags::SovietFactionAtlasIndex= 2;
int32 FRTS_FactionFlags::ItalianFactionAtlasIndex= 3;

void FRTS_FactionFlags::SetupFactionFlagsForActorPrimitives(AActor* ActorToSetDataOn)
{
	if (!IsValid(ActorToSetDataOn))
	{
		return;
	}
	ERTSFaction Faction = FRTS_Statics::GetPlayerFaction(ActorToSetDataOn);
	URTSComponent* RTSComp = Cast<URTSComponent>( ActorToSetDataOn->GetComponentByClass(URTSComponent::StaticClass()));
	if (!IsValid(RTSComp))
	{
		return;
	}
	const int32 FactionFlagAtlasIndex = GetPrimitiveDataFlagIndexForFaction(Faction, RTSComp->GetOwningPlayer());
	
	// Start the recursive tree check on the main vehicle
	SetFlagOnActorAndAllChildren(ActorToSetDataOn, FactionFlagAtlasIndex);
}

void FRTS_FactionFlags::SetFlagOnActorAndAllChildren(const AActor* TargetActor, const int32 FlagPrimitiveData)
{
	if (!IsValid(TargetActor))
	{
		return;
	}

	// TInlineComponentArray allocates on the CPU stack instead of the heap (very fast for RTS)
	TInlineComponentArray<UActorComponent*> Components(TargetActor);
	for (UActorComponent* EachComponent : Components)
	{
		if (!IsValid(EachComponent))
		{
			continue;
		}

		// 1. Process Primitives on this current actor level
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(EachComponent))
		{
			Primitive->SetCustomPrimitiveDataFloat(FRTS_FactionFlags::FlagPrimitiveDataIndex, static_cast<float>(FlagPrimitiveData));
			continue; 
		}

		// 2. Process Child Actors (Recursion ensures deeply nested actors work perfectly)
		if (UChildActorComponent* ChildActorComp = Cast<UChildActorComponent>(EachComponent))
		{
			AActor* ActorOfChild = ChildActorComp->GetChildActor();
			if (IsValid(ActorOfChild))
			{
				SetFlagOnActorAndAllChildren(ActorOfChild, FlagPrimitiveData);
			}
		}
	}
}


int32 FRTS_FactionFlags::GetPrimitiveDataFlagIndexForFaction(const ERTSFaction Faction, const int32 OwningPlayer)
{
	// If it is not Player 1 (e.g., enemy AI or neutral), assign the Soviet atlas index
	if (OwningPlayer != 1)
	{
		return FRTS_FactionFlags::SovietFactionAtlasIndex;
	}

	switch (Faction)
	{
	case ERTSFaction::GerBreakthroughDoctrine:
		return FRTS_FactionFlags::BreakThroughFactionAtlasIndex;	

	case ERTSFaction::GerStrikeDivision:
		return FRTS_FactionFlags::StrikeDivisionFactionAtlasIndex;

	case ERTSFaction::GerItalianFaction:
		return FRTS_FactionFlags::ItalianFactionAtlasIndex;

	case ERTSFaction::NotInitialised:
		RTSFunctionLibrary::ReportError("Not initialised faction attempted to get primitive data for flag index");
		return FRTS_FactionFlags::StrikeDivisionFactionAtlasIndex;
	}

	RTSFunctionLibrary::ReportError("Could not find the index of faction for the flag primitive data");
	return FRTS_FactionFlags::StrikeDivisionFactionAtlasIndex;
}
