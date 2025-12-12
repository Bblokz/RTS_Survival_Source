// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/AsyncBxpLoadingType/AsyncBxpLoadingType.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

#include "AsyncBxpRequestState.generated.h"

class IBuildingExpansionOwner;
class ABuildingExpansion;

enum class EAsyncBxpStatus
{
	Async_NoRequest,
	// The async manager is loading a bxp.
	Async_SpawnedPreview_WaitForBuildingMesh,
	// The asset is loaded and the bxp is spawned.
	Async_BxpIsSpawned
};

static FString Global_AsyncBxpStatusToString(EAsyncBxpStatus Status)
{
	switch (Status)
	{
	case EAsyncBxpStatus::Async_NoRequest:
		return "Async_NoRequest";
	case EAsyncBxpStatus::Async_SpawnedPreview_WaitForBuildingMesh:
		return "Async_SpawnedPreview_WaitForConstructBxp";
	case EAsyncBxpStatus::Async_BxpIsSpawned:
		return "Async_LoadingComplete";
	default:
		return "Unknown";
	}
}

USTRUCT()
struct FAsyncBxpRequestState
{
	GENERATED_BODY()

	// The building expansion that is spawned asynchronously, can be null.
	// Non-owning pointer saveguarded with GC.
	UPROPERTY()
	TObjectPtr<ABuildingExpansion> M_Expansion;

	EAsyncBxpStatus M_Status = EAsyncBxpStatus::Async_NoRequest;

	// Slot index in the array of bxps with widgets the bxp owner has.
	int M_ExpansionSlotIndex = INDEX_NONE;

	TScriptInterface<IBuildingExpansionOwner> M_BuildingExpansionOwner = nullptr;

	EAsyncBxpLoadingType M_BxpLoadingType = EAsyncBxpLoadingType::None;

	bool GetIsPackedExpansionRequest() const
	{
		if(M_BxpLoadingType == EAsyncBxpLoadingType::None)
		{
			RTSFunctionLibrary::ReportError("Attempted to request whether async loading request for bxp is packed,"
								   "but the async request type is None!"
		   "\n See function GetIsPackedExpansionRequest in AsyncBxpRequestState.h");
			return false;
		}
		return M_BxpLoadingType != EAsyncBxpLoadingType::AsyncLoadingNewBxp;
	}

	bool GetIsInstantPlacementRequest() const
	{
		return M_BxpLoadingType == EAsyncBxpLoadingType::AsyncLoadingPackedBxpInstantPlacement;
	}

	void Reset()
	{
		M_Expansion = nullptr;
		M_Status = EAsyncBxpStatus::Async_NoRequest;
		M_ExpansionSlotIndex = INDEX_NONE;
		M_BuildingExpansionOwner = nullptr;
		M_BxpLoadingType = EAsyncBxpLoadingType::None;
	}

	void InitSuccessfulRequest(
		const EAsyncBxpStatus InitStatus,
		const int InitExpansionIndex,
		TScriptInterface<IBuildingExpansionOwner> Owner,
		const EAsyncBxpLoadingType BxpLoadingType)
	{
		if(Owner)
		{
			M_Status = InitStatus;
			M_ExpansionSlotIndex = InitExpansionIndex;
			M_BuildingExpansionOwner = Owner;
			M_BxpLoadingType = BxpLoadingType;
		}
		else
		{
			RTSFunctionLibrary::ReportError("Cannont InitSuccesfulRequest as provided bxp Owner is null!"
								   "\n at function InitSuccessfulRequest in AsyncBxpRequestState.h");
		}

	}
};
