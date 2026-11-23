// Copyright (C) Bas Blokzijl - All rights reserved.

// -AircraftFlightValidate: an airborne Bf 110 flies three sharp turns (90, 180 and 135 degrees), attacks an enemy
// armoured car, goes idle once the car is gone and must find and attack a second car behind it on its own
// (UAircraftTestSubsystem). Every frame the speed, the sideslip between nose and flight direction, the yaw rate and its
// change, the roll rate and the height above the ground are measured per step; the rows go to
// Saved/AircraftFlightQA/<-AircraftFlightQALabel=, default flight>.csv for plotting.

#include "AircraftTestSubsystem.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Aircraft/AircraftState/AircraftState.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSEngagementStance/RTSAggroBehaviour.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAircraftFlightTest, Log, All);

namespace AircraftFlightTestPrivate
{
	const TCHAR* const AircraftClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/BF109/Ger_Aircraft_BF110Airborne.Ger_Aircraft_BF110Airborne_C");
	const TCHAR* const TargetClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/BTR152A/Rus_ArmoredCar_BTR152A.Rus_ArmoredCar_BTR152A_C");
	constexpr uint8 HumanPlayer = 1;
	constexpr uint8 EnemyPlayer = 2;
	constexpr float SpawnHeightAboveGround = 1800.f;
	// The aircraft first flies straight until it is this close to cruise speed (or the time is up), so the turns
	// start at cruise speed.
	constexpr float SettleSpeedRatio = 0.97f;
	constexpr float MaxSettleSeconds = 14.f;
	constexpr float SettleRunDistance = 60000.f;
	constexpr float TurnTimeoutSeconds = 25.f;
	constexpr float AttackTimeoutSeconds = 45.f;
	constexpr float IdleSeconds = 8.f;
	constexpr float ReacquireTimeoutSeconds = 40.f;
	constexpr float TargetDistance = 6000.f;
	// Speed and slip are only judged above this share of the cruise speed (the aircraft spawns at rest).
	constexpr float MinMovingSpeedRatio = 0.3f;
	// Judged limits for a smooth flight: the most speed a step may lose below its own fastest moment, and the idle
	// speed floor (idle flies at IdleMovementSpeedMlt, 0.66 on the Bf 110).
	constexpr float MaxSpeedDropRatio = 0.12f;
	constexpr float MinIdleSpeedRatio = 0.6f;
	// Frames this long are hitches (assets loading); the heading change over them is not a turn the player sees.
	constexpr float MaxJudgedFrameSeconds = 0.05f;
	// Player units elsewhere on the test map shoot at the targets too; they must outlive the aircraft's approach.
	constexpr float TargetHealth = 1000000.f;
	// The second target appears this share of the weapon range away, so the idle aircraft can find it.
	constexpr float ReacquireRangeShare = 0.8f;
	constexpr float MinReacquireDistance = 1500.f;
	constexpr float MaxSlipDeg = 4.f;
	// The flight model's turn rate acceleration (180 deg/s²) plus a margin: anything above is a snap.
	constexpr float MaxYawAccelerationDeg = 225.f;
	// A dive starts with the nose steady on the target and holds its heading.
	constexpr float MaxDiveStartYawRateDeg = 25.f;
	constexpr float MaxDiveHeadingSweepDeg = 20.f;
	constexpr float CloseTargetDistance = 1500.f;
	constexpr float CloseAttackTimeoutSeconds = 25.f;
	// The moving target starts ahead and to the right and drives across the aircraft's path at vehicle speed.
	constexpr float MovingTargetYaw = 35.f;
	constexpr float MovingTargetDistance = 7000.f;
	constexpr float MovingTargetDriveDistance = 9000.f;
	// An attack must reach its first dive within this (seconds); the re-acquire step includes finding the target.
	constexpr float MaxSecondsToFirstDive = 15.f;
	// A single turn order must not bank one way and then the other by more than this (degrees).
	constexpr float MaxCounterBankDeg = 15.f;
	// The bank never changes faster than this (deg/s): the flight model's roll rate cap plus a margin.
	constexpr float MaxRollRateDeg = 105.f;
	constexpr float MinDiveAboveGround = 900.f;
	constexpr float LandscapeTraceHalfHeight = 200000.f;
	// Frames shorter than this give a noisy yaw rate and are not used for its change.
	constexpr float MinRateDeltaSeconds = 0.004f;

