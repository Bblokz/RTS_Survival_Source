// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponShellType/WeaponShellType.h"
#include "VehicleModuleTypes.h"

/**
 * The only definition site for vehicle module tuning: probabilities, damage multipliers, HP thresholds,
 * repair settings, default behaviour penalties, source gates, anti-spam limits and UI defaults.
 * Fractions use the 01 suffix, calibre uses millimetres and time uses seconds.
 */
namespace VehicleModuleBalance
{
	/** @brief Chance to ignite an engine when damage changes its module state; HE and HEAT use separate odds. */
		namespace EngineFire
	{
		inline constexpr float EngineFireChanceDamagedEngine = 0.25f;
		inline constexpr float EngineFireChanceDestroyedEngine = 0.5f;
		inline constexpr float EngineFireChanceDamagedEngine_HE_HEAT = 0.33f;
		inline constexpr float EngineFireChanceDestroyedEngine_HE_HEAT = 0.8f;

		/**
		 * @brief Keeps the shell and engine state rules beside their tuning values.
		 * @param EngineState State reached by the engine after damage.
		 * @param ShellType Shell responsible for that state change.
		 * @return Fire probability, or zero when the engine remains healthy.
		 */
		constexpr float GetChance(const EVehicleModuleState EngineState, const EWeaponShellType ShellType)
		{
			const bool bIsExplosiveShell = ShellType == EWeaponShellType::Shell_HE
				|| ShellType == EWeaponShellType::Shell_HEAT;
			switch (EngineState)
			{
			case EVehicleModuleState::Damaged:
				return bIsExplosiveShell ? EngineFireChanceDamagedEngine_HE_HEAT : EngineFireChanceDamagedEngine;
			case EVehicleModuleState::Destroyed:
				return bIsExplosiveShell ? EngineFireChanceDestroyedEngine_HE_HEAT : EngineFireChanceDestroyedEngine;
			default:
				return 0.f;
			}
		}
	}

	/** @brief Chance to ignite ammunition when damage changes its module state, independent of shell type. */
	namespace AmmoCookOff
	{
		inline constexpr float AmmoCookOffDamagedAmmoChance = 0.1f;
		inline constexpr float AmmoCookOffDestroyedAmmoChance = 0.2f;

