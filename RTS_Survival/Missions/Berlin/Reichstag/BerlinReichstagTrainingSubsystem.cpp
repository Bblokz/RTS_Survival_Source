// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BerlinReichstagTrainingSubsystem.h"

#include "BerlinReichstagPatrolRoute.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/Fallback/EnemyFallbackSubsystem.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogBerlinReichstagTraining, Log, All);

namespace BerlinReichstagTrainingConstants
{
	constexpr float FormationOffsetMultiplier = 1.f;
	constexpr int32 MinPatrolPoints = 2;
	// Attack groups walk to the player's base in legs of at most this length (cm).
	constexpr float MaxAttackLegLength = 25000.f;
	const FVector WaypointProjectionExtent(2000.f, 2000.f, 3000.f);
}

UBerlinReichstagTrainingSubsystem* UBerlinReichstagTrainingSubsystem::GetBerlinReichstagTrainingSubsystem(
	const UObject* WorldContextObject)
{
	if (not IsValid(WorldContextObject))
	{
		return nullptr;
	}
	const UWorld* World = WorldContextObject->GetWorld();
	return IsValid(World) ? World->GetSubsystem<UBerlinReichstagTrainingSubsystem>() : nullptr;
}

bool UBerlinReichstagTrainingSubsystem::StartReichstagTraining(UBerlinReichstagTrainingData* TrainingData,
                                                               const ERTSGameDifficulty Difficulty)
{
	if (not IsValid(TrainingData))
	{
		RTSFunctionLibrary::ReportError("StartReichstagTraining: no training data asset; the Reichstag base keeps "
			"the map's training settings.");
		return false;
	}
	AEnemyController* EnemyController = FRTS_Statics::GetEnemyControllerForPlayer(this, RTSPlayers::FirstAIPlayer);
	if (not IsValid(EnemyController))
	{
		RTSFunctionLibrary::ReportError("StartReichstagTraining: no enemy controller for the Soviet player.");
		return false;
	}
	M_TrainingData = TrainingData;
	M_Level = TrainingData->GetLevelForDifficulty(Difficulty);
	M_EnemyController = EnemyController;
	EnemyController->ApplyMissionTraining(M_Level.Training);

	UEnemyStrategicAIComponent* StrategicAI = EnemyController->GetEnemyStrategicAIComponent();
	if (IsValid(StrategicAI) && not M_TrainedUnitReadyHandle.IsValid())
	{
		M_TrainedUnitReadyHandle = StrategicAI->OnBuildingTrainedUnitReady.AddUObject(
			this, &UBerlinReichstagTrainingSubsystem::HandleTrainedUnitReady);
		M_BoundStrategicAI = StrategicAI;
	}
	GatherRoutes();
	bM_IsStarted = true;
	UE_LOG(LogBerlinReichstagTraining, Display,
	       TEXT("RT_TRAIN start difficulty=%s points/min=%.1f start=%d cap=%d squads=%d tanks=%d routes=%d"),
	       *UEnum::GetValueAsString(Difficulty), M_Level.Training.TrainingPointsPerMinute,
	       M_Level.Training.TrainingPointsStart, M_Level.Training.MaxAliveBuildingTrainedUnits,
	       M_Level.Training.Squads.Num(), M_Level.Training.Tanks.Num(), M_Routes.Num());
	return true;
}

void UBerlinReichstagTrainingSubsystem::StartAttacksOnPlayerBase(AActor* Target)
{
	bM_IsAttackPhase = true;
	M_AttackTarget = Target;
	if (IsValid(Target))
	{
		M_LastAttackTargetLocation = Target->GetActorLocation();
	}
	UE_LOG(LogBerlinReichstagTraining, Display, TEXT("RT_TRAIN attack phase, target %s at %s"),
	       IsValid(Target) ? *Target->GetName() : TEXT("none"), *M_LastAttackTargetLocation.ToCompactString());
	DispatchGroups();
}

int32 UBerlinReichstagTrainingSubsystem::GetActivePatrolCount()
{
	for (int32 Index = M_Patrols.Num() - 1; Index >= 0; --Index)
	{
		const bool bAnyAlive = M_Patrols[Index].Units.ContainsByPredicate([](const TWeakObjectPtr<AActor>& Unit)
		{
			return UEnemyFallbackSubsystem::GetIsUnitActorAlive(Unit.Get());
		});
		if (not bAnyAlive)
		{
			M_Patrols.RemoveAtSwap(Index);
		}
	}
	return M_Patrols.Num();
}

void UBerlinReichstagTrainingSubsystem::Deinitialize()
{
	if (UEnemyStrategicAIComponent* StrategicAI = M_BoundStrategicAI.Get())
	{
		StrategicAI->OnBuildingTrainedUnitReady.Remove(M_TrainedUnitReadyHandle);
	}
	M_TrainedUnitReadyHandle.Reset();
	M_Garrison.Empty();
	M_Patrols.Empty();
	Super::Deinitialize();
}

bool UBerlinReichstagTrainingSubsystem::GetIsValidEnemyController() const
{
	if (M_EnemyController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_EnemyController",
	                                                             "GetIsValidEnemyController", this);
	return false;
}

