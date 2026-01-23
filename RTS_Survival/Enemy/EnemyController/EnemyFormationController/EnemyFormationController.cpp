// Copyright (C) Bas Blokzijl - All rights reserved.


#include "EnemyFormationController.h"

#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace EnemyFormationConstants
{
	const int32 HowOftenTickStuckUnitTillTeleport = 1;
	const float SquarredDistanceDeltaConsiderStuck = 90000.f; // ~300cm
	const float TeleportXYRange = 225.f;
	const float TeleportSideRange = 300.f;
	const float TeleportDegreesRange = 20.f;
	const float TeleportSquadUnitsUnStuckHeight = 100.f;
	// WARNING; multiplied by 5 if previous teleport attempts failed; do not make this too high.
	const float FindTeleportLocationProjectionExtent = 0.2f;
	const int32 TeleportProjectionAttempts = 8;
}

UEnemyFormationController::UEnemyFormationController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyFormationController::InitFormationController(AEnemyController* EnemyController)
{
	M_EnemyController = EnemyController;
}

void UEnemyFormationController::MoveFormationToLocation(TArray<ASquadController*> SquadControllers,
                                                        TArray<ATankMaster*> TankMasters,
                                                        const TArray<FVector>& Waypoints,
                                                        const FRotator& FinalWaypointDirection, int32 MaxFormationWidth,
                                                        const float FormationOffsetMlt)
{
	if (not EnsureFormationRequestIsValid(SquadControllers, TankMasters, MaxFormationWidth))
	{
		return;
	}

	// 1) Build a fresh FormationData
	FFormationData NewFormation;
	const int32 FormationID = GenerateUniqueFormationID();
	NewFormation.FormationID = FormationID;
	// Set up the waypoints and the direction of one waypoint to the next so we know how to use the offset vector for
	// formation calculations.
	InitWaypointsAndDirections(NewFormation, FinalWaypointDirection, Waypoints);

	// For each unit store their formation radius.
	TMap<TWeakInterfacePtr<ICommands>, float> MapGuidToFormationRadius;
	for (auto EachSquad : SquadControllers)
	{
		if (not IsValid(EachSquad))
		{
			continue;
		}
		MapGuidToFormationRadius.Add(EachSquad, ExtractFormationRadiusForUnit(EachSquad));
	}
	for (auto EachTank : TankMasters)
	{
		if (not IsValid(EachTank))
		{
			continue;
		}
		MapGuidToFormationRadius.Add(EachTank, ExtractFormationRadiusForUnit(EachTank));
	}

	// 3) Compute offsets & kick off movement
	InitFormationOffsets(NewFormation, MapGuidToFormationRadius, MaxFormationWidth, FormationOffsetMlt);
	// 4) Save the formation data and start the movement.
	SaveNewFormation(NewFormation);
	StartFormationMovement(NewFormation);
}

void UEnemyFormationController::MoveAttackMoveFormationToLocation(
	TArray<ASquadController*> SquadControllers,
	TArray<ATankMaster*> TankMasters,
	const TArray<FVector>& Waypoints,
	const FRotator& FinalWaypointDirection,
	int32 MaxFormationWidth,
	const float FormationOffsetMlt,
	const FAttackMoveWaveSettings& AttackMoveSettings)
{
	if (not EnsureFormationRequestIsValid(SquadControllers, TankMasters, MaxFormationWidth))
	{
		return;
	}

	FFormationData NewFormation;
	const int32 FormationID = GenerateUniqueFormationID();
	NewFormation.FormationID = FormationID;
	NewFormation.AttackMoveSettings = AttackMoveSettings;
	NewFormation.bIsAttackMoveFormation = true;

	InitWaypointsAndDirections(NewFormation, FinalWaypointDirection, Waypoints);

	TMap<TWeakInterfacePtr<ICommands>, float> MapGuidToFormationRadius;
	for (auto EachSquad : SquadControllers)
	{
		if (not IsValid(EachSquad))
		{
			continue;
		}
		MapGuidToFormationRadius.Add(EachSquad, ExtractFormationRadiusForUnit(EachSquad));
	}
	for (auto EachTank : TankMasters)
	{
		if (not IsValid(EachTank))
		{
			continue;
		}
		MapGuidToFormationRadius.Add(EachTank, ExtractFormationRadiusForUnit(EachTank));
	}

	InitFormationOffsets(NewFormation, MapGuidToFormationRadius, MaxFormationWidth, FormationOffsetMlt);
	SaveNewFormation(NewFormation);
	StartFormationMovement(NewFormation);
}

void UEnemyFormationController::DebugAllActiveFormations() const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		for (auto FormationTuple : M_ActiveFormations)
		{
			DebugDrawFormation(FormationTuple.Value);
		}
	}
}


void UEnemyFormationController::BeginPlay()
{
	Super::BeginPlay();
}

void UEnemyFormationController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_FormationCheckTimerHandle);
	}
}

bool UEnemyFormationController::EnsureEnemyControllerIsValid()
{
	if (M_EnemyController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("EnemyFormationController has no valid enemy controller! "
		"\n see UEnemyFormationController::EnsureEnemyControllerIsValid"));
	M_EnemyController = Cast<AEnemyController>(GetOwner());
	return M_EnemyController.IsValid();
}

void UEnemyFormationController::CheckFormations()
{
	// First, prune dead units and drop empty formations:
	CleanupInvalidFormations();

	// Early-out if nothing left
	if (M_ActiveFormations.Num() == 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(M_FormationCheckTimerHandle);
		return;
	}

	// Then  idle→teleport→reorder logic on the survivors:
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	for (auto& Pair : M_ActiveFormations)
	{
		FFormationData& Formation = Pair.Value;

		if (not Formation.FormationWaypoints.IsValidIndex(Formation.CurrentWaypointIndex))
		{
			RTSFunctionLibrary::ReportError(
				TEXT("On formation check formation has no valid waypoint index but it is still active!"));
			continue;
		}

		const FVector& WayLoc = Formation.FormationWaypoints[Formation.CurrentWaypointIndex];
		const FRotator& WayDir = Formation.FormationWaypointDirections[Formation.CurrentWaypointIndex];

		UpdateFormationUnitMovementProgress(Formation, WayLoc, WayDir, NavSys);

		if (Formation.bIsAttackMoveFormation)
		{
			HandleAttackMoveFormation(Formation, WayLoc, WayDir);
			continue;
		}

		HandleFormationIdleUnits(Formation, WayLoc, WayDir);
	}
}

void UEnemyFormationController::UpdateFormationUnitMovementProgress(
	FFormationData& Formation,
	const FVector& WaypointLocation,
	const FRotator& WaypointDirection,
	UNavigationSystemV1* NavSys)
{
	for (FFormationUnitData& UnitData : Formation.FormationUnits)
	{
		if (not UnitData.IsValidFormationUnit())
		{
			continue;
		}

		if (UnitData.bHasReachedNextDestination)
		{
			continue;
		}

		UpdateFormationUnitStuckState(UnitData, WaypointLocation, WaypointDirection, NavSys);
	}
}

