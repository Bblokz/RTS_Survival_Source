#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Audio/RTSVoiceLines/RTSVoicelines.h"

UENUM(Blueprintable)
enum class EPlayerError : uint8
{
	Error_None,
	Error_NotEnoughRadixite,
	Error_NotEnoughMetal,
	Error_NotEnoughEnergy,
	Error_NotEnoughVehicleParts,
	Error_NotEnoughtFuel,
	Error_NotENoughAmmo,
	Error_NotEnoughWeaponBlueprints,
	Error_NotEnoughBuildingBlueprints,
	Error_NotEnoughEnergyBlueprints,
	Error_NotEnoughVehicleBlueprints,
	Error_NotEnoughConstructionBlueprints,
	Error_LocationNotReachable,
	Error_CannotStackBuildingAbilities,
	Error_NoFreeTruckAvailable,
	Error_CannotBuildHere,
};

inline EAnnouncerVoiceLineType ConvertPlayerErrorToAnnouncerVoiceLineType(const EPlayerError PlayerError)
{
    EAnnouncerVoiceLineType VoiceLineType = EAnnouncerVoiceLineType::None;
    switch (PlayerError) {
    case EPlayerError::Error_None:
        return VoiceLineType;
    case EPlayerError::Error_NotEnoughRadixite:
        return EAnnouncerVoiceLineType::NotEnoughRadixite;
    case EPlayerError::Error_NotEnoughMetal:
        return EAnnouncerVoiceLineType::NotEnoughMetal;
    case EPlayerError::Error_NotEnoughVehicleParts:
        return EAnnouncerVoiceLineType::NotEnoughVehicleParts;
    case EPlayerError::Error_NotEnoughtFuel:
    case EPlayerError::Error_LocationNotReachable:
        return EAnnouncerVoiceLineType::ErrorLocationNotReachable;
    case EPlayerError::Error_CannotStackBuildingAbilities:
        return EAnnouncerVoiceLineType::ErrorCannotStackBuildingAbilities;
    case EPlayerError::Error_NoFreeTruckAvailable:
        return EAnnouncerVoiceLineType::ErrorNoFreeTruckAvailable;
    case EPlayerError::Error_CannotBuildHere:
        return EAnnouncerVoiceLineType::ErrorCannotBuildHere;
    default:
        return VoiceLineType;
    }
}
