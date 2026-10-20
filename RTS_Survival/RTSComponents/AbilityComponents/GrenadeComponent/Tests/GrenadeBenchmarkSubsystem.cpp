// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GrenadeBenchmarkSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "GrenadeTestHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Enemy/EnemyUnitAbilityAI/EnemyUnitAbilityAISettings.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeComponent.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/RTSRadiusPoolSubsystem/RTSRadiusPoolSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSGrenadeBench, Log, All);

namespace GrenadeBenchPrivate
{
	const TCHAR* const BenchmarkSwitch = TEXT("GrenadeBenchmark");
	const TCHAR* const DirectorySwitch = TEXT("GrenadeBenchDir=");
	const TCHAR* const OnlySwitch = TEXT("GrenadeBenchOnly=");
	const TCHAR* const OverrideSwitch = TEXT("GrenadeBenchOverride=");
	const TCHAR* const LocationSwitch = TEXT("GrenadeBenchAt=");
	// Class path of another infantry target squad, e.g. to compare how two squads take fire damage.
	const TCHAR* const InfantryTargetSwitch = TEXT("GrenadeBenchInfantry=");

	const TCHAR* const FaustSquad = TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/SteelFistAssaultSquad/Squad_Ger_Faust.Squad_Ger_Faust_C");
	const TCHAR* const JagerSquad = TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/JagerTrupp/Squad_Ger_JagerTrupp.Squad_Ger_JagerTrupp_C");
	const TCHAR* const LightBringerSquad = TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/EliteInfantry/LightBringers/Squad_Ger_LightBringers.Squad_Ger_LightBringers_C");
	const TCHAR* const MosinSquad = TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin.Rus_Squad_Mosin_C");
	const TCHAR* const HazmatSniperSquad = TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/ArmoryUnlocked/Squad_Ger_HazmatSniperTeam.Squad_Ger_HazmatSniperTeam_C");
	// Soviet WW2 grenades (Docs/Abilities/SovietGrenades.md).
	const TCHAR* const RedHammerSquad = TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/T2/Rus_T2Squad_RedHammer.Rus_T2Squad_RedHammer_C");
	const TCHAR* const RedHammerPTRSSquad = TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/T2/Rus_T2Squad_RedHammerPTRS.Rus_T2Squad_RedHammerPTRS_C");
	const TCHAR* const LargeMixSquad = TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_LargeMixBasic.Rus_Squad_LargeMixBasic_C");
	const TCHAR* const SVTSquad = TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_SVT.Rus_Squad_SVT_C");
	// Five soldiers in the normal squad formation: shows how much of a squad one blast reaches. It is German because
	// soldiers keep their Blueprint's collision side: Soviet soldiers collide as enemies and the enemy-owned thrower
	// squad's grenades only hit player objects. Squads of more than eight soldiers stack the extra ones on the spawn
	// grid origin (ASquadController::FindIdealSpawnLocation_GetNextGridPoint).
	const TCHAR* const InfantryTarget = TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/Scavengers/Squad_Ger_Scavengers.Squad_Ger_Scavengers_C");
	const TCHAR* const LightTank = TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Light/T70/Rus_Vehicle_T-70.Rus_Vehicle_T-70_C");
	const TCHAR* const MediumTank = TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");
	const TCHAR* const HeavyTank = TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Heavy/KV1/Rus_Vehicle_KV-1E.Rus_Vehicle_KV-1E_C");
	const TCHAR* const GermanMediumTank = TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H.Ger_Vehicle_PanzerIV_H_C");

	// Soldiers keep the owner and collision side of their own Blueprint; the grenade takes its victims from the
	// squad's owner. An enemy-owned thrower squad of German soldiers therefore hits German (player) targets, and a
	// player-owned thrower squad hits Soviet soldiers, whose Blueprints collide as enemies.
	constexpr uint8 PlayerOwner = 1;
	constexpr uint8 EnemyOwner = 2;
	// Soviet riflemen: the infantry target of the player-direction scenarios.
	const TCHAR* const EnemyInfantryTarget = TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin.Rus_Squad_Mosin_C");

	// Real seconds: the HUD and start widget need a moment before the game can be started.
	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 90.0;
	// Game seconds.
	constexpr float MaxUnitLoadSeconds = 60.f;
	constexpr float UnitSettleSeconds = 3.f;
	constexpr float MaxTankApproachSeconds = 40.f;
	constexpr float MaxExplosionWaitSeconds = 45.f;
	// The rear-armor AOE runs as an async sweep; damage lands a frame or two after the explosion.
	constexpr float DamageSettleSeconds = 1.5f;
	constexpr float BurnSettleMargin = 0.5f;
	constexpr float ExplosionPictureDelay = 0.15f;

	constexpr int32 MinLivingSoldiers = 2;
	// Well inside the throw range of every grenade (800 and up), so the squad throws from where it stands; a squad
	// controller that ends up just out of range first walks, and slow squads then miss the explosion timeout.
	const FVector ThrowerOffset(-550.f, 0.f, 0.f);
	// The driving tank crosses in front of the throwers, from MovingTankStart to MovingTankEnd.
	// The lane passes 400 cm in front of the throwers, well inside every throw range.
	const FVector MovingTankStart(-150.f, -2200.f, 0.f);
	const FVector MovingTankEnd(-150.f, 2200.f, 0.f);
	// The order goes out once the driving tank is within this share of the throw range of the throwers, like a
	// player clicking on a tank that passes in range. A tank ordered at from further away is chased to where it was
	// and followed only while it stays within twice the throw range.
	constexpr float MovingTankThrowRangeFraction = 0.9f;
	// Explosions further than this from the scenario spot belong to someone else on the map.
	constexpr float ScenarioRadius = 3500.f;
	// Aim at the hull top, like a click on the tank.
	constexpr float TankAimHeightFraction = 0.8f;

	const FVector NavSearchExtent(20000.f, 20000.f, 5000.f);
	const FVector SpotNavExtent(3000.f, 3000.f, 2000.f);
	constexpr float SpotSpacing = 4000.f;
	constexpr int32 SpotsPerRow = 3;
	// Soldiers further than this from the aim point when the order goes out did not spawn with their squad.
	constexpr float VictimMaxDistanceFromAimPoint = 1000.f;

	// Per-axis median: a soldier that spawned far from his squad does not drag the aim point into empty ground.
	FVector GetMedianLocation(TArray<FVector> Locations)
	{
		if (Locations.IsEmpty())
		{
			return FVector::ZeroVector;
		}
		const int32 Middle = Locations.Num() / 2;
		FVector Median;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Locations.Sort([Axis](const FVector& A, const FVector& B) { return A[Axis] < B[Axis]; });
			Median[Axis] = Locations[Middle][Axis];
		}
		return Median;
	}
	// Below this the AOE sweep does nothing at all (FRTS_AOE::StartAsyncSphereSweep).
	constexpr float MinimumWorkingAoeRadius = 85.f;

	FString GetGrenadeName(const EGrenadeAbilityType Grenade)
	{
		return StaticEnum<EGrenadeAbilityType>()->GetNameStringByValue(static_cast<int64>(Grenade));
	}

	FString GetDistanceSourceName(const EAoeDistanceSource Source)
	{
		switch (Source)
		{
		case EAoeDistanceSource::Collision:
			return TEXT("collision");
		case EAoeDistanceSource::BoundsBox:
			return TEXT("bounds");
		default:
			return TEXT("origin");
		}
	}
}

bool UGrenadeBenchmarkSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), GrenadeBenchPrivate::BenchmarkSwitch);
}

void UGrenadeBenchmarkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Enemy squads would otherwise throw at the waiting targets before their scenario starts.
	GetMutableDefault<UEnemyUnitAbilityAISettings>()->bEnabled = false;
}

void UGrenadeBenchmarkSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	M_OutputDirectory = FPaths::ProjectSavedDir() / TEXT("GrenadeBenchmark");
	FParse::Value(FCommandLine::Get(), GrenadeBenchPrivate::DirectorySwitch, M_OutputDirectory);
	BuildScenarios();
	ApplyScenarioFilter();
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	M_CsvText = TEXT("scenario,grenade,target,radius,damage,exponent,fuse_setting,stick,thrown,exploded,stuck,")
		TEXT("fuse_measured,rear_armor,armor_multiplier,health_before,health_after,dealt_percent,killed,")
		TEXT("soldiers,in_radius,damaged,soldiers_killed,closest_distance,distance_source\n");
	EnterPhase(EGrenadeBenchPhase::WaitingForGameStart);
	UE_LOG(LogRTSGrenadeBench, Display, TEXT("RTS_GRENADE_BENCH started: %d scenarios, output %s"), M_Scenarios.Num(),
	       *M_OutputDirectory);
}

void UGrenadeBenchmarkSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

void UGrenadeBenchmarkSubsystem::Tick(float DeltaTime)
{
	if (M_Phase == EGrenadeBenchPhase::Inactive || M_Phase == EGrenadeBenchPhase::Done || not IsValid(GetWorld()))
	{
		return;
	}
	if (M_Phase != EGrenadeBenchPhase::WaitingForGameStart)
	{
		FGrenadeTestHelpers::KeepGameRunning(*GetWorld(), M_PlayerController.Get());
		BindNewGrenades();
		TrackVictimHealth();
		SilenceTarget();
	}
	switch (M_Phase)
	{
	case EGrenadeBenchPhase::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case EGrenadeBenchPhase::WaitingForUnits:
		TickWaitingForUnits();
		break;
	case EGrenadeBenchPhase::WaitingForTankApproach:
		TickWaitingForTankApproach();
		break;
	case EGrenadeBenchPhase::WaitingForExplosions:
		TickWaitingForExplosions();
		break;
	case EGrenadeBenchPhase::WaitingForDamage:
		TickWaitingForDamage();
		break;
	default:
		break;
	}
}

TStatId UGrenadeBenchmarkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGrenadeBenchmarkSubsystem, STATGROUP_Tickables);
}

void UGrenadeBenchmarkSubsystem::EnterPhase(const EGrenadeBenchPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartGameSeconds = GetGameSeconds();
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
}

float UGrenadeBenchmarkSubsystem::GetGameSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

float UGrenadeBenchmarkSubsystem::GetGameSecondsInPhase() const
{
	return GetGameSeconds() - M_PhaseStartGameSeconds;
}

double UGrenadeBenchmarkSubsystem::GetRealSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartRealSeconds;
}

void UGrenadeBenchmarkSubsystem::Check(const bool bCondition, const FString& Description)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogRTSGrenadeBench, Display, TEXT("RTS_GRENADE_BENCH check=%s %s"), bCondition ? TEXT("PASS") : TEXT("FAIL"),
	       *Description);
}

