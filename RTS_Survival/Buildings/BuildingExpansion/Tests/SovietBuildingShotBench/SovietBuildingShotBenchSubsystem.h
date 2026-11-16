// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "SovietBuildingShotBenchSubsystem.generated.h"

class AActor;
class UHealthComponent;

enum class ESovietBuildingShotBenchStep : uint8
{
	StartGame,
	Spawn,
	Settle,
	Shoot,
	Done
};

/** One spawned Soviet building and the reference shots it took. */
struct FSovietBuildingShotTarget
{
	FString Label;
	TWeakObjectPtr<AActor> Building;
	TWeakObjectPtr<UHealthComponent> Health;
	float MaxHealth = 0.f;
	int32 ShotsTaken = 0;
	bool bDestroyed = false;
};

/**
 * @brief Map test (-SovietBuildingShotBench): spawns every Soviet base building as the enemy and hits it with the
 * Pz IV KwK 40 APHE shell damage until it dies, logging "RTS_SBSB" shots-to-kill lines for the health balance.
 */
UCLASS()
class RTS_SURVIVAL_API USovietBuildingShotBenchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(float DeltaTime) override;
	// The map waits paused for the start button; the bench presses it, so it must tick while paused.
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	bool TryStartGame() const;
	void KeepGameRunning() const;
	void SpawnBuildings();
	void SpawnBuilding(const FString& BlueprintPath, const FVector& Location);
	void TickSettle();
	void TickShoot();
	void ShootTarget(FSovietBuildingShotTarget& Target) const;
	void Finish(const TCHAR* Outcome);

	ESovietBuildingShotBenchStep M_Step = ESovietBuildingShotBenchStep::StartGame;
	bool bM_IsActive = false;
	double M_StartRealSeconds = 0.0;
	float M_StepGameSeconds = 0.f;
	float M_LastShotGameSeconds = 0.f;
	float M_ShotDamage = 0.f;

	TArray<FSovietBuildingShotTarget> M_Targets;
};
