#include "RTS_VoiceLineHelpers.h"
#include "RTS_Survival/Audio/RTSVoiceLines/RTSVoicelines.h"
#include "RTS_Survival/Player/Abilities.h"

#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerError/PlayerError.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

ERTSVoiceLine FRTS_VoiceLineHelpers::GetVoiceLineFromAbility(const EAbilityID Ability)
{
	switch (Ability)
	{
	case EAbilityID::IdNoAbility:
		return ERTSVoiceLine::None;
	case EAbilityID::IdNoAbility_MoveCloserToTarget:
		return ERTSVoiceLine::Attack;
	case EAbilityID::IdIdle:
		return ERTSVoiceLine::Select;
	case EAbilityID::IdAttack:
		return ERTSVoiceLine::Attack;
	case EAbilityID::IdMove:
		return ERTSVoiceLine::Move;
	case EAbilityID::IdPatrol:
		return ERTSVoiceLine::Move;
	case EAbilityID::IdStop:
		return ERTSVoiceLine::Stop;
	case EAbilityID::IdSwitchWeapon:
		return ERTSVoiceLine::Confirm;
	case EAbilityID::IdSwitchMelee:
		return ERTSVoiceLine::Confirm;
	case EAbilityID::IdGeneral_Confirm:
		return ERTSVoiceLine::Confirm;
	case EAbilityID::IdProne:
		return ERTSVoiceLine::Confirm;
	case EAbilityID::IdCrouch:
		return ERTSVoiceLine::Confirm;
	case EAbilityID::IdStand:
		return ERTSVoiceLine::Confirm;
	case EAbilityID::IdSwitchSingleBurst:
		return ERTSVoiceLine::Confirm;
	case EAbilityID::IdReverseMove:
		return ERTSVoiceLine::ReverseMove;
	case EAbilityID::IdRotateTowards:
		return ERTSVoiceLine::Rotation;
	case EAbilityID::IdCreateBuilding:
		return ERTSVoiceLine::Building;
	case EAbilityID::IdConvertToVehicle:
		return ERTSVoiceLine::Confirm;
	case EAbilityID::IdHarvestResource:
		return ERTSVoiceLine::Harvest;
	case EAbilityID::IdReturnCargo:
		return ERTSVoiceLine::Confirm;
	case EAbilityID::IdPickupItem:
		return ERTSVoiceLine::Pickup;
	case EAbilityID::IdScavenge:
		return ERTSVoiceLine::Scavenge;
	// todo
	case EAbilityID::IdReturnToBase:
		return ERTSVoiceLine::LoadAP;
	case EAbilityID::IdRetreat:
		return ERTSVoiceLine::FallBackToHQ;
	case EAbilityID::IdEnterCargo:
		return ERTSVoiceLine::EnterPosition;
	case EAbilityID::IdExitCargo:
		return ERTSVoiceLine::ExitPosition;
	// Falls through.
	case EAbilityID::IdDigIn:
	case EAbilityID::IdBreakCover:
		return ERTSVoiceLine::Confirm;
	case EAbilityID::IdFireRockets:
		return ERTSVoiceLine::FireExtraWeapons;
	case EAbilityID::IdCancelRocketFire:
		return ERTSVoiceLine::Confirm;
	case EAbilityID::IdRocketsReloading:
		break;
	case EAbilityID::IdRepair:
		return ERTSVoiceLine::Repair;
	case EAbilityID::IdAttackGround:
		return ERTSVoiceLine::Attack;
	case EAbilityID::IdAttachedWeapon:
		return ERTSVoiceLine::Attack;
	case EAbilityID::IdAircraftOwnerNotExpanded:
		return ERTSVoiceLine::Confirm;
	case EAbilityID::IdNoAbility_MoveToEvasionLocation:
		break;
	case EAbilityID::IdEnableResourceConversion:
		break;
	case EAbilityID::IdDisableResourceConversion:
		break;
	case EAbilityID::IdCapture:
		break;
	case EAbilityID::IdActivateMode:
		return ERTSVoiceLine::Confirm;
	case EAbilityID::IdDisableMode:
		return ERTSVoiceLine::Confirm;
	case EAbilityID::IdThrowGrenade:
		break;
	case EAbilityID::IdCancelThrowGrenade:
		break;
	case EAbilityID::IdGrenadesResupplying:
		break;
	case EAbilityID::IdReinforceSquad:
		break;
	case EAbilityID::IdApplyBehaviour:
		break;
	case EAbilityID::IdFieldConstruction:
		return ERTSVoiceLine::Building;
	case EAbilityID::IdAimAbility:
	case EAbilityID::IdCancelAimAbility:
		return ERTSVoiceLine::Confirm;
	}
	RTSFunctionLibrary::ReportError("Could not translate ability: " + Global_GetAbilityIDAsString(Ability) +
		"To voice line. Please check the enum and the translation function : GetVoiceLineFromAbility.");
	return ERTSVoiceLine::None;
}

