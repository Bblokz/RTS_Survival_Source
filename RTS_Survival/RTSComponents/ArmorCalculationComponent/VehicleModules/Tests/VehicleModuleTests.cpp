// Copyright (C) Bas Blokzijl - All rights reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/Tanks/AITankMaster.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"

namespace VehicleModuleTestConstants
{
	constexpr float TankMaxHealth = 1000.f;
	constexpr float Tolerance = 0.001f;
	constexpr int32 EngineModuleId = VehicleModuleBalance::SlotOffset::Engine;
	constexpr int32 AmmoModuleId = VehicleModuleBalance::SlotOffset::Ammo;
	constexpr int32 LeftTrackModuleId = VehicleModuleBalance::LeftRunningGearSlot;
	constexpr int32 RightTrackModuleId = VehicleModuleBalance::RightRunningGearSlot;
	constexpr float DestroyingDamage = 100000.f;
	constexpr int32 ImpactsForProbabilityChecks = 64;
	constexpr uint8 TestPlayer = 1;
}

/** @brief Test-only access to private module state; builds isolated tanks without Blueprint setup. */
struct FVehicleModuleTestAccess
{
	static void InitializeTankComponents(ATankMaster& Tank)
	{
		Tank.HealthComponent = NewObject<UHealthComponent>(&Tank);
		Tank.HealthComponent->HealthLevelsToNotifyOn.Empty();
		Tank.HealthComponent->SetMaxHealth(VehicleModuleTestConstants::TankMaxHealth);
		Tank.UnitCommandData = NewObject<UCommandData>(&Tank);
		Tank.UnitCommandData->InitCommandData(&Tank);
		Tank.BehaviourComponent = NewObject<UBehaviourComp>(&Tank);
		Tank.AITankController = Tank.GetWorld()->SpawnActorDeferred<AAITankMaster>(
			AAITankMaster::StaticClass(), FTransform::Identity);
	}

	static void FinalizeTankModules(ATankMaster& Tank, UArmorCalculation& Armor)
	{
		Armor.FinalizeVehicleModuleSetup(&Tank, VehicleModuleTestConstants::TankMaxHealth);
		Tank.M_ModuleArmor = &Armor;
		Tank.bM_AreVehicleModulesInitialized = true;
		Tank.BeginPlay_InitVehicleModuleBindings();
		Tank.HealthComponent->InitializeTankRepairOwner(&Tank);
		Tank.RefreshModuleRepairCounts();
		Tank.InitializeCrewRepairAbilitySlot();
	}

	static void ExecuteCrewRepair(ATankMaster& Tank, const ECrewRepairAbilityType Subtype)
	{
		Tank.ExecuteCrewRepairCommand(Subtype);
	}

	static void TerminateCrewRepair(ATankMaster& Tank)
	{
		Tank.TerminateCrewRepairCommand(ECrewRepairAbilityType::EnableRepair);
		Tank.CleanupVehicleModuleBindings();
	}

	static void AdvanceCrewRepair(ATankMaster& Tank, const double Seconds)
	{
		Tank.M_CrewRepairState.ModuleWorkStartGameTime -= Seconds;
		Tank.CrewRepairTick(Tank.M_CrewRepairState.SessionGeneration);
	}

	static float ConsumeNonPenBudget(UArmorCalculation& Armor, const int32 ModuleId, const float Damage)
	{
		const int32 SlotIndex = Armor.GetModuleSlotById(ModuleId);
		FVehicleModule& Module = Armor.M_Modules[SlotIndex];
		return Armor.ConsumeNonPenBudget(Module.NonPenBudget, Module.MaxHp, Damage);
	}

	static const TArray<FUnitAbilityEntry>& GetCard(const ATankMaster& Tank)
	{
		return Tank.UnitCommandData->GetAbilities();
	}
};

