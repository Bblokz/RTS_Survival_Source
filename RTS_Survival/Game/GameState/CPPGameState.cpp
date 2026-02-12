// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "CPPGameState.h"

#include "AI/NavigationSystemBase.h"
// used in distance calculation


#include "GameResourceManager/GameResourceManager.h"
#include "GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptionLibrary/TrainingOptionLibrary.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/UnitData/BuildingExpansionData.h"
#include "RTS_Survival/UnitData/NomadicVehicleData.h"
#include "RTS_Survival/UnitData/SquadData.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
// To setup experience levels for units.
#include "BxpDataHelpers/BxpConstructionRulesHelpers.h"
#include "BxpDataHelpers/BxpDataHelpers.h"
#include "GameDecalManager/GameDecalManager.h"
#include "GameExplosionManager/ExplosionManager.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/AttachedRocketsData/AttachedRocketsData.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInType/DigInType.h"
#include "RTS_Survival/UnitData/AircraftData.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/Utils/RTS_Statics/SubSystems/DecalManagerSubsystem/DecalManagerSubsystem.h"
#include "RTS_Survival/Utils/RTS_Statics/SubSystems/ExplosionManagerSubsystem/ExplosionManagerSubsystem.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "UnitDataHelpers/UnitDataHelpers.h"


/*--------------------------------- BUILDINGS ---------------------------------*/
ACPPGameState::ACPPGameState()
	: M_GameTimeDays(0),
	  M_GameTimeHours(0),
	  M_GameTimeMinutes(0),
	  M_GameTimeSeconds(0),
	  bM_IsClockRunning(false)
{
	// Init default fallback weapon.
	DefaultWeapon.BaseDamage = 7 * DeveloperSettings::GameBalance::Weapons::DamagePerMM;
	DefaultWeapon.DamageFlux = 10;
	DefaultWeapon.Range = 7000;
	DefaultWeapon.ArmorPen = 12;
	DefaultWeapon.MagCapacity = 80;
	DefaultWeapon.ReloadSpeed = 7;
	DefaultWeapon.BaseCooldown = 1.5;
	DefaultWeapon.CooldownFlux = 10;
	DefaultWeapon.Accuracy = 80;
	DefaultWeapon.ShrapnelRange = 0;
	DefaultWeapon.ShrapnelDamage = 0;
	DefaultWeapon.ShrapnelParticles = 0;
	DefaultWeapon.ShrapnelPen = 0;

	InitAllGameWeaponData();
	InitAllGameTankData();
	InitAllGameAircraftData();
	InitAllGameNomadicData();
	InitAllGameSquadData();
	InitAllGameBxpData();
	InitAllGameDigInData();
	InitAllGameRocketData();
	M_RTSGameSettings = CreateDefaultSubobject<URTSGameSettingsHandler>(TEXT("GameUpdateComponent"));
	M_GameResourceManager = CreateDefaultSubobject<UGameResourceManager>(TEXT("GameResourceManager"));
	M_GameUnitManager = CreateDefaultSubobject<UGameUnitManager>(TEXT("GameUnitManager"));
	M_GameExplosionsManager = CreateDefaultSubobject<UGameExplosionsManager>(TEXT("GameExplosionsManager"));
	M_GameDecalManager = CreateDefaultSubobject<UGameDecalManager>(TEXT("GameDecalManager"));
}

void ACPPGameState::RegisterCallbackForSmallArmsProjectileMgr(
	const TFunction<void(const TObjectPtr<ASmallArmsProjectileManager>&)>& Callback,
	const TWeakObjectPtr<UObject> WeakCallbackOwner)
{
	M_ProjectileManagerCallbacks.CallbackOnProjectileMgrReady(Callback, WeakCallbackOwner);
}

void ACPPGameState::SetGameSetting(
	ERTSGameSetting Setting,
	bool bValue,
	float fValue,
	int iValue)
{
}


void ACPPGameState::StartClock()
{
	M_GameTimeSeconds = 0;
	M_GameTimeMinutes = 0;
	M_GameTimeHours = 0;
	M_GameTimeDays = 0;
	bM_IsClockRunning = true;
}

FWeaponData* ACPPGameState::GetWeaponDataOfPlayer(const int32 PlayerOwningWeapon,
                                                  const EWeaponName WeaponName)
{
	if (PlayerOwningWeapon == 1)
	{
		if (M_TPlayerWeaponDataHashMap.Contains(WeaponName))
		{
			return &M_TPlayerWeaponDataHashMap[WeaponName];
		}
		return nullptr;
	}
	if (M_TEnemyWeaponDataHashMap.Contains(WeaponName))
	{
		return &M_TEnemyWeaponDataHashMap[WeaponName];
	}
	return nullptr;
}

const FWeaponData* ACPPGameState::GetDefaultFallBackWeapon() const
{
	return &DefaultWeapon;
}

bool ACPPGameState::UpgradeWeaponDataForPlayer(const int32 PlayerOwningWeapon, const EWeaponName WeaponName,
                                               const FWeaponData& NewWeaponData)
{
	if (PlayerOwningWeapon == 1)
	{
		if (M_TPlayerWeaponDataHashMap.Contains(WeaponName))
		{
			M_TPlayerWeaponDataHashMap[WeaponName] = NewWeaponData;
			return true;
		}
		return false;
	}
	if (M_TEnemyWeaponDataHashMap.Contains(WeaponName))
	{
		M_TEnemyWeaponDataHashMap[WeaponName] = NewWeaponData;
		return true;
	}
	return false;
}

FTankData ACPPGameState::GetTankDataOfPlayer(const int32 PlayerOwningTank,
                                             const ETankSubtype TankSubtype) const
{
	if (PlayerOwningTank == 1)
	{
		if (M_TPlayerTankDataHashMap.Contains(TankSubtype))
		{
			return M_TPlayerTankDataHashMap[TankSubtype];
		}
		FString TankName = Global_GetTankSubtypeEnumAsString(TankSubtype);
		RTSFunctionLibrary::ReportError("Could not find tank data for player tank: " + TankName);
		return FTankData();
	}
	if (M_TEnemyTankDataHashMap.Contains(TankSubtype))
	{
		return M_TEnemyTankDataHashMap[TankSubtype];
	}
	FString TankName = Global_GetTankSubtypeEnumAsString(TankSubtype);
	RTSFunctionLibrary::ReportError("Could not find tank data for enemy tank: " + TankName);
	return FTankData();
}

bool ACPPGameState::UpgradeTankDataForPlayer(const int32 PlayerOwningTank, const ETankSubtype TankSubtype,
                                             const FTankData& NewTankData)
{
	if (PlayerOwningTank == 1)
	{
		if (M_TPlayerTankDataHashMap.Contains(TankSubtype))
		{
			M_TPlayerTankDataHashMap[TankSubtype] = NewTankData;
			return true;
		}
		return false;
	}
	if (M_TEnemyTankDataHashMap.Contains(TankSubtype))
	{
		M_TEnemyTankDataHashMap[TankSubtype] = NewTankData;
		return true;
	}
	return false;
}

FNomadicData ACPPGameState::GetNomadicDataOfPlayer(const int32 PlayerOwningNomadic,
                                                   const ENomadicSubtype NomadicSubtype) const
{
	if (PlayerOwningNomadic == 1)
	{
		if (M_TPlayerNomadicDataHashMap.Contains(NomadicSubtype))
		{
			return M_TPlayerNomadicDataHashMap[NomadicSubtype];
		}
		FString NomadicName = Global_GetNomadicSubtypeEnumAsString(NomadicSubtype);
		RTSFunctionLibrary::ReportError("Could not find nomadic data for player nomadic: " + NomadicName);
		return FNomadicData();
	}
	if (M_TEnemyNomadicDataHashMap.Contains(NomadicSubtype))
	{
		return M_TEnemyNomadicDataHashMap[NomadicSubtype];
	}
	FString NomadicName = Global_GetNomadicSubtypeEnumAsString(NomadicSubtype);
	RTSFunctionLibrary::ReportError("Could not find nomadic data for enemy nomadic: " + NomadicName);
	return FNomadicData();
}

FSquadData ACPPGameState::GetSquadDataOfPlayer(const int32 PlayerOwningSquad, const ESquadSubtype SquadSubtype) const
{
	if (PlayerOwningSquad == 1)
	{
		if (M_TPlayerSquadDataHashMap.Contains(SquadSubtype))
		{
			return M_TPlayerSquadDataHashMap[SquadSubtype];
		}
		const FString SquadName = Global_GetSquadSubtypeEnumAsString(SquadSubtype);
		RTSFunctionLibrary::ReportError("Could not find squad data for player squad: " + SquadName);
		return FSquadData();
	}
	if (M_TEnemySquadDataHashMap.Contains(SquadSubtype))
	{
		return M_TEnemySquadDataHashMap[SquadSubtype];
	}
	const FString SquadName = Global_GetSquadSubtypeEnumAsString(SquadSubtype);
	RTSFunctionLibrary::ReportError("Could not find squad data for enemy squad: " + SquadName);
	return FSquadData();
}

FDigInData ACPPGameState::GetDigInDataOfPlayer(const int32 PlayerOwningDigIn, const EDigInType DigInType) const
{
	if (PlayerOwningDigIn == 1)
	{
		if (M_TPlayerDigInDataHashMap.Contains(DigInType))
		{
			return M_TPlayerDigInDataHashMap[DigInType];
		}
		const FString DigInName = Global_GetDigInTypeEnumAsString(DigInType);
		RTSFunctionLibrary::ReportError("Could not find dig in data for player dig in: " + DigInName);
		return FDigInData();
	}
	if (M_TEnemyDigInDataHashMap.Contains(DigInType))
	{
		return M_TEnemyDigInDataHashMap[DigInType];
	}
	const FString DigInName = Global_GetDigInTypeEnumAsString(DigInType);
	RTSFunctionLibrary::ReportError("Could not find dig in data for enemy dig in: " + DigInName);
	return FDigInData();
}

FAttachedRocketsData ACPPGameState::GetAttachedRocketDataOfPlayer(const int32 PlayerOwningRocket,
                                                                  const ERocketAbility RocketType) const
{
	if (PlayerOwningRocket == 1)
	{
		if (M_TPlayerAttachedRocketDataHashMap.Contains(RocketType))
		{
			return M_TPlayerAttachedRocketDataHashMap[RocketType];
		}
		const FString RocketAbilityString = Global_RocketAbilityEnumString(RocketType);
		RTSFunctionLibrary::ReportError("Could not find AttachedRocket data for player with: " + RocketAbilityString);
		return FAttachedRocketsData();
	}
	if (M_EnemyAttachedRocketDataHashMap.Contains(RocketType))
	{
		return M_EnemyAttachedRocketDataHashMap[RocketType];
	}
	const FString RocketAbilityString = Global_RocketAbilityEnumString(RocketType);
	RTSFunctionLibrary::ReportError("Could not find AttachedRocket data for enemy with: " + RocketAbilityString);
	return FAttachedRocketsData();
}

FBxpData ACPPGameState::GetPlayerBxpData(const EBuildingExpansionType BxpType) const
{
	if (M_TPlayerBxpDataHashMap.Contains(BxpType))
	{
		return M_TPlayerBxpDataHashMap[BxpType];
	}
	RTSFunctionLibrary::ReportError(
		"Could not find bxp data for player bxp: " + Global_GetBxpTypeEnumAsString(BxpType));
	return FBxpData();
}

FAircraftData ACPPGameState::GetAircraftDataOfPlayer(const EAircraftSubtype AircraftSubtype,
                                                     const uint8 PlayerOwningAircraft) const
{
	if (PlayerOwningAircraft == 1)
	{
		if (M_TPlayerAircraftDataHashMap.Contains(AircraftSubtype))
		{
			return M_TPlayerAircraftDataHashMap[AircraftSubtype];
		}
		const FString AircraftName = Global_GetAircraftDisplayName(AircraftSubtype);
		RTSFunctionLibrary::ReportError("Could not find aircraft data for player aircraft: " + AircraftName);
		return FAircraftData();
	}
	if (M_TEnemyAircraftDataHashMap.Contains(AircraftSubtype))
	{
		return M_TEnemyAircraftDataHashMap[AircraftSubtype];
	}
	const FString AircraftName = Global_GetAircraftDisplayName(AircraftSubtype);
	RTSFunctionLibrary::ReportError("Could not find aircraft data for enemy aircraft: " + AircraftName);
	return FAircraftData();
}

FBxpData ACPPGameState::GetEnemyBxpData(const EBuildingExpansionType BxpType) const
{
	if (M_TEnemyBxpDataHashMap.Contains(BxpType))
	{
		return M_TEnemyBxpDataHashMap[BxpType];
	}
	RTSFunctionLibrary::ReportError(
		"Could not find bxp data for enemy bxp: " + Global_GetBxpTypeEnumAsString(BxpType));
	return FBxpData();
}

FTankData ACPPGameState::GetTankData(const ETankSubtype TankSubtype) const
{
	if (M_TPlayerTankDataHashMap.Contains(TankSubtype))
	{
		return M_TPlayerTankDataHashMap[TankSubtype];
	}
	return {};
}

bool ACPPGameState::UpgradeSquadDataForPlayer(const int32 PlayerOwningSquad, const ESquadSubtype SquadSubtype,
                                              const FSquadData& NewSquadData)
{
	if (PlayerOwningSquad == 1)
	{
		if (M_TPlayerSquadDataHashMap.Contains(SquadSubtype))
		{
			M_TPlayerSquadDataHashMap[SquadSubtype] = NewSquadData;
			return true;
		}
		return false;
	}
	if (M_TEnemySquadDataHashMap.Contains(SquadSubtype))
	{
		M_TEnemySquadDataHashMap[SquadSubtype] = NewSquadData;
		return true;
	}
	return false;
}

bool ACPPGameState::UpgradeNomadicDataForPlayer(const int32 PlayerOwningNomadic, const ENomadicSubtype NomadicSubtype,
                                                const FNomadicData& NewNomadicData)
{
	if (PlayerOwningNomadic == 1)
	{
		if (M_TPlayerNomadicDataHashMap.Contains(NomadicSubtype))
		{
			M_TPlayerNomadicDataHashMap[NomadicSubtype] = NewNomadicData;
			return true;
		}
		return false;
	}
	if (M_TEnemyNomadicDataHashMap.Contains(NomadicSubtype))
	{
		M_TEnemyNomadicDataHashMap[NomadicSubtype] = NewNomadicData;
		return true;
	}
	return false;
}

TArray<FUnitAbilityEntry> ACPPGameState::GetNomadicAbilities(const ENomadicSubtype NomadicSubtype) const
{
	if (M_TPlayerNomadicDataHashMap.Contains(NomadicSubtype))
	{
		return M_TPlayerNomadicDataHashMap[NomadicSubtype].Abilities;
	}
	return {};
}

TArray<FUnitAbilityEntry> ACPPGameState::GetSquadAbilities(const ESquadSubtype SquadSubtype) const
{
	if (M_TPlayerSquadDataHashMap.Contains(SquadSubtype))
	{
		return M_TPlayerSquadDataHashMap[SquadSubtype].Abilities;
	}
	return {};
}

TArray<FUnitAbilityEntry> ACPPGameState::GetTankAbilities(const ETankSubtype TankSubtype) const
{
	if (M_TPlayerTankDataHashMap.Contains(TankSubtype))
	{
		return M_TPlayerTankDataHashMap[TankSubtype].Abilities;
	}
	return {};
}

TArray<FUnitAbilityEntry> ACPPGameState::GetBxpAbilities(const EBuildingExpansionType BxpType) const
{
	if (M_TPlayerBxpDataHashMap.Contains(BxpType))
	{
		return M_TPlayerBxpDataHashMap[BxpType].Abilities;
	}
	return {};
}

int ACPPGameState::GetGameTime(const EGameTimeUnit GameTimeUnit) const
{
	switch (GameTimeUnit)
	{
	case EGameTimeUnit::Seconds:
		// Round up to 1 decimal
		return M_GameTimeSeconds;
	case EGameTimeUnit::Minutes:
		return M_GameTimeHours;
	case EGameTimeUnit::Hours:
		return M_GameTimeHours;
	case EGameTimeUnit::Days:
		return M_GameTimeDays;
	default:
		return EGameTimeUnit::Seconds;
	}
}

FString ACPPGameState::GetGameTimeAsString(const EGameTimeUnit GameTimeUnit) const
{
	// get seconds, minutes and hours in 00
	switch (GameTimeUnit)
	{
	case EGameTimeUnit::Seconds:
		if (M_GameTimeSeconds < 10)
		{
			// only get the first digit of the float
			return "0" + FString::FromInt(M_GameTimeSeconds);
		}
		return FString::FromInt(M_GameTimeSeconds);
	case EGameTimeUnit::Minutes:
		if (M_GameTimeMinutes < 10)
		{
			return "0" + FString::FromInt(M_GameTimeMinutes);
		}
	case EGameTimeUnit::Hours:
		if (M_GameTimeHours < 10)
		{
			return "0" + FString::FromInt(M_GameTimeHours);
		}
		return FString::FromInt(M_GameTimeHours);
	case EGameTimeUnit::Days:
		return FString::FromInt(M_GameTimeDays);
	default:
		return "00";
	}
}

void ACPPGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bM_IsClockRunning)
	{
		M_GameTimeSeconds += DeltaSeconds;
		if (M_GameTimeSeconds >= 60)
		{
			M_GameTimeSeconds = 0;
			M_GameTimeMinutes++;
			if (M_GameTimeMinutes >= 60)
			{
				M_GameTimeMinutes = 0;
				M_GameTimeHours++;
				if (M_GameTimeHours >= 24)
				{
					M_GameTimeHours = 0;
					M_GameTimeDays++;
				}
			}
		}
	}
	if (M_MPC_Time_Instance.IsValid())
	{
		M_MPC_Time_Instance->SetScalarParameterValue("WorldTime", UGameplayStatics::GetTimeSeconds(this));
	}
}

void ACPPGameState::BeginPlay()
{
	Super::BeginPlay();
	NotifyPlayerGameSettingsLoaded();
	InitializeSmallArmsProjectileManager();
	InitializeWeaponVFXSettings();

	SetActorTickEnabled(true);
	StartClock();
	if (GEngine)
	{
		GEngine->Exec(GetWorld(), TEXT("stat fps"));
		// Cap the FPS to 60
		// GEngine->Exec(GetWorld(), TEXT("t.MaxFPS 60"));
	}
}

void ACPPGameState::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Initialize the explosion manager on the subsystem.
	InitExplMgrOnSubsystem();
	InitDecalMgrOnSubSystem();
}

void ACPPGameState::SetupMPCTime(UMaterialParameterCollection* MPC_Time)
{
	UMaterialParameterCollectionInstance* MPCInstance = GetWorld()->GetParameterCollectionInstance(MPC_Time);
	M_MPC_Time_Instance = MPCInstance;
}

void ACPPGameState::InitExplMgrOnSubsystem()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	UExplosionManagerSubsystem* ExplosionManagerSubsystem = World->GetSubsystem<UExplosionManagerSubsystem>();
	if (not IsValid(ExplosionManagerSubsystem))
	{
		RTSFunctionLibrary::ReportError("Could not find valid subsystem in world to set explosion manager.");
		return;
	}
	ExplosionManagerSubsystem->SetExplosionManager(M_GameExplosionsManager);
}

void ACPPGameState::InitDecalMgrOnSubSystem()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	UDecalManagerSubsystem* DecalMgrSubsystem = World->GetSubsystem<UDecalManagerSubsystem>();
	if (not IsValid(DecalMgrSubsystem))
	{
		RTSFunctionLibrary::ReportError("Could not find valid subsystem in world to set decal manager.");
		return;
	}
	DecalMgrSubsystem->SetDecalManager(M_GameDecalManager);
}


void ACPPGameState::InitAllGameWeaponData()
{
	// Pen values equal to 100m - 500m war thunder wiki: https://wiki.warthunder.com/Panther_G
	// for mg we use the pen values in 100m
	//
	// reload time is set to the time of an aced crew.
	// turret rotation set to realistic aced crew.

	// Orchestrate category initializers; each adds to M_TPlayerWeaponDataHashMap
	InitAllGameLaserWeapons();
	InitAllGameFlameWeapons();
	InitAllGameBombWeapons();
	InitAllGameSmallArmsWeapons();
	InitAllGameLightWeapons();
	InitAllGameMediumWeapons();
	InitAllGameHeavyWeapons();

	// Set the enemy weapons to the same base data.
	M_TEnemyWeaponDataHashMap = M_TPlayerWeaponDataHashMap;
}

void ACPPGameState::InitAllGameLaserWeapons()
{
	// -------------------------------------------------------------
	// --------------------- Laser Weapons -------------------------
	// -------------------------------------------------------------

	FWeaponData WeaponData;
	using DeveloperSettings::GameBalance::Weapons::DamagePerTNTEquivalentGrams;
	using DeveloperSettings::GameBalance::Weapons::DamageFluxPercentage;
	using DeveloperSettings::GameBalance::Weapons::CooldownFluxPercentage;
	using namespace DeveloperSettings::GameBalance::Weapons::LaserWeapons;


	// Range Settings.
	using DeveloperSettings::GameBalance::Ranges::MediumLaserWeaponRange;

	// Projectile Settings.
	using DeveloperSettings::GamePlay::Projectile::BaseProjectileSpeed;

	// Note: laser uses iterations (configurable in the BP that uses this weapon)
	WeaponData.WeaponName = EWeaponName::Strahlkanone39;
	WeaponData.DamageType = ERTSDamageType::Laser;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE};
	WeaponData.WeaponCalibre = 50.f;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = Strahlkanone39BaseDamage * LaserWeaponDamageMlt;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumLaserWeaponRange;
	WeaponData.ArmorPen = 0;
	WeaponData.ArmorPenMaxRange = 0;
	WeaponData.MagCapacity = 3;
	WeaponData.ReloadSpeed = 5;
	WeaponData.BaseCooldown = 0.25;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 100;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = 0;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Strahlkanone39, WeaponData);

	WeaponData.WeaponName = EWeaponName::LB14;
	WeaponData.DamageType = ERTSDamageType::Laser;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE};
	WeaponData.WeaponCalibre = 30.f;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = LB14BaseDamage * LaserWeaponDamageMlt;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumLaserWeaponRange;
	WeaponData.ArmorPen = 0;
	WeaponData.ArmorPenMaxRange = 0;
	WeaponData.MagCapacity = 3;
	WeaponData.ReloadSpeed = 2;
	WeaponData.BaseCooldown = 0.5;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 100;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = 0;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::LB14, WeaponData);


	WeaponData.WeaponName = EWeaponName::SPEKTR_V;
	WeaponData.DamageType = ERTSDamageType::Laser;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE};
	WeaponData.WeaponCalibre = 30.f;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = SpektrVBaseDamage * LaserWeaponDamageMlt;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumLaserWeaponRange;
	WeaponData.ArmorPen = 0;
	WeaponData.ArmorPenMaxRange = 0;
	WeaponData.MagCapacity = 3;
	WeaponData.ReloadSpeed = 2;
	WeaponData.BaseCooldown = 0.5;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 100;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = 0;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::SPEKTR_V, WeaponData);

	WeaponData.WeaponName = EWeaponName::LightStorm;
	WeaponData.DamageType = ERTSDamageType::Laser;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE};
	WeaponData.WeaponCalibre = 30.f;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = LightStormBaseDamage * LaserWeaponDamageMlt;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumLaserWeaponRange;
	WeaponData.ArmorPen = 0;
	WeaponData.ArmorPenMaxRange = 0;
	WeaponData.MagCapacity = 8;
	WeaponData.ReloadSpeed = 6;
	WeaponData.BaseCooldown = 0.1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 100;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = 0;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::LightStorm, WeaponData);

	const float T34LuchDamage = Luch50LBaseDamage * LaserWeaponDamageMlt;
	const float LuchDamage = Luch85LBaseDamage * LaserWeaponDamageMlt;
	const float ZaryaDamage = Zarya100LBaseDamage * LaserWeaponDamageMlt;

	WeaponData.WeaponName = EWeaponName::Luch_50L;
	WeaponData.DamageType = ERTSDamageType::Laser;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE};
	WeaponData.WeaponCalibre = 50.f;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = T34LuchDamage;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumLaserWeaponRange;
	WeaponData.ArmorPen = 0;
	WeaponData.ArmorPenMaxRange = 0;
	WeaponData.MagCapacity = 3;
	WeaponData.ReloadSpeed = 5;
	WeaponData.BaseCooldown = 0.25;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 100;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = 0;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Luch_50L, WeaponData);

	WeaponData.WeaponName = EWeaponName::Luch_85L;
	WeaponData.DamageType = ERTSDamageType::Laser;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE};
	WeaponData.WeaponCalibre = 85.f;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = LuchDamage;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumLaserWeaponRange;
	WeaponData.ArmorPen = 0;
	WeaponData.ArmorPenMaxRange = 0;
	WeaponData.MagCapacity = 3;
	WeaponData.ReloadSpeed = 5;
	WeaponData.BaseCooldown = 0.25;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 100;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = 0;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Luch_85L, WeaponData);

	WeaponData.WeaponName = EWeaponName::Zarya_100L;
	WeaponData.DamageType = ERTSDamageType::Laser;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE};
	WeaponData.WeaponCalibre = 100.f;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = ZaryaDamage;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumLaserWeaponRange;
	WeaponData.ArmorPen = 0;
	WeaponData.ArmorPenMaxRange = 0;
	WeaponData.MagCapacity = 3;
	WeaponData.ReloadSpeed = 6;
	WeaponData.BaseCooldown = 0.25;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 100;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = 0;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Zarya_100L, WeaponData);
}

void ACPPGameState::InitAllGameFlameWeapons()
{
	FWeaponData WeaponData;
	using DeveloperSettings::GameBalance::Weapons::DamageFluxPercentage;
	using DeveloperSettings::GameBalance::Weapons::CooldownFluxPercentage;
	using DeveloperSettings::GameBalance::Ranges::LightFlameRange;
	using DeveloperSettings::GameBalance::Ranges::MediumFlameRange;
	using DeveloperSettings::GameBalance::Weapons::FlameWeapons::FlameConeAngleUnit;


	WeaponData.WeaponName = EWeaponName::Ashmaker05;
	WeaponData.DamageType = ERTSDamageType::Fire;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8.f;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = 20;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = LightFlameRange;
	WeaponData.ArmorPen = 0;
	WeaponData.ArmorPenMaxRange = 0;
	WeaponData.MagCapacity = 5;
	WeaponData.ReloadSpeed = 1.5;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 100;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = 0;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Ashmaker05, WeaponData);

	WeaponData.WeaponName = EWeaponName::Flamm09;
	WeaponData.DamageType = ERTSDamageType::Fire;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 12.f;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = 25;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = LightFlameRange;
	WeaponData.ArmorPen = 0;
	WeaponData.ArmorPenMaxRange = 0;
	WeaponData.MagCapacity = 5;
	WeaponData.ReloadSpeed = 1.5;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 100;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = 0;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Flamm09, WeaponData);


	WeaponData.WeaponName = EWeaponName::Ashmaker11;
	WeaponData.DamageType = ERTSDamageType::Fire;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 40.f;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = 18;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumFlameRange;
	WeaponData.ArmorPen = 0;
	WeaponData.ArmorPenMaxRange = 0;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 2.25;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 100;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = 0;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Flamm15, WeaponData);

	WeaponData.WeaponName = EWeaponName::Flamm15;
	WeaponData.DamageType = ERTSDamageType::Fire;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 50;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = 25;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumFlameRange;
	WeaponData.ArmorPen = 0;
	WeaponData.ArmorPenMaxRange = 0;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 2.25;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 100;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = 0;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Flamm15, WeaponData);
}

