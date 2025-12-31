// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Enemy/Formation/FormationData.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "EnemyFormationController.generated.h"


class AEnemyController;
class ATankMaster;
class ASquadController;

// Hash TWeakInterfacePtr<ICommands> by its underlying UObject*
// Used for matching the radius of the unit with the icommand interface.
FORCEINLINE uint32 GetTypeHash(const TWeakInterfacePtr<ICommands>& WeakPtr)
{
	return ::GetTypeHash(WeakPtr.GetObject());
}

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UEnemyFormationController : public UActorComponent
{
	GENERATED_BODY()
	
	// to refund units.
	friend struct FFormationData;

public:
	// Sets default values for this component's properties
	UEnemyFormationController();

	void InitFormationController(AEnemyController* EnemyController);


	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void MoveFormationToLocation(
		TArray<ASquadController*> SquadControllers,
		TArray<ATankMaster*> TankMasters,
		const TArray<FVector>& Waypoints,
		const FRotator& FinalWaypointDirection,
		int32 MaxFormationWidth = 2,
		const float FormationOffsetMlt = 1.f);

	void DebugAllActiveFormations() const;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;
	bool EnsureEnemyControllerIsValid();


	TMap<int32, FFormationData> M_ActiveFormations;

	/**
	 * @brief Check if formations are still going.
	 * IF a unit is found to be idle; teleport it and order it to move to the same point again.
	 */
	void CheckFormations();

	// To continuously check if the formations are still going.
	FTimerHandle M_FormationCheckTimerHandle;

	void InitWaypointsAndDirections(FFormationData& OutFormationData, const FRotator& FinalWaypointDirection,
	                                const TArray<FVector>& Waypoints) const;

	void SaveNewFormation(const FFormationData& NewFormation);

	void OnInvalidTankForFormation(const bool bHasReachedNext) const;
	void OnInvalidSquadForFormation(const bool bHasReachedNext) const;

	int32 M_LastUsedFormationID = -1;

	int32 GenerateUniqueFormationID()
	{
		return ++M_LastUsedFormationID;
	}

	FVector ProjectLocationOnNavMesh(const FVector& Location, const float Radius,
	                                 const bool bProjectInZDirection) const;

	float ExtractFormationRadiusForUnit(const TObjectPtr<AActor>& FormationUnit) const;

	bool EnsureFormationRequestIsValid(const TArray<ASquadController*>& SquadControllers,
	                                   const TArray<ATankMaster*>& TankMasters,
	                                   int32& OutMaxFormationWidth) const;

	/**
	 * @brief Calculates the unit's next destination in the formation and projects it to navmesh. Issues order to the
	 * unit to move.
	 * @param FormationUnit The unit of the formation to move.
	 * @param WaypointLocation The point to move the formation to. 
	 * @param WaypointDirection The direction from the active point to the next point.
	 * @param FormationID The ID that identifies the formation.
	 * @post The unit has received the move to order to the projected waypoint location for this unit's offset in the formation
	 * the unit is set to not have reached destination.
	 */
	void MoveUnitToWayPoint(FFormationUnitData& FormationUnit,
	                        const FVector& WaypointLocation,
	                        const FRotator& WaypointDirection, const int32 FormationID);

	void OnUnitMovementError(
		TWeakInterfacePtr<ICommands> Unit,
		ECommandQueueError Error) const;

	void OnUnitReachedFWaypoint(
		TWeakInterfacePtr<ICommands> Unit,
		const int32 FormationID);

	/** 
	 * Find the live formation in the map. 
	 * @param FormationID – the key 
	 * @param OutFormation – will be set to point *inside* M_ActiveFormations if found 
	 * @return true if found, false (and reports an error) otherwise 
	 */
	bool GetFormationDataForIDChecked(
		int32 FormationID,
		FFormationData*& OutFormation);

	/** 
	 * Find the live unit‐data in a given formation. 
	 * @param Unit – the interface ptr you’re looking for 
	 * @param FormationData – the *live* formation (by reference) 
	 * @param OutFormationUnit – will be set to point *inside* FormationData.FormationUnits 
	 * @return true if found, false (and reports an error) otherwise 
	 */
	bool GetUnitInFormation(
		const TWeakInterfacePtr<ICommands>& Unit,
		FFormationData* FormationData,
		FFormationUnitData*& OutFormationUnit) const;


	void OnCompleteFormationReached(
		FFormationData* Formation);

	void OnFormationReachedFinalDestination(FFormationData* Formation);

	/** 
     * Populate FormationUnits[] with each unit’s offset vector 
     * based on their radius, max row width, and waypoint-forward = +X, right = +Y. 
     */
	void InitFormationOffsets(
		FFormationData& OutFormationData,
		const TMap<TWeakInterfacePtr<ICommands>, float>& RadiusMap,
		int32 MaxFormationWidth, const float FormationOffSetMlt) const;

	/** Kick off movement on row 0 toward the first waypoint. */
	void StartFormationMovement(FFormationData& Formation);

	/** 
     * Debug‐draw the current waypoint, each unit’s offset location, 
     * and its navmesh‐projected point, plus the unit’s name above it. 
     */
	void DebugDrawFormation(const FFormationData& Formation) const;

	bool GetIsFormationUnitIdle(const FFormationUnitData& Unit) const;

	void OnFormationUnitIdleReorderMove(
		FFormationUnitData& FormationUnit,
		const FVector& WaypointLocation,
		const FRotator& WaypointDirection,
		const int32 FormationID);

	void TeleportFormationUnitInWayPointDirection(
		const FFormationUnitData& FormationUnit,
		const FVector& WaypointLocation,
		const FRotator& WaypointDirection) const;

	void CleanupInvalidFormations();
	void OnFormationUnitInvalidAddBackSupply(const int32 AmountFormationUnitsInvalid);

	// On destroy delegate we use from actors that reached only accepts UFunctions.
	UFUNCTION()
	void OnFormationUnitDiedPostReachFinal(AActor* DestroyedActor);

	void RefundUnitWaveSupply(const int32 AmountToRefund);

	// ------------------------------------------------------------------
	// --------------Begin Formation Grid Helpers. --------------------
	// ------------------------------------------------------------------

	// Gather all units in insertion order from the radius map.
	TArray<TWeakInterfacePtr<ICommands>> GatherFormationUnits(
		const TMap<TWeakInterfacePtr<ICommands>, float>& RadiusMap) const;

	// From Units[startIndex .. startIndex+unitsInRow), collect each one's radius.
	TArray<float> GatherRowRadii(
		const TArray<TWeakInterfacePtr<ICommands>>& Units,
		int32 StartIndex,
		int32 UnitsInRow,
		const TMap<TWeakInterfacePtr<ICommands>, float>& RadiusMap) const;

	// Return the maximum value in RowRadii (or zero if empty).
	float ComputeMaxRadiusInRow(const TArray<float>& RowRadii) const;

	// Given how far back the row sits, the column index, and that row's radii, compute the FVector offset.
	FVector ComputeOffsetForUnit(
		float RowBackOffset,
		int32 ColumnIndex,
		const TArray<float>& RowRadii, const float FormationOffsetMlt) const;


	// ------------------------------------------------------------------
	// --------------Begin Formation Grid Helpers. --------------------
	// ------------------------------------------------------------------

	static void Debug(const FString& Message);

	void DebugFormationReached(FFormationData* ReachedFormation) const;

	void DebugStringAtLocation(const FString& Message, const FVector& Location, const FColor& Color = FColor::Red,
	                           float Duration = 5.0f) const;
};