void UEnemyFormationController::UpdateFormationUnitStuckState(
	FFormationUnitData& FormationUnit,
	const FVector& WaypointLocation,
	const FRotator& WaypointDirection,
	UNavigationSystemV1* NavSys)
{
	if (not FormationUnit.IsValidFormationUnit())
	{
		return;
	}

	const FVector UnitLocation = FormationUnit.Unit->GetOwnerLocation();
	if (not FormationUnit.bM_HasLastKnownLocation)
	{
		FormationUnit.M_LastKnownLocation = UnitLocation;
		FormationUnit.bM_HasLastKnownLocation = true;
		return;
	}

	const float DistanceMovedSquared = FVector::DistSquared(UnitLocation, FormationUnit.M_LastKnownLocation);
	FormationUnit.M_LastKnownLocation = UnitLocation;

	if (GetIsFormationUnitInCombat(FormationUnit))
	{
		DebugFormationUnitStillMoving(FormationUnit, WaypointLocation, WaypointDirection, DistanceMovedSquared);
		return;
	}

	if (GetHasUnitMovedEnough(DistanceMovedSquared))
	{
		DebugFormationUnitStillMoving(FormationUnit, WaypointLocation, WaypointDirection, DistanceMovedSquared);
		return;
	}

	FormationUnit.StuckCounts++;
	if (FormationUnit.StuckCounts <= EnemyFormationConstants::HowOftenTickStuckUnitTillTeleport)
	{
		DebugFormationUnitStillMoving(FormationUnit, WaypointLocation, WaypointDirection, DistanceMovedSquared);
		return;
	}
	if(FormationUnit.Unit->GetIsSquadUnit())
	{
		AttemptUnStuckSquad(FormationUnit);
		return;
	}
	// Unstuck vehicles.
	AttemptUnstuckWithTeleport(FormationUnit, WaypointLocation, WaypointDirection, NavSys);

}

FVector UEnemyFormationController::GetFormationUnitRawWaypointLocation(
	const FFormationUnitData& FormationUnit,
	const FVector& WaypointLocation,
	const FRotator& WaypointDirection) const
{
	return WaypointLocation + WaypointDirection.RotateVector(FormationUnit.Offset);
}

bool UEnemyFormationController::GetHasUnitMovedEnough(
	const float DistanceMovedSquared) const
{
	return DistanceMovedSquared >= EnemyFormationConstants::SquarredDistanceDeltaConsiderStuck;
}

bool UEnemyFormationController::TryTeleportStuckFormationUnit(
	FFormationUnitData& FormationUnit,
	const FVector& WaypointLocation,
	const FRotator& WaypointDirection,
	UNavigationSystemV1* NavSys) const
{
	if (not FormationUnit.IsValidFormationUnit())
	{
		return false;
	}

	if (not NavSys)
	{
		return false;
	}

	AActor* UnitActor = FormationUnit.Unit->GetOwnerActor();
	if (not IsValid(UnitActor))
	{
		return false;
	}
	FormationUnit.Unit->SetUnitToIdle();

	const FVector UnitLocation = UnitActor->GetActorLocation();
	const FVector RawWaypointLocation = GetFormationUnitRawWaypointLocation(
		FormationUnit,
		WaypointLocation,
		WaypointDirection);

	float ProjectionExtent = EnemyFormationConstants::FindTeleportLocationProjectionExtent;
	float TpDistance = EnemyFormationConstants::TeleportXYRange;
	float TpSideDistance = EnemyFormationConstants::TeleportSideRange;
	if (FormationUnit.bPreviousStuckTeleportsFailed)
	{
		TpDistance *= 1.33f * FormationUnit.StuckCounts;
		ProjectionExtent *= 3.f;
		TpSideDistance *= 1.33f * FormationUnit.StuckCounts;
	}
	for (int32 AttemptIndex = 0; AttemptIndex < EnemyFormationConstants::TeleportProjectionAttempts; ++AttemptIndex)
	{
		const float TeleportAngleDegrees = FMath::FRandRange(
			-EnemyFormationConstants::TeleportDegreesRange,
			EnemyFormationConstants::TeleportDegreesRange);
		FVector RawTeleportLocation = FVector::ZeroVector;
		if (AttemptIndex % 2 == 0)
		{
			RawTeleportLocation = GetTpLocationTowardsWayPoint(
				UnitLocation,
				RawWaypointLocation,
				TeleportAngleDegrees,
				TpDistance);
		}
		else
		{
			const FRotator UnitRotation = UnitActor->GetActorRotation();
			RawTeleportLocation = GetTpLocationToSide(
				AttemptIndex % 3 == 0,
				UnitLocation,
				TpSideDistance, AttemptIndex, UnitRotation
			);
		}

		bool bProjectionSuccess = false;
		const FVector ProjectedTeleportLocation = RTSFunctionLibrary::GetLocationProjected_WithNavSystem(
			NavSys,
			RawTeleportLocation,
			true,
			bProjectionSuccess,
			ProjectionExtent);

		if (not bProjectionSuccess)
		{
			if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
			{
				DebugTeleportAttempt(FColor::Red, RawTeleportLocation);
			}
			continue;
		}

		if (not UnitActor->TeleportTo(ProjectedTeleportLocation, UnitActor->GetActorRotation(), false, false))
		{
			if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
			{
				DebugTeleportAttempt(FColor::Black, RawTeleportLocation);
			}
			continue;
		}
		if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
		{
			DebugTeleportAttempt(FColor::Green, RawTeleportLocation);
		}
		FormationUnit.M_LastKnownLocation = ProjectedTeleportLocation;
		FormationUnit.bM_HasLastKnownLocation = true;
		return true;
	}
	return false;
}

FVector UEnemyFormationController::GetTpLocationTowardsWayPoint(
	const FVector& UnitLocation,
	const FVector& WaypointLocation,
	const float TeleportAngleDegrees,
	const float TpDistance) const
{
	FVector DirectionToWaypoint = WaypointLocation - UnitLocation;
	DirectionToWaypoint.Z = 0.f;
	if (DirectionToWaypoint.IsNearlyZero())
	{
		DirectionToWaypoint = FVector::ForwardVector;
	}
	const FVector NormalizedDirection = DirectionToWaypoint.GetSafeNormal();
	const FVector RotatedDirection = FRotator(0.f, TeleportAngleDegrees, 0.f).RotateVector(NormalizedDirection);
	return UnitLocation + RotatedDirection * TpDistance;
}