void ACPPGameState::InitAllGameBombWeapons()
{
	// -------------------------------------------------------------
	// --------------------- AIRCRAFT BOMBS ------------------------
	// -------------------------------------------------------------

	FWeaponData WeaponData;
	using DeveloperSettings::GameBalance::Weapons::DamagePerMM;
	using DeveloperSettings::GameBalance::Weapons::DamagePerTNTEquivalentGrams;
	using DeveloperSettings::GameBalance::Weapons::DamageFluxPercentage;
	using DeveloperSettings::GameBalance::Weapons::CooldownFluxPercentage;

	// Shrapnel settings.
	using DeveloperSettings::GameBalance::Weapons::ShrapnelAmountPerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelRangePerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelPenPerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelDamagePerTNTGram;

	// Projectile Settings.
	using DeveloperSettings::GamePlay::Projectile::HighVelocityProjectileSpeed;

	// Range Settings.
	using DeveloperSettings::GameBalance::Ranges::HeavyCannonRange;


	// ======================================================
	// Balance functions for bomb weapon data
	// Designers can adjust these to tweak scaling behavior.
	// ======================================================

	/** Weapon calibre scales roughly with explosive mass.
	 *  Adjust divisor to make weapons "thicker" or "thinner".
	 */
	auto GetWeaponCalibre = [](int TNTGrams)
	{
		return TNTGrams / 10;
	};

	/** Armor penetration grows sub-linearly with TNT mass.
	 *  Adjust multiplier or power to make bombs pierce more/less armor.
	 */
	auto GetArmorPen = [](int TNTGrams)
	{
		return static_cast<int>(100 * pow(TNTGrams / 250.0, 0.75));
	};

	/** Shrapnel range = direct function of explosive mass.
	 *  Adjust multiplier to make fragments reach further/shorter.
	 */
	auto GetShrapnelRange = [](int TNTGrams)
	{
		return TNTGrams;
	};

	/** Shrapnel particles scale ~20% of TNT grams.
	 *  Adjust ratio to make bombs fragment more/less.
	 */
	auto GetShrapnelParticles = [](int TNTGrams)
	{
		return TNTGrams / 5;
	};

	/** Shrapnel penetration tied to calibre.
	 *  Adjust multiplier to make fragments penetrate more/less armor.
	 */
	auto GetShrapnelPen = [&](int TNTGrams)
	{
		int calibre = GetWeaponCalibre(TNTGrams);
		return calibre * 5 * ShrapnelPenPerMM;
	};


	/** Reload speed increases slightly with bomb size.
	 *  Adjust divisor to slow/speed up heavy bomb reloads.
	 */
	auto GetReloadSpeed = [](int TNTGrams)
	{
		return 8 + (TNTGrams / 1000);
	};

	/** Accuracy drops slightly with bomb size.
	 *  Adjust rate to make larger bombs harder/easier to aim.
	 */
	auto GetAccuracy = [](int TNTGrams)
	{
		int penalty = TNTGrams / 1000 * 5;
		return std::max(70, 90 - penalty);
	};

	// ======================================================
	// Bomb weapon data declarations
	// ======================================================

	WeaponData.WeaponName = EWeaponName::Bomb_250Gr;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE};
	WeaponData.WeaponCalibre = GetWeaponCalibre(250);
	WeaponData.TNTExplosiveGrams = 250;
	WeaponData.BaseDamage = WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = HeavyCannonRange;
	WeaponData.ArmorPen = GetArmorPen(250);
	WeaponData.ArmorPenMaxRange = WeaponData.ArmorPen;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = GetReloadSpeed(250);
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = GetAccuracy(250);
	WeaponData.ShrapnelRange = GetShrapnelRange(250);
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = GetShrapnelParticles(250);
	WeaponData.ShrapnelPen = GetShrapnelPen(250);
	WeaponData.ProjectileMovementSpeed = HighVelocityProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Bomb_250Gr, WeaponData);

	WeaponData.WeaponName = EWeaponName::Bomb_500Gr;
	WeaponData.WeaponCalibre = GetWeaponCalibre(500);
	WeaponData.TNTExplosiveGrams = 500;
	WeaponData.BaseDamage = WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.ArmorPen = GetArmorPen(500);
	WeaponData.ArmorPenMaxRange = WeaponData.ArmorPen;
	WeaponData.ReloadSpeed = GetReloadSpeed(500);
	WeaponData.Accuracy = GetAccuracy(500);
	WeaponData.ShrapnelRange = GetShrapnelRange(500);
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = GetShrapnelParticles(500);
	WeaponData.ShrapnelPen = GetShrapnelPen(500);
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Bomb_500Gr, WeaponData);

	WeaponData.WeaponName = EWeaponName::Bomb_400Gr;
	WeaponData.WeaponCalibre = GetWeaponCalibre(400);
	WeaponData.TNTExplosiveGrams = 400;
	WeaponData.BaseDamage = WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.ArmorPen = GetArmorPen(400);
	WeaponData.ArmorPenMaxRange = WeaponData.ArmorPen;
	WeaponData.ReloadSpeed = GetReloadSpeed(400);
	WeaponData.Accuracy = GetAccuracy(400);
	WeaponData.ShrapnelRange = GetShrapnelRange(400);
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = GetShrapnelParticles(400);
	WeaponData.ShrapnelPen = GetShrapnelPen(400);
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Bomb_400Gr, WeaponData);

	WeaponData.WeaponName = EWeaponName::Bomb_1000Gr;
	WeaponData.WeaponCalibre = GetWeaponCalibre(1000);
	WeaponData.TNTExplosiveGrams = 1000;
	WeaponData.BaseDamage = WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.ArmorPen = GetArmorPen(1000);
	WeaponData.ArmorPenMaxRange = WeaponData.ArmorPen;
	WeaponData.ReloadSpeed = GetReloadSpeed(1000);
	WeaponData.Accuracy = GetAccuracy(1000);
	WeaponData.ShrapnelRange = GetShrapnelRange(1000);
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = GetShrapnelParticles(1000);
	WeaponData.ShrapnelPen = GetShrapnelPen(1000);
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Bomb_1000Gr, WeaponData);

	WeaponData.WeaponName = EWeaponName::BombRocket_700Gr;
	WeaponData.WeaponCalibre = GetWeaponCalibre(700);
	WeaponData.TNTExplosiveGrams = 700;
	WeaponData.BaseDamage = WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.ArmorPen = GetArmorPen(700);
	WeaponData.ArmorPenMaxRange = WeaponData.ArmorPen;
	WeaponData.ReloadSpeed = GetReloadSpeed(700);
	WeaponData.Accuracy = GetAccuracy(700);
	WeaponData.ShrapnelRange = GetShrapnelRange(700);
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = GetShrapnelParticles(700);
	WeaponData.ShrapnelPen = GetShrapnelPen(700);
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::BombRocket_700Gr, WeaponData);

	WeaponData.WeaponName = EWeaponName::BombRocket_800Gr;
	WeaponData.WeaponCalibre = GetWeaponCalibre(800);
	WeaponData.TNTExplosiveGrams = 800;
	WeaponData.BaseDamage = WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.ArmorPen = GetArmorPen(800);
	WeaponData.ArmorPenMaxRange = WeaponData.ArmorPen;
	WeaponData.ReloadSpeed = GetReloadSpeed(800);
	WeaponData.Accuracy = GetAccuracy(800);
	WeaponData.ShrapnelRange = GetShrapnelRange(800);
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = GetShrapnelParticles(800);
	WeaponData.ShrapnelPen = GetShrapnelPen(800);
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::BombRocket_800Gr, WeaponData);

	WeaponData.WeaponName = EWeaponName::BombRocket_1500Gr;
	WeaponData.WeaponCalibre = GetWeaponCalibre(1500);
	WeaponData.TNTExplosiveGrams = 1500;
	WeaponData.BaseDamage = WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.ArmorPen = GetArmorPen(1500);
	WeaponData.ArmorPenMaxRange = WeaponData.ArmorPen;
	WeaponData.ReloadSpeed = GetReloadSpeed(1500);
	WeaponData.Accuracy = GetAccuracy(1500);
	WeaponData.ShrapnelRange = GetShrapnelRange(1500);
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = GetShrapnelParticles(1500);
	WeaponData.ShrapnelPen = GetShrapnelPen(1500);
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::BombRocket_1500Gr, WeaponData);

	WeaponData.WeaponName = EWeaponName::BombRocket_2000Gr;
	WeaponData.WeaponCalibre = GetWeaponCalibre(2000);
	WeaponData.TNTExplosiveGrams = 2000;
	WeaponData.BaseDamage = WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.ArmorPen = GetArmorPen(2000);
	WeaponData.ArmorPenMaxRange = WeaponData.ArmorPen;
	WeaponData.ReloadSpeed = GetReloadSpeed(2000);
	WeaponData.Accuracy = GetAccuracy(2000);
	WeaponData.ShrapnelRange = GetShrapnelRange(2000);
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = GetShrapnelParticles(2000);
	WeaponData.ShrapnelPen = GetShrapnelPen(2000);
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::BombRocket_2000Gr, WeaponData);

	WeaponData.WeaponName = EWeaponName::BombRocket_3000Gr;
	WeaponData.WeaponCalibre = GetWeaponCalibre(3000);
	WeaponData.TNTExplosiveGrams = 3000;
	WeaponData.BaseDamage = WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.ArmorPen = GetArmorPen(3000);
	WeaponData.ArmorPenMaxRange = WeaponData.ArmorPen;
	WeaponData.ReloadSpeed = GetReloadSpeed(3000);
	WeaponData.Accuracy = GetAccuracy(3000);
	WeaponData.ShrapnelRange = GetShrapnelRange(3000);
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = GetShrapnelParticles(3000);
	WeaponData.ShrapnelPen = GetShrapnelPen(3000);
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::BombRocket_3000Gr, WeaponData);

	WeaponData.WeaponName = EWeaponName::BombRocket_5000Gr;
	WeaponData.WeaponCalibre = GetWeaponCalibre(5000);
	WeaponData.TNTExplosiveGrams = 5000;
	WeaponData.BaseDamage = WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.ArmorPen = GetArmorPen(5000);
	WeaponData.ArmorPenMaxRange = WeaponData.ArmorPen;
	WeaponData.ReloadSpeed = GetReloadSpeed(5000);
	WeaponData.Accuracy = GetAccuracy(5000);
	WeaponData.ShrapnelRange = GetShrapnelRange(5000);
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = GetShrapnelParticles(5000);
	WeaponData.ShrapnelPen = GetShrapnelPen(5000);
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::BombRocket_5000Gr, WeaponData);
}


void ACPPGameState::InitAllGameSmallArmsWeapons()
{
	// ---------------- Small arms: all weapons with calibre < 20mm ----------------

	FWeaponData WeaponData;
	using DeveloperSettings::GameBalance::Weapons::DamagePerMM;
	using DeveloperSettings::GameBalance::Weapons::DamagePerTNTEquivalentGrams;
	using DeveloperSettings::GameBalance::Weapons::DamageFluxPercentage;
	using DeveloperSettings::GameBalance::Weapons::CooldownFluxPercentage;
	using DeveloperSettings::GameBalance::Weapons::BaseTankMGReloadTime;

	// Small arms settings
	constexpr float SmallArmsDamageMlt = DeveloperSettings::GameBalance::Weapons::DamageBonusSmallArmsMlt;
	constexpr float SniperDamage = DeveloperSettings::GameBalance::Weapons::SniperDamage;

	// Shrapnel settings.
	using DeveloperSettings::GameBalance::Weapons::ShrapnelAmountPerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelRangePerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelPenPerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelDamagePerTNTGram;

	using namespace DeveloperSettings::GameBalance::Weapons::SmallArmsAccuracy;
	// Projectile Settings.
	using DeveloperSettings::GamePlay::Projectile::BaseProjectileSpeed;
	// weapon cooldown multipliers
	using DeveloperSettings::GameBalance::Weapons::SmallArmsCooldownMlt;
	using DeveloperSettings::GameBalance::Weapons::SmallArmsCooldownFluxMlt;

	// Range Settings.
	using DeveloperSettings::GameBalance::Ranges::BasicSmallArmsRange;
	using DeveloperSettings::GameBalance::Ranges::SmallArmsRifleRange;

	// Order used for every weapon below:
	// WeaponName, ShellType, ShellTypes, WeaponCalibre, TNTExplosiveGrams,
	// BaseDamage, DamageFlux, Range, ArmorPen, ArmorPenMaxRange,
	// MagCapacity, ReloadSpeed, BaseCooldown, CooldownFlux, Accuracy,
	// ShrapnelRange, ShrapnelDamage, ShrapnelParticles, ShrapnelPen,
	// ProjectileMovementSpeed

	auto SetSmallArmsCooldown = [](FWeaponData& Data, float BaseCooldown)
	{
		Data.BaseCooldown = BaseCooldown * SmallArmsCooldownMlt;
		Data.CooldownFlux = CooldownFluxPercentage * SmallArmsCooldownFluxMlt;
	};

	// FG-42
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::FG_42_7_92MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 10.f;
	WeaponData.ArmorPenMaxRange = 8.f;
	WeaponData.MagCapacity = 20;
	WeaponData.ReloadSpeed = 4.f;
	SetSmallArmsCooldown(WeaponData, 0.25f);
	WeaponData.Accuracy = SmgAccuracy + 20.f;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::FG_42_7_92MM, WeaponData);

	// STG44
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::STG44_7_92MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 12.f;
	WeaponData.ArmorPenMaxRange = 8.f;
	WeaponData.MagCapacity = 30;
	WeaponData.ReloadSpeed = 5.f;
	SetSmallArmsCooldown(WeaponData, 0.6f);
	WeaponData.Accuracy = SmgAccuracy + 10.f;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 1.1f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::STG44_7_92MM, WeaponData);

	// Ripper Gun
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::RipperGun_7_62MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 12.f;
	WeaponData.ArmorPenMaxRange = 8.f;
	WeaponData.MagCapacity = 50;
	WeaponData.ReloadSpeed = 4.f;
	SetSmallArmsCooldown(WeaponData, 0.2f);
	WeaponData.Accuracy = SmgAccuracy;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 1.1f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::RipperGun_7_62MM, WeaponData);

	// SVT-40
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::SVT_40_7_62MM;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 12.f;
	WeaponData.ArmorPenMaxRange = 8.f;
	WeaponData.MagCapacity = 10;
	WeaponData.ReloadSpeed = 5.f;
	SetSmallArmsCooldown(WeaponData, 0.8f);
	WeaponData.Accuracy = SmgAccuracy + 15.f;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 1.1f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::SVT_40_7_62MM, WeaponData);

	// PTRS-41 anti-tank rifle (14.5mm) — KEEP ORIGINAL COOLDOWN
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::PTRS_41_14_5MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 14.5f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre + 10.f;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 40.f;
	WeaponData.ArmorPenMaxRange = 30.f;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 4.f;
	WeaponData.BaseCooldown = 1.f;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	// Low accuracy to make it worse vs infantry.
	WeaponData.Accuracy = RifleAccuracy - 30.f;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::PTRS_41_14_5MM, WeaponData);
	WeaponData.WeaponName = EWeaponName::PTRS_41_14_5MM;


	// M920 special AT rifle.
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::M920_AtSniper;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 14.5f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre + 15.f;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 120.f;
	WeaponData.ArmorPenMaxRange = 110.f;
	WeaponData.MagCapacity = 10;
	WeaponData.ReloadSpeed = 6.f;
	SetSmallArmsCooldown(WeaponData, 1.67f);
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	// Low accuracy to make it worse vs infantry.
	WeaponData.Accuracy = SniperAccuracy - 20.f;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::M920_AtSniper, WeaponData);

	// Handheld railgun with radixite rounds
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::GerRailGun30MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_Radixite;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_Radixite};
	WeaponData.WeaponCalibre = 30;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre *
		DeveloperSettings::GameBalance::Weapons::RailGunDamageMlt;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 170.f;
	WeaponData.ArmorPenMaxRange = 150.f;
	WeaponData.MagCapacity = 6;
	WeaponData.ReloadSpeed = 4.f;
	WeaponData.BaseCooldown = 2.f;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	// Low accuracy to make it worse vs infantry.
	WeaponData.Accuracy = RifleAccuracy + 10;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::GerRailGun30MM, WeaponData);

	// Handheld railgun with radixite rounds
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::RailGunY;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_Radixite;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_Radixite};
	WeaponData.WeaponCalibre = 40;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre *
		DeveloperSettings::GameBalance::Weapons::RailGunDamageMlt;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 190.f;
	WeaponData.ArmorPenMaxRange = 170.f;
	WeaponData.MagCapacity = 2;
	WeaponData.ReloadSpeed = 3.f;
	WeaponData.BaseCooldown = 2.2f;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = RifleAccuracy + 10;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::RailGunY, WeaponData);

	WeaponData.WeaponName = EWeaponName::PTRS_41_14_5MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 14.5f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre + 10.f;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 100.f;
	WeaponData.ArmorPenMaxRange = 80.f;
	WeaponData.MagCapacity = 8;
	WeaponData.ReloadSpeed = 4.f;
	WeaponData.BaseCooldown = 1.f;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = RifleAccuracy + 10;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::PTRS_X_Tishina, WeaponData);

	WeaponData.WeaponName = EWeaponName::PTRS_50MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {
		EWeaponShellType::Shell_APHE,
	};
	WeaponData.WeaponCalibre = 50;
	WeaponData.TNTExplosiveGrams = 23;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = SmallArmsRifleRange + 500;
	WeaponData.ArmorPen = 143;
	WeaponData.ArmorPenMaxRange = 130;
	WeaponData.MagCapacity = 3;
	WeaponData.ReloadSpeed = 4.f;
	WeaponData.BaseCooldown = 3;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 75;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::PTRS_50MM, WeaponData);

	// Kar 98k
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::Kar_98k;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre + 10.f;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = SmallArmsRifleRange;
	WeaponData.ArmorPen = 14.f;
	WeaponData.ArmorPenMaxRange = 11.f;
	WeaponData.MagCapacity = 5;
	WeaponData.ReloadSpeed = 6.f;
	SetSmallArmsCooldown(WeaponData, 1.5f);
	WeaponData.Accuracy = RifleAccuracy;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 1.2f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Kar_98k, WeaponData);

	// Kar Sniper
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::Kar_Sniper;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SniperDamage;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = SmallArmsRifleRange;
	WeaponData.ArmorPen = 14.f;
	WeaponData.ArmorPenMaxRange = 11.f;
	WeaponData.MagCapacity = 5;
	WeaponData.ReloadSpeed = 6.f;
	SetSmallArmsCooldown(WeaponData, 1.5f);
	WeaponData.Accuracy = SniperAccuracy;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 1.2f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Kar_Sniper, WeaponData);

	// Mosin Sniper
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::Mosin_Snip;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SniperDamage;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = SmallArmsRifleRange;
	WeaponData.ArmorPen = 14.f;
	WeaponData.ArmorPenMaxRange = 11.f;
	WeaponData.MagCapacity = 5;
	WeaponData.ReloadSpeed = 6.f;
	SetSmallArmsCooldown(WeaponData, 1.5f);
	WeaponData.Accuracy = SniperAccuracy;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 1.2f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Mosin_Snip, WeaponData);

	// SKS 
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::SKS;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre + 20.f;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = SmallArmsRifleRange;
	WeaponData.ArmorPen = 18.f;
	WeaponData.ArmorPenMaxRange = 15.f;
	WeaponData.MagCapacity = 10;
	WeaponData.ReloadSpeed = 6.f;
	SetSmallArmsCooldown(WeaponData, 1.5f);
	WeaponData.Accuracy = RifleAccuracy + 15;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 1.2f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::SKS, WeaponData);

	// Mosin
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::Mosin;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre + 10.f;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = SmallArmsRifleRange;
	WeaponData.ArmorPen = 14.f;
	WeaponData.ArmorPenMaxRange = 11.f;
	WeaponData.MagCapacity = 5;
	WeaponData.ReloadSpeed = 6.f;
	SetSmallArmsCooldown(WeaponData, 1.5f);
	WeaponData.Accuracy = RifleAccuracy;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 1.2f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Mosin, WeaponData);

	// Mauser
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::Mauser;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre - 5.f;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 9.f;
	WeaponData.ArmorPenMaxRange = 6.f;
	WeaponData.MagCapacity = 10;
	WeaponData.ReloadSpeed = 3.f;
	SetSmallArmsCooldown(WeaponData, 0.25f);
	WeaponData.Accuracy = 50.f;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 0.9f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Mauser, WeaponData);

	// M1895 Nagant
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::M1895_Nagant;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre - 5.f;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 9.f;
	WeaponData.ArmorPenMaxRange = 6.f;
	WeaponData.MagCapacity = 7;
	WeaponData.ReloadSpeed = 3.f;
	SetSmallArmsCooldown(WeaponData, 1.5f);
	WeaponData.Accuracy = 50.f;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 0.9f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::M1895_Nagant, WeaponData);

	// MP40
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::MP40;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 9.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre - 5.f;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 9.f;
	WeaponData.ArmorPenMaxRange = 6.f;
	WeaponData.MagCapacity = 32;
	WeaponData.ReloadSpeed = 4.f;
	SetSmallArmsCooldown(WeaponData, 0.6f);
	WeaponData.Accuracy = SmgAccuracy;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::MP40, WeaponData);

	// MP46
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::MP46;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 9.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre + 4;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 11.f;
	WeaponData.ArmorPenMaxRange = 9.f;
	WeaponData.MagCapacity = 40;
	WeaponData.ReloadSpeed = 4.f;
	SetSmallArmsCooldown(WeaponData, 0.6f);
	WeaponData.Accuracy = SmgAccuracy;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::MP46, WeaponData);

	// Fedorov Avtomat
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::Fedrov_Avtomat;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 9.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre - 5.f;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 9.f;
	WeaponData.ArmorPenMaxRange = 6.f;
	WeaponData.MagCapacity = 25;
	WeaponData.ReloadSpeed = 4.f;
	SetSmallArmsCooldown(WeaponData, 0.6f);
	WeaponData.Accuracy = SmgAccuracy + 10;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Fedrov_Avtomat, WeaponData);

	// pps-43
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::PPS_43_7_62MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre + 3.f;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 10.f;
	WeaponData.ArmorPenMaxRange = 8.f;
	WeaponData.MagCapacity = 30;
	WeaponData.ReloadSpeed = 3.5f;
	SetSmallArmsCooldown(WeaponData, 0.6f);
	WeaponData.Accuracy = SmgAccuracy + 15;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::PPS_43_7_62MM, WeaponData);
	// PPSh-41
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::PPSh_41_7_62MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre - 5.f;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 9.f;
	WeaponData.ArmorPenMaxRange = 6.f;
	WeaponData.MagCapacity = 71;
	WeaponData.ReloadSpeed = 7.f;
	SetSmallArmsCooldown(WeaponData, 0.6f);
	WeaponData.Accuracy = SmgAccuracy;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::PPSh_41_7_62MM, WeaponData);

	// MG-34 — KEEP ORIGINAL
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::MG_34;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 12.f;
	WeaponData.ArmorPenMaxRange = 8.f;
	WeaponData.MagCapacity = 50;
	WeaponData.ReloadSpeed = 6.f;
	WeaponData.BaseCooldown = 0.4f;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = HMGAccuracy;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 0.9f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::MG_34, WeaponData);

	// MG-15 — KEEP ORIGINAL
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::MG_15;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 8.f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = SmallArmsDamageMlt * DamagePerMM * WeaponData.WeaponCalibre;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 12.f;
	WeaponData.ArmorPenMaxRange = 8.f;
	WeaponData.MagCapacity = 500;
	WeaponData.ReloadSpeed = 6.f;
	WeaponData.BaseCooldown = 0.4f;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = HMGAccuracy;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 0.9f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::MG_15, WeaponData);

	// T-26 hull MG — KEEP ORIGINAL
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::T26_Mg;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 7.6f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = 7.f * DamagePerMM;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 13.f;
	WeaponData.ArmorPenMaxRange = 7.f;
	WeaponData.MagCapacity = 80;
	WeaponData.ReloadSpeed = BaseTankMGReloadTime;
	WeaponData.BaseCooldown = 1.5f;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = HMGAccuracy;
	WeaponData.ShrapnelRange = 0.f;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = 0.f;
	WeaponData.ShrapnelPen = 0.f;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 0.9f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::T26_Mg, WeaponData);

	// DShK 12.7mm — KEEP ORIGINAL
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::DShk_12_7MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 12.7f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = 13.f * DamagePerMM;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 27.f;
	WeaponData.ArmorPenMaxRange = 20.f;
	WeaponData.MagCapacity = 200;
	WeaponData.ReloadSpeed = 6.f;
	WeaponData.BaseCooldown = 3.f;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = HMGAccuracy - 10.f;
	WeaponData.ShrapnelRange = 0.f;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = 0.f;
	WeaponData.ShrapnelPen = 0.f;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::DShk_12_7MM, WeaponData);

	// German tank MG 7.6mm — KEEP ORIGINAL
	WeaponData = {};
	WeaponData.WeaponName = EWeaponName::Ger_TankMG_7_6MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 7.6f;
	WeaponData.TNTExplosiveGrams = 0.f;
	WeaponData.BaseDamage = WeaponData.WeaponCalibre * DamagePerMM;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 12.f;
	WeaponData.ArmorPenMaxRange = 12.f;
	WeaponData.MagCapacity = 100;
	WeaponData.ReloadSpeed = BaseTankMGReloadTime;
	WeaponData.BaseCooldown = 3.f;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = HMGAccuracy;
	WeaponData.ShrapnelRange = 0.f;
	WeaponData.ShrapnelDamage = 0.f;
	WeaponData.ShrapnelParticles = 0.f;
	WeaponData.ShrapnelPen = 0.f;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Ger_TankMG_7_6MM, WeaponData);
}

