// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.
#include "RTSShortsDirector.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSOptimizer.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"

#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponState/TeamWeaponState.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShortsTeamWeapons, Log, All);

namespace RTSShortsTeamWeaponsPrivate
{
	AActor* FindNearestDeploymentTarget(const AActor* Crew, const TArray<TWeakObjectPtr<AActor>>& Candidates)
	{
		AActor* Nearest = nullptr;
		float NearestDistance = TNumericLimits<float>::Max();
		for (const TWeakObjectPtr<AActor>& Candidate : Candidates)
		{
			AActor* Target = Candidate.Get();
			if (not IsValid(Target))
			{
				continue;
			}
			const float Distance = FVector::DistSquared(Crew->GetActorLocation(), Target->GetActorLocation());
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				Nearest = Target;
			}
		}
		return Nearest;
	}

	void KeepCaptureActorUpdating(AActor* Actor)
	{
		if (not IsValid(Actor))
		{
			return;
		}
		// The cinematic camera is separate from the RTS camera used by the distance optimizer.
		if (URTSOptimizer* Optimizer = Actor->FindComponentByClass<URTSOptimizer>())
		{
			Optimizer->SetOptimizationEnabled(false);
		}
	}
}

bool ARTSShortsDirector::GetShouldPreRollTeamWeaponIntro() const
{
	return M_Job.Segments.IsValidIndex(0)
		&& M_Job.Segments[0].TeamWeapons.Shot == ERTSShortsTeamWeaponShot::TowConvoy;
}

void ARTSShortsDirector::BeginTeamWeaponShot(const int32 SegmentIndex)
{
	const FRTSShortsTeamWeaponSettings& Settings = M_Job.Segments[SegmentIndex].TeamWeapons;
	if (Settings.Shot == ERTSShortsTeamWeaponShot::None)
	{
		return;
	}
	const TArray<TWeakObjectPtr<AActor>> Vehicles = Settings.Shot == ERTSShortsTeamWeaponShot::TowConvoy
		? M_Convoy.Units : M_Battle.PlayerUnits;
	if (Vehicles.Num() != Settings.SquadClassPaths.Num())
	{
		RTSFunctionLibrary::ReportError(TEXT("Team weapons short requires one crew class per transport."));
		MarkSegmentSkipped(SegmentIndex, TEXT("team weapon/transport count mismatch"));
		return;
	}
	M_TeamWeaponPairs.Reset();
	for (int32 PairIndex = 0; PairIndex < Vehicles.Num(); ++PairIndex)
	{
		SpawnTeamWeaponPair(Vehicles[PairIndex].Get(), Settings.SquadClassPaths[PairIndex], Settings);
	}
	UE_LOG(LogRTSShortsTeamWeapons, Display, TEXT("RTS_SHORTS_TEAM_WEAPONS_BEGIN segment=%d shot=%d pairs=%d frame=%d"),
		SegmentIndex, static_cast<int32>(Settings.Shot), M_TeamWeaponPairs.Num(), GetTimelineFrame());
}