void UGrenadeBenchmarkSubsystem::Finish()
{
	M_Phase = EGrenadeBenchPhase::Done;
	for (const FString& Line : M_SummaryLines)
	{
		UE_LOG(LogRTSGrenadeBench, Display, TEXT("RTS_GRENADE_BENCH summary %s"), *Line);
	}
	FFileHelper::SaveStringToFile(M_CsvText, *(M_OutputDirectory / TEXT("GrenadeBenchmark.csv")));
	UE_LOG(LogRTSGrenadeBench, Display, TEXT("RTS_GRENADE_BENCH RESULT %s failed_checks=%d"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount);
	FPlatformMisc::RequestExit(false, TEXT("GrenadeBenchmark"));
}

void UGrenadeBenchmarkSubsystem::BuildScenarios()
{
	using namespace GrenadeBenchPrivate;
	using EGrenade = EGrenadeAbilityType;
	using ETarget = EGrenadeBenchTarget;
	/**
	 * Expectations sit below the tuned results of 2026-10-16 (Docs/Abilities/GrenadeQuality.md) with margin:
	 * two Faust AT grenades kill a T-70, take 83 % off a T-34 / Panzer IV and 43 % off a KV-1.
	 */
	const auto Add = [this](const TCHAR* Name, const TCHAR* Thrower, const EGrenadeAbilityType Grenade,
	                        const EGrenadeBenchTarget Target, const TCHAR* TargetClass, const float MinDealtPercent,
	                        const int32 MinSoldiersHurt, const bool bExpectAllStuck)
	{
		FGrenadeBenchScenario& Scenario = M_Scenarios.AddDefaulted_GetRef();
		Scenario.Name = Name;
		Scenario.ThrowerClassPath = Thrower;
		Scenario.Grenade = Grenade;
		Scenario.Target = Target;
		Scenario.TargetClassPath = TargetClass;
		Scenario.MinDealtPercent = MinDealtPercent;
		Scenario.MinSoldiersHurt = MinSoldiersHurt;
		Scenario.bExpectAllStuck = bExpectAllStuck;
	};
	constexpr float NotChecked = -1.f;
	constexpr int32 NoSoldierCheck = -1;
	Add(TEXT("AT_vs_T70"), FaustSquad, EGrenade::GerAtGrenade, ETarget::TankStill, LightTank, 60.f, NoSoldierCheck, true);
	Add(TEXT("AT_vs_T34"), FaustSquad, EGrenade::GerAtGrenade, ETarget::TankStill, MediumTank, 40.f, NoSoldierCheck, true);
	Add(TEXT("AT_vs_KV1"), FaustSquad, EGrenade::GerAtGrenade, ETarget::TankStill, HeavyTank, 20.f, NoSoldierCheck, true);
	Add(TEXT("AT_vs_PzIV"), FaustSquad, EGrenade::GerAtGrenade, ETarget::TankStill, GermanMediumTank, 40.f,
	    NoSoldierCheck, true);
	Add(TEXT("AT_vs_T34_Driving"), FaustSquad, EGrenade::GerAtGrenade, ETarget::TankMoving, MediumTank, 40.f,
	    NoSoldierCheck, true);
	Add(TEXT("AT_Hazmat_vs_T34"), HazmatSniperSquad, EGrenade::GerAtGrenade, ETarget::TankStill, MediumTank, 40.f,
	    NoSoldierCheck, true);
	Add(TEXT("AT_vs_Infantry"), FaustSquad, EGrenade::GerAtGrenade, ETarget::Infantry, InfantryTarget, NotChecked, 1,
	    false);
	Add(TEXT("Stick_vs_Infantry"), JagerSquad, EGrenade::GerStickGrenade, ETarget::Infantry, InfantryTarget, NotChecked,
	    3, false);
	// Anti-infantry grenades do not stick and do nothing against the T-34's rear armor; reported for comparison.
	Add(TEXT("Stick_vs_T34"), JagerSquad, EGrenade::GerStickGrenade, ETarget::TankStill, MediumTank, NotChecked,
	    NoSoldierCheck, false);
	Add(TEXT("Small_vs_Infantry"), LightBringerSquad, EGrenade::SmallGrenade, ETarget::Infantry, InfantryTarget,
	    NotChecked, 3, false);
	Add(TEXT("Molotov_vs_Infantry"), MosinSquad, EGrenade::RusMolotov, ETarget::Infantry, InfantryTarget, NotChecked, 2,
	    false);
	Add(TEXT("Molotov_vs_PzIV"), MosinSquad, EGrenade::RusMolotov, ETarget::TankStill, GermanMediumTank, NotChecked,
	    NoSoldierCheck, false);
	Add(TEXT("Molotov_vs_PzIV_Driving"), MosinSquad, EGrenade::RusMolotov, ETarget::TankMoving, GermanMediumTank,
	    NotChecked, NoSoldierCheck, false);
	Add(TEXT("RGD33_vs_Infantry"), RedHammerSquad, EGrenade::RusRGD33, ETarget::Infantry, InfantryTarget, NotChecked, 3,
	    false);
	Add(TEXT("F1_vs_Infantry"), LargeMixSquad, EGrenade::RusF1, ETarget::Infantry, InfantryTarget, NotChecked, 3, false);
	Add(TEXT("RG42_vs_Infantry"), SVTSquad, EGrenade::RusRG42, ETarget::Infantry, InfantryTarget, NotChecked, 2, false);
	Add(TEXT("RPG43_vs_PzIV"), RedHammerPTRSSquad, EGrenade::RusRPG43, ETarget::TankStill, GermanMediumTank, 40.f,
	    NoSoldierCheck, true);
	Add(TEXT("RPG43_vs_PzIV_Driving"), RedHammerPTRSSquad, EGrenade::RusRPG43, ETarget::TankMoving, GermanMediumTank,
	    40.f, NoSoldierCheck, true);
	// The player's own squads against enemy units: the other collision side of sticking, tracking and the AOE.
	Add(TEXT("PlayerAT_vs_EnemyT34"), FaustSquad, EGrenade::GerAtGrenade, ETarget::TankStill, MediumTank, 40.f,
	    NoSoldierCheck, true);
	M_Scenarios.Last().ThrowerPlayer = PlayerOwner;
	Add(TEXT("PlayerAT_vs_EnemyT34_Driving"), FaustSquad, EGrenade::GerAtGrenade, ETarget::TankMoving, MediumTank, 40.f,
	    NoSoldierCheck, true);
	M_Scenarios.Last().ThrowerPlayer = PlayerOwner;
	Add(TEXT("PlayerStick_vs_EnemyInfantry"), JagerSquad, EGrenade::GerStickGrenade, ETarget::Infantry,
	    EnemyInfantryTarget, NotChecked, 3, false);
	M_Scenarios.Last().ThrowerPlayer = PlayerOwner;
}

void UGrenadeBenchmarkSubsystem::ApplyScenarioFilter()
{
	FString OnlyText;
	if (not FParse::Value(FCommandLine::Get(), GrenadeBenchPrivate::OnlySwitch, OnlyText, false))
	{
		return;
	}
	TArray<FString> NameParts;
	OnlyText.ParseIntoArray(NameParts, TEXT(","), true);
	M_Scenarios.RemoveAll([&NameParts](const FGrenadeBenchScenario& Scenario)
	{
		return not NameParts.ContainsByPredicate([&Scenario](const FString& Part)
		{
			return Scenario.Name.Contains(Part);
		});
	});
}

bool UGrenadeBenchmarkSubsystem::AssignScenarioSpots(const FVector& BaseSpot)
{
	using namespace GrenadeBenchPrivate;
	int32 SpotsFound = 0;
	for (int32 Index = 0; Index < M_Scenarios.Num(); ++Index)
	{
		FGrenadeBenchScenario& Scenario = M_Scenarios[Index];
		// Every scenario on its own spot: wrecks, fires and burns of one never reach the next.
		FVector Spot = BaseSpot + FVector((Index % SpotsPerRow) * SpotSpacing, (Index / SpotsPerRow) * SpotSpacing, 0.f);
		FGrenadeTestHelpers::DropToGround(*GetWorld(), Spot);
		Scenario.bHasSpot = FGrenadeTestHelpers::ProjectOntoNavMesh(*GetWorld(), Spot, SpotNavExtent);
		Scenario.Spot = Spot;
		SpotsFound += Scenario.bHasSpot ? 1 : 0;
	}
	return SpotsFound > 0;
}

void UGrenadeBenchmarkSubsystem::TickWaitingForGameStart()
{
	using namespace GrenadeBenchPrivate;
	if (GetRealSecondsInPhase() > MaxStartSeconds)
	{
		Check(false, TEXT("the game started on a spot with navmesh"));
		Finish();
		return;
	}
	if (GetRealSecondsInPhase() < HudBuildSeconds ||
		not FGrenadeTestHelpers::TryStartGame(*GetWorld(), M_PlayerController))
	{
		return;
	}
	FVector BaseSpot;
	// Streaming levels and the navmesh can arrive a little after the game starts; retry until the start timeout.
	if (not FGrenadeTestHelpers::FindTestSpot(*GetWorld(), LocationSwitch, BaseSpot) ||
		not FGrenadeTestHelpers::ProjectOntoNavMesh(*GetWorld(), BaseSpot, NavSearchExtent) ||
		not AssignScenarioSpots(BaseSpot))
	{
		return;
	}
	ASceneCapture2D* Capture = nullptr;
	UTextureRenderTarget2D* RenderTarget = nullptr;
	if (FGrenadeTestHelpers::CreateCaptureCamera(*GetWorld(), 90.f, Capture, RenderTarget))
	{
		M_Capture = Capture;
		M_RenderTarget = RenderTarget;
	}
	StartNextScenario();
}

void UGrenadeBenchmarkSubsystem::StartNextScenario()
{
	++M_ScenarioIndex;
	while (M_Scenarios.IsValidIndex(M_ScenarioIndex) && not M_Scenarios[M_ScenarioIndex].bHasSpot)
	{
		Check(false, FString::Printf(TEXT("%s has a spot on the navmesh"), *M_Scenarios[M_ScenarioIndex].Name));
		++M_ScenarioIndex;
	}
	if (not M_Scenarios.IsValidIndex(M_ScenarioIndex))
	{
		Finish();
		return;
	}
	M_ScenarioGrenades.Reset();
	M_Victims.Reset();
	M_UnitsReadyGameSeconds = -1.f;
	bM_HasSavedExplosionPicture = false;
	bM_HasSavedFusePicture = false;
	const FGrenadeBenchScenario& Scenario = M_Scenarios[M_ScenarioIndex];
	if (not SpawnScenarioUnits(Scenario))
	{
		Check(false, FString::Printf(TEXT("%s spawned its units"), *Scenario.Name));
		StartNextScenario();
		return;
	}
	if (M_Capture.IsValid())
	{
		FGrenadeTestHelpers::AimLikeGameCamera(*M_Capture, Scenario.Spot);
	}
	EnterPhase(EGrenadeBenchPhase::WaitingForUnits);
}

bool UGrenadeBenchmarkSubsystem::SpawnScenarioUnits(const FGrenadeBenchScenario& Scenario)
{
	using namespace GrenadeBenchPrivate;
	UWorld& World = *GetWorld();
	const bool bIsDriving = Scenario.Target == EGrenadeBenchTarget::TankMoving;
	const FVector TargetLocation = Scenario.Spot + (bIsDriving ? MovingTankStart : FVector::ZeroVector);
	// Tanks face along +Y so the throwers on -X see their side.
	const FRotator TargetRotation(0.f, 90.f, 0.f);
	FString InfantryTargetOverride;
	const bool bOverridesInfantry = Scenario.Target == EGrenadeBenchTarget::Infantry &&
		FParse::Value(FCommandLine::Get(), InfantryTargetSwitch, InfantryTargetOverride);
	const uint8 TargetOwner = Scenario.ThrowerPlayer == PlayerOwner ? EnemyOwner : PlayerOwner;
	M_TargetActor = FGrenadeTestHelpers::SpawnUnitForPlayer(
		World, bOverridesInfantry ? *InfantryTargetOverride : Scenario.TargetClassPath,
		FTransform(TargetRotation, TargetLocation), TargetOwner);
	M_ThrowerSquad = Cast<ASquadController>(FGrenadeTestHelpers::SpawnUnitForPlayer(
		World, Scenario.ThrowerClassPath, FTransform(Scenario.Spot + ThrowerOffset), Scenario.ThrowerPlayer));
	UE_LOG(LogRTSGrenadeBench, Display, TEXT("RTS_GRENADE_BENCH scenario %s: thrower=%s target=%s at %s"),
	       *Scenario.Name, M_ThrowerSquad.IsValid() ? *M_ThrowerSquad->GetName() : TEXT("none"),
	       M_TargetActor.IsValid() ? *M_TargetActor->GetName() : TEXT("none"), *Scenario.Spot.ToCompactString());
	return M_ThrowerSquad.IsValid() && M_TargetActor.IsValid();
}

bool UGrenadeBenchmarkSubsystem::GetAreScenarioUnitsReady() const
{
	using namespace GrenadeBenchPrivate;
	if (not M_ThrowerSquad.IsValid() || not M_TargetActor.IsValid())
	{
		return false;
	}
	const FGrenadeBenchScenario& Scenario = M_Scenarios[M_ScenarioIndex];
	const bool bIsThrowerReady = M_ThrowerSquad->GetIsSquadFullyLoadedAndInitialized() &&
		FGrenadeTestHelpers::GetLivingSoldierCount(*M_ThrowerSquad) >= MinLivingSoldiers &&
		GetHasGrenadeOnCard(Scenario.Grenade);
	if (Scenario.Target != EGrenadeBenchTarget::Infantry)
	{
		return bIsThrowerReady && GetActorHealth(M_TargetActor.Get()) > 0.f;
	}
	ASquadController* TargetSquad = Cast<ASquadController>(M_TargetActor.Get());
	return bIsThrowerReady && IsValid(TargetSquad) && TargetSquad->GetIsSquadFullyLoadedAndInitialized() &&
		FGrenadeTestHelpers::GetLivingSoldierCount(*TargetSquad) >= MinLivingSoldiers;
}

bool UGrenadeBenchmarkSubsystem::GetHasGrenadeOnCard(const EGrenadeAbilityType Grenade) const
{
	if (not M_ThrowerSquad.IsValid())
	{
		return false;
	}
	for (const FUnitAbilityEntry& AbilityEntry : M_ThrowerSquad->GetUnitAbilityEntries())
	{
		if (AbilityEntry.AbilityId == EAbilityID::IdThrowGrenade && AbilityEntry.CustomType == static_cast<int32>(Grenade))
		{
			return true;
		}
	}
	return false;
}

UGrenadeComponent* UGrenadeBenchmarkSubsystem::GetThrowerGrenadeComponent() const
{
	return M_ThrowerSquad.IsValid() ? M_ThrowerSquad->FindComponentByClass<UGrenadeComponent>() : nullptr;
}

const FGrenadeComponentSettings* UGrenadeBenchmarkSubsystem::GetSettings(const UGrenadeComponent& GrenadeComponent)
{
	// M_Settings is private on purpose; the benchmark reads and tunes it through reflection.
	const FStructProperty* SettingsProperty = FindFProperty<FStructProperty>(UGrenadeComponent::StaticClass(),
	                                                                         TEXT("M_Settings"));
	return SettingsProperty != nullptr
		       ? SettingsProperty->ContainerPtrToValuePtr<FGrenadeComponentSettings>(&GrenadeComponent)
		       : nullptr;
}

void UGrenadeBenchmarkSubsystem::ApplySettingOverrides(UGrenadeComponent& GrenadeComponent,
                                                       const EGrenadeAbilityType Grenade) const
{
	FString OverrideText;
	if (not FParse::Value(FCommandLine::Get(), GrenadeBenchPrivate::OverrideSwitch, OverrideText, false))
	{
		return;
	}
	FGrenadeComponentSettings* Settings = const_cast<FGrenadeComponentSettings*>(GetSettings(GrenadeComponent));
	if (Settings == nullptr)
	{
		return;
	}
	const FString GrenadeName = GrenadeBenchPrivate::GetGrenadeName(Grenade);
	TArray<FString> GrenadeBlocks;
	OverrideText.ParseIntoArray(GrenadeBlocks, TEXT(";"), true);
	for (const FString& Block : GrenadeBlocks)
	{
		FString BlockGrenade, Assignments;
		if (not Block.Split(TEXT(":"), &BlockGrenade, &Assignments) || BlockGrenade != GrenadeName)
		{
			continue;
		}
		TArray<FString> Pairs;
		Assignments.ParseIntoArray(Pairs, TEXT(","), true);
		for (const FString& Pair : Pairs)
		{
			FString Name, Value;
			const FProperty* Property = Pair.Split(TEXT("="), &Name, &Value)
				                            ? FGrenadeComponentSettings::StaticStruct()->FindPropertyByName(FName(*Name))
				                            : nullptr;
			const bool bApplied = Property != nullptr &&
				Property->ImportText_Direct(*Value, Property->ContainerPtrToValuePtr<void>(Settings), nullptr, PPF_None) !=
				nullptr;
			UE_LOG(LogRTSGrenadeBench, Display, TEXT("RTS_GRENADE_BENCH override %s %s=%s %s"), *GrenadeName, *Name,
			       *Value, bApplied ? TEXT("applied") : TEXT("UNKNOWN SETTING"));
		}
	}
}

void UGrenadeBenchmarkSubsystem::TickWaitingForUnits()
{
	using namespace GrenadeBenchPrivate;
	const FGrenadeBenchScenario& Scenario = M_Scenarios[M_ScenarioIndex];
	if (not GetAreScenarioUnitsReady())
	{
		if (GetGameSecondsInPhase() > MaxUnitLoadSeconds)
		{
			Check(false, FString::Printf(TEXT("%s: thrower and target loaded, grenade on the card"), *Scenario.Name));
			StartNextScenario();
		}
		return;
	}
	if (M_UnitsReadyGameSeconds < 0.f)
	{
		M_UnitsReadyGameSeconds = GetGameSeconds();
	}
	if (GetGameSeconds() - M_UnitsReadyGameSeconds < UnitSettleSeconds)
	{
		return;
	}
	if (UGrenadeComponent* GrenadeComponent = GetThrowerGrenadeComponent())
	{
		ApplySettingOverrides(*GrenadeComponent, Scenario.Grenade);
	}
	SnapshotVictims();
	if (Scenario.Target == EGrenadeBenchTarget::TankMoving)
	{
		ICommands* TankCommands = Cast<ICommands>(M_TargetActor.Get());
		const ECommandQueueError MoveResult = TankCommands != nullptr
			                                      ? TankCommands->MoveToLocation(
				                                      Scenario.Spot + MovingTankEnd, true, FRotator(0.f, 90.f, 0.f))
			                                      : ECommandQueueError::AbilityNotAllowed;
		Check(MoveResult == ECommandQueueError::NoError, FString::Printf(TEXT("%s: the tank drives off"), *Scenario.Name));
		EnterPhase(EGrenadeBenchPhase::WaitingForTankApproach);
		return;
	}
	OrderThrow();
}

void UGrenadeBenchmarkSubsystem::TickWaitingForTankApproach()
{
	using namespace GrenadeBenchPrivate;
	const FGrenadeBenchScenario& Scenario = M_Scenarios[M_ScenarioIndex];
	const UGrenadeComponent* GrenadeComponent = GetThrowerGrenadeComponent();
	const float ThrowRange = IsValid(GrenadeComponent) ? GrenadeComponent->GetThrowRange() : 0.f;
	TArray<FVector> ThrowerLocations;
	const TArray<ASquadUnit*> Throwers = M_ThrowerSquad.IsValid() ? M_ThrowerSquad->GetSquadUnitsChecked() : TArray<ASquadUnit*>();
	for (ASquadUnit* Soldier : Throwers)
	{
		if (FGrenadeTestHelpers::GetIsLivingSoldier(Soldier))
		{
			ThrowerLocations.Add(Soldier->GetActorLocation());
		}
	}
	// Measured to the soldiers: the squad controller does not stand where its soldiers are.
	const bool bIsInThrowWindow = M_TargetActor.IsValid() && not ThrowerLocations.IsEmpty() &&
		FVector::Dist2D(M_TargetActor->GetActorLocation(), GetMedianLocation(ThrowerLocations)) <=
		ThrowRange * MovingTankThrowRangeFraction;
	if (bIsInThrowWindow)
	{
		UE_LOG(LogRTSGrenadeBench, Display, TEXT("RTS_GRENADE_BENCH %s: tank speed %.0f cm/s at the order"),
		       *Scenario.Name, M_TargetActor->GetVelocity().Size2D());
		OrderThrow();
		return;
	}
	if (GetGameSecondsInPhase() > MaxTankApproachSeconds)
	{
		Check(false, FString::Printf(TEXT("%s: the tank drove into the throw window"), *Scenario.Name));
		StartNextScenario();
	}
}

FVector UGrenadeBenchmarkSubsystem::GetTargetAimPoint() const
{
	const FGrenadeBenchScenario& Scenario = M_Scenarios[M_ScenarioIndex];
	if (not M_TargetActor.IsValid())
	{
		return Scenario.Spot;
	}
	if (Scenario.Target == EGrenadeBenchTarget::Infantry)
	{
		ASquadController* TargetSquad = Cast<ASquadController>(M_TargetActor.Get());
		const TArray<ASquadUnit*> Soldiers = IsValid(TargetSquad) ? TargetSquad->GetSquadUnitsChecked() : TArray<ASquadUnit*>();
		TArray<FVector> SoldierLocations;
		for (ASquadUnit* Soldier : Soldiers)
		{
			if (FGrenadeTestHelpers::GetIsLivingSoldier(Soldier))
			{
				SoldierLocations.Add(Soldier->GetActorLocation());
			}
		}
		return SoldierLocations.IsEmpty()
			       ? M_TargetActor->GetActorLocation()
			       : GrenadeBenchPrivate::GetMedianLocation(SoldierLocations);
	}
	FVector Origin, Extent;
	M_TargetActor->GetActorBounds(true, Origin, Extent);
	return Origin + FVector(0.f, 0.f, Extent.Z * GrenadeBenchPrivate::TankAimHeightFraction);
}

void UGrenadeBenchmarkSubsystem::SnapshotVictims()
{
	M_Victims.Reset();
	const auto AddVictim = [this](AActor* Actor)
	{
		const UHealthComponent* Health = IsValid(Actor) ? Actor->FindComponentByClass<UHealthComponent>() : nullptr;
		if (not IsValid(Health))
		{
			return;
		}
		// The AOE finds victims by collision object type; log what each victim's colliders are.
		TArray<UPrimitiveComponent*> Primitives;
		Actor->GetComponents(Primitives);
		FString Colliders;
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			if (IsValid(Primitive) && Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				Colliders += FString::Printf(TEXT(" %s:%s"), *Primitive->GetName(),
				                             *UEnum::GetValueAsString(Primitive->GetCollisionObjectType()));
			}
		}
		UE_LOG(LogRTSGrenadeBench, Display, TEXT("RTS_GRENADE_BENCH victim %s colliders:%s"), *Actor->GetName(), *Colliders);
		FGrenadeBenchVictim& Victim = M_Victims.AddDefaulted_GetRef();
		Victim.Actor = Actor;
		Victim.Name = Actor->GetName();
		Victim.HealthBefore = Health->GetCurrentHealth();
		Victim.LastKnownHealth = Victim.HealthBefore;
		Victim.MaxHealth = Health->GetMaxHealth();
	};
	ASquadController* TargetSquad = Cast<ASquadController>(M_TargetActor.Get());
	if (not IsValid(TargetSquad))
	{
		AddVictim(M_TargetActor.Get());
		return;
	}
	const FVector AimPoint = GetTargetAimPoint();
	for (ASquadUnit* Soldier : TargetSquad->GetSquadUnitsChecked())
	{
		if (not FGrenadeTestHelpers::GetIsLivingSoldier(Soldier))
		{
			continue;
		}
		const float DistanceToAimPoint = FVector::Dist2D(Soldier->GetActorLocation(), AimPoint);
		if (DistanceToAimPoint > GrenadeBenchPrivate::VictimMaxDistanceFromAimPoint)
		{
			UE_LOG(LogRTSGrenadeBench, Display, TEXT("RTS_GRENADE_BENCH %s left out: %.0f cm from his squad"),
			       *Soldier->GetName(), DistanceToAimPoint);
			continue;
		}
		AddVictim(Soldier);
	}
}

