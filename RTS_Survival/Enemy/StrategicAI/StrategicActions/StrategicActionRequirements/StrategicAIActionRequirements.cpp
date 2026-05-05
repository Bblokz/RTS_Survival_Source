#include "StrategicAIActionRequirements.h"

#include "GameFramework/Actor.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

FString UStrategicAIActionRequirement::GetDebugString() const
{
	return "DO NOT USE default requirement!!";
}

bool UStrategicAIGameTimePassedRequirement::GetIsRequirementMet(
	const FStrategicAIBlackboard& RequirementContext, const float GameTimeSeconds) const
{
	return GameTimeSeconds >= M_RequiredGameTimeSeconds;
}

FString UStrategicAIGameTimePassedRequirement::GetDebugString() const
{
		return FString::Printf(TEXT("Time Req: %f sec"), M_RequiredGameTimeSeconds);
}

bool UStrategicAIActorIsValidRequirement::GetIsRequirementMet(
	const FStrategicAIBlackboard& RequirementContext, const float GameTimeSeconds) const
{
	return GetIsValidRequiredActor();
}

bool UStrategicAIActorIsValidRequirement::GetIsValidRequiredActor() const
{
	if (IsValid(M_RequiredActor))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(TEXT("Strategic AI actor requirement failed because RequiredActor is invalid."));

	return false;
}

FString UStrategicAIActorIsValidRequirement::GetDebugString() const
{
	return FString::Printf(TEXT("Actor Valid Req: %s"), *GetNameSafe(M_RequiredActor));
}