FVector UEnemyFormationController::GetTpLocationToSide(const bool bLeftSide, const FVector& UnitLocation,
                                                       const float SideOffsetDistance, const int32 Index,
                                                       const FRotator& UnitRotation) const
{
	const float IndexMlt = 1.f + (Index / 6);
	FVector SideOffset = FVector::ZeroVector;
	if (bLeftSide)
	{
		SideOffset = UnitRotation.RotateVector(FVector(0.f, -1.f, 0.f));
	}
	else
	{
		SideOffset = UnitRotation.RotateVector(FVector(0.f, 1.f, 0.f));
	}
	return UnitLocation + SideOffset * SideOffsetDistance * IndexMlt;
}

void UEnemyFormationController::DebugFormationUnitStillMoving(
	const FFormationUnitData& FormationUnit,
	const FVector& WaypointLocation,
	const FRotator& WaypointDirection,
	const float DistanceMovedSquared) const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		if (not FormationUnit.IsValidFormationUnit())
		{
			return;
		}

		if (FormationUnit.Unit->GetActiveCommandID() == EAbilityID::IdIdle)
		{
			return;
		}

		using DeveloperSettings::GamePlay::Navigation::EnemyFormationPositionProjectionExtent;

		const FVector UnitLocation = FormationUnit.Unit->GetOwnerLocation();
		const FVector RawWaypointLocation = GetFormationUnitRawWaypointLocation(
			FormationUnit,
			WaypointLocation,
			WaypointDirection);
		const FVector ProjectedWaypointLocation = ProjectLocationOnNavMesh(
			RawWaypointLocation,
			EnemyFormationPositionProjectionExtent,
			false);
		const float DistanceToWaypointSquared = FVector::DistSquared(UnitLocation, ProjectedWaypointLocation);

		const float DebugTextHeightOffset = 200.f;
		const float DebugTextDurationSeconds = 2.f;
		const FVector DebugTextLocation = UnitLocation + FVector(0.f, 0.f, DebugTextHeightOffset);
		const FString DebugMessage = FString::Printf(
			TEXT("still moving\n%d / %d\n dist: %.2f\n prev dist: %.2f"),
			FormationUnit.StuckCounts,
			EnemyFormationConstants::HowOftenTickStuckUnitTillTeleport,
			DistanceToWaypointSquared,
			DistanceMovedSquared);
		DebugStringAtLocation(DebugMessage, DebugTextLocation, FColor::Cyan, DebugTextDurationSeconds);
	}
}

void UEnemyFormationController::DebugFormationUnitJustTeleported(const FFormationUnitData& FormationUnit,
                                                                 const FVector& WaypointLocation)
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		if (not FormationUnit.IsValidFormationUnit())
		{
			return;
		}

		const FVector UnitLocation = FormationUnit.Unit->GetOwnerLocation();
		const float SquaredDistToRawWayPoint = FVector::DistSquared(
			UnitLocation,
			GetFormationUnitRawWaypointLocation(FormationUnit, WaypointLocation, FRotator::ZeroRotator));
		const FString DebugString = "Unit just teleported."
			"\n Distance to raw waypoint squared: " + FString::SanitizeFloat(SquaredDistToRawWayPoint);
		const float DebugTextHeightOffset = 250.f;
		const float DebugTextDurationSeconds = 2.f;
		const FVector DebugTextLocation = UnitLocation + FVector(0.f, 0.f, DebugTextHeightOffset);
		DebugStringAtLocation(DebugString, DebugTextLocation, FColor::Red, DebugTextDurationSeconds);
	}
}

void UEnemyFormationController::DebugFormationUnitJustTeleported_AndFailed(const FFormationUnitData& FormationUnit,
                                                                           const FVector& WaypointLocation)
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		if (not FormationUnit.IsValidFormationUnit())
		{
			return;
		}

		const FVector UnitLocation = FormationUnit.Unit->GetOwnerLocation();
		const float SquaredDistToRawWayPoint = FVector::DistSquared(
			UnitLocation,
			GetFormationUnitRawWaypointLocation(FormationUnit, WaypointLocation, FRotator::ZeroRotator));
		const FString DebugString = "Unit FAILED TO TELEPORT."
			"\n Distance to raw waypoint squared: " + FString::SanitizeFloat(SquaredDistToRawWayPoint);
		const float DebugTextHeightOffset = 250.f;
		const float DebugTextDurationSeconds = 2.f;
		const FVector DebugTextLocation = UnitLocation + FVector(0.f, 0.f, DebugTextHeightOffset);
		DebugStringAtLocation(DebugString, DebugTextLocation, FColor::Red, DebugTextDurationSeconds);
	}
}

void UEnemyFormationController::HandleFormationIdleUnits(
	FFormationData& Formation,
	const FVector& WaypointLocation,
	const FRotator& WaypointDirection)
{
	for (auto& UnitData : Formation.FormationUnits)
	{
		// Do not reorder move if still stuck.
		if (not UnitData.bPreviousStuckTeleportsFailed && GetIsFormationUnitIdle(UnitData))
		{
			OnFormationUnitIdleReorderMove(
				UnitData, WaypointLocation, WaypointDirection, Formation.FormationID);
		}
	}
}

void UEnemyFormationController::HandleAttackMoveFormation(
	FFormationData& Formation,
	const FVector& WaypointLocation,
	const FRotator& WaypointDirection)
{
	HandleFormationIdleUnits(Formation, WaypointLocation, WaypointDirection);

	if (not GetDoAllFormationUnitsReachedWaypoint(Formation))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	const float CurrentTimeSeconds = World->GetTimeSeconds();
	bool bHasCombatUnits = false;
	TArray<const FFormationUnitData*> CombatUnits;
	const bool bCanAdvance = GetCanAttackMoveFormationAdvance(
		Formation,
		CurrentTimeSeconds,
		bHasCombatUnits,
		CombatUnits);

	if (bHasCombatUnits && not bCanAdvance)
	{
		IssueAttackMoveHelpOrders(Formation, CombatUnits);
	}

	if (bCanAdvance)
	{
		OnCompleteFormationReached(&Formation);
	}
}

bool UEnemyFormationController::GetDoAllFormationUnitsReachedWaypoint(const FFormationData& Formation) const
{
	for (const FFormationUnitData& UnitData : Formation.FormationUnits)
	{
		if (not UnitData.bHasReachedNextDestination)
		{
			return false;
		}
	}
	return true;
}

bool UEnemyFormationController::GetIsFormationUnitInCombat(const FFormationUnitData& FormationUnit) const
{
	if (not FormationUnit.IsValidFormationUnit())
	{
		return false;
	}

	// Uses RTS comp on the interface implementor.
	return FormationUnit.Unit->GetIsUnitInCombat();
}

