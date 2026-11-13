// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SmallArmsMuzzleClass.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

ESmallArmsMuzzleClass SmallArmsMuzzleClass::GetMuzzleClassForWeapon(const EWeaponName WeaponName)
{
	switch (WeaponName)
	{
	case EWeaponName::M1895_Nagant:
		return ESmallArmsMuzzleClass::Pistol;

	case EWeaponName::MP40:
	case EWeaponName::MP40_9MM:
	case EWeaponName::PPSh_41_7_62MM:
	case EWeaponName::PPS_43_7_62MM:
	case EWeaponName::Korovin_M1941_7_62MM:
		return ESmallArmsMuzzleClass::SubmachineGun;

	case EWeaponName::STG44_7_92MM:
	case EWeaponName::FG_42_7_92MM:
	case EWeaponName::Fedrov_Avtomat:
		return ESmallArmsMuzzleClass::AssaultRifle;

	case EWeaponName::Kar_98k:
	case EWeaponName::Kar_Sniper:
	case EWeaponName::Mauser:
	case EWeaponName::Mosin:
	case EWeaponName::Mosin_Snip:
	case EWeaponName::SKS:
	case EWeaponName::SVT_40_7_62MM:
		return ESmallArmsMuzzleClass::Rifle;

	case EWeaponName::MG_34:
	case EWeaponName::MP46:
	case EWeaponName::DP_28_7_62MM:
	case EWeaponName::DS39_7_62MM:
	case EWeaponName::RipperGun_7_62MM:
		return ESmallArmsMuzzleClass::MachineGun;

	case EWeaponName::PTRS_41_14_5MM:
	case EWeaponName::PTRS_X_Tishina:
	case EWeaponName::PTRS_50MM:
	case EWeaponName::M920_AtSniper:
		return ESmallArmsMuzzleClass::AntiTankRifle;

	default:
		return ESmallArmsMuzzleClass::None;
	}
}

FSmallArmsMuzzleProfile SmallArmsMuzzleClass::GetDefaultProfile(const ESmallArmsMuzzleClass MuzzleClass)
{
	// FlashScale, SmokeScale, SmokeLifetimeMlt, MinSecondsBetweenSmokePuffs.
	auto MakeProfile = [](const float FlashScale, const float SmokeScale, const float SmokeLifetimeMlt,
	                      const float MinSecondsBetweenSmokePuffs)
	{
		FSmallArmsMuzzleProfile Profile;
		Profile.FlashScale = FlashScale;
		Profile.SmokeScale = SmokeScale;
		Profile.SmokeLifetimeMlt = SmokeLifetimeMlt;
		Profile.MinSecondsBetweenSmokePuffs = MinSecondsBetweenSmokePuffs;
		return Profile;
	};
	switch (MuzzleClass)
	{
	case ESmallArmsMuzzleClass::Pistol:
		return MakeProfile(0.5f, 0.45f, 0.75f, 0.0f);
	case ESmallArmsMuzzleClass::SubmachineGun:
		return MakeProfile(0.6f, 0.55f, 0.85f, 0.14f);
	case ESmallArmsMuzzleClass::AssaultRifle:
		return MakeProfile(0.8f, 0.75f, 0.95f, 0.14f);
	case ESmallArmsMuzzleClass::Rifle:
		return MakeProfile(1.0f, 1.0f, 1.0f, 0.0f);
	case ESmallArmsMuzzleClass::MachineGun:
		return MakeProfile(1.0f, 0.85f, 1.15f, 0.12f);
	case ESmallArmsMuzzleClass::AntiTankRifle:
		return MakeProfile(1.9f, 1.9f, 1.4f, 0.0f);
	case ESmallArmsMuzzleClass::None:
		break;
	}
	return FSmallArmsMuzzleProfile();
}