void UGrenadeBenchmarkSubsystem::OrderThrow()
{
	const FGrenadeBenchScenario& Scenario = M_Scenarios[M_ScenarioIndex];
	const UGrenadeComponent* GrenadeComponent = GetThrowerGrenadeComponent();
	const FGrenadeComponentSettings* Settings = IsValid(GrenadeComponent) ? GetSettings(*GrenadeComponent) : nullptr;
	const int32 Throwers = Settings != nullptr
		                       ? FMath::Min(Settings->SquadUnitsThrowing,
		                                    FGrenadeTestHelpers::GetLivingSoldierCount(*M_ThrowerSquad))
		                       : 0;
	M_ExpectedGrenades = Settings != nullptr ? Throwers * Settings->GrenadesPerSquad : 0;
	const FVector AimPoint = GetTargetAimPoint();
	const ECommandQueueError OrderResult = M_ThrowerSquad->ThrowGrenade(AimPoint, true, Scenario.Grenade);
	Check(OrderResult == ECommandQueueError::NoError && M_ExpectedGrenades > 0,
	      FString::Printf(TEXT("%s: throw order accepted (expecting %d grenades)"), *Scenario.Name, M_ExpectedGrenades));
	EnterPhase(EGrenadeBenchPhase::WaitingForExplosions);
}