	const TCHAR* GetStepName(const EAircraftFlightTestStep Step)
	{
		switch (Step)
		{
		case EAircraftFlightTestStep::Settling: return TEXT("settle");
		case EAircraftFlightTestStep::TurnRight: return TEXT("turn_right_90");
		case EAircraftFlightTestStep::TurnBack: return TEXT("turn_back_180");
		case EAircraftFlightTestStep::TurnSharpLeft: return TEXT("turn_left_135");
		case EAircraftFlightTestStep::Attacking: return TEXT("attack");
		case EAircraftFlightTestStep::IdleAfterAttack: return TEXT("idle");
		case EAircraftFlightTestStep::Reacquiring: return TEXT("reacquire");
		case EAircraftFlightTestStep::AttackingClose: return TEXT("attack_close");
		case EAircraftFlightTestStep::AttackingMoving: return TEXT("attack_moving");
		case EAircraftFlightTestStep::Done: return TEXT("done");
		}
		return TEXT("unknown");
	}

	FVector GetFlatDirection(const AAircraftMaster& Aircraft)
	{
		FVector Direction = Aircraft.GetVelocity().GetSafeNormal2D();
		if (Direction.IsNearlyZero())
		{
			Direction = Aircraft.GetActorForwardVector().GetSafeNormal2D();
		}
		return Direction;
	}
}

void UAircraftTestSubsystem::StartAircraftFlight()
{
	using namespace AircraftFlightTestPrivate;
	FAircraftFlightUnderTest& Entry = M_Flight;
	EnterPhase(EAircraftTestPhase::AircraftFlightRunning);
	Entry.StepMetrics.SetNum(static_cast<int32>(EAircraftFlightTestStep::Done) + 1);
	Entry.CsvRows.Add(TEXT("t,step,x,y,z,ground_z,speed_ratio,yaw,pitch,roll,yaw_rate,yaw_accel,slip,move_state,dive"));
	const FVector SpawnLocation = M_Anchor + FVector(0.f, 0.f, SpawnHeightAboveGround);
	Entry.Aircraft = Cast<AAircraftMaster>(SpawnUnitForPlayer(
		AircraftClassPath, FTransform(FRotator::ZeroRotator, SpawnLocation), HumanPlayer));
	Check(Entry.Aircraft.IsValid(), TEXT("flight_aircraft_spawned"), AircraftClassPath);
	if (not Entry.Aircraft.IsValid())
	{
		Finish();
		return;
	}
	Entry.Aircraft->SetAggroStance(ERTSAggroBehaviour::Stance_Aggressive);
	Entry.MaxMoveSpeed = FMath::Max(Entry.Aircraft->GetMaxMoveSpeedForTests(), 1.f);
	EnterFlightStep(Entry, EAircraftFlightTestStep::Settling);
	const ECommandQueueError Error = Entry.Aircraft->MoveToLocation(
		SpawnLocation + FVector(SettleRunDistance, 0.f, 0.f), true, FRotator::ZeroRotator);
	Check(Error == ECommandQueueError::NoError, TEXT("flight_settle_order_accepted"),
	      FString::Printf(TEXT("error=%d"), static_cast<int32>(Error)));
}

void UAircraftTestSubsystem::TickAircraftFlight()
{
	FAircraftFlightUnderTest& Entry = M_Flight;
	if (not Entry.Aircraft.IsValid())
	{
		Check(false, TEXT("flight_aircraft_alive"), TEXT("destroyed during the test"));
		FinishAircraftFlight(Entry);
		return;
	}
	RecordFlightSample(Entry);
	switch (Entry.Step)
	{
	case EAircraftFlightTestStep::Settling:
		TickFlightSettling(Entry);
		return;
	case EAircraftFlightTestStep::TurnRight:
	case EAircraftFlightTestStep::TurnBack:
	case EAircraftFlightTestStep::TurnSharpLeft:
		TickFlightTurning(Entry);
		return;
	case EAircraftFlightTestStep::Attacking:
		TickFlightAttacking(Entry);
		return;
	case EAircraftFlightTestStep::IdleAfterAttack:
		TickFlightIdle(Entry);
		return;
	case EAircraftFlightTestStep::Reacquiring:
	case EAircraftFlightTestStep::AttackingClose:
	case EAircraftFlightTestStep::AttackingMoving:
		TickFlightReacquiring(Entry);
		return;
	case EAircraftFlightTestStep::Done:
		FinishAircraftFlight(Entry);
		return;
	}
}

