// Copyright (C) Bas Blokzijl - All rights reserved.

// -AttackMoveValidate: walks a tank, a squad and two team weapons, one at a time, down a lane with two enemy tanks on it.
// Each subject must stop for the first enemy, fight, carry on when it dies, do the same for the second and arrive.
// Logs RTS_ATTACKMOVE_TEST lines and quits. Everything lives in this file, behind RTS_WITH_MAP_TESTS: the regular
// Shipping target compiles none of it and game code holds no hook for it.

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/Parse.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpPawnMaster.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSOptimizer.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Weapons/HullWeaponComponent/HullWeaponComponent.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAttackMoveTest, Display, All);

namespace AttackMoveMapTestPrivate
{
	constexpr uint8 PlayerOwner = 1;
	constexpr uint8 EnemyOwner = 2;
	constexpr int32 EnemyCount = 2;

	constexpr float SubjectReadySeconds = 3.f;
	constexpr float FightSeconds = 6.f;
	constexpr float StepTimeoutSeconds = 150.f;
	constexpr float WalkingTimeoutSeconds = 420.f;
	constexpr float ArrivalToleranceCm = 2500.f;
	constexpr float StoppedSpeedCmPerSecond = 120.f;
	constexpr float KeepAliveHealthPerTick = 100000.f;
	// Units engage at sight, which is shorter than most weapon ranges, so the same lane suits every subject.
	constexpr float FirstEnemyDistanceCm = 4500.f;
	constexpr float SecondEnemyDistanceCm = 7500.f;
	// The flat test map ends about 8200 cm past the default lane start.
	constexpr float DestinationDistanceCm = 7900.f;
	constexpr float EnemySideOffsetCm = 1200.f;
	constexpr float TraceIntervalSeconds = 10.f;

	const TCHAR* const LocationSwitch = TEXT("AttackMoveAt=");
	const TCHAR* const EnemyTankClass =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");

	struct FSubjectDefinition
	{
		const TCHAR* Name;
		const TCHAR* ClassPath;
	};

	const FSubjectDefinition SubjectDefinitions[] = {
		{TEXT("Tank"), TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H.Ger_Vehicle_PanzerIV_H_C")},
		{TEXT("Squad"), TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/SteelFistAssaultSquad/Squad_Ger_Faust.Squad_Ger_Faust_C")},
		{TEXT("Pak38"), TEXT("/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Pak38_50mm/Ger_SquadTW_Pak38.Ger_SquadTW_Pak38_C")},
		{TEXT("Lefh"), TEXT("/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/LefhHowitzer/Ger_SquadTW_Lefh.Ger_SquadTW_Lefh_C")},
		{TEXT("Flak37"), TEXT("/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Flak37mm/Ger_SquadTW_Flak37MM.Ger_SquadTW_Flak37MM_C")},
		{TEXT("Flak88"), TEXT("/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Flak88Wheels/Ger_SquadTW_Flak88.Ger_SquadTW_Flak88_C")},
		{TEXT("Pak40"), TEXT("/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Pak40_75MM/Ger_SquadTW_Pak40_75MM.Ger_SquadTW_Pak40_75MM_C")},
		{TEXT("Mortar"), TEXT("/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/WheeledMortar/Ger_SquadTW_80mmMortar.Ger_SquadTW_80mmMortar_C")},
		{TEXT("Morser"), TEXT("/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Morser210MM/Ger_SquadTW_Morser210.Ger_SquadTW_Morser210_C")},
	};

	enum class EStep : uint8
	{
		StartingGame,
		SpawningSubject,
		WaitingForSubject,
		WalkingToEnemy,
		Fighting,
		WaitingForMoveToResume,
		WalkingToDestination,
		Finished
	};

	UWorld* FindGameWorld()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (IsValid(World) && World->IsGameWorld() && World->HasBegunPlay())
			{
				return World;
			}
		}
		return nullptr;
	}
}