void UGrenadeBenchmarkSubsystem::BindNewGrenades()
{
	for (TActorIterator<AGrenadeActor> It(GetWorld()); It; ++It)
	{
		AGrenadeActor* Grenade = *It;
		if (M_BoundGrenades.Contains(Grenade))
		{
			continue;
		}
		M_BoundGrenades.Add(Grenade);
		Grenade->OnGrenadeLanded.AddUObject(this, &UGrenadeBenchmarkSubsystem::OnGrenadeLanded);
		Grenade->OnGrenadeExploded.AddUObject(this, &UGrenadeBenchmarkSubsystem::OnGrenadeExploded);
	}
}

FGrenadeBenchGrenade* UGrenadeBenchmarkSubsystem::FindScenarioGrenade(const AGrenadeActor* Grenade)
{
	return M_ScenarioGrenades.FindByPredicate([Grenade](const FGrenadeBenchGrenade& Entry)
	{
		return Entry.Grenade.Get() == Grenade;
	});
}

void UGrenadeBenchmarkSubsystem::OnGrenadeLanded(AGrenadeActor* Grenade, AActor* StuckToVehicle)
{
	const bool bIsScenarioRunning = M_Phase == EGrenadeBenchPhase::WaitingForExplosions && M_Scenarios.IsValidIndex(
		M_ScenarioIndex);
	if (not bIsScenarioRunning || not IsValid(Grenade) ||
		FVector::Dist2D(Grenade->GetActorLocation(), M_Scenarios[M_ScenarioIndex].Spot) >
		GrenadeBenchPrivate::ScenarioRadius)
	{
		return;
	}
	FGrenadeBenchGrenade& Entry = M_ScenarioGrenades.AddDefaulted_GetRef();
	Entry.Grenade = Grenade;
	Entry.LandedGameSeconds = GetGameSeconds();
	Entry.StuckTo = IsValid(StuckToVehicle) ? StuckToVehicle->GetName() : FString();
}

