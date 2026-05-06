#include "StrategicAIActionRequirements.h"

#include "GameFramework/Actor.h"
#include "RTS_Survival/Enemy/StrategicAI/BlackboardQueries/BlackboardQueryHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

FString UStrategicAIActionRequirement::GetDebugString() const
{
	return "DO NOT USE default requirement!!";
}

bool UStrategicAIGameTimePassedRequirement::GetIsRequirementMet(
	const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const
{
	return GameTimeSeconds >= RequiredGameTimeSeconds;
}

FString UStrategicAIGameTimePassedRequirement::GetDebugString() const
{
		return FString::Printf(TEXT("Time Req: %f sec"), RequiredGameTimeSeconds);
}

bool UStrategicAIActorIsValidRequirement::GetIsRequirementMet(
	const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const
{
	return GetIsValidRequiredActor();
}

bool UStrategicAIActorIsValidRequirement::GetIsValidRequiredActor() const
{
	if (IsValid(RequiredActor))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(TEXT("Strategic AI actor requirement failed because RequiredActor is invalid."));

	return false;
}

FString UStrategicAIActorIsValidRequirement::GetDebugString() const
{
	return FString::Printf(TEXT("Actor Valid Req: %s"), *GetNameSafe(RequiredActor));
}

bool UStrategicAIHasPlayerHQLocationRequirement::GetIsRequirementMet(
	const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	return BlackboardQueries::HasValidPlayerHQLocation(Blackboard);
}

FString UStrategicAIHasPlayerHQLocationRequirement::GetDebugString() const
{
	return TEXT("Has Player HQ Location Req");
}

bool UStrategicAIHasPlayerResourceBuildingLocationsRequirement::GetIsRequirementMet(
	const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	return BlackboardQueries::HasValidPlayerResourceBuildingLocations(Blackboard);
}

FString UStrategicAIHasPlayerResourceBuildingLocationsRequirement::GetDebugString() const
{
	return TEXT("Has Player Resource Building Locations Req");
}

bool UStrategicAIHasAtLeastIdleSquads::GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard,
                                                           const float GameTimeSeconds) const
{
	return BlackboardQueries::HasAtLeastXSquads(Blackboard, AmountIdleNeeded, RequiredSquadSubtype);	

}

FString UStrategicAIHasAtLeastIdleSquads::GetDebugString() const
{
	return FString::Printf(TEXT("Has Idle Squads Req: %d %s"), AmountIdleNeeded, *Global_GetSquadDisplayName(RequiredSquadSubtype));
}

bool UStrategicAIHasAtLeastIdleTanks::GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard,
                                                          const float GameTimeSeconds) const
{
	return BlackboardQueries::HasAtLeastXTanks(Blackboard, AmountIdleNeeded, RequiredTankSubtype);
}

FString UStrategicAIHasAtLeastIdleTanks::GetDebugString() const
{
	return FString::Printf(TEXT("Has Idle Tanks Req: %d %s"), AmountIdleNeeded, *Global_GetTankDisplayName(RequiredTankSubtype));
}

bool UStrategicAIHasAtLeastIdleAircraft::GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard,
                                                             const float GameTimeSeconds) const
{
	return BlackboardQueries::HasAtLeastXAircraft(Blackboard, AmountIdleNeeded, RequiredAircraftSubtype);
}

FString UStrategicAIHasAtLeastIdleAircraft::GetDebugString() const
{
	return FString::Printf(TEXT("Has Idle Aircraft Req: %d %s"), AmountIdleNeeded, *Global_GetAircraftDisplayName(RequiredAircraftSubtype));
}
