// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"

#include "UnitVariantTestSubsystem.generated.h"

class ACPPController;
class ARTSAsyncSpawner;
class UPlayerUnitVariantManager;

UENUM()
enum class EUnitVariantTestPhase : uint8
{
	Inactive,
	WaitingForStart,
	// VariantA is selected; a full GC runs at the end of the frame and must leave the variant brush alive.
	WaitingForGarbageCollection,
	// The spawner must now spawn the variant Blueprint.
	WaitingForVariantSpawn,
	// Back to Base: the next unit is the base Blueprint, the variant already spawned keeps its class.
	WaitingForBaseSpawn
};

/**
 * @brief Map test of the unit variant class swap, started with -UnitVariantValidate on any map with a player
 * controller and the async spawner. Needs a DA_UnitVariants entry with a VariantA for the tested tank
 * (-UnitVariantTank=<ETankSubtype value>, default Pz IV G). Logs RTS_UNIT_VARIANT_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UUnitVariantTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EUnitVariantTestPhase M_Phase = EUnitVariantTestPhase::WaitingForStart;
	float M_PhaseStartSeconds = -1.f;
	int32 M_FailedCheckCount = 0;
	FTrainingOption M_TestedTank;

	// The class the asset lists for VariantA and the class the spawner held before the test changed anything.
	TSoftClassPtr<AActor> M_VariantClass;
	TSoftClassPtr<AActor> M_BaseClass;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_SpawnedVariant;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_SpawnedBase;

	bool bM_HasVariantSpawnReturned = false;
	bool bM_HasBaseSpawnReturned = false;

	void Check(const bool bCondition, const FString& Description, const FString& Details = FString());
	void EnterPhase(const EUnitVariantTestPhase NewPhase);
	float GetPhaseSeconds() const;
	void Finish();

	void TickWaitingForStart();
	void TickWaitingForGarbageCollection();
	void TickWaitingForVariantSpawn();
	void TickWaitingForBaseSpawn();

	// Selects VariantA, checks the mapping and the training presentation, then requests a full GC.
	void StartVariantPhase();
	int32 M_GarbageCollectionFramesLeft = 0;
	void CheckTrainingPresentation(const FString& CheckName);
	void CheckSpawnedVariant(AActor* SpawnedTank);
	void StartBasePhase();
	void CheckSpawnedBase(AActor* SpawnedTank);

	// The hull mesh name of the spawned variant; the base tank must show a different one.
	FString M_VariantHullMeshName;

	/**
	 * @brief Requests one tank from the spawner next to the player's camera.
	 * @param SpawnID Tells the two spawns apart in the callback.
	 * @return False when the spawner refused the request.
	 */
	bool RequestTankSpawn(const int32 SpawnID);
	void OnTankSpawned(AActor* SpawnedActor, const int32 SpawnID);

	ACPPController* GetController() const;
	ARTSAsyncSpawner* GetAsyncSpawner() const;
	UPlayerUnitVariantManager* GetVariantManager() const;
};