void UGrenadeBenchmarkSubsystem::OnGrenadeExploded(AGrenadeActor* Grenade, const FVector& Location)
{
	FGrenadeBenchGrenade* Entry = FindScenarioGrenade(Grenade);
	if (Entry == nullptr || Entry->ExplodedGameSeconds >= 0.f)
	{
		return;
	}
	Entry->ExplodedGameSeconds = GetGameSeconds();
	Entry->ExplosionLocation = Location;
}

int32 UGrenadeBenchmarkSubsystem::GetExplodedGrenadeCount() const
{
	int32 Exploded = 0;
	for (const FGrenadeBenchGrenade& Entry : M_ScenarioGrenades)
	{
		Exploded += Entry.ExplodedGameSeconds >= 0.f ? 1 : 0;
	}
	return Exploded;
}

void UGrenadeBenchmarkSubsystem::WatchFuseWarnings()
{
	const URTSRadiusPoolSubsystem* RadiusPool = GetWorld()->GetSubsystem<URTSRadiusPoolSubsystem>();
	const UGrenadeComponent* GrenadeComponent = GetThrowerGrenadeComponent();
	const FGrenadeComponentSettings* Settings = IsValid(GrenadeComponent) ? GetSettings(*GrenadeComponent) : nullptr;
	for (FGrenadeBenchGrenade& Entry : M_ScenarioGrenades)
	{
		const AGrenadeActor* Grenade = Entry.Grenade.Get();
		if (not IsValid(Grenade) || Grenade->GetGrenadeState() != EGrenadeActorState::Fusing)
		{
			continue;
		}
		const int32 WarningId = Grenade->GetFuseWarningRadiusId();
		Entry.bShowedFuseWarning |= WarningId != INDEX_NONE && IsValid(RadiusPool) &&
			RadiusPool->GetIsRTSRadiusIdActive(WarningId);
		const float HalfFuse = Settings != nullptr ? Settings->FuseSecondsAfterLanding * 0.5f : 0.f;
		if (not bM_HasSavedFusePicture && GetGameSeconds() - Entry.LandedGameSeconds >= HalfFuse)
		{
			bM_HasSavedFusePicture = true;
			SavePicture(TEXT("fuse"));
		}
	}
}

void UGrenadeBenchmarkSubsystem::TickWaitingForExplosions()
{
	using namespace GrenadeBenchPrivate;
	WatchFuseWarnings();
	const int32 Exploded = GetExplodedGrenadeCount();
	float FirstExplosionSeconds = -1.f;
	for (const FGrenadeBenchGrenade& Entry : M_ScenarioGrenades)
	{
		if (Entry.ExplodedGameSeconds >= 0.f &&
			(FirstExplosionSeconds < 0.f || Entry.ExplodedGameSeconds < FirstExplosionSeconds))
		{
			FirstExplosionSeconds = Entry.ExplodedGameSeconds;
		}
	}
	// The capture renders a frame behind; a moment after the blast shows the explosion itself.
	if (not bM_HasSavedExplosionPicture && FirstExplosionSeconds >= 0.f &&
		GetGameSeconds() - FirstExplosionSeconds >= ExplosionPictureDelay)
	{
		bM_HasSavedExplosionPicture = true;
		SavePicture(TEXT("explosion"));
	}
	if (Exploded >= M_ExpectedGrenades || GetGameSecondsInPhase() > MaxExplosionWaitSeconds)
	{
		EnterPhase(EGrenadeBenchPhase::WaitingForDamage);
	}
}

void UGrenadeBenchmarkSubsystem::TickWaitingForDamage()
{
	using namespace GrenadeBenchPrivate;
	const UGrenadeComponent* GrenadeComponent = GetThrowerGrenadeComponent();
	const FGrenadeComponentSettings* Settings = IsValid(GrenadeComponent) ? GetSettings(*GrenadeComponent) : nullptr;
	const float BurnSeconds = Settings != nullptr && Settings->BurnArea.GetIsBurnAreaEnabled()
		                          ? Settings->BurnArea.BurnDuration + BurnSettleMargin
		                          : 0.f;
	if (GetGameSecondsInPhase() < DamageSettleSeconds + BurnSeconds)
	{
		return;
	}
	SavePicture(TEXT("end"));
	ReportScenario();
	StartNextScenario();
}

FString UGrenadeBenchmarkSubsystem::DescribeGrenades(const FGrenadeComponentSettings& Settings, int32& OutStuckCount,
                                                     float& OutAverageFuse) const
{
	OutStuckCount = 0;
	OutAverageFuse = 0.f;
	int32 FusedGrenades = 0;
	FString Details;
	for (const FGrenadeBenchGrenade& Entry : M_ScenarioGrenades)
	{
		OutStuckCount += Entry.StuckTo.IsEmpty() ? 0 : 1;
		if (Entry.ExplodedGameSeconds >= 0.f)
		{
			OutAverageFuse += Entry.ExplodedGameSeconds - Entry.LandedGameSeconds;
			++FusedGrenades;
		}
		Details += FString::Printf(TEXT(" [fuse=%.2f stuck=%s]"), Entry.ExplodedGameSeconds - Entry.LandedGameSeconds,
		                           Entry.StuckTo.IsEmpty() ? TEXT("-") : *Entry.StuckTo);
	}
	OutAverageFuse = FusedGrenades > 0 ? OutAverageFuse / FusedGrenades : -1.f;
	return Details;
}