void ARTSShortsDirector::SpawnTeamWeaponPair(AActor* VehicleActor, const FString& SquadClassPath,
	const FRTSShortsTeamWeaponSettings& Settings)
{
	ATankMaster* Vehicle = Cast<ATankMaster>(VehicleActor);
	if (not IsValid(Vehicle))
	{
		RTSFunctionLibrary::ReportError(TEXT("Team weapons short transport must derive from TankMaster."));
		return;
	}
	const bool bConvoy = Settings.Shot == ERTSShortsTeamWeaponShot::TowConvoy;
	constexpr float TowSpawnDistanceCm = 100.f;
	const FVector Offset = bConvoy ? FVector(-TowSpawnDistanceCm, 0.f, 0.f) : Settings.DeploymentOffset;
	const FRotator Rotation(0.f, Vehicle->GetActorRotation().Yaw, 0.f);
	const FVector Location = GetBattleGroundPoint(Vehicle->GetActorLocation() + Rotation.RotateVector(Offset));
	AActor* Spawned = SpawnBattleParticipant(SquadClassPath, FTransform(Rotation, Location),
		static_cast<uint8>(M_Job.UnitOwningPlayer));
	ATeamWeaponController* Controller = Cast<ATeamWeaponController>(Spawned);
	if (not IsValid(Controller))
	{
		RTSFunctionLibrary::ReportError(TEXT("Team weapons short crew must derive from TeamWeaponController."));
		return;
	}
	FRTSShortsTowPair Pair;
	Pair.Vehicle = Vehicle;
	Pair.Controller = Controller;
	Pair.VehicleStartLocation = Vehicle->GetActorLocation();
	M_TeamWeaponPairs.Add(Pair);
	if (not bConvoy)
	{
		M_Battle.PlayerSquads.AddUnique(Controller);
	}
	RTSShortsTeamWeaponsPrivate::KeepCaptureActorUpdating(Vehicle);
	RTSShortsTeamWeaponsPrivate::KeepCaptureActorUpdating(Controller);
	ForceAnimationTicking(Controller);
	UE_LOG(LogRTSShortsTeamWeapons, Display, TEXT("RTS_SHORTS_TEAM_WEAPON_PAIR vehicle=%s crew=%s class=%s location=%s"),
		*Vehicle->GetName(), *Controller->GetName(), *SquadClassPath, *Location.ToString());
}

void ARTSShortsDirector::TickTeamWeaponShot(const int32 Frame)
{
	if (not M_Job.Segments.IsValidIndex(M_CurrentSegment) || M_TeamWeaponPairs.IsEmpty())
	{
		return;
	}
	for (FRTSShortsTowPair& Pair : M_TeamWeaponPairs)
	{
		TickTeamWeaponPair(Pair, Frame);
	}
	const FRTSShortsTeamWeaponSettings& Settings = M_Job.Segments[M_CurrentSegment].TeamWeapons;
	if (Settings.Shot == ERTSShortsTeamWeaponShot::TowConvoy)
	{
		StartPreparedTeamWeaponConvoy(Frame);
	}
}

void ARTSShortsDirector::TickTeamWeaponPair(FRTSShortsTowPair& Pair, const int32 Frame)
{
	ATeamWeaponController* Controller = Pair.Controller.Get();
	ATankMaster* Vehicle = Pair.Vehicle.Get();
	if (not IsValid(Controller))
	{
		return;
	}
	const ERTSShortsTeamWeaponShot Shot = M_Job.Segments[M_CurrentSegment].TeamWeapons.Shot;
	if (Pair.Phase == ERTSShortsTowPairPhase::WaitingForCrew)
	{
		if ((Shot == ERTSShortsTeamWeaponShot::TowConvoy && not IsValid(Vehicle))
			|| not Controller->GetIsSquadFullyLoaded() || not Controller->GetHasControlledTeamWeapon())
		{
			return;
		}
		const ETeamWeaponState State = Controller->GetTeamWeaponState();
		if (Shot == ERTSShortsTeamWeaponShot::DeployBattle
		    && (State == ETeamWeaponState::Spawning || State == ETeamWeaponState::Uninitialized))
		{
			return;
		}
		Pair.Weapon = Controller->GetControlledTeamWeaponNoReport();
		PrepareTeamWeaponPair(Pair, Shot, Frame);
	}
	ATeamWeapon* Weapon = Pair.Weapon.Get();
	if (not IsValid(Weapon))
	{
		return;
	}
	if (Pair.Phase == ERTSShortsTowPairPhase::HookRequested && IsValid(Vehicle)
		&& Weapon->GetAttachParentActor() == Vehicle)
	{
		// Packed guns stay attached through native towing; their collision must not pin the transport.
		Weapon->SetActorEnableCollision(false);
		Pair.Phase = ERTSShortsTowPairPhase::Towing;
		UE_LOG(LogRTSShortsTeamWeapons, Display, TEXT("RTS_SHORTS_TEAM_WEAPON_TOWED frame=%d vehicle=%s weapon=%s crew=%s"),
			Frame, *Vehicle->GetName(), *Weapon->GetName(), *Controller->GetName());
	}
	if (Pair.Phase == ERTSShortsTowPairPhase::Deploying && Controller->GetIsReadyDeployed())
	{
		Pair.Phase = ERTSShortsTowPairPhase::Ready;
		M_Battle.OrderedTargets.Remove(Controller);
		UE_LOG(LogRTSShortsTeamWeapons, Display, TEXT("RTS_SHORTS_TEAM_WEAPON_DEPLOYED frame=%d weapon=%s crew=%s location=%s"),
			Frame, *Weapon->GetName(), *Controller->GetName(), *Weapon->GetActorLocation().ToString());
	}
}

