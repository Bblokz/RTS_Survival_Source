// Copyright (C) Bas Blokzijl - All rights reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleSubsystem.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/Tanks/AITankMaster.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Behaviours/Derived/Damage/AmmoCookOff/AmmoCookOffBehaviour.h"
#include "RTS_Survival/Behaviours/Derived/Damage/TankEngineFire/TankEngineFireBehaviour.h"

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
	constexpr TCHAR Panzer38TChassisMeshPath[] =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/Panzer38T/Meshes/SK_Tracks_PZ38_T.SK_Tracks_PZ38_T");
	constexpr TCHAR Panzer38THullMeshPath[] =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/Panzer38T/Meshes/SM_Hull_PZ38_T.SM_Hull_PZ38_T");
	constexpr TCHAR VehicleModuleDataAssetPath[] =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/ModuleSystem/DA_VehicleModules.DA_VehicleModules");
}

/** @brief Exposes engine-fire attachment state so the real light-tank assets can be regression tested. */
struct FTankEngineFireBehaviourTestAccess
{
	static void SetTankMaster(UTankEngineFireBehaviour& Behaviour, ATankMaster& Tank)
	{
		Behaviour.M_TankMaster = &Tank;
	}

	static const FTankEngineFireAttachmentRules& GetAttachmentRules(const UTankEngineFireBehaviour& Behaviour)
	{
		return Behaviour.M_AttachmentRules;
	}

	static UMeshComponent* FindTankMeshWithSocket(const UTankEngineFireBehaviour& Behaviour, const FName SocketName)
	{
		return Behaviour.FindTankMeshWithSocket(SocketName);
	}
};

/** @brief Test-only access to private module state; builds isolated tanks without Blueprint setup. */
struct FVehicleModuleTestAccess
{
	static void SetModuleDataAsset(UVehicleModuleSubsystem& Subsystem, UVehicleModuleDataAsset* DataAsset)
	{
		Subsystem.M_ModuleDataAsset = DataAsset;
	}

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
		Tank.M_TurretModuleRegistration.bBlueprintModuleSetupComplete = true;
		Tank.M_TurretModuleRegistration.bInitialTurretsDiscovered = true;
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

	/** @brief A turret that never begins play: its mount mesh and weapons exist, registration is driven manually. */
	static ACPPTurretsMaster* SpawnTurretWithWeapons(UWorld& World, const TArray<float>& WeaponCalibres)
	{
		ACPPTurretsMaster* Turret = World.SpawnActorDeferred<ACPPTurretsMaster>(
			ACPPTurretsMaster::StaticClass(), FTransform::Identity);
		Turret->SceneSkeletalMesh = NewObject<USkeletalMeshComponent>(Turret);
		for (const float WeaponCalibre : WeaponCalibres)
		{
			UWeaponState* Weapon = NewObject<UWeaponState>(Turret);
			Weapon->WeaponData.WeaponCalibre = WeaponCalibre;
			Turret->M_TWeapons.Add(Weapon);
		}
		return Turret;
	}

	static UWeaponState* GetTurretWeapon(const ACPPTurretsMaster& Turret, const int32 WeaponIndex)
	{
		return Turret.M_TWeapons[WeaponIndex];
	}

	static void SetTurretWeaponRange(ACPPTurretsMaster& Turret, const int32 WeaponIndex, const float WeaponRange)
	{
		if (not Turret.M_TWeapons.IsValidIndex(WeaponIndex) || not IsValid(Turret.M_TWeapons[WeaponIndex]))
		{
			return;
		}
		Turret.M_TWeapons[WeaponIndex]->WeaponData.Range = WeaponRange;
	}

	static void AddTankTurret(ATankMaster& Tank, ACPPTurretsMaster& Turret)
	{
		Tank.Turrets.Add(&Turret);
		Turret.InitTurretOwner(&Tank);
	}

	static void ForceTurretCachedRange(ACPPTurretsMaster& Turret, const float WeaponRange)
	{
		Turret.M_WeaponRangeData.ForceSetRange(WeaponRange);
	}

	static void AddPendingTurret(ATankMaster& Tank, ACPPTurretsMaster* Turret)
	{
		Tank.AddPendingTurretModuleRegistration(Turret);
	}

	static void MarkTurretReady(ATankMaster& Tank, ACPPTurretsMaster* Turret)
	{
		Tank.OnTurretReadyForModuleRegistration(Turret);
	}

	static void SignalTankBlueprintModulesComplete(ATankMaster& Tank, UArmorCalculation& Armor)
	{
		Tank.BeginVehicleModuleFinalization(&Armor);
		Tank.GetWorld()->GetTimerManager().ClearTimer(
			Tank.M_TurretModuleRegistration.InitialTurretDiscoveryTimer);
		Tank.DiscoverInitialTurretsAndTryFinalize();
	}

	static bool GetAreTankModulesInitialized(const ATankMaster& Tank)
	{
		return Tank.bM_AreVehicleModulesInitialized;
	}

	static void CancelTurret(ATankMaster& Tank, const ACPPTurretsMaster* Turret)
	{
		Tank.CancelTurretModuleRegistration(Turret);
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
			RegisterMeshPlate(HullMesh, PlateType);
			return HullMesh;
		}

