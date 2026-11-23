// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TankDigInTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Navigation/PathFollowingComponent.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInWall/DigInWall.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSEngagementStance/RTSAggroBehaviour.h"
#include "RTS_Survival/Units/Tanks/AITankMaster.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackPhysicsMovementComp/TrackPhysicsMovement.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSTankDigIn, Log, All);

namespace TankDigInTestPrivate
{
	const TCHAR* const ValidateSwitch = TEXT("TankDigInValidate");
	const TCHAR* const LocationSwitch = TEXT("TankDigInAt=");
	const TCHAR* const DugInTankClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/"
		"Ger_Vehicle_PanzerIV_H.Ger_Vehicle_PanzerIV_H_C");
	const TCHAR* const EnemyTankClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/"
		"Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");
	constexpr uint8 PlayerOwner = 1;
	constexpr uint8 EnemyOwner = 2;

	// Real seconds: the HUD and start widget need a moment before the game can be started.
	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartRealSeconds = 90.0;
	constexpr double MaxRunRealSeconds = 900.0;

	// Game seconds per step.
	constexpr float SettleSeconds = 3.f;
	constexpr float MaxDigInSeconds = 60.f;
	constexpr float RamSeconds = 10.f;
	constexpr float ClearRamSeconds = 2.f;
	constexpr float AttackTargetSettleSeconds = 2.f;
	constexpr float AttackWatchSeconds = 12.f;
	constexpr float MaxBreakCoverSeconds = 15.f;
	constexpr float DriveOffSeconds = 20.f;

	// Layout, in centimetres.
	constexpr float ControlLaneOffset = 3500.f;
	// The pushers start broadside to the rammed hull, close enough to reach it within a second at full throttle.
	constexpr float PusherStartOffset = 750.f;
	constexpr float PusherYawTowardsPositiveY = 90.f;
	constexpr float FarEnemyBeyondRange = 2500.f;
	constexpr float NearEnemyRangeFraction = 0.5f;
	constexpr float DriveOffDistance = 1500.f;

	// Pass limits.
	constexpr float DugInMaxShoveCm = 15.f;
	constexpr float DugInMaxYawChangeDegrees = 1.5f;
	// The control tank must be moved at least this far, or the ram was too soft to tell anything.
	constexpr float ControlMinShoveCm = 30.f;
	constexpr float ControlMinYawChangeDegrees = 3.f;
	// A pusher that drove this far from its start reached and pressed on the hull.
	constexpr float PusherMinApproachCm = 150.f;
	constexpr float AttackMustEndWithinSeconds = 6.f;
	constexpr float DugInMaxAttackDriftCm = 15.f;
	constexpr float DriveOffMinDistanceCm = 600.f;
}

// ---------------------------------------------------------------------------------------------------------------
// Subsystem lifecycle

bool UTankDigInTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TankDigInTestPrivate::ValidateSwitch);
}

void UTankDigInTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Error dialogs stall unattended runs; they still reach the log.
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	bM_IsActive = true;
	M_StartRealSeconds = FPlatformTime::Seconds();
	UE_LOG(LogRTSTankDigIn, Display, TEXT("RTS_TANKDIG started"));
}

void UTankDigInTestSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

TStatId UTankDigInTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTankDigInTestSubsystem, STATGROUP_Tickables);
}

float UTankDigInTestSubsystem::GetGameSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

float UTankDigInTestSubsystem::GetSecondsInStep() const
{
	return GetGameSeconds() - M_StepStartGameSeconds;
}

void UTankDigInTestSubsystem::EnterStep(const ETankDigInTestStep NewStep)
{
	M_Step = NewStep;
	M_StepStartGameSeconds = GetGameSeconds();
}

void UTankDigInTestSubsystem::Check(const bool bCondition, const TCHAR* Description, const FString& Details)
{
	(bCondition ? M_PassedCheckCount : M_FailedCheckCount)++;
	UE_LOG(LogRTSTankDigIn, Display, TEXT("RTS_TANKDIG check=%s %s %s (t=%.1f)"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), Description, *Details, GetGameSeconds());
}