void ARTSShortsDirector::PrepareTeamWeaponPair(FRTSShortsTowPair& Pair, const ERTSShortsTeamWeaponShot Shot,
	const int32 Frame)
{
	ATeamWeaponController* Controller = Pair.Controller.Get();
	ATankMaster* Vehicle = Pair.Vehicle.Get();
	ATeamWeapon* Weapon = Pair.Weapon.Get();
	if (not IsValid(Controller) || not IsValid(Weapon)
		|| (Shot == ERTSShortsTeamWeaponShot::TowConvoy && not IsValid(Vehicle)))
	{
		return;
	}
	RTSShortsTeamWeaponsPrivate::KeepCaptureActorUpdating(Weapon);
	ForceAnimationTicking(Weapon);
	if (UFowComp* Fog = Weapon->FindComponentByClass<UFowComp>())
	{
		Fog->StopFowParticipation();
	}
	if (Shot == ERTSShortsTeamWeaponShot::TowConvoy)
	{
		const ECommandQueueError Result = Vehicle->TowActor(Weapon, ETowedActorTarget::TowTeamWeapon, true);
		if (Result != ECommandQueueError::NoError)
		{
			RTSFunctionLibrary::ReportError(TEXT("Team weapons short could not request native towing."));
			return;
		}
		Pair.Phase = ERTSShortsTowPairPhase::HookRequested;
		return;
	}
	AActor* Target = RTSShortsTeamWeaponsPrivate::FindNearestDeploymentTarget(Controller, M_Battle.EnemyUnits);
	if (not IsValid(Target))
	{
		return;
	}
	M_Battle.PlayerSquads.AddUnique(Controller);
	// The normal attack command stages native deployment and the crew's trail-handling montages.
	Controller->AttackActor(Target, true);
	Pair.Phase = ERTSShortsTowPairPhase::Deploying;
	UE_LOG(LogRTSShortsTeamWeapons, Display, TEXT("RTS_SHORTS_TEAM_WEAPON_DEPLOY_BEGIN frame=%d weapon=%s crew=%s vehicle=%s state=%d"),
		Frame, *Weapon->GetName(), *Controller->GetName(), *GetNameSafe(Vehicle), static_cast<int32>(Controller->GetTeamWeaponState()));
}

void ARTSShortsDirector::StartPreparedTeamWeaponConvoy(const int32 Frame)
{
	if (M_Convoy.bDriving)
	{
		LogTeamWeaponConvoyMotion(Frame);
		return;
	}
	constexpr float ConvoyRunInSeconds = 2.f;
	const int32 DriveBeginFrame = M_SegmentStartFrames[M_CurrentSegment]
		- FMath::RoundToInt(ConvoyRunInSeconds * M_Job.Fps);
	if (Frame < DriveBeginFrame)
	{
		return;
	}
	for (const FRTSShortsTowPair& Pair : M_TeamWeaponPairs)
	{
		if (Pair.Phase != ERTSShortsTowPairPhase::Towing)
		{
			return;
		}
	}
	OrderConvoyDrive(M_CurrentSegment);
	bM_FireOrdered = true;
	M_Manifest.Segments[M_CurrentSegment].FireOrderFrame = Frame;
	UE_LOG(LogRTSShortsTeamWeapons, Display, TEXT("RTS_SHORTS_TEAM_WEAPONS_CONVOY_READY frame=%d visibleStart=%d pairs=%d"),
		Frame, M_SegmentStartFrames[M_CurrentSegment], M_TeamWeaponPairs.Num());
}