float UEnemyFormationController::GetFormationUnitInnerRadius(const FFormationUnitData& FormationUnit) const
{
	if (not FormationUnit.IsValidFormationUnit())
	{
		return 0.f;
	}

	const AActor* UnitActor = FormationUnit.Unit->GetOwnerActor();
	if (not IsValid(UnitActor))
	{
		return 0.f;
	}

	const URTSComponent* RTSComponent = UnitActor->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent))
	{
		return 0.f;
	}

	return RTSComponent->GetFormationUnitInnerRadius();
}

const FFormationUnitData* UEnemyFormationController::FindClosestCombatUnit(
	const FFormationUnitData& UnitToAssist,
	const TArray<const FFormationUnitData*>& CombatUnits) const
{
	if (not UnitToAssist.IsValidFormationUnit())
	{
		return nullptr;
	}

	const FVector UnitLocation = UnitToAssist.Unit->GetOwnerLocation();
	const FFormationUnitData* ClosestUnit = nullptr;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();

	for (const FFormationUnitData* CombatUnit : CombatUnits)
	{
		if (not CombatUnit || not CombatUnit->IsValidFormationUnit())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(UnitLocation, CombatUnit->Unit->GetOwnerLocation());
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestUnit = CombatUnit;
		}
	}

	return ClosestUnit;
}

void UEnemyFormationController::IssueAttackMoveHelpOrders(
	const FFormationData& Formation,
	const TArray<const FFormationUnitData*>& CombatUnits) const
{
	if (CombatUnits.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (not NavSys)
	{
		return;
	}

	const float DebugTextDurationSeconds = 2.f;

	for (const FFormationUnitData& UnitData : Formation.FormationUnits)
	{
		IssueAttackMoveHelpOrderForUnit(Formation, UnitData, CombatUnits, NavSys, DebugTextDurationSeconds);
	}
}

void UEnemyFormationController::IssueAttackMoveHelpOrderForUnit(
	const FFormationData& Formation,
	const FFormationUnitData& UnitData,
	const TArray<const FFormationUnitData*>& CombatUnits,
	UNavigationSystemV1* NavSys,
	const float DebugTextDuration) const
{
	if (not UnitData.IsValidFormationUnit())
	{
		return;
	}
	if (GetIsFormationUnitInCombat(UnitData))
	{
		return;
	}

	const FFormationUnitData* ClosestCombatUnit = FindClosestCombatUnit(UnitData, CombatUnits);
	if (not ClosestCombatUnit)
	{
		return;
	}

	const float UnitInnerRadius = GetFormationUnitInnerRadius(UnitData);
	if (UnitInnerRadius <= 0.f)
	{
		return;
	}

	const FVector CombatUnitLocation = ClosestCombatUnit->Unit->GetOwnerLocation();
	FVector ProjectedLocation = CombatUnitLocation;
	if (not TryGetAttackMoveHelpLocation(
		Formation.AttackMoveSettings,
		CombatUnitLocation,
		UnitInnerRadius,
		NavSys,
		ProjectedLocation))
	{
		return;
	}

	const FVector UnitLocation = UnitData.Unit->GetOwnerLocation();
	const FRotator MoveRotation = UKismetMathLibrary::FindLookAtRotation(UnitLocation, ProjectedLocation);
	const ECommandQueueError Error = UnitData.Unit->MoveToLocation(ProjectedLocation, true, MoveRotation);
	if (Error != ECommandQueueError::NoError)
	{
		OnUnitMovementError(UnitData.Unit, Error);
		return;
	}

	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		UWorld* World = GetWorld();
		if (not World)
		{
			return;
		}

		const FString UnitName = UnitData.Unit->GetOwnerName();
		const float DebugTextHeight = 120.f;
		const FVector DebugTextLocation = ProjectedLocation + FVector(0.f, 0.f, DebugTextHeight);
		DrawDebugString(World, DebugTextLocation, "Helping combat unit: " + UnitName, nullptr, FColor::Yellow,
		                DebugTextDuration, false);
	}
}

bool UEnemyFormationController::TryGetAttackMoveHelpLocation(
	const FAttackMoveWaveSettings& AttackMoveSettings,
	const FVector& CombatUnitLocation,
	const float UnitInnerRadius,
	UNavigationSystemV1* NavSys,
	FVector& OutProjectedLocation) const
{
	const int32 MaxTries = AttackMoveSettings.MaxTriesFindNavPointForHelpOffset;
	if (MaxTries <= 0 || not NavSys)
	{
		return false;
	}

	const float FullCircleDegrees = 360.f;
	const float DebugSphereRadius = 45.f;
	const float DebugSphereDurationSeconds = 2.f;
	const int32 DebugSphereSegments = 12;

	for (int32 TryIndex = 0; TryIndex < MaxTries; ++TryIndex)
	{
		const float RandomMlt = FMath::FRandRange(
			AttackMoveSettings.HelpOffsetRadiusMltMin,
			AttackMoveSettings.HelpOffsetRadiusMltMax);
		const float Distance = UnitInnerRadius * RandomMlt;
		const float RandomAngleDegrees = FMath::FRandRange(0.f, FullCircleDegrees);
		const float RandomAngleRadians = FMath::DegreesToRadians(RandomAngleDegrees);
		const FVector OffsetDirection(FMath::Cos(RandomAngleRadians), FMath::Sin(RandomAngleRadians), 0.f);
		const FVector CandidateLocation = CombatUnitLocation + OffsetDirection * Distance;

		bool bProjectionSuccess = false;
		const FVector Projected = RTSFunctionLibrary::GetLocationProjected_WithNavSystem(
			NavSys,
			CandidateLocation,
			true,
			bProjectionSuccess,
			AttackMoveSettings.ProjectionScale);

		if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
		{
			DebugAttackMoveHelpProjection(
				CandidateLocation,
				Projected,
				bProjectionSuccess,
				DebugSphereRadius,
				DebugSphereSegments,
				DebugSphereDurationSeconds);
		}

		if (bProjectionSuccess)
		{
			OutProjectedLocation = Projected;
			return true;
		}
	}

	return false;
}

void UEnemyFormationController::DebugAttackMoveHelpProjection(
	const FVector& CandidateLocation,
	const FVector& ProjectedLocation,
	const bool bProjectionSuccess,
	const float DebugSphereRadius,
	const int32 DebugSphereSegments,
	const float DebugSphereDurationSeconds) const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		UWorld* World = GetWorld();
		if (not World)
		{
			return;
		}

		const FColor DebugColor = bProjectionSuccess ? FColor::Green : FColor::Red;
		const FVector SphereLocation = bProjectionSuccess ? ProjectedLocation : CandidateLocation;
		DrawDebugSphere(
			World,
			SphereLocation,
			DebugSphereRadius,
			DebugSphereSegments,
			DebugColor,
			false,
			DebugSphereDurationSeconds);
	}
}

