// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/EnemyResources/EnemyResources.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "EnemyController.generated.h"


struct FAttackWaveElement;
enum class EEnemyWaveType : uint8;
class UEnemyWaveController;
struct FEnemyResources;
class UEnemyFormationController;
class ATankMaster;
class ASquadController;

UCLASS()
class RTS_SURVIVAL_API AEnemyController : public AActorObjectsMaster
{
	GENERATED_BODY()


public:
	// Sets default values for this actor's properties
	AEnemyController(const FObjectInitializer& ObjectInitializer);

	// Assumes the wave supply was already paid for!
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void MoveFormationToLocation(
		const TArray<ASquadController*>& SquadControllers,
		const TArray<ATankMaster*>& TankMasters,
		const TArray<FVector>& Waypoints,
		const FRotator& FinalWaypointDirection,
		const int32 MaxFormationWidth = 2);

	/**
	 * 
	 * @param WaveType Determines wave logic: depending on actor? Time depending on generator buildings?
	 * @param WaveElements Defines a unit type and spawn point.
	 * @param WaveInterval The interval between the wave elements spawning.
	 * @param IntervalVarianceFraction Variance factor in [0.0 - 1.0) interval that determines the variance of the wave interval.
	 * Set to zero for no variance.
	 * @param Waypoints The waypoints that the formation will move to.
	 * @param FinalWaypointDirection The final direction the formation will face when it reaches the last waypoint.
	 * @param MaxFormationWidth The maximum width of the formation that is spawned in units next to eachother.
	 * @param bInstantStart Whether to start instantly or wait for the first timer.
	 * @param WaveCreator The actor that spawns this wave needs to be RTS IsValid can be left empty depending on wave type.
	 * @param WaveTimerAffectingBuildings The generator buildings checked for validity that influence the wave spawn timing.
	 * @param PerAffectingBuildingTimerFraction in (0.0 - 1.0) interval that determines
	 * the fraction of the wave interval that is added for each building generator that is destroyed.
	 * @note Possibly Leave WaveCreator and WaveTimerAffectingBuildings empty if the wave type does not depend on them.
	 * @note The wave handles wave supply logic; if a wave cannot spawn due to supply issues it will wait till the next wave iteration.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void CreateAttackWave(
		const EEnemyWaveType WaveType,
		const TArray<FAttackWaveElement>& WaveElements,
		const float WaveInterval,
		const float IntervalVarianceFraction,
		const TArray<FVector>& Waypoints,
		const FRotator& FinalWaypointDirection,
		const int32 MaxFormationWidth,
		const bool bInstantStart,
		AActor* WaveCreator,
		TArray<AActor*> WaveTimerAffectingBuildings,
		const float PerAffectingBuildingTimerFraction = 0.f);


	void DebugAllActiveFormations() const;

	// ------------------------------------------------------------
	// Enemy Resources Management
	// ------------------------------------------------------------
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	int32 GetEnemyWaveSupply() const
	{
		return M_Resources.WaveSupply;
	}

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetEnemyWaveSupply(const int32 NewSupply)
	{
		M_Resources.WaveSupply = NewSupply;
	}

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void AddToWaveSupply(const int32 AddSupply);

	// ------------------------------------------------------------
	// END Enemy Resources Management
	// ------------------------------------------------------------

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void PostInitializeComponents() override;

private:
	UPROPERTY()
	TObjectPtr<UEnemyFormationController> M_FormationController;

	UPROPERTY()
	TObjectPtr<UEnemyWaveController> M_WaveController;

	TArray<TWeakObjectPtr<ATankMaster>> M_Tanks;
	TArray<TWeakObjectPtr<ASquadController>> M_Squads;

	bool GetIsValidFormationController() const;
	bool GetIsValidWaveController() const;

	// Contains the supplies and other resource settings for waves and construction.
	FEnemyResources M_Resources;
};
