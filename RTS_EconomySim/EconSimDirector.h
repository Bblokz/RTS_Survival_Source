// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "EconSimDirector.generated.h"

class ABuildingExpansion;
class ACPPController;
class AEnemyController;
class ANomadicVehicle;
class UHarvester;
class UHealthComponent;
class UPlayerResourceManager;
class UTrainerComponent;

enum class EEconSimPhase : uint8
{
	WaitForGame,
	WaitForHq,
	Running,
	Done
};

enum class EEconSimStepType : uint8
{
	// Queue a unit on the HQ trainer and wait for it to come out; it pays through the training queue.
	Train,
	// Drive a trained nomadic truck to a site and convert it into its building.
	Deploy,
	// Start an expansion preview on a building and place it; the placement click pays for it.
	Expand,
	// Press the expansion's Enable Conversion button.
	EnableConversion
};

/** One step of the scripted player build order; the steps run strictly in sequence. */
struct FEconSimStep
{
	EEconSimStepType Type = EEconSimStepType::Train;
	FTrainingOption Option;
	// Train: the role of the trained unit ("Harvester" or a building role). Others: the building role they act on.
	FName Role;
	// Train of a harvester: the field it is ordered onto. Deploy: the site it deploys at.
	FName Target;
	EBuildingExpansionType BxpType = EBuildingExpansionType::BXT_Invalid;
};

/** A cluster of spawned resource nodes that harvesters are ordered onto. */
struct FEconSimField
{
	FName Name;
	ERTSResourceType ResourceType = ERTSResourceType::Resource_None;
	FVector Centre = FVector::ZeroVector;
	TArray<TWeakObjectPtr<AActor>> Nodes;
	TArray<FString> NodeClasses;
};

/** Cargo sampling of one harvester; turns cargo changes into delivered amounts and round-trip times. */
struct FEconSimHarvester
{
	TWeakObjectPtr<AActor> Unit;
	TWeakObjectPtr<UHarvester> Harvester;
	FName Field;
	int32 Index = 0;
	double SpawnSeconds = 0.0;
	int32 LastCargo = 0;
	int32 Capacity = 0;
	ERTSResourceType LastResource = ERTSResourceType::Resource_None;
	double LastEmptiedSeconds = -1.0;
	int32 Drops = 0;
	double CycleSecondsSum = 0.0;
	int32 Cycles = 0;
	int32 DeliveredRadixite = 0;
	int32 DeliveredMetal = 0;
	int32 DeliveredOther = 0;
};

/** The expansion being placed: the preview starts first, the placement click follows once it is loaded. */
struct FEconSimPendingExpansion
{
	TWeakObjectPtr<ANomadicVehicle> Building;
	TWeakObjectPtr<ABuildingExpansion> Expansion;
	int32 SlotIndex = INDEX_NONE;
	bool bPreviewStarted = false;
	bool bPlaced = false;
};

/** A what-if converter rate applied to the live converter at runtime (-EconSimConverter / -EconSimSmelter). */
struct FEconSimConverterOverride
{
	bool bActive = false;
	int32 InputPerTick = 0;
	int32 OutputPerTick = 0;
	float TickSeconds = 0.f;
};

/** Health protection of one player actor, so enemy attacks cannot cut the measured income short. */
struct FEconSimProtectedActor
{
	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<UHealthComponent> Health;
};

/**
 * @brief Spawned into the game world by the RTS_EconomySim module when -EconSim is on the command line (see
 * Source/Tools/EconomySim/run_economy_sim.ps1). Starts the game, lays out a player base with resource fields, runs a
 * scripted build order (harvesters, refinery + converter, metal vault + smelter) through the real training queue and
 * expansion payment paths against the map's default enemy AI, and writes timeline / events / enemy CSVs and a summary
 * to Saved/EconomySim/<timestamp>/ before it exits the process.
 */
UCLASS(NotPlaceable, Transient)
class AEconSimDirector : public AActor
{
	GENERATED_BODY()

public:
	AEconSimDirector();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// ---- Configuration (command line) ----
	void ReadCommandLine();
	void BuildScenario();

	// ---- Start of the game ----
	bool TryStartGame();
	void KeepGameRunning() const;
	void SetupScenario();
	void Setup_ClearPreplacedUnits();
	void Setup_SpawnEnemyResearchCentre();
	void Setup_SpawnResourceFields();
	void Setup_SpawnHq();
	void TickWaitForHq();
	void GrantStartResources() const;

	// ---- Build order ----
	void TickBuildOrder();
	void AdvanceStep(const FString& Note);
	bool TickStep_Train(const FEconSimStep& Step);
	bool TickStep_Deploy(const FEconSimStep& Step);
	bool TickStep_Expand(const FEconSimStep& Step);
	bool TickStep_EnableConversion(const FEconSimStep& Step);
	void ApplyConverterOverride(class UResourceConverterComponent* Converter);
	bool QueueOnHqTrainer(const FTrainingOption& Option);
	bool TryClaimTrainedUnit(const FEconSimStep& Step);
	void OrderHarvest(AActor* Unit, const FName FieldName) const;
	AActor* FindBestNode(const FName FieldName) const;
	bool TryStartExpansionPreview(ANomadicVehicle* Building, EBuildingExpansionType BxpType);
	bool TryPlaceExpansion();
	bool AimPreviewAtSlot(ANomadicVehicle* Building, EBuildingExpansionType BxpType) const;
	bool TickForceFinishDeploy(ANomadicVehicle* Truck);