void UBerlinReichstagTrainingSubsystem::HandleTrainedUnitReady(AActor* Unit)
{
	if (not bM_IsStarted || not IsValid(Unit))
	{
		return;
	}
	M_Garrison.AddUnique(Unit);
	UE_LOG(LogBerlinReichstagTraining, Display, TEXT("RT_TRAIN trained %s, garrison %d"), *Unit->GetName(),
	       M_Garrison.Num());
	DispatchGroups();
}

void UBerlinReichstagTrainingSubsystem::DispatchGroups()
{
	PruneDeadGarrison();
	// Each pass sends at most one group; stop when the next group cannot be filled or sent.
	while (true)
	{
		const EGroupKind Kind = PickNextGroupKind();
		if (Kind == EGroupKind::None)
		{
			return;
		}
		const int32 GroupSize = Kind == EGroupKind::Patrol ? M_Level.PatrolGroupSize : M_Level.AttackGroupSize;
		if (M_Garrison.Num() < FMath::Max(1, GroupSize))
		{
			return;
		}
		const TArray<AActor*> Units = TakeGarrisonUnits(GroupSize);
		const bool bSent = Kind == EGroupKind::Patrol ? SendPatrol(Units) : SendAttack(Units);
		if (not bSent)
		{
			// Back to the garrison; try again with the next trained unit.
			for (AActor* Unit : Units)
			{
				M_Garrison.Add(Unit);
			}
			return;
		}
		if (bM_IsAttackPhase)
		{
			M_AttackShareAccumulator += M_Level.AttackShareAfterAlarm;
			if (Kind == EGroupKind::Attack)
			{
				M_AttackShareAccumulator = FMath::Max(0.f, M_AttackShareAccumulator - 1.f);
			}
		}
	}
}

UBerlinReichstagTrainingSubsystem::EGroupKind UBerlinReichstagTrainingSubsystem::PickNextGroupKind()
{
	const bool bPatrolSlotFree = GetActivePatrolCount() < M_Level.MaxReichstagPatrols && not M_Routes.IsEmpty();
	if (not bM_IsAttackPhase)
	{
		return bPatrolSlotFree ? EGroupKind::Patrol : EGroupKind::None;
	}
	if (not bPatrolSlotFree || M_AttackShareAccumulator + M_Level.AttackShareAfterAlarm >= 1.f)
	{
		return EGroupKind::Attack;
	}
	return EGroupKind::Patrol;
}

bool UBerlinReichstagTrainingSubsystem::SendPatrol(const TArray<AActor*>& Units)
{
	if (not GetIsValidEnemyController() || M_Routes.IsEmpty() || not IsValid(M_TrainingData))
	{
		return false;
	}
	const ABerlinReichstagPatrolRoute* Route = M_Routes[M_NextRouteIndex % M_Routes.Num()].Get();
	M_NextRouteIndex++;
	const TArray<FVector> PatrolPoints = IsValid(Route) ? Route->GetWorldPatrolPoints() : TArray<FVector>();
	if (PatrolPoints.Num() < BerlinReichstagTrainingConstants::MinPatrolPoints)
	{
		RTSFunctionLibrary::ReportError("Reichstag patrol route has fewer than two patrol points; group stays home.");
		return false;
	}
	TArray<ASquadController*> Squads;
	TArray<ATankMaster*> Tanks;
	FVector AverageLocation;
	SplitUnits(Units, Squads, Tanks, AverageLocation);
	if (Squads.IsEmpty() && Tanks.IsEmpty())
	{
		return false;
	}
	ClaimUnits(Units);
	const FRandomPatrolWithAttackMoveSettings& Settings = M_TrainingData->PatrolSettings;
	M_EnemyController->MoveRandomPatrolWithAttackMoveFormation(
		Squads, Tanks, PatrolPoints, Settings.OverrideFirstPatrolPointIndex, Settings.AmountIterationsAtPatrolPoint,
		Settings.GuardTimePerPatrolPointIteration, Settings.GuardSphereRadius, M_TrainingData->MaxFormationWidth,
		BerlinReichstagTrainingConstants::FormationOffsetMultiplier, Settings.AttackMoveSettings, AverageLocation);
	FBerlinReichstagUnitGroup& Group = M_Patrols.AddDefaulted_GetRef();
	for (AActor* Unit : Units)
	{
		Group.Units.Add(Unit);
	}
	UE_LOG(LogBerlinReichstagTraining, Display, TEXT("RT_TRAIN patrol sent: %d units on %s, patrols %d"), Units.Num(),
	       IsValid(Route) ? *Route->GetName() : TEXT("?"), M_Patrols.Num());
	OnReichstagPatrolSent.Broadcast(Units);
	return true;
}