		void RegisterMeshPlate(UMeshComponent* Mesh, const EArmorPlate PlateType) const
		{
			FArmorSettings PlateSettings;
			PlateSettings.ArmorType = PlateType;
			PlateSettings.ArmorValue = 50.f;
			Armor->InitArmorCalculation(Mesh, {PlateSettings}, VehicleModuleTestConstants::TestPlayer);
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
	TestEqual(TEXT("Class chance factor is baked in once"), HeavyRearEngine.DamageProbability,
	          RuleSetProbabilities::Plate_Rear::Engine * ProfileDamageChanceMultipliers::HeavyTank::Engine,
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleEngineFireChanceTest, "RTS.VehicleModules.EngineFireChance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleEngineFireChanceTest::RunTest(const FString& Parameters)
{
	using VehicleModuleBalance::EngineFire::GetChance;
	TestEqual(TEXT("Damaged engine with AP"),
		GetChance(EVehicleModuleState::Damaged, EWeaponShellType::Shell_AP), 0.25f);
	TestEqual(TEXT("Destroyed engine with AP"),
		GetChance(EVehicleModuleState::Destroyed, EWeaponShellType::Shell_AP), 0.5f);
	TestEqual(TEXT("Damaged engine with HE"),
		GetChance(EVehicleModuleState::Damaged, EWeaponShellType::Shell_HE), 0.33f);
	TestEqual(TEXT("Damaged engine with HEAT"),
		GetChance(EVehicleModuleState::Damaged, EWeaponShellType::Shell_HEAT), 0.33f);
	TestEqual(TEXT("Destroyed engine with HE"),
		GetChance(EVehicleModuleState::Destroyed, EWeaponShellType::Shell_HE), 0.8f);
	TestEqual(TEXT("Destroyed engine with HEAT"),
		GetChance(EVehicleModuleState::Destroyed, EWeaponShellType::Shell_HEAT), 0.8f);
	TestEqual(TEXT("Healthy engine cannot ignite"),
		GetChance(EVehicleModuleState::Healthy, EWeaponShellType::Shell_HE), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleEngineFireProfileTest, "RTS.VehicleModules.EngineFireProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleEngineFireProfileTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UVehicleModuleSubsystem* Subsystem = NewObject<UVehicleModuleSubsystem>(GameInstance);
	UVehicleModuleDataAsset* DataAsset = NewObject<UVehicleModuleDataAsset>(Subsystem);
	FVehicleModuleTestAccess::SetModuleDataAsset(*Subsystem, DataAsset);
	const TSubclassOf<UTankEngineFireBehaviour> FireClass = UTankEngineFireBehaviour::StaticClass();

	DataAsset->ArmoredCarEngineFire = FireClass;
	TestTrue(TEXT("Armored car selects its fire"),
		Subsystem->GetEngineFireBehaviourClass(EVehicleModuleProfile::ArmoredCar) == FireClass);
	DataAsset->ArmoredCarEngineFire = nullptr;
	DataAsset->LightTankEngineFire = FireClass;
	TestTrue(TEXT("Light tank selects its fire"),
		Subsystem->GetEngineFireBehaviourClass(EVehicleModuleProfile::LightTank) == FireClass);
	DataAsset->LightTankEngineFire = nullptr;
	DataAsset->MediumTankEngineFire = FireClass;
	TestTrue(TEXT("Medium tank selects its fire"),
		Subsystem->GetEngineFireBehaviourClass(EVehicleModuleProfile::MediumTank) == FireClass);
	DataAsset->MediumTankEngineFire = nullptr;
	DataAsset->HeavyTankEngineFire = FireClass;
	TestTrue(TEXT("Heavy tank selects its fire"),
		Subsystem->GetEngineFireBehaviourClass(EVehicleModuleProfile::HeavyTank) == FireClass);
	TestTrue(TEXT("Super-heavy tank shares heavy fire"),
		Subsystem->GetEngineFireBehaviourClass(EVehicleModuleProfile::SuperHeavyTank) == FireClass);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleLightTankEngineFireAttachmentTest,
	"RTS.VehicleModules.LightTankEngineFireAttachment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleLightTankEngineFireAttachmentTest::RunTest(const FString& Parameters)
{
	UVehicleModuleDataAsset* ModuleDataAsset = LoadObject<UVehicleModuleDataAsset>(
		nullptr, VehicleModuleTestConstants::VehicleModuleDataAssetPath);
	if (not TestNotNull(TEXT("Vehicle module data asset loads"), ModuleDataAsset))
	{
		return false;
	}
	if (not TestNotNull(TEXT("Light tanks have an engine-fire behaviour"),
		ModuleDataAsset->LightTankEngineFire.Get()))
	{
		return false;
	}

	UTankEngineFireBehaviour* EngineFireBehaviour = NewObject<UTankEngineFireBehaviour>(
		GetTransientPackage(), ModuleDataAsset->LightTankEngineFire);
	if (not TestNotNull(TEXT("Light-tank engine-fire behaviour can be instantiated"), EngineFireBehaviour))
	{
		return false;
	}

	const FTankEngineFireAttachmentRules& AttachmentRules =
		FTankEngineFireBehaviourTestAccess::GetAttachmentRules(*EngineFireBehaviour);
	TestEqual(TEXT("Light-tank fire uses a hull socket"), AttachmentRules.Mode,
		ETankEngineFireAttachmentMode::HullSocket);
	TestEqual(TEXT("Light-tank fire uses the EngineFire socket"), AttachmentRules.HullSocketName,
		FName(TEXT("EngineFire")));

	UStaticMesh* PanzerHullMesh = LoadObject<UStaticMesh>(
		nullptr, VehicleModuleTestConstants::Panzer38THullMeshPath);
	if (not TestNotNull(TEXT("Panzer 38(t) hull mesh loads"), PanzerHullMesh))
	{
		return false;
	}
	USkeletalMesh* PanzerChassisMesh = LoadObject<USkeletalMesh>(
		nullptr, VehicleModuleTestConstants::Panzer38TChassisMeshPath);
	if (not TestNotNull(TEXT("Panzer 38(t) chassis mesh loads"), PanzerChassisMesh))
	{
		return false;
	}

	VehicleModuleTests::FTankFixture Fixture;
	USkeletalMeshComponent* ChassisComponent = NewObject<USkeletalMeshComponent>(Fixture.Tank);
	ChassisComponent->SetSkeletalMeshAsset(PanzerChassisMesh);
	Fixture.Tank->AddInstanceComponent(ChassisComponent);
	UStaticMeshComponent* StaticHullComponent = NewObject<UStaticMeshComponent>(Fixture.Tank);
	StaticHullComponent->SetStaticMesh(PanzerHullMesh);
	Fixture.Tank->AddInstanceComponent(StaticHullComponent);
	FTankEngineFireBehaviourTestAccess::SetTankMaster(*EngineFireBehaviour, *Fixture.Tank);

	TestFalse(TEXT("The skeletal chassis selected by GetTankMesh does not own EngineFire"),
		ChassisComponent->DoesSocketExist(AttachmentRules.HullSocketName));
	TestTrue(TEXT("The Panzer 38(t) static hull owns the configured socket"),
		StaticHullComponent->DoesSocketExist(AttachmentRules.HullSocketName));
	TestTrue(TEXT("Engine fire resolves the mesh that actually owns the socket"),
		FTankEngineFireBehaviourTestAccess::FindTankMeshWithSocket(
			*EngineFireBehaviour, AttachmentRules.HullSocketName) == StaticHullComponent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleAmmoCookOffChanceTest, "RTS.VehicleModules.AmmoCookOffChance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleAmmoCookOffChanceTest::RunTest(const FString& Parameters)
{
	using VehicleModuleBalance::AmmoCookOff::GetChance;
	TestEqual(TEXT("Damaged ammo cook-off chance"), GetChance(EVehicleModuleState::Damaged), 0.1f);
	TestEqual(TEXT("Destroyed ammo cook-off chance"), GetChance(EVehicleModuleState::Destroyed), 0.2f);
	TestEqual(TEXT("Healthy ammo cannot cook off"), GetChance(EVehicleModuleState::Healthy), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleAmmoCookOffProfileTest, "RTS.VehicleModules.AmmoCookOffProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleAmmoCookOffProfileTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UVehicleModuleSubsystem* Subsystem = NewObject<UVehicleModuleSubsystem>(GameInstance);
	UVehicleModuleDataAsset* DataAsset = NewObject<UVehicleModuleDataAsset>(Subsystem);
	FVehicleModuleTestAccess::SetModuleDataAsset(*Subsystem, DataAsset);
	const TSubclassOf<UAmmoCookOffBehaviour> CookOffClass = UAmmoCookOffBehaviour::StaticClass();

	DataAsset->ArmoredCarAmmoCookOff = CookOffClass;
	TestTrue(TEXT("Armored car selects its cook-off"),
		Subsystem->GetAmmoCookOffBehaviourClass(EVehicleModuleProfile::ArmoredCar) == CookOffClass);
	DataAsset->ArmoredCarAmmoCookOff = nullptr;
	DataAsset->LightTankAmmoCookOff = CookOffClass;
	TestTrue(TEXT("Light tank selects its cook-off"),
		Subsystem->GetAmmoCookOffBehaviourClass(EVehicleModuleProfile::LightTank) == CookOffClass);
	DataAsset->LightTankAmmoCookOff = nullptr;
	DataAsset->MediumTankAmmoCookOff = CookOffClass;
	TestTrue(TEXT("Medium tank selects its cook-off"),
		Subsystem->GetAmmoCookOffBehaviourClass(EVehicleModuleProfile::MediumTank) == CookOffClass);
	DataAsset->MediumTankAmmoCookOff = nullptr;
	DataAsset->HeavyTankAmmoCookOff = CookOffClass;
	TestTrue(TEXT("Heavy tank selects its cook-off"),
		Subsystem->GetAmmoCookOffBehaviourClass(EVehicleModuleProfile::HeavyTank) == CookOffClass);
	TestTrue(TEXT("Super-heavy tank shares heavy cook-off"),
		Subsystem->GetAmmoCookOffBehaviourClass(EVehicleModuleProfile::SuperHeavyTank) == CookOffClass);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleChangeShellAttributionTest,
	"RTS.VehicleModules.ChangeShellAttribution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleChangeShellAttributionTest::RunTest(const FString& Parameters)
{
	FModuleStateChange FirstChange;
	FirstChange.SlotIndex = VehicleModuleBalance::SlotOffset::Engine;
	FirstChange.PreviousState = EVehicleModuleState::Healthy;
	FirstChange.NewState = EVehicleModuleState::Damaged;
	FirstChange.DamageShellType = EWeaponShellType::Shell_AP;

	FModuleChangeBatch Changes;
	Changes.AddOrMerge(FirstChange);

	FModuleStateChange LatestChange = FirstChange;
	LatestChange.PreviousState = EVehicleModuleState::Damaged;
	LatestChange.NewState = EVehicleModuleState::Destroyed;
	LatestChange.DamageShellType = EWeaponShellType::Shell_HE;
	Changes.AddOrMerge(LatestChange);

	TestEqual(TEXT("Repeated transitions retain one fixed-storage record"), Changes.Count, 1);
	TestEqual(TEXT("The first observed state remains the batch baseline"), Changes.Changes[0].PreviousState,
	          EVehicleModuleState::Healthy);
	TestEqual(TEXT("The final state comes from the latest transition"), Changes.Changes[0].NewState,
	          EVehicleModuleState::Destroyed);
	TestEqual(TEXT("The shell comes from the transition that produced the final state"),
	          Changes.Changes[0].DamageShellType, EWeaponShellType::Shell_HE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleFrontPlateModuleRulesTest, "RTS.VehicleModules.FrontPlateModuleRules",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleFrontPlateModuleRulesTest::RunTest(const FString& Parameters)
{
	using namespace VehicleModuleBalance;
	constexpr int32 FirstInternalCandidateIndex = AddOnArmorCandidateIndex + 1;
	constexpr int32 SecondInternalCandidateIndex = FirstInternalCandidateIndex + 1;
	constexpr EVehicleModuleProfile Profile = EVehicleModuleProfile::LightTank;
	const FPlateModuleRuleSet* Rules = GetProfilePlateRules(Profile, EVehicleRunningGear::Tracks);
	const FPlateModuleRuleSet& Front = Rules[TryGetPlateRuleIndex(EArmorPlate::Plate_Front)];
	const FPlateModuleRuleSet& Upper = Rules[TryGetPlateRuleIndex(EArmorPlate::Plate_FrontUpperGlacis)];
	const FPlateModuleRuleSet& Lower = Rules[TryGetPlateRuleIndex(EArmorPlate::Plate_FrontLowerGlacis)];
	const FPlateModuleDamage& FrontEngine = Front.Entries[FirstInternalCandidateIndex];
	const FPlateModuleDamage& FrontAmmo = Front.Entries[SecondInternalCandidateIndex];
	const FPlateModuleDamage& UpperAmmo = Upper.Entries[FirstInternalCandidateIndex];
	const FPlateModuleDamage& UpperEngine = Upper.Entries[SecondInternalCandidateIndex];
	const FPlateModuleDamage& LowerEngine = Lower.Entries[FirstInternalCandidateIndex];
	const FPlateModuleDamage& LowerAmmo = Lower.Entries[SecondInternalCandidateIndex];
	const float EngineChanceMultiplier = GetDamageChanceMultiplier(Profile, EVehicleModuleTypes::Engine);
	constexpr float GuaranteedProbability01 = 1.f;

	TestEqual(TEXT("Front targets engine"), FrontEngine.TypeToDamage, EVehicleModuleTypes::Engine);
	TestEqual(TEXT("Front can hit ammo"), FrontAmmo.TypeToDamage, EVehicleModuleTypes::Ammo);
	TestEqual(TEXT("Upper front can hit ammo"), UpperAmmo.TypeToDamage, EVehicleModuleTypes::Ammo);
	TestEqual(TEXT("Upper front targets engine"), UpperEngine.TypeToDamage, EVehicleModuleTypes::Engine);
	TestEqual(TEXT("Lower front targets engine"), LowerEngine.TypeToDamage, EVehicleModuleTypes::Engine);
	TestEqual(TEXT("Lower front can hit ammo"), LowerAmmo.TypeToDamage, EVehicleModuleTypes::Ammo);
	TestEqual(TEXT("Front engine uses singleton routing"), FrontEngine.TargetSelector, EModuleTargetSelector::Singleton);
	TestEqual(TEXT("Upper engine uses singleton routing"), UpperEngine.TargetSelector, EModuleTargetSelector::Singleton);
	TestEqual(TEXT("Lower engine uses singleton routing"), LowerEngine.TargetSelector, EModuleTargetSelector::Singleton);
	TestEqual(TEXT("Light-tank front engine chance is guaranteed"), FrontEngine.DamageProbability, GuaranteedProbability01,
	          VehicleModuleTestConstants::Tolerance);
	TestEqual(TEXT("Front engine chance uses its named probability"), FrontEngine.DamageProbability,
	          ClampRuleValue01(RuleSetProbabilities::Plate_Front::Engine * EngineChanceMultiplier),
	          VehicleModuleTestConstants::Tolerance);
	TestEqual(TEXT("Upper engine chance uses its named probability"), UpperEngine.DamageProbability,
	          ClampRuleValue01(RuleSetProbabilities::Plate_FrontUpperGlacis::Engine * EngineChanceMultiplier),
	          VehicleModuleTestConstants::Tolerance);
	TestEqual(TEXT("Lower engine chance uses its named probability"), LowerEngine.DamageProbability,
	          ClampRuleValue01(RuleSetProbabilities::Plate_FrontLowerGlacis::Engine * EngineChanceMultiplier),
	          VehicleModuleTestConstants::Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleFrontEngineDamageTest, "RTS.VehicleModules.FrontEngineDamage",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleFrontEngineDamageTest::RunTest(const FString& Parameters)
{
	using namespace VehicleModuleTestConstants;
	VehicleModuleTests::FTankFixture Fixture;
	TestTrue(TEXT("Light tank profile selected"),
	         Fixture.Armor->SetVehicleModuleProfile(EVehicleModuleProfile::LightTank));
	Fixture.RegisterHullPlate(EArmorPlate::Plate_Front);
	TestTrue(TEXT("Engine installed"), Fixture.Install(EngineModuleId, EVehicleModuleTypes::Engine));
	Fixture.Finalize();
	TestTrue(TEXT("Module setup finalized"), Fixture.Armor->GetAreModulesFinalized());

	const float EngineMaxHp = Fixture.Armor->GetModuleSnapshot(EngineModuleId).MaxHp;
	constexpr float ProjectileBaseDamage = 400.f;
	constexpr float EffectiveArmor = 20.f;
	constexpr float ProjectileCalibreMm = 75.f;
	Fixture.Armor->CalculateModuleDamage(EArmorPlate::Plate_Front, EffectiveArmor, true,
	                                     ProjectileBaseDamage, ProjectileBaseDamage, ProjectileCalibreMm);
	TestTrue(TEXT("Penetrating front hit damages the only installed engine"),
	         Fixture.Armor->GetModuleSnapshot(EngineModuleId).CurrentHp < EngineMaxHp);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleDamagedCrewRepairTest, "RTS.VehicleModules.CrewRepair.Damaged",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleDamagedCrewRepairTest::RunTest(const FString& Parameters)
{
	using namespace VehicleModuleTestConstants;
	constexpr float LowHullHealth = 200.f;
	constexpr float DamagingChip = 1.f;
	constexpr int32 ReservedIndex = VehicleModuleBalance::CrewRepairAbilitySlotIndex;
	VehicleModuleTests::FTankFixture Fixture;
	Fixture.Install(AmmoModuleId, EVehicleModuleTypes::Ammo);
	Fixture.Finalize();
	UHealthComponent* Health = Fixture.GetHealth();
	Health->SetCurrentHealth(LowHullHealth);
	const TArray<FUnitAbilityEntry>& Card = FVehicleModuleTestAccess::GetCard(*Fixture.Tank);

	Fixture.Armor->DamageModule(AmmoModuleId, DamagingChip);
	TestEqual(TEXT("A yellow module inserts CrewRepair in the final slot"), Card[ReservedIndex].AbilityId,
	          EAbilityID::IdCrewRepair);
	TestEqual(TEXT("A yellow module needs one crew stage"), Fixture.Tank->GetRemainingCrewRepairSeconds(),
	          GetCrewRepairSeconds(EVehicleModuleTypes::Ammo), Tolerance);
	FVehicleModuleTestAccess::ExecuteCrewRepair(*Fixture.Tank, ECrewRepairAbilityType::EnableRepair);
	FVehicleModuleTestAccess::AdvanceCrewRepair(*Fixture.Tank,
	                                           GetCrewRepairSeconds(EVehicleModuleTypes::Ammo) - CrewRepairTickSeconds);
	TestEqual(TEXT("The damaged module needs its full duration"), Fixture.GetState(AmmoModuleId),
	          EVehicleModuleState::Damaged);
	FVehicleModuleTestAccess::AdvanceCrewRepair(*Fixture.Tank, CrewRepairTickSeconds);
	TestEqual(TEXT("CrewRepair restores a damaged module to healthy"), Fixture.GetState(AmmoModuleId),
	          EVehicleModuleState::Healthy);
	TestFalse(TEXT("Repair stops after the final yellow module is healthy"), Fixture.Tank->GetIsCrewRepairActive());
	TestEqual(TEXT("The entry is removed when every module is healthy"), Card[ReservedIndex].AbilityId,
	          EAbilityID::IdNoAbility);
	TestEqual(TEXT("Repairing a yellow module never heals the hull"), Health->GetCurrentHealth(), LowHullHealth,
	          Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleCrewRepairTurretRangeTest,
	"RTS.VehicleModules.CrewRepair.RestoresTurretMaximumRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleCrewRepairTurretRangeTest::RunTest(const FString& Parameters)
{
	using namespace VehicleModuleTestConstants;
	constexpr int32 MachineGunIndex = 0;
	constexpr int32 MainGunIndex = 1;
	constexpr float MachineGunCalibre = 7.92f;
	constexpr float MainGunCalibre = 75.f;
	constexpr float MachineGunRange = 3200.f;
	constexpr float MainGunRange = 4200.f;
	constexpr float DamagingChip = 1.f;

	VehicleModuleTests::FTankFixture Fixture;
	Fixture.Install(AmmoModuleId, EVehicleModuleTypes::Ammo);
	Fixture.Finalize();
	ACPPTurretsMaster* Turret = FVehicleModuleTestAccess::SpawnTurretWithWeapons(
		*Fixture.World, {MachineGunCalibre, MainGunCalibre});
	FVehicleModuleTestAccess::SetTurretWeaponRange(*Turret, MachineGunIndex, MachineGunRange);
	FVehicleModuleTestAccess::SetTurretWeaponRange(*Turret, MainGunIndex, MainGunRange);
	FVehicleModuleTestAccess::AddTankTurret(*Fixture.Tank, *Turret);

	Fixture.Armor->DamageModule(AmmoModuleId, DamagingChip);
	FVehicleModuleTestAccess::ExecuteCrewRepair(*Fixture.Tank, ECrewRepairAbilityType::EnableRepair);
	// A stale cache formerly survived the disable/enable cycle and could leave the MG as the engagement range.
	FVehicleModuleTestAccess::ForceTurretCachedRange(*Turret, MachineGunRange);
	FVehicleModuleTestAccess::AdvanceCrewRepair(*Fixture.Tank, GetCrewRepairSeconds(EVehicleModuleTypes::Ammo));

	TestFalse(TEXT("CrewRepair completes after the final module"), Fixture.Tank->GetIsCrewRepairActive());
	TestEqual(TEXT("Re-enabled turret rebuilds its range from the longest-ranged gun"),
	          Turret->GetMaxWeaponRange(), MainGunRange, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleDestroyedCrewRepairTest, "RTS.VehicleModules.CrewRepair.Destroyed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleDestroyedCrewRepairTest::RunTest(const FString& Parameters)
{
	using namespace VehicleModuleTestConstants;
	constexpr float LowHullHealth = 200.f;
	constexpr int32 DestroyedRepairStageCount = 2;
	constexpr int32 ReservedIndex = VehicleModuleBalance::CrewRepairAbilitySlotIndex;
	VehicleModuleTests::FTankFixture Fixture;
	FUnitAbilityEntry MoveEntry;
	MoveEntry.AbilityId = EAbilityID::IdMove;
	Fixture.Tank->GetIsValidCommandData()->SetAbilities({MoveEntry});
	Fixture.Install(EngineModuleId, EVehicleModuleTypes::Engine);
	Fixture.Install(AmmoModuleId, EVehicleModuleTypes::Ammo);
	Fixture.Finalize();
	UHealthComponent* Health = Fixture.GetHealth();
	Health->SetCurrentHealth(LowHullHealth);
	const TArray<FUnitAbilityEntry>& Card = FVehicleModuleTestAccess::GetCard(*Fixture.Tank);

	Fixture.Armor->DamageModule(EngineModuleId, DestroyingDamage);
	Fixture.Armor->DamageModule(AmmoModuleId, DestroyingDamage);
	TestEqual(TEXT("Red modules insert CrewRepair in the final slot"), Card[ReservedIndex].AbilityId,
	          EAbilityID::IdCrewRepair);
	const float ExpectedDestroyedRepairSeconds = DestroyedRepairStageCount * (
		GetCrewRepairSeconds(EVehicleModuleTypes::Engine) + GetCrewRepairSeconds(EVehicleModuleTypes::Ammo));
	TestEqual(TEXT("Each destroyed module needs a red and yellow crew stage"),
	          Fixture.Tank->GetRemainingCrewRepairSeconds(), ExpectedDestroyedRepairSeconds, Tolerance);
	TestEqual(TEXT("The inactive entry is EnableRepair"), Card[ReservedIndex].CustomType,
	          static_cast<int32>(ECrewRepairAbilityType::EnableRepair));

	FVehicleModuleTestAccess::ExecuteCrewRepair(*Fixture.Tank, ECrewRepairAbilityType::EnableRepair);
	TestTrue(TEXT("CrewRepair starts below the ordinary hull gate"), Fixture.Tank->GetIsCrewRepairActive());
	TestEqual(TEXT("The active entry is DisableRepair"), Card[ReservedIndex].CustomType,
	          static_cast<int32>(ECrewRepairAbilityType::DisableRepair));
	TestEqual(TEXT("Move is hidden while repairing"), Card[0].AbilityId, EAbilityID::IdNoAbility);

	FVehicleModuleTestAccess::AdvanceCrewRepair(
		*Fixture.Tank, GetCrewRepairSeconds(EVehicleModuleTypes::Engine) - CrewRepairTickSeconds);
	TestEqual(TEXT("The engine needs its full duration"), Fixture.GetState(EngineModuleId),
	          EVehicleModuleState::Destroyed);
	FVehicleModuleTestAccess::AdvanceCrewRepair(*Fixture.Tank, CrewRepairTickSeconds);
	TestEqual(TEXT("The engine advances from destroyed to damaged first"), Fixture.GetState(EngineModuleId),
	          EVehicleModuleState::Damaged);
	TestTrue(TEXT("CrewRepair continues until yellow modules are healthy"), Fixture.Tank->GetIsCrewRepairActive());
	TestEqual(TEXT("CrewRepair never heals the hull"), Health->GetCurrentHealth(), LowHullHealth, Tolerance);

	FVehicleModuleTestAccess::ExecuteCrewRepair(*Fixture.Tank, ECrewRepairAbilityType::DisableRepair);
	TestFalse(TEXT("DisableRepair stops immediately"), Fixture.Tank->GetIsCrewRepairActive());
	TestEqual(TEXT("Move is restored"), Card[0].AbilityId, EAbilityID::IdMove);
	TestEqual(TEXT("Remaining nonhealthy modules keep EnableRepair"), Card[ReservedIndex].CustomType,
	          static_cast<int32>(ECrewRepairAbilityType::EnableRepair));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleCrewRepairExternalHealingTest,
	"RTS.VehicleModules.CrewRepair.ExternalHealing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleCrewRepairExternalHealingTest::RunTest(const FString& Parameters)
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
	Health->SetCurrentHealth(LowHullHealth);
	const TArray<FUnitAbilityEntry>& Card = FVehicleModuleTestAccess::GetCard(*Fixture.Tank);
	Fixture.Armor->DamageModule(EngineModuleId, DestroyingDamage);
	Fixture.Armor->DamageModule(AmmoModuleId, DestroyingDamage);

	FVehicleModuleTestAccess::ExecuteCrewRepair(*Fixture.Tank, ECrewRepairAbilityType::EnableRepair);
	const float HealingToRecoveryGate = TankMaxHealth * TankHealthRequiredForModuleRecovery01 - LowHullHealth;
	Health->Heal(HealingToRecoveryGate);
	TestTrue(TEXT("External red recovery leaves CrewRepair active for yellow modules"),
	         Fixture.Tank->GetIsCrewRepairActive());
	TestEqual(TEXT("Yellow modules keep DisableRepair while active"), Card[ReservedIndex].CustomType,
	          static_cast<int32>(ECrewRepairAbilityType::DisableRepair));
	Health->Heal(TankMaxHealth);
	TestFalse(TEXT("External full service stops the crew"), Fixture.Tank->GetIsCrewRepairActive());
	TestEqual(TEXT("No nonhealthy modules remain, so the entry is removed"), Card[ReservedIndex].AbilityId,
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

// ----------------------------------------------------------------------------------------------------
// Turret-driven module registration
// ----------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleInitialTurretFinalizationTest,
	"RTS.VehicleModules.InitialTurretFinalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleInitialTurretFinalizationTest::RunTest(const FString& Parameters)
{
	using namespace VehicleModuleTestConstants;
	constexpr float MainGunCalibre = 75.f;
	constexpr float SecondaryGunCalibre = 37.f;
	constexpr int32 FinalInstalledModuleCount = 4;
	VehicleModuleTests::FTankFixture Fixture;
	TestTrue(TEXT("Engine is installed by the tank Blueprint"),
		Fixture.Install(EngineModuleId, EVehicleModuleTypes::Engine));
	ACPPTurretsMaster* FirstTurret = FVehicleModuleTestAccess::SpawnTurretWithWeapons(*Fixture.World, {MainGunCalibre});
	ACPPTurretsMaster* SecondTurret = FVehicleModuleTestAccess::SpawnTurretWithWeapons(*Fixture.World, {SecondaryGunCalibre});
	Fixture.RegisterMeshPlate(FirstTurret->GetModuleBindingMesh(), EArmorPlate::Turret_Front);
	Fixture.RegisterMeshPlate(SecondTurret->GetModuleBindingMesh(), EArmorPlate::Turret_Front);
	FVehicleModuleTestAccess::AddPendingTurret(*Fixture.Tank, FirstTurret);
	FVehicleModuleTestAccess::AddPendingTurret(*Fixture.Tank, SecondTurret);
	FVehicleModuleTestAccess::SignalTankBlueprintModulesComplete(*Fixture.Tank, *Fixture.Armor);
	TestFalse(TEXT("Blueprint completion waits for both turrets"),
		FVehicleModuleTestAccess::GetAreTankModulesInitialized(*Fixture.Tank));
	FVehicleModuleTestAccess::MarkTurretReady(*Fixture.Tank, SecondTurret);
	TestFalse(TEXT("One ready turret does not finalize the tank"),
		FVehicleModuleTestAccess::GetAreTankModulesInitialized(*Fixture.Tank));
	FVehicleModuleTestAccess::MarkTurretReady(*Fixture.Tank, FirstTurret);
	TestTrue(TEXT("Last turret readiness finalizes the tank"),
		FVehicleModuleTestAccess::GetAreTankModulesInitialized(*Fixture.Tank));
	TestEqual(TEXT("Tank and turrets are all installed before finalization"),
		Fixture.Armor->GetInstalledModuleCount(), FinalInstalledModuleCount);
	TestTrue(TEXT("Turret HP is derived during finalization"),
		Fixture.Armor->GetModuleSnapshot(VehicleModuleBalance::SlotOffset::Turret).MaxHp > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleEarlyTurretReadinessTest,
	"RTS.VehicleModules.EarlyTurretReadiness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleEarlyTurretReadinessTest::RunTest(const FString& Parameters)
{
	constexpr float MainGunCalibre = 75.f;
	VehicleModuleTests::FTankFixture EarlyReadyFixture;
	ACPPTurretsMaster* EarlyTurret = FVehicleModuleTestAccess::SpawnTurretWithWeapons(
		*EarlyReadyFixture.World, {MainGunCalibre});
	EarlyReadyFixture.RegisterMeshPlate(EarlyTurret->GetModuleBindingMesh(), EArmorPlate::Turret_Front);
	FVehicleModuleTestAccess::AddPendingTurret(*EarlyReadyFixture.Tank, EarlyTurret);
	FVehicleModuleTestAccess::MarkTurretReady(*EarlyReadyFixture.Tank, EarlyTurret);
	TestEqual(TEXT("A ready turret waits for Blueprint completion"),
		EarlyReadyFixture.Armor->GetInstalledModuleCount(), 0);
	FVehicleModuleTestAccess::SignalTankBlueprintModulesComplete(
		*EarlyReadyFixture.Tank, *EarlyReadyFixture.Armor);
	TestTrue(TEXT("Early readiness is retained and turret-only modules finalize"),
		FVehicleModuleTestAccess::GetAreTankModulesInitialized(*EarlyReadyFixture.Tank));
	TestTrue(TEXT("The early turret has initialized HP"),
		EarlyReadyFixture.Armor->GetModuleSnapshot(VehicleModuleBalance::SlotOffset::Turret).MaxHp > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleModuleTurretSelfRegistrationTest, "RTS.VehicleModules.TurretSelfRegistration",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleModuleTurretSelfRegistrationTest::RunTest(const FString& Parameters)
{
	using namespace VehicleModuleTestConstants;
	constexpr int32 FirstTurretModuleId = VehicleModuleBalance::SlotOffset::Turret;
	constexpr int32 SecondTurretModuleId = FirstTurretModuleId + 1;
	constexpr int32 WeaponModuleId = VehicleModuleBalance::SlotOffset::Weapon;
	constexpr int32 ModulesAfterSpawn = 4;
	constexpr int32 MainGunIndex = 1;
	constexpr float MachineGunCalibre = 7.92f;
	constexpr float MainGunCalibre = 75.f;
	constexpr float SecondaryGunCalibre = 37.f;
	constexpr float LargeGunCalibre = 88.f;
	constexpr float TurretChipDamage = 1.f;

	VehicleModuleTests::FTankFixture Fixture;
	TestTrue(TEXT("Engine opts the tank into modules"), Fixture.Install(EngineModuleId, EVehicleModuleTypes::Engine));
	Fixture.Finalize();
	ATankMaster& Tank = *Fixture.Tank;
	UArmorCalculation& Armor = *Fixture.Armor;

	ACPPTurretsMaster* MainTurret = FVehicleModuleTestAccess::SpawnTurretWithWeapons(
		*Fixture.World, {MachineGunCalibre, MainGunCalibre, MainGunCalibre});
	ACPPTurretsMaster* SecondaryTurret = FVehicleModuleTestAccess::SpawnTurretWithWeapons(
		*Fixture.World, {SecondaryGunCalibre});
	ACPPTurretsMaster* UnarmoredTurret = FVehicleModuleTestAccess::SpawnTurretWithWeapons(
		*Fixture.World, {LargeGunCalibre});
	UMeshComponent* MainMesh = MainTurret->GetModuleBindingMesh();
	UMeshComponent* SecondaryMesh = SecondaryTurret->GetModuleBindingMesh();
	Fixture.RegisterMeshPlate(MainMesh, EArmorPlate::Turret_Front);
	Fixture.RegisterMeshPlate(SecondaryMesh, EArmorPlate::Turret_Front);

	for (ACPPTurretsMaster* Turret : {MainTurret, SecondaryTurret, UnarmoredTurret})
	{
		FVehicleModuleTestAccess::AddPendingTurret(Tank, Turret);
	}
	// Reverse timer order on purpose: slot IDs must follow mount order, not readiness order.
	FVehicleModuleTestAccess::MarkTurretReady(Tank, UnarmoredTurret);
	FVehicleModuleTestAccess::MarkTurretReady(Tank, SecondaryTurret);
	TestEqual(TEXT("Nothing installs while a turret is still pending"), Armor.GetInstalledModuleCount(), 1);
	FVehicleModuleTestAccess::MarkTurretReady(Tank, MainTurret);

	TestEqual(TEXT("First mount gets the first turret slot"),
	          Armor.FindModuleIdBoundToMesh(EVehicleModuleTypes::Turret, MainMesh), FirstTurretModuleId);
	TestEqual(TEXT("Second mount gets the next turret slot"),
	          Armor.FindModuleIdBoundToMesh(EVehicleModuleTypes::Turret, SecondaryMesh), SecondTurretModuleId);
	TestEqual(TEXT("A mount without registered armor stays module-less"),
	          Armor.FindModuleIdBoundToMesh(EVehicleModuleTypes::Turret, UnarmoredTurret->GetModuleBindingMesh()),
	          static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("The tank gets exactly one weapon module"),
	          Armor.FindFirstInstalledModuleIdOfType(EVehicleModuleTypes::Weapon), WeaponModuleId);
	TestTrue(TEXT("The weapon module sits on the largest armored gun's mount"),
	         Armor.GetBoundMeshForSlot(WeaponModuleId) == MainMesh);
	TestTrue(TEXT("Equal calibres keep the first weapon in array order"),
	         Armor.GetBoundWeaponForSlot(WeaponModuleId).Get()
	         == FVehicleModuleTestAccess::GetTurretWeapon(*MainTurret, MainGunIndex));
	TestEqual(TEXT("Engine, two turrets and one weapon are installed"), Armor.GetInstalledModuleCount(),
	          ModulesAfterSpawn);

	FVehicleModuleTestAccess::AddPendingTurret(Tank, MainTurret);
	FVehicleModuleTestAccess::MarkTurretReady(Tank, MainTurret);
	TestEqual(TEXT("Registering a turret again installs nothing"), Armor.GetInstalledModuleCount(), ModulesAfterSpawn);

	Armor.DamageModule(FirstTurretModuleId, TurretChipDamage);
	const FVehicleModuleSaveData SaveData = Tank.ExportVehicleModuleSaveData();
	Armor.RestoreAllModulesToHealthy();
	FVehicleModuleTestAccess::AddPendingTurret(Tank, SecondaryTurret);
	TestTrue(TEXT("A load during pending turret registration is accepted"), Tank.ImportVehicleModuleSaveData(SaveData));
	TestEqual(TEXT("The load waits for the pending turret"), Fixture.GetState(FirstTurretModuleId),
	          EVehicleModuleState::Healthy);
	FVehicleModuleTestAccess::MarkTurretReady(Tank, SecondaryTurret);
	TestEqual(TEXT("The deferred load applies once the turrets registered"), Fixture.GetState(FirstTurretModuleId),
	          EVehicleModuleState::Damaged);

	FVehicleModuleTestAccess::CancelTurret(Tank, MainTurret);
	TestTrue(TEXT("A leaving turret hands the weapon module to the largest remaining gun"),
	         Armor.GetBoundWeaponForSlot(WeaponModuleId).Get()
	         == FVehicleModuleTestAccess::GetTurretWeapon(*SecondaryTurret, 0));

	// Swap: the old mount mesh is destroyed and the new turret registers its own mesh.
	MainMesh->MarkAsGarbage();
	ACPPTurretsMaster* SwappedTurret = FVehicleModuleTestAccess::SpawnTurretWithWeapons(
		*Fixture.World, {LargeGunCalibre});
	UMeshComponent* SwappedMesh = SwappedTurret->GetModuleBindingMesh();
	Fixture.RegisterMeshPlate(SwappedMesh, EArmorPlate::Turret_Front);
	FVehicleModuleTestAccess::AddPendingTurret(Tank, SwappedTurret);
	FVehicleModuleTestAccess::MarkTurretReady(Tank, SwappedTurret);

	TestEqual(TEXT("The swapped turret takes over the orphaned mount module"),
	          Armor.FindModuleIdBoundToMesh(EVehicleModuleTypes::Turret, SwappedMesh), FirstTurretModuleId);
	TestEqual(TEXT("The mount module keeps its damage state"), Fixture.GetState(FirstTurretModuleId),
	          EVehicleModuleState::Damaged);
	TestTrue(TEXT("A larger swapped-in gun takes the weapon module"),
	         Armor.GetBoundWeaponForSlot(WeaponModuleId).Get()
	         == FVehicleModuleTestAccess::GetTurretWeapon(*SwappedTurret, 0));
	TestEqual(TEXT("A swap installs no extra modules"), Armor.GetInstalledModuleCount(), ModulesAfterSpawn);
	return true;
}

#endif
