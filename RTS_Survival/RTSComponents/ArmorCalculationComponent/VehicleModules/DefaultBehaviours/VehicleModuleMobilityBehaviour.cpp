// Copyright (C) Bas Blokzijl - All rights reserved.

#include "VehicleModuleMobilityBehaviour.h"

#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleBalance.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UVehicleModuleMobilityBehaviour::UVehicleModuleMobilityBehaviour()
{
	BehaviourLifeTime = EBehaviourLifeTime::None;
	M_BuffType = EBuffDebuffType::Debuff;
	M_TitleText = TEXT("Damaged Mobility");
	M_DisplayText = TEXT("A damaged engine or running gear limits travel speed and turning.");
}

void UVehicleModuleMobilityBehaviour::OnAdded(AActor* BehaviourOwner)
{
	M_TankMaster = Cast<ATankMaster>(BehaviourOwner);
	ApplyMobilityRestriction();
	// Make sure to call the bp event.
	Super::OnAdded(BehaviourOwner);
}

void UVehicleModuleMobilityBehaviour::OnRemoved(AActor* BehaviourOwner)
{
	// Removes only this module's restriction; other sources keep theirs.
	if (GetIsValidTankMaster())
	{
		M_TankMaster->ClearMobilityRestriction(this);
	}
	M_TankMaster.Reset();
	// Make sure to call the bp event.
	Super::OnRemoved(BehaviourOwner);
}

void UVehicleModuleMobilityBehaviour::OnModuleContextUpdated(const FVehicleModuleBehaviourContext& PreviousContext)
{
	// Same class for yellow and red: only the registered limit changes, no remove/add cycle.
	ApplyMobilityRestriction();
	Super::OnModuleContextUpdated(PreviousContext);
}

void UVehicleModuleMobilityBehaviour::ApplyMobilityRestriction()
{
	if (not GetIsValidTankMaster())
	{
		return;
	}
	float TravelSpeedMultiplier = 1.f;
	float TurnRateMultiplier = 1.f;
	float AccelerationMultiplier = 1.f;
	GetMobilityMultipliersForContext(TravelSpeedMultiplier, TurnRateMultiplier, AccelerationMultiplier);
	M_TankMaster->SetMobilityRestriction(this, TravelSpeedMultiplier, TurnRateMultiplier, AccelerationMultiplier);
}

void UVehicleModuleMobilityBehaviour::GetMobilityMultipliersForContext(float& OutTravelSpeedMultiplier,
	                                                                       float& OutTurnRateMultiplier,
	                                                                       float& OutAccelerationMultiplier) const
{
	using namespace VehicleModuleBalance::DefaultBehaviours;
	const FVehicleModuleBehaviourContext& ModuleContext = GetModuleContext();
	OutTravelSpeedMultiplier = 1.f;
	OutTurnRateMultiplier = 1.f;
	OutAccelerationMultiplier = 1.f;
	if (ModuleContext.State == EVehicleModuleState::Destroyed)
	{
		const float SelectedDestroyedMobilityMultiplier = ModuleContext.Type == EVehicleModuleTypes::Engine
			                                          ? DestroyedEngineMobilityMlt
			                                          : DestroyedTrackOrWheelMobilityMlt;
		OutTravelSpeedMultiplier = SelectedDestroyedMobilityMultiplier;
		OutTurnRateMultiplier = SelectedDestroyedMobilityMultiplier;
		return;
	}
	if (ModuleContext.State != EVehicleModuleState::Damaged)
	{
		return;
	}
	if (ModuleContext.Type == EVehicleModuleTypes::Engine)
	{
		OutTravelSpeedMultiplier = EngineYellowTravelMultiplier;
		OutAccelerationMultiplier = EngineYellowAccelerationMultiplier;
		return;
	}
	if (VehicleModuleBalance::GetIsRunningGearType(ModuleContext.Type))
	{
		OutTravelSpeedMultiplier = RunningGearYellowTravelMultiplier;
		OutTurnRateMultiplier = RunningGearYellowTurnMultiplier;
	}
}

bool UVehicleModuleMobilityBehaviour::GetIsValidTankMaster() const
{
	if (M_TankMaster.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_TankMaster",
		"GetIsValidTankMaster",
		this
	);
	return false;
}