void ACPPGameState::InitAllGameLightWeapons()
{
	// ---------------- Light: 20mm–49mm (inclusive) ----------------

	FWeaponData WeaponData;
	using DeveloperSettings::GameBalance::Weapons::DamagePerMM;
	using DeveloperSettings::GameBalance::Weapons::DamagePerTNTEquivalentGrams;
	using DeveloperSettings::GameBalance::Weapons::DamageFluxPercentage;
	using DeveloperSettings::GameBalance::Weapons::CooldownFluxPercentage;

	constexpr float FlakCannonBonusDamageFactor = 1 +
		DeveloperSettings::GameBalance::Weapons::DamageBonusFlakAmmoPercentage / 100.f;

	// Shrapnel settings.
	using DeveloperSettings::GameBalance::Weapons::ShrapnelAmountPerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelRangePerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelPenPerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelDamagePerTNTGram;
	using DeveloperSettings::GameBalance::Weapons::MortarAOEMlt;

	// Projectile Settings.
	using DeveloperSettings::GamePlay::Projectile::BaseProjectileSpeed;
	using DeveloperSettings::GamePlay::Projectile::HighVelocityProjectileSpeed;

	// Range Settings.
	using DeveloperSettings::GameBalance::Ranges::LightCannonRange;
	using DeveloperSettings::GameBalance::Ranges::LightAssaultCannonRange;
	using DeveloperSettings::GameBalance::Ranges::BasicSmallArmsRange;
	using DeveloperSettings::GameBalance::Ranges::MediumArtilleryRange;

	const float PanzerSchreckTNTPerMM = 55.f / 88.f;
	const float PanzerSchreckArmorPenPerMM = 150.f / 88.f;
	const float PanzerSchreckArmorPenMaxRangePerMM = 100.f / 88.f;
	const float MortarTNTPerMM = 3400.f / 380.f;
	const float MortarArmorPenPerMM = 320.f / 380.f;
	const int32 PanzerwerferMagCapacity = 10;
	const float PanzerwerferReloadSpeed = 10.f;
	const float PanzerwerferBaseCooldown = 0.5f;

	// Panzerfaust (44mm; HEAT)
	WeaponData.WeaponName = EWeaponName::PanzerFaust;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_HEAT;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_HEAT};
	WeaponData.WeaponCalibre = 44;
	WeaponData.TNTExplosiveGrams = 32;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	// Gets multiplied with 1.2 for heat rounds.
	WeaponData.ArmorPen = 100 * 0.8f;
	WeaponData.ArmorPenMaxRange = 100 * 0.8f;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 5;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 70;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = DeveloperSettings::GamePlay::Projectile::BaseProjectileSpeed * 0.6f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::PanzerFaust, WeaponData);

	// Panzerwerfer small (40mm; HEAT)
	WeaponData.WeaponName = EWeaponName::Panzerwerfer_Small;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_HEAT;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_HEAT};
	WeaponData.WeaponCalibre = 40;
	WeaponData.TNTExplosiveGrams = PanzerSchreckTNTPerMM * WeaponData.WeaponCalibre;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = DeveloperSettings::GameBalance::Ranges::LightArtilleryRange;
	WeaponData.ArmorPen = (PanzerSchreckArmorPenPerMM * WeaponData.WeaponCalibre)
		/ DeveloperSettings::GameBalance::Weapons::Projectiles::HEAT_ArmorPenMlt;
	WeaponData.ArmorPenMaxRange = (PanzerSchreckArmorPenMaxRangePerMM * WeaponData.WeaponCalibre)
		/ DeveloperSettings::GameBalance::Weapons::Projectiles::HEAT_ArmorPenMlt;
	WeaponData.MagCapacity = PanzerwerferMagCapacity;
	WeaponData.ReloadSpeed = PanzerwerferReloadSpeed;
	WeaponData.BaseCooldown = PanzerwerferBaseCooldown;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 20;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 0.9f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Panzerwerfer_Small, WeaponData);

	// Mortar 40mm (HE)
	WeaponData.WeaponName = EWeaponName::Mortar_40MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_HE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 40;
	WeaponData.TNTExplosiveGrams = MortarTNTPerMM * WeaponData.WeaponCalibre;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = RTSFunctionLibrary::RoundToNearestMultipleOf(
		DeveloperSettings::GameBalance::Ranges::MediumArtilleryRange, 100);
	WeaponData.ArmorPen = (MortarArmorPenPerMM * WeaponData.WeaponCalibre)
		/ DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.ArmorPenMaxRange = (MortarArmorPenPerMM * WeaponData.WeaponCalibre)
		/ DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.MagCapacity = 4;
	WeaponData.ReloadSpeed = 10.f;
	WeaponData.BaseCooldown = 1.f;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = DeveloperSettings::GameBalance::Weapons::MortarAccuracy;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM * MortarAOEMlt;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Mortar_40MM, WeaponData);

	// MG 151 (20mm aircraft)
	WeaponData.WeaponName = EWeaponName::MG_151;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 20;
	WeaponData.TNTExplosiveGrams = 10;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange;
	WeaponData.ArmorPen = 24;
	WeaponData.ArmorPenMaxRange = 14;
	WeaponData.MagCapacity = 600;
	WeaponData.ReloadSpeed = 6;
	WeaponData.BaseCooldown = 0.4;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 60;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::MG_151, WeaponData);

	// shVAK 20mm
	WeaponData.WeaponName = EWeaponName::shVAK_20MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 20;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = 20 * DamagePerMM;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange + 250;
	WeaponData.ArmorPen = 28;
	WeaponData.ArmorPenMaxRange = 14;
	WeaponData.MagCapacity = 24;
	WeaponData.ReloadSpeed = 6;
	WeaponData.BaseCooldown = 0.22;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 20;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::shVAK_20MM, WeaponData);


	// Ba 12 23mm
	WeaponData.WeaponName = EWeaponName::Ba12_23MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 23;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = 23 * DamagePerMM + 4;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = BasicSmallArmsRange + 250;
	WeaponData.ArmorPen = 45;
	WeaponData.ArmorPenMaxRange = 38;
	WeaponData.MagCapacity = 10;
	WeaponData.ReloadSpeed = 4.5;
	WeaponData.BaseCooldown = 0.22;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 65;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Ba12_23MM, WeaponData);

	// NS-37 (37mm flak/aircraft)
	WeaponData.WeaponName = EWeaponName::NS_37MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 37;
	WeaponData.TNTExplosiveGrams = 18.2f;
	WeaponData.BaseDamage = (DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams) * FlakCannonBonusDamageFactor;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = LightCannonRange;
	WeaponData.ArmorPen = 68;
	WeaponData.ArmorPenMaxRange = 57;
	WeaponData.MagCapacity = 15;
	WeaponData.ReloadSpeed = 4;
	WeaponData.BaseCooldown = 0.25;
	WeaponData.CooldownFlux = 10;
	WeaponData.Accuracy = 50;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 1.1;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::NS_37MM, WeaponData);

	// KwK30 20mm
	WeaponData.WeaponName = EWeaponName::KwK30_20MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 20;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = LightCannonRange;
	WeaponData.ArmorPen = 45;
	WeaponData.ArmorPenMaxRange = 31;
	WeaponData.MagCapacity = 10;
	WeaponData.ReloadSpeed = 5;
	WeaponData.BaseCooldown = 0.8;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 75;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = HighVelocityProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK30_20MM, WeaponData);

	// KwK30 20mm (Sd.Kfz.231)
	WeaponData.WeaponName = EWeaponName::Kwk30_20MM_sdkfz231;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Kwk30_20MM_sdkfz231, WeaponData);

	// KwK37 20mm (same as previous block, preserved)
	WeaponData.WeaponName = EWeaponName::Kwk37_20MM;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Kwk37_20MM, WeaponData);

	// 30mm autocannon
	WeaponData.WeaponName = EWeaponName::Kwk31_30MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.WeaponCalibre = 30;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre;
	WeaponData.ArmorPen = 73;
	WeaponData.ArmorPenMaxRange = 59;
	WeaponData.MagCapacity = 4;
	WeaponData.ReloadSpeed = 5;
	WeaponData.BaseCooldown = 0.5;
	WeaponData.Accuracy = 80;
	WeaponData.ProjectileMovementSpeed = HighVelocityProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Kwk31_30MM, WeaponData);

	// 37mm autocannon Jaguar
	WeaponData.WeaponName = EWeaponName::Kwk32_35MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.WeaponCalibre = 35;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre;
	WeaponData.ArmorPen = 88;
	WeaponData.ArmorPenMaxRange = 70;
	WeaponData.MagCapacity = 5;
	WeaponData.ReloadSpeed = 5;
	WeaponData.BaseCooldown = 0.2;
	WeaponData.Accuracy = 80;
	WeaponData.ProjectileMovementSpeed = HighVelocityProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Kwk32_35MM, WeaponData);
	// 30mm twin autocannon
	WeaponData.WeaponName = EWeaponName::Kwk34_Twin30MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.WeaponCalibre = 30;
	WeaponData.TNTExplosiveGrams = 0;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre;
	WeaponData.ArmorPen = 60;
	WeaponData.ArmorPenMaxRange = 52;
	WeaponData.MagCapacity = 3;
	WeaponData.ReloadSpeed = 5;
	WeaponData.BaseCooldown = 0.3;
	WeaponData.Accuracy = 80;
	WeaponData.ProjectileMovementSpeed = HighVelocityProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Kwk34_Twin30MM, WeaponData);

	// https://wiki.warthunder.com/Pz.38(t)_F
	WeaponData.WeaponName = EWeaponName::KwK38_T_37MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE};
	WeaponData.WeaponCalibre = 37;
	WeaponData.TNTExplosiveGrams = 22;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = LightCannonRange;
	WeaponData.ArmorPen = 59;
	WeaponData.ArmorPenMaxRange = 45;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 3.3;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 80;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 0.95f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK38_T_37MM, WeaponData);

	// https://wiki.warthunder.com/Panzerjager_I
	WeaponData.WeaponName = EWeaponName::Pak_t_L_43_47MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	// Pz jager type uses only APCR shells.
	WeaponData.ShellType = EWeaponShellType::Shell_APCR;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APCR};
	WeaponData.WeaponCalibre = 47;
	WeaponData.TNTExplosiveGrams = 15;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = LightAssaultCannonRange;
	WeaponData.ArmorPen = 82;
	WeaponData.ArmorPenMaxRange = 66;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 3;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 95;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 1.2f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Pak_t_L_43_47MM, WeaponData);

	// https://wiki.warthunder.com/T-26
	WeaponData.WeaponName = EWeaponName::T26_45MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE};
	WeaponData.WeaponCalibre = 45;
	WeaponData.TNTExplosiveGrams = 29;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = LightCannonRange;
	WeaponData.ArmorPen = 67;
	WeaponData.ArmorPenMaxRange = 58;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 4;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 80;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 0.9f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::T26_45MM, WeaponData);

	// BT-7 20K (same stats)
	WeaponData.WeaponName = EWeaponName::BT_7_20K_45MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::BT_7_20K_45MM, WeaponData);

	// T-70 20K (faster reload & more accurate)
	WeaponData.WeaponName = EWeaponName::T_70_20k_45MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ReloadSpeed = 3.2f;
	WeaponData.Accuracy = 85;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::T_70_20k_45MM, WeaponData);

	// Flak 38 20mm
	// https://wiki.warthunder.com/Flakpanzer_38
	WeaponData.WeaponName = EWeaponName::Flak38_20MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP};
	WeaponData.WeaponCalibre = 20;
	WeaponData.TNTExplosiveGrams = 10.2f;
	WeaponData.BaseDamage = (DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams) * FlakCannonBonusDamageFactor;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = LightCannonRange;
	WeaponData.ArmorPen = 45;
	WeaponData.ArmorPenMaxRange = 31;
	WeaponData.MagCapacity = 20;
	WeaponData.ReloadSpeed = 10;
	WeaponData.BaseCooldown = 0.5;
	WeaponData.CooldownFlux = 0;
	WeaponData.Accuracy = 90;
	WeaponData.ShrapnelRange = 0;
	WeaponData.ShrapnelDamage = 0;
	WeaponData.ShrapnelParticles = 0;
	WeaponData.ShrapnelPen = 0;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 0.9f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Flak38_20MM, WeaponData);

	// Flak 36 37mm
	WeaponData.WeaponName = EWeaponName::Flak36_37MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.WeaponCalibre = 37;
	WeaponData.TNTExplosiveGrams = 18.2f;
	WeaponData.BaseDamage = (DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams) * FlakCannonBonusDamageFactor;
	WeaponData.ArmorPen = 45;
	WeaponData.ArmorPenMaxRange = 31;
	WeaponData.MagCapacity = 6;
	WeaponData.ReloadSpeed = 2;
	WeaponData.BaseCooldown = 0.38f;
	WeaponData.CooldownFlux = 10;
	WeaponData.Accuracy = 60;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 0.9f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Flak36_37MM, WeaponData);

	// Bofors 40mm
	WeaponData.WeaponName = EWeaponName::Bofors_40MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.WeaponCalibre = 40;
	WeaponData.TNTExplosiveGrams = 24;
	WeaponData.BaseDamage = (DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams) * FlakCannonBonusDamageFactor;
	WeaponData.ArmorPen = 50;
	WeaponData.ArmorPenMaxRange = 40;
	WeaponData.MagCapacity = 4;
	WeaponData.ReloadSpeed = 0.8f;
	WeaponData.BaseCooldown = 0.25f;
	WeaponData.CooldownFlux = 10;
	WeaponData.Accuracy = 60;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 0.9f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Bofors_40MM, WeaponData);
}

void ACPPGameState::InitAllGameMediumWeapons()
{
	// ---------------- Medium: 50mm–79mm (inclusive) ----------------

	FWeaponData WeaponData;
	using DeveloperSettings::GameBalance::Weapons::DamagePerMM;
	using DeveloperSettings::GameBalance::Weapons::DamagePerMM_FireProjectileBonus;
	using DeveloperSettings::GameBalance::Weapons::DamagePerTNTEquivalentGrams;
	using DeveloperSettings::GameBalance::Weapons::DamageFluxPercentage;
	using DeveloperSettings::GameBalance::Weapons::CooldownFluxPercentage;

	// Shrapnel settings.
	using DeveloperSettings::GameBalance::Weapons::ShrapnelAmountPerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelRangePerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelPenPerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelDamagePerTNTGram;

	// Projectile Settings.
	using DeveloperSettings::GamePlay::Projectile::BaseProjectileSpeed;
	using DeveloperSettings::GamePlay::Projectile::HighVelocityProjectileSpeed;
	using DeveloperSettings::GamePlay::Projectile::HEProjectileSpeed;

	// Range Settings.
	using DeveloperSettings::GameBalance::Ranges::LightCannonRange;
	using DeveloperSettings::GameBalance::Ranges::MediumCannonRange;
	using DeveloperSettings::GameBalance::Ranges::MediumAssaultCannonRange;

	// BK 5, 50mm (aircraft cannon)
	WeaponData.WeaponName = EWeaponName::BK_5_50MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_AP;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_AP, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 50;
	WeaponData.TNTExplosiveGrams = 24;
	WeaponData.BaseDamage = DeveloperSettings::GameBalance::Weapons::DamageBonusSmallArmsMlt * DamagePerMM * WeaponData.
		WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = DeveloperSettings::GameBalance::Ranges::BasicSmallArmsRange; // preserved from original
	WeaponData.ArmorPen = 101;
	WeaponData.ArmorPenMaxRange = 83;
	WeaponData.MagCapacity = 22;
	WeaponData.ReloadSpeed = 1.2f;
	WeaponData.BaseCooldown = 1.2f;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 100;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::BK_5_50MM, WeaponData);

	// https://wiki.warthunder.com/Pz.III_J
	WeaponData.WeaponName = EWeaponName::KwK42_L_50MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {
		EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE, EWeaponShellType::Shell_Radixite
	};
	WeaponData.WeaponCalibre = 50;
	WeaponData.TNTExplosiveGrams = 36;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumCannonRange;
	WeaponData.ArmorPen = 76;
	WeaponData.ArmorPenMaxRange = 62;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 4;
	WeaponData.BaseCooldown = 0;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 80;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 0.9f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK42_L_50MM, WeaponData);

	WeaponData.WeaponName = EWeaponName::KwK37_F_50MM;
	WeaponData.DamageType = ERTSDamageType::Fire;
	WeaponData.ShellType = EWeaponShellType::Shell_Fire;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_Fire};
	WeaponData.WeaponCalibre = 50;
	WeaponData.TNTExplosiveGrams = 36;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre + DamagePerMM_FireProjectileBonus * WeaponData.
		WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumCannonRange;
	WeaponData.ArmorPen = 76;
	WeaponData.ArmorPenMaxRange = 62;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 4;
	WeaponData.BaseCooldown = 0;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 80;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 0.9f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK37_F_50MM, WeaponData);


	// https://wiki.warthunder.com/Pz.III_M
	WeaponData.WeaponName = EWeaponName::KwK39_1_50MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 50.f;
	WeaponData.TNTExplosiveGrams = 38;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumCannonRange;
	WeaponData.ArmorPen = 101;
	WeaponData.ArmorPenMaxRange = 83;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 3.7f;
	WeaponData.BaseCooldown = 0;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 80;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK39_1_50MM, WeaponData);


	WeaponData.WeaponName = EWeaponName::PanzerRifle_50mm;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE};
	WeaponData.WeaponCalibre = 50.f;
	WeaponData.TNTExplosiveGrams = 38;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumCannonRange;
	WeaponData.ArmorPen = 101;
	WeaponData.ArmorPenMaxRange = 83;
	WeaponData.MagCapacity = 3;
	WeaponData.ReloadSpeed = 5.f;
	WeaponData.BaseCooldown = 0.5;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 60;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::PanzerRifle_50mm, WeaponData);

	// Puma has faster reload.
	WeaponData.ReloadSpeed = 3.3f;
	WeaponData.WeaponName = EWeaponName::KwK39_1_50MM_Puma;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK39_1_50MM_Puma, WeaponData);

	// Pak 38 bxp has fast reload and higher accuracy.
	WeaponData.WeaponName = EWeaponName::Pak38_50MM;
	WeaponData.ReloadSpeed = 3.3f;
	WeaponData.Accuracy = 90;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Pak38_50MM, WeaponData);

	// Panzer IV F1, short barrel 75mm
	// https://old-wiki.warthunder.com/KwK37_(75_mm)
	WeaponData.WeaponName = EWeaponName::KwK37_75MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {
		EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE
	};
	WeaponData.WeaponCalibre = 75;
	WeaponData.TNTExplosiveGrams = 73;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = RTSFunctionLibrary::RoundToNearestMultipleOf(
		DeveloperSettings::GameBalance::Ranges::LightArtilleryRange * 1.1, 100);
	WeaponData.ArmorPen = 51;
	WeaponData.ArmorPenMaxRange = 45;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 4.3;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 76;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK37_75MM, WeaponData);

	// https://wiki.warthunder.com/Pz.Bef.Wg.IV_J
	// Also used on Hetzer.
	WeaponData.WeaponName = EWeaponName::KwK40_L48_75MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {
		EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE, EWeaponShellType::Shell_APCR,
		EWeaponShellType::Shell_HEAT, EWeaponShellType::Shell_APHEBC
	};
	WeaponData.WeaponCalibre = 75;
	WeaponData.TNTExplosiveGrams = 73;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumCannonRange;
	WeaponData.ArmorPen = 143;
	WeaponData.ArmorPenMaxRange = 130;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 5.9f;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 85;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK40_L48_75MM, WeaponData);

	WeaponData.Range = MediumAssaultCannonRange;
	WeaponData.ReloadSpeed = 6.2f;
	WeaponData.WeaponName = EWeaponName::Pak39_L_48_75MM;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Pak39_L_48_75MM, WeaponData);

	// Stug has better reload.
	WeaponData.WeaponName = EWeaponName::StuK40_L48_75MM;
	WeaponData.ReloadSpeed = 5.0f;
	WeaponData.Range = MediumAssaultCannonRange;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::StuK40_L48_75MM, WeaponData);

	// Marder cannon.
	WeaponData.WeaponName = EWeaponName::Pak40_3_L46_75MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	// Pz jager type uses only APCR shells.
	WeaponData.ShellType = EWeaponShellType::Shell_APCR;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APCR};
	WeaponData.WeaponCalibre = 75;
	WeaponData.TNTExplosiveGrams = 73;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = RTSFunctionLibrary::RoundToNearestMultipleOf( MediumAssaultCannonRange * 1.2, 10);
	WeaponData.ArmorPen = 148;
	WeaponData.ArmorPenMaxRange = 135;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 5.9f;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 94;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Pak40_3_L46_75MM, WeaponData);

	WeaponData.WeaponName = EWeaponName::Pak40_3_L46_75MM_Sdkfz251;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ReloadSpeed = 5.9f;
	WeaponData.Accuracy = 88;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Pak40_3_L46_75MM_Sdkfz251, WeaponData);

	// https://wiki.warthunder.com/Maus
	WeaponData.WeaponName = EWeaponName::KwK44_L_36_5_75MM;
	WeaponData.ShellType = EWeaponShellType::Shell_HEAT;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_HEAT};
	// Note: calibre/TNT carry from previous block in original; preserved
	WeaponData.ArmorPen = 80;
	WeaponData.ArmorPenMaxRange = 80;
	WeaponData.ReloadSpeed = 4.0f;
	WeaponData.Accuracy = 85;
	WeaponData.Range = MediumCannonRange;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK44_L_36_5_75MM, WeaponData);
	// E100 has same secondary.
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK44_L_36_5_75MM_E100, WeaponData);

	// https://wiki.warthunder.com/Panther_G
	// Special; take 10 m pen
	WeaponData.WeaponName = EWeaponName::KwK42_75MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {
		EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_APCR, EWeaponShellType::Shell_HE,
		EWeaponShellType::Shell_HEAT,
		EWeaponShellType::Shell_APHEBC
	};
	WeaponData.WeaponCalibre = 75;
	WeaponData.TNTExplosiveGrams = 86;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	// Range left as HeavyCannonRange in original; calibre puts it in Medium category by your spec
	WeaponData.Range = DeveloperSettings::GameBalance::Ranges::HeavyCannonRange;
	WeaponData.ArmorPen = 192;
	WeaponData.ArmorPenMaxRange = 188;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 7.4f;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 98;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = HighVelocityProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK42_75MM, WeaponData);
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KWK42_75MM_PantherD, WeaponData);

	// F-34 76mm family
	WeaponData.WeaponName = EWeaponName::F_34_76MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 76;
	WeaponData.TNTExplosiveGrams = 79;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumCannonRange;
	WeaponData.ArmorPen = 85;
	WeaponData.ArmorPenMaxRange = 77;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 6.9f;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 80;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::F_34_76MM, WeaponData);

	// F-34 76mm for T34E.
	WeaponData.WeaponName = EWeaponName::F_34_T34E;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::F_34_T34E, WeaponData);

	WeaponData.WeaponName = EWeaponName::ZIS_3_76MM;
	WeaponData.TNTExplosiveGrams = 89;
	WeaponData.ArmorPen = 142;
	WeaponData.ArmorPenMaxRange = 128;
	WeaponData.ReloadSpeed = 5;
	WeaponData.Accuracy = 80;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::ZIS_3_76MM, WeaponData);

	// SU-76 gun
	{
		const float Su76HeArmorPen = 61.f;
		const int32 Su76Accuracy = 85;

		FWeaponData Su76WeaponData = WeaponData;
		Su76WeaponData.WeaponName = EWeaponName::ZIS_3_76MM_SU76;
		Su76WeaponData.DamageType = ERTSDamageType::Kinetic;
		Su76WeaponData.ShellType = EWeaponShellType::Shell_HE;
		Su76WeaponData.ShellTypes = {EWeaponShellType::Shell_HE, EWeaponShellType::Shell_HEAT};
		Su76WeaponData.WeaponCalibre = 76;
		Su76WeaponData.TNTExplosiveGrams = 89;
		Su76WeaponData.BaseDamage = DamagePerMM * Su76WeaponData.WeaponCalibre
			+ Su76WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
		Su76WeaponData.DamageFlux = DamageFluxPercentage;
		Su76WeaponData.Range = MediumCannonRange;
		// HE is the starting shell for this gun but the projectile multiplies with this factor so we neutralize it.
		Su76WeaponData.ArmorPen = Su76HeArmorPen / DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
		Su76WeaponData.ArmorPenMaxRange = Su76HeArmorPen /
			DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
		Su76WeaponData.MagCapacity = 1;
		Su76WeaponData.ReloadSpeed = 5.0f;
		Su76WeaponData.BaseCooldown = 1;
		Su76WeaponData.CooldownFlux = CooldownFluxPercentage;
		Su76WeaponData.Accuracy = Su76Accuracy;
		Su76WeaponData.ShrapnelRange = Su76WeaponData.WeaponCalibre * ShrapnelRangePerMM;
		Su76WeaponData.ShrapnelDamage = Su76WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
		Su76WeaponData.ShrapnelParticles = Su76WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
		Su76WeaponData.ShrapnelPen = Su76WeaponData.WeaponCalibre * ShrapnelPenPerMM;
		Su76WeaponData.ProjectileMovementSpeed = HEProjectileSpeed;
		M_TPlayerWeaponDataHashMap.Add(EWeaponName::ZIS_3_76MM_SU76, Su76WeaponData);
	}

	// L-10 T-28
	WeaponData.WeaponName = EWeaponName::L_10_76MM;
	WeaponData.TNTExplosiveGrams = 90;
	WeaponData.ArmorPen = 85;
	WeaponData.ArmorPenMaxRange = 77;
	WeaponData.ReloadSpeed = 5;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::L_10_76MM, WeaponData);

	// https://wiki.warthunder.com/KV-1_(L-11)
	// Cramped turret; stock reload.
	WeaponData.WeaponName = EWeaponName::L_11_76MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.TNTExplosiveGrams = 79;
	WeaponData.ArmorPen = 76;
	WeaponData.ArmorPenMaxRange = 70;
	WeaponData.ReloadSpeed = 7.1f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::L_11_76MM, WeaponData);

	{
		const float Kv1ArcDamageMlt = 1.2f;
		const float Kv1ArcArmorPen = 76.f;

		FWeaponData Kv1ArcWeaponData = WeaponData;
		Kv1ArcWeaponData.WeaponName = EWeaponName::L_14_122MM_Arc;
		Kv1ArcWeaponData.DamageType = ERTSDamageType::Kinetic;
		Kv1ArcWeaponData.ShellType = EWeaponShellType::Shell_HE;
		Kv1ArcWeaponData.ShellTypes = {EWeaponShellType::Shell_HE};
		Kv1ArcWeaponData.WeaponCalibre = 122;
		Kv1ArcWeaponData.TNTExplosiveGrams = 140;
		Kv1ArcWeaponData.BaseDamage = (DamagePerMM * Kv1ArcWeaponData.WeaponCalibre
			+ Kv1ArcWeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams) * Kv1ArcDamageMlt;
		Kv1ArcWeaponData.DamageFlux = DamageFluxPercentage;
		WeaponData.Range = RTSFunctionLibrary::RoundToNearestMultipleOf(
			DeveloperSettings::GameBalance::Ranges::HeavyArtilleryRange* 0.92, 100);
		// He is the only shell used for this gun but the projectile multiplies with this factor so we neutralize it.
		Kv1ArcWeaponData.ArmorPen = Kv1ArcArmorPen /
			DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
		Kv1ArcWeaponData.ArmorPenMaxRange = Kv1ArcArmorPen /
			DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
		Kv1ArcWeaponData.MagCapacity = 1;
		Kv1ArcWeaponData.ReloadSpeed = 7.1f;
		Kv1ArcWeaponData.BaseCooldown = 1;
		Kv1ArcWeaponData.CooldownFlux = CooldownFluxPercentage;
		Kv1ArcWeaponData.Accuracy = 50;
		Kv1ArcWeaponData.ShrapnelRange = Kv1ArcWeaponData.WeaponCalibre * ShrapnelRangePerMM;
		Kv1ArcWeaponData.ShrapnelDamage = Kv1ArcWeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
		Kv1ArcWeaponData.ShrapnelParticles = Kv1ArcWeaponData.WeaponCalibre * ShrapnelAmountPerMM;
		Kv1ArcWeaponData.ShrapnelPen = Kv1ArcWeaponData.WeaponCalibre * ShrapnelPenPerMM;
		Kv1ArcWeaponData.ProjectileMovementSpeed = HEProjectileSpeed;
		M_TPlayerWeaponDataHashMap.Add(EWeaponName::L_14_122MM_Arc, Kv1ArcWeaponData);
	}

	// https://wiki.warthunder.com/KV-1E
	// Faster reload than L_11.
	WeaponData.WeaponName = EWeaponName::F_35_76MM;
	WeaponData.ReloadSpeed = 6.0f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::F_35_76MM, WeaponData);

	// https://wiki.warthunder.com/KV-1S
	// Reload between F-35 and L-11 and better pen
	WeaponData.WeaponName = EWeaponName::ZIS_5_76MM;
	WeaponData.ReloadSpeed = 6.9f;
	WeaponData.ArmorPen = 94;
	WeaponData.ArmorPenMaxRange = 84;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::ZIS_5_76MM, WeaponData);

	// https://wiki.warthunder.com/T-28_(1938)
	// Cramped turret; stock reload.
	WeaponData.WeaponName = EWeaponName::KT_28_76MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 76;
	WeaponData.TNTExplosiveGrams = 75;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.Range = MediumCannonRange;
	WeaponData.ArmorPen = 37;
	WeaponData.ArmorPenMaxRange = 34;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 4.0f;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = DamageFluxPercentage; // preserved original naming pattern
	WeaponData.Accuracy = 80;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = HEProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KT_28_76MM, WeaponData);

	// Longer reload for cramped turrets
	WeaponData.ReloadSpeed = 6.0f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KT_28_76MM_T24, WeaponData);
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KT_28_76MM_BT, WeaponData);
}

