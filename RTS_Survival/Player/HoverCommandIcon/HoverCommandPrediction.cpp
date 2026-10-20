#include "HoverCommandPrediction.h"

#include "RTS_Survival/CaptureMechanic/CaptureMechanicHelpers.h"
#include "RTS_Survival/CaptureMechanic/CaptureInterface/CaptureInterface.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconTypes.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/CommandTypeDecoder/PlayerCommandTypeDecoder.h"
#include "RTS_Survival/RTSComponents/RepairComponent/RepairHelpers/RepairHelpers.h"
#include "RTS_Survival/RTSComponents/TowMechanic/TowedActor/TowedActor.h"
#include "RTS_Survival/RTSComponents/TowMechanic/VehicleTow/VehicleTow.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"

namespace
{
	ERTSVerticalAnimatedIcon IconIfAllowed(const bool bIsAllowed, const ERTSVerticalAnimatedIcon Icon)
	{
		return bIsAllowed ? Icon : ERTSVerticalAnimatedIcon::None;
	}

	// Mirrors OrderUnitsToAttackActor: unarmed vehicles are skipped there.
	ERTSVerticalAnimatedIcon PredictAttack(AActor* PrimaryUnit, ICommands& PrimaryCommands)
	{
		const ASelectablePawnMaster* PrimaryPawn = Cast<ASelectablePawnMaster>(PrimaryUnit);
		const bool bIsUnarmedPawn = IsValid(PrimaryPawn) && PrimaryPawn->GetVehicleType() != OrderTypeVehicle::OTV_Armed;
		const bool bCanAttack = not bIsUnarmedPawn && PrimaryCommands.HasAbility(EAbilityID::IdAttack);
		return IconIfAllowed(bCanAttack, ERTSVerticalAnimatedIcon::Command_Attack);
	}

	// Mirrors IssueOrderRepairAlly: allied characters and undamaged targets fall back to movement.
	ERTSVerticalAnimatedIcon PredictRepair(AActor* TargetActor, ICommands& PrimaryCommands)
	{
		const bool bCanRepair = FRTSRepairHelpers::GetIsUnitValidForRepairs(TargetActor)
			&& PrimaryCommands.HasAbility(EAbilityID::IdRepair);
		return IconIfAllowed(bCanRepair, ERTSVerticalAnimatedIcon::Command_Repair);
	}

	// Mirrors IssueOrderHarvestResource: only selected pawns harvest.
	ERTSVerticalAnimatedIcon PredictHarvest(AActor* PrimaryUnit, ICommands& PrimaryCommands)
	{
		const bool bCanHarvest = PrimaryUnit->IsA(ASelectablePawnMaster::StaticClass())
			&& PrimaryCommands.GetIsHarvester() != nullptr
			&& PrimaryCommands.HasAbility(EAbilityID::IdHarvestResource);
		return IconIfAllowed(bCanHarvest, ERTSVerticalAnimatedIcon::Command_HarvestResource);
	}

	ERTSVerticalAnimatedIcon PredictPickupItem(ASquadController* PrimarySquad)
	{
		const bool bCanPickup = IsValid(PrimarySquad) && PrimarySquad->CanSquadPickupWeapon()
			&& PrimarySquad->HasAbility(EAbilityID::IdPickupItem);
		return IconIfAllowed(bCanPickup, ERTSVerticalAnimatedIcon::Command_PickupItem);
	}

	ERTSVerticalAnimatedIcon PredictScavenge(AActor* TargetActor, ASquadController* PrimarySquad)
	{
		const AScavengeableObject* ScavengeObject = Cast<AScavengeableObject>(TargetActor);
		const bool bIsScavengeable = IsValid(ScavengeObject) && ScavengeObject->GetCanScavenge()
			&& not ScavengeObject->GetIsScavengedOrDestroyed();
		const bool bCanScavenge = bIsScavengeable && IsValid(PrimarySquad) && PrimarySquad->GetIsScavenger()
			&& PrimarySquad->HasAbility(EAbilityID::IdScavenge);
		return IconIfAllowed(bCanScavenge, ERTSVerticalAnimatedIcon::Command_Scavenge);
	}

