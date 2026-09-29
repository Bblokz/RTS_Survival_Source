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
			{0.f, 0.15f, 0.18f, 0.25f, 0.22f, 0.22f, 0.20f, 0.18f},
			{0.f, 1.00f, 1.15f, 1.20f, 1.10f, 1.10f, 1.10f, 1.15f},
			EVehicleRunningGear::Wheels
		},
		// LightTank
		{
			{0.f, 0.18f, 0.22f, 0.30f, 0.26f, 0.26f, 0.23f, 0.22f},
			{0.f, 1.00f, 1.05f, 1.10f, 1.05f, 1.05f, 1.05f, 1.05f},
			EVehicleRunningGear::Tracks
		},
		// MediumTank
		{
			{0.f, 0.20f, 0.25f, 0.35f, 0.30f, 0.30f, 0.25f, 0.25f},
			{0.f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f},
			EVehicleRunningGear::Tracks
		},
		// HeavyTank
		{
			{0.f, 0.25f, 0.30f, 0.45f, 0.36f, 0.36f, 0.30f, 0.30f},
			{0.f, 1.00f, 0.90f, 0.75f, 0.90f, 0.90f, 0.95f, 0.90f},
			EVehicleRunningGear::Tracks
		},
		// SuperHeavyTank
		{
			{0.f, 0.30f, 0.35f, 0.55f, 0.42f, 0.42f, 0.35f, 0.35f},
			{0.f, 1.00f, 0.85f, 0.60f, 0.80f, 0.85f, 0.90f, 0.85f},
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
		return MakeTypedRule(EVehicleModuleTypes::AddOnArmor, 1.00f, DamageMultiplier);
	}

	using EModule = EVehicleModuleTypes;

	// Rows follow EArmorPlate order. Tuple = probability for penetrating hits / damage multiplier.
	inline constexpr FPlateModuleRuleSet BasePlateRules[ArmorPlateRuleCount] = {
		// Plate_Front
		MakeRuleSet(MakeAddOnRule(0.30f), MakeTypedRule(EModule::Tracks, 0.12f, 0.30f),
		            MakeTypedRule(EModule::Ammo, 0.08f, 0.30f)),
		// Plate_FrontUpperGlacis
		MakeRuleSet(MakeAddOnRule(0.35f), MakeTypedRule(EModule::Ammo, 0.10f, 0.30f)),
		// Plate_FrontLowerGlacis
		MakeRuleSet(MakeAddOnRule(0.25f), MakeTypedRule(EModule::Tracks, 0.35f, 0.55f),
		            MakeTypedRule(EModule::Engine, 0.10f, 0.30f)),
		// Plate_SideLeft
		MakeRuleSet(MakeAddOnRule(0.30f), MakeTypedRule(EModule::Tracks, 0.25f, 0.40f),
		            MakeTypedRule(EModule::Ammo, 0.30f, 0.50f)),
		// Plate_SideRight
		MakeRuleSet(MakeAddOnRule(0.30f), MakeTypedRule(EModule::Tracks, 0.25f, 0.40f),
		            MakeTypedRule(EModule::Ammo, 0.30f, 0.50f)),
		// Plate_SideLowerLeft
		MakeRuleSet(MakeAddOnRule(0.20f), MakeTypedRule(EModule::Tracks, 0.65f, 0.70f),
		            MakeTypedRule(EModule::Ammo, 0.15f, 0.30f)),
		// Plate_SideLowerRight
		MakeRuleSet(MakeAddOnRule(0.20f), MakeTypedRule(EModule::Tracks, 0.65f, 0.70f),
		            MakeTypedRule(EModule::Ammo, 0.15f, 0.30f)),
		// Plate_Rear
		MakeRuleSet(MakeAddOnRule(0.25f), MakeTypedRule(EModule::Engine, 0.65f, 0.70f),
		            MakeTypedRule(EModule::Ammo, 0.20f, 0.40f)),
		// Plate_RearLowerGlacis
		MakeRuleSet(MakeAddOnRule(0.20f), MakeTypedRule(EModule::Engine, 0.60f, 0.65f),
		            MakeTypedRule(EModule::Tracks, 0.30f, 0.40f)),
		// Plate_RearUpperGlacis
		MakeRuleSet(MakeAddOnRule(0.25f), MakeTypedRule(EModule::Engine, 0.65f, 0.70f),
		            MakeTypedRule(EModule::Ammo, 0.25f, 0.45f)),
		// Turret_Front
		MakeRuleSet(MakeAddOnRule(0.25f), MakeTypedRule(EModule::Turret, 0.30f, 0.45f),
		            MakeTypedRule(EModule::Weapon, 0.20f, 0.40f)),
		// Turret_SideLeft
		MakeRuleSet(MakeAddOnRule(0.25f), MakeTypedRule(EModule::Turret, 0.40f, 0.50f),
		            MakeTypedRule(EModule::Ammo, 0.25f, 0.45f)),
		// Turret_SideRight
		MakeRuleSet(MakeAddOnRule(0.25f), MakeTypedRule(EModule::Turret, 0.40f, 0.50f),
		            MakeTypedRule(EModule::Ammo, 0.25f, 0.45f)),
		// Turret_Rear
		MakeRuleSet(MakeAddOnRule(0.25f), MakeTypedRule(EModule::Turret, 0.35f, 0.50f),
		            MakeTypedRule(EModule::Ammo, 0.40f, 0.55f)),
		// Turret_SidesAndRear
		MakeRuleSet(MakeAddOnRule(0.25f), MakeTypedRule(EModule::Turret, 0.40f, 0.50f),
		            MakeTypedRule(EModule::Ammo, 0.30f, 0.50f)),
		// Turret_Cupola
		MakeRuleSet(MakeAddOnRule(0.25f), MakeTypedRule(EModule::Turret, 0.15f, 0.25f),
		            MakeTypedRule(EModule::Ammo, 0.10f, 0.25f)),
		// Turret_Mantlet
		MakeRuleSet(MakeAddOnRule(0.25f), MakeTypedRule(EModule::Weapon, 0.60f, 0.65f),
		            MakeTypedRule(EModule::Turret, 0.30f, 0.45f))
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
		inline constexpr FShellModuleRule Kinetic = MakeShellRule(1.00f, 0.35f, 0.15f);
		inline constexpr FShellModuleRule ArmorPiercingHighExplosive = MakeShellRule(1.15f, 0.35f, 0.15f);
		inline constexpr FShellModuleRule HighExplosive = MakeShellRule(1.10f, 0.70f, 0.35f);
		inline constexpr FShellModuleRule HighExplosiveAntiTank = MakeShellRule(1.00f, 0.70f, 0.35f);
		inline constexpr FShellModuleRule Railgun = MakeShellRule(1.00f, 0.35f, 0.15f);
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
	inline constexpr FMineModuleRuleData MineModuleRule = {1.0f, 0.70f};

	/** @brief Splash reaches only external modules and is attenuated by the AOE falloff already applied. */
	struct FSplashModuleRuleData
	{
		float ProbabilityMultiplier;
		float EnergyMultiplier;
		// Probe length used to find the hull plate facing the explosion.
		float PlateProbeDistanceCm;
	};

	inline constexpr FSplashModuleRuleData SplashModuleRules = {0.50f, 0.25f, 5000.f};

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
		inline constexpr float DestroyedMobilityMultiplier = 0.0f;
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