void ACPPGameState::InitAllGameHeavyWeapons()
{
	// ---------------- Heavy: calibre >= 80mm ----------------

	FWeaponData WeaponData;
	using DeveloperSettings::GameBalance::Weapons::DamagePerMM;
	using DeveloperSettings::GameBalance::Weapons::DamagePerTNTEquivalentGrams;
	using DeveloperSettings::GameBalance::Weapons::DamageFluxPercentage;
	using DeveloperSettings::GameBalance::Weapons::CooldownFluxPercentage;

	// Shrapnel settings.
	using DeveloperSettings::GameBalance::Weapons::ShrapnelAmountPerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelRangePerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelPenPerMM;
	using DeveloperSettings::GameBalance::Weapons::ShrapnelDamagePerTNTGram;

	// Projectile Settings.
	using DeveloperSettings::GamePlay::Projectile::BaseProjectileSpeed;
	using DeveloperSettings::GamePlay::Projectile::HighVelocityProjectileSpeed;
	using DeveloperSettings::GamePlay::Projectile::HEProjectileSpeed;

	// Range Settings.
	using DeveloperSettings::GameBalance::Ranges::HeavyCannonRange;
	using DeveloperSettings::GameBalance::Ranges::MediumArtilleryRange;
	using DeveloperSettings::GameBalance::Ranges::LightCannonRange;

	using DeveloperSettings::GameBalance::Weapons::MortarAOEMlt;

	const float PanzerSchreckTNTPerMM = 55.f / 88.f;
	const float PanzerSchreckArmorPenPerMM = 150.f / 88.f;
	const float PanzerSchreckArmorPenMaxRangePerMM = 100.f / 88.f;
	const float MortarTNTPerMM = 3400.f / 380.f;
	const float MortarArmorPenPerMM = 320.f / 380.f;
	const int32 PanzerwerferMagCapacity = 10;
	const float PanzerwerferReloadSpeed = 10.f;
	const float PanzerwerferBaseCooldown = 0.5f;
	const int32 MortarMagCapacity = 1;
	const float MortarBaseCooldown = 1.f;

	// PanzerSchreck (88mm; HEAT)
	WeaponData.WeaponName = EWeaponName::PanzerSchreck;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_HEAT;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_HEAT};
	WeaponData.WeaponCalibre = 88;
	WeaponData.TNTExplosiveGrams = 55;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = DeveloperSettings::GameBalance::Ranges::BasicSmallArmsRange;
	// Gets multiplied with 1.2 for heat rounds.
	WeaponData.ArmorPen = 150 * 0.8f;
	WeaponData.ArmorPenMaxRange = 100 * 0.8f;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 8;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 70;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = DeveloperSettings::GamePlay::Projectile::BaseProjectileSpeed * 0.9f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::PanzerSchreck, WeaponData);

	// Panzerwerfer (150mm; HEAT)
	WeaponData.WeaponName = EWeaponName::Panzerwerfer;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_HEAT;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_HEAT};
	WeaponData.WeaponCalibre = 150;
	WeaponData.TNTExplosiveGrams = PanzerSchreckTNTPerMM * WeaponData.WeaponCalibre;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = MediumArtilleryRange;
	WeaponData.ArmorPen = (PanzerSchreckArmorPenPerMM * WeaponData.WeaponCalibre)
		/ DeveloperSettings::GameBalance::Weapons::Projectiles::HEAT_ArmorPenMlt;
	WeaponData.ArmorPenMaxRange = (PanzerSchreckArmorPenMaxRangePerMM * WeaponData.WeaponCalibre)
		/ DeveloperSettings::GameBalance::Weapons::Projectiles::HEAT_ArmorPenMlt;
	WeaponData.MagCapacity = PanzerwerferMagCapacity;
	WeaponData.ReloadSpeed = PanzerwerferReloadSpeed;
	WeaponData.BaseCooldown = PanzerwerferBaseCooldown;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 70;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = DeveloperSettings::GamePlay::Projectile::BaseProjectileSpeed * 0.9f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Panzerwerfer, WeaponData);

	// Bazooka (50mm; HEAT)
	WeaponData.WeaponName = EWeaponName::Bazooka_50MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_HEAT;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_HEAT};
	WeaponData.WeaponCalibre = 50;
	WeaponData.TNTExplosiveGrams = 40;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = DeveloperSettings::GameBalance::Ranges::BasicSmallArmsRange;
	// Gets multiplied with 1.2 for heat rounds.
	WeaponData.ArmorPen = 110 * 0.8f;
	WeaponData.ArmorPenMaxRange = 80 * 0.8f;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 5;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 70;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = DeveloperSettings::GamePlay::Projectile::BaseProjectileSpeed * 0.9f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Bazooka_50MM, WeaponData);

	// https://wiki.warthunder.com/T-34-85
	// Cramped turret; stock reload.
	WeaponData.WeaponName = EWeaponName::ZIS_S_53_85MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 85;
	WeaponData.TNTExplosiveGrams = 88;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = HeavyCannonRange;
	WeaponData.ArmorPen = 143;
	WeaponData.ArmorPenMaxRange = 126;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 9.6f;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 80;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::ZIS_S_53_85MM, WeaponData);

	{
		// Faster reload for TD.
		const float Su85ReloadSpeedMlt = 0.67f;

		WeaponData.WeaponName = EWeaponName::D_5S_85MM_SU85;
		WeaponData.DamageType = ERTSDamageType::Kinetic;
		WeaponData.ShellType = EWeaponShellType::Shell_APHE;
		WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE};
		WeaponData.ReloadSpeed = 9.6f * Su85ReloadSpeedMlt;
		WeaponData.Accuracy = 85;
		M_TPlayerWeaponDataHashMap.Add(EWeaponName::D_5S_85MM_SU85, WeaponData);
	}

	// https://wiki.warthunder.com/Tiger_E
	WeaponData.WeaponName = EWeaponName::QF_37In_94MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE};
	// 3.7 inch gun is 94 mm 
	WeaponData.WeaponCalibre = 94;
	WeaponData.TNTExplosiveGrams = 95;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = HeavyCannonRange;
	WeaponData.ArmorPen = 162;
	WeaponData.ArmorPenMaxRange = 151;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 8;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 90;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = HighVelocityProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::QF_37In_94MM, WeaponData);

	// https://wiki.warthunder.com/IS-1#:~:text=...
	// Better reload than the zis variant and better explosive filler.	
	WeaponData.WeaponName = EWeaponName::DT_5_85MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 85;
	WeaponData.TNTExplosiveGrams = 92;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = HeavyCannonRange;
	WeaponData.ArmorPen = 143;
	WeaponData.ArmorPenMaxRange = 126;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 7.4f;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 90;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::DT_5_85MM, WeaponData);

	// https://wiki.warthunder.com/Tiger_E
	WeaponData.WeaponName = EWeaponName::KwK36_88MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {
		EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_APHEBC, EWeaponShellType::Shell_APCR,
		EWeaponShellType::Shell_HE, EWeaponShellType::Shell_HEAT
	};
	WeaponData.WeaponCalibre = 88;
	WeaponData.TNTExplosiveGrams = 109;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.Range = HeavyCannonRange;
	WeaponData.ArmorPen = 162;
	WeaponData.ArmorPenMaxRange = 151;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 8;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 90;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = HighVelocityProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK36_88MM, WeaponData);
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Kwk36_88MM_TigerH1, WeaponData);

	WeaponData.WeaponName = EWeaponName::Flak37_88MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_APCR, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 88;
	WeaponData.TNTExplosiveGrams = 109;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.Range = HeavyCannonRange;
	WeaponData.ArmorPen = 151;
	WeaponData.ArmorPenMaxRange = 151;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 5;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 90;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = HighVelocityProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Flak37_88MM, WeaponData);

	// https://wiki.warthunder.com/Jagdpanther_G1
	WeaponData.WeaponName = EWeaponName::Pak43_88MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 88;
	WeaponData.TNTExplosiveGrams = 109;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.Range = RTSFunctionLibrary::RoundToNearestMultipleOf( DeveloperSettings::GameBalance::Ranges::HeavyAssaultCannonRange* 1.2, 10);
	WeaponData.ArmorPen = 232;
	WeaponData.ArmorPenMaxRange = 222;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 7.5f;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 95;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = HighVelocityProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Pak43_88MM, WeaponData);

	// https://wiki.warthunder.com/Panther_II
	WeaponData.WeaponName = EWeaponName::KwK43_88MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_APCR, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 88;
	WeaponData.TNTExplosiveGrams = 109;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.Range = HeavyCannonRange;
	WeaponData.ArmorPen = 232;
	WeaponData.ArmorPenMaxRange = 222;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 8;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 95;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = HighVelocityProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK43_88MM, WeaponData);
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK43_88MM_PantherII, WeaponData);

	// https://wiki.warthunder.com/Tiger_II_(10.5_cm_Kw.K)
	// Cramped turret; stock reload.
	WeaponData.WeaponName = EWeaponName::KwKL_68_105MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_APCR, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 105;
	WeaponData.TNTExplosiveGrams = 343;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.Range = HeavyCannonRange;
	WeaponData.ArmorPen = 248;
	WeaponData.ArmorPenMaxRange = 234;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 16.2f;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 95;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = HighVelocityProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwKL_68_105MM, WeaponData);

	// LeFH 18 105mm howitzer
	WeaponData.WeaponName = EWeaponName::LeFH_18_105MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_HE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 105;
	WeaponData.TNTExplosiveGrams = 343;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = DeveloperSettings::GameBalance::Ranges::StaticArtilleryRange;
	WeaponData.ArmorPen = 46 / DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.ArmorPenMaxRange = 46 / DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.MagCapacity = 1;
	// WeaponData.ReloadSpeed = 16.2;
	WeaponData.ReloadSpeed = 3;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 25;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = 0.5f * HEProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::LeFH_18_105MM, WeaponData);

	// Cramped turret; stock reload.
	WeaponData.WeaponName = EWeaponName::sIG_33_150MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_HE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 150;
	WeaponData.TNTExplosiveGrams = 630;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = RTSFunctionLibrary::RoundToNearestMultipleOf(
		DeveloperSettings::GameBalance::Ranges::MediumArtilleryRange* 0.92, 100);
	// He is the only shell used for this gun but the projectile multiplies with this factor so we neutralize it.
	WeaponData.ArmorPen = 61 / DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.ArmorPenMaxRange = 61 / DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 14.4f;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 80;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = HEProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::sIG_33_150MM, WeaponData);

	// https://old-wiki.warthunder.com/Brummbar
	WeaponData.WeaponName = EWeaponName::Stu_H_43_L_12_150MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_HE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_HE, EWeaponShellType::Shell_HEAT};
	WeaponData.WeaponCalibre = 150;
	WeaponData.TNTExplosiveGrams = 630;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = RTSFunctionLibrary::RoundToNearestMultipleOf(
		DeveloperSettings::GameBalance::Ranges::HeavyArtilleryRange * 1.1, 100);
	// He is the only shell used for this gun but the projectile multiplies with this factor so we neutralize it.
	WeaponData.ArmorPen = 61 / DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.ArmorPenMaxRange = 61 / DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 17.2f;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 94;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = HEProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Stu_H_43_L_12_150MM, WeaponData);


	// Mortar 120mm (HE)
	WeaponData.WeaponName = EWeaponName::Mortar_120MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_HE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 120;
	WeaponData.TNTExplosiveGrams = MortarTNTPerMM * WeaponData.WeaponCalibre;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = RTSFunctionLibrary::RoundToNearestMultipleOf(MediumArtilleryRange, 100);
	WeaponData.ArmorPen = (MortarArmorPenPerMM * WeaponData.WeaponCalibre)
		/ DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.ArmorPenMaxRange = (MortarArmorPenPerMM * WeaponData.WeaponCalibre)
		/ DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.MagCapacity = MortarMagCapacity;
	WeaponData.ReloadSpeed = 12.f;
	WeaponData.BaseCooldown = MortarBaseCooldown;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = DeveloperSettings::GameBalance::Weapons::MortarAccuracy;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM * MortarAOEMlt;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = HEProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Mortar_120MM, WeaponData);

	// Mortar 80mm (HE)
	WeaponData.WeaponName = EWeaponName::Mortar_80MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_HE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 80;
	WeaponData.TNTExplosiveGrams = MortarTNTPerMM * WeaponData.WeaponCalibre;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = RTSFunctionLibrary::RoundToNearestMultipleOf(DeveloperSettings::GameBalance::Ranges::LightArtilleryRange, 100);
	WeaponData.ArmorPen = (MortarArmorPenPerMM * WeaponData.WeaponCalibre)
		/ DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.ArmorPenMaxRange = (MortarArmorPenPerMM * WeaponData.WeaponCalibre)
		/ DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.MagCapacity = MortarMagCapacity;
	WeaponData.ReloadSpeed = 10.f;
	WeaponData.BaseCooldown = MortarBaseCooldown;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = DeveloperSettings::GameBalance::Weapons::MortarAccuracy;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM * MortarAOEMlt;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = HEProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::Mortar_80MM, WeaponData);

	// https://old-wiki.warthunder.com/Brummbar
	WeaponData.WeaponName = EWeaponName::RW61_Mortar_380MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_HE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 380;
	WeaponData.TNTExplosiveGrams = 3400;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = RTSFunctionLibrary::RoundToNearestMultipleOf(
		DeveloperSettings::GameBalance::Ranges::HeavyArtilleryRange* 1.33, 100);
	// He is the only shell used for this gun but the projectile multiplies with this factor so we neutralize it.
	WeaponData.ArmorPen = 320 / DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.ArmorPenMaxRange = 320 / DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 30.f;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 80;
	WeaponData.ShrapnelRange = 700;
	WeaponData.ShrapnelDamage = 800;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM * 2;
	WeaponData.ShrapnelPen = 82;
	WeaponData.ProjectileMovementSpeed = HEProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::RW61_Mortar_380MM, WeaponData);

	// https://wiki.warthunder.com/Maus
	WeaponData.WeaponName = EWeaponName::KwK44_128MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 128;
	WeaponData.TNTExplosiveGrams = 786;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = HeavyCannonRange;
	WeaponData.ArmorPen = 269;
	WeaponData.ArmorPenMaxRange = 257;
	WeaponData.MagCapacity = 1;
	// WeaponData.ReloadSpeed = 18.2;
	WeaponData.ReloadSpeed = 10;
	WeaponData.BaseCooldown = 1;
	// WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.CooldownFlux = CooldownFluxPercentage * 2;
	WeaponData.Accuracy = 95;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = HighVelocityProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK44_128MM, WeaponData);
	// E100 Uses the same gun
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::KwK44_128MM_E100, WeaponData);

	// https://wiki.warthunder.com/T-34-100
	// Cramped turret; stock reload.
	WeaponData.WeaponName = EWeaponName::LB_1_100MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 100;
	WeaponData.TNTExplosiveGrams = 124;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = HeavyCannonRange;
	WeaponData.ArmorPen = 212;
	WeaponData.ArmorPenMaxRange = 189;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 15.0f;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 90;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 0.95f;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::LB_1_100MM, WeaponData);

	// Faster reload for TD.
	WeaponData.ReloadSpeed *= 0.67;
	WeaponData.WeaponName = EWeaponName::D_10S_100MM_SU100;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE};
	WeaponData.Accuracy = 95;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::D_10S_100MM_SU100, WeaponData);


	WeaponData.WeaponName = EWeaponName::ZIS_6_107MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 107;
	WeaponData.TNTExplosiveGrams = 144;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.DamageFlux = DamageFluxPercentage;
	WeaponData.Range = HeavyCannonRange;
	WeaponData.ArmorPen = 302;
	WeaponData.ArmorPenMaxRange = 280;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 9.0f;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 90;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::ZIS_6_107MM, WeaponData);

	WeaponData.WeaponName = EWeaponName::D_25T_122MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 122;
	WeaponData.TNTExplosiveGrams = 562;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.Range = HeavyCannonRange;
	WeaponData.ArmorPen = 201;
	WeaponData.ArmorPenMaxRange = 182;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 26;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 90;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 1;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::D_25T_122MM, WeaponData);

	{
		const float Su122HeArmorPen = 80.f;

		WeaponData.WeaponName = EWeaponName::M_30S_122MM_SU122;
		WeaponData.DamageType = ERTSDamageType::Kinetic;
		WeaponData.ShellType = EWeaponShellType::Shell_HE;
		WeaponData.ShellTypes = {EWeaponShellType::Shell_HE, EWeaponShellType::Shell_HEAT};
		WeaponData.WeaponCalibre = 122;
		WeaponData.TNTExplosiveGrams = 562;
		WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
			+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
		WeaponData.DamageFlux = DamageFluxPercentage;
		WeaponData.Range = RTSFunctionLibrary::RoundToNearestMultipleOf(
			DeveloperSettings::GameBalance::Ranges::MediumArtilleryRange* 1, 100);
		// He is the starting shell for this gun but the projectile multiplies with this factor so we neutralize it.
		WeaponData.ArmorPen = Su122HeArmorPen / DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
		WeaponData.ArmorPenMaxRange = Su122HeArmorPen /
			DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
		WeaponData.MagCapacity = 1;
		WeaponData.ReloadSpeed = 26;
		WeaponData.BaseCooldown = 1;
		WeaponData.CooldownFlux = CooldownFluxPercentage;
		WeaponData.Accuracy = 60;
		WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
		WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
		WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
		WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
		WeaponData.ProjectileMovementSpeed = HEProjectileSpeed;
		M_TPlayerWeaponDataHashMap.Add(EWeaponName::M_30S_122MM_SU122, WeaponData);
	}

	WeaponData.WeaponName = EWeaponName::D_25T_122MM_IS3;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_APHE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 122;
	WeaponData.TNTExplosiveGrams = 562;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
	WeaponData.Range = HeavyCannonRange;
	WeaponData.ArmorPen = 227;
	WeaponData.ArmorPenMaxRange = 215;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 26;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 90;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 1;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::D_25T_122MM_IS3, WeaponData);

	WeaponData.WeaponName = EWeaponName::M_10T_152MM;
	WeaponData.DamageType = ERTSDamageType::Kinetic;
	WeaponData.ShellType = EWeaponShellType::Shell_HE;
	WeaponData.ShellTypes = {EWeaponShellType::Shell_APHE, EWeaponShellType::Shell_HE};
	WeaponData.WeaponCalibre = 152;
	WeaponData.TNTExplosiveGrams = 1062;
	WeaponData.BaseDamage = DamagePerMM * WeaponData.WeaponCalibre
		+ WeaponData.TNTExplosiveGrams * DamagePerTNTEquivalentGrams;
		WeaponData.Range = RTSFunctionLibrary::RoundToNearestMultipleOf(
			DeveloperSettings::GameBalance::Ranges::HeavyArtilleryRange * 1.33, 100);
	// He is the only shell used for this gun but the projectile multiplies with this factor so we neutralize it.
	WeaponData.ArmorPen = 66 / DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.ArmorPenMaxRange = 66 / DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	WeaponData.MagCapacity = 1;
	WeaponData.ReloadSpeed = 33.0f;
	WeaponData.BaseCooldown = 1;
	WeaponData.CooldownFlux = CooldownFluxPercentage;
	WeaponData.Accuracy = 80;
	WeaponData.ShrapnelRange = WeaponData.WeaponCalibre * ShrapnelRangePerMM;
	WeaponData.ShrapnelDamage = WeaponData.TNTExplosiveGrams * ShrapnelDamagePerTNTGram;
	WeaponData.ShrapnelParticles = WeaponData.WeaponCalibre * ShrapnelAmountPerMM;
	WeaponData.ShrapnelPen = WeaponData.WeaponCalibre * ShrapnelPenPerMM;
	WeaponData.ProjectileMovementSpeed = BaseProjectileSpeed * 1;
	M_TPlayerWeaponDataHashMap.Add(EWeaponName::M_10T_152MM, WeaponData);
}

TArray<FBxpOptionData> ACPPGameState::InitBxpOptions(
	const TArray<TPair<EBuildingExpansionType, EBxpOptionSection>>& TypedOptions) const
{
	TArray<FBxpOptionData> Options;
	Options.Reserve(TypedOptions.Num());
	for (const TPair<EBuildingExpansionType, EBxpOptionSection>& Each : TypedOptions)
	{
		Options.Add(InitBxpOptionEntry(Each.Key, Each.Value));
	}
	return Options;
}


FBxpOptionData ACPPGameState::InitBxpOptionEntry(const EBuildingExpansionType Type,
                                                 const EBxpOptionSection Section) const
{
	FBxpConstructionRules ConstructionRules = BxpConstructionRules::FreeConstructionRules;
	if (not BxpConstructionRules::TGameBxpConstructionRules.Contains(Type))
	{
		RTSFunctionLibrary::ReportError("No construction rules for BXP type: " + UEnum::GetValueAsString(Type));
	}
	else
	{
		ConstructionRules = BxpConstructionRules::TGameBxpConstructionRules[Type];
	}

	FBxpOptionData Data;
	Data.BxpConstructionRules = ConstructionRules;
	Data.ExpansionType = Type;
	Data.Section = Section;
	return Data;
}


void ACPPGameState::InitAllGameDigInData()
{
	using namespace DeveloperSettings::GameBalance::UnitHealth;
	using namespace DeveloperSettings::GameBalance::BuildingTime;
	FDigInData DigInData;

	DigInData.DigInTime = LightTankDigInWallBuildingTime;
	DigInData.DigInWallHealth = LightTankDigInWallHealth;
	M_TPlayerDigInDataHashMap.Add(EDigInType::GerLightTankWall, DigInData);
	M_TPlayerDigInDataHashMap.Add(EDigInType::RusLightTankWall, DigInData);

	DigInData.DigInTime = MediumTankDigInWallBuildingTime;
	DigInData.DigInWallHealth = MediumTankDigInWallHealth;
	M_TPlayerDigInDataHashMap.Add(EDigInType::GerMediumTankWall, DigInData);
	M_TPlayerDigInDataHashMap.Add(EDigInType::RusMediumTankWall, DigInData);

	DigInData.DigInTime = HeavyTankDigInWallBuildingTime;
	DigInData.DigInWallHealth = HeavyTankDigInWallHealth;
	M_TPlayerDigInDataHashMap.Add(EDigInType::GerHeavyTankWall, DigInData);
	M_TPlayerDigInDataHashMap.Add(EDigInType::RusHeavyTankWall, DigInData);

	M_TEnemyDigInDataHashMap = M_TPlayerDigInDataHashMap;
}

void ACPPGameState::InitAllGameRocketData()
{
	using DeveloperSettings::GameBalance::Weapons::DamageFluxPercentage;
	using DeveloperSettings::GameBalance::Weapons::ArmorPenFluxPercentage;
	using DeveloperSettings::GameBalance::Weapons::CooldownFluxPercentage;

	FAttachedRocketsData RocketData;

	RocketData.DamageType = ERTSDamageType::Kinetic;
	RocketData.Cooldown = 2.5f;
	RocketData.Damage = 300.f;
	RocketData.Range = 10000.f;
	RocketData.ArmorPen = 110.f;
	RocketData.ProjectileSpeed = 5000.f;
	RocketData.ProjectileAccelerationFactor = 1.f;
	RocketData.ReloadSpeed = 30.f;
	RocketData.Accuracy = 80.f;
	RocketData.DamageFluxMlt = DamageFluxPercentage / 100;
	RocketData.CoolDownFluxMlt = CooldownFluxPercentage / 100;
	RocketData.ArmorPenFluxMlt = ArmorPenFluxPercentage / 100;
	RocketData.WeaponCalibre = 88;
	M_TPlayerAttachedRocketDataHashMap.Add(ERocketAbility::HetzerRockets, RocketData);


	RocketData.DamageType = ERTSDamageType::Kinetic;
	RocketData.Cooldown = 0.33f;
	RocketData.Damage = 150.f;
	RocketData.Range = 10000.f;
	RocketData.ArmorPen = 100.f;
	RocketData.ProjectileSpeed = 5000.f;
	RocketData.ProjectileAccelerationFactor = 2.f;
	RocketData.ReloadSpeed = 40.f;
	RocketData.Accuracy = 60.f;
	RocketData.DamageFluxMlt = DamageFluxPercentage / 100;
	RocketData.CoolDownFluxMlt = CooldownFluxPercentage / 100;
	RocketData.ArmorPenFluxMlt = ArmorPenFluxPercentage / 100;
	RocketData.WeaponCalibre = 50;
	M_TPlayerAttachedRocketDataHashMap.Add(ERocketAbility::JagdPantherRockets, RocketData);

	M_EnemyAttachedRocketDataHashMap = M_TPlayerAttachedRocketDataHashMap;
}

void ACPPGameState::InitAllGameTankData()
{
	// If you may call this multiple times, preventing duplicates is wise:
	M_TPlayerTankDataHashMap.Reset();
	M_TEnemyTankDataHashMap.Reset();

	InitAllGameArmoredCarData();
	InitAllGameLightTankData();
	InitAllGameMediumTankData();
	InitAllGameHeavyTankData();

	// VERY IMPORTANT; SET ENEMY DATA TOO.
	M_TEnemyTankDataHashMap = M_TPlayerTankDataHashMap;
}

