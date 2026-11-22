// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyWorkCircuitRunner.h"

#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace EnemyWorkCircuitConstants
{
	constexpr float TickIntervalSeconds = 0.5f;
	// The squad counts as arrived when its soldiers' centre is this close to the point.
	constexpr float ArrivalRadius = 450.f;
	// A squad that went idle this close to its point also counts as arrived (it may stop short in a crowd).
	constexpr float IdleArrivalRadius = 1200.f;
	// Seconds a walk runs before an idle squad may count as arrived (the move order needs a moment to start).
	constexpr float MinimumWalkSeconds = 2.f;
	constexpr float MontageBlendOutSeconds = 0.25f;
}

void UEnemyWorkCircuitRunner::StartCircuit(ASquadController* Squad, const TArray<FVector>& WorldPoints,
                                               const float WorkSeconds, const float MaxWalkSeconds,
                                               UAnimMontage* WorkMontage)
{
	M_Squad = Squad;
	M_Points = WorldPoints;
	M_WorkSeconds = WorkSeconds;
	M_MaxWalkSeconds = MaxWalkSeconds;
	M_WorkMontage = WorkMontage;
	M_PointIndex = 0;
	if (M_Points.IsEmpty() || not GetIsValidSquad())
	{
		return;
	}
	UWorld* World = Squad->GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<UEnemyWorkCircuitRunner> WeakThis(this);
	World->GetTimerManager().SetTimer(M_TickTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->TickCircuit();
		}
	}), EnemyWorkCircuitConstants::TickIntervalSeconds, true);
	StartWalkingToCurrentPoint();
}

void UEnemyWorkCircuitRunner::StopCircuit()
{
	if (M_Phase == EEnemyWorkCircuitPhase::Working)
	{
		StopWorkMontage();
	}
	M_Phase = EEnemyWorkCircuitPhase::Stopped;
	ClearTickTimer();
}

void UEnemyWorkCircuitRunner::BeginDestroy()
{
	ClearTickTimer();
	Super::BeginDestroy();
}

bool UEnemyWorkCircuitRunner::GetIsValidSquad() const
{
	return M_Squad.IsValid() && RTSFunctionLibrary::RTSIsValid(M_Squad.Get());
}

void UEnemyWorkCircuitRunner::TickCircuit()
{
	if (M_Phase == EEnemyWorkCircuitPhase::Stopped)
	{
		return;
	}
	// Dead or fighting: the circuit is over for good; the squad's own combat behaviour takes it from here.
	if (not GetIsValidSquad() || M_Squad->GetIsUnitInCombat())
	{
		StopCircuit();
		return;
	}
	M_PhaseSeconds += EnemyWorkCircuitConstants::TickIntervalSeconds;
	if (M_Phase == EEnemyWorkCircuitPhase::Walking)
	{
		if (GetHasSquadArrived() || M_PhaseSeconds >= M_MaxWalkSeconds)
		{
			StartWorking();
		}
		return;
	}
	if (M_PhaseSeconds >= M_WorkSeconds)
	{
		FinishWorkingAtPoint();
		return;
	}
	LoopWorkMontage();
}

void UEnemyWorkCircuitRunner::StartWalkingToCurrentPoint()
{
	if (not GetIsValidSquad() || not M_Points.IsValidIndex(M_PointIndex))
	{
		StopCircuit();
		return;
	}
	M_Phase = EEnemyWorkCircuitPhase::Walking;
	M_PhaseSeconds = 0.f;
	ICommands* Commands = Cast<ICommands>(M_Squad.Get());
	if (Commands == nullptr)
	{
		StopCircuit();
		return;
	}
	Commands->MoveToLocation(M_Points[M_PointIndex], true, FRotator::ZeroRotator);
}

void UEnemyWorkCircuitRunner::StartWorking()
{
	M_Phase = EEnemyWorkCircuitPhase::Working;
	M_PhaseSeconds = 0.f;
	LoopWorkMontage();
}

void UEnemyWorkCircuitRunner::FinishWorkingAtPoint()
{
	StopWorkMontage();
	M_PointIndex = (M_PointIndex + 1) % M_Points.Num();
	StartWalkingToCurrentPoint();
}

bool UEnemyWorkCircuitRunner::GetHasSquadArrived() const
{
	using namespace EnemyWorkCircuitConstants;
	if (not M_Points.IsValidIndex(M_PointIndex))
	{
		return true;
	}
	const float Distance = FVector::Dist2D(GetSquadCentre(), M_Points[M_PointIndex]);
	if (Distance <= ArrivalRadius)
	{
		return true;
	}
	ICommands* Commands = Cast<ICommands>(M_Squad.Get());
	return Commands != nullptr && M_PhaseSeconds >= MinimumWalkSeconds && Distance <= IdleArrivalRadius
		&& Commands->GetIsUnitIdle();
}

FVector UEnemyWorkCircuitRunner::GetSquadCentre() const
{
	const TArray<ASquadUnit*> Units = GetSquadUnits();
	if (Units.IsEmpty())
	{
		return M_Squad.IsValid() ? M_Squad->GetActorLocation() : FVector::ZeroVector;
	}
	FVector Sum = FVector::ZeroVector;
	for (const ASquadUnit* Unit : Units)
	{
		Sum += Unit->GetActorLocation();
	}
	return Sum / Units.Num();
}

TArray<ASquadUnit*> UEnemyWorkCircuitRunner::GetSquadUnits() const
{
	TArray<ASquadUnit*> AliveUnits;
	if (not M_Squad.IsValid())
	{
		return AliveUnits;
	}
	for (ASquadUnit* Unit : M_Squad->GetSquadUnitsChecked())
	{
		if (RTSFunctionLibrary::RTSIsValid(Unit))
		{
			AliveUnits.Add(Unit);
		}
	}
	return AliveUnits;
}

void UEnemyWorkCircuitRunner::LoopWorkMontage() const
{
	if (not IsValid(M_WorkMontage))
	{
		return;
	}
	for (const ASquadUnit* Unit : GetSquadUnits())
	{
		USquadUnitAnimInstance* AnimInstance = Unit->GetAnimBP_SquadUnit();
		if (not IsValid(AnimInstance) || AnimInstance->Montage_IsPlaying(M_WorkMontage))
		{
			continue;
		}
		// Each soldier starts at his own point in the clip, so a squad does not work in lockstep.
		const float StartSeconds = FMath::FRandRange(0.f, M_WorkMontage->GetPlayLength() * 0.5f);
		AnimInstance->Montage_Play(M_WorkMontage, 1.f, EMontagePlayReturnType::MontageLength, StartSeconds);
	}
}

void UEnemyWorkCircuitRunner::StopWorkMontage() const
{
	if (not IsValid(M_WorkMontage))
	{
		return;
	}
	for (const ASquadUnit* Unit : GetSquadUnits())
	{
		USquadUnitAnimInstance* AnimInstance = Unit->GetAnimBP_SquadUnit();
		if (IsValid(AnimInstance))
		{
			AnimInstance->Montage_Stop(EnemyWorkCircuitConstants::MontageBlendOutSeconds, M_WorkMontage);
		}
	}
}

void UEnemyWorkCircuitRunner::ClearTickTimer()
{
	if (not M_TickTimerHandle.IsValid())
	{
		return;
	}
	const UObject* Outer = GetOuter();
	if (const UWorld* World = IsValid(Outer) ? Outer->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(M_TickTimerHandle);
	}
	M_TickTimerHandle.Invalidate();
}