	ERTSVerticalAnimatedIcon PredictEnterCargo(ASquadController* PrimarySquad)
	{
		const bool bCanEnterCargo = IsValid(PrimarySquad) && PrimarySquad->HasAbility(EAbilityID::IdEnterCargo);
		return IconIfAllowed(bCanEnterCargo, ERTSVerticalAnimatedIcon::Command_EnterCargo);
	}

	ERTSVerticalAnimatedIcon PredictManTeamWeapon(AActor* TargetActor, ASquadController* PrimarySquad)
	{
		const ATeamWeapon* TeamWeapon = Cast<ATeamWeapon>(TargetActor);
		const bool bCanMan = IsValid(TeamWeapon) && IsValid(PrimarySquad)
			&& FAbilityHelpers::GetCanSquadRemanAbandonedTeamWeapon(PrimarySquad, TeamWeapon);
		return IconIfAllowed(bCanMan, ERTSVerticalAnimatedIcon::Command_ManTeamWeapon);
	}

	ERTSVerticalAnimatedIcon PredictCapture(AActor* TargetActor, ASquadController* PrimarySquad)
	{
		const ICaptureInterface* CaptureInterface = FCaptureMechanicHelpers::GetValidCaptureInterface(TargetActor);
		if (CaptureInterface == nullptr || not IsValid(PrimarySquad))
		{
			return ERTSVerticalAnimatedIcon::None;
		}
		ICommands* SquadCommands = PrimarySquad;
		const bool bHasEnoughMembers =
			PrimarySquad->GetSquadUnitAmount() >= CaptureInterface->GetCaptureUnitAmountNeeded();
		return IconIfAllowed(SquadCommands->GetCanCaptureUnits() && bHasEnoughMembers,
		                     ERTSVerticalAnimatedIcon::Command_Capture);
	}

	ERTSVerticalAnimatedIcon PredictReturnToBase(AActor* PrimaryUnit)
	{
		return IconIfAllowed(PrimaryUnit->IsA(AAircraftMaster::StaticClass()),
		                     ERTSVerticalAnimatedIcon::Command_ReturnToBase);
	}

	bool GetIsPrimaryFreeToBeTowedBy(AActor* PrimaryUnit, ATankMaster* TowVehicle)
	{
		if (const ATeamWeaponController* TeamWeaponController = Cast<ATeamWeaponController>(PrimaryUnit))
		{
			UTowedActorComponent* UnusedTowedComponent = nullptr;
			return FAbilityHelpers::GetCanTowTeamWeaponWithCurrentCargoCapacity(
				TowVehicle, TeamWeaponController, UnusedTowedComponent);
		}
		const UTowedActorComponent* PrimaryTowedComponent = PrimaryUnit->FindComponentByClass<UTowedActorComponent>();
		return IsValid(PrimaryTowedComponent) && PrimaryTowedComponent->IsTowFree();
	}

	// Mirrors IssueOrderTowActor_ClickedTowVehicle: a busy hook moves, a failed hook-up falls back to entering cargo.
	ERTSVerticalAnimatedIcon PredictClickedTowVehicle(AActor* TargetActor, AActor* PrimaryUnit,
	                                                  ASquadController* PrimarySquad)
	{
		ATankMaster* TowVehicle = Cast<ATankMaster>(TargetActor);
		if (not IsValid(TowVehicle) || TowVehicle == PrimaryUnit)
		{
			return ERTSVerticalAnimatedIcon::None;
		}
		const UVehicleTowComponent* VehicleTowComponent = TowVehicle->FindComponentByClass<UVehicleTowComponent>();
		if (not IsValid(VehicleTowComponent) || not VehicleTowComponent->IsTowFree())
		{
			return ERTSVerticalAnimatedIcon::None;
		}
		if (GetIsPrimaryFreeToBeTowedBy(PrimaryUnit, TowVehicle))
		{
			return ERTSVerticalAnimatedIcon::Command_TowActor;
		}
		return PredictEnterCargo(PrimarySquad);
	}

