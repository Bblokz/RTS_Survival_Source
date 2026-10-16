// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Player/Abilities.h"
#include "EnemyAbilityAITestSubsystem.generated.h"

class ACPPController;
class AEnemyController;
class AMissionManager;

UENUM()
enum class EEnemyAbilityAITestPhase : uint8
{
	Inactive,
	WaitingForGameStart,
	Observing,
	Retreating,
	Done
};

// A formation one of whose units interrupted itself with a grenade; it has to keep going afterwards.
USTRUCT()
struct FEnemyAbilityAITestFormationWatch
{
	GENERATED_BODY()

	int32 FormationID = INDEX_NONE;
	int32 WaypointIndexAtAbility = 0;
	float AbilityGameSeconds = 0.f;
};

/**
 * @brief Plays a mission map with the enemy unit ability AI forced on (Ironman) and checks that abilities never break
 * enemy attack moves or retreats: no ability for retreating units, no grenade for units walking to a formation waypoint,
 * no early waypoint arrival after a grenade, formations keep advancing, and an ordered retreat still arrives.
 * Started with -EnemyAbilityAIValidate; logs RTS_ENEMY_ABILITY_AI_TEST. Not in a shipping build.
 */
UCLASS()
class RTS_SURVIVAL_API UEnemyAbilityAITestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// The game boots paused behind the start widget; the test has to tick to start it.
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	EEnemyAbilityAITestPhase M_Phase = EEnemyAbilityAITestPhase::Inactive;
	float M_PhaseStartGameSeconds = 0.f;
	double M_PhaseStartRealSeconds = 0.0;
	int32 M_FailedCheckCount = 0;
	bool bM_IsSuppressingErrorDialogs = false;
	float M_ObserveGameSeconds = 0.f;
	float M_RetreatAtGameSeconds = 0.f;
	float M_NextSampleGameSeconds = 0.f;
	float M_NextDiagnosticGameSeconds = 0.f;
	float M_InjectWaveAtGameSeconds = 0.f;
	bool bM_HasInjectedWave = false;
	// A second wave is sent when no formation is left to retreat once the first one has been judged.
	bool bM_HasInjectedRetreatWave = false;
	// Game seconds of the first grenade thrown by a formation unit; the retreat waits until its formation was judged.
	float M_FirstFormationGrenadeSeconds = -1.f;
	bool bM_HasLoggedEnemyCards = false;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController;

	UPROPERTY()
	TWeakObjectPtr<AMissionManager> M_MissionManager;

	// Formations to follow after one of their units threw a grenade, by formation ID.
	TMap<int32, FEnemyAbilityAITestFormationWatch> M_FormationWatches;

	// Game seconds of the last grenade per unit, to judge that unit's next waypoint arrival.
	TMap<TWeakObjectPtr<AActor>, float> M_LastGrenadeSecondsByUnit;

	// What the run saw.
	int32 M_BehavioursUsed = 0;
	int32 M_GrenadesThrown = 0;
	int32 M_GrenadesInFormations = 0;
	int32 M_AbilitiesByRetreatingUnits = 0;
	int32 M_GrenadesWhileWalkingToWaypoint = 0;
	int32 M_ArrivalsAfterGrenade = 0;
	int32 M_EarlyArrivalsAfterGrenade = 0;
	float M_MaxArrivalDistance = 0.f;
	int32 M_FormationsAdvancedAfterGrenade = 0;
	int32 M_FormationsEndedAfterGrenade = 0;
	int32 M_FormationsHoldingInCombat = 0;
	int32 M_StalledFormations = 0;
	int32 M_WaypointAdvancesSeen = 0;
	TMap<int32, int32> M_LastSeenWaypointIndexByFormation;

	bool bM_HasOrderedRetreat = false;
	bool bM_HasRetreatArrived = false;
	float M_RetreatSecondsTaken = 0.f;
	int32 M_RetreatingUnitCount = 0;

	FDelegateHandle M_AbilityUsedHandle;
	FDelegateHandle M_WaypointReachedHandle;
	FDelegateHandle M_RetreatArrivedHandle;

	void EnterPhase(EEnemyAbilityAITestPhase NewPhase);
	float GetGameSeconds() const;
	float GetGameSecondsInPhase() const;
	void Check(bool bCondition, const TCHAR* Description, const FString& Details = FString());
	void Finish();

	bool TryStartGame();
	void KeepGameRunning() const;
	void ForceHardestDifficulty();
	bool TryBindToEnemyAI();
	void UnbindFromEnemyAI();

	void TickWaitingForGameStart();
	void TickObserving();
	void TickRetreating();
	void SampleFormations();
	void ResolveFormationWatches(const TMap<int32, int32>& WaypointIndexByFormation,
	                             const TSet<int32>& FormationsInCombat);
	bool TryOrderRetreat();
	bool GetIsRetreatDue(const float NowSeconds) const;

	/**
	 * @brief Sends an enemy attack-move wave of units that carry abilities at the player's units, through the enemy
	 * controller's own wave spawning; mission maps alone often have no enemy units with abilities.
	 */
	void InjectAbilityWave();
	bool TryGetPlayerTargetLocation(FVector& OutLocation) const;
	bool TryGetEnemySpawnLocation(FVector& OutLocation) const;
	bool ProjectOntoNavMesh(FVector& InOutLocation) const;

	void OnEnemyUnitAbilityUsed(AActor* UnitActor, EAbilityID Ability, const FVector& AimLocation);
	void OnFormationUnitReachedWaypoint(AActor* UnitActor, const FVector& WaypointDestination, int32 FormationID);
	void OnRetreatArrived(int32 RetreatID, float SecondsTaken);
	bool GetIsUnitRetreating(const AActor* UnitActor) const;
	int32 FindFormationOfUnit(const AActor* UnitActor, int32& OutWaypointIndex) const;
	void LogSummary() const;
	void LogDiagnostics();
	void LogEnemyCardsOnce();
};