void ARTSShortsDirector::LogTeamWeaponConvoyMotion(const int32 Frame) const
{
	constexpr int32 LogIntervalFrames = 60;
	if (Frame % LogIntervalFrames != 0)
	{
		return;
	}
	for (const FRTSShortsTowPair& Pair : M_TeamWeaponPairs)
	{
		const ATankMaster* Vehicle = Pair.Vehicle.Get();
		const ATeamWeapon* Weapon = Pair.Weapon.Get();
		if (not IsValid(Vehicle) || not IsValid(Weapon))
		{
			continue;
		}
		const AAIController* Controller = Cast<AAIController>(Vehicle->GetController());
		const ATrackedTankMaster* TrackedVehicle = Cast<ATrackedTankMaster>(Vehicle);
		const UTrackPhysicsMovement* Movement = IsValid(TrackedVehicle) ? TrackedVehicle->GetTrackPhysicsMovement() : nullptr;
		const int32 MoveStatus = IsValid(Controller) ? static_cast<int32>(Controller->GetMoveStatus()) : -1;
		const float Throttle = IsValid(Movement) ? Movement->GetLastNoneZeroThrottle() : 0.f;
		UE_LOG(LogRTSShortsTeamWeapons, Display, TEXT("RTS_SHORTS_TEAM_WEAPON_MOTION frame=%d vehicle=%s attached=%d speed=%.1f distance=%.1f location=%s moveStatus=%d throttle=%.2f"),
			Frame, *Vehicle->GetName(), Weapon->GetAttachParentActor() == Vehicle ? 1 : 0,
			Vehicle->GetVelocity().Size(), FVector::Dist2D(Pair.VehicleStartLocation, Vehicle->GetActorLocation()),
			*Vehicle->GetActorLocation().ToString(), MoveStatus, Throttle);
	}
}

bool ARTSShortsDirector::GetShouldHoldTeamWeaponOrder(const AActor* Unit) const
{
	for (const FRTSShortsTowPair& Pair : M_TeamWeaponPairs)
	{
		if (Pair.Controller.Get() == Unit && Pair.Phase != ERTSShortsTowPairPhase::Ready)
		{
			return true;
		}
	}
	return false;
}

void ARTSShortsDirector::DestroyTeamWeaponPair(const FRTSShortsTowPair& Pair)
{
	ATankMaster* Vehicle = Pair.Vehicle.Get();
	ATeamWeaponController* Controller = Pair.Controller.Get();
	ATeamWeapon* Weapon = Pair.Weapon.Get();
	if (IsValid(Vehicle) && IsValid(Weapon) && Weapon->GetAttachParentActor() == Vehicle)
	{
		Vehicle->ExecuteDetachTowCommand();
	}
	TArray<TWeakObjectPtr<ASquadUnit>> Soldiers;
	if (IsValid(Controller))
	{
		for (ASquadUnit* Soldier : Controller->GetSquadUnitsChecked())
		{
			Soldiers.Add(Soldier);
		}
	}
	for (const TWeakObjectPtr<ASquadUnit>& Soldier : Soldiers)
	{
		if (Soldier.IsValid())
		{
			Soldier->Destroy();
		}
	}
	if (IsValid(Weapon))
	{
		Weapon->Destroy();
	}
	Controller = Pair.Controller.Get();
	if (IsValid(Controller))
	{
		Controller->Destroy();
	}
}

void ARTSShortsDirector::EndTeamWeaponShot()
{
	const TArray<FRTSShortsTowPair> Pairs = MoveTemp(M_TeamWeaponPairs);
	for (const FRTSShortsTowPair& Pair : Pairs)
	{
		DestroyTeamWeaponPair(Pair);
	}
}
