// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_ArmoredTroopBay.h"

#include "RTS_Survival/RTSComponents/ArmorComponent/Armor.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/TechTree/Technologies/TechHelpers/RTSTechHelpers.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"

namespace ArmoredTroopBayTechnologySettings
{
	constexpr float HullArmorMultiplier = 1.2f;
	constexpr float PassengerHealthFractionKept = 0.5f;
	// Every hull plate; half-tracks register only some of them, the others are ignored.
	const TArray<EArmorPlate> HullPlates = {
		EArmorPlate::Plate_Front,
		EArmorPlate::Plate_FrontUpperGlacis,
		EArmorPlate::Plate_FrontLowerGlacis,
		EArmorPlate::Plate_SideLeft,
		EArmorPlate::Plate_SideRight,
		EArmorPlate::Plate_SideLowerLeft,
		EArmorPlate::Plate_SideLowerRight,
		EArmorPlate::Plate_Rear,
		EArmorPlate::Plate_RearLowerGlacis,
		EArmorPlate::Plate_RearUpperGlacis
	};
}

UTE_ArmoredTroopBay::UTE_ArmoredTroopBay()
	: M_HullArmorMultiplier(ArmoredTroopBayTechnologySettings::HullArmorMultiplier)
	, M_PassengerHealthFractionKept(ArmoredTroopBayTechnologySettings::PassengerHealthFractionKept)
{
	Technology = ETechnology::Tech_HalfTrack_ArmoredBay;
	TanksToApplyTo = {
		ETankSubtype::Tank_Sdkfz250,
		ETankSubtype::Tank_Sdkfz250_37mm,
		ETankSubtype::Tank_Sdkfz251,
		ETankSubtype::Tank_Sdkfz251_PZIV,
		ETankSubtype::Tank_Sdkfz251_22,
		ETankSubtype::Tank_Sdkfz251_Mortar,
		ETankSubtype::Tank_Sdkfz251_Transport,
	};
}

void UTE_ArmoredTroopBay::ApplyOnTank_Internal(ATankMaster* Tank)
{
	if (not IsValid(Tank))
	{
		return;
	}
	RTSTechHelpers::ApplyArmorValueMultiplierToMatchingPlates(
		Tank, ArmoredTroopBayTechnologySettings::HullPlates, M_HullArmorMultiplier);
	// Only the troop-carrying variants have a cargo bay.
	if (UCargo* const Cargo = Tank->FindComponentByClass<UCargo>())
	{
		Cargo->SetPassengersBailOutOnOwnerDeath(M_PassengerHealthFractionKept);
	}
}
