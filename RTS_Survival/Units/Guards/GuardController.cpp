// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GuardController.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/Units/Guards/GuardUnit/GuardUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

AGuardController::AGuardController()
{
}

void AGuardController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAutoGuardingPermanently();
	Super::EndPlay(EndPlayReason);
}

void AGuardController::ExecuteAttackCommand(AActor* TargetActor)
{
	StopAutoGuardingPermanently();
	Super::ExecuteAttackCommand(TargetActor);
}

void AGuardController::ExecuteAttackGroundCommand(const FVector GroundLocation)
{
	StopAutoGuardingPermanently();
	Super::ExecuteAttackGroundCommand(GroundLocation);
}

void AGuardController::ExecutePatrolCommand(const FVector PatrolToLocation)
{
	StopAutoGuardingPermanently();
	Super::ExecutePatrolCommand(PatrolToLocation);
}

void AGuardController::GeneralMoveToForAbility(const FVector& MoveToLocation, const EAbilityID AbilityID)
{
	StopAutoGuardingPermanently();
	Super::GeneralMoveToForAbility(MoveToLocation, AbilityID);
}

void AGuardController::LoadSquadUnitsAsync()
{
	if (TryLoadSquadUnitsFromMapSetup())
	{
		return;
	}

	M_TSquadUnits.Reset();
	M_LoadedGuardClassesByPath.Reset();
	M_GuardUnitRuntimeStates.Reset();
	M_RuntimeGuardPoints.Reset();
	M_SquadLoadingStatus.bM_HasFinishedLoading = false;
	M_SquadLoadingStatus.bM_HasInitializedData = false;
	M_SquadLoadingStatus.M_AmountOfSquadUnitsToLoad = SquadUnitClasses.Num();

	if (M_SpawnPoints.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"Guard controller requires at least one spawn point before loading units."
			"\n Controller: " + GetName());
		OnAllSquadUnitsLoaded();
		return;
	}

	if (SquadUnitClasses.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"Guard controller requires at least one squad unit class before loading units."
			"\n Controller: " + GetName());
		OnAllSquadUnitsLoaded();
		return;
	}

	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	for (const TSoftClassPtr<ASquadUnit>& SoftClass : SquadUnitClasses)
	{
		if (SoftClass.IsValid())
		{
			OnSquadUnitClassLoaded(SoftClass);
			continue;
		}

		if (SoftClass.ToSoftObjectPath().IsNull())
		{
			OnSquadUnitClassLoaded(SoftClass);
			continue;
		}

		StreamableManager.RequestAsyncLoad(
			SoftClass.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(
				this,
				&AGuardController::OnSquadUnitClassLoaded,
				SoftClass));
	}
}

void AGuardController::OnAllSquadUnitsLoaded()
{
	Super::OnAllSquadUnitsLoaded();

	if (M_SquadLoadingStatus.bM_HasInitializedData)
	{
		TryStartAutoGuarding();
		return;
	}

	SquadDataCallbacks.CallbackOnSquadDataLoaded(&AGuardController::HandleGuardSquadDataLoaded, this);
}

void AGuardController::OnSquadUnitClassLoaded(TSoftClassPtr<ASquadUnit> LoadedClass)
{
	if (LoadedClass.IsValid())
	{
		UClass* UnitClass = LoadedClass.Get();
		if (UnitClass == nullptr || not UnitClass->IsChildOf(AGuardUnit::StaticClass()))
		{
			RTSFunctionLibrary::ReportError(
				"Guard controller received a squad unit class that does not derive from AGuardUnit."
				"\n Controller: " + GetName() +
				"\n Class: " + LoadedClass.ToString());
		}
		else
		{
			M_LoadedGuardClassesByPath.Add(LoadedClass.ToSoftObjectPath(), UnitClass);
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"Guard controller failed to load a squad unit class."
			"\n Controller: " + GetName());
	}

	M_SquadLoadingStatus.M_AmountOfSquadUnitsToLoad--;
	if (M_SquadLoadingStatus.M_AmountOfSquadUnitsToLoad > 0)
	{
		return;
	}

	SpawnLoadedGuardUnitsAtSpawnPoints();
	OnAllSquadUnitsLoaded();
}

void AGuardController::OnSquadUnitOutOfRange(const FVector& TargetLocation)
{
	if (bM_IsAutoGuardActive)
	{
		return;
	}

	Super::OnSquadUnitOutOfRange(TargetLocation);
}

void AGuardController::SetUnitToIdleSpecificLogic()
{
	StopAutoGuardingPermanently();
	Super::SetUnitToIdleSpecificLogic();
}