/** @brief Runs the lanes from a core ticker, so nothing in the game has to know about this test. */
class FAttackMoveMapTest
{
public:
	bool Tick()
	{
		using namespace AttackMoveMapTestPrivate;
		UWorld* World = FindGameWorld();
		if (not IsValid(World))
		{
			return true;
		}
		if (M_Step != EStep::StartingGame)
		{
			FGrenadeTestHelpers::KeepGameRunning(*World, M_PlayerController.Get());
			KeepSubjectAliveAndUnthrottled();
			KeepEnemiesDisarmed();
			TraceLane(*World);
		}
		switch (M_Step)
		{
		case EStep::StartingGame:
			TickStartingGame(*World);
			break;
		case EStep::SpawningSubject:
			TickSpawningSubject(*World);
			break;
		case EStep::WaitingForSubject:
			TickWaitingForSubject(*World);
			break;
		case EStep::WalkingToEnemy:
			TickWalkingToEnemy(*World);
			break;
		case EStep::Fighting:
			TickFighting(*World);
			break;
		case EStep::WaitingForMoveToResume:
			TickWaitingForMoveToResume(*World);
			break;
		case EStep::WalkingToDestination:
			TickWalkingToDestination(*World);
			break;
		case EStep::Finished:
			return false;
		}
		return true;
	}

private:
	AttackMoveMapTestPrivate::EStep M_Step = AttackMoveMapTestPrivate::EStep::StartingGame;
	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<AActor> M_Subject;
	TWeakObjectPtr<AActor> M_Enemies[AttackMoveMapTestPrivate::EnemyCount];
	FVector M_LaneStart = FVector::ZeroVector;
	FVector M_Destination = FVector::ZeroVector;
	FVector M_LocationWhenFightEnded = FVector::ZeroVector;
	float M_StepStartSeconds = 0.f;
	float M_NextTraceSeconds = 0.f;
	int32 M_SubjectIndex = 0;
	int32 M_FightsDone = 0;
	int32 M_MagazineAtFightStart = 0;
	bool bM_HasFiredInThisFight = false;
	int32 M_FailedChecks = 0;

	const TCHAR* GetSubjectName() const
	{
		return AttackMoveMapTestPrivate::SubjectDefinitions[M_SubjectIndex].Name;
	}

	ICommands* GetSubjectCommands() const
	{
		return Cast<ICommands>(M_Subject.Get());
	}

	ATeamWeaponController* GetSubjectTeamWeapon() const
	{
		return Cast<ATeamWeaponController>(M_Subject.Get());
	}

	void Check(const bool bCondition, const TCHAR* Description, const FString& Details = FString())
	{
		M_FailedChecks += bCondition ? 0 : 1;
		UE_LOG(LogRTSAttackMoveTest, Display, TEXT("RTS_ATTACKMOVE_TEST %s subject=%s check='%s' %s"),
		       bCondition ? TEXT("PASS") : TEXT("FAIL"), GetSubjectName(), Description, *Details);
	}

	void EnterStep(const UWorld& World, const AttackMoveMapTestPrivate::EStep NewStep)
	{
		M_Step = NewStep;
		M_StepStartSeconds = World.GetTimeSeconds();
	}

	float GetStepSeconds(const UWorld& World) const
	{
		return World.GetTimeSeconds() - M_StepStartSeconds;
	}

	FString DescribeSubject() const
	{
		ICommands* const Commands = GetSubjectCommands();
		const AActor* Subject = M_Subject.Get();
		const ATeamWeaponController* TeamWeapon = GetSubjectTeamWeapon();
		return FString::Printf(TEXT("command=%s attack_move=%d at=%s tw_state=%d"),
		                       Commands ? *Global_GetAbilityIDAsString(Commands->GetActiveCommandID()) : TEXT("none"),
		                       Commands && Commands->GetIsValidCommandData()
			                       ? Commands->GetIsValidCommandData()->GetIsActiveCommandAttackMove()
			                       : 0,
		                       IsValid(Subject) ? *Subject->GetActorLocation().ToCompactString() : TEXT("none"),
		                       IsValid(TeamWeapon) ? static_cast<int32>(TeamWeapon->GetTeamWeaponState()) : -1);
	}