float UGrenadeBenchmarkSubsystem::GetClosestExplosionDistance(const AActor& Actor, EAoeDistanceSource& OutSource) const
{
	float ClosestDistance = -1.f;
	OutSource = EAoeDistanceSource::ActorOrigin;
	TArray<UPrimitiveComponent*> Primitives;
	Actor.GetComponents(Primitives);
	for (const FGrenadeBenchGrenade& Entry : M_ScenarioGrenades)
	{
		if (Entry.ExplodedGameSeconds < 0.f)
		{
			continue;
		}
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			if (not IsValid(Primitive) || Primitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
			{
				continue;
			}
			EAoeDistanceSource Source = EAoeDistanceSource::ActorOrigin;
			const float Distance = FRTS_AOE::GetFalloffDistance(Actor, Primitive, Entry.ExplosionLocation, Source);
			if (ClosestDistance < 0.f || Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				OutSource = Source;
			}
		}
	}
	return ClosestDistance;
}

void UGrenadeBenchmarkSubsystem::SilenceTarget() const
{
	// The targets belong to the player and see the enemy-owned thrower squad: a tank would shell it, riflemen
	// would shoot back, and the thrower squad's rifles follow its owner and would shoot the target.
	// Only the grenades may deal damage in a scenario.
	SilenceSquad(M_ThrowerSquad.Get(), false);
	if (const ATankMaster* Tank = Cast<ATankMaster>(M_TargetActor.Get()))
	{
		for (ACPPTurretsMaster* Turret : Tank->GetTurrets())
		{
			if (IsValid(Turret))
			{
				Turret->DisableTurret();
			}
		}
		return;
	}
	SilenceSquad(Cast<ASquadController>(M_TargetActor.Get()), true);
}

void UGrenadeBenchmarkSubsystem::SilenceSquad(ASquadController* Squad, const bool bHoldInPlace)
{
	if (not IsValid(Squad))
	{
		return;
	}
	for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
	{
		if (not FGrenadeTestHelpers::GetIsLivingSoldier(Soldier))
		{
			continue;
		}
		if (Soldier->GetIsValidWeapon())
		{
			Soldier->GetInfantryWeapon()->DisableWeaponSearch(false, false);
		}
		// Soldiers walking into cover would leave the spot the grenades were aimed at.
		if (bHoldInPlace && IsValid(Soldier->GetCharacterMovement()))
		{
			Soldier->GetCharacterMovement()->StopMovementImmediately();
		}
	}
}

void UGrenadeBenchmarkSubsystem::TrackVictimHealth()
{
	for (FGrenadeBenchVictim& Victim : M_Victims)
	{
		if (Victim.Actor.IsValid())
		{
			Victim.LastKnownHealth = GetActorHealth(Victim.Actor.Get());
		}
	}
}

float UGrenadeBenchmarkSubsystem::GetVictimHealthAfter(const FGrenadeBenchVictim& Victim)
{
	// A soldier killed by the blast is removed after his death; his last health tells killed from vanished.
	if (Victim.Actor.IsValid())
	{
		return GetActorHealth(Victim.Actor.Get());
	}
	return Victim.LastKnownHealth < Victim.HealthBefore ? 0.f : Victim.LastKnownHealth;
}

float UGrenadeBenchmarkSubsystem::GetActorHealth(const AActor* Actor)
{
	const UHealthComponent* Health = IsValid(Actor) ? Actor->FindComponentByClass<UHealthComponent>() : nullptr;
	return IsValid(Health) ? Health->GetCurrentHealth() : 0.f;
}

void UGrenadeBenchmarkSubsystem::ReportScenario()
{
	using namespace GrenadeBenchPrivate;
	const FGrenadeBenchScenario& Scenario = M_Scenarios[M_ScenarioIndex];
	const UGrenadeComponent* GrenadeComponent = GetThrowerGrenadeComponent();
	const FGrenadeComponentSettings* Settings = IsValid(GrenadeComponent) ? GetSettings(*GrenadeComponent) : nullptr;
	if (Settings == nullptr)
	{
		Check(false, FString::Printf(TEXT("%s: thrower still has its grenade component"), *Scenario.Name));
		return;
	}
	int32 StuckCount = 0;
	float AverageFuse = 0.f;
	const FString GrenadeDetails = DescribeGrenades(*Settings, StuckCount, AverageFuse);
	const int32 Exploded = GetExplodedGrenadeCount();
	const FString GrenadeColumns = FString::Printf(TEXT("%s,%s,%s,%.0f,%.0f,%.2f,%.2f,%d,%d,%d,%d,%.2f"),
	                                               *Scenario.Name, *GetGrenadeName(Scenario.Grenade),
	                                               *FPaths::GetBaseFilename(Scenario.TargetClassPath),
	                                               Settings->AoeRange, Settings->Damage,
	                                               Settings->AoeDamageExponentReduction,
	                                               Settings->FuseSecondsAfterLanding,
	                                               Settings->bStickToVehicles ? 1 : 0, M_ExpectedGrenades, Exploded,
	                                               StuckCount, AverageFuse);
	UE_LOG(LogRTSGrenadeBench, Display, TEXT("RTS_GRENADE_BENCH %s grenades:%s"), *Scenario.Name, *GrenadeDetails);
	Check(Exploded == M_ExpectedGrenades,
	      FString::Printf(TEXT("%s: %d of %d grenades exploded"), *Scenario.Name, Exploded, M_ExpectedGrenades));
	Check(Settings->AoeRange >= MinimumWorkingAoeRadius,
	      FString::Printf(TEXT("%s: AOE radius %.0f is above the sweep's floor"), *Scenario.Name, Settings->AoeRange));
	const bool bFuseMatches = Exploded == 0 || FMath::Abs(AverageFuse - Settings->FuseSecondsAfterLanding) <= 0.15f ||
		Settings->FuseSecondsAfterLanding <= 0.f;
	Check(bFuseMatches, FString::Printf(TEXT("%s: fuse %.2f s after landing (setting %.2f)"), *Scenario.Name,
	                                    AverageFuse, Settings->FuseSecondsAfterLanding));
	if (Settings->FuseSecondsAfterLanding > 0.f)
	{
		const int32 WarnedGrenades = M_ScenarioGrenades.FilterByPredicate([](const FGrenadeBenchGrenade& Entry)
		{
			return Entry.bShowedFuseWarning;
		}).Num();
		Check(WarnedGrenades == Exploded, FString::Printf(TEXT("%s: %d of %d fused grenades showed the blast warning"),
		                                                  *Scenario.Name, WarnedGrenades, Exploded));
	}
	if (Scenario.Target == EGrenadeBenchTarget::Infantry)
	{
		ReportInfantry(Scenario, *Settings, GrenadeColumns);
		return;
	}
	ReportTank(Scenario, *Settings, GrenadeColumns);
}

