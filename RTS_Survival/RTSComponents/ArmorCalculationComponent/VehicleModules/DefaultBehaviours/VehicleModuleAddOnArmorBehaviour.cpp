// Copyright (C) Bas Blokzijl - All rights reserved.

#include "VehicleModuleAddOnArmorBehaviour.h"

#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleBalance.h"

namespace VehicleModuleAddOnArmorConstants
{
	constexpr float FullContributionMultiplier = 1.f;
}

UVehicleModuleAddOnArmorBehaviour::UVehicleModuleAddOnArmorBehaviour()
{
	BehaviourLifeTime = EBehaviourLifeTime::None;
	M_BuffType = EBuffDebuffType::Debuff;
	M_TitleText = TEXT("Damaged Add-on Armor");
	M_DisplayText = TEXT("Covered add-on armor plates give less protection; structural armor remains.");
}

void UVehicleModuleAddOnArmorBehaviour::OnAdded(AActor* BehaviourOwner)
{
	ApplyContributionForContext();
	// Make sure to call the bp event.
	Super::OnAdded(BehaviourOwner);
}

void UVehicleModuleAddOnArmorBehaviour::OnRemoved(AActor* BehaviourOwner)
{
	// Restores only this zone's add-on contribution; no stale base values are written back.
	UArmorCalculation* ArmorCalculation = GetModuleArmorCalculation();
	if (IsValid(ArmorCalculation))
	{
		ArmorCalculation->SetAddOnArmorContributionMultiplier(
			GetModuleContext().ModuleId, VehicleModuleAddOnArmorConstants::FullContributionMultiplier);
	}
	// Make sure to call the bp event.
	Super::OnRemoved(BehaviourOwner);
}

void UVehicleModuleAddOnArmorBehaviour::OnModuleContextUpdated(const FVehicleModuleBehaviourContext& PreviousContext)
{
	ApplyContributionForContext();
	Super::OnModuleContextUpdated(PreviousContext);
}

void UVehicleModuleAddOnArmorBehaviour::ApplyContributionForContext() const
{
	UArmorCalculation* ArmorCalculation = GetModuleArmorCalculation();
	const FVehicleModuleBehaviourContext& ModuleContext = GetModuleContext();
	if (not IsValid(ArmorCalculation) || ModuleContext.Type != EVehicleModuleTypes::AddOnArmor)
	{
		return;
	}
	ArmorCalculation->SetAddOnArmorContributionMultiplier(
		ModuleContext.ModuleId, GetContributionMultiplierForState(ModuleContext.State));
}

float UVehicleModuleAddOnArmorBehaviour::GetContributionMultiplierForState(const EVehicleModuleState State) const
{
	using namespace VehicleModuleBalance::DefaultBehaviours;
	switch (State)
	{
	case EVehicleModuleState::Damaged:
		return AddOnArmorYellowContributionMultiplier;
	case EVehicleModuleState::Destroyed:
		return AddOnArmorDestroyedContributionMultiplier;
	default:
		return VehicleModuleAddOnArmorConstants::FullContributionMultiplier;
	}
}
