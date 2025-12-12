#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BXPConstructionRules/BXPConstructionRules.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


#include "PlayerBuildingPreviewMode.generated.h"


class RTSFunctionLibrary;
/**
 * @brief Possible building preview modes
 */
UENUM()
enum class EPlayerBuildingPreviewMode : uint8
{
	// No preview mode is active.
	BuildingPreviewModeOFF,
	// Preview mode is active for determening the building expansion location.
	// This contains both free expansion placemetns as well as socket and origin placements
	BxpPreviewMode,
	// Preview mode is active for determening the building conversion location.
	NomadicPreviewMode
};

static FString Global_GetPlayerBuildingPreviewModeEnumAsString(const EPlayerBuildingPreviewMode Mode)
{
	switch (Mode) {
	case EPlayerBuildingPreviewMode::BuildingPreviewModeOFF:
		return "BuildingPreviewModeOFF";
	case EPlayerBuildingPreviewMode::BxpPreviewMode:
		return "BxpPreviewMode";
	case EPlayerBuildingPreviewMode::NomadicPreviewMode:
		return "NomadicPreviewMode";
	}
	return "Unknown";
}

UENUM()
enum class EConstructionPreviewMode : uint8
{
	Construct_None,
	// Used for preview placement for a nomadic building that wants to expand.
	Construct_NomadicPreview,
	// Used for BXps that are placed freely in a bxp construction radius.
	Construct_BxpFree,
	// Used for BXps that are placed in a socket.
	Construct_BxpSocket,
	// Used for BXps that are placed at the building origin.
	Construct_BxpOrigin,
	
};

static FString Global_GetConstructionPreviewModeEnumAsString(const EConstructionPreviewMode Mode)
{
	switch (Mode) {
	case EConstructionPreviewMode::Construct_None:
		return "Construct_None";
	case EConstructionPreviewMode::Construct_NomadicPreview:
		return "Construct_NomadicPreview";
	case EConstructionPreviewMode::Construct_BxpFree:
		return "Construct_BxpFree";
	case EConstructionPreviewMode::Construct_BxpSocket:
		return "Construct_BxpSocket";
	case EConstructionPreviewMode::Construct_BxpOrigin:
		return "Construct_BxpOrigin";
	}
	return "Unknown";
}

static EConstructionPreviewMode Global_GetConstructionTypeFromBxpPreviewMode(const EBxpConstructionType ConstructionType)
{
	switch (ConstructionType) {
	case EBxpConstructionType::None:
		RTSFunctionLibrary::ReportError("Cannot translate BxpConstructionType None to a valid EConstructionPreviewMode."
									 " See Global_ConvertCosntructionTypeToBxpPreviewMode in PlayerBuildingPreviewMode.h");
		return EConstructionPreviewMode::Construct_None;
	case EBxpConstructionType::Free:
		return EConstructionPreviewMode::Construct_BxpFree;
	case EBxpConstructionType::Socket:
		return EConstructionPreviewMode::Construct_BxpSocket;
	case EBxpConstructionType::AtBuildingOrigin:
		return EConstructionPreviewMode::Construct_BxpOrigin;
	}
	return EConstructionPreviewMode::Construct_None;
}