bool FRTS_VoiceLineHelpers::NeedToPlayAnnouncerLineForAbility(const EAbilityID Ability,
                                                              EAnnouncerVoiceLineType& OutAnnouncerLine)
{
	switch (Ability)
	{
	case EAbilityID::IdCapture:
		OutAnnouncerLine = EAnnouncerVoiceLineType::SquadWillCaptureObjective;
		return true;
	default:
		return false;
	}
}

ERTSVoiceLine FRTS_VoiceLineHelpers::GetStressedVoiceLineVersion(const ERTSVoiceLine VoiceLineType)
{
	switch (VoiceLineType)
	{
	case ERTSVoiceLine::Select:
		return ERTSVoiceLine::SelectStressed;
	case ERTSVoiceLine::Move:
		return ERTSVoiceLine::MoveStressed;
	case ERTSVoiceLine::Attack:
		return ERTSVoiceLine::AttackStressed;
	case ERTSVoiceLine::Confirm:
		return ERTSVoiceLine::ConfirmStressed;
	case ERTSVoiceLine::ReverseMove:
		return ERTSVoiceLine::ReverseMoveStressed;
	case ERTSVoiceLine::Stop:
		return ERTSVoiceLine::StopStressed;
	case ERTSVoiceLine::Rotation:
		return ERTSVoiceLine::RotationStressed;
	case ERTSVoiceLine::Repair:
		return ERTSVoiceLine::RepairStressed;
	case ERTSVoiceLine::Building:
		return ERTSVoiceLine::BuildingStressed;

	// Turn into confirm stressed.
	case ERTSVoiceLine::Scavenge:
	case ERTSVoiceLine::EnterPosition:
	case ERTSVoiceLine::ExitPosition:
	case ERTSVoiceLine::Pickup:
	case ERTSVoiceLine::Harvest:
		return ERTSVoiceLine::ConfirmStressed;

	// Already stressed or no stressed variant
	case ERTSVoiceLine::SelectStressed:
	case ERTSVoiceLine::MoveStressed:
	case ERTSVoiceLine::AttackStressed:
	case ERTSVoiceLine::ConfirmStressed:
	case ERTSVoiceLine::RotationStressed:
	case ERTSVoiceLine::StopStressed:
	case ERTSVoiceLine::RepairStressed:
	case ERTSVoiceLine::ReverseMoveStressed:
		return VoiceLineType;

	// No stressed version available
	case ERTSVoiceLine::None:
	case ERTSVoiceLine::SelectExcited:
	case ERTSVoiceLine::SelectNeedRepairs:
	case ERTSVoiceLine::SelectAnnoyed:
	case ERTSVoiceLine::AttackExcited:
	case ERTSVoiceLine::Fire:
	case ERTSVoiceLine::UnitPromoted:
	case ERTSVoiceLine::FireExtraWeapons:
	case ERTSVoiceLine::CommandCompleted:
	case ERTSVoiceLine::Fortified:
	case ERTSVoiceLine::ShotConnected:
	case ERTSVoiceLine::ShotBounced:
	case ERTSVoiceLine::EnemyDestroyed:
	case ERTSVoiceLine::LostEngine:
	case ERTSVoiceLine::LostGun:
	case ERTSVoiceLine::LostTrack:
	case ERTSVoiceLine::LoadAP:
	case ERTSVoiceLine::LoadHE:
	case ERTSVoiceLine::LoadHEAT:
	case ERTSVoiceLine::LoadRadixite:
	case ERTSVoiceLine::LoadAPCR:
	case ERTSVoiceLine::NeedHelp:
	case ERTSVoiceLine::UnitDies:
	default:
		return VoiceLineType;
	}
}