void UAircraftTestSubsystem::TickFlightSettling(FAircraftFlightUnderTest& Entry)
{
	using namespace AircraftFlightTestPrivate;
	const float SpeedRatio = Entry.Aircraft->GetVelocity().Size() / Entry.MaxMoveSpeed;
	if (SpeedRatio < SettleSpeedRatio && GetNow() - Entry.StepStartSeconds < MaxSettleSeconds)
	{
		return;
	}
	Check(SpeedRatio >= SettleSpeedRatio, TEXT("flight_reached_cruise_speed"),
	      FString::Printf(TEXT("speed=%.2f after=%.1fs"), SpeedRatio, GetNow() - Entry.StepStartSeconds));
	Check(Entry.Aircraft->GetLandedState() == EAircraftLandingState::Airborne, TEXT("flight_spawned_airborne"),
	      FString::Printf(TEXT("state=%d"), static_cast<int32>(Entry.Aircraft->GetLandedState())));
	constexpr float RightTurnYaw = 90.f;
	constexpr float RightTurnDistance = 7000.f;
	OrderFlightTurn(Entry, RightTurnYaw, RightTurnDistance, EAircraftFlightTestStep::TurnRight);
}

void UAircraftTestSubsystem::OrderFlightTurn(FAircraftFlightUnderTest& Entry, const float YawOffsetDeg,
                                             const float Distance, const EAircraftFlightTestStep NextStep)
{
	using namespace AircraftFlightTestPrivate;
	AAircraftMaster& Aircraft = *Entry.Aircraft;
	const FVector Direction = GetFlatDirection(Aircraft).RotateAngleAxis(YawOffsetDeg, FVector::UpVector);
	const FVector Target = Aircraft.GetActorLocation() + Direction * Distance;
	const ECommandQueueError Error = Aircraft.MoveToLocation(Target, true, FRotator::ZeroRotator);
	Check(Error == ECommandQueueError::NoError, FString::Printf(TEXT("flight_%s_order_accepted"), GetStepName(NextStep)),
	      FString::Printf(TEXT("error=%d"), static_cast<int32>(Error)));
	EnterFlightStep(Entry, NextStep);
}

void UAircraftTestSubsystem::TickFlightTurning(FAircraftFlightUnderTest& Entry)
{
	using namespace AircraftFlightTestPrivate;
	const float Waited = GetNow() - Entry.StepStartSeconds;
	// A short grace so the order has turned into flight before "idle" counts as arrived.
	constexpr float MinStepSeconds = 1.f;
	const bool bArrived = Waited > MinStepSeconds && Entry.Aircraft->GetIsIdleAndAirborne();
	if (not bArrived && Waited < TurnTimeoutSeconds)
	{
		return;
	}
	Check(bArrived, FString::Printf(TEXT("flight_%s_arrived"), GetStepName(Entry.Step)),
	      FString::Printf(TEXT("after=%.1fs"), Waited));
	if (Entry.Step == EAircraftFlightTestStep::TurnRight)
	{
		constexpr float BackTurnYaw = 180.f;
		constexpr float BackTurnDistance = 6000.f;
		OrderFlightTurn(Entry, BackTurnYaw, BackTurnDistance, EAircraftFlightTestStep::TurnBack);
		return;
	}
	if (Entry.Step == EAircraftFlightTestStep::TurnBack)
	{
		constexpr float SharpTurnYaw = -135.f;
		constexpr float SharpTurnDistance = 4000.f;
		OrderFlightTurn(Entry, SharpTurnYaw, SharpTurnDistance, EAircraftFlightTestStep::TurnSharpLeft);
		return;
	}
	constexpr float AttackTargetYaw = 40.f;
	Entry.Target = SpawnFlightTarget(Entry, AttackTargetYaw, TargetDistance);
	Check(Entry.Target.IsValid(), TEXT("flight_attack_target_spawned"), TargetClassPath);
	if (not Entry.Target.IsValid())
	{
		EnterFlightStep(Entry, EAircraftFlightTestStep::Done);
		return;
	}
	const ECommandQueueError Error = Entry.Aircraft->AttackActor(Entry.Target.Get(), true);
	Check(Error == ECommandQueueError::NoError, TEXT("flight_attack_order_accepted"),
	      FString::Printf(TEXT("error=%d"), static_cast<int32>(Error)));
	EnterFlightStep(Entry, EAircraftFlightTestStep::Attacking);
}