void AGuardController::StopMovement()
{
	StopAutoGuardingPermanently();
	Super::StopMovement();
}

void AGuardController::UnitInSquadDied(ASquadUnit* UnitDied, const bool bUnitSelected, const ERTSDeathType DeathType)
{
	RemoveGuardRuntimeStateForUnit(UnitDied);
	Super::UnitInSquadDied(UnitDied, bUnitSelected, DeathType);
}

void AGuardController::BuildRuntimeGuardPoints()
{
	M_RuntimeGuardPoints.Reset();

	const auto AddPointIfUnique = [this](const FVector& Point)
	{
		const bool bAlreadyAdded = M_RuntimeGuardPoints.ContainsByPredicate(
			[this, Point](const FVector& ExistingPoint)
			{
				return ExistingPoint.Equals(Point, M_GuardPointEqualityToleranceCm);
			});
		if (bAlreadyAdded)
		{
			return;
		}

		M_RuntimeGuardPoints.Add(Point);
	};

	for (const FVector& SpawnPoint : M_SpawnPoints)
	{
		AddPointIfUnique(SpawnPoint);
	}

	for (const FVector& GuardPoint : M_GuardPoints)
	{
		AddPointIfUnique(GuardPoint);
	}
}

void AGuardController::HandleGuardSquadDataLoaded()
{
	TryStartAutoGuarding();
}

void AGuardController::HandleGuardUnitMoveFinished(AGuardUnit* GuardUnit, const bool bReachedDestination)
{
	if (not bM_IsAutoGuardActive || not IsValid(GuardUnit))
	{
		return;
	}

	for (FGuardUnitRuntimeState& GuardState : M_GuardUnitRuntimeStates)
	{
		if (GuardState.M_GuardUnit.Get() != GuardUnit)
		{
			continue;
		}

		if (bReachedDestination)
		{
			GuardState.M_ConsecutiveMoveFailures = 0;
			GuardState.bM_IsMovingTowardsGuardPoint = not GuardState.bM_IsMovingTowardsGuardPoint;
			IssueGuardMoveForState(GuardState);
			return;
		}

		GuardState.M_ConsecutiveMoveFailures++;
		if (GuardState.M_ConsecutiveMoveFailures > M_MaxGuardMoveRetries)
		{
			RTSFunctionLibrary::ReportError(
				"Guard unit failed too many guard moves and will stop autonomous guarding."
				"\n Controller: " + GetName() +
				"\n Unit: " + GuardUnit->GetName());
			GuardUnit->SetAutoGuardingEnabled(false);
			RemoveGuardRuntimeStateForUnit(GuardUnit);
			return;
		}

		IssueGuardMoveForState(GuardState);
		return;
	}
}

void AGuardController::IssueGuardMoveForState(FGuardUnitRuntimeState& GuardState)
{
	AGuardUnit* GuardUnit = GuardState.M_GuardUnit.Get();
	if (not IsValid(GuardUnit))
	{
		return;
	}

	const FVector Destination = GuardState.bM_IsMovingTowardsGuardPoint
		? GuardState.M_GuardPoint
		: GuardState.M_SpawnPoint;
	if (GuardUnit->StartGuardMove(Destination))
	{
		return;
	}

	GuardState.M_ConsecutiveMoveFailures++;
	if (GuardState.M_ConsecutiveMoveFailures <= M_MaxGuardMoveRetries)
	{
		return;
	}

	RTSFunctionLibrary::ReportError(
		"Guard unit failed to start custom guard movement and will stop autonomous guarding."
		"\n Controller: " + GetName() +
		"\n Unit: " + GuardUnit->GetName());
	GuardUnit->SetAutoGuardingEnabled(false);
	RemoveGuardRuntimeStateForUnit(GuardUnit);
}

bool AGuardController::TryAddGuardRuntimeState(AGuardUnit* GuardUnit, const FVector& SpawnPoint)
{
	if (not IsValid(GuardUnit))
	{
		return false;
	}

	FVector GuardPoint;
	if (not TryGetClosestGuardPoint(SpawnPoint, SpawnPoint, GuardPoint))
	{
		return false;
	}

	FGuardUnitRuntimeState GuardState;
	GuardState.Reset();
	GuardState.M_GuardUnit = GuardUnit;
	GuardState.M_SpawnPoint = SpawnPoint;
	GuardState.M_GuardPoint = GuardPoint;
	GuardState.bM_IsMovingTowardsGuardPoint = true;
	GuardState.M_MoveCompletedHandle = GuardUnit->OnGuardMoveFinished.AddUObject(
		this,
		&AGuardController::HandleGuardUnitMoveFinished);
	M_GuardUnitRuntimeStates.Add(GuardState);
	return true;
}