namespace VehicleModuleTests
{
	/** @brief Owns a transient world with one isolated module tank. */
	struct FTankFixture
	{
		UWorld* World = nullptr;
		ATankMaster* Tank = nullptr;
		UArmorCalculation* Armor = nullptr;

		FTankFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Tank = World->SpawnActorDeferred<ATankMaster>(ATankMaster::StaticClass(), FTransform::Identity);
			FVehicleModuleTestAccess::InitializeTankComponents(*Tank);
			Armor = NewObject<UArmorCalculation>(Tank);
		}

		~FTankFixture()
		{
			FVehicleModuleTestAccess::TerminateCrewRepair(*Tank);
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		bool Install(const int32 ExpectedModuleId, const EVehicleModuleTypes Type, const bool bRightSide = false) const
		{
			FVehicleModuleSetup Setup;
			Setup.Type = Type;
			Setup.bRightSide = bRightSide;
			int32 AssignedModuleId = INDEX_NONE;
			return Armor->SetupModule(Setup, AssignedModuleId) && AssignedModuleId == ExpectedModuleId;
		}

		void Finalize() const
		{
			FVehicleModuleTestAccess::FinalizeTankModules(*Tank, *Armor);
		}

		UHealthComponent* GetHealth() const
		{
			return Tank->GetHealthComponent();
		}

		EVehicleModuleState GetState(const int32 ModuleId) const
		{
			return Armor->GetModuleSnapshot(ModuleId).State;
		}

		UStaticMeshComponent* RegisterHullPlate(const EArmorPlate PlateType) const
		{
			UStaticMeshComponent* HullMesh = NewObject<UStaticMeshComponent>(Tank);
			FArmorSettings PlateSettings;
			PlateSettings.ArmorType = PlateType;
			PlateSettings.ArmorValue = 50.f;
			Armor->InitArmorCalculation(HullMesh, {PlateSettings}, VehicleModuleTestConstants::TestPlayer);
			return HullMesh;
		}
	};
}

// ----------------------------------------------------------------------------------------------------
// Compile-time tables
// ----------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleBalanceTablesTest, "RTS.VehicleModules.BalanceTables",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleBalanceTablesTest::RunTest(const FString& Parameters)
{
	using namespace VehicleModuleBalance;
	const FPlateModuleRuleSet* HeavyRules = GetProfilePlateRules(EVehicleModuleProfile::HeavyTank,
	                                                             EVehicleRunningGear::Tracks);
	const FPlateModuleDamage& HeavyRearEngine = HeavyRules[TryGetPlateRuleIndex(EArmorPlate::Plate_Rear)].Entries[1];
	TestEqual(TEXT("Class chance factor is baked in once"), HeavyRearEngine.DamageProbability, 0.4875f,
	          VehicleModuleTestConstants::Tolerance);

	const FPlateModuleRuleSet* CarRules = GetProfilePlateRules(EVehicleModuleProfile::ArmoredCar,
	                                                           EVehicleRunningGear::Wheels);
	const FPlateModuleDamage& CarGear = CarRules[TryGetPlateRuleIndex(EArmorPlate::Plate_SideLowerLeft)].Entries[1];
	TestEqual(TEXT("Wheels replace tracks in the wheeled table"), CarGear.TypeToDamage, EVehicleModuleTypes::Wheels);
	TestEqual(TEXT("Wheel chance uses the wheel factor"), CarGear.DamageProbability, 0.7475f,
	          VehicleModuleTestConstants::Tolerance);

	TestEqual(TEXT("Engine recovers to threshold plus margin"), GetRecoveredHealth01(EVehicleModuleTypes::Engine),
	          0.20f, VehicleModuleTestConstants::Tolerance);
	TestEqual(TEXT("Tracks recover to 25%"), GetRecoveredHealth01(EVehicleModuleTypes::Tracks), 0.25f,
	          VehicleModuleTestConstants::Tolerance);
	TestEqual(TEXT("Equality at the threshold is red"),
	          GetModuleStateForHealth01(EVehicleModuleTypes::Engine, DestroyedHealth01::Engine),
	          EVehicleModuleState::Destroyed);
	TestEqual(TEXT("Recovered engine is yellow"),
	          GetModuleStateForHealth01(EVehicleModuleTypes::Engine, GetRecoveredHealth01(EVehicleModuleTypes::Engine)),
	          EVehicleModuleState::Damaged);
	const float EngineAndTwoTracks = GetCrewRepairSeconds(EVehicleModuleTypes::Engine)
		+ 2.f * GetCrewRepairSeconds(EVehicleModuleTypes::Tracks);
	TestEqual(TEXT("Engine plus two tracks takes 44 crew seconds"), EngineAndTwoTracks, 44.f);
	TestEqual(TEXT("Seventeen fixed module slots"), MaxModuleInstances, 17);
	return true;
}

