// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"

#include "TechBatchTwoTestSubsystem.generated.h"

class ACPPController;
class ASquadController;
class ATankMaster;
class UTechnologyEffect;

enum class ETechBatchTwoTestPhase : uint8
{
	Inactive,
	WaitingForGameStart,
	WaitingForUnits,
	WaitingForEffects,
	FlakModeOn,
	FlakModeOff,
	BailingOut,
	Done
};

/** What the test measured on a unit before any technology was researched. */
struct FTechBatchTwoBaseline
{
	float MainGunPenetration = 0.f;
	float MainGunPenetrationMaxRange = 0.f;
	float MainGunRange = 0.f;
	int32 MainGunAccuracy = 0;
	float MainGunReload = 0.f;
	// Sum of the stats of every weapon below the main gun's calibre (machine guns), which must not change.
	float SecondaryWeaponsSignature = 0.f;
	float ArmorSum = 0.f;
	float SoldierMaxHealth = 0.f;
	float ConstructionDuration = 0.f;
};

/**
 * @brief Validates the second batch of German technologies in a game run (-TechBatchTwoValidate): spawns one unit
 * of every affected family through the player's async spawner, researches the eight techs through
 * UPlayerTechManager (or applies their TEBPs directly when the controller map lacks them, which it reports) and
 * asserts every effect, including reinforcement re-inits, the Flak mode toggle and half-track bail-outs.
 * Tools/TechSetup/run_tech_test.sh is the runner. Not created in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UTechBatchTwoTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	ETechBatchTwoTestPhase M_Phase = ETechBatchTwoTestPhase::Inactive;
	double M_PhaseStartRealSeconds = 0.0;
	float M_PhaseStartGameSeconds = 0.f;
	int32 M_PassedChecks = 0;
	int32 M_FailedChecks = 0;
	bool bM_IsSuppressingErrorDialogs = false;
	bool bM_UsedDirectApplication = false;
	// Game seconds into WaitingForUnits at which every unit first reported ready; -1 until then.
	float M_UnitsReadyAtPhaseSeconds = -1.f;
	FVector M_TestOrigin = FVector::ZeroVector;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	// Spawned test units by label; filled by the async spawner callbacks.
	TMap<FString, TWeakObjectPtr<AActor>> M_Units;
	TMap<FString, FTechBatchTwoBaseline> M_Baselines;
	// Effects created for direct application; kept alive for the run.
	UPROPERTY()
	TArray<TObjectPtr<UTechnologyEffect>> M_DirectEffects;

	void TickWaitingForGameStart();
	void TickWaitingForUnits();
	void TickWaitingForEffects();
	void TickFlakModeOn();
	void TickFlakModeOff();
	void TickBailingOut();

	void EnterPhase(ETechBatchTwoTestPhase NewPhase);
	double GetRealSecondsInPhase() const;
	float GetGameSecondsInPhase() const;
	bool TryStartGame();
	void KeepGameRunning() const;

	void RequestTestUnits();
	void RequestUnit(const FString& Label, bool bIsTank, uint8 Subtype, const FVector& Offset);
	bool GetAreAllUnitsReady() const;
	ATankMaster* GetTank(const FString& Label) const;
	ASquadController* GetSquad(const FString& Label) const;

	void RecordBaselines();
	FTechBatchTwoBaseline MeasureUnit(const FString& Label) const;
	void ResearchTechnologies();
	/** @return False when the controller has no TEBP for the tech, so it was applied directly instead. */
	bool ResearchThroughTechManager(ETechnology Technology);
	void ApplyTechnologyDirectly(ETechnology Technology, const FString& EffectClassPath);

	void CheckWeaponTechs();
	void CheckArmoredTroopBay();
	void CheckSquadHealthTechs();
	void CheckReinforcementKeepsHealth();
	void CheckEntrenchingTools();
	bool StartFlakGroundRole();
	void CheckFlakGroundRole(bool bExpectActive);
	bool StartBailOut();
	void CheckBailOut();

	void Check(bool bCondition, const FString& Name, const FString& Detail);
	void CheckRatio(const FString& Name, float Before, float After, float ExpectedRatio);
	void Finish();
};
