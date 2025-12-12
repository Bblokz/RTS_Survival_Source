#pragma once


#include "CoreMinimal.h"

#include "AsyncBxpLoadingType.generated.h"


UENUM()
enum class EAsyncBxpLoadingType : uint8
{
	None,
	AsyncLoadingNewBxp,
	// Specific unpacking of a BXP that uses free placement and needs the preview to let the user pick a location.
	AsyncLoadingPreviewPackedBxp,
	// Used for socket and origin expansion that once loaded start unpacking instantly.
	AsyncLoadingPackedBxpInstantPlacement
};

static FString Global_GetAsyncBxpLoadingTypeEnumAsString(const EAsyncBxpLoadingType Type)
{
	switch (Type) {
	case EAsyncBxpLoadingType::None:
		return "None";
	case EAsyncBxpLoadingType::AsyncLoadingNewBxp:
		return "AsyncLoadingNewBxp";
	case EAsyncBxpLoadingType::AsyncLoadingPreviewPackedBxp:
		return "AsyncLoadingPreviewPackedBxp";
	case EAsyncBxpLoadingType::AsyncLoadingPackedBxpInstantPlacement:
		return "AsyncLoadingPackedBxpInstantPlacement";
	}
	return "Unknown";
	
}
