#include "SocketRecord.h"

#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"

FString FSocketRecord::GetDebugStr() 
{
	// Convert socket state to string
	const FString StateStr = StaticEnum<EAircraftSocketState>()->GetNameStringByValue(static_cast<int64>(SocketState));

	// Aircraft name or null
	FString AircraftStr = TEXT("null");
	if (AssignedAircraft.IsValid())
	{
		AircraftStr = AssignedAircraft->GetName();
	}

	// Build output
	FString Out;
	Out += FString::Printf(TEXT("SocketState: %s\n"), *StateStr);
	Out += FString::Printf(TEXT("Aircraft: %s\n"), *AircraftStr);

	// Booleans
	Out += FString::Printf(TEXT("bM_IsInside: %s\n"), bM_IsInside ? TEXT("true") : TEXT("false"));
	Out += FString::Printf(TEXT("bM_ReservedByTraining: %s\n"), bM_ReservedByTraining ? TEXT("true") : TEXT("false"));
	Out += FString::Printf(TEXT("bM_PendingVTO: %s\n"), bM_PendingVTO ? TEXT("true") : TEXT("false"));
	Out += FString::Printf(TEXT("bM_PendingLanding: %s\n"), bM_PendingLanding ? TEXT("true") : TEXT("false"));

	return Out;
}
