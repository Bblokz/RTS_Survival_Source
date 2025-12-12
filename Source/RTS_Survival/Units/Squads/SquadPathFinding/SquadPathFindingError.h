#pragma once

#include "CoreMinimal.h"
#include "NavigationData.h"

#include "SquadPathFindingError.generated.h"


UENUM()
enum class ESquadPathFindingError : uint8
{
	NoError,
	InvalidWorld,
	InvalidNavSystem,
	InvalidAIController,
	CannotProjectStartLocation,
	NoUnitsLeftInSquad,
	CannotBuildValidPathFindingQuery,
	QueryResult_Invalid,
	QueryResult_Error,
	QueryResult_Fail,
	PathResultIsInvalid
};

static FString GetSquadPathFindingErrorMsg(ESquadPathFindingError Error)
{
	switch (Error)
	{
	case ESquadPathFindingError::NoError:
		return TEXT("No Error");
	case ESquadPathFindingError::InvalidWorld:
		return TEXT("Invalid World cannot complete squad path finding.");
	case ESquadPathFindingError::InvalidNavSystem:
		return TEXT("Invalid Nav System cannot complete squad path finding.");
	case ESquadPathFindingError::InvalidAIController:
		return TEXT("Invalid AI Controller of unit in squad cannot complete squad path finding.");
	case ESquadPathFindingError::CannotProjectStartLocation:
		return TEXT("Cannot Project Start Location for squad path finding. Will not complete the path finding request");
	case ESquadPathFindingError::NoUnitsLeftInSquad:
		return TEXT("No Units Left in Squad to complete path finding.");
	case ESquadPathFindingError::CannotBuildValidPathFindingQuery:
		return TEXT("Cannot Build Valid Path Finding Query for Squad Path Finding.");
	case ESquadPathFindingError::QueryResult_Invalid:
		return TEXT(
			"Submitted query but squad failed to find path (ENavigationQueryResult::Invalid) will not complete path finding.");
	case ESquadPathFindingError::QueryResult_Error:
		return TEXT(
			"Submitted query but squad failed to find path (ENavigationQueryResult::Error) will not complete path finding.");
	case ESquadPathFindingError::QueryResult_Fail:
		return TEXT(
			"Submitted query but squad failed to find path (ENavigationQueryResult::Fail) will not complete path finding.");
	case ESquadPathFindingError::PathResultIsInvalid:
		return TEXT("Path Result is Invalid. Cannot complete squad path finding.");
	default:
		return TEXT("Unknown Error");
	}
}

static ESquadPathFindingError GetPathFindingErrorFromQuery(const FPathFindingResult& InQueryResult)
{
	
	const auto QueryResult = InQueryResult.Result;
	switch (QueryResult)
	{
	case ENavigationQueryResult::Invalid:
		return ESquadPathFindingError::QueryResult_Invalid;
	case ENavigationQueryResult::Error:
		return ESquadPathFindingError::QueryResult_Error;
	case ENavigationQueryResult::Fail:
		return ESquadPathFindingError::QueryResult_Fail;
	case ENavigationQueryResult::Success:
		return ESquadPathFindingError::NoError;
	}
	return ESquadPathFindingError::NoError;
}