void UTankDigInTestSubsystem::Finish(const TCHAR* Reason)
{
	if (bM_IsFinished)
	{
		return;
	}
	bM_IsFinished = true;
	EnterStep(ETankDigInTestStep::Done);
	UE_LOG(LogRTSTankDigIn, Display, TEXT("RTS_TANKDIG RESULT %s passed=%d failed=%d reason=%s game_t=%.1f real_s=%.0f"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount, Reason,
	       GetGameSeconds(), FPlatformTime::Seconds() - M_StartRealSeconds);
	FPlatformMisc::RequestExit(false, TEXT("TankDigInTest"));
}

void UTankDigInTestSubsystem::Tick(const float DeltaTime)
{
	using namespace TankDigInTestPrivate;
	UWorld* World = GetWorld();
	if (not bM_IsActive || bM_IsFinished || not IsValid(World))
	{
		return;
	}
	const double RealSeconds = FPlatformTime::Seconds() - M_StartRealSeconds;
	if (M_Step == ETankDigInTestStep::WaitingForGameStart)
	{
		TickWaitingForGameStart(RealSeconds);
		return;
	}
	FGrenadeTestHelpers::KeepGameRunning(*World, M_PlayerController.Get());
	if (RealSeconds > MaxRunRealSeconds)
	{
		Check(false, TEXT("finished_in_time"), TEXT(""));
		Finish(TEXT("timeout"));
		return;
	}
	if (M_Step != ETankDigInTestStep::Done && not M_DugInTank.IsValid())
	{
		Check(false, TEXT("dug_in_tank_alive"), TEXT(""));
		Finish(TEXT("tank_lost"));
		return;
	}
	switch (M_Step)
	{
	case ETankDigInTestStep::SettlingBeforeDigIn:
		TickSettlingBeforeDigIn();
		break;
	case ETankDigInTestStep::WaitingForDigIn:
		TickWaitingForDigIn();
		break;
	case ETankDigInTestStep::Ramming:
		TickRamming(DeltaTime);
		break;
	case ETankDigInTestStep::ClearingRam:
		TickClearingRam();
		break;
	case ETankDigInTestStep::SettlingAttackTargets:
		TickSettlingAttackTargets();
		break;
	case ETankDigInTestStep::WatchingAttack:
		TickWatchingAttack();
		break;
	case ETankDigInTestStep::WaitingForBreakCover:
		TickWaitingForBreakCover();
		break;
	case ETankDigInTestStep::DrivingOff:
		TickDrivingOff();
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Helpers

ATrackedTankMaster* UTankDigInTestSubsystem::SpawnTank(const TCHAR* ClassPath, const FVector& Location,
                                                      const float Yaw, const uint8 OwningPlayer, const TCHAR* Label)
{
	ATrackedTankMaster* Tank = Cast<ATrackedTankMaster>(FGrenadeTestHelpers::SpawnUnitForPlayer(
		*GetWorld(), ClassPath, FTransform(FRotator(0.f, Yaw, 0.f), Location), OwningPlayer));
	UE_LOG(LogRTSTankDigIn, Display, TEXT("RTS_TANKDIG spawned %s=%s owner=%d at %s"), Label,
	       IsValid(Tank) ? *Tank->GetName() : TEXT("nothing"), OwningPlayer,
	       IsValid(Tank) ? *Tank->GetActorLocation().ToCompactString() : TEXT("-"));
	return Tank;
}

void UTankDigInTestSubsystem::HoldEnemyQuiet(ATrackedTankMaster* Enemy, const bool bRevealToPlayer) const
{
	if (not IsValid(Enemy))
	{
		return;
	}
	// Idle tanks set their turrets to auto-engage again, so this is repeated every tick.
	Enemy->SetTurretsDisabled();
	if (not bRevealToPlayer)
	{
		return;
	}
	UFowComp* const FowComponent = Enemy->FindComponentByClass<UFowComp>();
	if (IsValid(FowComponent))
	{
		FowComponent->StopFowParticipation();
	}
	if (Enemy->IsHidden())
	{
		Enemy->SetActorHiddenInGame(false);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Dig in

void UTankDigInTestSubsystem::TickWaitingForGameStart(const double RealSeconds)
{
	using namespace TankDigInTestPrivate;
	if (RealSeconds > MaxStartRealSeconds)
	{
		Check(false, TEXT("game_started"), TEXT(""));
		Finish(TEXT("no_start"));
		return;
	}
	UWorld& World = *GetWorld();
	if (RealSeconds < HudBuildSeconds || not FGrenadeTestHelpers::TryStartGame(World, M_PlayerController))
	{
		return;
	}
	if (not FGrenadeTestHelpers::FindTestSpot(World, LocationSwitch, M_Centre))
	{
		Check(false, TEXT("test_spot_found"), TEXT("pass -TankDigInAt=X,Y"));
		Finish(TEXT("no_spot"));
		return;
	}
	UE_LOG(LogRTSTankDigIn, Display, TEXT("RTS_TANKDIG centre %s"), *M_Centre.ToCompactString());
	ATrackedTankMaster* const DugInTank = SpawnTank(DugInTankClassPath, M_Centre, 0.f, PlayerOwner, TEXT("dug_in"));
	ATrackedTankMaster* const ControlTank = SpawnTank(DugInTankClassPath,
	                                                  M_Centre + FVector(0.f, ControlLaneOffset, 0.f), 0.f,
	                                                  PlayerOwner, TEXT("control"));
	Check(IsValid(DugInTank) && IsValid(ControlTank), TEXT("tanks_spawned"), TEXT(""));
	if (not IsValid(DugInTank) || not IsValid(ControlTank))
	{
		Finish(TEXT("spawn_failed"));
		return;
	}
	M_DugInTank = DugInTank;
	M_DugInRam.Tank = DugInTank;
	M_ControlRam.Tank = ControlTank;
	EnterStep(ETankDigInTestStep::SettlingBeforeDigIn);
}

void UTankDigInTestSubsystem::TickSettlingBeforeDigIn()
{
	if (GetSecondsInStep() < TankDigInTestPrivate::SettleSeconds)
	{
		return;
	}
	ATrackedTankMaster* const Tank = M_DugInTank.Get();
	const bool bHasDigIn = IsValid(Tank->GetDigInComponent());
	const ECommandQueueError Error = bHasDigIn ? Tank->DigIn(true) : ECommandQueueError::AbilityNotAllowed;
	Check(Error == ECommandQueueError::NoError, TEXT("dig_in_order_accepted"),
	      FString::Printf(TEXT("has_component=%d error=%s"), bHasDigIn ? 1 : 0, *UEnum::GetValueAsString(Error)));
	if (Error != ECommandQueueError::NoError)
	{
		Finish(TEXT("no_dig_in"));
		return;
	}
	EnterStep(ETankDigInTestStep::WaitingForDigIn);
}

void UTankDigInTestSubsystem::TickWaitingForDigIn()
{
	ATrackedTankMaster* const Tank = M_DugInTank.Get();
	const UDigInComponent* const DigIn = Tank->GetDigInComponent();
	// The dig-in command stays active while the tank stands dug in; the wall standing is what counts.
	const bool bIsDugIn = IsValid(DigIn) && DigIn->GetIsDugIn();
	if (not bIsDugIn)
	{
		if (GetSecondsInStep() > TankDigInTestPrivate::MaxDigInSeconds)
		{
			int32 WallCount = 0;
			for (TActorIterator<ADigInWall> It(GetWorld()); It; ++It)
			{
				++WallCount;
			}
			const USkeletalMeshComponent* const Chassis = Tank->GetTankMesh();
			Check(false, TEXT("dug_in_in_time"), FString::Printf(
				      TEXT("command=%s digging_or_dug_in=%d movable=%d walls=%d simulating=%d"),
				      *UEnum::GetValueAsString(Tank->GetActiveCommandID()),
				      IsValid(DigIn) && DigIn->GetIsDiggingInOrDugIn() ? 1 : 0,
				      IsValid(DigIn) && DigIn->GetIsMovable() ? 1 : 0, WallCount,
				      IsValid(Chassis) && Chassis->IsSimulatingPhysics() ? 1 : 0));
			Finish(TEXT("dig_in_timeout"));
		}
		return;
	}
	Check(true, TEXT("dug_in_in_time"), FString::Printf(TEXT("seconds=%.1f"), GetSecondsInStep()));
	const USkeletalMeshComponent* const DugInChassis = Tank->GetTankMesh();
	Check(IsValid(DugInChassis) && not DugInChassis->IsSimulatingPhysics(), TEXT("dug_in_chassis_held_kinematic"),
	      TEXT(""));
	Check(not Tank->GetCanBePushedAside(), TEXT("dug_in_not_pushable_by_allies"), TEXT(""));
	StartRam();
}

// ---------------------------------------------------------------------------------------------------------------
// Ram

void UTankDigInTestSubsystem::StartRam()
{
	using namespace TankDigInTestPrivate;
	FTankDigInRamTarget* const RamTargets[] = {&M_DugInRam, &M_ControlRam};
	for (int32 RamIndex = 0; RamIndex < UE_ARRAY_COUNT(RamTargets); ++RamIndex)
	{
		FTankDigInRamTarget& RamTarget = *RamTargets[RamIndex];
		ATrackedTankMaster* const Tank = RamTarget.Tank.Get();
		if (not IsValid(Tank))
		{
			continue;
		}
		// The dug-in tank's pusher comes from -Y, the control tank's from +Y, so the two rams stay apart.
		const float Side = RamIndex == 0 ? -1.f : 1.f;
		RamTarget.StartLocation = Tank->GetActorLocation();
		RamTarget.StartYaw = Tank->GetActorRotation().Yaw;
		ATrackedTankMaster* const Pusher = SpawnTank(
			EnemyTankClassPath, RamTarget.StartLocation + FVector(0.f, Side * PusherStartOffset, 0.f),
			-Side * PusherYawTowardsPositiveY, EnemyOwner, RamIndex == 0 ? TEXT("pusher_dug_in") : TEXT("pusher_control"));
		Check(IsValid(Pusher), TEXT("pusher_spawned"), RamIndex == 0 ? TEXT("dug_in") : TEXT("control"));
		if (not IsValid(Pusher))
		{
			continue;
		}
		Pusher->SetAggroStance(ERTSAggroBehaviour::Stance_HoldPosition);
		RamTarget.Pusher = Pusher;
		RamTarget.PusherStartLocation = Pusher->GetActorLocation();
	}
	EnterStep(ETankDigInTestStep::Ramming);
}

void UTankDigInTestSubsystem::TickRamming(const float DeltaTime)
{
	TickRamTarget(M_DugInRam, DeltaTime);
	TickRamTarget(M_ControlRam, DeltaTime);
	if (GetSecondsInStep() >= TankDigInTestPrivate::RamSeconds)
	{
		FinishRam();
	}
}

void UTankDigInTestSubsystem::TickRamTarget(FTankDigInRamTarget& RamTarget, const float DeltaTime) const
{
	ATrackedTankMaster* const Tank = RamTarget.Tank.Get();
	ATrackedTankMaster* const Pusher = RamTarget.Pusher.Get();
	if (not IsValid(Tank) || not IsValid(Pusher))
	{
		return;
	}
	// Neither side may shoot the other: a dead pusher stops pushing.
	Tank->SetTurretsDisabled();
	HoldEnemyQuiet(Pusher, false);
	UTrackPhysicsMovement* const PusherMovement = Pusher->GetTrackPhysicsMovement();
	if (IsValid(PusherMovement))
	{
		// Full throttle straight into the hull, past any path following or avoidance.
		PusherMovement->UpdateTankMovement(DeltaTime, Pusher->GetVelocity().Size2D(), 1.f, 0.f);
	}
	RamTarget.MaxShoveCm = FMath::Max(RamTarget.MaxShoveCm,
	                                  FVector::Dist2D(Tank->GetActorLocation(), RamTarget.StartLocation));
	RamTarget.MaxYawChangeDegrees = FMath::Max(
		RamTarget.MaxYawChangeDegrees,
		FMath::Abs(FMath::FindDeltaAngleDegrees(RamTarget.StartYaw, Tank->GetActorRotation().Yaw)));
}

void UTankDigInTestSubsystem::FinishRam()
{
	using namespace TankDigInTestPrivate;
	FTankDigInRamTarget* const RamTargets[] = {&M_DugInRam, &M_ControlRam};
	for (FTankDigInRamTarget* const RamTarget : RamTargets)
	{
		ATrackedTankMaster* const Pusher = RamTarget->Pusher.Get();
		// How far the pusher drove; a shoved control tank lets it drive on past where that tank stood.
		const float Approach = IsValid(Pusher)
			                       ? FVector::Dist2D(Pusher->GetActorLocation(), RamTarget->PusherStartLocation)
			                       : 0.f;
		const bool bIsDugIn = RamTarget == &M_DugInRam;
		Check(IsValid(Pusher) && Pusher->IsUnitAlive() && Approach >= PusherMinApproachCm,
		      bIsDugIn ? TEXT("pusher_pressed_on_dug_in_hull") : TEXT("pusher_pressed_on_control_hull"),
		      FString::Printf(TEXT("approach=%.0f cm"), Approach));
		if (IsValid(Pusher) && IsValid(Pusher->GetTrackPhysicsMovement()))
		{
			Pusher->GetTrackPhysicsMovement()->OnPathFollowingFinished();
		}
	}
	Check(M_DugInRam.MaxShoveCm <= DugInMaxShoveCm && M_DugInRam.MaxYawChangeDegrees <= DugInMaxYawChangeDegrees,
	      TEXT("dug_in_not_shoved_by_enemy_hull"),
	      FString::Printf(TEXT("shove=%.1f cm yaw=%.2f deg"), M_DugInRam.MaxShoveCm, M_DugInRam.MaxYawChangeDegrees));
	// Shows the ram itself moves a hull that is only parked; without it a passing dug-in check proves nothing.
	Check(M_ControlRam.MaxShoveCm >= ControlMinShoveCm || M_ControlRam.MaxYawChangeDegrees >= ControlMinYawChangeDegrees,
	      TEXT("control_parked_tank_shoved_by_same_ram"),
	      FString::Printf(TEXT("shove=%.1f cm yaw=%.2f deg"), M_ControlRam.MaxShoveCm,
	                      M_ControlRam.MaxYawChangeDegrees));
	// The ram units are done; destroy them so they take no part in the attack check.
	for (FTankDigInRamTarget* const RamTarget : RamTargets)
	{
		if (ATrackedTankMaster* const Pusher = RamTarget->Pusher.Get())
		{
			Pusher->TakeFatalDamage();
		}
	}
	if (ATrackedTankMaster* const ControlTank = M_ControlRam.Tank.Get())
	{
		ControlTank->TakeFatalDamage();
	}
	EnterStep(ETankDigInTestStep::ClearingRam);
}

// ---------------------------------------------------------------------------------------------------------------
// Attack out of range

void UTankDigInTestSubsystem::TickClearingRam()
{
	using namespace TankDigInTestPrivate;
	if (GetSecondsInStep() < ClearRamSeconds)
	{
		return;
	}
	ATrackedTankMaster* const Tank = M_DugInTank.Get();
	for (ACPPTurretsMaster* const Turret : Tank->GetTurrets())
	{
		if (IsValid(Turret))
		{
			Turret->SetAutoEngageTargets(false);
		}
	}
	const float Range = Tank->GetVehicleHighestWeaponRange();
	const FVector TankLocation = Tank->GetActorLocation();
	// The far enemy stands ahead, past the dig-in wall; the near one behind, inside range.
	M_FarEnemy = SpawnTank(EnemyTankClassPath, TankLocation + FVector(Range + FarEnemyBeyondRange, 0.f, 0.f), 180.f,
	                       EnemyOwner, TEXT("far_enemy"));
	M_NearEnemy = SpawnTank(EnemyTankClassPath, TankLocation - FVector(Range * NearEnemyRangeFraction, 0.f, 0.f), 0.f,
	                        EnemyOwner, TEXT("near_enemy"));
	Check(Range > 0.f && M_FarEnemy.IsValid() && M_NearEnemy.IsValid(), TEXT("attack_targets_spawned"),
	      FString::Printf(TEXT("range=%.0f cm"), Range));
	if (not M_FarEnemy.IsValid() || not M_NearEnemy.IsValid())
	{
		Finish(TEXT("attack_targets_missing"));
		return;
	}
	M_FarEnemy->SetAggroStance(ERTSAggroBehaviour::Stance_HoldPosition);
	M_NearEnemy->SetAggroStance(ERTSAggroBehaviour::Stance_HoldPosition);
	EnterStep(ETankDigInTestStep::SettlingAttackTargets);
}

void UTankDigInTestSubsystem::TickSettlingAttackTargets()
{
	HoldEnemyQuiet(M_FarEnemy.Get(), true);
	HoldEnemyQuiet(M_NearEnemy.Get(), true);
	if (GetSecondsInStep() < TankDigInTestPrivate::AttackTargetSettleSeconds)
	{
		return;
	}
	ATrackedTankMaster* const Tank = M_DugInTank.Get();
	const float FarDistance = M_FarEnemy.IsValid() ? FVector::Dist2D(M_FarEnemy->GetActorLocation(),
	                                                                 Tank->GetActorLocation()) : -1.f;
	const bool bIsFarVisible = RTSFunctionLibrary::RTSIsVisibleTarget(M_FarEnemy.Get(),
	                                                                  TankDigInTestPrivate::PlayerOwner);
	const ECommandQueueError Error = Tank->AttackActor(M_FarEnemy.Get(), true);
	Check(Error == ECommandQueueError::NoError && bIsFarVisible, TEXT("attack_on_far_enemy_ordered"),
	      FString::Printf(TEXT("error=%s visible=%d distance=%.0f range=%.0f"), *UEnum::GetValueAsString(Error),
	                      bIsFarVisible ? 1 : 0, FarDistance, Tank->GetVehicleHighestWeaponRange()));
	M_AttackStartLocation = Tank->GetActorLocation();
	EnterStep(ETankDigInTestStep::WatchingAttack);
}

void UTankDigInTestSubsystem::TickWatchingAttack()
{
	using namespace TankDigInTestPrivate;
	HoldEnemyQuiet(M_FarEnemy.Get(), true);
	HoldEnemyQuiet(M_NearEnemy.Get(), true);
	ATrackedTankMaster* const Tank = M_DugInTank.Get();
	const AAITankMaster* const TankController = Tank->GetAIController();
	if (IsValid(TankController) && TankController->GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		bM_DidDriveForTarget = true;
	}
	if (M_AttackEndedAfterSeconds < 0.f && Tank->GetActiveCommandID() != EAbilityID::IdAttack)
	{
		M_AttackEndedAfterSeconds = GetSecondsInStep();
	}
	const bool bIsNearEnemyDown = not M_NearEnemy.IsValid() || not M_NearEnemy->IsUnitAlive();
	for (const ACPPTurretsMaster* const Turret : Tank->GetTurrets())
	{
		if (IsValid(Turret) && M_NearEnemy.IsValid() && Turret->GetCurrentTargetActor() == M_NearEnemy.Get())
		{
			bM_DidEngageNearEnemy = true;
		}
	}
	bM_DidEngageNearEnemy |= bIsNearEnemyDown;
	if (GetSecondsInStep() < AttackWatchSeconds)
	{
		return;
	}
	Check(M_AttackEndedAfterSeconds >= 0.f && M_AttackEndedAfterSeconds <= AttackMustEndWithinSeconds,
	      TEXT("out_of_range_attack_dropped"), FString::Printf(TEXT("ended_after=%.1f s"), M_AttackEndedAfterSeconds));
	Check(not bM_DidDriveForTarget, TEXT("dug_in_never_drives_for_target"), TEXT(""));
	const float Drift = FVector::Dist2D(Tank->GetActorLocation(), M_AttackStartLocation);
	Check(Drift <= DugInMaxAttackDriftCm, TEXT("dug_in_stays_put_during_attack"),
	      FString::Printf(TEXT("drift=%.1f cm"), Drift));
	Check(bM_DidEngageNearEnemy, TEXT("engages_closer_target_in_range"),
	      FString::Printf(TEXT("near_enemy_down=%d"), bIsNearEnemyDown ? 1 : 0));
	const ECommandQueueError Error = Tank->BreakCover(true);
	Check(Error == ECommandQueueError::NoError, TEXT("break_cover_ordered"), UEnum::GetValueAsString(Error));
	EnterStep(ETankDigInTestStep::WaitingForBreakCover);
}

// ---------------------------------------------------------------------------------------------------------------
// Break cover

void UTankDigInTestSubsystem::TickWaitingForBreakCover()
{
	using namespace TankDigInTestPrivate;
	ATrackedTankMaster* const Tank = M_DugInTank.Get();
	const UDigInComponent* const DigIn = Tank->GetDigInComponent();
	if (not IsValid(DigIn) || not DigIn->GetIsMovable())
	{
		if (GetSecondsInStep() > MaxBreakCoverSeconds)
		{
			Check(false, TEXT("cover_broken_in_time"), TEXT(""));
			Finish(TEXT("break_cover_timeout"));
		}
		return;
	}
	const USkeletalMeshComponent* const Chassis = Tank->GetTankMesh();
	Check(IsValid(Chassis) && Chassis->IsSimulatingPhysics(), TEXT("break_cover_restores_chassis_physics"), TEXT(""));
	M_DriveOffGoal = Tank->GetActorLocation() + FVector(0.f, DriveOffDistance, 0.f);
	const ECommandQueueError Error = Tank->MoveToLocation(M_DriveOffGoal, true, FRotator(0.f, 90.f, 0.f));
	Check(Error == ECommandQueueError::NoError, TEXT("move_after_break_cover_ordered"), UEnum::GetValueAsString(Error));
	M_AttackStartLocation = Tank->GetActorLocation();
	EnterStep(ETankDigInTestStep::DrivingOff);
}

void UTankDigInTestSubsystem::TickDrivingOff()
{
	ATrackedTankMaster* const Tank = M_DugInTank.Get();
	const float Moved = FVector::Dist2D(Tank->GetActorLocation(), M_AttackStartLocation);
	if (Moved < TankDigInTestPrivate::DriveOffMinDistanceCm &&
		GetSecondsInStep() < TankDigInTestPrivate::DriveOffSeconds)
	{
		return;
	}
	Check(Moved >= TankDigInTestPrivate::DriveOffMinDistanceCm, TEXT("drives_after_break_cover"),
	      FString::Printf(TEXT("moved=%.0f cm"), Moved));
	Finish(TEXT("all_steps_done"));
}

#else

bool UTankDigInTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UTankDigInTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UTankDigInTestSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UTankDigInTestSubsystem::Tick(float DeltaTime)
{
}

TStatId UTankDigInTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTankDigInTestSubsystem, STATGROUP_Tickables);
}

#endif