bool UEnemyFormationController::GetCanAttackMoveFormationAdvance(
	FFormationData& Formation,
	const float CurrentTimeSeconds,
	bool& bOutHasCombatUnits,
	TArray<const FFormationUnitData*>& OutCombatUnits)
{
	bOutHasCombatUnits = false;
	OutCombatUnits.Reset();

	const float MaxCombatTime = Formation.AttackMoveSettings.MaxAttackTimeBeforeAdvancingToNextWayPoint;
	bool bCanAdvance = true;

	for (FFormationUnitData& UnitData : Formation.FormationUnits)
	{
		if (not UnitData.IsValidFormationUnit())
		{
			continue;
		}

		if (not GetIsFormationUnitInCombat(UnitData))
		{
			UnitData.M_CombatStartTimeSeconds = -1.f;
			continue;
		}

		bOutHasCombatUnits = true;
		OutCombatUnits.Add(&UnitData);

		if (MaxCombatTime <= 0.f)
		{
			bCanAdvance = false;
			continue;
		}

		if (UnitData.M_CombatStartTimeSeconds < 0.f)
		{
			UnitData.M_CombatStartTimeSeconds = CurrentTimeSeconds;
			bCanAdvance = false;
			continue;
		}

		const float TimeInCombat = CurrentTimeSeconds - UnitData.M_CombatStartTimeSeconds;
		if (TimeInCombat < MaxCombatTime)
		{
			bCanAdvance = false;
		}
	}

	return bCanAdvance;
}

void UEnemyFormationController::InitWaypointsAndDirections(FFormationData& OutFormationData,
                                                           const FRotator& FinalWaypointDirection,
                                                           const TArray<FVector>& Waypoints) const
{
	OutFormationData.FormationWaypoints = Waypoints;
	OutFormationData.CurrentWaypointIndex = 0;
	// For each point calculate the direction to the next point, for the final point use the special rotator.
	const int32 NumWaypoints = Waypoints.Num();
	for (int32 i = 0; i < NumWaypoints; ++i)
	{
		const FVector& CurrentWaypoint = Waypoints[i];
		if (i + 1 < NumWaypoints)
		{
			const FVector& NextWaypoint = Waypoints[i + 1];
			const FRotator DirectionToNext = UKismetMathLibrary::FindLookAtRotation(CurrentWaypoint, NextWaypoint);
			OutFormationData.FormationWaypointDirections.Add(DirectionToNext);
			continue;
		}
		OutFormationData.FormationWaypointDirections.Add(FinalWaypointDirection);
	}
}

void UEnemyFormationController::SaveNewFormation(const FFormationData& NewFormation)
{
	if (not GetWorld())
	{
		return;
	}
	using EnemyAISettings::EnemyFormationCheckInterval;
	// store into the map _before_ any callbacks fire
	M_ActiveFormations.Add(NewFormation.FormationID, NewFormation);
	if (not M_FormationCheckTimerHandle.IsValid())
	{
		FTimerDelegate TimerDelegate;
		TWeakObjectPtr<UEnemyFormationController> WeakThis(this);
		TimerDelegate.BindLambda(
			[WeakThis]()-> void
			{
				if (WeakThis.IsValid())
				{
					WeakThis->CheckFormations();
				}
			});
		GetWorld()->GetTimerManager().SetTimer(M_FormationCheckTimerHandle, TimerDelegate, EnemyFormationCheckInterval,
		                                       true);
	}
}

void UEnemyFormationController::OnInvalidTankForFormation(const bool bHasReachedNext) const
{
	const FString ErrorMessage = bHasReachedNext
		                             ? TEXT("Tank has reached the next waypoint, but is invalid for the formation.")
		                             : TEXT("Tank is invalid for the formation and no waypoint reached.");
	RTSFunctionLibrary::ReportError("Invalid tank for formation with status: " + ErrorMessage);
}

void UEnemyFormationController::OnInvalidSquadForFormation(const bool bHasReachedNext) const
{
	const FString ErrorMessage = bHasReachedNext
		                             ? TEXT("Squad has reached the next waypoint, but is invalid for the formation.")
		                             : TEXT("Squad is invalid for the formation and no waypoint reached.");
	RTSFunctionLibrary::ReportError("Invalid Squad for formation with status: " + ErrorMessage);
}

FVector UEnemyFormationController::ProjectLocationOnNavMesh(const FVector& Location, const float Radius,
                                                            const bool bProjectInZDirection) const
{
	const float ProjectionRadius = Radius > 0 ? Radius : 200.f;
	const float ZDirectionRadius = bProjectInZDirection ? ProjectionRadius : 5.f;
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (not NavSys)
	{
		return Location;
	}
	FNavLocation ProjectedLocation;
	const FVector ProjectionExtent(ProjectionRadius, ProjectionRadius, ZDirectionRadius);

	if (NavSys->ProjectPointToNavigation(Location, ProjectedLocation, ProjectionExtent))
	{
		// Successfully projected onto navmesh
		return ProjectedLocation.Location;
	}
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		Debug(TEXT("NavMesh projection failed in EnemyController."));
	}
	return Location;
}

float UEnemyFormationController::ExtractFormationRadiusForUnit(const TObjectPtr<AActor>& FormationUnit) const
{
	if (const URTSComponent* RTSComp = FormationUnit->FindComponentByClass<URTSComponent>())
	{
		return RTSComp->GetFormationUnitInnerRadius();
	}
	const FString UnitName = FormationUnit->GetName();
	RTSFunctionLibrary::ReportError("Could not get formation radius for unit: " + UnitName +
		"\n This is likely because the unit does not have a RTSComponent. "
		"\n See UEnemyFormationController::ExtractFormationRadiusForUnit"
		"\n falling back to 350 units");
	return 350.f;
}

bool UEnemyFormationController::EnsureFormationRequestIsValid(
	const TArray<ASquadController*>& SquadControllers, const TArray<ATankMaster*>& TankMasters,
	int32& OutMaxFormationWidth) const
{
	if (SquadControllers.IsEmpty() && TankMasters.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Formation request made with no units!"
			"\n see UEnemyFormationController ");
		return false;
	}
	if (OutMaxFormationWidth <= 0)
	{
		if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
		{
			Debug("Formation request made with invalid width; fallback to 2");
		}
		OutMaxFormationWidth = 2;
	}
	return true;
}