bool UBerlinReichstagTrainingSubsystem::SendAttack(const TArray<AActor*>& Units)
{
	if (not GetIsValidEnemyController() || not IsValid(M_TrainingData))
	{
		return false;
	}
	if (const AActor* Target = M_AttackTarget.Get())
	{
		M_LastAttackTargetLocation = Target->GetActorLocation();
	}
	if (M_LastAttackTargetLocation.IsNearlyZero())
	{
		return false;
	}
	TArray<ASquadController*> Squads;
	TArray<ATankMaster*> Tanks;
	FVector AverageLocation;
	SplitUnits(Units, Squads, Tanks, AverageLocation);
	if (Squads.IsEmpty() && Tanks.IsEmpty())
	{
		return false;
	}
	ClaimUnits(Units);
	const TArray<FVector> Waypoints = BuildAttackWaypoints(AverageLocation, M_LastAttackTargetLocation);
	M_EnemyController->MoveAttackMoveFormationToLocation(
		Squads, Tanks, Waypoints, FRotator::ZeroRotator, M_TrainingData->MaxFormationWidth,
		BerlinReichstagTrainingConstants::FormationOffsetMultiplier, M_TrainingData->AttackSettings, AverageLocation);
	UE_LOG(LogBerlinReichstagTraining, Display, TEXT("RT_TRAIN attack sent: %d units at %s via %d waypoints"),
	       Units.Num(), *M_LastAttackTargetLocation.ToCompactString(), Waypoints.Num());
	OnReichstagAttackSent.Broadcast(Units);
	return true;
}

TArray<FVector> UBerlinReichstagTrainingSubsystem::BuildAttackWaypoints(const FVector& From, const FVector& To) const
{
	using namespace BerlinReichstagTrainingConstants;
	// Long path queries fail on the navmesh node limit (about 600 m on Ger_Berlin): go in legs.
	const float Distance = FVector::Dist2D(From, To);
	const int32 LegCount = FMath::Max(1, FMath::CeilToInt(Distance / MaxAttackLegLength));
	const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	TArray<FVector> Waypoints;
	for (int32 Leg = 1; Leg < LegCount; ++Leg)
	{
		const FVector Point = FMath::Lerp(From, To, static_cast<float>(Leg) / LegCount);
		FNavLocation Projected;
		if (IsValid(NavSystem) && NavSystem->ProjectPointToNavigation(Point, Projected, WaypointProjectionExtent))
		{
			Waypoints.Add(Projected.Location);
		}
	}
	Waypoints.Add(To);
	return Waypoints;
}

void UBerlinReichstagTrainingSubsystem::ClaimUnits(const TArray<AActor*>& Units) const
{
	if (not GetIsValidEnemyController())
	{
		return;
	}
	UEnemyDirectControlComponent* DirectControl = M_EnemyController->GetEnemyDirectControlComponent();
	if (not IsValid(DirectControl))
	{
		return;
	}
	// Out of the AI's idle pool, so its own orders (stand spot release, defense positions) leave them alone.
	DirectControl->DeregisterDirectControlUnits(Units);
}

void UBerlinReichstagTrainingSubsystem::SplitUnits(const TArray<AActor*>& Units, TArray<ASquadController*>& OutSquads,
                                                   TArray<ATankMaster*>& OutTanks, FVector& OutAverageLocation)
{
	OutAverageLocation = FVector::ZeroVector;
	int32 Counted = 0;
	for (AActor* Unit : Units)
	{
		if (ASquadController* Squad = Cast<ASquadController>(Unit))
		{
			OutSquads.Add(Squad);
		}
		else if (ATankMaster* Tank = Cast<ATankMaster>(Unit))
		{
			OutTanks.Add(Tank);
		}
		else
		{
			continue;
		}
		OutAverageLocation += Unit->GetActorLocation();
		++Counted;
	}
	if (Counted > 0)
	{
		OutAverageLocation /= Counted;
	}
}

TArray<AActor*> UBerlinReichstagTrainingSubsystem::TakeGarrisonUnits(const int32 Count)
{
	TArray<AActor*> Units;
	while (Units.Num() < Count && not M_Garrison.IsEmpty())
	{
		AActor* Unit = M_Garrison[0].Get();
		M_Garrison.RemoveAt(0);
		if (UEnemyFallbackSubsystem::GetIsUnitActorAlive(Unit))
		{
			Units.Add(Unit);
		}
	}
	return Units;
}

void UBerlinReichstagTrainingSubsystem::PruneDeadGarrison()
{
	for (int32 Index = M_Garrison.Num() - 1; Index >= 0; --Index)
	{
		if (not UEnemyFallbackSubsystem::GetIsUnitActorAlive(M_Garrison[Index].Get()))
		{
			M_Garrison.RemoveAt(Index);
		}
	}
}

void UBerlinReichstagTrainingSubsystem::GatherRoutes()
{
	M_Routes.Empty();
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TArray<ABerlinReichstagPatrolRoute*> Found;
	for (TActorIterator<ABerlinReichstagPatrolRoute> It(World); It; ++It)
	{
		Found.Add(*It);
	}
	// Stable order so routes are used in the same turn every game.
	Found.Sort([](const ABerlinReichstagPatrolRoute& A, const ABerlinReichstagPatrolRoute& B)
	{
		return A.GetName() < B.GetName();
	});
	for (ABerlinReichstagPatrolRoute* Route : Found)
	{
		M_Routes.Add(Route);
	}
}