		/** @return Cook-off probability for the resulting ammo state; healthy ammo cannot ignite here. */
		constexpr float GetChance(const EVehicleModuleState AmmoState)
		{
			switch (AmmoState)
			{
			case EVehicleModuleState::Damaged:
				return AmmoCookOffDamagedAmmoChance;
			case EVehicleModuleState::Destroyed:
				return AmmoCookOffDestroyedAmmoChance;
			default:
				return 0.f;
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Editable probabilities and damage-chance multipliers
	// ------------------------------------------------------------------------------------------------
	/** @brief Tune each plate's module hit chance here; rows follow EArmorPlate order. */
	namespace RuleSetProbabilities
	{
		// Covered add-on armor always receives its reserved candidate roll.
		inline constexpr float CoveredAddOnArmor = 1.00f;
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Plate_Front
		{
			inline constexpr float Engine = 1.00f;
			inline constexpr float Ammo = 0.08f;
		}
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Plate_FrontUpperGlacis
		{
			inline constexpr float Ammo = 0.10f;
			inline constexpr float Engine = 0.15f;
		}
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Plate_FrontLowerGlacis
		{
			inline constexpr float Engine = 0.35f;
			inline constexpr float Ammo = 0.10f;
		}
		
		inline constexpr float SideAmmoHitChance= 0.3;
		inline constexpr float SideTrackHitChance= 0.6;
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Plate_SideLeft
		{
			inline constexpr float Tracks = SideTrackHitChance;
			inline constexpr float Ammo = SideAmmoHitChance;
		}
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Plate_SideRight
		{
			inline constexpr float Tracks = SideTrackHitChance;
			inline constexpr float Ammo = SideAmmoHitChance;
		}
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Plate_SideLowerLeft
		{
			inline constexpr float Tracks = SideTrackHitChance;
			inline constexpr float Ammo = SideAmmoHitChance;
		}
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Plate_SideLowerRight
		{
			inline constexpr float Tracks = SideTrackHitChance;
			inline constexpr float Ammo = SideAmmoHitChance;
		}
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Plate_Rear
		{
			inline constexpr float Engine = 0.8f;
			inline constexpr float Ammo = 0.33f;
		}
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Plate_RearLowerGlacis
		{
			inline constexpr float Engine = 0.8f;
			inline constexpr float Tracks = 0.30f;
		}
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Plate_RearUpperGlacis
		{
			inline constexpr float Engine = 0.8f;
			inline constexpr float Ammo = 0.33f;
		}
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Turret_Front
		{
			inline constexpr float Turret = 0.30f;
			inline constexpr float Weapon = 0.50f;
		}
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Turret_SideLeft
		{
			inline constexpr float Turret = 0.40f;
			inline constexpr float Ammo = 0.25f;
		}
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Turret_SideRight
		{
			inline constexpr float Turret = 0.40f;
			inline constexpr float Ammo = 0.25f;
		}
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Turret_Rear
		{
			inline constexpr float Turret = 0.8f;
			inline constexpr float Ammo = 0.40f;
		}
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Turret_SidesAndRear
		{
			inline constexpr float Turret = 0.40f;
			inline constexpr float Ammo = 0.30f;
		}
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Turret_Cupola
		{
			inline constexpr float Turret = 0.15f;
			inline constexpr float Ammo = 0.10f;
		}
		/** @brief Base module hit chances for this armor plate; each module rolls separately. */
		namespace Turret_Mantlet
		{
			inline constexpr float Weapon = 0.80f;
			inline constexpr float Turret = 0.30f;
		}
	}

	/** @brief Tune the chance scaling for each vehicle class and module type here. */
	namespace ProfileDamageChanceMultipliers
	{
		inline constexpr float NoModule = 0.f;
		/** @brief Scales each module damage roll for this profile, in module type order. */
		namespace ArmoredCar
		{
			inline constexpr float AddOnArmor = 1.00f;
			inline constexpr float Tracks = 1.15f;
			inline constexpr float Engine = 1.20f;
			inline constexpr float Ammo = 1.10f;
			inline constexpr float Turret = 1.10f;
			inline constexpr float Weapon = 1.10f;
			inline constexpr float Wheels = 1.15f;
		}
		/** @brief Scales each module damage roll for this profile, in module type order. */
		namespace LightTank
		{
			inline constexpr float AddOnArmor = 1.00f;
			inline constexpr float Tracks = 1.05f;
			inline constexpr float Engine = 1.10f;
			inline constexpr float Ammo = 1.05f;
			inline constexpr float Turret = 1.05f;
			inline constexpr float Weapon = 1.05f;
			inline constexpr float Wheels = 1.05f;
		}
		/** @brief Scales each module damage roll for this profile, in module type order. */
		namespace MediumTank
		{
			inline constexpr float AddOnArmor = 1.00f;
			inline constexpr float Tracks = 1.00f;
			inline constexpr float Engine = 1.00f;
			inline constexpr float Ammo = 1.00f;
			inline constexpr float Turret = 1.00f;
			inline constexpr float Weapon = 1.00f;
			inline constexpr float Wheels = 1.00f;
		}
		/** @brief Scales each module damage roll for this profile, in module type order. */
		namespace HeavyTank
		{
			inline constexpr float AddOnArmor = 1.00f;
			inline constexpr float Tracks = 0.90f;
			inline constexpr float Engine = 0.75f;
			inline constexpr float Ammo = 0.90f;
			inline constexpr float Turret = 0.90f;
			inline constexpr float Weapon = 0.95f;
			inline constexpr float Wheels = 0.90f;
		}
		/** @brief Scales each module damage roll for this profile, in module type order. */
		namespace SuperHeavyTank
		{
			inline constexpr float AddOnArmor = 1.00f;
			inline constexpr float Tracks = 0.85f;
			inline constexpr float Engine = 0.60f;
			inline constexpr float Ammo = 0.80f;
			inline constexpr float Turret = 0.85f;
			inline constexpr float Weapon = 0.90f;
			inline constexpr float Wheels = 0.85f;
		}
	}

	/** @brief Tune non-penetrating shell hit chances before plate and profile scaling. */
	namespace SourceProbabilities
	{
		inline constexpr float KineticNonPen = 0.25f;
		inline constexpr float ArmorPiercingHighExplosiveNonPen = 0.33f;
		inline constexpr float HighExplosiveNonPen = 0.70f;
		inline constexpr float HighExplosiveAntiTankNonPen = 0.70f;
		inline constexpr float RailgunNonPen = 0.35f;
		inline constexpr float MineRunningGear = 1.0f;
		inline constexpr float Splash = 0.50f;
	}

	// ------------------------------------------------------------------------------------------------
	// Editable profile module health multipliers
	// ------------------------------------------------------------------------------------------------
	/** @brief Tune module MaxHP as a fraction of tank MaxHealth for each vehicle class. */
	namespace ProfileHealthMultipliers
	{
		inline constexpr float NoModule = 0.f;
		/** @brief Module MaxHP / tank MaxHealth for this profile, in module type order. */
		namespace ArmoredCar
		{
			inline constexpr float AddOnArmor = 0.15f;
			inline constexpr float Tracks = 0.18f;
			inline constexpr float Engine = 0.25f;
			inline constexpr float Ammo = 0.22f;
			inline constexpr float Turret = 0.22f;
			inline constexpr float Weapon = 0.20f;
			inline constexpr float Wheels = 0.18f;
		}
		/** @brief Module MaxHP / tank MaxHealth for this profile, in module type order. */
		namespace LightTank
		{
			inline constexpr float AddOnArmor = 0.18f;
			inline constexpr float Tracks = 0.22f;
			inline constexpr float Engine = 0.30f;
			inline constexpr float Ammo = 0.26f;
			inline constexpr float Turret = 0.26f;
			inline constexpr float Weapon = 0.23f;
			inline constexpr float Wheels = 0.22f;
		}
		/** @brief Module MaxHP / tank MaxHealth for this profile, in module type order. */
		namespace MediumTank
		{
			inline constexpr float AddOnArmor = 0.20f;
			inline constexpr float Tracks = 0.25f;
			inline constexpr float Engine = 0.35f;
			inline constexpr float Ammo = 0.30f;
			inline constexpr float Turret = 0.30f;
			inline constexpr float Weapon = 0.25f;
			inline constexpr float Wheels = 0.25f;
		}
		/** @brief Module MaxHP / tank MaxHealth for this profile, in module type order. */
		namespace HeavyTank
		{
			inline constexpr float AddOnArmor = 0.25f;
			inline constexpr float Tracks = 0.30f;
			inline constexpr float Engine = 0.45f;
			inline constexpr float Ammo = 0.36f;
			inline constexpr float Turret = 0.36f;
			inline constexpr float Weapon = 0.30f;
			inline constexpr float Wheels = 0.30f;
		}
		/** @brief Module MaxHP / tank MaxHealth for this profile, in module type order. */
		namespace SuperHeavyTank
		{
			inline constexpr float AddOnArmor = 0.30f;
			inline constexpr float Tracks = 0.35f;
			inline constexpr float Engine = 0.55f;
			inline constexpr float Ammo = 0.42f;
			inline constexpr float Turret = 0.42f;
			inline constexpr float Weapon = 0.35f;
			inline constexpr float Wheels = 0.35f;
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Editable rule-set damage multipliers
	// ------------------------------------------------------------------------------------------------
	/** @brief Tune the damage dealt by each armor plate's module candidates. */
	namespace RuleSetDamageMultipliers
	{
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Plate_Front
		{
			inline constexpr float AddOnArmor = 0.30f;
			inline constexpr float Engine = 0.30f;
			inline constexpr float Ammo = 0.30f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Plate_FrontUpperGlacis
		{
			inline constexpr float AddOnArmor = 0.35f;
			inline constexpr float Ammo = 0.30f;
			inline constexpr float Engine = 0.30f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Plate_FrontLowerGlacis
		{
			inline constexpr float AddOnArmor = 0.25f;
			inline constexpr float Engine = 0.55f;
			inline constexpr float Ammo = 0.30f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Plate_SideLeft
		{
			inline constexpr float AddOnArmor = 0.30f;
			inline constexpr float Tracks = 0.40f;
			inline constexpr float Ammo = 0.50f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Plate_SideRight
		{
			inline constexpr float AddOnArmor = 0.30f;
			inline constexpr float Tracks = 0.40f;
			inline constexpr float Ammo = 0.50f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Plate_SideLowerLeft
		{
			inline constexpr float AddOnArmor = 0.20f;
			inline constexpr float Tracks = 0.70f;
			inline constexpr float Ammo = 0.30f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Plate_SideLowerRight
		{
			inline constexpr float AddOnArmor = 0.20f;
			inline constexpr float Tracks = 0.70f;
			inline constexpr float Ammo = 0.30f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Plate_Rear
		{
			inline constexpr float AddOnArmor = 0.25f;
			inline constexpr float Engine = 0.70f;
			inline constexpr float Ammo = 0.40f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Plate_RearLowerGlacis
		{
			inline constexpr float AddOnArmor = 0.20f;
			inline constexpr float Engine = 0.65f;
			inline constexpr float Tracks = 0.40f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Plate_RearUpperGlacis
		{
			inline constexpr float AddOnArmor = 0.25f;
			inline constexpr float Engine = 0.70f;
			inline constexpr float Ammo = 0.45f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Turret_Front
		{
			inline constexpr float AddOnArmor = 0.25f;
			inline constexpr float Turret = 0.45f;
			inline constexpr float Weapon = 0.40f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Turret_SideLeft
		{
			inline constexpr float AddOnArmor = 0.25f;
			inline constexpr float Turret = 0.50f;
			inline constexpr float Ammo = 0.45f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Turret_SideRight
		{
			inline constexpr float AddOnArmor = 0.25f;
			inline constexpr float Turret = 0.50f;
			inline constexpr float Ammo = 0.45f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Turret_Rear
		{
			inline constexpr float AddOnArmor = 0.25f;
			inline constexpr float Turret = 0.50f;
			inline constexpr float Ammo = 0.55f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Turret_SidesAndRear
		{
			inline constexpr float AddOnArmor = 0.25f;
			inline constexpr float Turret = 0.50f;
			inline constexpr float Ammo = 0.50f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Turret_Cupola
		{
			inline constexpr float AddOnArmor = 0.25f;
			inline constexpr float Turret = 0.25f;
			inline constexpr float Ammo = 0.25f;
		}
		/** @brief Damage fraction passed to each module hit on this armor plate. */
		namespace Turret_Mantlet
		{
			inline constexpr float AddOnArmor = 0.25f;
			inline constexpr float Weapon = 0.65f;
			inline constexpr float Turret = 0.45f;
		}
	}

	/** @brief Tune source energy and explosion damage passed into the module damage formula. */
	namespace SourceDamageMultipliers
	{
		inline constexpr float KineticPenEnergy = 1.00f;
		inline constexpr float KineticNonPenEnergy = 0.15f;
		inline constexpr float ArmorPiercingHighExplosivePenEnergy = 1.15f;
		inline constexpr float ArmorPiercingHighExplosiveNonPenEnergy = 0.15f;
		inline constexpr float HighExplosivePenEnergy = 1.10f;
		inline constexpr float HighExplosiveNonPenEnergy = 0.35f;
		inline constexpr float HighExplosiveAntiTankPenEnergy = 1.00f;
		inline constexpr float HighExplosiveAntiTankNonPenEnergy = 0.35f;
		inline constexpr float RailgunPenEnergy = 1.00f;
		inline constexpr float RailgunNonPenEnergy = 0.15f;
		inline constexpr float MineRunningGear = 0.70f;
		inline constexpr float Splash = 0.25f;
	}

	// Version 1 saves used designer-assigned IDs; their module array was exported in fixed slot order.
	inline constexpr int32 DesignerAssignedModuleIdRuleVersion = 1;
	// Bump when rule semantics change; version 1 remains readable for the module ID migration.
	inline constexpr int32 RuleVersion = 2;

	// ------------------------------------------------------------------------------------------------
	// Capacities
	// ------------------------------------------------------------------------------------------------
	inline constexpr int32 MaxModulesPerPlate = 3;
	inline constexpr int32 AddOnArmorCandidateIndex = 0;
	inline constexpr int32 MaxRunningGearModules = 2;
	inline constexpr int32 MaxEngineModules = 1;
	inline constexpr int32 MaxAmmoModules = 1;
	inline constexpr int32 MaxTurretModules = 4;
	inline constexpr int32 MaxWeaponModules = 1;
	inline constexpr int32 MaxAddOnArmorModules = 8;
	inline constexpr int32 MaxModuleInstances = MaxRunningGearModules + MaxEngineModules
		+ MaxAmmoModules + MaxTurretModules + MaxWeaponModules + MaxAddOnArmorModules;
	inline constexpr int32 ArmorPlateRuleCount = 17;
	// Includes None and Wheels.
	inline constexpr int32 ModuleTypeCount = 8;
	inline constexpr int32 VehicleProfileCount = 5;
	inline constexpr int32 RunningGearTypeCount = 2;
	inline constexpr int32 MaxRegisteredArmorMeshes = 3;
	inline constexpr int32 MaxArmorPlatesPerRegisteredMesh = DeveloperSettings::GameBalance::Weapons::MaxArmorPlatesPerMesh;
	inline constexpr int32 MaxPlateBindings = MaxRegisteredArmorMeshes * MaxArmorPlatesPerRegisteredMesh;

	// Fixed slot layout; Tracks and Wheels share the running-gear range because they are mutually exclusive.
	namespace SlotOffset
	{
		inline constexpr int32 RunningGear = 0;
		inline constexpr int32 Engine = RunningGear + MaxRunningGearModules;
		inline constexpr int32 Ammo = Engine + MaxEngineModules;
		inline constexpr int32 Turret = Ammo + MaxAmmoModules;
		inline constexpr int32 Weapon = Turret + MaxTurretModules;
		inline constexpr int32 AddOnArmor = Weapon + MaxWeaponModules;
		inline constexpr int32 End = AddOnArmor + MaxAddOnArmorModules;
	}

	// Running-gear slots are fixed per hull side.
	inline constexpr int32 LeftRunningGearSlot = SlotOffset::RunningGear;
	inline constexpr int32 RightRunningGearSlot = SlotOffset::RunningGear + 1;

	constexpr int32 GetModuleTypeIndex(const EVehicleModuleTypes Type)
	{
		return static_cast<int32>(Type);
	}

	constexpr bool GetIsInstallableModuleType(const EVehicleModuleTypes Type)
	{
		const int32 TypeIndex = GetModuleTypeIndex(Type);
		return TypeIndex > 0 && TypeIndex < ModuleTypeCount;
	}

	constexpr bool GetIsRunningGearType(const EVehicleModuleTypes Type)
	{
		return Type == EVehicleModuleTypes::Tracks || Type == EVehicleModuleTypes::Wheels;
	}

	constexpr int32 GetFirstSlotForType(const EVehicleModuleTypes Type)
	{
		switch (Type)
		{
		case EVehicleModuleTypes::Tracks:
		case EVehicleModuleTypes::Wheels:
			return SlotOffset::RunningGear;
		case EVehicleModuleTypes::Engine:
			return SlotOffset::Engine;
		case EVehicleModuleTypes::Ammo:
			return SlotOffset::Ammo;
		case EVehicleModuleTypes::Turret:
			return SlotOffset::Turret;
		case EVehicleModuleTypes::Weapon:
			return SlotOffset::Weapon;
		case EVehicleModuleTypes::AddOnArmor:
			return SlotOffset::AddOnArmor;
		default:
			return INDEX_NONE;
		}
	}

	constexpr int32 GetSlotCountForType(const EVehicleModuleTypes Type)
	{
		switch (Type)
		{
		case EVehicleModuleTypes::Tracks:
		case EVehicleModuleTypes::Wheels:
			return MaxRunningGearModules;
		case EVehicleModuleTypes::Engine:
			return MaxEngineModules;
		case EVehicleModuleTypes::Ammo:
			return MaxAmmoModules;
		case EVehicleModuleTypes::Turret:
			return MaxTurretModules;
		case EVehicleModuleTypes::Weapon:
			return MaxWeaponModules;
		case EVehicleModuleTypes::AddOnArmor:
			return MaxAddOnArmorModules;
		default:
			return 0;
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Module health thresholds
	// ------------------------------------------------------------------------------------------------

	// The single tank-health gate for ordinary healing to restore red modules to yellow.
	inline constexpr float TankHealthRequiredForModuleRecovery01 = 0.75f;

	// Percentage points above the module type's destruction threshold.
	inline constexpr float RepairedModuleHealthMargin01 = 0.05f;

	inline constexpr float CompletedHealth01 = 1.0f;
	inline constexpr float HealthCompletionTolerance01 = 0.0001f;

	namespace DestroyedHealth01
	{
		inline constexpr float AddOnArmor = 0.05f;
		inline constexpr float Tracks = 0.20f;
		inline constexpr float Wheels = 0.20f;
		inline constexpr float Engine = 0.15f;
		inline constexpr float Ammo = 0.10f;
		inline constexpr float Turret = 0.25f;
		inline constexpr float Weapon = 0.20f;
	}

	// Indexed by EVehicleModuleTypes.
	inline constexpr float DestroyedHealthThresholdByType01[ModuleTypeCount] = {
		0.f,
		DestroyedHealth01::AddOnArmor,
		DestroyedHealth01::Tracks,
		DestroyedHealth01::Engine,
		DestroyedHealth01::Ammo,
		DestroyedHealth01::Turret,
		DestroyedHealth01::Weapon,
		DestroyedHealth01::Wheels
	};

	constexpr float GetDestroyedHealthThreshold01(const EVehicleModuleTypes Type)
	{
		return GetIsInstallableModuleType(Type) ? DestroyedHealthThresholdByType01[GetModuleTypeIndex(Type)] : 0.f;
	}

	constexpr float GetRecoveredHealth01(const EVehicleModuleTypes Type)
	{
		return GetDestroyedHealthThreshold01(Type) + RepairedModuleHealthMargin01;
	}

	// Exact yellow/red transitions: equality at the destruction threshold is red.
	constexpr EVehicleModuleState GetModuleStateForHealth01(const EVehicleModuleTypes Type, const float Health01)
	{
		if (Health01 <= GetDestroyedHealthThreshold01(Type))
		{
			return EVehicleModuleState::Destroyed;
		}
		if (Health01 < CompletedHealth01)
		{
			return EVehicleModuleState::Damaged;
		}
		return EVehicleModuleState::Healthy;
	}

	// Floor for a second would-be failure in the same impact.
	inline constexpr float SurvivingModuleThresholdMargin01 = 0.01f;

	// ------------------------------------------------------------------------------------------------
	// Crew repair
	// ------------------------------------------------------------------------------------------------
	inline constexpr float CrewRepairTickSeconds = 1.0f;

	namespace CrewRepairSeconds
	{
		inline constexpr float AddOnArmor = 8.0f;
		inline constexpr float Tracks = 12.0f;
		inline constexpr float Wheels = 10.0f;
		inline constexpr float Engine = 20.0f;
		inline constexpr float Ammo = 18.0f;
		inline constexpr float Turret = 15.0f;
		inline constexpr float Weapon = 12.0f;
	}

	// Indexed by EVehicleModuleTypes.
	inline constexpr float CrewRepairSecondsByType[ModuleTypeCount] = {
		0.f,
		CrewRepairSeconds::AddOnArmor,
		CrewRepairSeconds::Tracks,
		CrewRepairSeconds::Engine,
		CrewRepairSeconds::Ammo,
		CrewRepairSeconds::Turret,
		CrewRepairSeconds::Weapon,
		CrewRepairSeconds::Wheels
	};

	constexpr float GetCrewRepairSeconds(const EVehicleModuleTypes Type)
	{
		return GetIsInstallableModuleType(Type) ? CrewRepairSecondsByType[GetModuleTypeIndex(Type)] : 0.f;
	}

	// Order in which the crew picks the next red module; stable module slot order breaks ties.
	inline constexpr EVehicleModuleTypes CrewRepairPriority[] = {
		EVehicleModuleTypes::Engine,
		EVehicleModuleTypes::Tracks,
		EVehicleModuleTypes::Wheels,
		EVehicleModuleTypes::Weapon,
		EVehicleModuleTypes::Turret,
		EVehicleModuleTypes::Ammo,
		EVehicleModuleTypes::AddOnArmor
	};

	// Removed from the effective command card while the crew repairs.
	inline constexpr EAbilityID CrewRepairSuppressedAbilities[] = {
		EAbilityID::IdAttack,
		EAbilityID::IdMove,
		EAbilityID::IdReverseMove,
		EAbilityID::IdRotateTowards
	};

	// The final command-card slot, guaranteed unused by tank loadouts.
	inline constexpr int32 CrewRepairAbilitySlotIndex =
		DeveloperSettings::GamePlay::ActionUI::MaxAbilitiesForActionUI - 1;

	// Both EnableRepair and DisableRepair are free and have no cooldown.
	inline constexpr int32 CrewRepairCooldownSeconds = 0;

	// ------------------------------------------------------------------------------------------------
	// Ordinary healing
	// ------------------------------------------------------------------------------------------------

	// Healing-work units (1 unit = 1 accepted HP) that complete yellow -> Healthy at full tank health.
	inline constexpr float FullModuleServiceWork = 60.f;

	inline constexpr float VehicleRepairTickSeconds = 0.5f;
	inline constexpr float BaseWorkerRepairHpPerSecond = 7.5f;
	inline constexpr float BaseWorkerRepairHpPerTick = BaseWorkerRepairHpPerSecond * VehicleRepairTickSeconds;

	// ------------------------------------------------------------------------------------------------
	// Class profiles: module MaxHP / tank MaxHealth and damage-chance multipliers
	// ------------------------------------------------------------------------------------------------

	/** @brief Per-class tuning; arrays are indexed by EVehicleModuleTypes. */
	struct FVehicleModuleProfileRule
	{
		float ModuleHealthMultiplier[ModuleTypeCount];
		float DamageChanceMultiplier[ModuleTypeCount];
		EVehicleRunningGear DefaultRunningGear;
	};

	// Order per row: None, AddOnArmor, Tracks, Engine, Ammo, Turret, Weapon, Wheels.
	inline constexpr FVehicleModuleProfileRule ModuleProfileRules[VehicleProfileCount] = {
		// ArmoredCar
		{
			{ProfileHealthMultipliers::NoModule,
				ProfileHealthMultipliers::ArmoredCar::AddOnArmor,
				ProfileHealthMultipliers::ArmoredCar::Tracks,
				ProfileHealthMultipliers::ArmoredCar::Engine,
				ProfileHealthMultipliers::ArmoredCar::Ammo,
				ProfileHealthMultipliers::ArmoredCar::Turret,
				ProfileHealthMultipliers::ArmoredCar::Weapon,
				ProfileHealthMultipliers::ArmoredCar::Wheels},
			{ProfileDamageChanceMultipliers::NoModule,
				ProfileDamageChanceMultipliers::ArmoredCar::AddOnArmor,
				ProfileDamageChanceMultipliers::ArmoredCar::Tracks,
				ProfileDamageChanceMultipliers::ArmoredCar::Engine,
				ProfileDamageChanceMultipliers::ArmoredCar::Ammo,
				ProfileDamageChanceMultipliers::ArmoredCar::Turret,
				ProfileDamageChanceMultipliers::ArmoredCar::Weapon,
				ProfileDamageChanceMultipliers::ArmoredCar::Wheels},
			EVehicleRunningGear::Wheels
		},
		// LightTank
		{
			{ProfileHealthMultipliers::NoModule,
				ProfileHealthMultipliers::LightTank::AddOnArmor,
				ProfileHealthMultipliers::LightTank::Tracks,
				ProfileHealthMultipliers::LightTank::Engine,
				ProfileHealthMultipliers::LightTank::Ammo,
				ProfileHealthMultipliers::LightTank::Turret,
				ProfileHealthMultipliers::LightTank::Weapon,
				ProfileHealthMultipliers::LightTank::Wheels},
			{ProfileDamageChanceMultipliers::NoModule,
				ProfileDamageChanceMultipliers::LightTank::AddOnArmor,
				ProfileDamageChanceMultipliers::LightTank::Tracks,
				ProfileDamageChanceMultipliers::LightTank::Engine,
				ProfileDamageChanceMultipliers::LightTank::Ammo,
				ProfileDamageChanceMultipliers::LightTank::Turret,
				ProfileDamageChanceMultipliers::LightTank::Weapon,
				ProfileDamageChanceMultipliers::LightTank::Wheels},
			EVehicleRunningGear::Tracks
		},
		// MediumTank
		{
			{ProfileHealthMultipliers::NoModule,
				ProfileHealthMultipliers::MediumTank::AddOnArmor,
				ProfileHealthMultipliers::MediumTank::Tracks,
				ProfileHealthMultipliers::MediumTank::Engine,
				ProfileHealthMultipliers::MediumTank::Ammo,
				ProfileHealthMultipliers::MediumTank::Turret,
				ProfileHealthMultipliers::MediumTank::Weapon,
				ProfileHealthMultipliers::MediumTank::Wheels},
			{ProfileDamageChanceMultipliers::NoModule,
				ProfileDamageChanceMultipliers::MediumTank::AddOnArmor,
				ProfileDamageChanceMultipliers::MediumTank::Tracks,
				ProfileDamageChanceMultipliers::MediumTank::Engine,
				ProfileDamageChanceMultipliers::MediumTank::Ammo,
				ProfileDamageChanceMultipliers::MediumTank::Turret,
				ProfileDamageChanceMultipliers::MediumTank::Weapon,
				ProfileDamageChanceMultipliers::MediumTank::Wheels},
			EVehicleRunningGear::Tracks
		},
		// HeavyTank
		{
			{ProfileHealthMultipliers::NoModule,
				ProfileHealthMultipliers::HeavyTank::AddOnArmor,
				ProfileHealthMultipliers::HeavyTank::Tracks,
				ProfileHealthMultipliers::HeavyTank::Engine,
				ProfileHealthMultipliers::HeavyTank::Ammo,
				ProfileHealthMultipliers::HeavyTank::Turret,
				ProfileHealthMultipliers::HeavyTank::Weapon,
				ProfileHealthMultipliers::HeavyTank::Wheels},
			{ProfileDamageChanceMultipliers::NoModule,
				ProfileDamageChanceMultipliers::HeavyTank::AddOnArmor,
				ProfileDamageChanceMultipliers::HeavyTank::Tracks,
				ProfileDamageChanceMultipliers::HeavyTank::Engine,
				ProfileDamageChanceMultipliers::HeavyTank::Ammo,
				ProfileDamageChanceMultipliers::HeavyTank::Turret,
				ProfileDamageChanceMultipliers::HeavyTank::Weapon,
				ProfileDamageChanceMultipliers::HeavyTank::Wheels},
			EVehicleRunningGear::Tracks
		},
		// SuperHeavyTank
		{
			{ProfileHealthMultipliers::NoModule,
				ProfileHealthMultipliers::SuperHeavyTank::AddOnArmor,
				ProfileHealthMultipliers::SuperHeavyTank::Tracks,
				ProfileHealthMultipliers::SuperHeavyTank::Engine,
				ProfileHealthMultipliers::SuperHeavyTank::Ammo,
				ProfileHealthMultipliers::SuperHeavyTank::Turret,
				ProfileHealthMultipliers::SuperHeavyTank::Weapon,
				ProfileHealthMultipliers::SuperHeavyTank::Wheels},
			{ProfileDamageChanceMultipliers::NoModule,
				ProfileDamageChanceMultipliers::SuperHeavyTank::AddOnArmor,
				ProfileDamageChanceMultipliers::SuperHeavyTank::Tracks,
				ProfileDamageChanceMultipliers::SuperHeavyTank::Engine,
				ProfileDamageChanceMultipliers::SuperHeavyTank::Ammo,
				ProfileDamageChanceMultipliers::SuperHeavyTank::Turret,
				ProfileDamageChanceMultipliers::SuperHeavyTank::Weapon,
				ProfileDamageChanceMultipliers::SuperHeavyTank::Wheels},
			EVehicleRunningGear::Tracks
		}
	};

	inline constexpr EVehicleModuleProfile DefaultModuleProfile = EVehicleModuleProfile::MediumTank;

	constexpr int32 GetProfileIndex(const EVehicleModuleProfile Profile)
	{
		return static_cast<int32>(Profile);
	}

	constexpr int32 GetRunningGearIndex(const EVehicleRunningGear RunningGear)
	{
		return static_cast<int32>(RunningGear);
	}

	constexpr float GetModuleHealthMultiplier(const EVehicleModuleProfile Profile, const EVehicleModuleTypes Type)
	{
		return ModuleProfileRules[GetProfileIndex(Profile)].ModuleHealthMultiplier[GetModuleTypeIndex(Type)];
	}

	constexpr float GetDamageChanceMultiplier(const EVehicleModuleProfile Profile, const EVehicleModuleTypes Type)
	{
		return ModuleProfileRules[GetProfileIndex(Profile)].DamageChanceMultiplier[GetModuleTypeIndex(Type)];
	}

	// ------------------------------------------------------------------------------------------------
	// Compile-time plate -> module topology
	// ------------------------------------------------------------------------------------------------

	/** @brief One armor-plate row; unused entries have TypeToDamage = None. */
	struct FPlateModuleRuleSet
	{
		FPlateModuleDamage Entries[MaxModulesPerPlate];
	};

	constexpr EModuleTargetSelector GetTargetSelectorForType(const EVehicleModuleTypes Type)
	{
		switch (Type)
		{
		case EVehicleModuleTypes::AddOnArmor:
			return EModuleTargetSelector::CoveringArmorZone;
		case EVehicleModuleTypes::Tracks:
		case EVehicleModuleTypes::Wheels:
			return EModuleTargetSelector::RunningGearSide;
		case EVehicleModuleTypes::Engine:
		case EVehicleModuleTypes::Ammo:
			return EModuleTargetSelector::Singleton;
		case EVehicleModuleTypes::Turret:
			return EModuleTargetSelector::BoundTurret;
		case EVehicleModuleTypes::Weapon:
			return EModuleTargetSelector::BoundWeapon;
		default:
			return EModuleTargetSelector::None;
		}
	}

	// Non-penetration can damage running gear, add-on armor and the mantlet-bound weapon only.
	constexpr EModuleNonPenPolicy GetNonPenPolicyForType(const EVehicleModuleTypes Type)
	{
		switch (Type)
		{
		case EVehicleModuleTypes::AddOnArmor:
		case EVehicleModuleTypes::Tracks:
		case EVehicleModuleTypes::Wheels:
			return EModuleNonPenPolicy::External;
		case EVehicleModuleTypes::Weapon:
			return EModuleNonPenPolicy::MantletOnly;
		default:
			return EModuleNonPenPolicy::Never;
		}
	}

	constexpr FPlateModuleDamage MakeRule(
		const EVehicleModuleTypes Type,
		const float Probability,
		const float DamageMultiplier,
		const EModuleTargetSelector Selector,
		const EModuleNonPenPolicy NonPenPolicy)
	{
		FPlateModuleDamage Rule;
		Rule.TypeToDamage = Type;
		Rule.DamageProbability = Probability;
		Rule.DamageMultiplier = DamageMultiplier;
		Rule.TargetSelector = Selector;
		Rule.NonPenPolicy = NonPenPolicy;
		return Rule;
	}

	// Selector and non-pen policy follow the module type.
	constexpr FPlateModuleDamage MakeTypedRule(
		const EVehicleModuleTypes Type,
		const float Probability,
		const float DamageMultiplier)
	{
		return MakeRule(Type, Probability, DamageMultiplier, GetTargetSelectorForType(Type),
		                GetNonPenPolicyForType(Type));
	}

	inline constexpr FPlateModuleDamage NoModuleRule = FPlateModuleDamage();

	constexpr FPlateModuleRuleSet MakeRuleSet(
		const FPlateModuleDamage First,
		const FPlateModuleDamage Second = NoModuleRule,
		const FPlateModuleDamage Third = NoModuleRule)
	{
		FPlateModuleRuleSet RuleSet;
		RuleSet.Entries[0] = First;
		RuleSet.Entries[1] = Second;
		RuleSet.Entries[2] = Third;
		return RuleSet;
	}

	// The reserved candidate for optional per-vehicle add-on coverage.
	constexpr FPlateModuleDamage MakeAddOnRule(const float DamageMultiplier)
	{
		return MakeTypedRule(EVehicleModuleTypes::AddOnArmor,
		                     RuleSetProbabilities::CoveredAddOnArmor, DamageMultiplier);
	}

	using EModule = EVehicleModuleTypes;

	// Rows follow EArmorPlate order; candidate values are declared in the tuning sections above.
	inline constexpr FPlateModuleRuleSet BasePlateRules[ArmorPlateRuleCount] = {
		// Plate_Front
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Plate_Front::AddOnArmor),
			MakeTypedRule(EModule::Engine,
			              RuleSetProbabilities::Plate_Front::Engine,
			              RuleSetDamageMultipliers::Plate_Front::Engine),
			MakeTypedRule(EModule::Ammo,
			              RuleSetProbabilities::Plate_Front::Ammo,
			              RuleSetDamageMultipliers::Plate_Front::Ammo)
		),
		// Plate_FrontUpperGlacis
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Plate_FrontUpperGlacis::AddOnArmor),
			MakeTypedRule(EModule::Ammo,
			              RuleSetProbabilities::Plate_FrontUpperGlacis::Ammo,
			              RuleSetDamageMultipliers::Plate_FrontUpperGlacis::Ammo),
			MakeTypedRule(EModule::Engine,
			              RuleSetProbabilities::Plate_FrontUpperGlacis::Engine,
			              RuleSetDamageMultipliers::Plate_FrontUpperGlacis::Engine)
		),
		// Plate_FrontLowerGlacis
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Plate_FrontLowerGlacis::AddOnArmor),
			MakeTypedRule(EModule::Engine,
			              RuleSetProbabilities::Plate_FrontLowerGlacis::Engine,
			              RuleSetDamageMultipliers::Plate_FrontLowerGlacis::Engine),
			MakeTypedRule(EModule::Ammo,
			              RuleSetProbabilities::Plate_FrontLowerGlacis::Ammo,
			              RuleSetDamageMultipliers::Plate_FrontLowerGlacis::Ammo)
		),
		// Plate_SideLeft
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Plate_SideLeft::AddOnArmor),
			MakeTypedRule(EModule::Tracks,
			              RuleSetProbabilities::Plate_SideLeft::Tracks,
			              RuleSetDamageMultipliers::Plate_SideLeft::Tracks),
			MakeTypedRule(EModule::Ammo,
			              RuleSetProbabilities::Plate_SideLeft::Ammo,
			              RuleSetDamageMultipliers::Plate_SideLeft::Ammo)
		),
		// Plate_SideRight
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Plate_SideRight::AddOnArmor),
			MakeTypedRule(EModule::Tracks,
			              RuleSetProbabilities::Plate_SideRight::Tracks,
			              RuleSetDamageMultipliers::Plate_SideRight::Tracks),
			MakeTypedRule(EModule::Ammo,
			              RuleSetProbabilities::Plate_SideRight::Ammo,
			              RuleSetDamageMultipliers::Plate_SideRight::Ammo)
		),
		// Plate_SideLowerLeft
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Plate_SideLowerLeft::AddOnArmor),
			MakeTypedRule(EModule::Tracks,
			              RuleSetProbabilities::Plate_SideLowerLeft::Tracks,
			              RuleSetDamageMultipliers::Plate_SideLowerLeft::Tracks),
			MakeTypedRule(EModule::Ammo,
			              RuleSetProbabilities::Plate_SideLowerLeft::Ammo,
			              RuleSetDamageMultipliers::Plate_SideLowerLeft::Ammo)
		),
		// Plate_SideLowerRight
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Plate_SideLowerRight::AddOnArmor),
			MakeTypedRule(EModule::Tracks,
			              RuleSetProbabilities::Plate_SideLowerRight::Tracks,
			              RuleSetDamageMultipliers::Plate_SideLowerRight::Tracks),
			MakeTypedRule(EModule::Ammo,
			              RuleSetProbabilities::Plate_SideLowerRight::Ammo,
			              RuleSetDamageMultipliers::Plate_SideLowerRight::Ammo)
		),
		// Plate_Rear
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Plate_Rear::AddOnArmor),
			MakeTypedRule(EModule::Engine,
			              RuleSetProbabilities::Plate_Rear::Engine,
			              RuleSetDamageMultipliers::Plate_Rear::Engine),
			MakeTypedRule(EModule::Ammo,
			              RuleSetProbabilities::Plate_Rear::Ammo,
			              RuleSetDamageMultipliers::Plate_Rear::Ammo)
		),
		// Plate_RearLowerGlacis
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Plate_RearLowerGlacis::AddOnArmor),
			MakeTypedRule(EModule::Engine,
			              RuleSetProbabilities::Plate_RearLowerGlacis::Engine,
			              RuleSetDamageMultipliers::Plate_RearLowerGlacis::Engine),
			MakeTypedRule(EModule::Tracks,
			              RuleSetProbabilities::Plate_RearLowerGlacis::Tracks,
			              RuleSetDamageMultipliers::Plate_RearLowerGlacis::Tracks)
		),
		// Plate_RearUpperGlacis
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Plate_RearUpperGlacis::AddOnArmor),
			MakeTypedRule(EModule::Engine,
			              RuleSetProbabilities::Plate_RearUpperGlacis::Engine,
			              RuleSetDamageMultipliers::Plate_RearUpperGlacis::Engine),
			MakeTypedRule(EModule::Ammo,
			              RuleSetProbabilities::Plate_RearUpperGlacis::Ammo,
			              RuleSetDamageMultipliers::Plate_RearUpperGlacis::Ammo)
		),
		// Turret_Front
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Turret_Front::AddOnArmor),
			MakeTypedRule(EModule::Turret,
			              RuleSetProbabilities::Turret_Front::Turret,
			              RuleSetDamageMultipliers::Turret_Front::Turret),
			MakeTypedRule(EModule::Weapon,
			              RuleSetProbabilities::Turret_Front::Weapon,
			              RuleSetDamageMultipliers::Turret_Front::Weapon)
		),
		// Turret_SideLeft
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Turret_SideLeft::AddOnArmor),
			MakeTypedRule(EModule::Turret,
			              RuleSetProbabilities::Turret_SideLeft::Turret,
			              RuleSetDamageMultipliers::Turret_SideLeft::Turret),
			MakeTypedRule(EModule::Ammo,
			              RuleSetProbabilities::Turret_SideLeft::Ammo,
			              RuleSetDamageMultipliers::Turret_SideLeft::Ammo)
		),
		// Turret_SideRight
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Turret_SideRight::AddOnArmor),
			MakeTypedRule(EModule::Turret,
			              RuleSetProbabilities::Turret_SideRight::Turret,
			              RuleSetDamageMultipliers::Turret_SideRight::Turret),
			MakeTypedRule(EModule::Ammo,
			              RuleSetProbabilities::Turret_SideRight::Ammo,
			              RuleSetDamageMultipliers::Turret_SideRight::Ammo)
		),
		// Turret_Rear
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Turret_Rear::AddOnArmor),
			MakeTypedRule(EModule::Turret,
			              RuleSetProbabilities::Turret_Rear::Turret,
			              RuleSetDamageMultipliers::Turret_Rear::Turret),
			MakeTypedRule(EModule::Ammo,
			              RuleSetProbabilities::Turret_Rear::Ammo,
			              RuleSetDamageMultipliers::Turret_Rear::Ammo)
		),
		// Turret_SidesAndRear
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Turret_SidesAndRear::AddOnArmor),
			MakeTypedRule(EModule::Turret,
			              RuleSetProbabilities::Turret_SidesAndRear::Turret,
			              RuleSetDamageMultipliers::Turret_SidesAndRear::Turret),
			MakeTypedRule(EModule::Ammo,
			              RuleSetProbabilities::Turret_SidesAndRear::Ammo,
			              RuleSetDamageMultipliers::Turret_SidesAndRear::Ammo)
		),
		// Turret_Cupola
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Turret_Cupola::AddOnArmor),
			MakeTypedRule(EModule::Turret,
			              RuleSetProbabilities::Turret_Cupola::Turret,
			              RuleSetDamageMultipliers::Turret_Cupola::Turret),
			MakeTypedRule(EModule::Ammo,
			              RuleSetProbabilities::Turret_Cupola::Ammo,
			              RuleSetDamageMultipliers::Turret_Cupola::Ammo)
		),
		// Turret_Mantlet
		MakeRuleSet(
			MakeAddOnRule(RuleSetDamageMultipliers::Turret_Mantlet::AddOnArmor),
			MakeTypedRule(EModule::Weapon,
			              RuleSetProbabilities::Turret_Mantlet::Weapon,
			              RuleSetDamageMultipliers::Turret_Mantlet::Weapon),
			MakeTypedRule(EModule::Turret,
			              RuleSetProbabilities::Turret_Mantlet::Turret,
			              RuleSetDamageMultipliers::Turret_Mantlet::Turret)
		)
	};

	/** @return The plate row index, or INDEX_NONE for a value outside EArmorPlate. */
	constexpr int32 TryGetPlateRuleIndex(const EArmorPlate Plate)
	{
		const int32 PlateIndex = static_cast<int32>(Plate);
		return PlateIndex >= 0 && PlateIndex < ArmorPlateRuleCount ? PlateIndex : INDEX_NONE;
	}

	constexpr float ClampRuleValue01(const float Value)
	{
		return Value < 0.f ? 0.f : (Value > 1.f ? 1.f : Value);
	}

	/** @brief Every profile/gear combination; selected once per tank and referenced by pointer. */
	struct FProfilePlateRuleTable
	{
		FPlateModuleRuleSet Rows[VehicleProfileCount][RunningGearTypeCount][ArmorPlateRuleCount];
	};

	// Substitutes the selected running gear, then applies the resolved type's class chance multiplier once.
	constexpr FPlateModuleDamage MakeProfileRule(
		const FPlateModuleDamage& BaseRule,
		const EVehicleModuleProfile Profile,
		const EVehicleRunningGear RunningGear)
	{
		FPlateModuleDamage Rule = BaseRule;
		if (Rule.TypeToDamage == EVehicleModuleTypes::None)
		{
			return Rule;
		}
		if (Rule.TypeToDamage == EVehicleModuleTypes::Tracks && RunningGear == EVehicleRunningGear::Wheels)
		{
			Rule.TypeToDamage = EVehicleModuleTypes::Wheels;
		}
		Rule.DamageProbability = ClampRuleValue01(
			BaseRule.DamageProbability * GetDamageChanceMultiplier(Profile, Rule.TypeToDamage));
		return Rule;
	}

	constexpr FProfilePlateRuleTable BuildProfilePlateRules()
	{
		FProfilePlateRuleTable Table{};
		for (int32 ProfileIndex = 0; ProfileIndex < VehicleProfileCount; ++ProfileIndex)
		{
			for (int32 GearIndex = 0; GearIndex < RunningGearTypeCount; ++GearIndex)
			{
				for (int32 RowIndex = 0; RowIndex < ArmorPlateRuleCount; ++RowIndex)
				{
					for (int32 EntryIndex = 0; EntryIndex < MaxModulesPerPlate; ++EntryIndex)
					{
						Table.Rows[ProfileIndex][GearIndex][RowIndex].Entries[EntryIndex] = MakeProfileRule(
							BasePlateRules[RowIndex].Entries[EntryIndex],
							static_cast<EVehicleModuleProfile>(ProfileIndex),
							static_cast<EVehicleRunningGear>(GearIndex));
					}
				}
			}
		}
		return Table;
	}

	inline constexpr FProfilePlateRuleTable ProfilePlateRules = BuildProfilePlateRules();

	/** @return The first row of the immutable table selected for this profile and gear. */
	constexpr const FPlateModuleRuleSet* GetProfilePlateRules(
		const EVehicleModuleProfile Profile,
		const EVehicleRunningGear RunningGear)
	{
		return ProfilePlateRules.Rows[GetProfileIndex(Profile)][GetRunningGearIndex(RunningGear)];
	}

	// ------------------------------------------------------------------------------------------------
	// Damage formula and source rules
	// ------------------------------------------------------------------------------------------------

	// Fractions of module MaxHP per impact.
	inline constexpr float PenetratingModuleDamageCap01 = 0.60f;
	inline constexpr float NonPenModuleDamageCap01 = 0.20f;

	// Limits state transitions per impact regardless of assigned behaviours; AddOnArmor is excluded.
	inline constexpr int32 MaxNewDestroyedModulesPerImpact = 1;

	inline constexpr float PenDamageFloorFromBase = 0.20f;
	inline constexpr float PenDamageCeilingFromBase = 1.25f;

	// Shared non-penetrating damage budget per module across all attackers.
	inline constexpr float NonPenWindowSeconds = 1.0f;
	inline constexpr float NonPenWindowDamageCap01 = 0.20f;
	inline constexpr int32 NonPenWindowBucketCount = 20;
	// Buckets are one step wider than Window / Count so the retained ring always spans the full window;
	// the oldest bucket is dropped only after it has completely left the window (never under-counts).
	inline constexpr float NonPenBucketSeconds = NonPenWindowSeconds / (NonPenWindowBucketCount - 1);

	/** @brief Source energy and probability factors per shell type. */
	struct FShellModuleRule
	{
		bool bIsSupported = false;
		float PenEnergyMultiplier = 0.f;
		float NonPenProbabilityMultiplier = 0.f;
		float NonPenEnergyMultiplier = 0.f;
	};

	constexpr FShellModuleRule MakeShellRule(
		const float PenEnergyMultiplier,
		const float NonPenProbabilityMultiplier,
		const float NonPenEnergyMultiplier)
	{
		FShellModuleRule Rule;
		Rule.bIsSupported = true;
		Rule.PenEnergyMultiplier = PenEnergyMultiplier;
		Rule.NonPenProbabilityMultiplier = NonPenProbabilityMultiplier;
		Rule.NonPenEnergyMultiplier = NonPenEnergyMultiplier;
		return Rule;
	}

	namespace ShellModuleRules
	{
		inline constexpr FShellModuleRule Kinetic = MakeShellRule(
			SourceDamageMultipliers::KineticPenEnergy,
			SourceProbabilities::KineticNonPen,
			SourceDamageMultipliers::KineticNonPenEnergy);
		inline constexpr FShellModuleRule ArmorPiercingHighExplosive = MakeShellRule(
			SourceDamageMultipliers::ArmorPiercingHighExplosivePenEnergy,
			SourceProbabilities::ArmorPiercingHighExplosiveNonPen,
			SourceDamageMultipliers::ArmorPiercingHighExplosiveNonPenEnergy);
		inline constexpr FShellModuleRule HighExplosive = MakeShellRule(
			SourceDamageMultipliers::HighExplosivePenEnergy,
			SourceProbabilities::HighExplosiveNonPen,
			SourceDamageMultipliers::HighExplosiveNonPenEnergy);
		inline constexpr FShellModuleRule HighExplosiveAntiTank = MakeShellRule(
			SourceDamageMultipliers::HighExplosiveAntiTankPenEnergy,
			SourceProbabilities::HighExplosiveAntiTankNonPen,
			SourceDamageMultipliers::HighExplosiveAntiTankNonPenEnergy);
		inline constexpr FShellModuleRule Railgun = MakeShellRule(
			SourceDamageMultipliers::RailgunPenEnergy,
			SourceProbabilities::RailgunNonPen,
			SourceDamageMultipliers::RailgunNonPenEnergy);
	}

	// Global penetrating-energy factor for a railgun round that overpenetrates the victim.
	inline constexpr float OverpenetrationEnergyMultiplier = 0.70f;

	/** @return The shell's rule; unsupported sources (fire, radixite, none) fail closed. */
	constexpr FShellModuleRule GetShellModuleRule(const EWeaponShellType ShellType)
	{
		switch (ShellType)
		{
		case EWeaponShellType::Shell_AP:
		case EWeaponShellType::Shell_APCR:
			return ShellModuleRules::Kinetic;
		case EWeaponShellType::Shell_APHE:
		case EWeaponShellType::Shell_APHEBC:
			return ShellModuleRules::ArmorPiercingHighExplosive;
		case EWeaponShellType::Shell_HE:
			return ShellModuleRules::HighExplosive;
		case EWeaponShellType::Shell_HEAT:
			return ShellModuleRules::HighExplosiveAntiTank;
		case EWeaponShellType::Shell_Railgun:
			return ShellModuleRules::Railgun;
		default:
			return FShellModuleRule();
		}
	}

	// Independent ballistic calibre gates per module type, indexed by EVehicleModuleTypes; None never rolls.
	inline constexpr float MinBallisticCalibreMmByModuleType[ModuleTypeCount] = {
		0.f,
		// AddOnArmor
		20.f,
		// Tracks
		20.f,
		// Engine
		20.f,
		// Ammo
		20.f,
		// Turret
		20.f,
		// Weapon
		20.f,
		// Wheels
		20.f
	};

	constexpr float GetMinBallisticCalibreMm(const EVehicleModuleTypes Type)
	{
		return MinBallisticCalibreMmByModuleType[GetModuleTypeIndex(Type)];
	}

	/** @brief Direct mine hits roll only the running gear on the side nearest the explosion. */
	struct FMineModuleRuleData
	{
		float DamageProbability;
		float DamageMultiplier;
	};

	// Base probability 1 is scaled once by the class chance factor of the resolved running gear.
	inline constexpr FMineModuleRuleData MineModuleRule = {SourceProbabilities::MineRunningGear, SourceDamageMultipliers::MineRunningGear};

	/** @brief Splash reaches only external modules and is attenuated by the AOE falloff already applied. */
	struct FSplashModuleRuleData
	{
		float ProbabilityMultiplier;
		float EnergyMultiplier;
		// Probe length used to find the hull plate facing the explosion.
		float PlateProbeDistanceCm;
	};

	inline constexpr FSplashModuleRuleData SplashModuleRules = {SourceProbabilities::Splash, SourceDamageMultipliers::Splash, 5000.f};

	// ------------------------------------------------------------------------------------------------
	// Default module behaviours (optional classes in the shared module data asset read these values)
	// ------------------------------------------------------------------------------------------------
	namespace DefaultBehaviours
	{
		inline constexpr float RunningGearYellowTravelMultiplier = 0.65f;
		inline constexpr float RunningGearYellowTurnMultiplier = 0.70f;
		inline constexpr float EngineYellowTravelMultiplier = 0.70f;
		inline constexpr float EngineYellowAccelerationMultiplier = 0.60f;
		// Red running gear or engine: no powered travel or turning.
		inline constexpr float DestroyedEngineMobilityMlt = 0.0f;
		inline constexpr float DestroyedTrackOrWheelMobilityMlt = 0.0f;
		inline constexpr float AmmoYellowReloadDurationMultiplier = 1.50f;
		inline constexpr float AmmoAffectedWeaponCalibreExclusiveMm = 19.f;
		inline constexpr float TurretYellowTraverseMultiplier = 0.50f;
		inline constexpr float TurretDestroyedTraverseMultiplier = 0.0f;
		inline constexpr float WeaponYellowDispersionMultiplier = 1.25f;
		inline constexpr float WeaponYellowFiringCycleMultiplier = 1.20f;
		inline constexpr float AddOnArmorYellowContributionMultiplier = 0.50f;
		inline constexpr float AddOnArmorDestroyedContributionMultiplier = 0.0f;
	}

	// ------------------------------------------------------------------------------------------------
	// Crew shock, announcements and UI
	// ------------------------------------------------------------------------------------------------
	inline constexpr float CrewShockMaxSeconds = 4.f;
	inline constexpr float CrewShockImmunitySeconds = 8.f;

	// Announcements only; never delays icon changes.
	inline constexpr float ModuleAnnouncementCooldownSeconds = 5.f;

	// Defaults for the presentation asset's editable dimensions (UMG units before healthbar render scale).
	inline constexpr float DefaultModuleIconWidth = 225.f;
	inline constexpr float DefaultModuleIconHeight = 225.f;

	// ------------------------------------------------------------------------------------------------
	// Compile-time validation
	// ------------------------------------------------------------------------------------------------
	constexpr bool GetIsFiniteNonNegative(const float Value)
	{
		// NaN fails both comparisons; infinity fails the upper bound.
		return Value >= 0.f && Value < 3.0e38f;
	}

	constexpr float GetMaxDestroyedThreshold01()
	{
		float MaxThreshold = 0.f;
		for (const float Threshold : DestroyedHealthThresholdByType01)
		{
			MaxThreshold = Threshold > MaxThreshold ? Threshold : MaxThreshold;
		}
		return MaxThreshold;
	}

	constexpr bool ValidateTypeTables()
	{
		for (int32 TypeIndex = 1; TypeIndex < ModuleTypeCount; ++TypeIndex)
		{
			const EVehicleModuleTypes Type = static_cast<EVehicleModuleTypes>(TypeIndex);
			const float Threshold = GetDestroyedHealthThreshold01(Type);
			const float Recovered = GetRecoveredHealth01(Type);
			if (Threshold < 0.f || Threshold >= 1.f || Recovered <= Threshold || Recovered >= 1.f)
			{
				return false;
			}
			const float CrewSeconds = GetCrewRepairSeconds(Type);
			const int32 WholeTicks = static_cast<int32>(CrewSeconds / CrewRepairTickSeconds);
			if (CrewSeconds <= 0.f || static_cast<float>(WholeTicks) * CrewRepairTickSeconds != CrewSeconds)
			{
				return false;
			}
			if (not GetIsFiniteNonNegative(GetMinBallisticCalibreMm(Type)))
			{
				return false;
			}
			if (GetFirstSlotForType(Type) == INDEX_NONE || GetSlotCountForType(Type) <= 0)
			{
				return false;
			}
		}
		return true;
	}

	constexpr bool ValidateProfileRules()
	{
		for (const FVehicleModuleProfileRule& Profile : ModuleProfileRules)
		{
			if (Profile.ModuleHealthMultiplier[0] != 0.f || Profile.DamageChanceMultiplier[0] != 0.f)
			{
				return false;
			}
			for (int32 TypeIndex = 1; TypeIndex < ModuleTypeCount; ++TypeIndex)
			{
				if (not GetIsFiniteNonNegative(Profile.ModuleHealthMultiplier[TypeIndex])
					|| Profile.ModuleHealthMultiplier[TypeIndex] <= 0.f)
				{
					return false;
				}
				if (not GetIsFiniteNonNegative(Profile.DamageChanceMultiplier[TypeIndex])
					|| Profile.DamageChanceMultiplier[TypeIndex] <= 0.f)
				{
					return false;
				}
			}
		}
		return true;
	}

	constexpr bool ValidatePlateRuleEntry(const FPlateModuleDamage& Rule, const int32 EntryIndex)
	{
		const bool bIsAddOnEntry = EntryIndex == AddOnArmorCandidateIndex;
		if (bIsAddOnEntry != (Rule.TypeToDamage == EVehicleModuleTypes::AddOnArmor))
		{
			return false;
		}
		if (Rule.TypeToDamage == EVehicleModuleTypes::None)
		{
			return Rule.DamageProbability == 0.f && Rule.DamageMultiplier == 0.f;
		}
		return Rule.DamageProbability >= 0.f && Rule.DamageProbability <= 1.f
			&& GetIsFiniteNonNegative(Rule.DamageMultiplier) && Rule.DamageMultiplier <= 1.f
			&& Rule.TargetSelector == GetTargetSelectorForType(Rule.TypeToDamage);
	}

	constexpr bool ValidatePlateRuleRows(const FPlateModuleRuleSet* Rows, const EVehicleRunningGear RunningGear)
	{
		const EVehicleModuleTypes ForbiddenGear = RunningGear == EVehicleRunningGear::Tracks
			                                          ? EVehicleModuleTypes::Wheels
			                                          : EVehicleModuleTypes::Tracks;
		for (int32 RowIndex = 0; RowIndex < ArmorPlateRuleCount; ++RowIndex)
		{
			for (int32 EntryIndex = 0; EntryIndex < MaxModulesPerPlate; ++EntryIndex)
			{
				const FPlateModuleDamage& Rule = Rows[RowIndex].Entries[EntryIndex];
				if (not ValidatePlateRuleEntry(Rule, EntryIndex) || Rule.TypeToDamage == ForbiddenGear)
				{
					return false;
				}
			}
		}
		return true;
	}

	constexpr bool ValidatePlateRules()
	{
		for (int32 ProfileIndex = 0; ProfileIndex < VehicleProfileCount; ++ProfileIndex)
		{
			for (int32 GearIndex = 0; GearIndex < RunningGearTypeCount; ++GearIndex)
			{
				if (not ValidatePlateRuleRows(ProfilePlateRules.Rows[ProfileIndex][GearIndex],
				                              static_cast<EVehicleRunningGear>(GearIndex)))
				{
					return false;
				}
			}
		}
		return true;
	}

	static_assert(MaxModuleInstances == 17, "The fixed module slot budget is 17 instances.");
	static_assert(MaxWeaponModules == 1, "This version models at most one damaged weapon per tank.");
	static_assert(SlotOffset::End == MaxModuleInstances, "Slot ranges must cover every instance exactly once.");
	static_assert(MaxModuleInstances <= 32, "Module bitmasks use uint32.");
	static_assert(MaxPlateBindings <= 64, "Add-on coverage masks use uint64 per module.");
	static_assert(static_cast<int32>(EArmorPlate::Turret_Mantlet) + 1 == ArmorPlateRuleCount,
		"Every EArmorPlate value needs one plate rule row.");
	static_assert(static_cast<int32>(EVehicleModuleTypes::Wheels) + 1 == ModuleTypeCount,
		"Every EVehicleModuleTypes value needs one table entry.");
	static_assert(static_cast<int32>(EVehicleModuleProfile::SuperHeavyTank) + 1 == VehicleProfileCount,
		"Every EVehicleModuleProfile value needs one profile rule.");
	static_assert(static_cast<int32>(EVehicleRunningGear::Wheels) + 1 == RunningGearTypeCount,
		"Every EVehicleRunningGear value needs one generated table.");
	static_assert(ValidateTypeTables(), "Invalid module thresholds, crew durations or calibre gates.");
	static_assert(ValidateProfileRules(), "Invalid vehicle module profile ratios.");
	static_assert(ValidatePlateRules(), "Invalid generated plate rule tables.");
	static_assert(1.f - PenetratingModuleDamageCap01 > GetMaxDestroyedThreshold01(),
		"A single capped hit must not turn a full-health module red.");
	static_assert(NonPenWindowBucketCount > 1 && NonPenBucketSeconds > 0.f, "Invalid non-pen window.");
	static_assert(FullModuleServiceWork > 0.f, "Finishing work must be positive.");
	static_assert(CrewRepairAbilitySlotIndex >= 0, "The command card needs a final slot.");
	static_assert(MaxNewDestroyedModulesPerImpact >= 1 && MaxNewDestroyedModulesPerImpact <= MaxModulesPerPlate,
		"Invalid destroyed-module limit.");
}