AActor* UAircraftTestSubsystem::SpawnFlightTarget(const FAircraftFlightUnderTest& Entry, const float YawOffsetDeg,
                                                  const float Distance)
{
	using namespace AircraftFlightTestPrivate;
	const AAircraftMaster& Aircraft = *Entry.Aircraft;
	const FVector Direction = GetFlatDirection(Aircraft).RotateAngleAxis(YawOffsetDeg, FVector::UpVector);
	FVector Ground;
	if (not GetGroundAt(FVector2D(Aircraft.GetActorLocation() + Direction * Distance), Ground))
	{
		return nullptr;
	}
	AActor* Target = SpawnUnitForPlayer(TargetClassPath, FTransform(Direction.Rotation(), Ground), EnemyPlayer);
	if (UHealthComponent* Health = IsValid(Target) ? Target->FindComponentByClass<UHealthComponent>() : nullptr)
	{
		Health->SetMaxHealth(TargetHealth);
		Health->SetCurrentHealth(TargetHealth);
	}
	return Target;
}

void UAircraftTestSubsystem::TickFlightAttacking(FAircraftFlightUnderTest& Entry)
{
	using namespace AircraftFlightTestPrivate;
	const bool bIsDiving = Entry.Aircraft->GetIsFlyingAttackDive();
	Entry.bSawDive = Entry.bSawDive || bIsDiving;
	const float Waited = GetNow() - Entry.StepStartSeconds;
	if (Entry.bSawDive && not bIsDiving && Entry.Target.IsValid())
	{
		// The first dive is over: without its target the attack ends at the end of the path.
		Entry.Target->Destroy();
		Entry.Target.Reset();
	}
	if (Entry.Target.IsValid())
	{
		Entry.ClosestTargetDistance = FMath::Min(Entry.ClosestTargetDistance,
		                                         FVector::Dist2D(Entry.Aircraft->GetActorLocation(),
		                                                         Entry.Target->GetActorLocation()));
		Entry.FarthestTargetDistance = FMath::Max(Entry.FarthestTargetDistance,
		                                          FVector::Dist2D(Entry.Aircraft->GetActorLocation(),
		                                                          Entry.Target->GetActorLocation()));
	}
	if (Entry.Aircraft->GetIsIdleAndAirborne() && not Entry.bSawIdle)
	{
		Entry.bSawIdle = true;
		UE_LOG(LogRTSAircraftFlightTest, Display,
		       TEXT("RTS_AIRCRAFT_TEST FLIGHT_ATTACK_IDLE at=%.1f dived=%d target_alive=%d target_dist=%.0f..%.0f"),
		       Waited, Entry.bSawDive ? 1 : 0, Entry.Target.IsValid() ? 1 : 0, Entry.ClosestTargetDistance,
		       Entry.FarthestTargetDistance);
	}
	// Over at the first idle after the dive; the aggressive aircraft may then find other enemies of the map.
	const bool bAttackOver = Entry.bSawDive && not Entry.Target.IsValid() && Entry.bSawIdle;
	if (not bAttackOver && Waited < AttackTimeoutSeconds)
	{
		return;
	}
	Check(Entry.bSawDive, TEXT("flight_attack_dived"), FString::Printf(TEXT("after=%.1fs"), Waited));
	Check(bAttackOver, TEXT("flight_attack_ended_in_idle"), FString::Printf(TEXT("after=%.1fs"), Waited));
	EnterFlightStep(Entry, EAircraftFlightTestStep::IdleAfterAttack);
}

void UAircraftTestSubsystem::TickFlightIdle(FAircraftFlightUnderTest& Entry)
{
	using namespace AircraftFlightTestPrivate;
	if (GetNow() - Entry.StepStartSeconds < IdleSeconds)
	{
		return;
	}
	// Behind and to the side, within the weapon range: the idle aircraft has to find it and turn around for it.
	constexpr float ReacquireTargetYaw = 120.f;
	const float WeaponRange = Entry.Aircraft->GetHighestAircraftWeaponRange();
	const float ReacquireDistance = FMath::Clamp(WeaponRange * ReacquireRangeShare, MinReacquireDistance, TargetDistance);
	UE_LOG(LogRTSAircraftFlightTest, Display, TEXT("RTS_AIRCRAFT_TEST FLIGHT_REACQUIRE weapon_range=%.0f distance=%.0f"),
	       WeaponRange, ReacquireDistance);
	Entry.Target = SpawnFlightTarget(Entry, ReacquireTargetYaw, ReacquireDistance);
	Check(Entry.Target.IsValid(), TEXT("flight_reacquire_target_spawned"), TargetClassPath);
	Entry.bSawDive = false;
	EnterFlightStep(Entry, Entry.Target.IsValid() ? EAircraftFlightTestStep::Reacquiring : EAircraftFlightTestStep::Done);
}

