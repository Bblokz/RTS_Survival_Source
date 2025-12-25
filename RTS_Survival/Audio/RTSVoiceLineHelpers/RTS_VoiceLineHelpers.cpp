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
	}
	RTSFunctionLibrary::ReportError("Could not translate ability: " + Global_GetAbilityIDAsString(Ability) +
		"To voice line. Please check the enum and the translation function : GetVoiceLineFromAbility.");
	return ERTSVoiceLine::None;
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

	// Turn into confirm stressed.
	case ERTSVoiceLine::Building:
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
	switch (Type)
	{
	// Harvester
	case ETankSubtype::Tank_PzI_Harvester:
		return EAnnouncerVoiceLineType::LostHarvester;

	// Armored Cars
	case ETankSubtype::Tank_Sdkfz251:
	case ETankSubtype::Tank_Sdkfz250:
	case ETankSubtype::Tank_Sdkfz250_37mm:
	case ETankSubtype::Tank_Sdkfz251_PZIV:
	case ETankSubtype::Tank_Sdkfz251_22:
	case ETankSubtype::Tank_Puma:
	case ETankSubtype::Tank_Sdkfz_231:
	case ETankSubtype::Tank_Sdkfz_232_3:
		return EAnnouncerVoiceLineType::LostArmoredCar;

	// Light Tanks
	case ETankSubtype::Tank_PzJager:
	case ETankSubtype::Tank_PzI_Scout:
	case ETankSubtype::Tank_PzI_15cm:
	case ETankSubtype::Tank_Pz38t:
	case ETankSubtype::Tank_PzII_F:
	case ETankSubtype::Tank_Sdkfz_140:
	case ETankSubtype::Tank_BT7:
	case ETankSubtype::Tank_T26:
	case ETankSubtype::Tank_BT7_4:
	case ETankSubtype::Tank_T70:
		return EAnnouncerVoiceLineType::LostLightVehicle;

	// Medium Tanks
	case ETankSubtype::Tank_PanzerIv:
	case ETankSubtype::Tank_PzIII_J:
	case ETankSubtype::Tank_PzIII_AA:
	case ETankSubtype::Tank_PzIII_FLamm:
	case ETankSubtype::Tank_PzIII_J_Commander:
	case ETankSubtype::Tank_PzIII_M:
	case ETankSubtype::Tank_PzIV_F1:
	case ETankSubtype::Tank_PzIV_F1_Commander:
	case ETankSubtype::Tank_PzIV_G:
	case ETankSubtype::Tank_PzIV_H:
	case ETankSubtype::Tank_Stug:
	case ETankSubtype::Tank_Marder:
	case ETankSubtype::Tank_PzIV_70:
	case ETankSubtype::Tank_Brumbar:
	case ETankSubtype::Tank_Hetzer:
	case ETankSubtype::Tank_Jaguar:
	case ETankSubtype::Tank_T34_85:
	case ETankSubtype::Tank_T34_100:
	case ETankSubtype::Tank_T34_76:
	case ETankSubtype::Tank_T34E:
		return EAnnouncerVoiceLineType::LostMediumVehicle;

	// Heavy Tanks
	case ETankSubtype::Tank_PantherD:
	case ETankSubtype::Tank_PantherG:
	case ETankSubtype::Tank_PanzerV_III:
	case ETankSubtype::Tank_PanzerV_IV:
	case ETankSubtype::Tank_PantherII:
	case ETankSubtype::Tank_KeugelT38:
	case ETankSubtype::Tank_JagdPanther:
	case ETankSubtype::Tank_SturmTiger:
	case ETankSubtype::Tank_Tiger:
	case ETankSubtype::Tank_TigerH1:
	case ETankSubtype::Tank_KingTiger:
	case ETankSubtype::Tank_Tiger105:
	case ETankSubtype::Tank_E25:
	case ETankSubtype::Tank_JagdTiger:
	case ETankSubtype::Tank_Maus:
	case ETankSubtype::Tank_E100:
	case ETankSubtype::Tank_T35:
	case ETankSubtype::Tank_KV_1:
	case ETankSubtype::Tank_KV_2:
	case ETankSubtype::Tank_KV_1E:
	case ETankSubtype::Tank_T28:
	case ETankSubtype::Tank_IS_1:
	case ETankSubtype::Tank_KV_IS:
	case ETankSubtype::Tank_IS_2:
	case ETankSubtype::Tank_IS_3:
	case ETankSubtype::Tank_KV_5:
		return EAnnouncerVoiceLineType::LostHeavyVehicle;

	// Default case
	default:
		return EAnnouncerVoiceLineType::None;
	}
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
}
