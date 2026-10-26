// Copyright (C) Bas Blokzijl - All rights reserved.

#include "VariantBatchThreeWeaponData.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace VariantBatchThreeWeaponsPrivate
{
	using DeveloperSettings::GameBalance::Weapons::DamagePerMM;
	using DeveloperSettings::GameBalance::Weapons::DamagePerTNTEquivalentGrams;
	using DeveloperSettings::GameBalance::Weapons::DamageFluxPercentage;
	using DeveloperSettings::GameBalance::Weapons::CooldownFluxPercentage;
	using DeveloperSettings::GameBalance::Weapons::ArmorPenFluxPercentage;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelAmountPerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelRangePerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelPenPerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelDamagePerTNTGram;
	using DeveloperSettings::GameBalance::Weapons::MortarAOEMlt;
	using DeveloperSettings::GameBalance::Weapons::MortarAccuracy;
	using DeveloperSettings::GameBalance::Weapons::Mortars::MortarArmorPenPerMM;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_DamageMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_TNTExplosiveGramsMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ShrapnelRangeMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ShrapnelDamageMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ShrapnelPenMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ShrapnelParticlesMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::Radixite_DamageMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::Radixite_ArmorPenMaxRangeMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::Radixite_AccuracyDelta;
	using DeveloperSettings::GamePlay::Projectile::BaseProjectileSpeed;
	using DeveloperSettings::GamePlay::Projectile::HighVelocityProjectileSpeed;
	using DeveloperSettings::GameBalance::Ranges::LightCannonRange;
	using DeveloperSettings::GameBalance::Ranges::LightArtilleryRange;
	using DeveloperSettings::GameBalance::Ranges::MediumCannonRange;

	constexpr float FlakCannonBonusDamageFactor =
		1 + DeveloperSettings::GameBalance::Weapons::DamageBonusFlakAmmoPercentage / 100.f;

	/**
	 * @brief HE-only weapons store values divided by the HE multipliers so the fired shell carries the plain formulas
	 * (Jagdpanzer IV/70 Siege pattern).
	 */
	void SetHeOnlyFiredValues(FWeaponData& WeaponData, const float FiredTntGrams, const float FiredDamage,
	                          const float FiredArmorPen, const float FiredShrapnelRange,
	                          const float FiredShrapnelDamage, const float FiredShrapnelParticles,
	                          const float FiredShrapnelPen)
	{
		WeaponData.ShellType = EWeaponShellType::Shell_HE;
		WeaponData.ShellTypes = {EWeaponShellType::Shell_HE};
		WeaponData.TNTExplosiveGrams = FiredTntGrams / HE_TNTExplosiveGramsMlt;
		WeaponData.BaseDamage = FiredDamage / HE_DamageMlt;
		WeaponData.ArmorPen = FiredArmorPen / HE_ArmorPenMlt;
		WeaponData.ArmorPenMaxRange = WeaponData.ArmorPen;
		WeaponData.ShrapnelRange = FiredShrapnelRange / HE_ShrapnelRangeMlt;
		WeaponData.ShrapnelDamage = FiredShrapnelDamage / HE_ShrapnelDamageMlt;
		WeaponData.ShrapnelParticles = FMath::RoundToInt(FiredShrapnelParticles / HE_ShrapnelParticlesMlt);
		WeaponData.ShrapnelPen = FiredShrapnelPen / HE_ShrapnelPenMlt;
	}

	void SetGroundFire(FRTSGroundFireSettings& GroundFire, const float RadiusCm, const float DurationSeconds,
	                   const float DamagePerSecond, const float VfxScale)
	{
		// Rear armor up to 20 burns fully, 40 not at all: infantry and soft vehicles, not tanks.
		constexpr float IncendiaryFullArmorPen = 20.f;
		constexpr float IncendiaryArmorPenFallOff = 5.f;
		constexpr float IncendiaryMaxArmorPen = 40.f;
		GroundFire.RadiusCm = RadiusCm;
		GroundFire.DurationSeconds = DurationSeconds;
		GroundFire.DamagePerSecond = DamagePerSecond;
		GroundFire.DamageFalloffExponent = 0.f;
		GroundFire.FullArmorPen = IncendiaryFullArmorPen;
		GroundFire.ArmorPenFallOff = IncendiaryArmorPenFallOff;
		GroundFire.MaxArmorPen = IncendiaryMaxArmorPen;
		GroundFire.VfxScale = VfxScale;
	}

	// Sd.Kfz. 140 AA: one barrel of the triple MG 151/15; the turret holds three of these weapons.
	void AddDrillingMg151(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const TwinMg151Data = WeaponDataMap.Find(EWeaponName::MG151_15MM_Twin);
		if (not TwinMg151Data)
		{
			return;
		}
		constexpr int32 DrillingMagazine = 30;
		// The 5.0 s crew + gun box drill is stretched to the reload time minus 0.25 s.
		constexpr float DrillingReloadSpeed = 5.25f;
		constexpr float DrillingCooldown = 0.30f;
		constexpr int32 DrillingAccuracy = 85;
		FWeaponData DrillingData = *TwinMg151Data;
		DrillingData.WeaponName = EWeaponName::MG151_15MM_Drilling;
		DrillingData.Range = LightCannonRange;
		DrillingData.MagCapacity = DrillingMagazine;
		DrillingData.ReloadSpeed = DrillingReloadSpeed;
		DrillingData.BaseCooldown = DrillingCooldown;
		DrillingData.Accuracy = DrillingAccuracy;
		WeaponDataMap.Add(EWeaponName::MG151_15MM_Drilling, DrillingData);
	}

	// Sd.Kfz. 140 Bunker: the casemate's paired 5 cm mortar, HE only, with the autocannon's range.
	void AddBunkerMortar(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		constexpr float Calibre = 50.f;
		constexpr float FiredTntGrams = 90.f;
		constexpr float MortarShrapnelDamageMlt = 1.45f;
		FWeaponData MortarData = {};
		MortarData.WeaponName = EWeaponName::Mortar_50MM_140Bunker;
		MortarData.DamageType = ERTSDamageType::Kinetic;
		MortarData.WeaponCalibre = Calibre;
		SetHeOnlyFiredValues(MortarData, FiredTntGrams,
		                     DamagePerMM * Calibre + FiredTntGrams * DamagePerTNTEquivalentGrams,
		                     MortarArmorPenPerMM * Calibre,
		                     Calibre * ShrapnelRangePerMM * MortarAOEMlt,
		                     MortarShrapnelDamageMlt * FiredTntGrams * ShrapnelDamagePerTNTGram,
		                     Calibre * ShrapnelAmountPerMM,
		                     Calibre * ShrapnelPenPerMM);
		MortarData.DamageFlux = DamageFluxPercentage;
		// The user: "the same range" as the autocannon.
		MortarData.Range = LightCannonRange;
		MortarData.MagCapacity = 3;
		MortarData.ReloadSpeed = 7.f;
		MortarData.BaseCooldown = 1.5f;
		MortarData.CooldownFlux = CooldownFluxPercentage;
		MortarData.Accuracy = MortarAccuracy;
		MortarData.ProjectileMovementSpeed = BaseProjectileSpeed;
		WeaponDataMap.Add(EWeaponName::Mortar_50MM_140Bunker, MortarData);
	}

	// Puma Dual Barrel: each MK 103 is the KwK 31 with a 3-round magazine.
	void AddPumaDualMk103(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const Kwk31Data = WeaponDataMap.Find(EWeaponName::Kwk31_30MM);
		if (not Kwk31Data)
		{
			return;
		}
		constexpr int32 DualBarrelMagazine = 3;
		FWeaponData DualData = *Kwk31Data;
		DualData.WeaponName = EWeaponName::MK103_30MM_PumaDual;
		DualData.MagCapacity = DualBarrelMagazine;
		WeaponDataMap.Add(EWeaponName::MK103_30MM_PumaDual, DualData);
	}

	// Puma Radixite Sniper: the 50 mm laser as one shot of seven Puma shots, 1.67x the Puma's reload.
	void AddRadixiteSniper(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const LaserData = WeaponDataMap.Find(EWeaponName::Strahlkanone39);
		const FWeaponData* const PumaGunData = WeaponDataMap.Find(EWeaponName::KwK39_1_50MM_Puma);
		if (not LaserData || not PumaGunData)
		{
			return;
		}
		constexpr float SniperReloadMlt = 1.67f;
		const float PumaShotDamage = PumaGunData->BaseDamage;
		const float PumaReloadSpeed = PumaGunData->ReloadSpeed;
		FWeaponData SniperData = *LaserData;
		SniperData.WeaponName = EWeaponName::RadixiteSniper_50MM_Laser;
		SniperData.MagCapacity = 1;
		SniperData.ReloadSpeed = PumaReloadSpeed * SniperReloadMlt;
		// Laser damage is dealt per beam iteration.
		SniperData.BaseDamage = VariantBatchThreeWeapons::RadixiteSniperDamageInPumaShots * PumaShotDamage
			/ VariantBatchThreeWeapons::RadixiteSniperBeamIterations;
		WeaponDataMap.Add(EWeaponName::RadixiteSniper_50MM_Laser, SniperData);
	}

	/**
	 * @brief Radixite-only variant of a cannon: the table damage reaches EffectiveDamageMlt x the source after the
	 * radixite shell's x1.2, and the shell's accuracy and max-range pen penalties are cancelled (user default).
	 */
	FWeaponData MakeRadixiteCannon(const FWeaponData& SourceData, const EWeaponName NewWeaponName,
	                               const float EffectiveDamageMlt, const float ReloadSpeedMlt,
	                               const float DotTotalDamage, const float DotDurationSeconds)
	{
		FWeaponData RadixiteData = SourceData;
		RadixiteData.WeaponName = NewWeaponName;
		RadixiteData.ShellType = EWeaponShellType::Shell_Radixite;
		RadixiteData.BaseDamage = SourceData.BaseDamage * EffectiveDamageMlt / Radixite_DamageMlt;
		RadixiteData.ReloadSpeed = SourceData.ReloadSpeed * ReloadSpeedMlt;
		RadixiteData.Accuracy = SourceData.Accuracy - Radixite_AccuracyDelta;
		RadixiteData.ArmorPenMaxRange = SourceData.ArmorPenMaxRange / Radixite_ArmorPenMaxRangeMlt;
		RadixiteData.RadixiteDotTotalDamage = DotTotalDamage;
		RadixiteData.RadixiteDotDurationSeconds = DotDurationSeconds;
		return RadixiteData;
	}

	// Pz II F Radixite Autocannon: +30% damage, reload x1.5, 30 radixite damage over 4 s refreshed per hit.
	void AddPzIIFRadixiteAutocannon(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const KwK30Data = WeaponDataMap.Find(EWeaponName::KwK30_20MM);
		if (not KwK30Data)
		{
			return;
		}
		FWeaponData RadixiteData = MakeRadixiteCannon(*KwK30Data, EWeaponName::KwK30_Radixite_20MM,
		                                              1.30f, 1.5f, 30.f, 4.f);
		RadixiteData.ShellTypes = {EWeaponShellType::Shell_Radixite};
		WeaponDataMap.Add(EWeaponName::KwK30_Radixite_20MM, RadixiteData);
	}

	// Pz III J Radixite Cannon: +25% damage on radixite rounds, reload x1.4, 50 over 5 s; HE stays selectable.
	void AddPzIIIJRadixiteCannon(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const KwK42Data = WeaponDataMap.Find(EWeaponName::KwK42_L_50MM);
		if (not KwK42Data)
		{
			return;
		}
		FWeaponData RadixiteData = MakeRadixiteCannon(*KwK42Data, EWeaponName::KwK42_Radixite_50MM,
		                                              1.25f, 1.4f, 50.f, 5.f);
		RadixiteData.ShellTypes = {EWeaponShellType::Shell_Radixite, EWeaponShellType::Shell_HE};
		WeaponDataMap.Add(EWeaponName::KwK42_Radixite_50MM, RadixiteData);
	}

	// Pz III J Rocket Launcher: eight 8 cm HEAT rockets; each does half a KwK 42 shot, salvo = 4 shots.
	void AddPzIIIJRocketRack(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const Rocket50Data = WeaponDataMap.Find(EWeaponName::Rocket_50mm);
		const FWeaponData* const KwK42Data = WeaponDataMap.Find(EWeaponName::KwK42_L_50MM);
		if (not Rocket50Data || not KwK42Data)
		{
			return;
		}
		constexpr float RackDamageInKwK42Shots = 0.5f;
		// HEAT x1.2 when fired: 90 mm, the same at every range.
		constexpr float RackTableArmorPen = 75.f;
		const float KwK42ShotDamage = KwK42Data->BaseDamage;
		FWeaponData RackData = *Rocket50Data;
		RackData.WeaponName = EWeaponName::Rocket_80mm_PzIIIRack;
		RackData.ShellType = EWeaponShellType::Shell_HEAT;
		RackData.ShellTypes = {EWeaponShellType::Shell_HEAT};
		RackData.WeaponCalibre = 80.f;
		RackData.TNTExplosiveGrams = 30.f;
		RackData.BaseDamage = KwK42ShotDamage * RackDamageInKwK42Shots;
		RackData.Range = MediumCannonRange;
		RackData.ArmorPen = RackTableArmorPen;
		RackData.ArmorPenMaxRange = RackTableArmorPen;
		RackData.MagCapacity = 8;
		RackData.ReloadSpeed = 24.f;
		RackData.BaseCooldown = 0.35f;
		RackData.Accuracy = 50;
		RackData.ShrapnelRange = RackData.WeaponCalibre * ShrapnelRangePerMM;
		RackData.ShrapnelDamage = RackData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
		RackData.ShrapnelParticles = FMath::RoundToInt(RackData.WeaponCalibre * ShrapnelAmountPerMM);
		RackData.ShrapnelPen = RackData.WeaponCalibre * ShrapnelPenPerMM;
		WeaponDataMap.Add(EWeaponName::Rocket_80mm_PzIIIRack, RackData);
	}

	// Sd.Kfz. 234/3 Incendiary Support: the Stummel firing napalm HE (the KwK 37's HE) that leaves burning ground.
	void AddNapalmStummel(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const KwK37Data = WeaponDataMap.Find(EWeaponName::KwK37_75MM);
		if (not KwK37Data)
		{
			return;
		}
		FWeaponData NapalmData = *KwK37Data;
		NapalmData.WeaponName = EWeaponName::KwK37_75MM_Napalm;
		NapalmData.ShellType = EWeaponShellType::Shell_HE;
		NapalmData.ShellTypes = {EWeaponShellType::Shell_HE};
		SetGroundFire(NapalmData.GroundFire, 350.f, 8.f, 20.f, 1.6f);
		WeaponDataMap.Add(EWeaponName::KwK37_75MM_Napalm, NapalmData);
	}

	// Sd.Kfz. 234/3 Incendiary Support: each rear flame projector (an arch weapon, used twice).
	void AddFlameCharge(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const KwK37Data = WeaponDataMap.Find(EWeaponName::KwK37_75MM);
		if (not KwK37Data)
		{
			return;
		}
		constexpr float Calibre = 50.f;
		constexpr float FiredTntGrams = 40.f;
		const float StummelRange = KwK37Data->Range;
		FWeaponData FlameData = {};
		FlameData.WeaponName = EWeaponName::FlameCharge_50MM_234;
		FlameData.DamageType = ERTSDamageType::Kinetic;
		FlameData.WeaponCalibre = Calibre;
		SetHeOnlyFiredValues(FlameData, FiredTntGrams,
		                     DamagePerMM * Calibre + FiredTntGrams * DamagePerTNTEquivalentGrams,
		                     MortarArmorPenPerMM * Calibre,
		                     Calibre * ShrapnelRangePerMM,
		                     FiredTntGrams * ShrapnelDamagePerTNTGram,
		                     Calibre * ShrapnelAmountPerMM,
		                     Calibre * ShrapnelPenPerMM);
		FlameData.DamageFlux = DamageFluxPercentage;
		FlameData.Range = StummelRange;
		FlameData.MagCapacity = 1;
		// The 5.0 s loader drill is stretched to the reload time minus 0.25 s.
		FlameData.ReloadSpeed = 5.25f;
		FlameData.BaseCooldown = 1.f;
		FlameData.CooldownFlux = CooldownFluxPercentage;
		FlameData.Accuracy = MortarAccuracy;
		FlameData.ProjectileMovementSpeed = BaseProjectileSpeed;
		SetGroundFire(FlameData.GroundFire, 300.f, 7.f, 18.f, 1.4f);
		WeaponDataMap.Add(EWeaponName::FlameCharge_50MM_234, FlameData);
	}

	// Pz III AA Drilling: three Flak 38 barrels fired as one multi projectile weapon (stats per projectile).
	void AddDrillingFlak38(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const Flak38Data = WeaponDataMap.Find(EWeaponName::Flak38_20MM);
		if (not Flak38Data)
		{
			return;
		}
		FWeaponData DrillingData = *Flak38Data;
		DrillingData.WeaponName = EWeaponName::Flak38_20MM_Drilling;
		DrillingData.Range = LightCannonRange;
		DrillingData.MagCapacity = 20;
		// The 5.0 s two-man drill is stretched to the reload time minus 0.25 s.
		DrillingData.ReloadSpeed = 5.25f;
		DrillingData.BaseCooldown = 0.30f;
		DrillingData.Accuracy = 70;
		WeaponDataMap.Add(EWeaponName::Flak38_20MM_Drilling, DrillingData);
	}

	// Pz III AA FlaK 41: 5 cm flak with 5-round clips.
	void AddFlak41(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const Flak36Data = WeaponDataMap.Find(EWeaponName::Flak36_37MM);
		if (not Flak36Data)
		{
			return;
		}
		// TUNABLE. The request's 175 g would give 412 damage per shot, more than the 88 mm Maus AA Flak (302);
		// 45 g is the Flak 36's 18.2 g scaled by shell volume (50/37 cubed).
		constexpr float Flak41TntGrams = 45.f;
		FWeaponData Flak41Data = *Flak36Data;
		Flak41Data.WeaponName = EWeaponName::Flak41_50MM;
		Flak41Data.ShellType = EWeaponShellType::Shell_AP;
		Flak41Data.ShellTypes = {EWeaponShellType::Shell_AP};
		Flak41Data.WeaponCalibre = 50.f;
		Flak41Data.TNTExplosiveGrams = Flak41TntGrams;
		Flak41Data.BaseDamage = (DamagePerMM * Flak41Data.WeaponCalibre
			+ Flak41Data.TNTExplosiveGrams * DamagePerTNTEquivalentGrams) * FlakCannonBonusDamageFactor;
		Flak41Data.Range = MediumCannonRange;
		Flak41Data.ArmorPen = 72.f;
		Flak41Data.ArmorPenMaxRange = 55.f;
		Flak41Data.MagCapacity = 5;
		// The 3.0 s clip drill is stretched to the reload time minus 0.25 s.
		Flak41Data.ReloadSpeed = 3.25f;
		Flak41Data.BaseCooldown = 0.35f;
		Flak41Data.Accuracy = 70;
		Flak41Data.ProjectileMovementSpeed = HighVelocityProjectileSpeed;
		WeaponDataMap.Add(EWeaponName::Flak41_50MM, Flak41Data);
	}

	// Pz 38(t) Rockets Incendiary Salvo: six 15 cm fire rockets, each 3x one Panzerwerfer_Small rocket.
	void AddIncendiaryRocket150(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const PanzerwerferData = WeaponDataMap.Find(EWeaponName::Panzerwerfer_Small);
		if (not PanzerwerferData)
		{
			return;
		}
		constexpr float DamageInPanzerwerferRockets = 3.f;
		constexpr float IncendiaryProjectileSpeedMlt = 0.81f;
		const float PanzerwerferRocketDamage = PanzerwerferData->BaseDamage;
		FWeaponData IncendiaryData = {};
		IncendiaryData.WeaponName = EWeaponName::Rocket_150mm_Incendiary;
		IncendiaryData.DamageType = ERTSDamageType::Fire;
		// Fire shells skip the shell multipliers when fired: stored = fired.
		IncendiaryData.ShellType = EWeaponShellType::Shell_Fire;
		IncendiaryData.ShellTypes = {EWeaponShellType::Shell_Fire};
		IncendiaryData.WeaponCalibre = 150.f;
		IncendiaryData.TNTExplosiveGrams = 0.f;
		IncendiaryData.BaseDamage = PanzerwerferRocketDamage * DamageInPanzerwerferRockets;
		IncendiaryData.DamageFlux = DamageFluxPercentage;
		IncendiaryData.Range = LightArtilleryRange;
		IncendiaryData.ArmorPen = 20.f;
		IncendiaryData.ArmorPenMaxRange = 20.f;
		IncendiaryData.MagCapacity = 6;
		IncendiaryData.ReloadSpeed = 15.f;
		IncendiaryData.BaseCooldown = 0.6f;
		IncendiaryData.CooldownFlux = CooldownFluxPercentage;
		IncendiaryData.Accuracy = 20;
		IncendiaryData.ShrapnelRange = 450.f;
		IncendiaryData.ShrapnelDamage = 40.f;
		IncendiaryData.ShrapnelParticles = FMath::RoundToInt(IncendiaryData.WeaponCalibre * ShrapnelAmountPerMM);
		IncendiaryData.ShrapnelPen = 0.f;
		IncendiaryData.ProjectileMovementSpeed = BaseProjectileSpeed * IncendiaryProjectileSpeedMlt;
		// No lingering ground fire (user default R4).
		WeaponDataMap.Add(EWeaponName::Rocket_150mm_Incendiary, IncendiaryData);
	}

	// Pz 38(t) Rockets Precision Missile: the Panzerwerfer_Small formulas at 200 mm (all linear in calibre), so one
	// missile is five base rockets: 465 damage, and the pen formula gives 341 mm when fired (TUNABLE, maybe cap 180).
	void AddPrecisionMissile200(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
	{
		const FWeaponData* const PanzerwerferData = WeaponDataMap.Find(EWeaponName::Panzerwerfer_Small);
		if (not PanzerwerferData)
		{
			return;
		}
		constexpr float MissileCalibre = 200.f;
		const float CalibreScale = MissileCalibre / PanzerwerferData->WeaponCalibre;
		FWeaponData MissileData = *PanzerwerferData;
		MissileData.WeaponName = EWeaponName::Rocket_200mm_PrecisionMissile;
		MissileData.ShellType = EWeaponShellType::Shell_HEAT;
		MissileData.ShellTypes = {EWeaponShellType::Shell_HEAT};
		MissileData.WeaponCalibre = MissileCalibre;
		MissileData.TNTExplosiveGrams = PanzerwerferData->TNTExplosiveGrams * CalibreScale;
		MissileData.BaseDamage = PanzerwerferData->BaseDamage * CalibreScale;
		MissileData.ArmorPen = PanzerwerferData->ArmorPen * CalibreScale;
		MissileData.ArmorPenMaxRange = PanzerwerferData->ArmorPenMaxRange * CalibreScale;
		MissileData.Range = LightArtilleryRange;
		// Informational: the vertical rocket state uses the launcher's socket count as the magazine.
		MissileData.MagCapacity = 2;
		MissileData.ReloadSpeed = 10.f;
		MissileData.BaseCooldown = 1.f;
		MissileData.Accuracy = 70;
		MissileData.ShrapnelRange = MissileCalibre * ShrapnelRangePerMM;
		MissileData.ShrapnelDamage = MissileData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
		MissileData.ShrapnelParticles = FMath::RoundToInt(MissileCalibre * ShrapnelAmountPerMM);
		MissileData.ShrapnelPen = MissileCalibre * ShrapnelPenPerMM;
		WeaponDataMap.Add(EWeaponName::Rocket_200mm_PrecisionMissile, MissileData);
	}

	void SetRocketFlux(FAttachedRocketsData& RocketData)
	{
		RocketData.DamageFluxMlt = DamageFluxPercentage / 100;
		RocketData.CoolDownFluxMlt = CooldownFluxPercentage / 100;
		RocketData.ArmorPenFluxMlt = ArmorPenFluxPercentage / 100;
	}
}

void VariantBatchThreeWeapons::AddWeapons(TMap<EWeaponName, FWeaponData>& WeaponDataMap)
{
	using namespace VariantBatchThreeWeaponsPrivate;
	AddDrillingMg151(WeaponDataMap);
	AddBunkerMortar(WeaponDataMap);
	AddPumaDualMk103(WeaponDataMap);
	AddRadixiteSniper(WeaponDataMap);
	AddPzIIFRadixiteAutocannon(WeaponDataMap);
	AddPzIIIJRadixiteCannon(WeaponDataMap);
	AddPzIIIJRocketRack(WeaponDataMap);
	AddNapalmStummel(WeaponDataMap);
	AddFlameCharge(WeaponDataMap);
	AddDrillingFlak38(WeaponDataMap);
	AddFlak41(WeaponDataMap);
	AddIncendiaryRocket150(WeaponDataMap);
	AddPrecisionMissile200(WeaponDataMap);
}

void VariantBatchThreeWeapons::AddRocketData(TMap<ERocketAbility, FAttachedRocketsData>& RocketDataMap)
{
	using namespace VariantBatchThreeWeaponsPrivate;

	// Sd.Kfz. 251 Mortar Incendiary: six 32 cm fire rockets 0.5 s apart, each leaves the Pz IV incendiary's
	// burning ground (450 cm, 9 s, 25 DPS).
	FAttachedRocketsData IncendiaryRockets = {};
	IncendiaryRockets.DamageType = ERTSDamageType::Fire;
	IncendiaryRockets.ShellType = EWeaponShellType::Shell_Fire;
	IncendiaryRockets.WeaponCalibre = 320;
	IncendiaryRockets.Damage = 180.f;
	IncendiaryRockets.ArmorPen = 20.f;
	IncendiaryRockets.Range = 9000.f;
	IncendiaryRockets.ProjectileSpeed = 3500.f;
	IncendiaryRockets.ProjectileAccelerationFactor = 1.5f;
	IncendiaryRockets.Cooldown = 0.5f;
	IncendiaryRockets.ReloadSpeed = 60.f;
	IncendiaryRockets.Accuracy = 40;
	SetRocketFlux(IncendiaryRockets);
	SetGroundFire(IncendiaryRockets.GroundFire, 450.f, 9.f, 25.f, 2.f);
	RocketDataMap.Add(ERocketAbility::Sdkfz251IncendiaryRockets, IncendiaryRockets);

	// Sd.Kfz. 251 Mortar Guided Missile: four missiles 0.9 s apart homing onto one target per salvo (the current
	// target or the closest enemy within 60 degrees of the heading).
	FAttachedRocketsData GuidedMissiles = {};
	GuidedMissiles.DamageType = ERTSDamageType::Kinetic;
	GuidedMissiles.ShellType = EWeaponShellType::Shell_HEAT;
	GuidedMissiles.WeaponCalibre = 180;
	GuidedMissiles.Damage = 280.f;
	GuidedMissiles.ArmorPen = 170.f;
	GuidedMissiles.Range = 11000.f;
	GuidedMissiles.ProjectileSpeed = 3000.f;
	GuidedMissiles.ProjectileAccelerationFactor = 2.f;
	GuidedMissiles.Cooldown = 0.9f;
	GuidedMissiles.ReloadSpeed = 50.f;
	GuidedMissiles.Accuracy = 85;
	SetRocketFlux(GuidedMissiles);
	GuidedMissiles.bHomingMissiles = true;
	GuidedMissiles.HomingConeHalfAngleDegrees = 60.f;
	GuidedMissiles.HomingSettings.TurnResponsiveness = 0.3f;
	RocketDataMap.Add(ERocketAbility::Sdkfz251GuidedMissiles, GuidedMissiles);
}
