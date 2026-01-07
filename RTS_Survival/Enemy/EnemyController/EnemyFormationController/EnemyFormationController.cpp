// Copyright (C) Bas Blokzijl - All rights reserved.


#include "EnemyFormationController.h"

#include "NavigationSystem.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


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

void UEnemyFormationController::DebugAllActiveFormations() const
{
	for (auto FormationTuple : M_ActiveFormations)
	{
		DebugDrawFormation(FormationTuple.Value);
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
	for (auto& Pair : M_ActiveFormations)
	{
		FFormationData& Formation = Pair.Value;

		if (!Formation.FormationWaypoints.IsValidIndex(Formation.CurrentWaypointIndex))
		{
			RTSFunctionLibrary::ReportError(
				TEXT("On formation check formation has no valid waypoint index but it is still active!"));
			continue;
		}

		const FVector& WayLoc = Formation.FormationWaypoints[Formation.CurrentWaypointIndex];
		const FRotator& WayDir = Formation.FormationWaypointDirections[Formation.CurrentWaypointIndex];

		for (auto& UnitData : Formation.FormationUnits)
		{
			if (GetIsFormationUnitIdle(UnitData))
			{
				OnFormationUnitIdleReorderMove(
					UnitData, WayLoc, WayDir, Formation.FormationID);
			}
		}
	}
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
	using DeveloperSettings::GamePlay::Navigation::EnemyFormationCheckInterval;
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
	Debug(TEXT("NavMesh projection failed in EnemyController."));
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
		Debug("Formation request made with invalid width; fallback to 2");
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
	if (Formation->CheckIfFormationReachedCurrentWayPoint(Unit, this))
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
	DebugFormationReached(Formation);
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
	Debug("Formation reached FINAL destination!");
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
	if (!AliveFormation)
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
	DebugDrawFormation(*AliveFormation);

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
	UWorld* World = GetWorld();
	if (!World) return;

	const int32 WPIndex = Formation.CurrentWaypointIndex;
	if (!Formation.FormationWaypoints.IsValidIndex(WPIndex) ||
		!Formation.FormationWaypointDirections.IsValidIndex(WPIndex))
	{
		return;
	}

	// 1) Red sphere at the raw waypoint
	const FVector WaypointLocation = Formation.FormationWaypoints[WPIndex];

	const FRotator WaypointDirection = Formation.FormationWaypointDirections[WPIndex];

	for (const FFormationUnitData& UnitData : Formation.FormationUnits)
	{
		if (!UnitData.IsValidFormationUnit())
			continue;

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
			DebugStringAtLocation("Unit idle but actually waiting for formation: " + UnitName, UnitLocation,
			                      FColor::Orange, 8.f);
			return false;
		}
		DebugStringAtLocation("Formation Unit Idle" + UnitName, UnitLocation,
		                      FColor::Red, 8.f);
		return true;
	}
	return false;
}

void UEnemyFormationController::OnFormationUnitIdleReorderMove(FFormationUnitData& FormationUnit,
                                                               const FVector& WaypointLocation,
                                                               const FRotator& WaypointDirection,
                                                               const int32 FormationID)
{
	Debug("Formation unit idle -> reorder movement to waypoint.");
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
			if (!Formation.FormationUnits[i].IsValidFormationUnit())
			{
				Formation.FormationUnits.RemoveAtSwap(i);
				++RemovedCount;
			}
		}

		// Single refund equal to the number of removed units
		if (RemovedCount > 0)
		{
			Debug("Removing invalid units: gaining supplies: " + FString::FromInt(RemovedCount));
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
	Debug("Formation Units got invalid providing back supply: " + FString::FromInt(AmountFormationUnitsInvalid));
	RefundUnitWaveSupply(AmountFormationUnitsInvalid);
}

void UEnemyFormationController::OnFormationUnitDiedPostReachFinal(AActor* DestroyedActor)
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}
	Debug("Formation unit got invalid after reaching final destination, provide back supply.");
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
