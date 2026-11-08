// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FieldWorkshopRearmAura.h"

#include "RTS_Survival/Behaviours/Behaviour.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UFieldWorkshopRearmAura::UFieldWorkshopRearmAura()
	: URadiusAOEBehaviourComponent()
{
	RadiusSettings.RadiusType = ERTSRadiusType::FullCircle_AmmoResupplyAura;
	// The workshop draws its own radius while it works; hover and selection would stack a second disc on it.
	RadiusSettings.ShowRadiusType = EAOeBehaviourShowRadiusType::None;
	// The reload buff is permanent while in range: applied once on entering, removed on leaving.
	AOEBehaviourSettings.ApplyStrategy = EInAOEBehaviourApplyStrategy::ApplyOnlyOnEnter;
	AOEBehaviourSettings.IntervalSeconds = 1.f;
	// Soldiers and gun crews carry no behaviour component of their own: the buff goes on their squad.
	AOEBehaviourSettings.bAffectSquadControllers = true;
}

bool UFieldWorkshopRearmAura::IsValidTarget(AActor* ValidActor) const
{
	// Vehicles, and soldiers of infantry squads and team weapon crews (their squad controller gets the buff).
	const bool bIsVehicle = IsValid(Cast<ATankMaster>(ValidActor));
	const bool bIsSoldier = IsValid(Cast<ASquadUnit>(ValidActor));
	return (bIsVehicle || bIsSoldier) && RTSFunctionLibrary::RTSIsValid(ValidActor);
}

void UFieldWorkshopRearmAura::SetHostBehaviourUIData(UBehaviour& Behaviour) const
{
	constexpr float CentimetresPerMetre = 100.f;
	FBehaviourUIData UIData = Behaviour.GetUIData();
	UIData.TitleText = RadiusSettings.HostBehaviourTitleText;
	UIData.BehaviourIcon = RadiusSettings.HostBehaviourIcon;
	UIData.DescriptionText = FString::Printf(
		TEXT("Your vehicles, infantry and guns within <Text_Exp>%d m</> reload faster."),
		FMath::RoundToInt(GetAOERadius() / CentimetresPerMetre));
	Behaviour.SetCustomUIData(UIData);
}