void UEnemyFormationController::MoveUnitToWayPoint(FFormationUnitData& FormationUnit, const FVector& WaypointLocation,
                                                   const FRotator& WaypointDirection, const int32 FormationID)
{
	using DeveloperSettings::GamePlay::Navigation::EnemyFormationPositionProjectionExtent;
	if (not FormationUnit.IsValidFormationUnit())
	{
		return;
	}
	// Calculate base location with waypoint location + offset in the direction of this waypoint.
	FVector LocationToProject = WaypointLocation + WaypointDirection.RotateVector(FormationUnit.Offset);

	LocationToProject = ProjectLocationOnNavMesh(LocationToProject, EnemyFormationPositionProjectionExtent, false);
	FormationUnit.M_LastKnownLocation = FormationUnit.Unit->GetOwnerLocation();
	FormationUnit.bM_HasLastKnownLocation = true;
	// Move the unit to the projected location.
	const ECommandQueueError Error = FormationUnit.Unit->MoveToLocation(LocationToProject, true, WaypointDirection);
	if (Error != ECommandQueueError::NoError)
	{
		OnUnitMovementError(FormationUnit.Unit, Error);
		return;
	}
	TWeakObjectPtr<UEnemyFormationController> WeakThis(this);
	auto CallBackOnReached = [WeakThis, FormationID, UnitWeak = FormationUnit.Unit]()
	{
		if (WeakThis.IsValid() && UnitWeak.IsValid())
		{
			WeakThis->OnUnitReachedFWaypoint(UnitWeak, FormationID);
		}
	};

	// Save the handle to the delegate so we can unbind once the unit finished the movement.
	const FDelegateHandle Handle = FormationUnit.Unit->OnUnitIdleAndNoNewCommandsDelegate.AddLambda(CallBackOnReached);
	FormationUnit.MovementCompleteHandle = Handle;
	FormationUnit.bHasReachedNextDestination = false;
	FormationUnit.M_CombatStartTimeSeconds = -1.f;
}

void UEnemyFormationController::OnUnitMovementError(const TWeakInterfacePtr<ICommands> Unit,
                                                    const ECommandQueueError Error) const
{
	if (not Unit.IsValid())
	{
		return;
	}
	const FString BaseError = "Could not move unit for enemy formation.";
	const FString UnitName = Unit->GetOwnerName();
	switch (Error)
	{
	case ECommandQueueError::QueueFull:
		RTSFunctionLibrary::ReportError(BaseError + " Unit " + UnitName + " has full queue!");
		break;
	case ECommandQueueError::QueueInactive:
		RTSFunctionLibrary::ReportError(BaseError + " Unit " + UnitName + " has Inactive queue!");
		break;
	case ECommandQueueError::QueueHasPatrol:
		RTSFunctionLibrary::ReportError(BaseError + " Unit " + UnitName + " has a patrol queue.");
		break;
	case ECommandQueueError::CommandDataInvalid:
		RTSFunctionLibrary::ReportError(BaseError + " Unit " + UnitName + " has invalid command data.");
		break;
	case ECommandQueueError::AbilityNotAllowed:
		RTSFunctionLibrary::ReportError(
			BaseError + " Unit " + UnitName + " does not allow this ability.");
		break;
	default:
		break;
	}
}

void UEnemyFormationController::OnUnitReachedFWaypoint(TWeakInterfacePtr<ICommands> Unit, const int32 FormationID)
{
	FFormationData* Formation;
	if (not GetFormationDataForIDChecked(FormationID, Formation))
	{
		return;
	}
	FFormationUnitData* UnitInFormation;
	if (not GetUnitInFormation(Unit, Formation, UnitInFormation))
	{
		return;
	}
	// Unbind the delegate for this unit.
	(void)Unit->OnUnitIdleAndNoNewCommandsDelegate.Remove(UnitInFormation->MovementCompleteHandle);
	// Note: this also marks this formation unit as "bHasReachedNextDestination".
	const bool bAllUnitsReached = Formation->CheckIfFormationReachedCurrentWayPoint(Unit, this);
	if (Formation->bIsAttackMoveFormation)
	{
		return;
	}
	if (bAllUnitsReached)
	{
		// Everyone reached the waypoint, move to the next one.
		OnCompleteFormationReached(Formation);
	}
	// Not all units in formation have reached yet, waiting for them to complete movement.
}

bool UEnemyFormationController::GetFormationDataForIDChecked(
	int32 FormationID,
	FFormationData*& OutFormation)
{
	// Find returns nullptr if missing
	FFormationData* Found = M_ActiveFormations.Find(FormationID);
	if (Found)
	{
		OutFormation = Found;
		return true;
	}

	RTSFunctionLibrary::ReportError(
		TEXT("Could not find active formation with ID: ") +
		FString::FromInt(FormationID) +
		TEXT("\n see UEnemyFormationController::GetFormationDataForIDChecked"));
	return false;
}

bool UEnemyFormationController::GetUnitInFormation(
	const TWeakInterfacePtr<ICommands>& Unit,
	FFormationData* FormationData,
	FFormationUnitData*& OutFormationUnit) const
{
	for (FFormationUnitData& EachUnit : FormationData->FormationUnits)
	{
		if (EachUnit.Unit == Unit)
		{
			OutFormationUnit = &EachUnit;
			return true;
		}
	}

	const FString UnitName = Unit.IsValid() ? Unit->GetOwnerName() : TEXT("Invalid");
	RTSFunctionLibrary::ReportError(
		TEXT("Could not find unit in formation with ID: ") +
		FString::FromInt(FormationData->FormationID) +
		TEXT("\n see UEnemyFormationController::GetUnitInFormation") +
		TEXT("\n Unit name: ") + UnitName);
	return false;
}


void UEnemyFormationController::OnCompleteFormationReached(FFormationData* Formation)
{
	Formation->CurrentWaypointIndex++;
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		DebugFormationReached(Formation);
	}
	if (Formation->CurrentWaypointIndex >= Formation->FormationWaypoints.Num())
	{
		OnFormationReachedFinalDestination(Formation);
		return;
	}
	const FVector& NextWaypoint = Formation->FormationWaypoints[Formation->CurrentWaypointIndex];
	const FRotator& NextWaypointDirection = Formation->FormationWaypointDirections[Formation->CurrentWaypointIndex];
	for (auto& EachUnit : Formation->FormationUnits)
	{
		if (not EachUnit.IsValidFormationUnit())
		{
			continue;
		}
		MoveUnitToWayPoint(EachUnit, NextWaypoint, NextWaypointDirection, Formation->FormationID);
	}
}

void UEnemyFormationController::OnFormationReachedFinalDestination(FFormationData* Formation)
{
	if (M_ActiveFormations.Num() == 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(M_FormationCheckTimerHandle);
	}
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		Debug("Formation reached FINAL destination!");
	}
	int32 InvalidUnitsInFormation = 0;
	TWeakObjectPtr<UEnemyFormationController> WeakThis(this);
	for (auto EachUnit : Formation->FormationUnits)
	{
		AActor* UnitAsActor = EachUnit.Unit->GetOwnerActor();
		if (RTSFunctionLibrary::RTSIsValid(UnitAsActor))
		{
			// Create delegate and bind to UFUNCTION
			FScriptDelegate DestroyedDelegate;
			DestroyedDelegate.BindUFunction(this, FName("OnFormationUnitDiedPostReachFinal"));

			UnitAsActor->OnDestroyed.Add(DestroyedDelegate);
			continue;;
		}
		InvalidUnitsInFormation++;
	}
	if (InvalidUnitsInFormation > 0)
	{
		OnFormationUnitInvalidAddBackSupply(InvalidUnitsInFormation);
	}
	// remove only after we iterated otherwise we have dead mem.
	M_ActiveFormations.Remove(Formation->FormationID);
}

