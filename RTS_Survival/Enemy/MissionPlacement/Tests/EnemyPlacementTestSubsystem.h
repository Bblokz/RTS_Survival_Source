// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyTypes.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "Subsystems/WorldSubsystem.h"

#include "EnemyPlacementTestSubsystem.generated.h"

class AConvoy;
class AEnemyController;
class AEnemyConvoyRoute;
class AEnemyStrongpoint;
class AEnemyWaveSpawner;
class UConvoyDefinition;

/**
 * @brief -EnemyPlacementValidate on a mission map with an enemy controller (UT_EnemyAI): places a strongpoint and
 * convoy routes near the camera and checks posts, facing, dig-in, the trench base, reinforcements, the release to
 * direct control, the cleared event, convoy guard tiers per difficulty, waiting convoys, truck tallies, the owning
 * building stop and that nothing shows in game. Writes Saved/EnemyPlacementTest/results.txt and quits.
 * Only built with RTS_WITH_MAP_TESTS.
 */
UCLASS()
class RTS_SURVIVAL_API UEnemyPlacementTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(const float DeltaTime) override;
	// The controller pauses mission maps at start; the test unpauses it.
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	// Steps of the scripted run, each taken once its time since the test started has passed.
	enum class EStep : uint8
	{
		WaitingForStart,
		CheckGarrison,
		KillConvoyTruck,
		CheckReleaseAndReinforcements,
		EndFirstConvoy,
		CheckCleared,
		EndSecondConvoy,
		CheckRoutes,
		Finished
	};

	EStep M_Step = EStep::WaitingForStart;
	double M_TestStartSeconds = -1.0;
	ERTSGameDifficulty M_Difficulty = ERTSGameDifficulty::Normal;
	int32 M_ExpectedDefenders = 0;

	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController;

	UPROPERTY()
	TObjectPtr<AEnemyStrongpoint> M_Strongpoint;

	UPROPERTY()
	TObjectPtr<AEnemyWaveSpawner> M_ReinforcementSpawner;

	UPROPERTY()
	TObjectPtr<AEnemyConvoyRoute> M_ConvoyRoute;

	UPROPERTY()
	TObjectPtr<AEnemyConvoyRoute> M_DeadBuildingRoute;

	UPROPERTY()
	TObjectPtr<UConvoyDefinition> M_ConvoyDefinition;

	UPROPERTY()
	TArray<TWeakObjectPtr<AConvoy>> M_StartedConvoys;

	TArray<int32> M_StartedGuardTiers;
	TArray<int32> M_StartedTruckCounts;
	int32 M_KilledTrucksInFirstConvoy = 0;
	int32 M_ReleasedUnitsRegistered = 0;
	bool bM_ReleasedSquad = false;
	TArray<EConvoyOutcome> M_ConvoyOutcomes;
	int32 M_GarrisonSpawnedBroadcasts = 0;
	int32 M_ReleasedUnitCount = 0;
	int32 M_ReinforcementsBroadcasts = 0;
	int32 M_ClearedBroadcasts = 0;
	int32 M_RouteFinishedBroadcasts = 0;
	int32 M_DeadRouteFinishedBroadcasts = 0;
	double M_SecondConvoyStartSeconds = -1.0;
	double M_GarrisonSpawnedSeconds = -1.0;
	double M_LastStepSeconds = 0.0;
	FVector M_Center = FVector::ZeroVector;

	TArray<FString> M_Lines;
	int32 M_Failed = 0;

	UFUNCTION()
	void OnGarrisonSpawned(AEnemyStrongpoint* Strongpoint, const TArray<AActor*>& Units);

	UFUNCTION()
	void OnGarrisonReleased(AEnemyStrongpoint* Strongpoint, const TArray<AActor*>& Units);

	UFUNCTION()
	void OnReinforcementsCalled(AEnemyStrongpoint* Strongpoint);

	UFUNCTION()
	void OnStrongpointCleared(AEnemyStrongpoint* Strongpoint);

	UFUNCTION()
	void OnConvoyStarted(AEnemyConvoyRoute* Route, AConvoy* Convoy);

	UFUNCTION()
	void OnConvoyFinished(AEnemyConvoyRoute* Route, AConvoy* Convoy, EConvoyOutcome Outcome);

	UFUNCTION()
	void OnRouteFinished(AEnemyConvoyRoute* Route);

	UFUNCTION()
	void OnDeadRouteFinished(AEnemyConvoyRoute* Route);

	double GetTestSeconds() const;
	void StartTest();
	void TickSteps();
	void AdvanceStep(const EStep NextStep);
	bool GetHasLivingTruckOrFinishedFirstConvoy() const;
	void SpawnStrongpoint();
	void ConfigureStrongpoint(AEnemyStrongpoint& Strongpoint) const;
	void SpawnReinforcementSpawner();
	void SpawnConvoyRoutes();
	AEnemyConvoyRoute* SpawnConfiguredRoute(const FVector& Location, const FName RouteId);
	bool FindDrivableRouteStart(FVector& InOutStart) const;
	void CheckGarrison();
	void CheckDefenderOnPost(const AActor* Defender, const int32 PostIndex);
	void CheckDigIn();
	void CheckTrenchBase();
	void KillDefenders(const bool bOnlyDugIn);
	void KillUnit(AActor* Unit) const;
	bool GetIsStepDue(const double DueSeconds);
	void CheckReleaseAndReinforcements();
	void CheckCleared();
	void KillFirstConvoyTruck();
	void EndNewestConvoy();
	void CheckRoutes();
	int32 GetExpectedGuardTier(const int32 ConvoyIndex) const;
	void CheckNothingVisibleInGame(const AActor* PlacedActor, const FString& Name);
	void Evaluate();
	void Check(const bool bCondition, const FString& Name, const FString& Detail);
	bool FindNavigableCenter(FVector& OutCenter) const;
};
