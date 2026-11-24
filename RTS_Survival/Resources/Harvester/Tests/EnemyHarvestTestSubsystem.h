// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "EnemyHarvestTestSubsystem.generated.h"

class UHarvester;
class UResourceComponent;
class UResourceDropOff;
class UStaticMeshComponent;

UENUM()
enum class EEnemyHarvestTestPhase : uint8
{
	Inactive,
	WaitingForStart,
	WaitingForUnits,
	// Both harvesters work: the player-2 one autonomously, the player-1 one by a harvest order.
	Harvesting,
	// The player-2 harvester's field was drained (a fresh field was added first); it must move on by itself.
	WaitingForRetarget,
	// All radixite drained: the player-2 harvester runs out of work and must end up idle.
	WaitingForIdleAfterDrainAll,
	// A fresh field appeared while it was idle; its 10 s retry must find it.
	WaitingForRetry
};

/** One test harvester and what the run saw of it. */
struct FEnemyHarvestTestHarvester
{
	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<UHarvester> Harvester;
	uint8 OwningPlayer = 0;
	int32 MaxCargoSeen = 0;
	// Times its target drop-off belonged to another player (must stay 0).
	int32 WrongOwnerDropOffTicks = 0;
	FString WrongOwnerDropOffName;
	float SecondsInHarvestAnimation = 0.f;
	int32 ForcedAnimationFinishes = 0;
	uint8 LastLoggedStatus = MAX_uint8;
};

/**
 * @brief Map test of AI harvesting, started with -EnemyHarvestValidate on a map with radixite fields (default
 * RTS_ToCopySmall). Spawns a drop-off owned by player 2 and one owned by player 1 (plain actors with a
 * UResourceDropOff, crossed so each harvester's nearest drop-off belongs to the other player), a player-2 Pz I
 * harvester running UHarvester::StartAutonomousHarvest(Radixite) and a player-1 one under a normal harvest order.
 * Checks owner-filtered delivery, the player-2 stockpile, that the human's resources only follow player-1
 * deliveries, the enemy unit-pool filter, and that the player-2 harvester re-targets when its field runs dry.
 * Logs RTS_ENEMY_HARVEST_TEST ... RESULT PASS|FAIL; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UEnemyHarvestTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	void TickWaitingForStart();
	void TickWaitingForUnits();
	void TickHarvesting(float DeltaTime);
	void TickWaitingForRetarget(float DeltaTime);
	void TickWaitingForIdleAfterDrainAll(float DeltaTime);
	void TickWaitingForRetry(float DeltaTime);
	/** @return A new radixite field (same class as the test field) at the field plus Offset, or null. */
	UResourceComponent* SpawnFreshField(const FVector& Offset) const;
	void DrainField(UResourceComponent& Field) const;
	void DrainAllRadixite() const;
	bool GetIsEnemyHarvesterIdle() const;
	void TickExitPending();

	/** @return The radixite field the test is built around (closest to -EnemyHarvestAt=X,Y when given). */
	UResourceComponent* FindTestField() const;
	/**
	 * @brief Spawns a plain actor (cube, RTS component with the owner set before BeginPlay, UResourceDropOff for
	 * radixite) like an AI building that does not implement IResourceStorageOwner.
	 */
	UResourceDropOff* SpawnDropOffActor(const FVector& Location, uint8 OwningPlayer);
	AActor* SpawnHarvester(const FVector& Location, uint8 OwningPlayer) const;
	FEnemyHarvestTestHarvester MakeTestHarvester(AActor* HarvesterActor, uint8 OwningPlayer) const;

	void TrackHarvester(FEnemyHarvestTestHarvester& TestHarvester, float DeltaTime) const;
	void CheckAIPoolFilter();
	void DrainRetargetField();
	void FinishWithResults();

	int32 GetCargo(const FEnemyHarvestTestHarvester& TestHarvester) const;
	int32 GetHumanRadixite() const;
	int32 GetHumanRadixiteStorage() const;
	/** @return Radixite stored in all registered drop-offs of the player. */
	int32 GetStoredRadixiteOfPlayer(uint8 OwningPlayer) const;
	void StartGameAndUnpause() const;

	void EnterPhase(EEnemyHarvestTestPhase NewPhase);
	float GetPhaseSeconds() const;
	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	void Finish();

	EEnemyHarvestTestPhase M_Phase = EEnemyHarvestTestPhase::WaitingForStart;
	float M_PhaseStartSeconds = -1.f;
	int32 M_FailedCheckCount = 0;
	bool bM_IsExitPending = false;
	int32 M_ExitFramesLeft = 0;

	TWeakObjectPtr<UResourceComponent> M_Field;
	// The first field may be emptied (and destroyed) before fresh fields are spawned from it: keep its class and spot.
	UPROPERTY()
	TSubclassOf<AActor> M_FieldClass;
	FTransform M_FieldTransform = FTransform::Identity;
	TWeakObjectPtr<UResourceDropOff> M_EnemyDropOff;
	TWeakObjectPtr<UResourceDropOff> M_PlayerDropOff;
	FEnemyHarvestTestHarvester M_EnemyHarvester;
	FEnemyHarvestTestHarvester M_PlayerHarvester;

	// Baselines taken when both harvesters start.
	int32 M_HumanRadixiteBefore = 0;
	int32 M_HumanStoredBefore = 0;
	int32 M_EnemyStoredBefore = 0;

	// The player-2 harvester's field when it was drained, and the field it moved on to.
	TWeakObjectPtr<UResourceComponent> M_DrainedField;
	bool bM_CanTestRetarget = false;
	// Fresh fields are placed beside the test field, across the layout axis.
	FVector M_LayoutAxis = FVector::ForwardVector;
	TWeakObjectPtr<UResourceComponent> M_RetryField;
	float M_SecondsEnemyIdle = 0.f;
};
