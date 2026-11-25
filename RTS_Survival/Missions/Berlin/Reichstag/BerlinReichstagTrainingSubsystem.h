// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BerlinReichstagTrainingData.h"

#include "BerlinReichstagTrainingSubsystem.generated.h"

class ABerlinReichstagPatrolRoute;
class AEnemyController;
class ASquadController;
class ATankMaster;
class UEnemyStrategicAIComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBerlinReichstagGroupSent, const TArray<AActor*>&, Units);

/** Units of one group the base sent out; weak, they die. */
USTRUCT()
struct FBerlinReichstagUnitGroup
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Units;
};

/**
 * @brief Runs the Reichstag base's training in the Berlin mission. Mission_Berlin_Arrival calls StartReichstagTraining
 * with the difficulty's data; units the base trains then guard it or leave as patrols round the Reichstag
 * (ABerlinReichstagPatrolRoute). Mission_Berlin_ResponseForce calls StartAttacksOnPlayerBase on the alarm: from then
 * on part of the new groups attack the player's HQ.
 * @note StartReichstagTraining: call once at mission start (Mission_Berlin_Arrival, SetUpReichstagTraining).
 */
UCLASS()
class RTS_SURVIVAL_API UBerlinReichstagTrainingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Berlin|Reichstag", meta = (WorldContext = "WorldContextObject"))
	static UBerlinReichstagTrainingSubsystem* GetBerlinReichstagTrainingSubsystem(const UObject* WorldContextObject);

	/**
	 * @brief Applies the difficulty's training to the Soviet AI and starts routing the units its buildings train.
	 * @param TrainingData The Reichstag training asset (DA_Berlin_ReichstagTraining).
	 * @param Difficulty The running difficulty; Ironman uses Brutal.
	 * @return False when the asset or the enemy controller is missing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Berlin|Reichstag")
	bool StartReichstagTraining(UBerlinReichstagTrainingData* TrainingData, const ERTSGameDifficulty Difficulty);

	/** @brief The alarm: from now on part of the new groups (and the waiting garrison) attack Target's location. */
	UFUNCTION(BlueprintCallable, Category = "Berlin|Reichstag")
	void StartAttacksOnPlayerBase(AActor* Target);

	UFUNCTION(BlueprintPure, Category = "Berlin|Reichstag")
	int32 GetActivePatrolCount();

	UFUNCTION(BlueprintPure, Category = "Berlin|Reichstag")
	bool GetIsAttackPhase() const { return bM_IsAttackPhase; }

	UPROPERTY(BlueprintAssignable, Category = "Berlin|Reichstag")
	FOnBerlinReichstagGroupSent OnReichstagPatrolSent;

	UPROPERTY(BlueprintAssignable, Category = "Berlin|Reichstag")
	FOnBerlinReichstagGroupSent OnReichstagAttackSent;

	virtual void Deinitialize() override;

private:
	enum class EGroupKind : uint8
	{
		None,
		Patrol,
		Attack
	};

	UPROPERTY()
	TObjectPtr<UBerlinReichstagTrainingData> M_TrainingData = nullptr;

	FBerlinReichstagTrainingLevel M_Level;

	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;

	// Where OnBuildingTrainedUnitReady is bound; unbound on Deinitialize.
	TWeakObjectPtr<UEnemyStrategicAIComponent> M_BoundStrategicAI = nullptr;
	FDelegateHandle M_TrainedUnitReadyHandle;

	UPROPERTY()
	TArray<TWeakObjectPtr<ABerlinReichstagPatrolRoute>> M_Routes;
	int32 M_NextRouteIndex = 0;

	// Trained units not sent anywhere yet: they guard the base (the AI's own stand spot / defense orders).
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_Garrison;

	UPROPERTY()
	TArray<FBerlinReichstagUnitGroup> M_Patrols;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_AttackTarget = nullptr;
	FVector M_LastAttackTargetLocation = FVector::ZeroVector;

	// After the alarm: grows by AttackShareAfterAlarm per group; at 1 or more the group attacks.
	float M_AttackShareAccumulator = 0.f;
	bool bM_IsAttackPhase = false;
	bool bM_IsStarted = false;

	bool GetIsValidEnemyController() const;
	void HandleTrainedUnitReady(AActor* Unit);
	void DispatchGroups();
	EGroupKind PickNextGroupKind();
	bool SendPatrol(const TArray<AActor*>& Units);
	bool SendAttack(const TArray<AActor*>& Units);
	// Straight-line legs from the group to the target, projected onto the navmesh, ending on the target.
	TArray<FVector> BuildAttackWaypoints(const FVector& From, const FVector& To) const;
	void ClaimUnits(const TArray<AActor*>& Units) const;
	static void SplitUnits(const TArray<AActor*>& Units, TArray<ASquadController*>& OutSquads,
	                       TArray<ATankMaster*>& OutTanks, FVector& OutAverageLocation);
	TArray<AActor*> TakeGarrisonUnits(const int32 Count);
	void PruneDeadGarrison();
	void GatherRoutes();
};