void FRTS_VoiceLineHelpers::PlayUnitDeathVoiceLineOnRadio(AActor* UnitThatDied, const bool bForcePlay,
                                                          const bool bQueueIfNotPlayed)
{
	if (not IsValid(UnitThatDied))
	{
		return;
	}
	URTSComponent* RTSComponent = UnitThatDied->FindComponentByClass<URTSComponent>();
	if (not RTSComponent || RTSComponent->GetOwningPlayer() != 1)
	{
		return;
	}
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(UnitThatDied);
	if (not PlayerController)
	{
		return;
	}
	PlayerController->PlayVoiceLine(UnitThatDied, ERTSVoiceLine::UnitDies, bForcePlay, bQueueIfNotPlayed);
}

EAnnouncerVoiceLineType FRTS_VoiceLineHelpers::GetDeathVoiceLineForTank(const ETankSubtype Type)
{
	// Handle the harvester as an exception
	if (Type == ETankSubtype::Tank_PzI_Harvester)
	{
		return EAnnouncerVoiceLineType::LostHarvester;
	}

	// Use helper functions for other categories
	if (Global_GetIsArmoredCar(Type))
	{
		return EAnnouncerVoiceLineType::LostArmoredCar;
	}
	if (Global_GetIsLightTank(Type))
	{
		return EAnnouncerVoiceLineType::LostLightVehicle;
	}
	if (Global_GetIsMediumTank(Type))
	{
		return EAnnouncerVoiceLineType::LostMediumVehicle;
	}
	if (Global_GetIsHeavyTank(Type))
	{
		return EAnnouncerVoiceLineType::LostHeavyVehicle;
	}

	// Default case
	return EAnnouncerVoiceLineType::None;
}

EAnnouncerVoiceLineType FRTS_VoiceLineHelpers::GetAnnouncerForBxp(const EBxpOptionSection Section)
{
	switch (Section)
	{
	case EBxpOptionSection::BOS_Defense:
		return EAnnouncerVoiceLineType::StartDefensiveBxpConstruction;
	case EBxpOptionSection::BOS_Tech:
		return EAnnouncerVoiceLineType::StartTechnologyBxpConstruction;
	case EBxpOptionSection::BOS_Economic:
		return EAnnouncerVoiceLineType::StartEconomicBxpConstruction;
	}
	return EAnnouncerVoiceLineType::None;
}

EAnnouncerVoiceLineType FRTS_VoiceLineHelpers::GetAnnouncerForPlayerError(const EPlayerError Error)
{
	switch (Error)
	{
	case EPlayerError::Error_None:
		return EAnnouncerVoiceLineType::None;
	case EPlayerError::Error_NotEnoughRadixite:
		return EAnnouncerVoiceLineType::NotEnoughRadixite;
	case EPlayerError::Error_NotEnoughMetal:
		return EAnnouncerVoiceLineType::NotEnoughMetal;
	case EPlayerError::Error_NotEnoughEnergy:
		return EAnnouncerVoiceLineType::LowEnergy;
	case EPlayerError::Error_NotEnoughVehicleParts:
		return EAnnouncerVoiceLineType::NotEnoughVehicleParts;
	case EPlayerError::Error_NotEnoughtFuel:
		return EAnnouncerVoiceLineType::LowEnergy;
	case EPlayerError::Error_NotENoughAmmo:
		break;
	case EPlayerError::Error_NotEnoughWeaponBlueprints:
		break;
	case EPlayerError::Error_NotEnoughBuildingBlueprints:
		break;
	case EPlayerError::Error_NotEnoughEnergyBlueprints:
		break;
	case EPlayerError::Error_NotEnoughVehicleBlueprints:
		break;
	case EPlayerError::Error_NotEnoughConstructionBlueprints:
		break;
	case EPlayerError::Error_LocationNotReachable:
		return EAnnouncerVoiceLineType::ErrorLocationNotReachable;
	case EPlayerError::Error_CannotStackBuildingAbilities:
		return EAnnouncerVoiceLineType::ErrorCannotStackBuildingAbilities;
	case EPlayerError::Error_NoFreeTruckAvailable:
		return EAnnouncerVoiceLineType::ErrorNoFreeTruckAvailable;
	case EPlayerError::Error_CannotBuildHere:
		return EAnnouncerVoiceLineType::ErrorCannotBuildHere;
	}
	return EAnnouncerVoiceLineType::None;
}
