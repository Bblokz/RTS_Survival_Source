// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"

#include "EnemyAirDropTestSubsystem.generated.h"

class AEnemyAirDrop;
class AEnemyController;

/**
 * @brief -EnemyAirDropValidate on a mission map with an enemy controller (UT_EnemyAI): places air drops near the
 * camera and checks a mixed parachute drop (squads jump, vehicles land) repeated a fixed count while waiting for the
 * previous drop, a cargo plane drop with per-difficulty extras, Only While Alive with a destroyed linked actor,
 * Start When Destroyed, the delivered units (owner, type, direct control) and that nothing shows in game.
 * Writes Saved/EnemyAirDropTest/results.txt and quits. Only built with RTS_WITH_MAP_TESTS.
 */
UCLASS()
class RTS_SURVIVAL_API UEnemyAirDropTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(const float DeltaTime) override;
	// The controller pauses mission maps at start; the test unpauses it.
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual void Deinitialize() override;

private:
	enum class EStep : uint8
	{
		WaitingForStart,
		DestroyTriggerActor,
		WaitForDrops,
		CheckUnits,
		Finished
	};

	EStep M_Step = EStep::WaitingForStart;
	double M_TestStartSeconds = -1.0;
	double M_LastStepSeconds = 0.0;
	double M_DropsDoneSeconds = -1.0;
	ERTSGameDifficulty M_Difficulty = ERTSGameDifficulty::Normal;
	FVector M_Center = FVector::ZeroVector;

	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController;

	UPROPERTY()
	TObjectPtr<AEnemyAirDrop> M_ParaDrop;

	UPROPERTY()
	TObjectPtr<AEnemyAirDrop> M_CargoDrop;

	UPROPERTY()
	TObjectPtr<AEnemyAirDrop> M_WhileAliveDrop;

	UPROPERTY()
	TObjectPtr<AEnemyAirDrop> M_WhenDestroyedDrop;

	UPROPERTY()
	TObjectPtr<AActor> M_TriggerActor;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_SpawnedUnits;

	FDelegateHandle M_ActorSpawnedHandle;

	/** What the test saw of one spawned unit near a landing zone; kept after the unit dies in the fighting. */
	struct FSeenUnit
	{
		bool bVehicle = false;
		uint8 Subtype = 0;
		bool bWasRegistered = false;
	};

	// Spawned enemy units seen within LandingZoneRadius of a test landing point, by unit.
	TMap<FObjectKey, FSeenUnit> M_SeenUnits;
	TArray<FVector> M_LandingPoints;

	// Abilities each para drop flew, by drop number (read from the run while it flies).
	TMap<int32, int32> M_ParaAbilitiesPerDrop;
	TArray<double> M_ParaDropStartSeconds;
	TArray<double> M_ParaDropFinishSeconds;
	int32 M_ParaFinishedBroadcasts = 0;
	int32 M_CargoFinishedBroadcasts = 0;
	int32 M_WhileAliveFinishedBroadcasts = 0;
	int32 M_WhenDestroyedStartedBroadcasts = 0;

	TArray<FString> M_Lines;
	int32 M_Failed = 0;

	UFUNCTION()
	void OnParaDropStarted(AEnemyAirDrop* AirDrop, int32 DropNumber);

	UFUNCTION()
	void OnParaDropFinished(AEnemyAirDrop* AirDrop, int32 DropNumber);

	UFUNCTION()
	void OnParaAirDropFinished(AEnemyAirDrop* AirDrop);

	UFUNCTION()
	void OnCargoAirDropFinished(AEnemyAirDrop* AirDrop);

	UFUNCTION()
	void OnWhileAliveAirDropFinished(AEnemyAirDrop* AirDrop);

	UFUNCTION()
	void OnWhenDestroyedDropStarted(AEnemyAirDrop* AirDrop, int32 DropNumber);

	double GetTestSeconds() const;
	void StartTest();
	void TickSteps();
	void AdvanceStep(const EStep NextStep);
	void OnActorSpawned(AActor* SpawnedActor);
	void TrackParaRuns();
	void TrackDeliveredUnits();
	bool GetIsNearLandingPoint(const FVector& Location) const;
	AEnemyAirDrop* SpawnAirDrop(const FVector& Offset, const FName AirDropId);
	void SpawnParaDrop();
	void SpawnCargoDrop();
	void SpawnLinkedDrops();
	void CheckTemplates();
	void CheckLinkedDropsBeforeTrigger();
	void CheckWhenDestroyedStarted();
	void CheckParaDrops();
	void CheckDeliveredUnits();
	int32 CountDeliveredUnits(const bool bVehicle, const uint8 Subtype, int32& OutRegistered) const;
	void CheckConfiguredDropHasNoProblem();
	void CheckNothingVisibleInGame(const AActor* PlacedActor, const FString& Name);
	void Evaluate();
	void Check(const bool bCondition, const FString& Name, const FString& Detail);
	bool FindNavigableCenter(FVector& OutCenter) const;
};