void UAircraftTestSubsystem::TickFlightReacquiring(FAircraftFlightUnderTest& Entry)
{
	using namespace AircraftFlightTestPrivate;
	const bool bIsDiving = Entry.Aircraft->GetIsFlyingAttackDive();
	Entry.bSawDive = Entry.bSawDive || bIsDiving;
	const float Waited = GetNow() - Entry.StepStartSeconds;
	const bool bDiveOver = Entry.bSawDive && not bIsDiving;
	const bool bIsCloseStep = Entry.Step == EAircraftFlightTestStep::AttackingClose;
	const bool bIsMovingStep = Entry.Step == EAircraftFlightTestStep::AttackingMoving;
	const float Timeout = bIsCloseStep || bIsMovingStep ? CloseAttackTimeoutSeconds : ReacquireTimeoutSeconds;
	if (not bDiveOver && Waited < Timeout)
	{
		return;
	}
	Check(Entry.bSawDive, FString::Printf(TEXT("flight_%s_dived"), GetStepName(Entry.Step)),
	      FString::Printf(TEXT("after=%.1fs"), Waited));
	if (bIsMovingStep)
	{
		EnterFlightStep(Entry, EAircraftFlightTestStep::Done);
		return;
	}
	if (bIsCloseStep)
	{
		StartFlightMovingTargetAttack(Entry);
		return;
	}
	if (Entry.Target.IsValid())
	{
		Entry.Target->Destroy();
	}
	// Straight ahead and close: no dive is possible from here, the aircraft must fly out and come back.
	Entry.Target = SpawnFlightTarget(Entry, 0.f, CloseTargetDistance);
	Check(Entry.Target.IsValid(), TEXT("flight_close_target_spawned"), TargetClassPath);
	if (not Entry.Target.IsValid())
	{
		EnterFlightStep(Entry, EAircraftFlightTestStep::Done);
		return;
	}
	const ECommandQueueError Error = Entry.Aircraft->AttackActor(Entry.Target.Get(), true);
	Check(Error == ECommandQueueError::NoError, TEXT("flight_close_attack_order_accepted"),
	      FString::Printf(TEXT("error=%d"), static_cast<int32>(Error)));
	Entry.bSawDive = false;
	EnterFlightStep(Entry, EAircraftFlightTestStep::AttackingClose);
}

void UAircraftTestSubsystem::StartFlightMovingTargetAttack(FAircraftFlightUnderTest& Entry)
{
	using namespace AircraftFlightTestPrivate;
	if (Entry.Target.IsValid())
	{
		Entry.Target->Destroy();
	}
	Entry.Target = SpawnFlightTarget(Entry, MovingTargetYaw, MovingTargetDistance);
	Check(Entry.Target.IsValid(), TEXT("flight_moving_target_spawned"), TargetClassPath);
	if (not Entry.Target.IsValid())
	{
		EnterFlightStep(Entry, EAircraftFlightTestStep::Done);
		return;
	}
	// Drives to the left across the aircraft's flight.
	const FVector Across = AircraftFlightTestPrivate::GetFlatDirection(*Entry.Aircraft)
		.RotateAngleAxis(-90.f, FVector::UpVector);
	FVector DriveTo;
	const bool bHasDriveTo = GetGroundAt(FVector2D(Entry.Target->GetActorLocation() + Across * MovingTargetDriveDistance),
	                                     DriveTo);
	ICommands* TargetCommands = Cast<ICommands>(Entry.Target.Get());
	const ECommandQueueError DriveError = bHasDriveTo && TargetCommands
		                                      ? TargetCommands->MoveToLocation(DriveTo, true, Across.Rotation())
		                                      : ECommandQueueError::CommandDataInvalid;
	Check(DriveError == ECommandQueueError::NoError, TEXT("flight_moving_target_drives"),
	      FString::Printf(TEXT("error=%d"), static_cast<int32>(DriveError)));
	const ECommandQueueError Error = Entry.Aircraft->AttackActor(Entry.Target.Get(), true);
	Check(Error == ECommandQueueError::NoError, TEXT("flight_moving_attack_order_accepted"),
	      FString::Printf(TEXT("error=%d"), static_cast<int32>(Error)));
	Entry.bSawDive = false;
	EnterFlightStep(Entry, EAircraftFlightTestStep::AttackingMoving);
}