	bool FailStepOnTimeout(UWorld& World, const float TimeoutSeconds, const TCHAR* ExpectedEvent)
	{
		if (GetStepSeconds(World) < TimeoutSeconds)
		{
			return false;
		}
		Check(false, ExpectedEvent, TEXT("timed out: ") + DescribeSubject());
		FinishSubject(World);
		return true;
	}

	float GetSubjectSpeed() const
	{
		const AActor* Subject = M_Subject.Get();
		if (const ATeamWeaponController* TeamWeapon = GetSubjectTeamWeapon())
		{
			const ATeamWeapon* Weapon = TeamWeapon->GetControlledTeamWeaponNoReport();
			return IsValid(Weapon) ? Weapon->GetVelocity().Size2D() : 0.f;
		}
		if (ASquadController* Squad = Cast<ASquadController>(M_Subject.Get()))
		{
			float FastestSoldier = 0.f;
			for (const ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
			{
				FastestSoldier = FMath::Max(FastestSoldier, Soldier->GetVelocity().Size2D());
			}
			return FastestSoldier;
		}
		return IsValid(Subject) ? Subject->GetVelocity().Size2D() : 0.f;
	}

	int32 GetSubjectMovementType() const
	{
		const ATeamWeaponController* TeamWeapon = GetSubjectTeamWeapon();
		const ATeamWeapon* Weapon = IsValid(TeamWeapon) ? TeamWeapon->GetControlledTeamWeaponNoReport() : nullptr;
		return IsValid(Weapon) ? static_cast<int32>(Weapon->GetMovementType()) : -1;
	}

	// Team weapons do not enter the in-combat state by firing, so their magazines tell whether they fired.
	int32 GetTeamWeaponMagazine(bool& bOutIsReloading) const
	{
		bOutIsReloading = false;
		const ATeamWeaponController* TeamWeapon = GetSubjectTeamWeapon();
		ATeamWeapon* Weapon = IsValid(TeamWeapon) ? TeamWeapon->GetControlledTeamWeaponNoReport() : nullptr;
		if (not IsValid(Weapon))
		{
			return 0;
		}
		int32 Magazine = 0;
		for (const UWeaponState* WeaponState : Weapon->GetWeapons())
		{
			if (IsValid(WeaponState))
			{
				Magazine += WeaponState->GetCurrentMagCapacity();
				bOutIsReloading = bOutIsReloading || WeaponState->GetIsReloading();
			}
		}
		return Magazine;
	}

	void BeginFight(const UWorld& World)
	{
		bool bIsReloading = false;
		M_MagazineAtFightStart = GetTeamWeaponMagazine(bIsReloading);
		bM_HasFiredInThisFight = false;
		EnterStep(World, AttackMoveMapTestPrivate::EStep::Fighting);
	}

	void UpdateHasFired()
	{
		ICommands* const Commands = GetSubjectCommands();
		bool bIsReloading = false;
		const int32 Magazine = GetTeamWeaponMagazine(bIsReloading);
		bM_HasFiredInThisFight = bM_HasFiredInThisFight || bIsReloading || Magazine < M_MagazineAtFightStart ||
			(Commands != nullptr && Commands->GetIsUnitInCombat());
	}

	float GetSubjectWeaponRange() const
	{
		if (ASquadController* Squad = Cast<ASquadController>(M_Subject.Get()))
		{
			return Squad->GetSquadRange();
		}
		const ATrackedTankMaster* Tank = Cast<ATrackedTankMaster>(M_Subject.Get());
		return IsValid(Tank) ? Tank->GetVehicleHighestWeaponRange() : 0.f;
	}

	void KeepUnitAliveAndUnthrottled(AActor* Unit) const
	{
		if (not IsValid(Unit))
		{
			return;
		}
		// Unseen units tick coarsely in an unattended run, and the enemy tanks would otherwise end the test early.
		TInlineComponentArray<URTSOptimizer*> Optimizers(Unit);
		for (URTSOptimizer* Optimizer : Optimizers)
		{
			Optimizer->SetOptimizationEnabled(false);
		}
		TInlineComponentArray<UHealthComponent*> HealthComponents(Unit);
		for (UHealthComponent* HealthComponent : HealthComponents)
		{
			HealthComponent->AddHealth(AttackMoveMapTestPrivate::KeepAliveHealthPerTick);
		}
	}

	void KeepSubjectAliveAndUnthrottled() const
	{
		KeepUnitAliveAndUnthrottled(M_Subject.Get());
		ASquadController* Squad = Cast<ASquadController>(M_Subject.Get());
		if (not IsValid(Squad))
		{
			return;
		}
		for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			KeepUnitAliveAndUnthrottled(Soldier);
		}
	}

