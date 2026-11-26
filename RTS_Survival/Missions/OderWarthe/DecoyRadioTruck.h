// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementActor.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"

#include "DecoyRadioTruck.generated.h"

class ADirectionFindingManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDecoyRadioTruckDestroyed);

/** One point of the radio truck's drive; it waits there WaitSeconds before the next. The last point is where it parks. */
USTRUCT(BlueprintType)
struct FDecoyTruckRoutePoint
{
	GENERATED_BODY()

	// Relative to the actor (drag the diamond in the viewport).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route", meta = (MakeEditWidget = "true"))
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route", meta = (ClampMin = "0.0", Units = "s"))
	float WaitSeconds = 0.f;
};

/** The two vehicles; kept together because every step orders both. */
USTRUCT()
struct FDecoyTruckVehicles
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Truck;

	UPROPERTY()
	TWeakObjectPtr<AActor> Escort;
};

/**
 * @brief Place one in Ger_OderWarthe: the decoy radio truck of Hard and up (OderWartheMissionFlow.md section 15).
 * Once the enemy AI is ready it spawns the ZIL radio truck and its escort at the first route point. They drive the
 * route behind the line, waiting at each point with a WaitSeconds, and park for good at the last point. There the
 * truck starts transmitting: the direction-finding manager adds decoy wedges towards it. Destroying the truck silences
 * the decoy and raises OnRadioTruckDestroyed (bonus "Destroy the radio truck"). Below MinimumDifficulty nothing spawns.
 * Placed by ow_map_build.step_decoy_truck().
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ADecoyRadioTruck : public AEnemyPlacementActor
{
	GENERATED_BODY()

public:
	ADecoyRadioTruck();

	virtual FColor GetPlacementColor() const override;
	virtual FString GetConfigurationProblem() const override;

	/** @return The map's decoy radio truck actor, or null (no error) on maps without one. */
	UFUNCTION(BlueprintPure, Category = "Decoy Truck", meta = (WorldContext = "WorldContextObject"))
	static ADecoyRadioTruck* FindDecoyRadioTruck(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Decoy Truck")
	bool GetIsRadioTruckDestroyed() const { return bM_IsTruckDestroyed; }

	// True once it parked at the last point and transmits (and until the truck dies).
	UFUNCTION(BlueprintPure, Category = "Decoy Truck")
	bool GetIsTransmitting() const { return bM_IsTransmitting; }

	UFUNCTION(BlueprintPure, Category = "Decoy Truck")
	AActor* GetRadioTruck() const { return M_Vehicles.Truck.Get(); }

	UPROPERTY(BlueprintAssignable, Category = "Decoy Truck")
	FOnDecoyRadioTruckDestroyed OnRadioTruckDestroyed;

protected:
	virtual FString GetPlacementKindName() const override;
	virtual FName GetPlacementId() const override { return TEXT("DecoyRadioTruck"); }
	virtual void BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const override;
	virtual void OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty) override;
	virtual bool GetNeedsEnemyController() const override { return false; }
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoy Truck")
	ERTSGameDifficulty MinimumDifficulty = ERTSGameDifficulty::Hard;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoy Truck|Units")
	TSoftClassPtr<AActor> TruckClass;

	// The unit the truck counts as (the radio truck runs as the ZIL fuel truck).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoy Truck|Units")
	FTrainingOption TruckOption;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoy Truck|Units")
	FTrainingOption EscortOption;

	// In driving order; the last point is the parking spot the decoy transmits from.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoy Truck|Route")
	TArray<FDecoyTruckRoutePoint> Route;

	// The truck counts as arrived this close to a route point (2D).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoy Truck|Route", meta = (ClampMin = "100.0"))
	float ArriveRadiusCm = 900.f;

	// The escort drives to each point this far behind the truck's target, along the route.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoy Truck|Route", meta = (ClampMin = "0.0"))
	float EscortTrailCm = 1200.f;

	// Set by the map build.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Decoy Truck")
	TObjectPtr<ADirectionFindingManager> DirectionFinding;

private:
	UPROPERTY()
	FDecoyTruckVehicles M_Vehicles;

	FTimerHandle M_DriveTimerHandle;
	FTimerHandle M_StartTimerHandle;
	int32 M_RoutePointIndex = 0;
	double M_WaitUntilSeconds = -1.0;
	bool bM_IsTransmitting = false;
	bool bM_IsTruckDestroyed = false;

	FVector GetRoutePointWorld(const int32 PointIndex) const;
	// The enemy AI can be ready behind the start screen, before the difficulty is final: wait for Start, then decide.
	void TryStartAfterGameStart();
	void SpawnVehicles();
	void OnTruckSpawned(AActor* SpawnedActor);
	void OnEscortSpawned(AActor* SpawnedActor);
	void OrderToRoutePoint(const int32 PointIndex) const;
	static void OrderMove(AActor* Vehicle, const FVector& Location, const FRotator& Facing);
	void TickDrive();
	void HandleArrival();
	void StartTransmitting();
	void HandleTruckDestroyed();
	bool GetIsValidDirectionFinding() const;
};