void UAircraftTestSubsystem::RecordFlightSample(FAircraftFlightUnderTest& Entry)
{
	using namespace AircraftFlightTestPrivate;
	const UWorld* World = GetWorld();
	const float DeltaSeconds = IsValid(World) ? World->GetDeltaSeconds() : 0.f;
	if (DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const AAircraftMaster& Aircraft = *Entry.Aircraft;
	const FRotator Rotation = Aircraft.GetActorRotation();
	const FVector Location = Aircraft.GetActorLocation();
	const float SpeedRatio = Aircraft.GetVelocity().Size2D() / Entry.MaxMoveSpeed;
	float GroundZ = 0.f;
	const bool bHasGround = GetLandscapeZ(FVector2D(Location), GroundZ);

	float YawRate = 0.f;
	float YawAcceleration = 0.f;
	float RollRate = 0.f;
	if (Entry.bHasLastSample)
	{
		YawRate = FMath::FindDeltaAngleDegrees(Entry.LastYaw, Rotation.Yaw) / DeltaSeconds;
		RollRate = FMath::FindDeltaAngleDegrees(Entry.LastRoll, Rotation.Roll) / DeltaSeconds;
		if (Entry.bHasLastYawRate && DeltaSeconds >= MinRateDeltaSeconds)
		{
			YawAcceleration = (YawRate - Entry.LastYawRate) / DeltaSeconds;
		}
		Entry.LastYawRate = YawRate;
		// A hitch frame's yaw rate is an average over the hitch: the next frame's change against it is no snap.
		Entry.bHasLastYawRate = DeltaSeconds >= MinRateDeltaSeconds && DeltaSeconds <= MaxJudgedFrameSeconds;
	}
	Entry.LastYaw = Rotation.Yaw;
	Entry.LastRoll = Rotation.Roll;
	Entry.bHasLastSample = true;

	const FVector Nose = Aircraft.GetActorForwardVector().GetSafeNormal2D();
	const FVector Flight = Aircraft.GetVelocity().GetSafeNormal2D();
	const float SlipDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Nose, Flight), -1.f, 1.f)));
	const bool bIsMoving = SpeedRatio >= MinMovingSpeedRatio && not Flight.IsNearlyZero()
		&& DeltaSeconds <= MaxJudgedFrameSeconds;

	FAircraftFlightStepMetrics& Metrics = Entry.StepMetrics[static_cast<int32>(Entry.Step)];
	++Metrics.Frames;
	Metrics.SpeedRatioSum += SpeedRatio;
	// The first second of a step still carries the frame the order arrived in.
	constexpr float IgnoreSpeedSeconds = 0.25f;
	if (GetNow() - Entry.StepStartSeconds >= IgnoreSpeedSeconds && Entry.Step != EAircraftFlightTestStep::Settling)
	{
		Metrics.LowestSpeedRatio = FMath::Min(Metrics.LowestSpeedRatio, SpeedRatio);
		Metrics.HighestSpeedRatio = FMath::Max(Metrics.HighestSpeedRatio, SpeedRatio);
		Metrics.LargestSpeedDrop = FMath::Max(Metrics.LargestSpeedDrop, Metrics.HighestSpeedRatio - SpeedRatio);
	}
	if (bIsMoving)
	{
		Metrics.LargestSlipDeg = FMath::Max(Metrics.LargestSlipDeg, SlipDeg);
		Metrics.LargestYawAccelerationDeg = FMath::Max(Metrics.LargestYawAccelerationDeg, FMath::Abs(YawAcceleration));
		Metrics.LargestYawRateDeg = FMath::Max(Metrics.LargestYawRateDeg, FMath::Abs(YawRate));
		Metrics.LargestRollRateDeg = FMath::Max(Metrics.LargestRollRateDeg, FMath::Abs(RollRate));
		Metrics.LargestRightRollDeg = FMath::Max(Metrics.LargestRightRollDeg, Rotation.Roll);
		Metrics.LargestLeftRollDeg = FMath::Max(Metrics.LargestLeftRollDeg, -Rotation.Roll);
	}
	if (bHasGround)
	{
		Metrics.LowestAboveGround = FMath::Min(Metrics.LowestAboveGround, Location.Z - GroundZ);
	}
	const bool bIsDiving = Aircraft.GetIsFlyingAttackDive();
	if (bIsDiving && not Entry.bWasDiving)
	{
		if (Metrics.DiveCount == 0)
		{
			Metrics.SecondsToFirstDive = GetNow() - Entry.StepStartSeconds;
		}
		++Metrics.DiveCount;
		Entry.DiveStartYaw = Rotation.Yaw;
		Metrics.LargestDiveStartYawRateDeg = FMath::Max(Metrics.LargestDiveStartYawRateDeg, FMath::Abs(YawRate));
	}
	if (bIsDiving)
	{
		Metrics.LargestDiveHeadingSweepDeg = FMath::Max(
			Metrics.LargestDiveHeadingSweepDeg, FMath::Abs(FMath::FindDeltaAngleDegrees(Entry.DiveStartYaw, Rotation.Yaw)));
	}
	Entry.bWasDiving = bIsDiving;
	Entry.CsvRows.Add(FString::Printf(TEXT("%.3f,%s,%.0f,%.0f,%.0f,%.0f,%.3f,%.2f,%.2f,%.2f,%.2f,%.1f,%.2f,%d,%d"),
	                                  GetNow(), GetStepName(Entry.Step), Location.X, Location.Y, Location.Z,
	                                  bHasGround ? GroundZ : 0.f, SpeedRatio, Rotation.Yaw, Rotation.Pitch,
	                                  Rotation.Roll, YawRate, YawAcceleration, SlipDeg,
	                                  static_cast<int32>(Aircraft.GetMovementStateForTests()),
	                                  Aircraft.GetIsFlyingAttackDive() ? 1 : 0));
}