bool AGuardController::TryGetClosestGuardPoint(
	const FVector& CurrentLocation,
	const FVector& ExcludedLocation,
	FVector& OutGuardPoint) const
{
	OutGuardPoint = FVector::ZeroVector;

	float BestAxisDelta = TNumericLimits<float>::Max();
	float BestDistanceSquared = TNumericLimits<float>::Max();
	bool bFoundPoint = false;

	for (const FVector& CandidatePoint : M_RuntimeGuardPoints)
	{
		if (CandidatePoint.Equals(ExcludedLocation, M_GuardPointEqualityToleranceCm))
		{
			continue;
		}

		const float XDelta = FMath::Abs(CandidatePoint.X - CurrentLocation.X);
		const float YDelta = FMath::Abs(CandidatePoint.Y - CurrentLocation.Y);
		const float BestCandidateAxisDelta = FMath::Min(XDelta, YDelta);
		const float CandidateDistanceSquared = FVector::DistSquared2D(CandidatePoint, CurrentLocation);

		const bool bIsCloserAxisMatch = BestCandidateAxisDelta < BestAxisDelta;
		const bool bIsEquallyAlignedButCloser = FMath::IsNearlyEqual(BestCandidateAxisDelta, BestAxisDelta)
			&& CandidateDistanceSquared < BestDistanceSquared;
		if (not bIsCloserAxisMatch && not bIsEquallyAlignedButCloser)
		{
			continue;
		}

		BestAxisDelta = BestCandidateAxisDelta;
		BestDistanceSquared = CandidateDistanceSquared;
		OutGuardPoint = CandidatePoint;
		bFoundPoint = true;
	}

	return bFoundPoint;
}

TArray<TSubclassOf<AGuardUnit>> AGuardController::GetOrderedGuardClasses() const
{
	TArray<TSubclassOf<AGuardUnit>> OrderedGuardClasses;
	OrderedGuardClasses.Reserve(SquadUnitClasses.Num());

	for (const TSoftClassPtr<ASquadUnit>& SoftClass : SquadUnitClasses)
	{
		const TSubclassOf<AGuardUnit>* FoundClass = M_LoadedGuardClassesByPath.Find(SoftClass.ToSoftObjectPath());
		if (FoundClass == nullptr || *FoundClass == nullptr)
		{
			RTSFunctionLibrary::ReportError(
				"Guard controller could not resolve a loaded guard class for spawning."
				"\n Controller: " + GetName() +
				"\n Class: " + SoftClass.ToString());
			continue;
		}

		OrderedGuardClasses.Add(*FoundClass);
	}

	return OrderedGuardClasses;
}

void AGuardController::RemoveGuardDelegateBinding(FGuardUnitRuntimeState& GuardState)
{
	AGuardUnit* GuardUnit = GuardState.M_GuardUnit.Get();
	if (not IsValid(GuardUnit) || not GuardState.M_MoveCompletedHandle.IsValid())
	{
		GuardState.M_MoveCompletedHandle.Reset();
		return;
	}

	GuardUnit->OnGuardMoveFinished.Remove(GuardState.M_MoveCompletedHandle);
	GuardState.M_MoveCompletedHandle.Reset();
}

void AGuardController::RemoveGuardRuntimeStateForUnit(const ASquadUnit* GuardUnit)
{
	for (int32 GuardStateIndex = M_GuardUnitRuntimeStates.Num() - 1; GuardStateIndex >= 0; --GuardStateIndex)
	{
		FGuardUnitRuntimeState& GuardState = M_GuardUnitRuntimeStates[GuardStateIndex];
		if (GuardState.M_GuardUnit.Get() != GuardUnit)
		{
			continue;
		}

		RemoveGuardDelegateBinding(GuardState);
		M_GuardUnitRuntimeStates.RemoveAt(GuardStateIndex);
	}

	bM_IsAutoGuardActive = M_GuardUnitRuntimeStates.Num() > 0;
}