// -----------------------------------------------------------------------------
// Armored Cars
// -----------------------------------------------------------------------------
void ACPPGameState::InitAllGameArmoredCarData()
{
	// --- bring developer settings into scope via using (no local constexpr mirrors)
	using namespace DeveloperSettings::GameBalance::UnitCosts;
	using namespace DeveloperSettings::GameBalance::VisionRadii::UnitVision;
	using namespace DeveloperSettings::GameBalance::Experience;

	using DeveloperSettings::GameBalance::UnitHealth::LightTankHealthBase;
	using DeveloperSettings::GameBalance::UnitHealth::ArmoredCarHealthBase;

	// Abilities 
	const TArray<FUnitAbilityEntry> BasicTankAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdReverseMove,
		EAbilityID::IdNoAbility,
		EAbilityID::IdDigIn, EAbilityID::IdRotateTowards, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility
	});
	const TArray<FUnitAbilityEntry> TankAbilitiesWithRockets = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdReverseMove,
		EAbilityID::IdNoAbility,
		EAbilityID::IdDigIn, EAbilityID::IdRotateTowards, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdFireRockets, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility
	});

	const TArray<FUnitAbilityEntry> BasicHarvesterTankAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdReverseMove,
		EAbilityID::IdNoAbility,
		EAbilityID::IdHarvestResource, EAbilityID::IdRotateTowards, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility
	});

	FTankData TankData;

	// ------------------------------- GER Armored Cars -------------------------------

	// Puma
	TankData.MaxHealth = LightTankHealthBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 30;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 35;
	TankData.VehicleReverseSpeedKmh = 35;
	TankData.VisionRadius = ArmoredCarVisionRadius;
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseArmoredCarExp * 1.2f, 5);
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, ArmoredCarMediumCalibreRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, ArmoredCarMediumCalibreVehiclePartsCost}
	});
	TankData.ExperienceLevels = GetArmoredCarExpLevels();
	TankData.ExperienceMultiplier = 1.0f;
	TankData.Abilities = BasicTankAbilities;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Puma, TankData);

	// Panzerwerfer
	{
		const float PanzerwerferRadixiteCostMlt = 1.3f;
		const float PanzerwerferVehiclePartsCostMlt = 1.8f;

		TankData.MaxHealth = LightTankHealthBase;
		TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(TankData.MaxHealth);
		TankData.VehicleRotationSpeed = 30;
		TankData.TurretRotationSpeed = 14;
		TankData.VehicleMaxSpeedKmh = 35;
		TankData.VehicleReverseSpeedKmh = 35;
		TankData.VisionRadius = ArmoredCarVisionRadius;
		TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseArmoredCarExp * 1.2f, 5);
		TankData.Cost = FUnitCost({
			{ERTSResourceType::Resource_Radixite, FMath::RoundToInt(
				 ArmoredCarMediumCalibreRadixiteCost * PanzerwerferRadixiteCostMlt)},
			{ERTSResourceType::Resource_VehicleParts, FMath::RoundToInt(
				 ArmoredCarMediumCalibreVehiclePartsCost * PanzerwerferVehiclePartsCostMlt)}
		});
		TankData.ExperienceLevels = GetArmoredCarExpLevels();
		TankData.ExperienceMultiplier = 1.0f;
		TankData.Abilities = TankAbilitiesWithRockets;
		M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Panzerwerfer, TankData);
	}

	// Sd.Kfz. 232/3
	TankData.MaxHealth = LightTankHealthBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 30;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 35;
	TankData.VehicleReverseSpeedKmh = 35;
	TankData.VisionRadius = ArmoredCarVisionRadius;
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseArmoredCarExp * 1.2f, 5);
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, ArmoredCarMediumCalibreRadixiteCost + 50},
		{ERTSResourceType::Resource_VehicleParts, ArmoredCarMediumCalibreVehiclePartsCost + 25}
	});
	TankData.ExperienceLevels = GetArmoredCarExpLevels();
	TankData.ExperienceMultiplier = 1.0f;
	TankData.Abilities = BasicTankAbilities;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Sdkfz_232_3, TankData);

	// Sd.Kfz. 250
	TankData.MaxHealth = ArmoredCarHealthBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 25;
	TankData.TurretRotationSpeed = 40;
	TankData.VehicleMaxSpeedKmh = 20;
	TankData.VehicleReverseSpeedKmh = 15;
	TankData.VisionRadius = ArmoredCarVisionRadius;
	TankData.ExperienceWorth = BaseArmoredCarExp;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, ArmoredCarRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, ArmoredCarVehiclePartsCost}
	});
	TankData.ExperienceLevels = GetArmoredCarExpLevels();
	TankData.ExperienceMultiplier = 1.0f;
	TankData.Abilities = BasicTankAbilities;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Sdkfz250, TankData);

	// Sd.Kfz. 251/22
	TankData.MaxHealth = ArmoredCarHealthBase + 150;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 25;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 20;
	TankData.VehicleReverseSpeedKmh = 15;
	TankData.VisionRadius = ArmoredCarVisionRadius;
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseArmoredCarExp * 1.1f, 5);
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, ArmoredCarRadixiteCost + 125},
		{ERTSResourceType::Resource_VehicleParts, ArmoredCarVehiclePartsCost + 50}
	});
	TankData.ExperienceLevels = GetArmoredCarExpLevels();
	TankData.ExperienceMultiplier = 1.0f;
	TankData.Abilities = BasicTankAbilities;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Sdkfz251_22, TankData);

	// Sd.Kfz. 251 (Mortar)
	{
		const float MortarRadixiteCostMlt = 1.45f;
		const float MortarVehiclePartsCostMlt = 1.33f;

		TankData.MaxHealth = ArmoredCarHealthBase + 150;
		TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(TankData.MaxHealth);
		TankData.VehicleRotationSpeed = 25;
		TankData.TurretRotationSpeed = 14;
		TankData.VehicleMaxSpeedKmh = 20;
		TankData.VehicleReverseSpeedKmh = 15;
		TankData.VisionRadius = ArmoredCarVisionRadius;
		TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseArmoredCarExp * 1.1f, 5);
		TankData.Cost = FUnitCost({
			{ERTSResourceType::Resource_Radixite, RTSFunctionLibrary::RoundToNearestMultipleOf(
				FMath::RoundToInt(ArmoredCarRadixiteCost * MortarRadixiteCostMlt),
				10)},
			{ERTSResourceType::Resource_VehicleParts, FMath::RoundToInt(
				ArmoredCarVehiclePartsCost * MortarVehiclePartsCostMlt)}
		});
		TankData.ExperienceLevels = GetArmoredCarExpLevels();
		TankData.ExperienceMultiplier = 1.0f;
		TankData.Abilities = BasicTankAbilities;
		M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Sdkfz251_Mortar, TankData);
	}

	// Sd.Kfz. 251 (Transport)
	{
		const float TransportRadixiteCostMlt = 1.3f;

		TankData.MaxHealth = ArmoredCarHealthBase + 150;
		TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(TankData.MaxHealth);
		TankData.VehicleRotationSpeed = 25;
		TankData.TurretRotationSpeed = 14;
		TankData.VehicleMaxSpeedKmh = 20;
		TankData.VehicleReverseSpeedKmh = 15;
		TankData.VisionRadius = ArmoredCarVisionRadius;
		TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseArmoredCarExp * 1.1f, 5);
		TankData.Cost = FUnitCost({
			{ERTSResourceType::Resource_Radixite, FMath::RoundToInt(ArmoredCarRadixiteCost * TransportRadixiteCostMlt)},
			{ERTSResourceType::Resource_VehicleParts, ArmoredCarVehiclePartsCost}
		});
		TankData.ExperienceLevels = GetArmoredCarExpLevels();
		TankData.ExperienceMultiplier = 1.0f;
		TankData.Abilities = BasicTankAbilities;
		M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Sdkfz251_Transport, TankData);
	}

	// Sd.Kfz. 231
	TankData.MaxHealth = ArmoredCarHealthBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 25;
	TankData.TurretRotationSpeed = 40;
	TankData.VehicleMaxSpeedKmh = 20;
	TankData.VehicleReverseSpeedKmh = 15;
	TankData.VisionRadius = ArmoredCarVisionRadius;
	TankData.ExperienceWorth = BaseArmoredCarExp;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, ArmoredCarRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, ArmoredCarVehiclePartsCost}
	});
	TankData.ExperienceLevels = GetArmoredCarExpLevels();
	TankData.ExperienceMultiplier = 1.0f;
	TankData.Abilities = BasicTankAbilities;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Sdkfz_231, TankData);

	// Sd.Kfz. 251 (Pz IV)
	TankData.MaxHealth = ArmoredCarHealthBase + 50;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 30;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 25;
	TankData.VehicleReverseSpeedKmh = 20;
	TankData.VisionRadius = ArmoredCarVisionRadius;
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseArmoredCarExp * 1.3f, 5);
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, ArmoredCarMediumCalibreRadixiteCost - 50},
		{ERTSResourceType::Resource_VehicleParts, ArmoredCarMediumCalibreVehiclePartsCost - 15}
	});
	TankData.ExperienceLevels = GetArmoredCarExpLevels();
	TankData.ExperienceMultiplier = 1.0f;
	TankData.Abilities = BasicTankAbilities;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Sdkfz251_PZIV, TankData);


	// ------------------------------- RUS Armored Cars -------------------------------

	// BA-12 
	TankData.MaxHealth = LightTankHealthBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 30;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 35;
	TankData.VehicleReverseSpeedKmh = 35;
	TankData.VisionRadius = ArmoredCarVisionRadius;
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseArmoredCarExp * 1.2f, 5);
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, ArmoredCarMediumCalibreRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, ArmoredCarMediumCalibreVehiclePartsCost}
	});
	TankData.ExperienceLevels = GetArmoredCarExpLevels();
	TankData.ExperienceMultiplier = 1.0f;
	TankData.Abilities = BasicTankAbilities;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Ba12, TankData);

	// BA-14 
	TankData.MaxHealth = LightTankHealthBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 30;
	TankData.TurretRotationSpeed = 30;
	TankData.VehicleMaxSpeedKmh = 35;
	TankData.VehicleReverseSpeedKmh = 35;
	TankData.VisionRadius = ArmoredCarVisionRadius;
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseArmoredCarExp * 1.2f, 5);
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, ArmoredCarMediumCalibreRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, ArmoredCarMediumCalibreVehiclePartsCost}
	});
	TankData.ExperienceLevels = GetArmoredCarExpLevels();
	TankData.ExperienceMultiplier = 1.0f;
	TankData.Abilities = BasicTankAbilities;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Ba14, TankData);
}

void ACPPGameState::InitAllGameLightTankData()
{
	using namespace DeveloperSettings::GameBalance::UnitCosts;
	using namespace DeveloperSettings::GameBalance::VisionRadii::UnitVision;
	using namespace DeveloperSettings::GameBalance::Experience;
	using namespace DeveloperSettings::GameBalance::UnitHealth;

	const TArray<FUnitAbilityEntry> BasicTankAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdReverseMove,
		EAbilityID::IdNoAbility,
		EAbilityID::IdDigIn, EAbilityID::IdRotateTowards, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility
	});
	const TArray<FUnitAbilityEntry> TankAbilitiesWithRockets = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdReverseMove,
		EAbilityID::IdNoAbility,
		EAbilityID::IdDigIn, EAbilityID::IdRotateTowards, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdFireRockets, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
	});
	const TArray<FUnitAbilityEntry> BasicHarvesterTankAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdReverseMove,
		EAbilityID::IdNoAbility,
		EAbilityID::IdHarvestResource, EAbilityID::IdRotateTowards, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
	});
	const TArray<FUnitAbilityEntry> BasicTankDestroyerAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdReverseMove,
		EAbilityID::IdNoAbility,
		EAbilityID::IdDigIn, EAbilityID::IdRotateTowards, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
	});

	FTankData TankData;

	// ------------------------------- GER Light Tanks -------------------------------

	// Pz 38(t)
	TankData.MaxHealth = LightTankHealthBase + OneLightTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetILightArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 20;
	TankData.VehicleReverseSpeedKmh = 8;
	TankData.VisionRadius = T1TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightTankRadixiteCost + 50},
		{ERTSResourceType::Resource_VehicleParts, LightTankVehiclePartsCost}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetLightTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseLightTankExp * 1.2f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Pz38t, TankData);

	// Pz 38(t) R
	{
		const float Panzer38TRRadixiteCostMlt = 1.5f;
		const float Panzer38TRVehiclePartsCostMlt = 1.8f;
		const int32 Panzer38TBaseRadixiteCost = LightTankRadixiteCost + 50;
		const int32 Panzer38TBaseVehiclePartsCost = LightTankVehiclePartsCost;

		TankData.MaxHealth = LightTankHealthBase + OneLightTankShotHp;
		TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetILightArmorResistances(TankData.MaxHealth);
		TankData.VehicleRotationSpeed = 40;
		TankData.TurretRotationSpeed = 30;
		TankData.VehicleMaxSpeedKmh = 20;
		TankData.VehicleReverseSpeedKmh = 8;
		TankData.VisionRadius = T1TankVisionRadius;
		TankData.Cost = FUnitCost({
			{ERTSResourceType::Resource_Radixite, FMath::RoundToInt(
				 Panzer38TBaseRadixiteCost * Panzer38TRRadixiteCostMlt)},
			{ERTSResourceType::Resource_VehicleParts, FMath::RoundToInt(
				 Panzer38TBaseVehiclePartsCost * Panzer38TRVehiclePartsCostMlt)}
		});
		TankData.Abilities = BasicTankAbilities;
		TankData.ExperienceLevels = GetLightTankExpLevels();
		TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseLightTankExp * 1.2f, 5);
		TankData.ExperienceMultiplier = 1.0f;
		M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Pz38t_R, TankData);
	}

	// Pz II F
	TankData.MaxHealth = LightTankHealthBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetILightArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 20;
	TankData.VehicleReverseSpeedKmh = 7;
	TankData.VisionRadius = T1TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightTankRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, LightTankVehiclePartsCost}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetLightTankExpLevels();
	TankData.ExperienceWorth = BaseLightTankExp;
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PzII_F, TankData);

	// Pz I Harvester
	TankData.MaxHealth = LightTankHealthBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetILightArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 25;
	TankData.VehicleReverseSpeedKmh = 15;
	TankData.VisionRadius = T1TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightTankRadixiteCost - 25},
		{ERTSResourceType::Resource_VehicleParts, LightTankVehiclePartsCost - 25}
	});
	TankData.Abilities = BasicHarvesterTankAbilities;
	TankData.ExperienceLevels = GetLightTankExpLevels();
	TankData.ExperienceWorth = BaseLightTankExp;
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PzI_Harvester, TankData);

	// Sd.Kfz. 140 (open top)
	TankData.MaxHealth = LightTankHealthBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetILightArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 40;
	TankData.VehicleMaxSpeedKmh = 25;
	TankData.VehicleReverseSpeedKmh = 11;
	TankData.VisionRadius = OpenTopVehicleVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightTankRadixiteCost + 25},
		{ERTSResourceType::Resource_VehicleParts, LightTankVehiclePartsCost + 20}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetLightTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseLightTankExp * 1.25f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Sdkfz_140, TankData);

	// Pz Jäger (light TD)
	TankData.MaxHealth = LightTankHealthBase - OneLightTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 25;
	TankData.TurretRotationSpeed = 12;
	TankData.VehicleMaxSpeedKmh = 18;
	TankData.VehicleReverseSpeedKmh = 6;
	TankData.VisionRadius = OpenTopVehicleVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightTankDestroyerRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, LightTankDestroyerVehiclePartsCost}
	});
	TankData.Abilities = BasicTankDestroyerAbilities;
	TankData.ExperienceLevels = GetLightTankDestroyerExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseLightTankExp * 1.1f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PzJager, TankData);

	// Marder (light TD)
	TankData.MaxHealth = LightMediumTankBase - OneLightTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 25;
	TankData.TurretRotationSpeed = 12;
	TankData.VehicleMaxSpeedKmh = 20;
	TankData.VehicleReverseSpeedKmh = 8;
	TankData.VisionRadius = OpenTopVehicleVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightTankDestroyerRadixiteCost + 80},
		{ERTSResourceType::Resource_VehicleParts, LightTankDestroyerVehiclePartsCost + 50}
	});
	TankData.Abilities = BasicTankDestroyerAbilities;
	TankData.ExperienceLevels = GetLightTankDestroyerExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseLightTankExp * 1.25f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Marder, TankData);

	// Pz I 15cm (SPG)
	TankData.MaxHealth = LightTankHealthBase - OneLightTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 6;
	TankData.VehicleMaxSpeedKmh = 18;
	TankData.VehicleReverseSpeedKmh = 6;
	TankData.VisionRadius = OpenTopVehicleVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightTankDestroyerRadixiteCost + 25},
		{ERTSResourceType::Resource_VehicleParts, LightTankDestroyerVehiclePartsCost + 40}
	});
	TankData.Abilities = BasicTankDestroyerAbilities;
	TankData.ExperienceLevels = GetLightTankDestroyerExpLevels();
	TankData.ExperienceWorth = BaseLightTankExp;
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PzI_15cm, TankData);

	// Hetzer (light TD with rockets ability per original)
	TankData.MaxHealth = LightMediumTankBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 6;
	TankData.VehicleMaxSpeedKmh = 25;
	TankData.VehicleReverseSpeedKmh = 8;
	TankData.VisionRadius = T1TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightTankDestroyerRadixiteCost + 100},
		{ERTSResourceType::Resource_VehicleParts, LightTankDestroyerVehiclePartsCost + 70}
	});
	TankData.Abilities = TankAbilitiesWithRockets;
	TankData.ExperienceLevels = GetLightTankDestroyerExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseLightTankExp * 1.25f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Hetzer, TankData);

	// ------------------------------- RUS Light Tanks -------------------------------

	// BT-7
	TankData.MaxHealth = LightTankHealthBase - OneLightTankShotHp - 50;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetILightArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 16;
	TankData.VehicleMaxSpeedKmh = 50;
	TankData.VehicleReverseSpeedKmh = 30;
	TankData.VisionRadius = T1TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightTankRadixiteCost - 50},
		{ERTSResourceType::Resource_VehicleParts, LightTankVehiclePartsCost - 10}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetLightTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseLightTankExp * 0.9f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_BT7, TankData);

	// BT-7 (4)
	TankData.MaxHealth = LightTankHealthBase - OneLightTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetILightArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 16;
	TankData.VehicleMaxSpeedKmh = 50;
	TankData.VehicleReverseSpeedKmh = 30;
	TankData.VisionRadius = T1TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightTankRadixiteCost - 25},
		{ERTSResourceType::Resource_VehicleParts, LightTankVehiclePartsCost}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetLightTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseLightTankExp * 0.9f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_BT7_4, TankData);

	// T-26
	TankData.MaxHealth = LightTankHealthBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetILightArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 35;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 15;
	TankData.VehicleReverseSpeedKmh = 7;
	TankData.VisionRadius = T1TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightTankRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, LightTankVehiclePartsCost}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetLightTankExpLevels();
	TankData.ExperienceWorth = BaseLightTankExp;
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_T26, TankData);

	// T-70
	TankData.MaxHealth = LightTankHealthBase + OneLightTankShotHp + 25;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetILightArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 15;
	TankData.VehicleMaxSpeedKmh = 20;
	TankData.VehicleReverseSpeedKmh = 9;
	TankData.VisionRadius = T1TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightTankRadixiteCost + 50},
		{ERTSResourceType::Resource_VehicleParts, LightTankVehiclePartsCost + 25}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetLightTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseLightTankExp * 1.1f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_T70, TankData);

	// SU-76 (light TD)
	{
		const float Su76HealthMlt = 1.2f;
		const float Su76Health = RTSFunctionLibrary::RoundToNearestMultipleOf(LightTankHealthBase * Su76HealthMlt, 10);
		const float Su76VehicleRotationSpeed = 50.f;
		const float Su76TurretRotationSpeed = 12.f;
		const float Su76MaxSpeedKmh = 25.f;
		const float Su76ReverseSpeedKmh = 10.f;
		const int32 Su76RadixiteCostOffset = 50;
		const int32 Su76VehiclePartsCostOffset = 40;
		const float Su76ExpWorthMlt = 1.2f;

		TankData.MaxHealth = Su76Health;
		TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetILightArmorResistances(TankData.MaxHealth);
		TankData.VehicleRotationSpeed = Su76VehicleRotationSpeed;
		TankData.TurretRotationSpeed = Su76TurretRotationSpeed;
		TankData.VehicleMaxSpeedKmh = Su76MaxSpeedKmh;
		TankData.VehicleReverseSpeedKmh = Su76ReverseSpeedKmh;
		TankData.VisionRadius = OpenTopVehicleVisionRadius;
		TankData.Cost = FUnitCost({
			{ERTSResourceType::Resource_Radixite, LightTankDestroyerRadixiteCost + Su76RadixiteCostOffset},
			{ERTSResourceType::Resource_VehicleParts, LightTankDestroyerVehiclePartsCost + Su76VehiclePartsCostOffset}
		});
		TankData.Abilities = BasicTankDestroyerAbilities;
		TankData.ExperienceLevels = GetLightTankDestroyerExpLevels();
		TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseLightTankExp * Su76ExpWorthMlt, 5);
		TankData.ExperienceMultiplier = 1.0f;
		M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_SU_76, TankData);
	}
}

void ACPPGameState::InitAllGameMediumTankData()
{
	using namespace DeveloperSettings::GameBalance::UnitCosts;
	using namespace DeveloperSettings::GameBalance::VisionRadii::UnitVision;
	using namespace DeveloperSettings::GameBalance::Experience;

	using DeveloperSettings::GameBalance::UnitHealth::LightMediumTankBase;
	using DeveloperSettings::GameBalance::UnitHealth::MediumTankHealthBase;
	using DeveloperSettings::GameBalance::UnitHealth::T3MediumTankBase;
	using DeveloperSettings::GameBalance::UnitHealth::OneLightTankShotHp;
	using DeveloperSettings::GameBalance::UnitHealth::OneMediumTankShotHp;

	const TArray<FUnitAbilityEntry> BasicTankAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdReverseMove,
		EAbilityID::IdNoAbility,
		EAbilityID::IdDigIn, EAbilityID::IdRotateTowards, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
	});
	const TArray<FUnitAbilityEntry> TankAbilitiesWithRockets = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdReverseMove,
		EAbilityID::IdNoAbility,
		EAbilityID::IdDigIn, EAbilityID::IdRotateTowards, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdFireRockets, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
	});
	const TArray<FUnitAbilityEntry> BasicTankDestroyerAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdReverseMove,
		EAbilityID::IdNoAbility,
		EAbilityID::IdDigIn, EAbilityID::IdRotateTowards, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
	});

	FTankData TankData;

	// ------------------------------- GER Medium Tanks -------------------------------

	// Pz III J
	TankData.MaxHealth = LightMediumTankBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetILightArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 25;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 20;
	TankData.VehicleReverseSpeedKmh = 8;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightMediumTankRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, LightMediumTankVehiclePartsCost}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetLightMediumTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseMediumTankExp * 0.9f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PzIII_J, TankData);

	// Pz III AA
	TankData.MaxHealth = LightMediumTankBase + 50;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetILightArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 25;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 20;
	TankData.VehicleReverseSpeedKmh = 8;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightMediumTankRadixiteCost + 100},
		{ERTSResourceType::Resource_VehicleParts, LightMediumTankVehiclePartsCost + 150}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetLightMediumTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseMediumTankExp * 0.9f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PzIII_AA, TankData);

	// Pz III J Commander (rockets)
	TankData.MaxHealth = LightMediumTankBase + 150;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 25;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 20;
	TankData.VehicleReverseSpeedKmh = 8;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightMediumTankRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, LightMediumTankVehiclePartsCost}
	});
	TankData.Abilities = TankAbilitiesWithRockets;
	TankData.ExperienceLevels = GetLightMediumTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseMediumTankExp * 1.5f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PzIII_J_Commander, TankData);

	// Pz III M
	TankData.MaxHealth = MediumTankHealthBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 25;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 20;
	TankData.VehicleReverseSpeedKmh = 8;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, MediumTankRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, MediumTankVehiclePartsCost}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetLightMediumTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseMediumTankExp * 1.f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PzIII_M, TankData);


	// Pz III FLAMM
	TankData.MaxHealth = MediumTankHealthBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 25;
	TankData.TurretRotationSpeed = 20;
	TankData.VehicleMaxSpeedKmh = 20;
	TankData.VehicleReverseSpeedKmh = 8;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, MediumTankRadixiteCost + 200},
		{ERTSResourceType::Resource_VehicleParts, MediumTankVehiclePartsCost + 100}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetLightMediumTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseMediumTankExp * 0.95f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PzIII_FLamm, TankData);

	// Pz IV G
	TankData.MaxHealth = MediumTankHealthBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 26;
	TankData.VehicleReverseSpeedKmh = 15;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, MediumTankRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, MediumTankVehiclePartsCost}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetMediumTankExpLevels();
	TankData.ExperienceWorth = BaseMediumTankExp;
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PzIV_G, TankData);

	// Pz IV F1
	TankData.MaxHealth = MediumTankHealthBase - 150;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetILightArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 26;
	TankData.VehicleReverseSpeedKmh = 15;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, MediumTankRadixiteCost - 125},
		{ERTSResourceType::Resource_VehicleParts, MediumTankVehiclePartsCost - 50}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetMediumTankExpLevels();
	TankData.ExperienceWorth = BxpHelpers::RoundToNearestMultipleOfFive(BaseMediumTankExp * 0.8f);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PzIV_F1, TankData);

	// Pz IV F1 Commander
	TankData.MaxHealth = MediumTankHealthBase - 150;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 26;
	TankData.VehicleReverseSpeedKmh = 15;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, MediumTankRadixiteCost - 125},
		{ERTSResourceType::Resource_VehicleParts, MediumTankVehiclePartsCost - 50}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetMediumTankExpLevels();
	TankData.ExperienceWorth = BxpHelpers::RoundToNearestMultipleOfFive(BaseMediumTankExp * 1.5f);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PzIV_F1_Commander, TankData);

	// Jaguar 
	TankData.MaxHealth = RTSFunctionLibrary::RoundToNearestMultipleOf(MediumTankHealthBase * 1.2f, 10);
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 55;
	TankData.TurretRotationSpeed = 35;
	TankData.VehicleMaxSpeedKmh = 35;
	TankData.VehicleReverseSpeedKmh = 20;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, MediumTankRadixiteCost + 150},
		{ERTSResourceType::Resource_VehicleParts, MediumTankVehiclePartsCost + 100}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetMediumTankExpLevels();
	TankData.ExperienceWorth = BaseMediumTankExp;
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Jaguar, TankData);

	// Brummbär
	TankData.MaxHealth = MediumTankHealthBase + 400;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 20;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 18;
	TankData.VehicleReverseSpeedKmh = 10;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, MediumTankRadixiteCost + 150},
		{ERTSResourceType::Resource_VehicleParts, MediumTankVehiclePartsCost + 200}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetMediumTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseMediumTankExp * 1.5f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Brumbar, TankData);

	// StuG (medium TD) 
	TankData.MaxHealth = MediumTankHealthBase - OneLightTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 25;
	TankData.TurretRotationSpeed = 15;
	TankData.VehicleMaxSpeedKmh = 22;
	TankData.VehicleReverseSpeedKmh = 12;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, MediumTankDestroyerRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, MediumTankDestroyerVehiclePartsCost}
	});
	TankData.Abilities = BasicTankDestroyerAbilities;
	TankData.ExperienceLevels = GetMediumTankDestroyerExpLevels();
	TankData.ExperienceWorth = BaseMediumTankExp;
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Stug, TankData);

	// ------------------------------- RUS Medium Tanks -------------------------------

	// T-34/76
	TankData.MaxHealth = MediumTankHealthBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 49;
	TankData.TurretRotationSpeed = 25;
	TankData.VehicleMaxSpeedKmh = 26;
	TankData.VehicleReverseSpeedKmh = 18;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, MediumTankRadixiteCost - 50},
		{ERTSResourceType::Resource_VehicleParts, MediumTankVehiclePartsCost - 10}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetMediumTankExpLevels();
	TankData.ExperienceWorth = BaseMediumTankExp;
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_T34_76, TankData);

	// T-34/76L
	{
		const int32 T34_76_L_RadixiteCostOffset = 25;
		const int32 T34_76_L_VehiclePartsCostOffset = 120;
		const float T34_76_L_ExpWorthMlt = 1.1f;

		TankData.MaxHealth = MediumTankHealthBase;
		TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
		TankData.VehicleRotationSpeed = 49;
		TankData.TurretRotationSpeed = 25;
		TankData.VehicleMaxSpeedKmh = 26;
		TankData.VehicleReverseSpeedKmh = 18;
		TankData.VisionRadius = T2TankVisionRadius;
		TankData.Cost = FUnitCost({
			{ERTSResourceType::Resource_Radixite, MediumTankRadixiteCost + T34_76_L_RadixiteCostOffset},
			{ERTSResourceType::Resource_VehicleParts, MediumTankVehiclePartsCost + T34_76_L_VehiclePartsCostOffset}
		});
		TankData.Abilities = BasicTankAbilities;
		TankData.ExperienceLevels = GetMediumTankExpLevels();
		TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(
			BaseMediumTankExp * T34_76_L_ExpWorthMlt, 5);
		TankData.ExperienceMultiplier = 1.0f;
		M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_T34_76_L, TankData);
	}

	// T-34E
	TankData.MaxHealth = MediumTankHealthBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 49;
	TankData.TurretRotationSpeed = 25;
	TankData.VehicleMaxSpeedKmh = 26;
	TankData.VehicleReverseSpeedKmh = 18;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, MediumTankRadixiteCost + 100},
		{ERTSResourceType::Resource_VehicleParts, MediumTankVehiclePartsCost + 50}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetMediumTankExpLevels();
	TankData.ExperienceWorth = BaseMediumTankExp;
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_T34E, TankData);

	// T-34/85
	TankData.MaxHealth = MediumTankHealthBase + OneMediumTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 20;
	TankData.VehicleMaxSpeedKmh = 22;
	TankData.VehicleReverseSpeedKmh = 18;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, MediumTankRadixiteCost + 150},
		{ERTSResourceType::Resource_VehicleParts, MediumTankVehiclePartsCost + 75}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetMediumTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(MediumTankHealthBase * 1.25f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_T34_85, TankData);

	// T-34/100
	TankData.MaxHealth = MediumTankHealthBase + OneMediumTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 15;
	TankData.VehicleMaxSpeedKmh = 22;
	TankData.VehicleReverseSpeedKmh = 8;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, MediumTankRadixiteCost + 250},
		{ERTSResourceType::Resource_VehicleParts, MediumTankVehiclePartsCost + 125}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetMediumTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(MediumTankHealthBase * 1.5f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_T34_100, TankData);

	// SU-85
	{
		const float SuMediumTankDestroyerBaseHealth = MediumTankHealthBase + OneLightTankShotHp;
		const float SuMediumTankDestroyerHealthMlt = 1.2f;
		const float SuMediumTankDestroyerHealth = RTSFunctionLibrary::RoundToNearestMultipleOf(
			SuMediumTankDestroyerBaseHealth * SuMediumTankDestroyerHealthMlt, 10);
		const float SuMediumTankDestroyerRotationSpeed = 35.f;
		const float SuMediumTankDestroyerTurretRotationSpeed = 15.f;
		const float SuMediumTankDestroyerMaxSpeedKmh = 22.f;
		const float SuMediumTankDestroyerReverseSpeedKmh = 12.f;
		const float Su85LaserHealthMlt = 1.1f;
		const int32 Su85LaserRadixiteCostOffset = 150;
		const int32 Su85LaserVehiclePartsCostOffset = 100;
		const float Su85LaserExpWorthMlt = 1.2f;
		const int32 Su100RadixiteCostOffset = 100;
		const int32 Su100VehiclePartsCostOffset = 75;
		const float Su100ExpWorthMlt = 1.1f;
		const int32 Su122RadixiteCostOffset = 150;
		const int32 Su122VehiclePartsCostOffset = 120;
		const float Su122ExpWorthMlt = 1.2f;
		const float Su122MaxSpeedKmh = 20.f;
		const float Su122ReverseSpeedKmh = 10.f;

		TankData.MaxHealth = SuMediumTankDestroyerHealth;
		TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
		TankData.VehicleRotationSpeed = SuMediumTankDestroyerRotationSpeed;
		TankData.TurretRotationSpeed = SuMediumTankDestroyerTurretRotationSpeed;
		TankData.VehicleMaxSpeedKmh = SuMediumTankDestroyerMaxSpeedKmh;
		TankData.VehicleReverseSpeedKmh = SuMediumTankDestroyerReverseSpeedKmh;
		TankData.VisionRadius = T2TankVisionRadius;
		TankData.Cost = FUnitCost({
			{ERTSResourceType::Resource_Radixite, MediumTankDestroyerRadixiteCost},
			{ERTSResourceType::Resource_VehicleParts, MediumTankDestroyerVehiclePartsCost}
		});
		TankData.Abilities = BasicTankDestroyerAbilities;
		TankData.ExperienceLevels = GetMediumTankDestroyerExpLevels();
		TankData.ExperienceWorth = BaseMediumTankExp;
		TankData.ExperienceMultiplier = 1.0f;
		M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_SU_85, TankData);

		TankData.MaxHealth = RTSFunctionLibrary::RoundToNearestMultipleOf(
			SuMediumTankDestroyerHealth * Su85LaserHealthMlt, 10);
		TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
		TankData.VehicleRotationSpeed = SuMediumTankDestroyerRotationSpeed;
		TankData.TurretRotationSpeed = SuMediumTankDestroyerTurretRotationSpeed;
		TankData.VehicleMaxSpeedKmh = SuMediumTankDestroyerMaxSpeedKmh;
		TankData.VehicleReverseSpeedKmh = SuMediumTankDestroyerReverseSpeedKmh;
		TankData.VisionRadius = T2TankVisionRadius;
		TankData.Cost = FUnitCost({
			{
				ERTSResourceType::Resource_Radixite,
				MediumTankDestroyerRadixiteCost + Su85LaserRadixiteCostOffset
			},
			{
				ERTSResourceType::Resource_VehicleParts,
				MediumTankDestroyerVehiclePartsCost + Su85LaserVehiclePartsCostOffset
			}
		});
		TankData.Abilities = BasicTankDestroyerAbilities;
		TankData.ExperienceLevels = GetMediumTankDestroyerExpLevels();
		TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(
			BaseMediumTankExp * Su85LaserExpWorthMlt, 5);
		TankData.ExperienceMultiplier = 1.0f;
		M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_SU_85_L, TankData);

		TankData.MaxHealth = SuMediumTankDestroyerHealth;
		TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
		TankData.VehicleRotationSpeed = SuMediumTankDestroyerRotationSpeed;
		TankData.TurretRotationSpeed = SuMediumTankDestroyerTurretRotationSpeed;
		TankData.VehicleMaxSpeedKmh = SuMediumTankDestroyerMaxSpeedKmh;
		TankData.VehicleReverseSpeedKmh = SuMediumTankDestroyerReverseSpeedKmh;
		TankData.VisionRadius = T2TankVisionRadius;
		TankData.Cost = FUnitCost({
			{ERTSResourceType::Resource_Radixite, MediumTankDestroyerRadixiteCost + Su100RadixiteCostOffset},
			{ERTSResourceType::Resource_VehicleParts, MediumTankDestroyerVehiclePartsCost + Su100VehiclePartsCostOffset}
		});
		TankData.Abilities = BasicTankDestroyerAbilities;
		TankData.ExperienceLevels = GetMediumTankDestroyerExpLevels();
		TankData.ExperienceWorth =
			RTSFunctionLibrary::RoundToNearestMultipleOf(BaseMediumTankExp * Su100ExpWorthMlt, 5);
		TankData.ExperienceMultiplier = 1.0f;
		M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_SU_100, TankData);

		TankData.MaxHealth = SuMediumTankDestroyerHealth;
		TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
		TankData.VehicleRotationSpeed = SuMediumTankDestroyerRotationSpeed;
		TankData.TurretRotationSpeed = SuMediumTankDestroyerTurretRotationSpeed;
		TankData.VehicleMaxSpeedKmh = Su122MaxSpeedKmh;
		TankData.VehicleReverseSpeedKmh = Su122ReverseSpeedKmh;
		TankData.VisionRadius = T2TankVisionRadius;
		TankData.Cost = FUnitCost({
			{ERTSResourceType::Resource_Radixite, MediumTankDestroyerRadixiteCost + Su122RadixiteCostOffset},
			{ERTSResourceType::Resource_VehicleParts, MediumTankDestroyerVehiclePartsCost + Su122VehiclePartsCostOffset}
		});
		TankData.Abilities = BasicTankDestroyerAbilities;
		TankData.ExperienceLevels = GetMediumTankDestroyerExpLevels();
		TankData.ExperienceWorth =
			RTSFunctionLibrary::RoundToNearestMultipleOf(BaseMediumTankExp * Su122ExpWorthMlt, 5);
		TankData.ExperienceMultiplier = 1.0f;
		M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_SU_122, TankData);
	}

	// T-44/100L
	{
		const float T44LaserRotationSpeed = 60.f;
		const float T44LaserTurretRotationSpeed = 30.f;
		const float T44LaserMaxSpeedKmh = 30.f;
		const float T44LaserReverseSpeedKmh = 20.f;
		const float T44LaserCostMlt = 2.0f;
		const float T44LaserExpWorthMlt = 1.75f;
		const int32 T44LaserCost = RTSFunctionLibrary::RoundToNearestMultipleOf(
			T3MediumTankRadixiteCost * T44LaserCostMlt, 10);
		const int32 T44LaserVehiclePartsCost = RTSFunctionLibrary::RoundToNearestMultipleOf(
			T3MediumTankVehiclePartsCost * T44LaserCostMlt, 10);

		TankData.MaxHealth = T3MediumTankBase;
		TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyArmorResistances(TankData.MaxHealth);
		TankData.VehicleRotationSpeed = T44LaserRotationSpeed;
		TankData.TurretRotationSpeed = T44LaserTurretRotationSpeed;
		TankData.VehicleMaxSpeedKmh = T44LaserMaxSpeedKmh;
		TankData.VehicleReverseSpeedKmh = T44LaserReverseSpeedKmh;
		TankData.VisionRadius = T2TankVisionRadius;
		TankData.Cost = FUnitCost({
			{ERTSResourceType::Resource_Radixite, T44LaserCost},
			{ERTSResourceType::Resource_VehicleParts, T44LaserVehiclePartsCost}
		});
		TankData.Abilities = BasicTankAbilities;
		TankData.ExperienceLevels = GetMediumTankExpLevels();
		TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(
			BaseMediumTankExp * T44LaserExpWorthMlt, 5);
		TankData.ExperienceMultiplier = 1.0f;
		M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_T44_100L, TankData);
	}
}