void UGrenadeBenchmarkSubsystem::ReportInfantry(const FGrenadeBenchScenario& Scenario,
                                                const FGrenadeComponentSettings& Settings,
                                                const FString& GrenadeColumns)
{
	int32 InRadius = 0;
	int32 Damaged = 0;
	int32 Killed = 0;
	float TotalDamage = 0.f;
	float ClosestDistance = -1.f;
	FString PerSoldier;
	EAoeDistanceSource ClosestSource = EAoeDistanceSource::ActorOrigin;
	int32 Vanished = 0;
	for (const FGrenadeBenchVictim& Victim : M_Victims)
	{
		const float HealthAfter = GetVictimHealthAfter(Victim);
		Vanished += not Victim.Actor.IsValid() && HealthAfter > 0.f ? 1 : 0;
		const float Damage = FMath::Max(0.f, Victim.HealthBefore - HealthAfter);
		EAoeDistanceSource Source = EAoeDistanceSource::ActorOrigin;
		const float Distance = Victim.Actor.IsValid() ? GetClosestExplosionDistance(*Victim.Actor, Source) : -1.f;
		InRadius += Distance >= 0.f && Distance <= Settings.AoeRange ? 1 : 0;
		Damaged += Damage > 0.f ? 1 : 0;
		Killed += HealthAfter <= 0.f ? 1 : 0;
		TotalDamage += Damage;
		if (Distance >= 0.f && (ClosestDistance < 0.f || Distance < ClosestDistance))
		{
			ClosestDistance = Distance;
			ClosestSource = Source;
		}
		PerSoldier += FString::Printf(TEXT(" %.0fcm:%.0f->%.0f"), Distance, Victim.HealthBefore, HealthAfter);
	}
	const FString Line = FString::Printf(
		TEXT("%s soldiers=%d in_radius=%d damaged=%d killed=%d vanished=%d damage_total=%.0f (distance:health%s)"),
		*Scenario.Name, M_Victims.Num(), InRadius, Damaged, Killed, Vanished, TotalDamage, *PerSoldier);
	Check(Vanished == 0, FString::Printf(TEXT("%s: no target soldier vanished without being hit"), *Scenario.Name));
	if (Scenario.MinSoldiersHurt >= 0)
	{
		Check(Damaged >= Scenario.MinSoldiersHurt, FString::Printf(TEXT("%s: %d of %d soldiers hurt (expected %d)"),
		                                                           *Scenario.Name, Damaged, M_Victims.Num(),
		                                                           Scenario.MinSoldiersHurt));
	}
	UE_LOG(LogRTSGrenadeBench, Display, TEXT("RTS_GRENADE_BENCH %s"), *Line);
	M_SummaryLines.Add(FString::Printf(TEXT("%-22s r=%3.0f dmg=%3.0f exp=%.2f fuse=%.1f: %d soldiers, %d in radius, %d hurt, %d killed, %.0f damage"),
	                                   *Scenario.Name, Settings.AoeRange, Settings.Damage,
	                                   Settings.AoeDamageExponentReduction, Settings.FuseSecondsAfterLanding,
	                                   M_Victims.Num(), InRadius, Damaged, Killed, TotalDamage));
	M_CsvText += FString::Printf(TEXT("%s,,,,,,,,%d,%d,%d,%d,%.0f,%s\n"), *GrenadeColumns, M_Victims.Num(), InRadius,
	                             Damaged, Killed, ClosestDistance, *GrenadeBenchPrivate::GetDistanceSourceName(ClosestSource));
}

void UGrenadeBenchmarkSubsystem::ReportTank(const FGrenadeBenchScenario& Scenario,
                                            const FGrenadeComponentSettings& Settings,
                                            const FString& GrenadeColumns)
{
	if (M_Victims.IsEmpty())
	{
		Check(false, FString::Printf(TEXT("%s: the tank had health at the order"), *Scenario.Name));
		return;
	}
	const FGrenadeBenchVictim& Victim = M_Victims[0];
	const float HealthAfter = GetVictimHealthAfter(Victim);
	const bool bWasKilled = HealthAfter <= 0.f;
	const float DealtPercent = Victim.MaxHealth > 0.f
		                           ? 100.f * FMath::Max(0.f, Victim.HealthBefore - HealthAfter) / Victim.MaxHealth
		                           : 0.f;
	const UArmorCalculation* Armor = Victim.Actor.IsValid()
		                                 ? Victim.Actor->FindComponentByClass<UArmorCalculation>()
		                                 : nullptr;
	const float RearArmor = IsValid(Armor) ? Armor->GetRearArmor() : -1.f;
	const float ArmorMultiplier = RearArmor >= 0.f
		                              ? FRTS_AOE::GetRearArmorDamageMultiplier(
			                              RearArmor, Settings.FullArmorPen, Settings.ArmorPenFallOff, Settings.MaxArmorPen)
		                              : -1.f;
	EAoeDistanceSource Source = EAoeDistanceSource::ActorOrigin;
	const float HullDistance = Victim.Actor.IsValid() ? GetClosestExplosionDistance(*Victim.Actor, Source) : -1.f;
	const FString Line = FString::Printf(
		TEXT("%s tank=%s rear_armor=%.0f armor_multiplier=%.2f health %.0f -> %.0f of %.0f dealt=%.0f%% killed=%d hull_distance=%.0f (%s)"),
		*Scenario.Name, *Victim.Name, RearArmor, ArmorMultiplier, Victim.HealthBefore, HealthAfter, Victim.MaxHealth,
		DealtPercent, bWasKilled ? 1 : 0, HullDistance, *GrenadeBenchPrivate::GetDistanceSourceName(Source));
	UE_LOG(LogRTSGrenadeBench, Display, TEXT("RTS_GRENADE_BENCH %s"), *Line);
	int32 StuckCount = 0;
	float AverageFuse = 0.f;
	DescribeGrenades(Settings, StuckCount, AverageFuse);
	if (Scenario.bExpectAllStuck)
	{
		Check(StuckCount == M_ExpectedGrenades, FString::Printf(TEXT("%s: %d of %d grenades stuck to the tank"),
		                                                        *Scenario.Name, StuckCount, M_ExpectedGrenades));
	}
	if (Scenario.MinDealtPercent >= 0.f)
	{
		Check(bWasKilled || DealtPercent >= Scenario.MinDealtPercent,
		      FString::Printf(TEXT("%s: dealt %.0f%% of the tank's health (expected %.0f%%)"), *Scenario.Name,
		                      DealtPercent, Scenario.MinDealtPercent));
	}
	M_SummaryLines.Add(FString::Printf(TEXT("%-22s r=%3.0f dmg=%3.0f exp=%.2f fuse=%.1f: rear %3.0f x%.2f, %d/%d stuck, dealt %3.0f%%%s"),
	                                   *Scenario.Name, Settings.AoeRange, Settings.Damage,
	                                   Settings.AoeDamageExponentReduction, Settings.FuseSecondsAfterLanding, RearArmor,
	                                   ArmorMultiplier, StuckCount, M_ExpectedGrenades, DealtPercent,
	                                   bWasKilled ? TEXT(" KILLED") : TEXT("")));
	M_CsvText += FString::Printf(TEXT("%s,%.0f,%.2f,%.0f,%.0f,%.1f,%d,,,,,%.0f,%s\n"), *GrenadeColumns, RearArmor,
	                             ArmorMultiplier, Victim.HealthBefore, HealthAfter, DealtPercent, bWasKilled ? 1 : 0,
	                             HullDistance, *GrenadeBenchPrivate::GetDistanceSourceName(Source));
}

void UGrenadeBenchmarkSubsystem::SavePicture(const FString& Label) const
{
	if (not IsValid(M_RenderTarget) || not M_Scenarios.IsValidIndex(M_ScenarioIndex))
	{
		return;
	}
	FGrenadeTestHelpers::SaveCapture(*GetWorld(), *M_RenderTarget, M_OutputDirectory,
	                                 FString::Printf(TEXT("%s_%s.png"), *M_Scenarios[M_ScenarioIndex].Name, *Label));
}

#else

bool UGrenadeBenchmarkSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UGrenadeBenchmarkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UGrenadeBenchmarkSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UGrenadeBenchmarkSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UGrenadeBenchmarkSubsystem::Tick(float DeltaTime)
{
}

TStatId UGrenadeBenchmarkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGrenadeBenchmarkSubsystem, STATGROUP_Tickables);
}

#endif