// ----------------------------------------------------------------------------------------------------
// Command-card suppression and reserved CrewRepair slot
// ----------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleAbilitySuppressionTest, "RTS.VehicleModules.AbilitySuppression",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleAbilitySuppressionTest::RunTest(const FString& Parameters)
{
	constexpr int32 MoveCooldown = 9;
	UCommandData* CommandData = NewObject<UCommandData>();
	UObject* CrewSource = NewObject<UCommandData>();
	UObject* StunSource = NewObject<UCommandData>();
	FUnitAbilityEntry MoveEntry;
	MoveEntry.AbilityId = EAbilityID::IdMove;
	MoveEntry.CooldownRemaining = MoveCooldown;
	FUnitAbilityEntry AttackEntry;
	AttackEntry.AbilityId = EAbilityID::IdAttack;
	CommandData->SetAbilities({MoveEntry, AttackEntry, FUnitAbilityEntry()});

	const EAbilityID CrewAbilities[] = {EAbilityID::IdMove, EAbilityID::IdAttack};
	const EAbilityID StunAbilities[] = {EAbilityID::IdMove};
	FAbilitySuppressionHandle CrewHandle = CommandData->BeginAbilitySuppression(CrewSource, CrewAbilities);
	FAbilitySuppressionHandle StunHandle = CommandData->BeginAbilitySuppression(StunSource, StunAbilities);
	TestEqual(TEXT("Hidden entries show as empty"), CommandData->GetAbilities()[0].AbilityId, EAbilityID::IdNoAbility);

	CommandData->EndAbilitySuppression(CrewHandle);
	TestEqual(TEXT("Another source still hides Move"), CommandData->GetAbilities()[0].AbilityId,
	          EAbilityID::IdNoAbility);
	TestEqual(TEXT("Attack returns to its original index"), CommandData->GetAbilities()[1].AbilityId,
	          EAbilityID::IdAttack);

	CommandData->EndAbilitySuppression(StunHandle);
	TestEqual(TEXT("Move returns to its original index"), CommandData->GetAbilities()[0].AbilityId, EAbilityID::IdMove);
	TestEqual(TEXT("Hidden cooldown metadata is preserved"), CommandData->GetAbilities()[0].CooldownRemaining,
	          MoveCooldown);

	CrewHandle = CommandData->BeginAbilitySuppression(CrewSource, StunAbilities);
	FUnitAbilityEntry RepairEntry;
	RepairEntry.AbilityId = EAbilityID::IdRepair;
	TestTrue(TEXT("A new grant is placed in a free slot"), CommandData->AddAbility(RepairEntry, INDEX_NONE));
	TestEqual(TEXT("The hidden slot is not reused"), CommandData->GetAbilities()[2].AbilityId, EAbilityID::IdRepair);
	TestTrue(TEXT("A hidden entry can be revoked"), CommandData->RemoveAbility(EAbilityID::IdMove));
	CommandData->EndAbilitySuppression(CrewHandle);
	TestEqual(TEXT("Revoked grants are not restored"), CommandData->GetAbilities()[0].AbilityId,
	          EAbilityID::IdNoAbility);

	TestTrue(TEXT("The final slot can be reserved"), CommandData->ReserveCrewRepairAbilitySlot());
	TestEqual(TEXT("The card is sized to its capacity"), CommandData->GetAbilities().Num(),
	          DeveloperSettings::GamePlay::ActionUI::MaxAbilitiesForActionUI);
	FUnitAbilityEntry CrewRepairEntry;
	CrewRepairEntry.AbilityId = EAbilityID::IdCrewRepair;
	TestTrue(TEXT("CrewRepair writes the reserved slot"), CommandData->SetCrewRepairAbilityEntry(CrewRepairEntry));
	TestFalse(TEXT("An unchanged entry is not rewritten"), CommandData->SetCrewRepairAbilityEntry(CrewRepairEntry));
	TestEqual(TEXT("CrewRepair lives in the final slot"),
	          CommandData->GetAbilities()[VehicleModuleBalance::CrewRepairAbilitySlotIndex].AbilityId,
	          EAbilityID::IdCrewRepair);
	return true;
}

