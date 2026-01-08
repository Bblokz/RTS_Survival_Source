// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/EnemyResources/EnemyResources.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFieldConstructionComponent/EnemyFieldConstructionComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyNavigationAIComponent/EnemyNavigationAIComponent.h"
#include "EnemyController.generated.h"


struct FAttackWaveElement;
enum class EEnemyWaveType : uint8;
class UEnemyWaveController;
struct FEnemyResources;
class UEnemyFormationController;
class UEnemyNavigationAIComponent;
class ATankMaster;
class ASquadController;

/**
 * @brief Coordinates enemy formations and wave orchestration for AI-controlled assaults.
 */
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
		const int32 MaxFormationWidth = 2,
		const float FormationOffsetMlt = 1.f);

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
	 * @param FormationOffsetMultiplier Multiplier for the formation offset when spawning units in formation.
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
		const float PerAffectingBuildingTimerFraction = 0.f,
		const float FormationOffsetMultiplier = 1.f);

	/**
	 * @brief Creates a one-off enemy attack wave with an optional delay before spawning.
	 * @param WaveType Determines wave logic and whether an owning actor is required.
	 * @param WaveElements Defines a unit type and spawn point.
	 * @param Waypoints The waypoints that the formation will move to.
	 * @param FinalWaypointDirection The final direction the formation will face when it reaches the last waypoint.
	 * @param MaxFormationWidth The maximum width of the formation that is spawned in units next to eachother.
	 * @param TimeTillWave Delay before starting the wave; zero or less starts immediately.
	 * @param WaveCreator The actor that spawns this wave needs to be RTS IsValid can be left empty depending on wave type.
	 * @param FormationOffsetMultiplier Multiplier for the formation offset when spawning units in formation.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void CreateSingleAttackWave(
		const EEnemyWaveType WaveType,
		const TArray<FAttackWaveElement>& WaveElements,
		const TArray<FVector>& Waypoints,
		const FRotator& FinalWaypointDirection,
		const int32 MaxFormationWidth,
		const float TimeTillWave,
		AActor* WaveCreator, const float FormationOffsetMultiplier = 1.f);

	/**
	 * @brief Assigns squads to construct field constructions at provided locations.
	 * @param SquadControllers Squads used to determine available field constructions.
	 * @param ConstructionLocations Locations to build at.
	 * @param AdditionalLocationsStrategy Strategy to fetch more locations when the order runs out.
	 * @param Strategy Desired strategy or None to pick one automatically.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void CreateFieldConstructionOrder(
		const TArray<ASquadController*>& SquadControllers,
		const TArray<FVector>& ConstructionLocations,
		const FFindAddtionalLocationsStrategy& AdditionalLocationsStrategy,
		const EFieldConstructionStrategy Strategy = EFieldConstructionStrategy::None);

	/**
	 * @brief Spawns squads for wave elements and issues a construction order once all spawns complete.
	 * @param WaveElements Units to spawn and their spawn locations.
	 * @param ConstructionLocations Locations to build at.
	 * @param AdditionalLocationsStrategy Strategy to fetch more locations when the order runs out.
	 * @param Strategy Desired strategy or None to pick automatically.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void CreateFieldConstructionOrderFromWaveElements(
		const TArray<FAttackWaveElement>& WaveElements,
		const TArray<FVector>& ConstructionLocations,
		const FFindAddtionalLocationsStrategy& AdditionalLocationsStrategy,
		const EFieldConstructionStrategy Strategy = EFieldConstructionStrategy::None);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetFieldConstructionOrderInterval(const float NewIntervalSeconds);


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

	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable)
	UEnemyNavigationAIComponent* GetEnemyNavigationAIComponent() const;

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

	UPROPERTY()
	TObjectPtr<UEnemyFieldConstructionComponent> M_FieldConstructionComponent;

	UPROPERTY()
	TObjectPtr<UEnemyNavigationAIComponent> M_EnemyNavigationAIComponent;

	TArray<TWeakObjectPtr<ATankMaster>> M_Tanks;
	TArray<TWeakObjectPtr<ASquadController>> M_Squads;

	bool GetIsValidFormationController() const;
	bool GetIsValidWaveController() const;
	bool GetIsValidFieldConstructionComponent() const;
	bool GetIsValidEnemyNavigationAIComponent() const;

	// Contains the supplies and other resource settings for waves and construction.
	FEnemyResources M_Resources;
};