void UEnemyFormationController::InitFormationOffsets(
	FFormationData& OutFormationData,
	const TMap<TWeakInterfacePtr<ICommands>, float>& RadiusMap,
	const int32 MaxFormationWidth, const float FormationOffSetMlt) const
{
	TArray<TWeakInterfacePtr<ICommands>> Units = GatherFormationUnits(RadiusMap);
	const int32 TotalUnits = Units.Num();
	OutFormationData.FormationUnits.Empty(TotalUnits);

	// To place rows behind each other.
	float AccumulatedRowOffset = 0.f;
	int32 NextUnitIndex = 0;

	while (NextUnitIndex < TotalUnits)
	{
		const int32 UnitsThisRow = FMath::Min(MaxFormationWidth, TotalUnits - NextUnitIndex);

		// 1) collect radii for this row
		TArray<float> RowRadii = GatherRowRadii(Units, NextUnitIndex, UnitsThisRow, RadiusMap);

		// 2) figure out how far back the *next* row will sit
		const float MaxRowRadius = ComputeMaxRadiusInRow(RowRadii);

		// 3) place each unit in this row
		for (int32 Column = 0; Column < UnitsThisRow; ++Column, ++NextUnitIndex)
		{
			FFormationUnitData UnitData;
			UnitData.Unit = Units[NextUnitIndex];
			UnitData.Offset = ComputeOffsetForUnit(AccumulatedRowOffset, Column, RowRadii, FormationOffSetMlt);
			OutFormationData.FormationUnits.Add(UnitData);
		}

		AccumulatedRowOffset += MaxRowRadius;
	}
}


void UEnemyFormationController::StartFormationMovement(FFormationData& Formation)
{
	FFormationData* AliveFormation = M_ActiveFormations.Find(Formation.FormationID);
	if (not AliveFormation)
	{
		// we’ve already torn this formation down
		return;
	}
	if (AliveFormation->FormationWaypoints.IsEmpty())
	{
		// remove this formation as no valid waypoitns found.

		M_ActiveFormations.Remove(Formation.FormationID);
		return;
	}

	// now use *Live* safely, without risk of reinserting
	const FVector& WayPoint = AliveFormation->FormationWaypoints[0];
	const FRotator& Direction = AliveFormation->FormationWaypointDirections[0];
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		DebugDrawFormation(*AliveFormation);
	}

	for (FFormationUnitData& UnitData : AliveFormation->FormationUnits)
	{
		if (UnitData.IsValidFormationUnit())
		{
			MoveUnitToWayPoint(UnitData, WayPoint, Direction, AliveFormation->FormationID);
		}
	}
}

void UEnemyFormationController::DebugDrawFormation(const FFormationData& Formation) const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		UWorld* World = GetWorld();
		if (not World)
		{
			return;
		}

		const int32 WPIndex = Formation.CurrentWaypointIndex;
		if (not Formation.FormationWaypoints.IsValidIndex(WPIndex) ||
			not Formation.FormationWaypointDirections.IsValidIndex(WPIndex))
		{
			return;
		}

		// 1) Red sphere at the raw waypoint
		const FVector WaypointLocation = Formation.FormationWaypoints[WPIndex];

		const FRotator WaypointDirection = Formation.FormationWaypointDirections[WPIndex];

		for (const FFormationUnitData& UnitData : Formation.FormationUnits)
		{
			if (not UnitData.IsValidFormationUnit())
			{
				continue;
			}

			// 2) Green sphere at the unprojected offset‐location
			const FVector OffsetLocation =
				WaypointLocation + WaypointDirection.RotateVector(UnitData.Offset);

			// 3) Purple sphere at the navmesh‐projected point
			const FVector ProjectedLocation =
				ProjectLocationOnNavMesh(OffsetLocation, /*Radius=*/0.f, /*bZ=*/false);

			// 4) Draw the unit’s name 100 units above that point
			const FString UnitName = UnitData.Unit->GetOwnerName();
			const FVector TextLocation = ProjectedLocation + FVector(0.f, 0.f, 100.f);
			DrawDebugString(World, TextLocation, UnitName, nullptr, FColor::White, 10.f, false);
		}
	}
}

bool UEnemyFormationController::GetIsFormationUnitIdle(const FFormationUnitData& Unit) const
{
	if (not Unit.IsValidFormationUnit())
	{
		return false;
	}
	if (Unit.Unit->GetActiveCommandID() == EAbilityID::IdIdle)
	{
		const FString UnitName = Unit.Unit->GetOwnerName();
		const FVector UnitLocation = Unit.Unit->GetOwnerLocation() + FVector(0.f, 0.f, 200.f);
		if (Unit.bHasReachedNextDestination)
		{
			if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
			{
				DebugStringAtLocation("Unit idle but actually waiting for other units. ", UnitLocation,
				                      FColor::Orange, 8.f);
			}
			return false;
		}
		if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
		{
			DebugStringAtLocation("Formation Unit Idle" + UnitName, UnitLocation,
			                      FColor::Red, 8.f);
		}
		return true;
	}
	return false;
}

void UEnemyFormationController::OnFormationUnitIdleReorderMove(FFormationUnitData& FormationUnit,
                                                               const FVector& WaypointLocation,
                                                               const FRotator& WaypointDirection,
                                                               const int32 FormationID)
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		Debug("Formation unit idle -> reorder movement to waypoint.");
	}
	TeleportFormationUnitInWayPointDirection(FormationUnit, WaypointLocation, WaypointDirection);

	if (FormationUnit.MovementCompleteHandle.IsValid())
	{
		// Unbind the delegate for this unit as the move to way point will bind a new one.
		FormationUnit.Unit->OnUnitIdleAndNoNewCommandsDelegate.Remove(FormationUnit.MovementCompleteHandle);
	}
	// Move the unit to the projected location.
	MoveUnitToWayPoint(FormationUnit, WaypointLocation, WaypointDirection, FormationID);
}

void UEnemyFormationController::TeleportFormationUnitInWayPointDirection(
	const FFormationUnitData& FormationUnit, const FVector& WaypointLocation, const FRotator& WaypointDirection) const
{
	using DeveloperSettings::GamePlay::Navigation::EnemyFormationPositionProjectionExtent;

	if (not FormationUnit.IsValidFormationUnit())
	{
		return;
	}
	UObject* Unit = FormationUnit.Unit.GetObject();
	if (AActor* UnitAsActor = Cast<AActor>(Unit))
	{
		FVector TeleportLocation = UnitAsActor->GetActorLocation();
		const FVector HalvedOffset = FormationUnit.Offset / 2.f;
		TeleportLocation += WaypointDirection.RotateVector(HalvedOffset);
		TeleportLocation = ProjectLocationOnNavMesh(TeleportLocation, EnemyFormationPositionProjectionExtent, false);
		UnitAsActor->SetActorLocation(TeleportLocation, false, nullptr, ETeleportType::ResetPhysics);
		return;
	}
	RTSFunctionLibrary::ReportError("Could not cast formation unit to actor for teleportation."
		"\n See UEnemyFormationController::TeleportFormationUnitInWayPointDirection");
}

