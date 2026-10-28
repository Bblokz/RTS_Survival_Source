// Copyright (C) Bas Blokzijl - All rights reserved.

#include "VariantBatchFourWeaponData.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace VariantBatchFourWeaponsPrivate
{
	using DeveloperSettings::GameBalance::Weapons::DamagePerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelAmountPerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelRangePerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelPenPerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelDamagePerTNTGram;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_DamageMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ProjectileSpeedMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::Radixite_DamageMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::Radixite_ArmorPenMaxRangeMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::Radixite_AccuracyDelta;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::APCR_DamageMlt;
	using DeveloperSettings::GameBalance::Ranges::MediumCannonRange;
	using DeveloperSettings::GameBalance::Ranges::MediumLaserWeaponRange;
	using DeveloperSettings::GameBalance::Ranges::HeavyArtilleryRange;

	/**
	 * @brief Wespe Radixite Howitzer: 20 cm radixite shells that fire like the base Wespe's HE shell (pen, accuracy,
	 * arc speed) at 1.5x its damage, plus 60 radixite damage over 6 s per direct hit. HE stays loadable; it uses the
	 * same stored values with the HE multipliers (see CPP_IMPL_DONE_B4.md for its fired numbers).
	 */
	void AddWespeRadixiteHowitzer(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const LeFHData = WeaponDataMap.Find(EWeaponName::LeFH_18_105MM);
		if (not LeFHData)
		{
			RTSFunctionLibrary::ReportError("VariantBatchFour: LeFH_18_105MM missing, LeFH_Radixite_200MM not added.");
			return;
		}
		using namespace VariantBatchFourWeapons;
		// The base Wespe always fires HE: these are its fired values.
		const float BaseFiredDamage = LeFHData->BaseDamage * HE_DamageMlt;
		const float BaseFiredArmorPen = LeFHData->ArmorPen * HE_ArmorPenMlt;
		const float BaseFiredProjectileSpeed = LeFHData->ProjectileMovementSpeed * HE_ProjectileSpeedMlt;
		constexpr float TntGrams = 686.f;
		FWeaponData RadixiteData = *LeFHData;
		RadixiteData.WeaponName = EWeaponName::LeFH_Radixite_200MM;
		RadixiteData.ShellType = EWeaponShellType::Shell_Radixite;
		// Radixite is the default; HE can be loaded (user W3).
		RadixiteData.ShellTypes = {EWeaponShellType::Shell_Radixite, EWeaponShellType::Shell_HE};
		RadixiteData.WeaponCalibre = RadixiteHowitzerCalibre;
		RadixiteData.TNTExplosiveGrams = TntGrams;
		RadixiteData.BaseDamage = BaseFiredDamage * RadixiteHowitzerDamageInBaseShells / Radixite_DamageMlt;
		// Radixite keeps the close-range pen and multiplies the max-range pen: both fire as the base HE's pen.
		RadixiteData.ArmorPen = BaseFiredArmorPen;
		RadixiteData.ArmorPenMaxRange = BaseFiredArmorPen / Radixite_ArmorPenMaxRangeMlt;
		RadixiteData.ReloadSpeed = LeFHData->ReloadSpeed * 2.f;
		// Cancels the radixite accuracy penalty: fires with the base's accuracy.
		RadixiteData.Accuracy = LeFHData->Accuracy - Radixite_AccuracyDelta;
		// Radixite does not slow the shell: store the base HE's fired speed so the arc stays the same.
		RadixiteData.ProjectileMovementSpeed = BaseFiredProjectileSpeed;
		RadixiteData.ShrapnelRange = RadixiteHowitzerCalibre * ShrapnelRangePerMM;
		RadixiteData.ShrapnelDamage = TntGrams * ShrapnelDamagePerTNTGram;
		RadixiteData.ShrapnelParticles = FMath::RoundToInt(RadixiteHowitzerCalibre * ShrapnelAmountPerMM);
		RadixiteData.ShrapnelPen = RadixiteHowitzerCalibre * ShrapnelPenPerMM;
		// Direct hits only (user W4: no splash damage over time).
		RadixiteData.RadixiteDotTotalDamage = RadixiteHowitzerDotTotalDamage;
		RadixiteData.RadixiteDotDurationSeconds = RadixiteHowitzerDotDurationSeconds;
		WeaponDataMap.Add(EWeaponName::LeFH_Radixite_200MM, RadixiteData);
	}

	// Pz III M Overcharged: the laser deals three KwK 39/1 shots in one beam and reloads four KwK 39/1 reloads.
	void AddPzIIIMLaser(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const LaserData = WeaponDataMap.Find(EWeaponName::Strahlkanone39);
		const FWeaponData* const KwK39Data = WeaponDataMap.Find(EWeaponName::KwK39_1_50MM);
		if (not LaserData || not KwK39Data)
		{
			RTSFunctionLibrary::ReportError("VariantBatchFour: Strahlkanone39 or KwK39_1_50MM missing, "
				"Laser_60MM_PzIIIM not added.");
			return;
		}
		using namespace VariantBatchFourWeapons;
		const float KwK39ShotDamage = KwK39Data->BaseDamage;
		const float KwK39ReloadSpeed = KwK39Data->ReloadSpeed;
		FWeaponData PzIIIMLaserData = *LaserData;
		PzIIIMLaserData.WeaponName = EWeaponName::Laser_60MM_PzIIIM;
		PzIIIMLaserData.WeaponCalibre = 60.f;
		// Laser damage is dealt per beam iteration.
		PzIIIMLaserData.BaseDamage = PzIIIMLaserDamageInKwK39Shots * KwK39ShotDamage / PzIIIMLaserBeamIterations;
		PzIIIMLaserData.ReloadSpeed = PzIIIMLaserReloadInKwK39Reloads * KwK39ReloadSpeed;
		PzIIIMLaserData.MagCapacity = 1;
		PzIIIMLaserData.BaseCooldown = 0.25f;
		PzIIIMLaserData.Accuracy = 100;
		PzIIIMLaserData.Range = MediumLaserWeaponRange;
		WeaponDataMap.Add(EWeaponName::Laser_60MM_PzIIIM, PzIIIMLaserData);
	}

	// Pz III M Spaced Armor: the Panzerschreck foxhole rocket with medium tank cannon range (user M7).
	void AddPzIIIMPanzerschreck(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const PitSchreckData = WeaponDataMap.Find(EWeaponName::PanzerSchreck_Pit);
		if (not PitSchreckData)
		{
			RTSFunctionLibrary::ReportError("VariantBatchFour: PanzerSchreck_Pit missing, PanzerSchreck_PzIIIM not added.");
			return;
		}
		FWeaponData SchreckData = *PitSchreckData;
		SchreckData.WeaponName = EWeaponName::PanzerSchreck_PzIIIM;
		SchreckData.Range = MediumCannonRange;
		WeaponDataMap.Add(EWeaponName::PanzerSchreck_PzIIIM, SchreckData);
	}

	// Hetzer Flame: the Pz III Flamm's Flamm 15 (same range) with 33% less damage.
	void AddHetzerHullFlamm(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const Flamm15Data = WeaponDataMap.Find(EWeaponName::Flamm15);
		if (not Flamm15Data)
		{
			RTSFunctionLibrary::ReportError("VariantBatchFour: Flamm15 missing, Flamm15_HetzerHull not added.");
			return;
		}
		FWeaponData HullFlammData = *Flamm15Data;
		HullFlammData.WeaponName = EWeaponName::Flamm15_HetzerHull;
		HullFlammData.BaseDamage = VariantBatchFourWeapons::HetzerHullFlameDamage;
		WeaponDataMap.Add(EWeaponName::Flamm15_HetzerHull, HullFlammData);
	}

	/**
	 * @brief Hetzer Mortar: the base 12 cm mortar (HE only) with x1.5 damage, x1.4 TNT (shrapnel damage follows the
	 * TNT) and a 7 s reload. The stored values are HE-neutralised, so scaling them scales the fired values the same.
	 */
	void AddHetzerMortar(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const MortarData = WeaponDataMap.Find(EWeaponName::Mortar_120MM);
		if (not MortarData)
		{
			RTSFunctionLibrary::ReportError("VariantBatchFour: Mortar_120MM missing, Mortar_120MM_Hetzer not added.");
			return;
		}
		using namespace VariantBatchFourWeapons;
		FWeaponData HetzerMortarData = *MortarData;
		HetzerMortarData.WeaponName = EWeaponName::Mortar_120MM_Hetzer;
		HetzerMortarData.ShellType = EWeaponShellType::Shell_HE;
		HetzerMortarData.ShellTypes = {EWeaponShellType::Shell_HE};
		HetzerMortarData.BaseDamage = MortarData->BaseDamage * HetzerMortarDamageMlt;
		HetzerMortarData.TNTExplosiveGrams = MortarData->TNTExplosiveGrams * HetzerMortarTntMlt;
		HetzerMortarData.ShrapnelDamage = MortarData->ShrapnelDamage * HetzerMortarTntMlt;
		HetzerMortarData.ReloadSpeed = HetzerMortarReloadSpeed;
		HetzerMortarData.MagCapacity = 1;
		WeaponDataMap.Add(EWeaponName::Mortar_120MM_Hetzer, HetzerMortarData);
	}

	// JagdPanther Pfeil: the PaK 43 firing APCR only; no explosive filler or shrapnel, more pen, longer range.
	void AddPfeilPak43(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const Pak43Data = WeaponDataMap.Find(EWeaponName::Pak43_88MM);
		if (not Pak43Data)
		{
			RTSFunctionLibrary::ReportError("VariantBatchFour: Pak43_88MM missing, Pak43_88MM_APCR_Pfeil not added.");
			return;
		}
		using namespace VariantBatchFourWeapons;
		FWeaponData PfeilData = *Pak43Data;
		PfeilData.WeaponName = EWeaponName::Pak43_88MM_APCR_Pfeil;
		PfeilData.ShellType = EWeaponShellType::Shell_APCR;
		PfeilData.ShellTypes = {EWeaponShellType::Shell_APCR};
		PfeilData.TNTExplosiveGrams = 0.f;
		// Stored before the APCR damage multiplier, so a fired shot deals PfeilFiredDamagePerShot (250).
		PfeilData.BaseDamage = PfeilFiredDamagePerShot / APCR_DamageMlt;
		PfeilData.ArmorPen = PfeilArmorPen;
		PfeilData.ArmorPenMaxRange = PfeilArmorPenMaxRange;
		PfeilData.Range = RTSFunctionLibrary::RoundToNearestMultipleOf(
			FMath::RoundToInt(Pak43Data->Range * PfeilRangeMlt), 10);
		PfeilData.Accuracy = PfeilAccuracy;
		PfeilData.ShrapnelRange = 0.f;
		PfeilData.ShrapnelDamage = 0.f;
		PfeilData.ShrapnelParticles = 0;
		PfeilData.ShrapnelPen = 0.f;
		WeaponDataMap.Add(EWeaponName::Pak43_88MM_APCR_Pfeil, PfeilData);
	}

	// JagdPanther Bollwerk: the towed sFH 18 (HE only) as heavy artillery with a slower reload and better accuracy.
	void AddBollwerkSfh18(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const TowedSfhData = WeaponDataMap.Find(EWeaponName::sFH18_150MM_TW);
		if (not TowedSfhData)
		{
			RTSFunctionLibrary::ReportError("VariantBatchFour: sFH18_150MM_TW missing, sFH18_150MM_Bollwerk not added.");
			return;
		}
		using namespace VariantBatchFourWeapons;
		FWeaponData BollwerkData = *TowedSfhData;
		BollwerkData.WeaponName = EWeaponName::sFH18_150MM_Bollwerk;
		BollwerkData.ShellType = EWeaponShellType::Shell_HE;
		BollwerkData.ShellTypes = {EWeaponShellType::Shell_HE};
		BollwerkData.Range = RTSFunctionLibrary::RoundToNearestMultipleOf(
			FMath::RoundToInt(HeavyArtilleryRange), 100);
		BollwerkData.ReloadSpeed = BollwerkReloadSpeed;
		BollwerkData.Accuracy = BollwerkAccuracy;
		BollwerkData.MagCapacity = 1;
		BollwerkData.BaseCooldown = 1.f;
		WeaponDataMap.Add(EWeaponName::sFH18_150MM_Bollwerk, BollwerkData);
	}
}

void VariantBatchFourWeapons::AddWeapons(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
{
	using namespace VariantBatchFourWeaponsPrivate;
	AddWespeRadixiteHowitzer(WeaponDataMap);
	AddPzIIIMLaser(WeaponDataMap);
	AddPzIIIMPanzerschreck(WeaponDataMap);
	AddHetzerHullFlamm(WeaponDataMap);
	AddHetzerMortar(WeaponDataMap);
	AddPfeilPak43(WeaponDataMap);
	AddBollwerkSfh18(WeaponDataMap);
}