	// ---- Measurement ----
	void OnActorSpawned(AActor* SpawnedActor);
	void ProcessNewActors();
	void RegisterPlayerActor(AActor* Actor);
	void RegisterEnemyUnit(AActor* Actor);
	void SampleHarvesters();
	void SampleOneHarvester(FEconSimHarvester& Track);
	void KeepHarvestersOnTheirFields();
	// Writes harvester_state.csv: where each harvester is, what it carries and whether its move is running, so a
	// harvester that never delivers can be told apart (stuck, idle, never reached its node).
	void LogHarvesterStates() const;
	void StarveMissionWaves() const;
	void ProjectEnemyBasePointsToNavmesh(class UEnemyStrategicAIComponent& Strategic);
	void SilenceMissions() const;
	void ProtectPlayerActors();
	void RunResourceSink();
	void UpdateEnemyPressure();
	float GetEnemyIncomeForMinute(const float Minute) const;
	void WriteTimelineRow();
	int32 CountEnabledConverters() const;
	void LogEvent(const FString& Kind, const FString& Detail);

	// ---- Output ----
	void OpenOutputFiles();
	void AppendLine(const FString& FileName, const FString& Line) const;
	void WriteContextFile() const;
	void WriteSummary() const;
	FString DescribeConverters() const;
	FString DescribeEnemyTraining() const;
	void Finish(const FString& Reason);

	// ---- Helpers ----
	double GetGameSeconds() const;
	FVector BaseOffset(const FVector2D& Offset) const;
	FVector GroundAt(const FVector& Location) const;
	FVector NavPointNear(const FVector& Location) const;
	ACPPController* GetController() const;
	UPlayerResourceManager* GetResources() const;
	UTrainerComponent* GetHqTrainer() const;
	ANomadicVehicle* GetBuilding(const FName RoleName) const;

	// Settings from the command line; see ReadCommandLine for the switches.
	float M_SimMinutes = 30.f;
	float M_SampleSeconds = 10.f;
	float M_TimeDilation = 1.f;
	bool bM_KeepPreplacedUnits = false;
	bool bM_ProtectPlayer = true;
	bool bM_EnemyResearchCentre = true;
	bool bM_SinkResources = true;
	// The UT map's mission floods waves for its stress test; starve them so only the strategic AI trains.
	bool bM_StarveMissionWaves = true;
	// <0: follow the survival Normal curve; 0: keep the map's value; >0: a fixed training income per minute.
	float M_EnemyIncomePerMinute = -1.f;
	double M_LastEnemyPressureSeconds = -1000.0;
	double M_LastHarvestOrderCheckSeconds = -1000.0;
	double M_LastHarvesterStateLogSeconds = -1000.0;
	double M_LastDeployOrderSeconds = -1000.0;
	int32 M_HarvestReorders = 0;
	// Where the map's mission spawns its waves; an enemy unit that appears next to one came from a wave.
	TArray<FVector> M_MissionSpawnPoints;
	// South of this Y the map's test T-34 drop delivers its tanks (UT_EconomySim's south edge); -EconSimMissionLaneY.
	float M_MissionLaneY = -20000.f;
	int32 M_MissionWaveUnitsRemoved = 0;
	// Test-only workaround so the strategic AI's paid training batches can spawn (-EconSimRawEnemyBasePoints: off).
	bool bM_FixEnemyBasePoints = true;
	FEconSimConverterOverride M_ConverterOverride;
	FEconSimConverterOverride M_SmelterOverride;
	int32 M_BasePointsProjected = 0;
	float M_ProtectDamageMlt = 0.02f;
	FIntVector M_StartResources = FIntVector(800, 300, 0);
	FVector M_Base = FVector::ZeroVector;
	bool bM_HasBaseOverride = false;
	FString M_OutputDirectory;

	EEconSimPhase M_Phase = EEconSimPhase::WaitForGame;
	double M_StartRealSeconds = 0.0;
	double M_SimStartGameSeconds = -1.0;
	double M_LastSampleSeconds = -1000.0;
	double M_LastHarvesterSampleSeconds = -1000.0;
	double M_LastProtectSeconds = -1000.0;
	double M_LastSinkSeconds = -1000.0;
	double M_StepStartSeconds = 0.0;
	double M_BuildOrderDoneSeconds = -1.0;
	double M_DeployConversionStartSeconds = -1.0;

	TArray<FEconSimStep> M_BuildOrder;
	int32 M_StepIndex = 0;
	bool bM_StepQueued = false;

	TMap<FName, FVector2D> M_SiteOffsets;
	TArray<FEconSimField> M_Fields;
	TMap<FName, TWeakObjectPtr<AActor>> M_Roles;
	TArray<FEconSimHarvester> M_Harvesters;
	FEconSimPendingExpansion M_PendingExpansion;

	// Actors spawned since the last tick; their owner is only reliable once they began play.
	TArray<TWeakObjectPtr<AActor>> M_NewActors;
	TSet<TWeakObjectPtr<AActor>> M_SeenActors;
	TArray<FEconSimProtectedActor> M_ProtectedActors;
	TArray<TWeakObjectPtr<AActor>> M_EnemyUnits;
	int32 M_EnemyTanksSpawned = 0;
	int32 M_EnemySquadsSpawned = 0;
	int32 M_EnemyTanksPreplaced = 0;
	int32 M_EnemySquadsPreplaced = 0;

	// Amounts moved out of storage into a virtual bank, so full storage never blocks harvesting or conversion.
	int64 M_BankRadixite = 0;
	int64 M_BankMetal = 0;
	int64 M_BankVehicleParts = 0;
	// Health the protection restored: what the enemy would have taken off the player's units and buildings.
	double M_DamageAbsorbed = 0.0;

	FDelegateHandle M_ActorSpawnedHandle;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TWeakObjectPtr<ANomadicVehicle> M_Hq;
};