void ACPPGameState::InitAllGameHeavyTankData()
{
	using namespace DeveloperSettings::GameBalance::UnitCosts;
	using namespace DeveloperSettings::GameBalance::VisionRadii::UnitVision;
	using namespace DeveloperSettings::GameBalance::Experience;

	using DeveloperSettings::GameBalance::UnitHealth::MediumTankHealthBase;
	using DeveloperSettings::GameBalance::UnitHealth::T3MediumTankBase;
	using DeveloperSettings::GameBalance::UnitHealth::T2HeavyTankBase;
	using DeveloperSettings::GameBalance::UnitHealth::T3HeavyTankBase;
	using DeveloperSettings::GameBalance::UnitHealth::OneMediumTankShotHp;
	using DeveloperSettings::GameBalance::UnitHealth::OneHeavyTankShotHp;

	const TArray<FUnitAbilityEntry> BasicTankAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdReverseMove,
		EAbilityID::IdNoAbility,
		EAbilityID::IdDigIn, EAbilityID::IdRotateTowards, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
	});
	const TArray<FUnitAbilityEntry> TankAbilitiesWithRockets = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdReverseMove,
		EAbilityID::IdNoAbility,
		EAbilityID::IdDigIn, EAbilityID::IdRotateTowards, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdFireRockets, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
	});
	const TArray<FUnitAbilityEntry> BasicTankDestroyerAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdReverseMove,
		EAbilityID::IdNoAbility,
		EAbilityID::IdDigIn, EAbilityID::IdRotateTowards, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility
	});

	FTankData TankData;

	// ------------------------------- GER Heavy Tanks -------------------------------

	// Jagdpanther (heavy TD tier per original)
	TankData.MaxHealth = T3MediumTankBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 25;
	TankData.TurretRotationSpeed = 8;
	TankData.VehicleMaxSpeedKmh = 28;
	TankData.VehicleReverseSpeedKmh = 11;
	TankData.VisionRadius = T3TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T3MediumTankDestroyerRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, T3MediumTankDestroyerVehiclePartsCost}
	});
	TankData.Abilities = FAbilityHelpers::SwapAtIdForNewEntry(TankAbilitiesWithRockets, EAbilityID::IdFireRockets,
	                                                          FAbilityHelpers::GetRocketAbilityEntry(
		                                                          EAttachedRocketAbilityType::SmallRockets));
	TankData.ExperienceLevels = GetHeavyTankDestroyerExpLevels();
	TankData.ExperienceWorth = BaseHeavyTankExp;
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_JagdPanther, TankData);

	// Maus.
	TankData.MaxHealth = DeveloperSettings::GameBalance::UnitHealth::SuperHeavyTankBase + 2 * OneHeavyTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetISuperHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 15;
	TankData.TurretRotationSpeed = 8;
	TankData.VehicleMaxSpeedKmh = 8;
	TankData.VehicleReverseSpeedKmh = 5;
	TankData.VisionRadius = SuperHeavyTankVision;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, SuperHeavyTankRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, SuperHeavyTankVehiclePartsCost}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetHeavyTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseHeavyTankExp * 2.0f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Maus, TankData);
	// E100 less health but more speed than Maus
	TankData.MaxHealth = DeveloperSettings::GameBalance::UnitHealth::SuperHeavyTankBase;
	TankData.VehicleRotationSpeed = 20;
	TankData.TurretRotationSpeed = 12;
	TankData.VehicleMaxSpeedKmh = 12;
	TankData.VehicleReverseSpeedKmh = 8;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_E100, TankData);

	// Panther G
	TankData.MaxHealth = T3MediumTankBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 20;
	TankData.VehicleMaxSpeedKmh = 30;
	TankData.VehicleReverseSpeedKmh = 8;
	TankData.VisionRadius = T3TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T3MediumTankRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, T3MediumTankVehiclePartsCost}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetHeavyTankExpLevels();
	TankData.ExperienceWorth = BaseHeavyTankExp;
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PantherG, TankData);

	// Panzer V/IV (hybrid) — increased cost *1.5 and *2 per original
	{
		FTankData PzV_IV = TankData;
		PzV_IV.Cost = FUnitCost({
			{
				ERTSResourceType::Resource_Radixite,
				RTSFunctionLibrary::RoundToNearestMultipleOf(T3MediumTankRadixiteCost * 1.5f, 5)
			},
			{
				ERTSResourceType::Resource_VehicleParts,
				RTSFunctionLibrary::RoundToNearestMultipleOf(MediumTankVehiclePartsCost * 2.0f, 5)
			}
		});
		M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PanzerV_IV, PzV_IV);

		FTankData PzV_III = PzV_IV;
		PzV_III.TurretRotationSpeed = 25;
		M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PanzerV_III, PzV_III);

		FTankData PantherD = PzV_IV;
		PantherD.TurretRotationSpeed = 12;
		M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PantherD, PantherD);
	}

	// Panther II
	TankData.MaxHealth = T3MediumTankBase + OneHeavyTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetISuperHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 40;
	TankData.TurretRotationSpeed = 25;
	TankData.VehicleMaxSpeedKmh = 40;
	TankData.VehicleReverseSpeedKmh = 20;
	TankData.VisionRadius = T3TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T3MediumTankRadixiteCost + 300},
		{ERTSResourceType::Resource_VehicleParts, T3MediumTankVehiclePartsCost + 250}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetHeavyTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseHeavyTankExp * 1.3f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_PantherII, TankData);

	// Tiger / Tiger H1
	TankData.MaxHealth = T3HeavyTankBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 25;
	TankData.TurretRotationSpeed = 12;
	TankData.VehicleMaxSpeedKmh = 26;
	TankData.VehicleReverseSpeedKmh = 15;
	TankData.VisionRadius = T3TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T3HeavyTankRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, T3HeavyTankVehiclePartsCost}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetHeavyTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseHeavyTankExp * 1.1f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Tiger, TankData);
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_TigerH1, TankData);


	// Sturm Tiger
	TankData.MaxHealth = T3HeavyTankBase + 400;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 25;
	TankData.TurretRotationSpeed = 12;
	TankData.VehicleMaxSpeedKmh = 26;
	TankData.VehicleReverseSpeedKmh = 15;
	TankData.VisionRadius = T3TankVisionRadius;
	TankData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(T3HeavyTankRadixiteCost * 1.5, 25)
		},
		{
			ERTSResourceType::Resource_VehicleParts,
			RTSFunctionLibrary::RoundToNearestMultipleOf(T3HeavyTankVehiclePartsCost * 1.33, 25)
		}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetHeavyTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseHeavyTankExp * 1.1f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_SturmTiger, TankData);

	// King Tiger
	TankData.MaxHealth = T3HeavyTankBase + OneHeavyTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetISuperHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 15;
	TankData.TurretRotationSpeed = 19;
	TankData.VehicleMaxSpeedKmh = 15;
	TankData.VehicleReverseSpeedKmh = 8;
	TankData.VisionRadius = T3TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T3HeavyTankRadixiteCost + 400},
		{ERTSResourceType::Resource_VehicleParts, T3HeavyTankVehiclePartsCost + 200}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetHeavyTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseHeavyTankExp * 1.5f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_KingTiger, TankData);

	// Tiger 105 (same as King Tiger with higher cost and exp)
	TankData.MaxHealth = T3HeavyTankBase + OneHeavyTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetISuperHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 15;
	TankData.TurretRotationSpeed = 19;
	TankData.VehicleMaxSpeedKmh = 15;
	TankData.VehicleReverseSpeedKmh = 8;
	TankData.VisionRadius = T3TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T3HeavyTankRadixiteCost + 600},
		{ERTSResourceType::Resource_VehicleParts, T3HeavyTankVehiclePartsCost + 300}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetHeavyTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseHeavyTankExp * 1.75f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_Tiger105, TankData);

	// ------------------------------- RUS Heavy Tanks -------------------------------

	// T-28 (uses Medium exp levels per original)
	TankData.MaxHealth = T2HeavyTankBase - OneMediumTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 15;
	TankData.TurretRotationSpeed = 12;
	TankData.VehicleMaxSpeedKmh = 20;
	TankData.VehicleReverseSpeedKmh = 10;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T2HeavyTankRadixiteCost - 250},
		{ERTSResourceType::Resource_VehicleParts, T2HeavyTankVehiclePartsCost - 100}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetMediumTankExpLevels();
	TankData.ExperienceWorth = MediumTankHealthBase;
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_T28, TankData);

	// T-35 (super large)
	TankData.MaxHealth = RTSFunctionLibrary::RoundToNearestMultipleOf(T3HeavyTankBase * 2.0f, 10);
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIMediumArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 15;
	TankData.TurretRotationSpeed = 12;
	TankData.VehicleMaxSpeedKmh = 12;
	TankData.VehicleReverseSpeedKmh = 6;
	TankData.VisionRadius = T3TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, 800},
		{ERTSResourceType::Resource_VehicleParts, 200}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetHeavyTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseHeavyTankExp * 1.33f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_T35, TankData);

	// KV-1 (uses Medium exp levels per original)
	TankData.MaxHealth = T2HeavyTankBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 15;
	TankData.TurretRotationSpeed = 12;
	TankData.VehicleMaxSpeedKmh = 12;
	TankData.VehicleReverseSpeedKmh = 6;
	TankData.VisionRadius = T2TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T2HeavyTankRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, T2HeavyTankVehiclePartsCost}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetMediumTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseMediumTankExp * 1.33f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_KV_1, TankData);

	// KV-2 (heavy exp levels; T3 vision; higher cost)
	TankData.MaxHealth = T2HeavyTankBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 15;
	TankData.TurretRotationSpeed = 12;
	TankData.VehicleMaxSpeedKmh = 12;
	TankData.VehicleReverseSpeedKmh = 6;
	TankData.VisionRadius = T3TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T2HeavyTankRadixiteCost + 250},
		{ERTSResourceType::Resource_VehicleParts, T2HeavyTankVehiclePartsCost + 125}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetHeavyTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseMediumTankExp * 1.85f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_KV_2, TankData);

	// KV-1E (inherits KV-2 vision/speeds in your original — made explicit)
	TankData.MaxHealth = T2HeavyTankBase + 2.0f * OneMediumTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetISuperHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 15;
	TankData.TurretRotationSpeed = 12;
	TankData.VehicleMaxSpeedKmh = 12;
	TankData.VehicleReverseSpeedKmh = 6;
	TankData.VisionRadius = T3TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T2HeavyTankRadixiteCost + 200},
		{ERTSResourceType::Resource_VehicleParts, T2HeavyTankVehiclePartsCost + 125}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetMediumTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseMediumTankExp * 1.5f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_KV_1E, TankData);
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_KV_1_Arc, TankData);

	// KV-IS (same speeds/vision as KV-1E per inheritance; health uses +2 * OneLightTankShot)
	TankData.MaxHealth = T2HeavyTankBase + 2.0f * DeveloperSettings::GameBalance::UnitHealth::OneLightTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 15;
	TankData.TurretRotationSpeed = 12;
	TankData.VehicleMaxSpeedKmh = 12;
	TankData.VehicleReverseSpeedKmh = 6;
	TankData.VisionRadius = T3TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T2HeavyTankRadixiteCost + 200},
		{ERTSResourceType::Resource_VehicleParts, T2HeavyTankVehiclePartsCost + 125}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetMediumTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseMediumTankExp * 1.7f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_KV_IS, TankData);

	// IS-1
	TankData.MaxHealth = T3HeavyTankBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 18;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 20;
	TankData.VehicleReverseSpeedKmh = 10;
	TankData.VisionRadius = T3TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T3HeavyTankRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, T3HeavyTankVehiclePartsCost}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetHeavyTankExpLevels();
	TankData.ExperienceWorth = BaseHeavyTankExp;
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_IS_1, TankData);

	// IS-2
	TankData.MaxHealth = T3HeavyTankBase + OneHeavyTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetISuperHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 18;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 20;
	TankData.VehicleReverseSpeedKmh = 10;
	TankData.VisionRadius = T3TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T3HeavyTankRadixiteCost + 200},
		{ERTSResourceType::Resource_VehicleParts, T3HeavyTankVehiclePartsCost + 200}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetHeavyTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseHeavyTankExp * 1.33f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_IS_2, TankData);

	// IS-3 (super heavy)
	TankData.MaxHealth = DeveloperSettings::GameBalance::UnitHealth::SuperHeavyTankBase;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetISuperHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 18;
	TankData.TurretRotationSpeed = 14;
	TankData.VehicleMaxSpeedKmh = 17;
	TankData.VehicleReverseSpeedKmh = 9;
	TankData.VisionRadius = T3TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, SuperHeavyTankRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, SuperHeavyTankVehiclePartsCost}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetHeavyTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseHeavyTankExp * 2.0f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_IS_3, TankData);

	// KV-5 (super heavy)
	TankData.MaxHealth = DeveloperSettings::GameBalance::UnitHealth::SuperHeavyTankBase + 2 * OneHeavyTankShotHp;
	TankData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetISuperHeavyArmorResistances(TankData.MaxHealth);
	TankData.VehicleRotationSpeed = 20;
	TankData.TurretRotationSpeed = 12;
	TankData.VehicleMaxSpeedKmh = 12;
	TankData.VehicleReverseSpeedKmh = 8;
	TankData.VisionRadius = T3TankVisionRadius;
	TankData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, SuperHeavyTankRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, SuperHeavyTankVehiclePartsCost}
	});
	TankData.Abilities = BasicTankAbilities;
	TankData.ExperienceLevels = GetHeavyTankExpLevels();
	TankData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(BaseHeavyTankExp * 2.0f, 5);
	TankData.ExperienceMultiplier = 1.0f;
	M_TPlayerTankDataHashMap.Add(ETankSubtype::Tank_KV_5, TankData);
}

void ACPPGameState::InitAllGameAircraftData()
{
	using DeveloperSettings::GameBalance::UnitCosts::FighterRadixiteCost;
	using DeveloperSettings::GameBalance::UnitCosts::FighterVehiclePartsCost;
	using DeveloperSettings::GameBalance::VisionRadii::UnitVision::AircraftVisionRadius;
	using DeveloperSettings::GameBalance::UnitHealth::FighterHealth;
	using DeveloperSettings::GameBalance::Experience::BaseAircraftExp;
	using DeveloperSettings::GameBalance::UnitCosts::AttackAircraftRadixiteCost;
	using DeveloperSettings::GameBalance::UnitCosts::AttackAircraftVehiclePartsCost;


	// Start with ability no owner; when the owner is set the return to base ability will automatically be swapped with it.
	TArray<FUnitAbilityEntry> BasicAircraftAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdAircraftOwnerNotExpanded,
		EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
	});

	FAircraftData AircraftData;

	AircraftData.Abilities = BasicAircraftAbilities;
	AircraftData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, FighterRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, FighterVehiclePartsCost}
	});
	AircraftData.MaxHealth = FighterHealth;
	AircraftData.ResistancesAndDamageMlt =
		FUnitResistanceDataHelpers::GetILightArmorResistances(AircraftData.MaxHealth);
	AircraftData.ExperienceLevels = GetFighterExpLevels();
	AircraftData.ExperienceWorth = BaseAircraftExp;
	AircraftData.ExperienceMultiplier = 1.0;
	AircraftData.VisionRadius = AircraftVisionRadius;
	M_TPlayerAircraftDataHashMap.Add(EAircraftSubtype::Aircraft_Bf109, AircraftData);

	AircraftData.Abilities = BasicAircraftAbilities;
	AircraftData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, AttackAircraftRadixiteCost - 200},
		{ERTSResourceType::Resource_VehicleParts, AttackAircraftVehiclePartsCost - 50}
	});
	AircraftData.MaxHealth = DeveloperSettings::GameBalance::UnitHealth::AttackAircraftHealth;
	AircraftData.ResistancesAndDamageMlt =
		FUnitResistanceDataHelpers::GetILightArmorResistances(AircraftData.MaxHealth);
	AircraftData.ExperienceLevels = GetFighterExpLevels();
	AircraftData.ExperienceWorth = BaseAircraftExp;
	AircraftData.ExperienceMultiplier = 1.0;
	AircraftData.VisionRadius = AircraftVisionRadius;
	M_TPlayerAircraftDataHashMap.Add(EAircraftSubtype::Aircraft_Ju87, AircraftData);


	AircraftData.Abilities = BasicAircraftAbilities;
	AircraftData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, AttackAircraftRadixiteCost},
		{ERTSResourceType::Resource_VehicleParts, AttackAircraftVehiclePartsCost}
	});
	AircraftData.MaxHealth = DeveloperSettings::GameBalance::UnitHealth::AttackAircraftHealth;
	AircraftData.ResistancesAndDamageMlt =
		FUnitResistanceDataHelpers::GetILightArmorResistances(AircraftData.MaxHealth);
	AircraftData.ExperienceLevels = GetFighterExpLevels();
	AircraftData.ExperienceWorth = BaseAircraftExp;
	AircraftData.ExperienceMultiplier = 1.0;
	AircraftData.VisionRadius = AircraftVisionRadius;
	M_TPlayerAircraftDataHashMap.Add(EAircraftSubtype::Aircraft_Me410, AircraftData);


	// Ensure the enemy has the same aircraft data.
	M_TEnemyAircraftDataHashMap = M_TPlayerAircraftDataHashMap;
}

TArray<FExperienceLevel> ACPPGameState::GetLightTankExpLevels() const
{
	TArray<FExperienceLevel> Levels;
	float CulumativeExp = 0;
	// Level 1
	TArray<FExperiencePerk> ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.1),
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.1)
	};
	CulumativeExp += 100;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 2
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.25)};
	CulumativeExp += 250;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 3
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Health, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_SightRange, 1.1)
	};
	CulumativeExp += 450;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 4
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.1)
	};
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 5
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_Damage, 1.25)};
	CulumativeExp += 600;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));

	return Levels;
}

TArray<FExperienceLevel> ACPPGameState::GetLightTankDestroyerExpLevels() const
{
	TArray<FExperienceLevel> Levels;
	float CulumativeExp = 0;
	// Level 1
	TArray<FExperiencePerk> ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.2)
	};
	CulumativeExp += 100;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 2
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.25)};
	CulumativeExp += 250;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 3
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_SightRange, 1.25),
		FExperiencePerk(EExperiencePerkType::EP_Range, 1.25)
	};
	CulumativeExp += 450;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 4
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.1)
	};
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 5
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_Damage, 1.25)};
	CulumativeExp += 600;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));

	return Levels;
}

TArray<FExperienceLevel> ACPPGameState::GetArmoredCarExpLevels() const
{
	TArray<FExperienceLevel> Levels;
	float CulumativeExp = 0;
	// Level 1
	TArray<FExperiencePerk> ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.2)
	};
	CulumativeExp += 70;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 2
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.25)};
	CulumativeExp += 150;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 3
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_SightRange, 1.2)
	};
	CulumativeExp += 275;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 4
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.1)
	};
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 5
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_Damage, 1.25)};
	CulumativeExp += 425;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));

	return Levels;
}

TArray<FExperienceLevel> ACPPGameState::GetLightMediumTankExpLevels() const
{
	TArray<FExperienceLevel> Levels;
	float CulumativeExp = 0;
	// Level 1
	TArray<FExperiencePerk> ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.1),
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.1)
	};
	CulumativeExp += 150;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 2
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.25)};
	CulumativeExp += 400;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 3
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Health, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_SightRange, 1.1)
	};
	CulumativeExp += 700;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 4
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.1)
	};
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 5
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_Damage, 1.25)};
	CulumativeExp += 850;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));

	return Levels;
}

TArray<FExperienceLevel> ACPPGameState::GetMediumTankExpLevels() const
{
	TArray<FExperienceLevel> Levels;
	float CulumativeExp = 0;
	// Level 1
	TArray<FExperiencePerk> ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.1),
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.1)
	};
	CulumativeExp += 200;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 2
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.25)};
	CulumativeExp += 500;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 3
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Health, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_SightRange, 1.1)
	};
	CulumativeExp += 800;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 4
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.1)
	};
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 5
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_Damage, 1.25)};
	CulumativeExp += 1000;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));

	return Levels;
}

TArray<FExperienceLevel> ACPPGameState::GetMediumTankDestroyerExpLevels() const
{
	TArray<FExperienceLevel> Levels;
	float CulumativeExp = 0;
	// Level 1
	TArray<FExperiencePerk> ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.1),
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.1)
	};
	CulumativeExp += 200;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 2
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.25)};
	CulumativeExp += 500;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 3
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_SightRange, 1.25),
		FExperiencePerk(EExperiencePerkType::EP_Range, 1.25)
	};
	CulumativeExp += 800;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 4
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.1)
	};
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 5
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_Damage, 1.25)};
	CulumativeExp += 1000;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));

	return Levels;
}

TArray<FExperienceLevel> ACPPGameState::GetHeavyTankExpLevels() const
{
	TArray<FExperienceLevel> Levels;
	float CulumativeExp = 0;
	// Level 1
	TArray<FExperiencePerk> ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.1),
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.1)
	};
	CulumativeExp += 300;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 2
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.25)};
	CulumativeExp += 700;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 3
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Health, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_SightRange, 1.1)
	};
	CulumativeExp += 900;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 4
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.1)
	};
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 5
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_Damage, 1.25)};
	CulumativeExp += 1200;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));

	return Levels;
}

TArray<FExperienceLevel> ACPPGameState::GetHeavyTankDestroyerExpLevels() const
{
	TArray<FExperienceLevel> Levels;
	float CulumativeExp = 0;
	// Level 1
	TArray<FExperiencePerk> ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.1),
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.1)
	};
	CulumativeExp += 300;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 2
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.25)};
	CulumativeExp += 700;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 3
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_SightRange, 1.25),
		FExperiencePerk(EExperiencePerkType::EP_Range, 1.25)
	};
	CulumativeExp += 900;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 4
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.1)
	};
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 5
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_Damage, 1.25)};
	CulumativeExp += 1200;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));

	return Levels;
}

TArray<FExperienceLevel> ACPPGameState::GetNomadicTruckExpLevels() const
{
	TArray<FExperienceLevel> Levels;
	float CulumativeExp = 0;
	// Level 1
	TArray<FExperiencePerk> ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_SightRange, 1.1),
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.1)
	};
	CulumativeExp += 100;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 2
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.25)};
	CulumativeExp += 250;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 3
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Health, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_SightRange, 1.1)
	};
	CulumativeExp += 450;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 4
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.1),
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.1)
	};
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 5
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_Damage, 1.25)};
	CulumativeExp += 600;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));

	return Levels;
}