	// The enemies are only there to be found: an armed tank kills a squad before the test reaches its second fight.
	void KeepEnemiesDisarmed() const
	{
		for (const TWeakObjectPtr<AActor>& Enemy : M_Enemies)
		{
			const ATankMaster* EnemyTank = Cast<ATankMaster>(Enemy.Get());
			if (not IsValid(EnemyTank))
			{
				continue;
			}
			for (ACPPTurretsMaster* Turret : EnemyTank->GetTurrets())
			{
				if (IsValid(Turret))
				{
					Turret->DisableTurret();
				}
			}
			for (UHullWeaponComponent* HullWeapon : EnemyTank->GetHullWeapons())
			{
				if (IsValid(HullWeapon))
				{
					HullWeapon->DisableHullWeapon();
				}
			}
		}
	}

	void TraceLane(const UWorld& World)
	{
		using namespace AttackMoveMapTestPrivate;
		if (World.GetTimeSeconds() < M_NextTraceSeconds || not M_Subject.IsValid())
		{
			return;
		}
		M_NextTraceSeconds = World.GetTimeSeconds() + TraceIntervalSeconds;
		const AActor* FirstEnemy = M_Enemies[0].Get();
		const AActor* SecondEnemy = M_Enemies[1].Get();
		UE_LOG(LogRTSAttackMoveTest, Display, TEXT("RTS_ATTACKMOVE_TEST trace subject=%s step=%d %s speed=%.0f enemy0=%s enemy1=%s"),
		       GetSubjectName(), static_cast<int32>(M_Step), *DescribeSubject(), GetSubjectSpeed(),
		       IsValid(FirstEnemy) ? *FirstEnemy->GetActorLocation().ToCompactString() : TEXT("gone"),
		       IsValid(SecondEnemy) ? *SecondEnemy->GetActorLocation().ToCompactString() : TEXT("gone"));
	}