void UAircraftTestSubsystem::EnterFlightStep(FAircraftFlightUnderTest& Entry, const EAircraftFlightTestStep NewStep)
{
	using namespace AircraftFlightTestPrivate;
	Entry.Step = NewStep;
	Entry.StepStartSeconds = GetNow();
	UE_LOG(LogRTSAircraftFlightTest, Display, TEXT("RTS_AIRCRAFT_TEST FLIGHT_STEP %s at=%.1f %s"), GetStepName(NewStep),
	       Entry.StepStartSeconds, Entry.Aircraft.IsValid() ? *Entry.Aircraft->DescribeFlightForTests() : TEXT(""));
}

void UAircraftTestSubsystem::FinishAircraftFlight(FAircraftFlightUnderTest& Entry)
{
	using namespace AircraftFlightTestPrivate;
	for (int32 StepIndex = 0; StepIndex < Entry.StepMetrics.Num(); ++StepIndex)
	{
		const FAircraftFlightStepMetrics& Metrics = Entry.StepMetrics[StepIndex];
		if (Metrics.Frames == 0)
		{
			continue;
		}
		const EAircraftFlightTestStep Step = static_cast<EAircraftFlightTestStep>(StepIndex);
		UE_LOG(LogRTSAircraftFlightTest, Display,
		       TEXT("RTS_AIRCRAFT_TEST FLIGHT_METRICS step=%s frames=%d min_speed=%.2f speed_drop=%.2f avg_speed=%.2f "
			       "max_slip=%.1f max_yaw_rate=%.1f max_yaw_accel=%.0f max_roll_rate=%.1f bank_right=%.0f bank_left=%.0f "
			       "min_above_ground=%.0f dives=%d dive_start_yaw_rate=%.1f dive_sweep=%.1f first_dive_after=%.1f"),
		       GetStepName(Step), Metrics.Frames, Metrics.LowestSpeedRatio, Metrics.LargestSpeedDrop,
		       Metrics.SpeedRatioSum / Metrics.Frames, Metrics.LargestSlipDeg, Metrics.LargestYawRateDeg,
		       Metrics.LargestYawAccelerationDeg, Metrics.LargestRollRateDeg, Metrics.LargestRightRollDeg,
		       Metrics.LargestLeftRollDeg, Metrics.LowestAboveGround, Metrics.DiveCount,
		       Metrics.LargestDiveStartYawRateDeg, Metrics.LargestDiveHeadingSweepDeg, Metrics.SecondsToFirstDive);
		if (Step == EAircraftFlightTestStep::Settling || Step == EAircraftFlightTestStep::Done)
		{
			continue;
		}
		const bool bIsIdleStep = Step == EAircraftFlightTestStep::IdleAfterAttack;
		if (bIsIdleStep)
		{
			Check(Metrics.LowestSpeedRatio >= MinIdleSpeedRatio,
			      FString::Printf(TEXT("flight_%s_keeps_speed"), GetStepName(Step)),
			      FString::Printf(TEXT("min=%.2f wanted>=%.2f"), Metrics.LowestSpeedRatio, MinIdleSpeedRatio));
		}
		else
		{
			Check(Metrics.LargestSpeedDrop <= MaxSpeedDropRatio,
			      FString::Printf(TEXT("flight_%s_keeps_speed"), GetStepName(Step)),
			      FString::Printf(TEXT("drop=%.2f wanted<=%.2f min=%.2f"), Metrics.LargestSpeedDrop, MaxSpeedDropRatio,
			                      Metrics.LowestSpeedRatio));
		}
		Check(Metrics.LargestSlipDeg <= MaxSlipDeg, FString::Printf(TEXT("flight_%s_nose_follows_flight"), GetStepName(Step)),
		      FString::Printf(TEXT("max_slip=%.1f wanted<=%.1f"), Metrics.LargestSlipDeg, MaxSlipDeg));
		Check(Metrics.LargestYawAccelerationDeg <= MaxYawAccelerationDeg,
		      FString::Printf(TEXT("flight_%s_no_turn_snaps"), GetStepName(Step)),
		      FString::Printf(TEXT("max_yaw_accel=%.0f wanted<=%.0f"), Metrics.LargestYawAccelerationDeg,
		                      MaxYawAccelerationDeg));
		Check(Metrics.LargestRollRateDeg <= MaxRollRateDeg, FString::Printf(TEXT("flight_%s_smooth_bank"), GetStepName(Step)),
		      FString::Printf(TEXT("max_roll_rate=%.0f wanted<=%.0f"), Metrics.LargestRollRateDeg, MaxRollRateDeg));
		const bool bIsTurnOrder = Step == EAircraftFlightTestStep::TurnRight || Step == EAircraftFlightTestStep::TurnBack
			|| Step == EAircraftFlightTestStep::TurnSharpLeft;
		if (bIsTurnOrder)
		{
			const float CounterBank = FMath::Min(Metrics.LargestRightRollDeg, Metrics.LargestLeftRollDeg);
			Check(CounterBank <= MaxCounterBankDeg, FString::Printf(TEXT("flight_%s_one_turn_no_s_bend"), GetStepName(Step)),
			      FString::Printf(TEXT("bank_right=%.0f bank_left=%.0f wanted min<=%.0f"), Metrics.LargestRightRollDeg,
			                      Metrics.LargestLeftRollDeg, MaxCounterBankDeg));
		}
		const bool bHasDive = Step == EAircraftFlightTestStep::Attacking || Step == EAircraftFlightTestStep::Reacquiring
			|| Step == EAircraftFlightTestStep::AttackingClose || Step == EAircraftFlightTestStep::AttackingMoving;
		if (bHasDive)
		{
			Check(Metrics.SecondsToFirstDive >= 0.f && Metrics.SecondsToFirstDive <= MaxSecondsToFirstDive,
			      FString::Printf(TEXT("flight_%s_dives_soon"), GetStepName(Step)),
			      FString::Printf(TEXT("first_dive_after=%.1f wanted<=%.1f"), Metrics.SecondsToFirstDive,
			                      MaxSecondsToFirstDive));
			Check(Metrics.LargestDiveStartYawRateDeg <= MaxDiveStartYawRateDeg,
			      FString::Printf(TEXT("flight_%s_dive_starts_aligned"), GetStepName(Step)),
			      FString::Printf(TEXT("yaw_rate_at_dive_start=%.1f wanted<=%.1f"), Metrics.LargestDiveStartYawRateDeg,
			                      MaxDiveStartYawRateDeg));
			Check(Metrics.LargestDiveHeadingSweepDeg <= MaxDiveHeadingSweepDeg,
			      FString::Printf(TEXT("flight_%s_dive_holds_heading"), GetStepName(Step)),
			      FString::Printf(TEXT("sweep=%.1f wanted<=%.1f"), Metrics.LargestDiveHeadingSweepDeg,
			                      MaxDiveHeadingSweepDeg));
			Check(Metrics.LowestAboveGround >= MinDiveAboveGround,
			      FString::Printf(TEXT("flight_%s_dive_clear_of_ground"), GetStepName(Step)),
			      FString::Printf(TEXT("min=%.0f wanted>=%.0f"), Metrics.LowestAboveGround, MinDiveAboveGround));
		}
	}
	FString Label = TEXT("flight");
	FParse::Value(FCommandLine::Get(), TEXT("AircraftFlightQALabel="), Label);
	const FString CsvPath = FPaths::ProjectSavedDir() / TEXT("AircraftFlightQA") / (Label + TEXT(".csv"));
	const bool bWritten = FFileHelper::SaveStringArrayToFile(Entry.CsvRows, *CsvPath);
	UE_LOG(LogRTSAircraftFlightTest, Display, TEXT("RTS_AIRCRAFT_TEST FLIGHT_CSV %s written=%d rows=%d"),
	       *FPaths::ConvertRelativePathToFull(CsvPath), bWritten ? 1 : 0, Entry.CsvRows.Num());
	Entry.Step = EAircraftFlightTestStep::Done;
	Finish();
}

#endif