	// Mirrors IssueOrderTowActor_ClickedTowableActor: only a selected tank with a free hook tows.
	ERTSVerticalAnimatedIcon PredictClickedTowableActor(AActor* TargetActor, AActor* PrimaryUnit)
	{
		if (TargetActor == PrimaryUnit || not PrimaryUnit->IsA(ATankMaster::StaticClass()))
		{
			return ERTSVerticalAnimatedIcon::None;
		}
		const UTowedActorComponent* TargetTowedComponent = TargetActor->FindComponentByClass<UTowedActorComponent>();
		const UVehicleTowComponent* PrimaryTowComponent = PrimaryUnit->FindComponentByClass<UVehicleTowComponent>();
		const bool bCanTow = IsValid(TargetTowedComponent) && TargetTowedComponent->IsTowFree()
			&& IsValid(PrimaryTowComponent) && PrimaryTowComponent->IsTowFree();
		return IconIfAllowed(bCanTow, ERTSVerticalAnimatedIcon::Command_TowActor);
	}

	ERTSVerticalAnimatedIcon PredictForCommandType(const ECommandType CommandType, AActor* TargetActor,
	                                               AActor* PrimaryUnit, ICommands& PrimaryCommands)
	{
		ASquadController* PrimarySquad = Cast<ASquadController>(PrimaryUnit);
		switch (CommandType)
		{
		case ECommandType::Attack:
		case ECommandType::EnemyActor:
		case ECommandType::EnemyCharacter:
			return PredictAttack(PrimaryUnit, PrimaryCommands);
		case ECommandType::AlliedActor:
		case ECommandType::AlliedBuilding:
		case ECommandType::AlliedCharacter:
			return PredictRepair(TargetActor, PrimaryCommands);
		case ECommandType::HarvestResource:
			return PredictHarvest(PrimaryUnit, PrimaryCommands);
		case ECommandType::PickupItem:
			return PredictPickupItem(PrimarySquad);
		case ECommandType::ScavengeObject:
			return PredictScavenge(TargetActor, PrimarySquad);
		case ECommandType::EnterCargo:
			return PredictEnterCargo(PrimarySquad);
		case ECommandType::ManAbandonedTeamWeapon:
			return PredictManTeamWeapon(TargetActor, PrimarySquad);
		case ECommandType::CaptureActor:
			return PredictCapture(TargetActor, PrimarySquad);
		case ECommandType::AlliedAirBase:
			return PredictReturnToBase(PrimaryUnit);
		case ECommandType::ClickedTowVehicle:
			return PredictClickedTowVehicle(TargetActor, PrimaryUnit, PrimarySquad);
		case ECommandType::ClickedTowableActor:
			return PredictClickedTowableActor(TargetActor, PrimaryUnit);
		default:
			// Movement, rally points and decoded types the click dispatch turns into movement.
			return ERTSVerticalAnimatedIcon::None;
		}
	}
}

ERTSVerticalAnimatedIcon FHoverCommandPrediction::PredictCommandIcon(
	const UPlayerCommandTypeDecoder& CommandTypeDecoder,
	AActor* HoveredActor,
	AActor* PrimarySelectedUnit)
{
	if (not RTSFunctionLibrary::RTSIsValid(HoveredActor) || not RTSFunctionLibrary::RTSIsValid(PrimarySelectedUnit))
	{
		return ERTSVerticalAnimatedIcon::None;
	}
	ICommands* PrimaryCommands = Cast<ICommands>(PrimarySelectedUnit);
	if (PrimaryCommands == nullptr)
	{
		return ERTSVerticalAnimatedIcon::None;
	}
	// The decoder may redirect a turret to its owning vehicle, exactly as it does for the click.
	AActor* DecodedTargetActor = HoveredActor;
	FTargetUnion UnusedTarget;
	const ECommandType CommandType = CommandTypeDecoder.DecodeTargetedActor(DecodedTargetActor, UnusedTarget);
	if (not IsValid(DecodedTargetActor))
	{
		return ERTSVerticalAnimatedIcon::None;
	}
	return PredictForCommandType(CommandType, DecodedTargetActor, PrimarySelectedUnit, *PrimaryCommands);
}