	FString DescribePathToDestination(UWorld& World) const
	{
		if (not M_Subject.IsValid())
		{
			return TEXT("no subject");
		}
		UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
			&World, M_Subject->GetActorLocation(), M_Destination);
		if (not IsValid(Path) || not Path->IsValid())
		{
			return TEXT("no path to the destination");
		}
		return FString::Printf(TEXT("path_partial=%d path_length=%.0f path_points=%d"),
		                       Path->IsPartial(), Path->GetPathLength(), Path->PathPoints.Num());
	}

	void TickStartingGame(UWorld& World)
	{
		if (not FGrenadeTestHelpers::TryStartGame(World, M_PlayerController))
		{
			return;
		}
		if (not FGrenadeTestHelpers::FindTestSpot(World, AttackMoveMapTestPrivate::LocationSwitch, M_LaneStart))
		{
			UE_LOG(LogRTSAttackMoveTest, Error, TEXT("RTS_ATTACKMOVE_TEST no test spot on this map"));
			++M_FailedChecks;
			FinishTest(World);
			return;
		}
		UE_LOG(LogRTSAttackMoveTest, Display, TEXT("RTS_ATTACKMOVE_TEST lane_start=%s"), *M_LaneStart.ToCompactString());
		EnterStep(World, AttackMoveMapTestPrivate::EStep::SpawningSubject);
	}

	void TickSpawningSubject(UWorld& World)
	{
		using namespace AttackMoveMapTestPrivate;
		const FTransform SpawnTransform(FRotator::ZeroRotator, M_LaneStart);
		M_Subject = FGrenadeTestHelpers::SpawnUnitForPlayer(
			World, SubjectDefinitions[M_SubjectIndex].ClassPath, SpawnTransform, PlayerOwner);
		M_FightsDone = 0;
		if (not M_Subject.IsValid())
		{
			Check(false, TEXT("subject spawns"), SubjectDefinitions[M_SubjectIndex].ClassPath);
			FinishSubject(World);
			return;
		}
		EnterStep(World, EStep::WaitingForSubject);
	}

	bool GetIsSubjectReady() const
	{
		ICommands* const Commands = GetSubjectCommands();
		if (Commands == nullptr || not Commands->HasAbility(EAbilityID::IdMove) ||
			not Commands->HasAbility(EAbilityID::IdAttack))
		{
			return false;
		}
		const ASquadController* Squad = Cast<ASquadController>(M_Subject.Get());
		if (IsValid(Squad) && not Squad->GetIsSquadFullyLoadedAndInitialized())
		{
			return false;
		}
		const ATeamWeaponController* TeamWeapon = GetSubjectTeamWeapon();
		return not IsValid(TeamWeapon) || TeamWeapon->GetTeamWeaponState() == ETeamWeaponState::Ready_Deployed ||
			TeamWeapon->GetTeamWeaponState() == ETeamWeaponState::Ready_Packed;
	}

	void TickWaitingForSubject(UWorld& World)
	{
		using namespace AttackMoveMapTestPrivate;
		if (FailStepOnTimeout(World, StepTimeoutSeconds, TEXT("subject gets ready")))
		{
			return;
		}
		if (GetStepSeconds(World) < SubjectReadySeconds || not GetIsSubjectReady())
		{
			return;
		}
		LayOutLaneAndOrderAttackMove(World);
	}

	void LayOutLaneAndOrderAttackMove(UWorld& World)
	{
		using namespace AttackMoveMapTestPrivate;
		const FVector Start = M_Subject->GetActorLocation();
		const float EnemyDistances[EnemyCount] = {FirstEnemyDistanceCm, SecondEnemyDistanceCm};
		for (int32 EnemyIndex = 0; EnemyIndex < EnemyCount; ++EnemyIndex)
		{
			const FVector EnemyLocation = Start + FVector(EnemyDistances[EnemyIndex], EnemySideOffsetCm, 0.f);
			M_Enemies[EnemyIndex] = FGrenadeTestHelpers::SpawnUnitForPlayer(
				World, EnemyTankClass, FTransform(FRotator(0.f, 180.f, 0.f), EnemyLocation), EnemyOwner);
			Check(M_Enemies[EnemyIndex].IsValid(), TEXT("enemy spawns"));
		}
		M_Destination = Start + FVector(DestinationDistanceCm, 0.f, 0.f);
		FGrenadeTestHelpers::ProjectOntoNavMesh(World, M_Destination, FVector(1500.f));

		ICommands* const Commands = GetSubjectCommands();
		const ECommandQueueError OrderError = Commands->AttackMoveToLocation(M_Destination, true, FRotator::ZeroRotator);
		Check(DescribePathToDestination(World).StartsWith(TEXT("path_partial=0")), TEXT("the lane is walkable"),
		      DescribePathToDestination(World));
		Check(OrderError == ECommandQueueError::NoError, TEXT("attack move order is accepted"),
		      FString::Printf(TEXT("range=%.0f destination=%s movement_type=%d %s"), GetSubjectWeaponRange(),
		                      *M_Destination.ToCompactString(), GetSubjectMovementType(), *DescribePathToDestination(World)));
		Check(Commands->GetIsValidCommandData()->GetIsActiveCommandAttackMove(), TEXT("active command is an attack move"));
		EnterStep(World, EStep::WalkingToEnemy);
	}

	void TickWalkingToEnemy(UWorld& World)
	{
		using namespace AttackMoveMapTestPrivate;
		ICommands* const Commands = GetSubjectCommands();
		if (Commands == nullptr)
		{
			Check(false, TEXT("subject survives"));
			FinishSubject(World);
			return;
		}
		if (FailStepOnTimeout(World, WalkingTimeoutSeconds, TEXT("subject stops for an enemy")))
		{
			return;
		}
		if (Commands->GetIsUnitIdle())
		{
			Check(false, TEXT("subject stays on the attack move until the enemy"),
			      DescribeSubject() + TEXT(" ") + DescribePathToDestination(World));
			FinishSubject(World);
			return;
		}
		if (Commands->GetActiveCommandID() != EAbilityID::IdAttack)
		{
			return;
		}
		AActor* Target = Commands->GetActiveCommandTargetActor();
		Check(Target == M_Enemies[M_FightsDone].Get(), TEXT("attack move attacks the enemy on the lane"),
		      FString::Printf(TEXT("fight=%d target=%s distance=%.0f"), M_FightsDone, *GetNameSafe(Target),
		                      IsValid(Target) ? FVector::Dist2D(Target->GetActorLocation(), M_Subject->GetActorLocation()) : -1.f));
		BeginFight(World);
	}

	void TickFighting(UWorld& World)
	{
		using namespace AttackMoveMapTestPrivate;
		ICommands* const Commands = GetSubjectCommands();
		UpdateHasFired();
		if (not IsValid(M_Enemies[M_FightsDone].Get()))
		{
			// A strong gun finishes the fight by itself, which is the case the player sees.
			Check(bM_HasFiredInThisFight, TEXT("subject destroyed the enemy itself"), DescribeSubject());
			++M_FightsDone;
			EnterStep(World, EStep::WaitingForMoveToResume);
			return;
		}
		if (FailStepOnTimeout(World, StepTimeoutSeconds, TEXT("subject deploys and fires at the enemy")))
		{
			return;
		}
		// The enemies are disarmed, so a shot can only come from this unit.
		if (GetStepSeconds(World) < FightSeconds || not bM_HasFiredInThisFight)
		{
			return;
		}
		// The team weapon trap: a gun stopped mid-move has to leave Moving and deploy, or the fight never starts.
		const ATeamWeaponController* TeamWeapon = GetSubjectTeamWeapon();
		if (IsValid(TeamWeapon) && TeamWeapon->GetTeamWeaponState() != ETeamWeaponState::Ready_Deployed)
		{
			return;
		}
		Check(Commands != nullptr && Commands->GetActiveCommandID() == EAbilityID::IdAttack, TEXT("subject is still fighting"),
		      DescribeSubject());
		Check(GetSubjectSpeed() < StoppedSpeedCmPerSecond, TEXT("subject stopped for the fight"),
		      FString::Printf(TEXT("speed=%.0f in_combat=%d"), GetSubjectSpeed(),
		                      Commands != nullptr && Commands->GetIsUnitInCombat()));
		M_LocationWhenFightEnded = M_Subject->GetActorLocation();
		if (AHpPawnMaster* Enemy = Cast<AHpPawnMaster>(M_Enemies[M_FightsDone].Get()))
		{
			Enemy->TakeFatalDamage();
		}
		++M_FightsDone;
		EnterStep(World, EStep::WaitingForMoveToResume);
	}

	void TickWaitingForMoveToResume(UWorld& World)
	{
		using namespace AttackMoveMapTestPrivate;
		ICommands* const Commands = GetSubjectCommands();
		if (FailStepOnTimeout(World, StepTimeoutSeconds, TEXT("attack move resumes after the fight")))
		{
			return;
		}
		const bool bIsOnNextEnemy = Commands != nullptr && M_FightsDone < EnemyCount &&
			Commands->GetActiveCommandID() == EAbilityID::IdAttack &&
			Commands->GetActiveCommandTargetActor() == M_Enemies[M_FightsDone].Get();
		if (bIsOnNextEnemy)
		{
			// The next enemy was already in reach, so the move lasted only until the scan found it.
			Check(true, TEXT("attack move goes straight on to the next enemy"), DescribeSubject());
			BeginFight(World);
			return;
		}
		const bool bHasArrived = Commands != nullptr && M_FightsDone == EnemyCount && Commands->GetIsUnitIdle();
		if (bHasArrived)
		{
			EnterStep(World, EStep::WalkingToDestination);
			return;
		}
		if (Commands == nullptr || not Commands->GetIsValidCommandData()->GetIsActiveCommandAttackMove())
		{
			return;
		}
		Check(true, TEXT("attack move resumes after the fight"),
		      FString::Printf(TEXT("fight=%d after=%.1fs %s"), M_FightsDone, GetStepSeconds(World), *DescribeSubject()));
		EnterStep(World, M_FightsDone < EnemyCount ? EStep::WalkingToEnemy : EStep::WalkingToDestination);
	}

	void TickWalkingToDestination(UWorld& World)
	{
		using namespace AttackMoveMapTestPrivate;
		ICommands* const Commands = GetSubjectCommands();
		if (FailStepOnTimeout(World, WalkingTimeoutSeconds, TEXT("subject arrives")))
		{
			return;
		}
		if (Commands == nullptr || not Commands->GetIsUnitIdle())
		{
			return;
		}
		const float DistanceToDestination = FVector::Dist2D(M_Subject->GetActorLocation(), M_Destination);
		const float DistanceWalkedAfterFight = FVector::Dist2D(M_Subject->GetActorLocation(), M_LocationWhenFightEnded);
		Check(DistanceToDestination < ArrivalToleranceCm, TEXT("subject arrives at the destination"),
		      FString::Printf(TEXT("distance=%.0f walked_after_last_fight=%.0f"), DistanceToDestination,
		                      DistanceWalkedAfterFight));
		FinishSubject(World);
	}

	void DestroyLane()
	{
		if (M_Subject.IsValid())
		{
			M_Subject->Destroy();
		}
		for (TWeakObjectPtr<AActor>& Enemy : M_Enemies)
		{
			if (Enemy.IsValid())
			{
				Enemy->Destroy();
			}
		}
	}

	void FinishSubject(UWorld& World)
	{
		using namespace AttackMoveMapTestPrivate;
		DestroyLane();
		++M_SubjectIndex;
		if (M_SubjectIndex >= UE_ARRAY_COUNT(SubjectDefinitions))
		{
			FinishTest(World);
			return;
		}
		EnterStep(World, EStep::SpawningSubject);
	}

	void FinishTest(UWorld& World)
	{
		UE_LOG(LogRTSAttackMoveTest, Display, TEXT("RTS_ATTACKMOVE_TEST RESULT %s failed_checks=%d"),
		       M_FailedChecks == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedChecks);
		EnterStep(World, AttackMoveMapTestPrivate::EStep::Finished);
		FPlatformMisc::RequestExit(false);
	}
};

static FDelayedAutoRegisterHelper GRegisterAttackMoveMapTest(EDelayedRegisterRunPhase::EndOfEngineInit, []()
{
	if (not FParse::Param(FCommandLine::Get(), TEXT("AttackMoveValidate")))
	{
		return;
	}
	const TSharedRef<FAttackMoveMapTest> Test = MakeShared<FAttackMoveMapTest>();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Test](float)
	{
		return Test->Tick();
	}));
});

#endif