// ----------------------------------------------------------------------------------------------------
// Damage resolver
// ----------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleDamageResolverTest, "RTS.VehicleModules.DamageResolver",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleDamageResolverTest::RunTest(const FString& Parameters)
{
	using namespace VehicleModuleTestConstants;
	VehicleModuleTests::FTankFixture Fixture;
	Fixture.RegisterHullPlate(EArmorPlate::Plate_Rear);
	Fixture.Install(EngineModuleId, EVehicleModuleTypes::Engine);
	Fixture.Finalize();

	constexpr float BaseDamage = 400.f;
	constexpr float Calibre = 75.f;
	const float EngineMaxHp = Fixture.Armor->GetModuleSnapshot(EngineModuleId).MaxHp;
	TestEqual(TEXT("Medium engine HP derives from tank MaxHealth"), EngineMaxHp, 350.f, Tolerance);

	for (int32 ImpactIndex = 0; ImpactIndex < ImpactsForProbabilityChecks; ++ImpactIndex)
	{
		Fixture.Armor->CalculateModuleDamage(EArmorPlate::Plate_Rear, 50.f, false, 0.f, BaseDamage, Calibre);
	}
	TestEqual(TEXT("Non-penetrating hits never damage the engine"),
	          Fixture.Armor->GetModuleSnapshot(EngineModuleId).CurrentHp, EngineMaxHp, Tolerance);

	Fixture.Armor->CalculateModuleDamage(EArmorPlate::Plate_Rear, 50.f, true, BaseDamage, BaseDamage, 19.9f);
	TestEqual(TEXT("Rounds below the calibre gate never damage modules"),
	          Fixture.Armor->GetModuleSnapshot(EngineModuleId).CurrentHp, EngineMaxHp, Tolerance);

	bool bEngineWasHit = false;
	for (int32 ImpactIndex = 0; ImpactIndex < ImpactsForProbabilityChecks && not bEngineWasHit; ++ImpactIndex)
	{
		Fixture.Armor->CalculateModuleDamage(EArmorPlate::Plate_Rear, 50.f, true, BaseDamage, BaseDamage, Calibre);
		bEngineWasHit = Fixture.Armor->GetModuleSnapshot(EngineModuleId).CurrentHp < EngineMaxHp;
	}
	TestTrue(TEXT("Penetrating rear hits roll the engine"), bEngineWasHit);
	TestNotEqual(TEXT("A fresh module cannot become red from one hit"), Fixture.GetState(EngineModuleId),
	             EVehicleModuleState::Destroyed);
	TestTrue(TEXT("The per-impact penetrating cap holds"),
	         Fixture.Armor->GetModuleSnapshot(EngineModuleId).CurrentHp
	         >= EngineMaxHp * (1.f - VehicleModuleBalance::PenetratingModuleDamageCap01) - Tolerance);

	const float FirstNonPenDamage = FVehicleModuleTestAccess::ConsumeNonPenBudget(*Fixture.Armor, EngineModuleId,
	                                                                               DestroyingDamage);
	const float SecondNonPenDamage = FVehicleModuleTestAccess::ConsumeNonPenBudget(*Fixture.Armor, EngineModuleId,
	                                                                                DestroyingDamage);
	TestEqual(TEXT("The non-pen window caps damage across attackers"), FirstNonPenDamage,
	          EngineMaxHp * VehicleModuleBalance::NonPenWindowDamageCap01, Tolerance);
	TestEqual(TEXT("A second attacker cannot bypass the shared window"), SecondNonPenDamage, 0.f, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleRunningGearRoutingTest, "RTS.VehicleModules.RunningGearRouting",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleRunningGearRoutingTest::RunTest(const FString& Parameters)
{
	using namespace VehicleModuleTestConstants;
	VehicleModuleTests::FTankFixture Fixture;
	Fixture.RegisterHullPlate(EArmorPlate::Plate_SideLowerLeft);
	Fixture.Install(LeftTrackModuleId, EVehicleModuleTypes::Tracks, false);
	Fixture.Install(RightTrackModuleId, EVehicleModuleTypes::Tracks, true);
	Fixture.Finalize();

	for (int32 ImpactIndex = 0; ImpactIndex < ImpactsForProbabilityChecks; ++ImpactIndex)
	{
		Fixture.Armor->CalculateModuleDamage(EArmorPlate::Plate_SideLowerLeft, 50.f, true, 200.f, 200.f, 75.f);
	}
	TestTrue(TEXT("The struck side's track is damaged"),
	         Fixture.GetState(LeftTrackModuleId) != EVehicleModuleState::Healthy);
	TestEqual(TEXT("The other side is never routed"), Fixture.GetState(RightTrackModuleId),
	          EVehicleModuleState::Healthy);
	return true;
}

// ----------------------------------------------------------------------------------------------------
// Ordinary healing milestones
// ----------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleHealingTest, "RTS.VehicleModules.HealingAndService",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleHealingTest::RunTest(const FString& Parameters)
{
	using namespace VehicleModuleTestConstants;
	VehicleModuleTests::FTankFixture Fixture;
	Fixture.Install(EngineModuleId, EVehicleModuleTypes::Engine);
	Fixture.Install(AmmoModuleId, EVehicleModuleTypes::Ammo);
	Fixture.Finalize();
	UHealthComponent* Health = Fixture.GetHealth();

	Fixture.Armor->DamageModule(EngineModuleId, DestroyingDamage);
	Fixture.Armor->DamageModule(AmmoModuleId, DestroyingDamage);
	Health->SetCurrentHealth(700.f);
	Health->Heal(49.f);
	TestEqual(TEXT("Below the hull gate reds stay red"), Fixture.Armor->GetDestroyedModuleCount(), 2);
	Health->Heal(1.f);
	TestEqual(TEXT("A heal reaching the gate recovers every red module"), Fixture.Armor->GetDestroyedModuleCount(), 0);
	TestEqual(TEXT("Recovery sets threshold plus margin"), Fixture.Armor->GetModuleSnapshot(EngineModuleId).CurrentHp,
	          70.f, Tolerance);
	TestTrue(TEXT("Yellow modules keep the tank eligible for repair"), Health->GetHasDamageToRepair());
	TestTrue(TEXT("Hull plus 60 surplus completes everything in one heal"), Health->Heal(310.f));
	TestEqual(TEXT("No damaged modules remain"), Fixture.Armor->GetNonHealthyModuleCount(), 0);

	Fixture.Armor->DamageModule(EngineModuleId, DestroyingDamage);
	Health->SetCurrentHealth(700.f);
	TestTrue(TEXT("A strong heal restores red directly to healthy"), Health->Heal(360.f));
	TestEqual(TEXT("Direct final state is healthy"), Fixture.GetState(EngineModuleId), EVehicleModuleState::Healthy);

	Fixture.Armor->DamageModule(EngineModuleId, DestroyingDamage);
	TestFalse(TEXT("Partial finishing work does not complete"), Health->Heal(30.f));
	TestEqual(TEXT("At full hull a partial heal still recovers red to yellow"), Fixture.GetState(EngineModuleId),
	          EVehicleModuleState::Damaged);
	Fixture.Armor->DamageModule(EngineModuleId, 1.f);
	TestFalse(TEXT("New module damage resets finishing work"), Health->Heal(30.f));
	TestTrue(TEXT("The remaining work completes at full hull"), Health->Heal(30.f));
	TestEqual(TEXT("Hull health is never over-healed"), Health->GetCurrentHealth(), TankMaxHealth, Tolerance);
	return true;
}

// ----------------------------------------------------------------------------------------------------
// CrewRepair
// ----------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleCrewRepairTest, "RTS.VehicleModules.CrewRepair",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleCrewRepairTest::RunTest(const FString& Parameters)
{
	using namespace VehicleModuleTestConstants;
	constexpr float LowHullHealth = 200.f;
	constexpr int32 ReservedIndex = VehicleModuleBalance::CrewRepairAbilitySlotIndex;
	VehicleModuleTests::FTankFixture Fixture;
	FUnitAbilityEntry MoveEntry;
	MoveEntry.AbilityId = EAbilityID::IdMove;
	Fixture.Tank->GetIsValidCommandData()->SetAbilities({MoveEntry});
	Fixture.Install(EngineModuleId, EVehicleModuleTypes::Engine);
	Fixture.Install(AmmoModuleId, EVehicleModuleTypes::Ammo);
	Fixture.Finalize();
	UHealthComponent* Health = Fixture.GetHealth();

	Fixture.Armor->DamageModule(EngineModuleId, DestroyingDamage);
	Fixture.Armor->DamageModule(AmmoModuleId, DestroyingDamage);
	Health->SetCurrentHealth(LowHullHealth);
	const TArray<FUnitAbilityEntry>& Card = FVehicleModuleTestAccess::GetCard(*Fixture.Tank);
	TestEqual(TEXT("Red modules insert CrewRepair in the final slot"), Card[ReservedIndex].AbilityId,
	          EAbilityID::IdCrewRepair);
	TestEqual(TEXT("The inactive entry is EnableRepair"), Card[ReservedIndex].CustomType,
	          static_cast<int32>(ECrewRepairAbilityType::EnableRepair));

	FVehicleModuleTestAccess::ExecuteCrewRepair(*Fixture.Tank, ECrewRepairAbilityType::EnableRepair);
	TestTrue(TEXT("CrewRepair starts below the ordinary hull gate"), Fixture.Tank->GetIsCrewRepairActive());
	TestEqual(TEXT("The active entry is DisableRepair"), Card[ReservedIndex].CustomType,
	          static_cast<int32>(ECrewRepairAbilityType::DisableRepair));
	TestEqual(TEXT("Move is hidden while repairing"), Card[0].AbilityId, EAbilityID::IdNoAbility);

	FVehicleModuleTestAccess::AdvanceCrewRepair(*Fixture.Tank, 19.0);
	TestEqual(TEXT("The engine needs its full duration"), Fixture.GetState(EngineModuleId),
	          EVehicleModuleState::Destroyed);
	FVehicleModuleTestAccess::AdvanceCrewRepair(*Fixture.Tank, 1.0);
	TestEqual(TEXT("The engine is repaired first"), Fixture.GetState(EngineModuleId), EVehicleModuleState::Damaged);
	TestEqual(TEXT("CrewRepair never heals the hull"), Health->GetCurrentHealth(), LowHullHealth, Tolerance);

	FVehicleModuleTestAccess::ExecuteCrewRepair(*Fixture.Tank, ECrewRepairAbilityType::DisableRepair);
	TestFalse(TEXT("DisableRepair stops immediately"), Fixture.Tank->GetIsCrewRepairActive());
	TestEqual(TEXT("Move is restored"), Card[0].AbilityId, EAbilityID::IdMove);
	TestEqual(TEXT("Remaining reds keep EnableRepair"), Card[ReservedIndex].CustomType,
	          static_cast<int32>(ECrewRepairAbilityType::EnableRepair));

	FVehicleModuleTestAccess::ExecuteCrewRepair(*Fixture.Tank, ECrewRepairAbilityType::EnableRepair);
	Health->Heal(550.f);
	TestFalse(TEXT("External recovery of every red module stops the crew"), Fixture.Tank->GetIsCrewRepairActive());
	TestEqual(TEXT("No reds remain, so the entry is removed"), Card[ReservedIndex].AbilityId,
	          EAbilityID::IdNoAbility);
	TestEqual(TEXT("Crew restrictions are released"), Card[0].AbilityId, EAbilityID::IdMove);
	return true;
}

// ----------------------------------------------------------------------------------------------------
// Persistence and MaxHealth upgrades
// ----------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleAutomaticIdsTest, "RTS.VehicleModules.AutomaticIds",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleAutomaticIdsTest::RunTest(const FString& Parameters)
{
	VehicleModuleTests::FTankFixture Fixture;
	FVehicleModuleSetup Setup;
	int32 AssignedModuleId = INDEX_NONE;

	Setup.Type = EVehicleModuleTypes::Tracks;
	Setup.bRightSide = true;
	TestTrue(TEXT("Right track installs without an authored ID"), Fixture.Armor->SetupModule(Setup, AssignedModuleId));
	TestEqual(TEXT("Right track uses its fixed side slot"), AssignedModuleId,
	          VehicleModuleTestConstants::RightTrackModuleId);
	Setup.bRightSide = false;
	TestTrue(TEXT("Left track installs after right track"), Fixture.Armor->SetupModule(Setup, AssignedModuleId));
	TestEqual(TEXT("Left track ID does not depend on setup order"), AssignedModuleId,
	          VehicleModuleTestConstants::LeftTrackModuleId);

	Setup.Type = EVehicleModuleTypes::Engine;
	TestTrue(TEXT("Engine installs without an authored ID"), Fixture.Armor->SetupModule(Setup, AssignedModuleId));
	TestEqual(TEXT("Engine ID is its fixed slot"), AssignedModuleId, VehicleModuleTestConstants::EngineModuleId);
	TestEqual(TEXT("Assigned ID retrieves the installed engine"), Fixture.Armor->GetModuleSnapshot(AssignedModuleId).Type,
	          EVehicleModuleTypes::Engine);

	Setup.Type = EVehicleModuleTypes::AddOnArmor;
	TestTrue(TEXT("First add-on zone installs"), Fixture.Armor->SetupModule(Setup, AssignedModuleId));
	TestEqual(TEXT("First add-on zone gets the first add-on slot"), AssignedModuleId,
	          VehicleModuleBalance::SlotOffset::AddOnArmor);
	TestTrue(TEXT("Second add-on zone installs"), Fixture.Armor->SetupModule(Setup, AssignedModuleId));
	TestEqual(TEXT("Second add-on zone gets the next add-on slot"), AssignedModuleId,
	          VehicleModuleBalance::SlotOffset::AddOnArmor + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModulePersistenceTest, "RTS.VehicleModules.ProfilesAndPersistence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModulePersistenceTest::RunTest(const FString& Parameters)
{
	using namespace VehicleModuleTestConstants;
	VehicleModuleTests::FTankFixture Fixture;
	TestTrue(TEXT("Profile selection before installation"),
	         Fixture.Armor->SetVehicleModuleProfile(EVehicleModuleProfile::HeavyTank));
	Fixture.Install(EngineModuleId, EVehicleModuleTypes::Engine);
	Fixture.Finalize();
	TestEqual(TEXT("Heavy engine HP"), Fixture.Armor->GetModuleSnapshot(EngineModuleId).MaxHp, 450.f, Tolerance);

	Fixture.Armor->DamageModule(EngineModuleId, 150.f);
	const FVehicleModuleSaveData SaveData = Fixture.Tank->ExportVehicleModuleSaveData();
	Fixture.GetHealth()->SetMaxHealth(2.f * TankMaxHealth);
	TestEqual(TEXT("MaxHealth upgrades preserve the module health fraction"),
	          Fixture.Armor->GetModuleSnapshot(EngineModuleId).CurrentHp, 600.f, Tolerance);

	Fixture.Armor->DamageModule(EngineModuleId, DestroyingDamage);
	TestTrue(TEXT("Saved state imports into the same setup"), Fixture.Tank->ImportVehicleModuleSaveData(SaveData));
	TestEqual(TEXT("Imported HP derives from the current MaxHealth"),
	          Fixture.Armor->GetModuleSnapshot(EngineModuleId).CurrentHp, 600.f, Tolerance);
	TestFalse(TEXT("Load never resumes crew work"), Fixture.Tank->GetIsCrewRepairActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleLegacyIdsTest, "RTS.VehicleModules.LegacyIds",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleLegacyIdsTest::RunTest(const FString& Parameters)
{
	using namespace VehicleModuleTestConstants;
	VehicleModuleTests::FTankFixture Fixture;
	UStaticMeshComponent* HullMesh = Fixture.RegisterHullPlate(EArmorPlate::Plate_Front);
	TestTrue(TEXT("Engine installs with its automatic ID"), Fixture.Install(EngineModuleId, EVehicleModuleTypes::Engine));

	FVehicleModuleSetup AddOnSetup;
	AddOnSetup.Type = EVehicleModuleTypes::AddOnArmor;
	int32 AddOnModuleId = INDEX_NONE;
	TestTrue(TEXT("Add-on zone installs with an automatic ID"), Fixture.Armor->SetupModule(AddOnSetup, AddOnModuleId));
	FAddOnArmorPlateBinding PlateBinding;
	PlateBinding.MeshWithArmor = HullMesh;
	PlateBinding.PlateType = EArmorPlate::Plate_Front;
	TestTrue(TEXT("Add-on zone covers the registered plate"),
	         Fixture.Armor->SetAddOnArmorPlateCoverage(AddOnModuleId, {PlateBinding}));
	Fixture.Finalize();

	FVehicleModuleSaveData LegacySaveData = Fixture.Armor->ExportModuleState();
	LegacySaveData.RuleVersion = VehicleModuleBalance::DesignerAssignedModuleIdRuleVersion;
	LegacySaveData.Modules[0].ModuleId = 101;
	LegacySaveData.Modules[0].HealthFraction = 0.5f;
	LegacySaveData.Modules[1].ModuleId = 202;
	LegacySaveData.Coverage[0].ModuleId = 202;

	const TArray<FAddOnArmorPlateBinding> EmptyCoverage;
	TestTrue(TEXT("Coverage can be cleared before loading"),
	         Fixture.Armor->SetAddOnArmorPlateCoverage(AddOnModuleId, EmptyCoverage));
	TestTrue(TEXT("Legacy designer IDs map back to installed slots"), Fixture.Armor->ImportModuleState(LegacySaveData));
	TestEqual(TEXT("Legacy health is restored to the engine"),
	          Fixture.Armor->GetModuleSnapshot(EngineModuleId).CurrentHp,
	          Fixture.Armor->GetModuleSnapshot(EngineModuleId).MaxHp * 0.5f, Tolerance);
	TestEqual(TEXT("Legacy add-on coverage is restored to the correct zone"),
	          Fixture.Armor->ExportModuleState().Coverage.Num(), 1);
	return true;
}

#endif