TArray<FExperienceLevel> ACPPGameState::GetTier1InfantryExpLevels() const
{
	TArray<FExperienceLevel> Levels;
	float CulumativeExp = 0;
	// Level 1
	TArray<FExperiencePerk> ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.1),
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.1)
	};
	CulumativeExp += 70;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 2
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.25)};
	CulumativeExp += 150;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 3
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_SightRange, 1.2)
	};
	CulumativeExp += 275;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 4
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.1)
	};
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 5
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_Damage, 1.25)};
	CulumativeExp += 425;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	return Levels;
}

TArray<FExperienceLevel> ACPPGameState::GetTier2InfantryExpLevels() const
{
	TArray<FExperienceLevel> Levels;
	float CulumativeExp = 0;
	// Level 1
	TArray<FExperiencePerk> ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.1),
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.1)
	};
	CulumativeExp += 70;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 2
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.25)};
	CulumativeExp += 150;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 3
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_SightRange, 1.2)
	};
	CulumativeExp += 275;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 4
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.1)
	};
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 5
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_Damage, 1.25)};
	CulumativeExp += 425;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	return Levels;
}

TArray<FExperienceLevel> ACPPGameState::GetEliteInfantryExpLevels() const
{
	TArray<FExperienceLevel> Levels;
	float CulumativeExp = 0;
	// Level 1
	TArray<FExperiencePerk> ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.1),
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.1)
	};
	CulumativeExp += 70;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 2
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.25)};
	CulumativeExp += 150;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 3
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_SightRange, 1.2)
	};
	CulumativeExp += 275;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 4
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.1)
	};
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 5
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_Damage, 1.25)};
	CulumativeExp += 425;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	return Levels;
}

TArray<FExperienceLevel> ACPPGameState::GetFighterExpLevels() const
{
	TArray<FExperienceLevel> Levels;
	float CulumativeExp = 0;
	// Level 1
	TArray<FExperiencePerk> ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.1),
		FExperiencePerk(EExperiencePerkType::EP_Accuracy, 1.1)
	};
	CulumativeExp += 400;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 2
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.25)};
	CulumativeExp += 700;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 3
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Health, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_SightRange, 1.1)
	};
	CulumativeExp += 1200;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 4
	ExperiencePerks = {
		FExperiencePerk(EExperiencePerkType::EP_Speed, 1.2),
		FExperiencePerk(EExperiencePerkType::EP_ReloadSpeed, 1.1)
	};
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));
	// Level 5
	ExperiencePerks = {FExperiencePerk(EExperiencePerkType::EP_Damage, 1.25)};
	CulumativeExp += 2000;
	Levels.Add(FExperienceLevel(CulumativeExp, ExperiencePerks));

	return Levels;
}

void ACPPGameState::InitAllGameBxpData()
{
	using namespace DeveloperSettings::GameBalance::TrainingTime;
	using namespace DeveloperSettings::GameBalance::UnitCosts;
	using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
	using namespace DeveloperSettings::GameBalance::UnitHealth;


	TArray<FUnitAbilityEntry> ArmedBxpAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdNoAbility, EAbilityID::IdStop, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
	});
	TArray<FUnitAbilityEntry> NotArmedBxpAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdStop, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
	});

	FBxpData BxpData;

	BxpHelpers::GerBxpData::InitGerHQBxpData(M_TPlayerBxpDataHashMap);
	BxpHelpers::GerBxpData::InitGerLFactoryBxpData(M_TPlayerBxpDataHashMap);
	BxpHelpers::GerBxpData::InitRefineryBxpData(M_TPlayerBxpDataHashMap);
	BxpHelpers::GerBxpData::InitGerCaptureBunkersBxpData(M_TPlayerBxpDataHashMap);
	BxpHelpers::GerBxpData::InitGerBarracksBxpData(M_TPlayerBxpDataHashMap);

	BxpData.ConstructionTime = T1BxpBuildTime;
	BxpData.Health = T1BxpHealth;
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
	BxpData.VisionRadius = T1BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, BxpT1DefensiveRadixiteCost},
		{ERTSResourceType::Resource_Metal, BxpT1DefensiveMetalCost}
	});
	BxpData.Abilities = ArmedBxpAbilities;
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_37mmFlak, BxpData);

	BxpData.ConstructionTime = T1BxpBuildTime * 2;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T1BxpHealth * 1.5, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
	BxpData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, BxpT1DefensiveRadixiteCost * 3},
		{ERTSResourceType::Resource_Metal, BxpT1DefensiveMetalCost * 4}
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.VisionRadius = T2BxpVisionRadius;
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BXT_88mmFlak, BxpData);

	BxpData.ConstructionTime = T1BxpBuildTime;
	BxpData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, BxpT1UtilityRadixiteCost},
		{ERTSResourceType::Resource_Metal, BxpT1UtilityMetalCost}
	});
	BxpData.Abilities = NotArmedBxpAbilities;
	BxpData.VisionRadius = T1BxpVisionRadius;
	BxpData.Health = T1BxpHealth;
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BXT_SolarSmall, BxpData);

	BxpData.ConstructionTime = T1BxpBuildTime * 2;
	BxpData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, BxpT1UtilityRadixiteCost + 75},
		{ERTSResourceType::Resource_Metal, BxpT1UtilityMetalCost + 50}
	});
	BxpData.Abilities = NotArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T1BxpHealth * 1.25, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
	BxpData.VisionRadius = T1BxpVisionRadius;
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BXT_SolarLarge, BxpData);

	BxpData.ConstructionTime = BxpT2BuildTime;
	BxpData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, BxpT2DefensiveRadixiteCost},
		{ERTSResourceType::Resource_Metal, BxpT2DefensiveMetalCost}
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.VisionRadius = T2BxpVisionRadius;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T1BxpHealth * 1.75, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_Pak38, BxpData);

	// FLAK BUNKER
	BxpData.ConstructionTime = BxpT2BuildTime;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(BxpT2BunkerRadixiteCost * 4, 5)
		},
		{ERTSResourceType::Resource_Metal, RTSFunctionLibrary::RoundToNearestMultipleOf(HeavyBunkerMetalCost * 2, 5)}
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.VisionRadius = T2BxpVisionRadius;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(BxpHeavyBunkerHealth * 2.5, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BXT_GerFLakBunker, BxpData);

	BxpData.ConstructionTime = BxpT2BuildTime;
	BxpData.VisionRadius = T2BxpVisionRadius;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T1BxpHealth * 1.75, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
	BxpData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, BxpT2DefensiveRadixiteCost + 200},
		{ERTSResourceType::Resource_Metal, BxpT2DefensiveMetalCost + 75}
	});
	BxpData.Abilities = ArmedBxpAbilities;
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_LeFH_150mm, BxpData);


	// todo later fix the prices of these russian buildings when we have their counter parts for the player.
	// -----------------------------------------------------------------------
	// ----------------------------- RUS FACTORY ----------------------------
	// -----------------------------------------------------------------------
	BxpData.ConstructionTime = T1BxpBuildTime;
	BxpData.VisionRadius = 3 * T1BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(NomadicT1VehicleFactoryRadixiteCost * 4),
			                                             10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(NomadicT1VehicleFactoryMetalCost * 4), 10)
		},
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T3NomadicBuildingHealth * 5, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_RusFactory, BxpData);

	// -----------------------------------------------------------------------
	// ------------------------- RUS PLATFORM FACTORY -----------------------
	// -----------------------------------------------------------------------
	BxpData.ConstructionTime = T1BxpBuildTime;
	BxpData.VisionRadius = 3 * T1BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(NomadicT1VehicleFactoryRadixiteCost * 7),
			                                             10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(NomadicT1VehicleFactoryMetalCost * 7), 10)
		},
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T3NomadicBuildingHealth * 7, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_RusPlatformFactory, BxpData);

	// -----------------------------------------------------------------------
	// ------------------------- RUS RESEARCH CENTER ------------------------
	// -----------------------------------------------------------------------
	BxpData.ConstructionTime = T1BxpBuildTime;
	BxpData.VisionRadius = 2 * T1BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(T1StorageBuildingRadixite * 3), 10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(T1StorageBuildingMetal * 3), 10)
		},
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T2NomadicBuildingHealth * 2, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_RusResearchCenter, BxpData);

	// -----------------------------------------------------------------------
	// ------------------------- RUS COOLING TOWERS -------------------------
	// -----------------------------------------------------------------------
	BxpData.ConstructionTime = T1BxpBuildTime;
	BxpData.VisionRadius = 2 * T1BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(T1StorageBuildingRadixite * 3), 10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(T1StorageBuildingMetal * 3), 10)
		},
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T2NomadicBuildingHealth * 2, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_RusCoolingTowers, BxpData);

	// -----------------------------------------------------------------------
	// ----------------------------- RUS BARRACKS ---------------------------
	// -----------------------------------------------------------------------
	BxpData.ConstructionTime = T1BxpBuildTime;
	BxpData.VisionRadius = T1BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveRadixiteCost * 1.5), 10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveMetalCost * 1.5), 10)
		},
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T1NomadicBuildingHealth * 1, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_RusBarracks, BxpData);

	// -----------------------------------------------------------------------
	// --------------------------- RUS BUNKER MG ----------------------------
	// -----------------------------------------------------------------------
	BxpData.ConstructionTime = T1BxpBuildTime;
	BxpData.VisionRadius = T1BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveRadixiteCost * 1.33), 10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveMetalCost * 1.25), 10)
		},
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T1BxpHealth * 1.5, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_RusBunkerMG, BxpData);

	// -----------------------------------------------------------------------
	// --------------------------- RUS GUARD TOWER --------------------------
	// -----------------------------------------------------------------------
	BxpData.ConstructionTime = T1BxpBuildTime;
	BxpData.VisionRadius = T1BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveRadixiteCost * 1.8), 10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveMetalCost * 1.8), 10)
		},
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T1BxpHealth * 2.2, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_RusGuardTower, BxpData);


	// -----------------------------------------------------------------------
	// ----------------------------- RUS BOFORS POSITION ---------------------
	// -----------------------------------------------------------------------
	BxpData.ConstructionTime = BxpT2BuildTime;
	BxpData.VisionRadius = T2BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveRadixiteCost * 2), 10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveMetalCost * 2), 10)
		},
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T2BxpHealth * 1.33, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_RusBoforsPosition, BxpData);

	// -----------------------------------------------------------------------
	// --------------------------- RUS WALL AND GATE -------------------------
	// -----------------------------------------------------------------------
	BxpData.ConstructionTime = BxpT2BuildTime;
	BxpData.VisionRadius = T2BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveRadixiteCost * 1), 10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveMetalCost * 2), 10)
		},
	});
	BxpData.Abilities = NotArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(BxpWallHealth, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_RusWall, BxpData);
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(BxpGateHealth, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_RusGate, BxpData);

	// -----------------------------------------------------------------------
	// --------------------------- RUS AMMO STORAGE -------------------------
	// -----------------------------------------------------------------------


	BxpData.ConstructionTime = T1BxpBuildTime;
	BxpData.VisionRadius = T1BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveRadixiteCost * 1), 10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveMetalCost * 1), 10)
		},
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T1BxpHealth * 1, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_RusAmmoStorage, BxpData);

	// -----------------------------------------------------------------------
	// ------------------------- RUS FUEL STORAGE 1 -------------------------
	// -----------------------------------------------------------------------
	BxpData.ConstructionTime = T1BxpBuildTime;
	BxpData.VisionRadius = T1BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveRadixiteCost * 1), 10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveMetalCost * 1), 10)
		},
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T1BxpHealth * 0.8, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_RusFuelStorage1, BxpData);


	// -----------------------------------------------------------------------
	// ------------- RUS FUEL STORAGE 2, Supplies Storage -----------------
	// -----------------------------------------------------------------------
	BxpData.ConstructionTime = T1BxpBuildTime;
	BxpData.VisionRadius = T1BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveRadixiteCost * 2), 10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT1DefensiveMetalCost * 2), 10)
		},
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T1BxpHealth * 2.5, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_RusFuelStorage2, BxpData);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_Rus_SuppliesStorage, BxpData);


	BxpData.ConstructionTime = BxpT2BuildTime;
	BxpData.VisionRadius = T2BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(HeavyBunkerRadixiteCost - 600), 10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(HeavyBunkerMetalCost - 350), 10)
		},
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(BxpHeavyBunkerHealth + 250, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_Bunker05, BxpData);

	// -----------------------------------------------------------------------
	// ------------------------- RUS BUNKER 05 Turrets ----------------------
	// -----------------------------------------------------------------------
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(HeavyBunkerRadixiteCost - 200), 10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(HeavyBunkerMetalCost - 100), 10)
		},
	});
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(BxpHeavyBunkerHealth, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_Bunker05_WithTurrets, BxpData);


	// -----------------------------------------------------------------------
	// ------------------------- RUS BUNKER 02 ZIS ----------------------
	// -----------------------------------------------------------------------
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(HeavyBunkerRadixiteCost), 10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(HeavyBunkerMetalCost), 10)
		},
	});
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(BxpHeavyBunkerHealth, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_Bunker02_ZIS, BxpData);

	// -----------------------------------------------------------------------
	// ------------------------- RUS BUNKER 02 204MM CANNON ----------------------
	// -----------------------------------------------------------------------
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(HeavyBunkerRadixiteCost * 1.5), 20)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(HeavyBunkerMetalCost * 1.33), 20)
		},
	});
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(BxpHeavyBunkerHealth + 200, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_Bunker02_204MM, BxpData);

	// -----------------------------------------------------------------------
	// ----------------------------- RUS Long Camo Bunker---------------------
	// -----------------------------------------------------------------------
	BxpData.ConstructionTime = BxpT2BuildTime;
	BxpData.VisionRadius = T2BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT2BunkerRadixiteCost), 10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT2BunkerMetalCost * 1.1), 10)
		},
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T2BxpBunkerHealth, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_RusLongCamoBunker, BxpData);


	// -----------------------------------------------------------------------
	// ----------------------------- RUS Dome Bunker---------------------
	// -----------------------------------------------------------------------
	BxpData.ConstructionTime = BxpT2BuildTime;
	BxpData.VisionRadius = T2BxpVisionRadius;
	BxpData.Cost = FUnitCost({
		{
			ERTSResourceType::Resource_Radixite,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT2BunkerRadixiteCost * 1.3), 10)
		},
		{
			ERTSResourceType::Resource_Metal,
			RTSFunctionLibrary::RoundToNearestMultipleOf(static_cast<int32>(BxpT2BunkerMetalCost * 1.2), 10)
		},
	});
	BxpData.Abilities = ArmedBxpAbilities;
	BxpData.Health = RTSFunctionLibrary::RoundToNearestMultipleOf(T2BxpBunkerHealth * 1.33, 10);
	BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
	M_TPlayerBxpDataHashMap.Add(EBuildingExpansionType::BTX_RusDomeBunker, BxpData);


	// Important set the enemy data too.
	M_TEnemyBxpDataHashMap = M_TPlayerBxpDataHashMap;
}

void ACPPGameState::InitAllGameNomadicData()
{
	TArray<FUnitAbilityEntry> BasicNomadicAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdNoAbility, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility, EAbilityID::IdRotateTowards, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
	});

	TArray<FUnitAbilityEntry> NomadicWithWeaponsAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility, EAbilityID::IdRotateTowards, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
	});

	using namespace DeveloperSettings::GameBalance::UnitCosts;
	using namespace DeveloperSettings::GameBalance::BuildingRadii;
	using namespace DeveloperSettings::GameBalance::BuildingTime;
	using namespace DeveloperSettings::GameBalance::VisionRadii::BuildingVision;
	using namespace DeveloperSettings::GameBalance::VisionRadii::UnitVision;
	using namespace DeveloperSettings::GameBalance::Resources;
	using namespace DeveloperSettings::GameBalance::UnitHealth;
	using namespace DeveloperSettings::GameBalance::Experience;
	using namespace DeveloperSettings::GameBalance::UnitCosts::EnergySupplies;

	FNomadicData NomadicData;

	// --------------------------------------------------
	//                     Ger Nomadic
	// --------------------------------------------------

	// ---------------------------------Nomadic Ger HQ----------------------------
	NomadicData.Abilities = NomadicWithWeaponsAbilities;
	NomadicData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, 100},
		{ERTSResourceType::Resource_VehicleParts, 100}
	});
	NomadicData.EnergySupply = HqEnergySupply;
	// Expansion radius is for BXPs that are freely placed, building radius is for other buildings.
	NomadicData.ExpansionRadius = HQExpansionRadius;
	NomadicData.BuildRadius = HQBuildingRadius;
	NomadicData.BuildingExpansionOptions = InitBxpOptions({
		AsDefense(EBuildingExpansionType::BTX_37mmFlak),
		AsDefense(EBuildingExpansionType::BTX_LeFH_150mm),
		AsTech(EBuildingExpansionType::BTX_GerHQRadar),
		AsDefense(EBuildingExpansionType::BTX_GerHQPlatform),
		AsEconomic(EBuildingExpansionType::BTX_GerHQHarvester),
		AsEconomic(EBuildingExpansionType::BTX_GerHQRepairBay),
		AsDefense(EBuildingExpansionType::BTX_GerHQTower),
	});

	NomadicData.MaxAmountBuildingExpansions = 5;
	NomadicData.ExperienceLevels = GetNomadicTruckExpLevels();
	NomadicData.ExperienceMultiplier = 1.0;
	NomadicData.ExperienceWorth = HQNomadicExp;
	NomadicData.TrainingOptions = {
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_PzI_Harvester)),
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_Scavengers)),
		FTrainingOption(),
		FTrainingOption(),
		FTrainingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerGammaFacility)),
		FTrainingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerRefinery)),
		FTrainingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerMetalVault)),
		FTrainingOption(),
		FTrainingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerBarracks)),
		FTrainingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerMechanizedDepot)),
		FTrainingOption(),
		FTrainingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerLightSteelForge)),
		// todo armory.
		FTrainingOption(),
		FTrainingOption(EAllUnitType::UNType_Nomadic,
		                static_cast<uint8>(ENomadicSubtype::Nomadic_GerCommunicationCenter)),
		FTrainingOption(EAllUnitType::UNType_Nomadic,
		                static_cast<uint8>(ENomadicSubtype::Nomadic_GerAirbase)),
		FTrainingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerMedTankFactory)),
	};
	// BuildingAnimationTime with vehicle expansion time is total deployment time.
	NomadicData.BuildingAnimationTime = HQBuildingAnimationTime;
	NomadicData.VehicleExpansionTime = HQVehicleConversionTime;
	NomadicData.VehiclePackUpTime = HQPackUpTime;
	// Health and vehicle settings.
	NomadicData.MaxHealthBuilding = NomadicHQBuildingHealth;
	NomadicData.Building_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(
		NomadicData.MaxHealthBuilding);
	NomadicData.MaxHealthVehicle = NomadicHQTruckHealth;
	NomadicData.Vehicle_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetILightArmorResistances(
		NomadicData.MaxHealthVehicle);
	NomadicData.VisionRadiusAsBuilding = HQVisionRadius;
	NomadicData.VisionRadiusAsTruck = T2TankVisionRadius;
	NomadicData.VehicleRotationSpeed = 40;
	NomadicData.VehicleMaxSpeedKmh = 20;
	NomadicData.ResourceDropOffCapacity = {
		{ERTSResourceType::Resource_Radixite, HQBaseRadixiteStorage},
		{ERTSResourceType::Resource_Metal, HQBaseMetalStorage},
		{ERTSResourceType::Resource_VehicleParts, HQBaseVehiclePartsStorage}
	};
	M_TPlayerNomadicDataHashMap.Add(ENomadicSubtype::Nomadic_GerHq, NomadicData);

	// ---------------------------------Nomadic Ger Gamma Facility----------------------------
	NomadicData.Abilities = BasicNomadicAbilities;
	NomadicData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, NomadicSolarFarmRadixiteCost},
		{ERTSResourceType::Resource_Metal, NomadicSolarFarmMetalCost}
	});
	NomadicData.EnergySupply = T1EnergyBuildingSupply;
	NomadicData.ExpansionRadius = T1BuildingRadius - 200;
	NomadicData.BuildRadius = T1BuildingRadius;
	NomadicData.BuildingExpansionOptions = InitBxpOptions({
		AsDefense(EBuildingExpansionType::BTX_37mmFlak),
		AsEconomic(EBuildingExpansionType::BXT_SolarSmall),
		AsEconomic(EBuildingExpansionType::BXT_SolarLarge),
	});

	NomadicData.MaxAmountBuildingExpansions = 2;
	NomadicData.ExperienceLevels = GetNomadicTruckExpLevels();
	NomadicData.ExperienceMultiplier = 1.0;
	NomadicData.ExperienceWorth = T1NomadicExp;
	NomadicData.TrainingOptions = {};
	NomadicData.BuildingAnimationTime = BarracksBuildingAnimationTime;
	NomadicData.VehicleExpansionTime = T1TruckVehicleConversionTime;
	NomadicData.VehiclePackUpTime = T1TruckVehiclePackUpTime;
	NomadicData.MaxHealthBuilding = T1NomadicBuildingHealth;
	NomadicData.Building_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
		NomadicData.MaxHealthBuilding);
	NomadicData.MaxHealthVehicle = T1NomadicTruckHealth;
	NomadicData.Vehicle_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(
		NomadicData.MaxHealthVehicle);
	NomadicData.VisionRadiusAsBuilding = T1BuildingVisionRadius - 200;
	NomadicData.VisionRadiusAsTruck = T1TankVisionRadius;
	NomadicData.VehicleRotationSpeed = 40;
	NomadicData.VehicleMaxSpeedKmh = 20;
	NomadicData.ResourceDropOffCapacity = {};
	M_TPlayerNomadicDataHashMap.Add(ENomadicSubtype::Nomadic_GerGammaFacility, NomadicData);

	// ---------------------------------Nomadic Ger Refinery----------------------------
	NomadicData.Abilities = BasicNomadicAbilities;
	NomadicData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T1StorageBuildingRadixite},
		{ERTSResourceType::Resource_Metal, T1StorageBuildingMetal}
	});
	NomadicData.EnergySupply = T1ResourceStorageBuildingSupply;
	NomadicData.ExpansionRadius = T1BuildingRadius - 200;
	NomadicData.BuildRadius = 0;
	NomadicData.BuildingExpansionOptions = InitBxpOptions({
		AsDefense(EBuildingExpansionType::BTX_37mmFlak),
	});

	NomadicData.MaxAmountBuildingExpansions = 2;
	NomadicData.ExperienceLevels = GetNomadicTruckExpLevels();
	NomadicData.ExperienceMultiplier = 1.0;
	NomadicData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(T1NomadicExp * 1.2, 5);
	NomadicData.TrainingOptions = {};
	NomadicData.BuildingAnimationTime = T1ResourceStorageAnimationTime;
	NomadicData.VehicleExpansionTime = T1TruckVehicleConversionTime;
	NomadicData.VehiclePackUpTime = T1TruckVehiclePackUpTime;
	NomadicData.MaxHealthBuilding = T1NomadicBuildingHealth + 150;
	NomadicData.Building_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
		NomadicData.MaxHealthBuilding);
	NomadicData.MaxHealthVehicle = T1NomadicTruckHealth;
	NomadicData.Vehicle_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(
		NomadicData.MaxHealthVehicle);
	NomadicData.VisionRadiusAsBuilding = T1BuildingVisionRadius - 200;
	NomadicData.VisionRadiusAsTruck = T1TankVisionRadius;
	NomadicData.VehicleRotationSpeed = 40;
	NomadicData.VehicleMaxSpeedKmh = 20;
	NomadicData.ResourceDropOffCapacity = {
		{ERTSResourceType::Resource_Radixite, RefineryRadixiteStorage}
	};
	M_TPlayerNomadicDataHashMap.Add(ENomadicSubtype::Nomadic_GerRefinery, NomadicData);

	// ---------------------------------Nomadic Ger Metal Vault----------------------------
	NomadicData.Abilities = BasicNomadicAbilities;
	NomadicData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Metal, T1StorageBuildingRadixite},
		{ERTSResourceType::Resource_Metal, T1StorageBuildingMetal}
	});
	NomadicData.EnergySupply = T1ResourceStorageBuildingSupply;
	NomadicData.ExpansionRadius = T1BuildingRadius - 200;
	NomadicData.BuildRadius = 0;
	NomadicData.BuildingExpansionOptions = InitBxpOptions({
		AsDefense(EBuildingExpansionType::BTX_37mmFlak),
	});

	NomadicData.MaxAmountBuildingExpansions = 2;
	NomadicData.ExperienceLevels = GetNomadicTruckExpLevels();
	NomadicData.ExperienceMultiplier = 1.0;
	NomadicData.ExperienceWorth = T1NomadicExp;
	NomadicData.TrainingOptions = {};
	NomadicData.BuildingAnimationTime = T1ResourceStorageAnimationTime;
	NomadicData.VehicleExpansionTime = T1TruckVehicleConversionTime;
	NomadicData.VehiclePackUpTime = T1TruckVehiclePackUpTime;
	NomadicData.MaxHealthBuilding = T1NomadicBuildingHealth + 150;
	NomadicData.Building_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
		NomadicData.MaxHealthBuilding);
	NomadicData.MaxHealthVehicle = T1NomadicTruckHealth;
	NomadicData.Vehicle_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(
		NomadicData.MaxHealthVehicle);
	NomadicData.VisionRadiusAsBuilding = T1BuildingVisionRadius - 200;
	NomadicData.VisionRadiusAsTruck = T1TankVisionRadius;
	NomadicData.VehicleRotationSpeed = 40;
	NomadicData.VehicleMaxSpeedKmh = 20;
	NomadicData.ResourceDropOffCapacity = {
		{ERTSResourceType::Resource_Metal, MetalVaultMetalStorage},
		{ERTSResourceType::Resource_VehicleParts, MetalVaultVehiclePartsStorage}
	};
	M_TPlayerNomadicDataHashMap.Add(ENomadicSubtype::Nomadic_GerMetalVault, NomadicData);

	// ---------------------------------Nomadic Ger Barracks----------------------------
	NomadicData.Abilities = BasicNomadicAbilities;
	NomadicData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, NomadicBarracksRadixiteCost},
		{ERTSResourceType::Resource_Metal, NomadicBarracksMetalCost}
	});
	NomadicData.EnergySupply = T1BarracksSupply;
	NomadicData.ExpansionRadius = T1ExpansionRadius;
	NomadicData.BuildRadius = 0;
	NomadicData.BuildingExpansionOptions = InitBxpOptions({
		AsDefense(EBuildingExpansionType::BTX_37mmFlak),
		AsTech(EBuildingExpansionType::BTX_GerBarrackFuelCell),
		AsTech(EBuildingExpansionType::BTX_GerBarracksRadixReactor)
	});
	NomadicData.MaxAmountBuildingExpansions = 2;
	NomadicData.ExperienceLevels = GetNomadicTruckExpLevels();
	NomadicData.ExperienceMultiplier = 1.0;
	NomadicData.ExperienceWorth = T1NomadicExp;
	NomadicData.TrainingOptions = {
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_Scavengers)),
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_JagerTruppKar98k)),
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_SteelFistAssaultSquad)),
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_Gebirgsjagerin)),
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_Vultures)),
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_SniperTeam)),
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_SturmPionieren)),
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_PanzerGrenadiere)),
		FTrainingOption(),
		FTrainingOption(),
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_LightBringers)),
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_FeuerSturm)),
		FTrainingOption(),
		FTrainingOption(),
		FTrainingOption(),
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_SturmKommandos)),
	};
	NomadicData.BuildingAnimationTime = BarracksBuildingAnimationTime;
	NomadicData.VehicleExpansionTime = T1TruckVehicleConversionTime;
	NomadicData.VehiclePackUpTime = T1TruckVehiclePackUpTime;
	NomadicData.MaxHealthBuilding = T1NomadicBuildingHealth;
	NomadicData.Building_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
		NomadicData.MaxHealthBuilding);
	NomadicData.MaxHealthVehicle = T1NomadicTruckHealth;
	NomadicData.Vehicle_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(
		NomadicData.MaxHealthVehicle);
	NomadicData.VisionRadiusAsBuilding = T1BuildingVisionRadius;
	NomadicData.VisionRadiusAsTruck = T1TankVisionRadius;
	NomadicData.VehicleRotationSpeed = 40;
	NomadicData.VehicleMaxSpeedKmh = 20;
	NomadicData.ResourceDropOffCapacity = {};
	M_TPlayerNomadicDataHashMap.Add(ENomadicSubtype::Nomadic_GerBarracks, NomadicData);

	// ---------------------------------Nomadic Ger Mechanized Depot----------------------------
	NomadicData.Abilities = NomadicWithWeaponsAbilities;
	NomadicData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, NomadicMechanizedDepotRadixiteCost},
		{ERTSResourceType::Resource_Metal, NomadicMechanizedDepotMetalCost}
	});
	NomadicData.EnergySupply = T1MechanizedDepotSupply;
	NomadicData.ExpansionRadius = T1ExpansionRadius;
	NomadicData.BuildRadius = 0;
	NomadicData.BuildingExpansionOptions = InitBxpOptions({
		AsDefense(EBuildingExpansionType::BTX_37mmFlak),
	});
	NomadicData.MaxAmountBuildingExpansions = 2;
	NomadicData.ExperienceLevels = GetNomadicTruckExpLevels();
	NomadicData.ExperienceMultiplier = 1.0;
	NomadicData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(T1NomadicExp * 1.2, 5);
	NomadicData.TrainingOptions = {
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_Puma)),
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_Panzerwerfer)),
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_Sdkfz251)),
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_Sdkfz251_Mortar)),
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_Sdkfz251_Transport))
	};
	NomadicData.BuildingAnimationTime = MechanizedDepotAnimationTime;
	NomadicData.VehicleExpansionTime = T1TruckVehicleConversionTime;
	NomadicData.VehiclePackUpTime = T1TruckVehiclePackUpTime;
	NomadicData.MaxHealthBuilding = T1NomadicBuildingHealth + 100;
	NomadicData.Building_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
		NomadicData.MaxHealthBuilding);
	NomadicData.MaxHealthVehicle = T1NomadicTruckHealth;
	NomadicData.Vehicle_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(
		NomadicData.MaxHealthVehicle);
	NomadicData.VisionRadiusAsBuilding = T1BuildingVisionRadius;
	NomadicData.VisionRadiusAsTruck = T1TankVisionRadius;
	NomadicData.VehicleRotationSpeed = 40;
	NomadicData.VehicleMaxSpeedKmh = 20;
	NomadicData.ResourceDropOffCapacity = {};
	M_TPlayerNomadicDataHashMap.Add(ENomadicSubtype::Nomadic_GerMechanizedDepot, NomadicData);

	// ---------------------------------Nomadic Ger Light Steel Forge----------------------------
	NomadicData.Abilities = BasicNomadicAbilities;
	NomadicData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, NomadicT1VehicleFactoryRadixiteCost},
		{ERTSResourceType::Resource_Metal, NomadicT1VehicleFactoryMetalCost}
	});
	NomadicData.EnergySupply = T1VehicleFactorySupply;
	NomadicData.ExpansionRadius = T1ExpansionRadius + 100;
	NomadicData.BuildRadius = 0;
	NomadicData.BuildingExpansionOptions = InitBxpOptions({
		AsDefense(EBuildingExpansionType::BTX_37mmFlak),
		AsEconomic(EBuildingExpansionType::BTX_GerLFactoryRadixiteReactor),
		AsEconomic(EBuildingExpansionType::BTX_GerLFactoryFuelStorage),
	});

	NomadicData.MaxAmountBuildingExpansions = 2;
	NomadicData.ExperienceLevels = GetNomadicTruckExpLevels();
	NomadicData.ExperienceMultiplier = 1.0;
	NomadicData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(T1NomadicExp * 1.2, 5);
	NomadicData.TrainingOptions = {
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_PzII_F)),
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_Pz38t)),
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_Pz38t_R)),
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_PzI_15cm)),
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_PzJager))
	};
	NomadicData.BuildingAnimationTime = LightTankFactoryBuildingAnimationTime;
	NomadicData.VehicleExpansionTime = T1TruckVehicleConversionTime;
	NomadicData.VehiclePackUpTime = T1TruckVehiclePackUpTime;
	NomadicData.MaxHealthBuilding = T1NomadicBuildingHealth + 300;
	NomadicData.Building_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
		NomadicData.MaxHealthBuilding);
	NomadicData.MaxHealthVehicle = T1NomadicTruckHealth;
	NomadicData.Vehicle_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(
		NomadicData.MaxHealthVehicle);
	NomadicData.VisionRadiusAsBuilding = T1BuildingVisionRadius + 500;
	NomadicData.VisionRadiusAsTruck = T1TankVisionRadius;
	NomadicData.VehicleRotationSpeed = 40;
	NomadicData.VehicleMaxSpeedKmh = 20;
	NomadicData.ResourceDropOffCapacity = {};
	M_TPlayerNomadicDataHashMap.Add(ENomadicSubtype::Nomadic_GerLightSteelForge, NomadicData);

	// ------------------------------------ Tier II -----------------------------------------------

	// ---------------------------------Nomadic Ger Communication Center----------------------------
	NomadicData.Abilities = BasicNomadicAbilities;
	NomadicData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, NomadicT2CommunicationCenterRadixiteCost},
		{ERTSResourceType::Resource_Metal, NomadicT2CommunicationCenterMetalCost}
	});
	NomadicData.EnergySupply = T2CommunicationCenterSupply;
	// Expansion radius is for BXPs that are freely placed, building radius is for other buildings.
	NomadicData.ExpansionRadius = T2ExpansionRadius;
	NomadicData.BuildRadius = 0;
	NomadicData.BuildingExpansionOptions = InitBxpOptions({
		AsDefense(EBuildingExpansionType::BTX_37mmFlak),
	});

	NomadicData.MaxAmountBuildingExpansions = 2;
	NomadicData.ExperienceLevels = GetNomadicTruckExpLevels();
	NomadicData.ExperienceMultiplier = 1.0;
	NomadicData.ExperienceWorth = T2NomadicExp;
	NomadicData.TrainingOptions = {};
	NomadicData.BuildingAnimationTime = T2CommunicationCenterBuildingAnimationTime;
	NomadicData.VehicleExpansionTime = T2TruckVehicleConversionTime;
	NomadicData.VehiclePackUpTime = T2TruckVehiclePackUpTime;
	NomadicData.MaxHealthBuilding = RTSFunctionLibrary::RoundToNearestMultipleOf(T2NomadicBuildingHealth + 500, 10);
	NomadicData.Building_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
		NomadicData.MaxHealthBuilding);
	NomadicData.MaxHealthVehicle = T2NomadicTruckHealth;
	NomadicData.Vehicle_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(
		NomadicData.MaxHealthVehicle);
	NomadicData.VisionRadiusAsBuilding = T2BuildingRadius;
	NomadicData.VisionRadiusAsTruck = T2BuildingVisionRadius;
	NomadicData.VehicleRotationSpeed = 40;
	NomadicData.VehicleMaxSpeedKmh = 20;
	NomadicData.ResourceDropOffCapacity = {};
	M_TPlayerNomadicDataHashMap.Add(ENomadicSubtype::Nomadic_GerCommunicationCenter, NomadicData);

	// ---------------------------------T2 Nomadic Ger Medium Tank Factory----------------------------
	NomadicData.Abilities = BasicNomadicAbilities;
	NomadicData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, NomadicT2MedFactoryRadixiteCost},
		{ERTSResourceType::Resource_Metal, NomadicT2MedFactoryRadixiteCost}
	});
	NomadicData.EnergySupply = T2VehicleFactorySupply;
	NomadicData.ExpansionRadius = T2ExpansionRadius;
	NomadicData.BuildRadius = 0;
	NomadicData.BuildingExpansionOptions = InitBxpOptions({
		AsDefense(EBuildingExpansionType::BTX_37mmFlak),
	});

	NomadicData.MaxAmountBuildingExpansions = 2;
	NomadicData.ExperienceLevels = GetNomadicTruckExpLevels();
	NomadicData.ExperienceMultiplier = 1.0;
	NomadicData.ExperienceWorth = T2NomadicExp;
	NomadicData.TrainingOptions = {
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_PzIV_G)),
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_PzIV_F1)),
	};

	NomadicData.BuildingAnimationTime = T2MedTankFactoryBuildingAnimationTime;
	NomadicData.VehicleExpansionTime = T2TruckVehicleConversionTime;
	NomadicData.VehiclePackUpTime = T2TruckVehiclePackUpTime;
	NomadicData.MaxHealthBuilding = RTSFunctionLibrary::RoundToNearestMultipleOf(T2NomadicBuildingHealth + 800, 10);
	NomadicData.Building_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
		NomadicData.MaxHealthBuilding);
	NomadicData.MaxHealthVehicle = T2NomadicTruckHealth;
	NomadicData.Vehicle_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(
		NomadicData.MaxHealthVehicle);
	NomadicData.VisionRadiusAsBuilding = T2BuildingRadius;
	NomadicData.VisionRadiusAsTruck = T2BuildingVisionRadius;
	NomadicData.VehicleRotationSpeed = 40;
	NomadicData.VehicleMaxSpeedKmh = 20;
	NomadicData.ResourceDropOffCapacity = {};
	M_TPlayerNomadicDataHashMap.Add(ENomadicSubtype::Nomadic_GerMedTankFactory, NomadicData);

	// ---------------------------------Nomadic Ger Training Centre----------------------------
	NomadicData.Abilities = BasicNomadicAbilities;
	NomadicData.Cost = FUnitCost({}); // Missing before, using default
	NomadicData.EnergySupply = T2TrainingCenterSupply;
	NomadicData.ExpansionRadius = 0; // Missing before, using default
	NomadicData.BuildRadius = 0; // Missing before, using default
	NomadicData.BuildingExpansionOptions = InitBxpOptions({
		AsDefense(EBuildingExpansionType::BTX_37mmFlak),
	});

	NomadicData.MaxAmountBuildingExpansions = 2;
	NomadicData.ExperienceLevels = GetNomadicTruckExpLevels();
	NomadicData.ExperienceMultiplier = 1.0;
	NomadicData.ExperienceWorth = T2NomadicExp;
	NomadicData.TrainingOptions = {
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_PzIV_G))
	};
	NomadicData.BuildingAnimationTime = 3.f;
	NomadicData.VehicleExpansionTime = 3.f;
	NomadicData.VehiclePackUpTime = 0; // Missing before, using default
	NomadicData.MaxHealthBuilding = T2NomadicBuildingHealth - 100;
	NomadicData.Building_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
		NomadicData.MaxHealthBuilding);
	NomadicData.MaxHealthVehicle = T2NomadicTruckHealth - 50;
	NomadicData.Vehicle_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(
		NomadicData.MaxHealthVehicle);
	NomadicData.VisionRadiusAsBuilding = 0; // Missing before, using default
	NomadicData.VisionRadiusAsTruck = 0; // Missing before, using default
	NomadicData.VehicleRotationSpeed = 40;
	NomadicData.VehicleMaxSpeedKmh = 20;
	NomadicData.ResourceDropOffCapacity = {};
	M_TPlayerNomadicDataHashMap.Add(ENomadicSubtype::Nomadic_GerTrainingCentre, NomadicData);

	// ---------------------------------T2 Nomadic Ger Airbase----------------------------
	NomadicData.Abilities = BasicNomadicAbilities;
	NomadicData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, NomadicT2AirbaseRadixiteCost},
		{ERTSResourceType::Resource_Metal, NomadicT2AirbaseMetalCost}
	});
	NomadicData.EnergySupply = T2AirPadSupply;
	NomadicData.ExpansionRadius = T2ExpansionRadius;
	NomadicData.BuildRadius = 0;
	NomadicData.BuildingExpansionOptions = InitBxpOptions({
		AsDefense(EBuildingExpansionType::BTX_37mmFlak),
		AsDefense(EBuildingExpansionType::BTX_GerHQPlatform),
	});

	NomadicData.MaxAmountBuildingExpansions = 2;
	NomadicData.ExperienceLevels = GetNomadicTruckExpLevels();
	NomadicData.ExperienceMultiplier = 1.0;
	NomadicData.ExperienceWorth = T2NomadicExp;
	NomadicData.TrainingOptions = {
		FTrainingOption(EAllUnitType::UNType_Aircraft, static_cast<uint8>(EAircraftSubtype::Aircraft_Ju87)),
		FTrainingOption(EAllUnitType::UNType_Aircraft, static_cast<uint8>(EAircraftSubtype::Aircraft_Me410)),
		FTrainingOption(EAllUnitType::UNType_Aircraft, static_cast<uint8>(EAircraftSubtype::Aircraft_Bf109)),
	};

	NomadicData.BuildingAnimationTime = T2MedTankFactoryBuildingAnimationTime;
	NomadicData.VehicleExpansionTime = T2TruckVehicleConversionTime;
	NomadicData.VehiclePackUpTime = T2TruckVehiclePackUpTime;
	NomadicData.MaxHealthBuilding = RTSFunctionLibrary::RoundToNearestMultipleOf(T2NomadicBuildingHealth + 1200, 10);
	NomadicData.Building_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
		NomadicData.MaxHealthBuilding);
	NomadicData.MaxHealthVehicle = T2NomadicTruckHealth;
	NomadicData.Vehicle_ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIArmoredCarResistances(
		NomadicData.MaxHealthVehicle);
	NomadicData.VisionRadiusAsBuilding = T2BuildingRadius;
	NomadicData.VisionRadiusAsTruck = T2BuildingVisionRadius;
	NomadicData.VehicleRotationSpeed = 40;
	NomadicData.VehicleMaxSpeedKmh = 20;
	NomadicData.ResourceDropOffCapacity = {};
	NomadicData.RepairPowerMlt = 2.f;
	M_TPlayerNomadicDataHashMap.Add(ENomadicSubtype::Nomadic_GerAirbase, NomadicData);

	M_TEnemyNomadicDataHashMap = M_TPlayerNomadicDataHashMap;
}


