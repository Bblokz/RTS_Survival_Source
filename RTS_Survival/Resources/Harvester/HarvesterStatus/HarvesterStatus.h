#pragma once

#include "CoreMinimal.h"

#include "HarvesterStatus.generated.h"


UENUM()
enum EHarvesterAIAction : uint8
{
	NoHarvestingAction,
	MoveToResource,
	PlayHarvestAnimation,
	HarvestResource,
	AsyncFindDropOff,
	MoveToDropOff,
	DropOff,
	AsyncFindResource,
	FinishHarvestCommand,
	UnstuckTowardsResource,
	UnstuckTowardsDropOff
};

static FString GetStringHarvesterAIStatus(EHarvesterAIAction Status)
{
	switch (Status)
	{
	case EHarvesterAIAction::NoHarvestingAction:
		return "NoHarvestingAction";
	case EHarvesterAIAction::MoveToResource:
		return "MoveToResource";
	case EHarvesterAIAction::PlayHarvestAnimation:
		return "PlayHarvestAnimation";
	case EHarvesterAIAction::HarvestResource:
		return "HarvestResource";
	case EHarvesterAIAction::AsyncFindDropOff:
		return "AsyncFindDropOff";
	case EHarvesterAIAction::MoveToDropOff:
		return "HarvestAIAction_MoveToDropOff";
	case EHarvesterAIAction::DropOff:
		return "DropOff";
	case EHarvesterAIAction::AsyncFindResource:
		return "AsyncFindResource";
	case EHarvesterAIAction::FinishHarvestCommand:
		return "FinishHarvestCommand";
	default:
		return "Unknown";
	}
}

UENUM()
enum EResourceValidation : uint8
{
	IsValidForHarvest,
	ResourceNotValid,
	ResourceEmpty,
	CargoFull
};

static FString GetStringResourceValidation(EResourceValidation Validation)
{
	switch (Validation)
	{
	case IsValidForHarvest:
		return "IsValidForHarvest";
	case ResourceNotValid:
		return "ResourceNotValid";
	case ResourceEmpty:
		return "ResourceEmpty";
	case CargoFull:
		return "CargoFull";
	default:
		return "Unknown";
	}
}
