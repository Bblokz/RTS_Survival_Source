// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "FireStrikesTestSubsystem.generated.h"

class ACameraActor;
class ARTSFireManager;
class ASquadController;
class ASquadUnit;
class UGlobalAbility;
class UNiagaraComponent;

UENUM()
enum class EFireStrikesTestPhase : uint8
{
	WaitingForStart,
	Running,
	Finished
};

/**
 * @brief -FireStrikesValidate: fires GA_GerIncendiaryJu87 (along a strip) and GA_GerFireBarrage (at a point) on the
 * anchor (camera centre, or -FireStrikesAnchorX= / -FireStrikesAnchorY=) with an enemy squad and a T-34 in the target
 * area. Checks the fires land on the ground where intended, a squad sent into a fire keeps losing health, the fires go
 * out after their duration and each ability ends once. Screenshots to Saved/FireStrikesTest; logs RTS_FIRE_TEST.
 * Not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UFireStrikesTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	struct FScenario
	{
		EGlobalAbility AbilityType = EGlobalAbility::GA_None;
		FString Label;
		bool bIsStripAbility = false;
		FVector2D TargetOffset = FVector2D::ZeroVector;
		// Fires must lie within this distance (XY) of the target or of a planned drop point.
		float MaxFireDistanceCm = 0.f;
		int32 MinFires = 1;
		// Exact number of separate fires, or INDEX_NONE when merging makes it vary.
		int32 ExactFires = INDEX_NONE;
		// Also fires GA_GerFireBarrage beside the strip at the same moment (the cap and lifetime check).
		bool bAlsoFireBarrage = false;
	};

	struct FSeenFire
	{
		TWeakObjectPtr<UNiagaraComponent> Component;
		FVector Location = FVector::ZeroVector;
		float FirstSeenSeconds = 0.f;
		// When its flames stopped (or its pooled component moved to another fire); below zero while burning.
		float EndSeconds = -1.f;
	};

	struct FSoldierSample
	{
		TWeakObjectPtr<ASquadUnit> Soldier;
		float Health = 0.f;
	};

	EFireStrikesTestPhase M_Phase = EFireStrikesTestPhase::WaitingForStart;
	float M_StartSeconds = -1.f;
	float M_RunStartSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	int32 M_ScreenshotCount = 0;
	FVector M_Anchor = FVector::ZeroVector;
	float M_CameraWarmupStartSeconds = 0.f;

	TArray<FScenario> M_Scenarios;
	int32 M_ScenarioIndex = INDEX_NONE;

	// Per scenario.
	FVector M_Target = FVector::ZeroVector;
	FVector M_LineStart = FVector::ZeroVector;
	FVector M_LineEnd = FVector::ZeroVector;
	TArray<FVector> M_PlannedPoints;
	int32 M_BaselineFireCount = 0;
	int32 M_PeakFireCount = 0;
	TArray<FSeenFire> M_SeenFires;
	float M_LastNewFireSeconds = -1.f;
	float M_FiresOutSeconds = -1.f;
	int32 M_EndedCount = 0;
	float M_EndedSeconds = -1.f;
	float M_FireDurationSeconds = 0.f;
	float M_FireRadiusCm = 0.f;
	TSet<TWeakObjectPtr<AActor>> M_PreExistingAircraft;
	TArray<TWeakObjectPtr<AActor>> M_StrikeAircraft;
	int32 M_Ju87Seen = 0;
	TArray<TWeakObjectPtr<ASquadController>> M_EnemySquads;
	TWeakObjectPtr<ASquadController> M_VictimSquad;
	TWeakObjectPtr<ASquadController> M_WalkInSquad;
	TWeakObjectPtr<AActor> M_Vehicle;
	float M_VehicleStartHealth = -1.f;
	float M_VehicleMaxHealth = -1.f;
	float M_VehicleHealthAfterBlasts = -1.f;
	TWeakObjectPtr<AActor> M_WalkInVehicle;
	float M_WalkInVehicleEarlyHealth = -1.f;
	float M_VictimSquadStartHealth = -1.f;
	FVector M_WalkInFireLocation = FVector::ZeroVector;
	float M_WalkInSpawnSeconds = -1.f;
	TArray<FSoldierSample> M_EarlySamples;
	float M_EarlySampleSeconds = -1.f;
	float M_EarlyHealthSum = -1.f;
	float M_LateHealthSum = -1.f;
	bool bM_TookStrikeShot = false;
	bool bM_TookBurningShot = false;
	bool bM_TookFireballShot = false;
	bool bM_TookLateShot = false;

	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_Ability;

	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_SecondAbility;

	FVector M_SecondTarget = FVector::ZeroVector;
	float M_SecondTargetReachCm = 0.f;
	// Combined scenario: spots of the flame-unit stand-in fires and when to feed them again.
	TArray<FVector> M_FlamerSpots;
	float M_NextFlamerFeedSeconds = 0.f;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	float GetRunSeconds() const;
	void Finish();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;
	void BuildScenarios();
	void DisableFogDarkening() const;
	void StartScenario(int32 ScenarioIndex);
	void ResetScenarioState();
	bool CreateAbility(const FScenario& Scenario);
	void ReadFireTuning(const FScenario& Scenario);
	void PlanTarget(const FScenario& Scenario);
	void SpawnVictims(const FScenario& Scenario);
	ASquadController* SpawnEnemySquad(const FVector& GroundLocation);
	AActor* SpawnEnemyVehicle(const FVector& GroundLocation) const;
	void ExecuteAbility(const FScenario& Scenario);
	void ExecuteSecondBarrage();
	void PlanFlamerFires();
	void FeedFlamerFires();
	bool GetIsFlamerFire(const FVector& Location) const;
	void OnAbilityEnded(UGlobalAbility* Ability);
	void TrackFireEnds(const TArray<UNiagaraComponent*>& ActiveVfx);
	void CheckFireLifetimes(const FScenario& Scenario);

	void TickRun();
	void MatchEnemySoldiersToSquadOwner();
	void TrackAircraft();
	void TrackFires();
	void TrackWalkInSquad();
	void TakeEarlySamples();
	void TakeLateSamples();
	bool GetIsScenarioDone() const;
	void FinishScenario();
	void CheckFirePlacement(const FScenario& Scenario);
	void CheckBurnDamage();
	void CheckVehicle();
	void CheckCleanEnd();

	ARTSFireManager* FindFireManager() const;
	void GatherActiveGroundFireVfx(TArray<UNiagaraComponent*>& OutComponents) const;
	bool GetIsInsideAnyFire(const FVector& Location) const;
	float GetUnitHealth(const AActor* Unit) const;
	float GetUnitMaxHealth(const AActor* Unit) const;
	float GetSquadHealth(ASquadController* Squad) const;
	int32 CountLivingStrikeAircraft() const;
	int32 CountCanisters() const;

	void UpdateCamera() const;
	void RequestScreenshot(const FString& Label);
};