void AGuardController::SpawnLoadedGuardUnitsAtSpawnPoints()
{
	M_TSquadUnits.Reset();
	M_TSquadUnits.Reserve(M_SpawnPoints.Num());

	const TArray<TSubclassOf<AGuardUnit>> OrderedGuardClasses = GetOrderedGuardClasses();
	if (OrderedGuardClasses.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"Guard controller did not resolve any valid AGuardUnit classes for spawning."
			"\n Controller: " + GetName());
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(
			"Guard controller could not access a valid world for spawning units."
			"\n Controller: " + GetName());
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 SpawnPointIndex = 0; SpawnPointIndex < M_SpawnPoints.Num(); SpawnPointIndex++)
	{
		const FVector SpawnPoint = M_SpawnPoints[SpawnPointIndex];
		const TSubclassOf<AGuardUnit> GuardClass = OrderedGuardClasses[SpawnPointIndex % OrderedGuardClasses.Num()];
		if (GuardClass == nullptr)
		{
			RTSFunctionLibrary::ReportError(
				"Guard controller encountered a null guard class while cycling spawn points."
				"\n Controller: " + GetName());
			continue;
		}

		AGuardUnit* SpawnedGuardUnit = World->SpawnActor<AGuardUnit>(
			GuardClass,
			SpawnPoint,
			FRotator::ZeroRotator,
			SpawnParameters);
		if (not IsValid(SpawnedGuardUnit))
		{
			RTSFunctionLibrary::ReportError(
				"Guard controller failed to spawn a guard unit."
				"\n Controller: " + GetName());
			continue;
		}

		SpawnedGuardUnit->SetActorLocation(SpawnPoint, false, nullptr, ETeleportType::TeleportPhysics);
		SpawnedGuardUnit->SetSquadController(this);
		SpawnedGuardUnit->SetAwaitingInitialGuardMove(true);
		M_TSquadUnits.Add(SpawnedGuardUnit);
	}

	if (M_TSquadUnits.Num() == M_SpawnPoints.Num())
	{
		return;
	}

	RTSFunctionLibrary::ReportError(
		"Guard controller spawned fewer guard units than authored spawn points."
		"\n Controller: " + GetName() +
		"\n Spawn points: " + FString::FromInt(M_SpawnPoints.Num()) +
		"\n Spawned units: " + FString::FromInt(M_TSquadUnits.Num()));
}

void AGuardController::StopAutoGuardingPermanently()
{
	if (bM_AutoGuardPermanentlyStopped)
	{
		return;
	}

	bM_AutoGuardPermanentlyStopped = true;
	bM_IsAutoGuardActive = false;

	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		AGuardUnit* GuardUnit = Cast<AGuardUnit>(SquadUnit);
		if (not IsValid(GuardUnit))
		{
			continue;
		}

		GuardUnit->SetAutoGuardingEnabled(false);
	}

	for (FGuardUnitRuntimeState& GuardState : M_GuardUnitRuntimeStates)
	{
		RemoveGuardDelegateBinding(GuardState);
	}

	M_GuardUnitRuntimeStates.Reset();
}

void AGuardController::TryStartAutoGuarding()
{
	if (bM_AutoGuardPermanentlyStopped || bM_IsAutoGuardActive)
	{
		return;
	}

	BuildRuntimeGuardPoints();
	if (M_RuntimeGuardPoints.Num() <= 1)
	{
		RTSFunctionLibrary::ReportError(
			"Guard controller requires at least two distinct guard points across spawn points and guard points."
			"\n Controller: " + GetName());
		return;
	}

	M_GuardUnitRuntimeStates.Reset();
	const int32 GuardUnitCount = FMath::Min(M_SpawnPoints.Num(), M_TSquadUnits.Num());
	for (int32 GuardUnitIndex = 0; GuardUnitIndex < GuardUnitCount; GuardUnitIndex++)
	{
		AGuardUnit* GuardUnit = Cast<AGuardUnit>(M_TSquadUnits[GuardUnitIndex]);
		if (not IsValid(GuardUnit))
		{
			RTSFunctionLibrary::ReportError(
				"Guard controller contains a squad unit that is not a valid guard unit."
				"\n Controller: " + GetName());
			continue;
		}

		GuardUnit->SetAutoGuardingEnabled(true);
		if (not TryAddGuardRuntimeState(GuardUnit, M_SpawnPoints[GuardUnitIndex]))
		{
			RTSFunctionLibrary::ReportError(
				"Guard controller could not assign a guard point to one of its guard units."
				"\n Controller: " + GetName() +
				"\n Unit: " + GuardUnit->GetName());
		}
	}

	for (FGuardUnitRuntimeState& GuardState : M_GuardUnitRuntimeStates)
	{
		IssueGuardMoveForState(GuardState);
	}

	bM_IsAutoGuardActive = M_GuardUnitRuntimeStates.Num() > 0;
}