void ACPPGameState::InitAllGameSquadData()
{
	using namespace DeveloperSettings::GameBalance::UnitHealth;
	using namespace DeveloperSettings::GameBalance::UnitCosts::InfantryCosts;

	using DeveloperSettings::GameBalance::InfantrySettings::BasicInfantrySpeed;
	using DeveloperSettings::GameBalance::InfantrySettings::BasicInfantryAcceleration;
	using DeveloperSettings::GameBalance::InfantrySettings::FastInfantrySpeed;
	using DeveloperSettings::GameBalance::InfantrySettings::FastInfantryAcceleration;
	using DeveloperSettings::GameBalance::InfantrySettings::SlowInfantrySpeed;
	using DeveloperSettings::GameBalance::InfantrySettings::SlowInfantryAcceleration;

	using namespace DeveloperSettings::GameBalance::UnitCosts;
	using namespace DeveloperSettings::GameBalance::VisionRadii::UnitVision;
	using namespace DeveloperSettings::GameBalance::Experience;
	TArray<FUnitAbilityEntry> BasicSquadAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdPatrol, EAbilityID::IdRetreat,
		EAbilityID::IdSwitchWeapon, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdPickupItem, EAbilityID::IdEnterCargo, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility
	});

	TArray<FUnitAbilityEntry> BasicScavengerAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
		EAbilityID::IdAttack, EAbilityID::IdMove, EAbilityID::IdStop, EAbilityID::IdPatrol, EAbilityID::IdRetreat,
		EAbilityID::IdSwitchWeapon, EAbilityID::IdScavenge, EAbilityID::IdRepair, EAbilityID::IdNoAbility,
		EAbilityID::IdAttackGround,
		EAbilityID::IdPickupItem, EAbilityID::IdEnterCargo, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility,
	});
	FSquadData SquadData;
	// --------------------------------------------------
	//                     Ger Infantry
	// --------------------------------------------------
	SquadData.MaxHealth = BasicInfantryHealth;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicInfantryResistances(SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, ScavengersRadixiteCost}
	});
	SquadData.Abilities = BasicScavengerAbilities;
	SquadData.ExperienceWorth = BaseInfantryExp;
	SquadData.ExperienceLevels = GetTier1InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Ger_Scavengers, SquadData);

	SquadData.MaxHealth = BasicInfantryHealth;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicInfantryResistances(SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, JagerRadixiteCost}
	});
	SquadData.Abilities = BasicSquadAbilities;
	SquadData.ExperienceLevels = GetTier1InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	SquadData.ExperienceWorth = BaseInfantryExp;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Ger_JagerTruppKar98k, SquadData);

	SquadData.MaxHealth = ArmoredInfantryHealth;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyInfantryArmorResistances(
		SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = SlowInfantrySpeed;
	SquadData.MaxAcceleration = SlowInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, ArmoredInfantryRadixiteCost},
		{ERTSResourceType::Resource_Metal, ArmoredInfantryMetalCost}
	});
	SquadData.Abilities = BasicSquadAbilities;
	SquadData.ExperienceWorth = BaseInfantryExp + 5;
	SquadData.ExperienceLevels = GetTier1InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Ger_SteelFistAssaultSquad, SquadData);


	SquadData.MaxHealth = RTSFunctionLibrary::RoundToNearestMultipleOf(BasicInfantryHealth * 1.67, 10);
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicInfantryResistances(SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, BasicSniperRadixiteCost}
	});
	SquadData.Abilities = BasicScavengerAbilities;
	SquadData.ExperienceWorth = BaseInfantryExp;
	SquadData.ExperienceLevels = GetTier1InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Ger_Gebirgsjagerin, SquadData);

	// ----------------------------
	// -------- Ger Armory Infantry
	// -----------------------------

	SquadData.Abilities = BasicSquadAbilities;
	SquadData.MaxHealth = DeveloperSettings::GameBalance::UnitHealth::ArmoredHazmatInfantryHealth + 40;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetArmoredIHazmatInfantryResistances(
		SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, VulturesFg42RadixiteCost},
		{ERTSResourceType::Resource_Metal, VulturesFg42MetalCost}
	});
	SquadData.ExperienceLevels = GetTier2InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	SquadData.ExperienceWorth = HazmatInfantryExp;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Ger_Vultures, SquadData);

	SquadData.MaxHealth = DeveloperSettings::GameBalance::UnitHealth::ArmoredHazmatInfantryHealth;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicInfantryResistances(SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = FastInfantrySpeed;
	SquadData.MaxAcceleration = FastInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, SniperTeamRadixiteCost},
		{ERTSResourceType::Resource_Metal, SniperTeamMetalCost}
	});
	SquadData.ExperienceLevels = GetTier2InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	SquadData.ExperienceWorth = HazmatInfantryExp;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Ger_SniperTeam, SquadData);

	// --------------------------------------------
	// ---------------- T2 Infantry ---------------
	// --------------------------------------------
	SquadData.MaxHealth = T2InfantryHealth;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyInfantryArmorResistances(
		SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T2FeuerStormRadixiteCosts},
		{ERTSResourceType::Resource_Metal, T2FeuerStormMetalCosts},
	});
	SquadData.Abilities = BasicSquadAbilities;
	SquadData.ExperienceLevels = GetTier2InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	SquadData.ExperienceWorth = T2InfantryExp;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Ger_FeuerSturm, SquadData);


	SquadData.MaxHealth = T2InfantryHealth - 25;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyInfantryArmorResistances(
		SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T2SturmPioneerRadixiteCosts},
		{ERTSResourceType::Resource_Metal, T2SturmPioneerMetalCosts},
	});
	SquadData.Abilities = BasicScavengerAbilities;
	SquadData.ExperienceLevels = GetTier2InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	SquadData.ExperienceWorth = T2InfantryExp;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Ger_SturmPionieren, SquadData);


	// --------------------------------------------
	// ---------------- Elite Infantry ---------------
	// --------------------------------------------
	SquadData.MaxHealth = EliteInfantryHealth + 100;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyInfantryArmorResistances(
		SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = SlowInfantrySpeed;
	SquadData.MaxAcceleration = SlowInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, PanzerGrenadierRadixiteCosts},
		{ERTSResourceType::Resource_Metal, PanzerGrenadierMetalCosts}
	});
	SquadData.Abilities = BasicSquadAbilities;
	SquadData.ExperienceLevels = GetTier2InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	SquadData.ExperienceWorth = EliteInfantryExp;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Ger_PanzerGrenadiere, SquadData);


	SquadData.MaxHealth = CommandoInfantryHealth;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetCommandoInfantryArmorResistances(
		SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = FastInfantrySpeed;
	SquadData.MaxAcceleration = FastInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{
			{ERTSResourceType::Resource_Radixite, SturmkommandoRadixiteCost},
			{ERTSResourceType::Resource_Metal, SturmkommandoMetalCost}
		}
	});
	SquadData.Abilities = BasicSquadAbilities;
	SquadData.ExperienceLevels = GetTier2InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	SquadData.ExperienceWorth = RTSFunctionLibrary::RoundToNearestMultipleOf(EliteInfantryExp * 1.33, 5);
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Ger_SturmKommandos, SquadData);


	SquadData.MaxHealth = EliteInfantryHealth;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyInfantryArmorResistances(
		SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, LightBringersRadixiteCosts},
		{ERTSResourceType::Resource_Metal, LightBringersMetalCosts}
	});
	SquadData.Abilities = BasicSquadAbilities;
	SquadData.ExperienceLevels = GetTier2InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	SquadData.ExperienceWorth = EliteInfantryExp;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Ger_LightBringers, SquadData);


	SquadData.MaxHealth = EliteInfantryHealth;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyInfantryArmorResistances(
		SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, EliteInfantryRadixiteCost},
		{ERTSResourceType::Resource_Metal, EliteInfantryMetalCost}
	});
	SquadData.Abilities = BasicSquadAbilities;
	SquadData.ExperienceLevels = GetTier2InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	SquadData.ExperienceWorth = EliteInfantryExp;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Ger_IronStorm, SquadData);

	// --------------------------------------------------
	//                     Rus Infantry
	// --------------------------------------------------

	// --------------------------------------------------
	//                     Basic Rus Infantry
	// --------------------------------------------------

	SquadData.MaxHealth = DeveloperSettings::GameBalance::UnitHealth::HazmatInfantryHealth;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHazmatInfantryResistances(SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, HazmatEngineersRadixiteCost}
	});
	SquadData.Abilities = BasicSquadAbilities;
	SquadData.ExperienceWorth = HazmatInfantryExp;
	SquadData.ExperienceLevels = GetTier1InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Rus_HazmatEngineers, SquadData);

	SquadData.MaxHealth = BasicInfantryHealth;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicInfantryResistances(SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, MosinSquadRadixiteCost}
	});
	SquadData.Abilities = BasicSquadAbilities;
	SquadData.ExperienceWorth = BaseInfantryExp;
	SquadData.ExperienceLevels = GetTier1InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Rus_Mosin, SquadData);

	SquadData.MaxHealth = BasicInfantryHealth;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicInfantryResistances(SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, PTRS_SquadRadixiteCost}
	});
	SquadData.Abilities = BasicSquadAbilities;
	SquadData.ExperienceWorth = BaseInfantryExp;
	SquadData.ExperienceLevels = GetTier1InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Rus_Okhotnik, SquadData);


	// --------------------------------------------------
	//                     T2 Rus Infantry
	// --------------------------------------------------
	SquadData.MaxHealth = ArmoredInfantryHealth + 75;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyInfantryArmorResistances(
		SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = SlowInfantrySpeed;
	SquadData.MaxAcceleration = SlowInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T2RedHammerRadixiteCosts},
		{ERTSResourceType::Resource_Metal, T2RedHammerMetalCosts}
	});
	SquadData.Abilities = BasicSquadAbilities;
	SquadData.ExperienceWorth = ArmoredInfantryExp + 8;
	SquadData.ExperienceLevels = GetTier2InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Rus_RedHammer, SquadData);

	// uses same armor as T1 hazmat but has more health.
	SquadData.MaxHealth = DeveloperSettings::GameBalance::UnitHealth::HazmatInfantryHealth + 50;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHazmatInfantryResistances(SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, T2ToxicGuardRadixiteCosts},
	});
	SquadData.ExperienceLevels = GetTier2InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	SquadData.ExperienceWorth = HazmatInfantryExp;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Rus_ToxicGuard, SquadData);

	// --------------------------------------------
	// --------- Russian Elite Infantry
	// --------------------------------------------

	SquadData.MaxHealth = EliteInfantryHealth;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyInfantryArmorResistances(
		SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, EliteRusCyborgAtRadixiteCost},
		{ERTSResourceType::Resource_Metal, EliteRusCyborgAtMetalCost}
	});
	SquadData.Abilities = BasicSquadAbilities;
	SquadData.ExperienceLevels = GetTier2InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	SquadData.ExperienceWorth = EliteInfantryExp;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Rus_Kvarc77, SquadData);


	SquadData.MaxHealth = EliteInfantryHealth;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyInfantryArmorResistances(
		SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, EliteRusCyborgFedrovRadixiteCost},
		{ERTSResourceType::Resource_Metal, EliteRusCyborgFedrovRadixiteCost}
	});
	SquadData.Abilities = BasicSquadAbilities;
	SquadData.ExperienceLevels = GetTier2InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	SquadData.ExperienceWorth = EliteInfantryExp;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Rus_Tucha12T, SquadData);

	SquadData.MaxHealth = EliteInfantryHealth;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicInfantryResistances(
		SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, GhostOfStalingradRadixiteCost}
	});
	SquadData.Abilities = BasicSquadAbilities;
	SquadData.ExperienceLevels = GetTier2InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	SquadData.ExperienceWorth = EliteInfantryExp;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Rus_GhostsOfStalingrad, SquadData);

	SquadData.MaxHealth = CommandoInfantryHealth;
	SquadData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIHeavyInfantryArmorResistances(
		SquadData.MaxHealth);
	SquadData.MaxWalkSpeedCms = BasicInfantrySpeed;
	SquadData.MaxAcceleration = BasicInfantryAcceleration;
	SquadData.VisionRadius = InfantryVisionRadius;
	SquadData.Cost = FUnitCost({
		{ERTSResourceType::Resource_Radixite, CortextOfficerRadixiteCost},
		{ERTSResourceType::Resource_Metal, CortextOfficerMetalCost}
	});
	SquadData.Abilities = BasicSquadAbilities;
	SquadData.ExperienceLevels = GetTier2InfantryExpLevels();
	SquadData.ExperienceMultiplier = 1.0;
	SquadData.ExperienceWorth = EliteInfantryExp;
	M_TPlayerSquadDataHashMap.Add(ESquadSubtype::Squad_Rus_CortexOfficer, SquadData);

	M_TEnemySquadDataHashMap = M_TPlayerSquadDataHashMap;
}

void ACPPGameState::NotifyPlayerGameSettingsLoaded()
{
	// Get player controller of player one.
	const UWorld* World = GetWorld();
	if (IsValid(World) && IsValid(M_RTSGameSettings))
	{
		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
		if (IsValid(PlayerController))
		{
			if (ACPPController* RTSController = Cast<ACPPController>(PlayerController))
			{
				RTSController->OnRTSGameSettingsLoaded(M_RTSGameSettings);
			}
			else
			{
				RTSFunctionLibrary::ReportFailedCastError(
					"PlayerController",
					"ACPPController",
					"ACPPGameState::NotifyPlayerGameSettingsLoaded");
			}
		}
		else
		{
			RTSFunctionLibrary::ReportError("player controller not found for player 0");
		}
	}
}

void ACPPGameState::InitializeSmallArmsProjectileManager()
{
	UWorld* World = GetWorld();
	if (not IsValid(ProjectileManagerClass) || not IsValid(World))
	{
		RTSFunctionLibrary::ReportError("ProjectileManagerClass is not valid or Invalid world"
			"\n In ACPPGameState::InitializeSmallArmsProjectileManager");
		return;
	}
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_ProjectileManagerCallbacks.ProjectileManager = World->SpawnActor<ASmallArmsProjectileManager>(
		ProjectileManagerClass, FVector::ZeroVector, FRotator::ZeroRotator,
		SpawnParams);
	if (not IsValid(M_ProjectileManagerCallbacks.ProjectileManager))
	{
		RTSFunctionLibrary::ReportError("attempted but failed to spawn projectile manager of class: + "
			+ ProjectileManagerClass->GetName() + " in ACPPGameState::InitializeSmallArmsProjectileManager");
		return;
	}
	// Notify any weapon that was waiting that the projectile manager is loaded.
	M_ProjectileManagerCallbacks.OnProjectileManagerLoaded.Broadcast(M_ProjectileManagerCallbacks.ProjectileManager);
	M_ProjectileManagerCallbacks.OnProjectileManagerLoaded.Clear();
}

void ACPPGameState::InitializeWeaponVFXSettings()
{
	GLOBAL_RefreshShellColorsFromSettings();
}
