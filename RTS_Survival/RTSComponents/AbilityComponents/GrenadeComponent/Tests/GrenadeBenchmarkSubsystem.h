// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeAbilityTypes/GrenadeAbilityTypes.h"
#include "RTS_Survival/Utils/AOE/FRTS_AOE.h"
#include "GrenadeBenchmarkSubsystem.generated.h"

class ACPPController;
class AGrenadeActor;
class ASceneCapture2D;
class ASquadController;
class UGrenadeComponent;
class UTextureRenderTarget2D;
struct FGrenadeComponentSettings;

UENUM()
enum class EGrenadeBenchTarget : uint8
{
	Infantry,
	TankStill,
	// The tank drives past the throwers; tells whether sticky grenades still find it.
	TankMoving
};

UENUM()
enum class EGrenadeBenchPhase : uint8
{
	Inactive,
	WaitingForGameStart,
	WaitingForUnits,
	WaitingForTankApproach,
	WaitingForExplosions,
	WaitingForDamage,
	Done
};

/** One thrower squad against one target, played on its own spot of the navmesh. */
struct FGrenadeBenchScenario
{
	FString Name;
	const TCHAR* ThrowerClassPath = nullptr;
	EGrenadeAbilityType Grenade = EGrenadeAbilityType::DefaultGerBundleGrenade;
	EGrenadeBenchTarget Target = EGrenadeBenchTarget::Infantry;
	const TCHAR* TargetClassPath = nullptr;
	FVector Spot = FVector::ZeroVector;
	bool bHasSpot = false;

	// 2: an enemy squad throws at player units. 1: the player's squad throws at enemy units, as in a mission.
	uint8 ThrowerPlayer = 2;

	// What the tuned grenades must keep doing; negative values are not checked.
	float MinDealtPercent = -1.f;
	int32 MinSoldiersHurt = -1;
	bool bExpectAllStuck = false;
};

/** What one grenade of the running scenario did. */
struct FGrenadeBenchGrenade
{
	TWeakObjectPtr<AGrenadeActor> Grenade;
	float LandedGameSeconds = -1.f;
	float ExplodedGameSeconds = -1.f;
	FVector ExplosionLocation = FVector::ZeroVector;
	FString StuckTo;
	// The blast-radius warning was on screen while the fuse burned.
	bool bShowedFuseWarning = false;
};

/** A target actor and its health when the order was given. */
struct FGrenadeBenchVictim
{
	TWeakObjectPtr<AActor> Actor;
	FString Name;
	float HealthBefore = 0.f;
	// Updated every frame while the actor exists; dead soldiers are removed shortly after they die.
	float LastKnownHealth = 0.f;
	float MaxHealth = 0.f;
};

/**
 * @brief Benchmarks every grenade type the squads carry: AT grenades against light, medium and heavy tanks (standing
 * and driving), anti-infantry grenades and the molotov against a large squad. Logs damage, radius coverage, fuse
 * and sticking per scenario as RTS_GRENADE_BENCH lines and as CSV.
 * Started with -GrenadeBenchmark; -GrenadeBenchDir=<folder>, -GrenadeBenchOnly=<name part,...>,
 * -GrenadeBenchOverride="<GrenadeType>:<Setting>=<value>,...;..." tunes settings without saving Blueprints.
 * Not in a shipping build.
 */
UCLASS()
class RTS_SURVIVAL_API UGrenadeBenchmarkSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// The game boots paused behind the start widget; the benchmark has to tick to start it.
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	EGrenadeBenchPhase M_Phase = EGrenadeBenchPhase::Inactive;
	float M_PhaseStartGameSeconds = 0.f;
	double M_PhaseStartRealSeconds = 0.0;
	int32 M_FailedCheckCount = 0;
	bool bM_IsSuppressingErrorDialogs = false;
	FString M_OutputDirectory;
	FString M_CsvText;
	TArray<FString> M_SummaryLines;

	TArray<FGrenadeBenchScenario> M_Scenarios;
	int32 M_ScenarioIndex = INDEX_NONE;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_ThrowerSquad;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_TargetActor;

	UPROPERTY()
	TWeakObjectPtr<ASceneCapture2D> M_Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> M_RenderTarget = nullptr;

	// Grenade actors whose landing and explosion events this benchmark listens to.
	UPROPERTY()
	TArray<TWeakObjectPtr<AGrenadeActor>> M_BoundGrenades;

	TArray<FGrenadeBenchGrenade> M_ScenarioGrenades;
	TArray<FGrenadeBenchVictim> M_Victims;
	float M_UnitsReadyGameSeconds = -1.f;
	int32 M_ExpectedGrenades = 0;
	bool bM_HasSavedExplosionPicture = false;
	bool bM_HasSavedFusePicture = false;

	void EnterPhase(EGrenadeBenchPhase NewPhase);
	float GetGameSeconds() const;
	float GetGameSecondsInPhase() const;
	double GetRealSecondsInPhase() const;
	void Check(bool bCondition, const FString& Description);
	void Finish();

	void BuildScenarios();
	void ApplyScenarioFilter();
	bool AssignScenarioSpots(const FVector& BaseSpot);

	void TickWaitingForGameStart();
	void TickWaitingForUnits();
	void TickWaitingForTankApproach();
	void TickWaitingForExplosions();
	void TickWaitingForDamage();

	void StartNextScenario();
	bool SpawnScenarioUnits(const FGrenadeBenchScenario& Scenario);
	bool GetAreScenarioUnitsReady() const;
	bool GetHasGrenadeOnCard(EGrenadeAbilityType Grenade) const;
	UGrenadeComponent* GetThrowerGrenadeComponent() const;
	void ApplySettingOverrides(UGrenadeComponent& GrenadeComponent, EGrenadeAbilityType Grenade) const;
	void SnapshotVictims();
	void OrderThrow();
	FVector GetTargetAimPoint() const;
	void BindNewGrenades();
	void OnGrenadeLanded(AGrenadeActor* Grenade, AActor* StuckToVehicle);
	void OnGrenadeExploded(AGrenadeActor* Grenade, const FVector& Location);
	FGrenadeBenchGrenade* FindScenarioGrenade(const AGrenadeActor* Grenade);
	int32 GetExplodedGrenadeCount() const;
	// Notes which fusing grenades show their warning, and takes a picture halfway through the first fuse.
	void WatchFuseWarnings();

	void ReportScenario();
	void ReportInfantry(const FGrenadeBenchScenario& Scenario, const FGrenadeComponentSettings& Settings,
	                    const FString& GrenadeColumns);
	void ReportTank(const FGrenadeBenchScenario& Scenario, const FGrenadeComponentSettings& Settings,
	                const FString& GrenadeColumns);
	FString DescribeGrenades(const FGrenadeComponentSettings& Settings, int32& OutStuckCount,
	                         float& OutAverageFuse) const;

	/**
	 * @brief Smallest distance from any of the scenario's explosions to the actor's colliding components.
	 * @param Actor Victim.
	 * @param OutSource What the closest distance was measured to.
	 * @return Distance in cm, or a negative value when nothing exploded.
	 */
	float GetClosestExplosionDistance(const AActor& Actor, EAoeDistanceSource& OutSource) const;

	void SilenceTarget() const;
	static void SilenceSquad(ASquadController* Squad, bool bHoldInPlace);
	void TrackVictimHealth();
	static float GetVictimHealthAfter(const FGrenadeBenchVictim& Victim);
	static float GetActorHealth(const AActor* Actor);
	static const FGrenadeComponentSettings* GetSettings(const UGrenadeComponent& GrenadeComponent);
	void SavePicture(const FString& Label) const;
};
