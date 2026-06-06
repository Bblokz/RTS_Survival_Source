#pragma once

#include "CoreMinimal.h"

enum class ERTSFaction : uint8;

class FRTS_FactionFlags
{
public:
	static void SetupFactionFlagsForActorPrimitives(AActor* ActorToSetDataOn);
	static void SetFlagOnActorAndAllChildren(const AActor* TargetActor, int32 FlagPrimitiveData);

	static int32 GetPrimitiveDataFlagIndexForFaction(ERTSFaction Faction, int32 OwningPlayer);
	
	private:
	
	
	static int32 FlagPrimitiveDataIndex;
	static int32 BreakThroughFactionAtlasIndex;
	static int32 StrikeDivisionFactionAtlasIndex;
	static int32 SovietFactionAtlasIndex;
	static int32 ItalianFactionAtlasIndex;
	
};