void UEnemyFormationController::CleanupInvalidFormations()
{
	TArray<int32> ToRemove;

	for (auto& Pair : M_ActiveFormations)
	{
		FFormationData& Formation = Pair.Value;
		int32 RemovedCount = 0;

		// Collect & remove all dead units in one go
		for (int32 i = Formation.FormationUnits.Num() - 1; i >= 0; --i)
		{
			if (not Formation.FormationUnits[i].IsValidFormationUnit())
			{
				Formation.FormationUnits.RemoveAtSwap(i);
				++RemovedCount;
			}
		}

		// Single refund equal to the number of removed units
		if (RemovedCount > 0)
		{
			if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
			{
				Debug("Removing invalid units: gaining supplies: " + FString::FromInt(RemovedCount));
			}
			RefundUnitWaveSupply(RemovedCount);
		}

		if (Formation.FormationUnits.IsEmpty())
		{
			ToRemove.Add(Formation.FormationID);
		}
	}

	for (int32 ID : ToRemove)
	{
		M_ActiveFormations.Remove(ID);
	}
}


void UEnemyFormationController::OnFormationUnitInvalidAddBackSupply(const int32 AmountFormationUnitsInvalid)
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		Debug("Formation Units got invalid providing back supply: " + FString::FromInt(AmountFormationUnitsInvalid));
	}
	RefundUnitWaveSupply(AmountFormationUnitsInvalid);
}

void UEnemyFormationController::OnFormationUnitDiedPostReachFinal(AActor* DestroyedActor)
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		Debug("Formation unit got invalid after reaching final destination, provide back supply.");
	}
	// Provide back the wave supply after the unit finished its formation movement and died later.
	RefundUnitWaveSupply(1);
}

void UEnemyFormationController::RefundUnitWaveSupply(const int32 AmountToRefund)
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}
	M_EnemyController->AddToWaveSupply(AmountToRefund);
}


TArray<TWeakInterfacePtr<ICommands>> UEnemyFormationController::GatherFormationUnits(
	const TMap<TWeakInterfacePtr<ICommands>, float>& RadiusMap) const
{
	TArray<TWeakInterfacePtr<ICommands>> Units;
	Units.Reserve(RadiusMap.Num());
	for (const auto& Pair : RadiusMap)
	{
		Units.Add(Pair.Key);
	}
	return Units;
}

TArray<float> UEnemyFormationController::GatherRowRadii(
	const TArray<TWeakInterfacePtr<ICommands>>& Units,
	const int32 StartIndex,
	const int32 UnitsInRow,
	const TMap<TWeakInterfacePtr<ICommands>, float>& RadiusMap) const
{
	TArray<float> RowRadii;
	RowRadii.Reserve(UnitsInRow);
	for (int32 i = 0; i < UnitsInRow; ++i)
	{
		const auto& UnitKey = Units[StartIndex + i];
		RowRadii.Add(RadiusMap[UnitKey]);
	}
	return RowRadii;
}

float UEnemyFormationController::ComputeMaxRadiusInRow(const TArray<float>& RowRadii) const
{
	float MaxRadius = 0.f;
	for (const float Radius : RowRadii)
	{
		MaxRadius = FMath::Max(MaxRadius, Radius);
	}
	return MaxRadius;
}

FVector UEnemyFormationController::ComputeOffsetForUnit(
	const float RowBackOffset,
	const int32 ColumnIndex,
	const TArray<float>& RowRadii, const float FormationOffsetMlt) const
{
	// X = negative forward to Place rows behind eachother.
	float XOffset = -RowBackOffset;

	// Y = lateral: center (0), then left, then right...
	float YOffset = 0.f;
	if (ColumnIndex == 1)
	{
		// left of the lead unit
		YOffset = -FMath::Max(RowRadii[0], RowRadii[1]);
	}
	else if (ColumnIndex > 1)
	{
		// to the right of the lead unit
		YOffset = FMath::Max(RowRadii[0], RowRadii[ColumnIndex]);
	}
	const float Mlt = FMath::Max(1.f, FormationOffsetMlt);

	return FVector(XOffset, YOffset, 0.f) * Mlt;
}

bool UEnemyFormationController::TeleportActorWithCheck(AActor* ValidUnit, const FVector& TeleportLocation,
                                                       const FRotator& TeleportRotation) const
{
	return true;
}

void UEnemyFormationController::Debug(const FString& Message)
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message, FColor::Purple);
	}
}

void UEnemyFormationController::DebugFormationReached(FFormationData* ReachedFormation) const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		const FVector Location = ReachedFormation->GetFormationUnitLocation() + FVector(0.f, 0.f, 400.f);
		DebugStringAtLocation("Formation reached a location", Location, FColor::Green, 10);
	}
}

void UEnemyFormationController::DebugStringAtLocation(const FString& Message, const FVector& Location,
                                                      const FColor& Color, float Duration) const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		DrawDebugString(GetWorld(), Location, Message, nullptr, Color, Duration, false);
	}
}

void UEnemyFormationController::DebugTeleportAttempt(const FColor& TpColor, const FVector& Location) const
{
	const float Radius = 100.f;
	DrawDebugSphere(GetWorld(), Location, Radius, 12, TpColor, false, 5.f);
}

void UEnemyFormationController::AttemptUnstuckWithTeleport(FFormationUnitData& FormationUnit,
                                                             const FVector& WaypointLocation,
                                                             const FRotator& WaypointDirection,
                                                             UNavigationSystemV1* NavSys)
{
	if (TryTeleportStuckFormationUnit(FormationUnit, WaypointLocation, WaypointDirection, NavSys))
	{
		FormationUnit.StuckCounts = 0;
		FormationUnit.bPreviousStuckTeleportsFailed = false;
		DebugFormationUnitJustTeleported(FormationUnit, WaypointLocation);
	}
	else
	{
		FormationUnit.bPreviousStuckTeleportsFailed = true;
		DebugFormationUnitJustTeleported_AndFailed(FormationUnit, WaypointLocation);
	}
	
}

void UEnemyFormationController::AttemptUnStuckSquad(const FFormationUnitData& FormationUnit) const
{
	FormationUnit.Unit->UnstuckSquadMoveUp(EnemyFormationConstants::TeleportSquadUnitsUnStuckHeight);
}
