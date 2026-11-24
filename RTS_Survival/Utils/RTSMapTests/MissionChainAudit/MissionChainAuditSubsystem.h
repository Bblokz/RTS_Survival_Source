// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "MissionChainAuditSubsystem.generated.h"

class FMissionChainAuditLogCapture;
class UMissionBase;
struct FMapTestDamageReport;

/**
 * @brief Checks that the Blueprint missions of a two-player map still drive the enemy AI as before the four-player
 * refactor. Launch together with the map smoke test (which starts the game, lifts pauses and counts errors):
 * -game -RTSMapSmokeTest -RTSMapSmokeTestSeconds=<n> -RTSMissionChainAudit [-RTSMissionChainAuditDir=<dir>]
 * [-RTSMissionChainAuditStep=<s>]. Every step it fires the trigger of a waiting mission or completes the oldest
 * running one, so the map's whole mission chain runs its Blueprint logic (waves, spawns, wave supply, AI start).
 * Meanwhile it checks every unit's owner and its weapons' owners, the enemy controller's formation units, who damaged
 * whom, and collects the error lines; summary.md and a RTS_MCA RESULT line are written when the world ends.
 * Map tests only: the body compiles to nothing without RTS_WITH_MAP_TESTS.
 */
UCLASS()
class RTS_SURVIVAL_API UMissionChainAuditSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return false; }
	virtual bool IsTickableInEditor() const override { return false; }

#if RTS_WITH_MAP_TESTS

private:
	void TickMissionChain(float GameSeconds);
	/** @return Whether a step was taken: a trigger fired or a running mission completed. */
	bool TryAdvanceMissionChain(float GameSeconds);
	void RecordActiveMissions(float GameSeconds);
	void AuditUnits(float GameSeconds);
	void AuditUnit(AActor& Unit, int32 Owner, float GameSeconds);
	void AuditEnemyFormations(float GameSeconds);
	void OnDamageApplied(const FMapTestDamageReport& Report);
	/** @return Reasons the audit fails; empty for a pass. */
	TArray<FString> EvaluateVerdict() const;
	void WriteSummary(bool bIsFinal) const;
	void AddTimelineLine(float GameSeconds, const FString& Line);

	TSharedPtr<FMissionChainAuditLogCapture> M_LogCapture;
	FDelegateHandle M_DamageHandle;
	FString M_OutputDirectory;
	float M_StepSeconds = 30.f;
	float M_LastStepGameSeconds = -1.f;
	float M_LastUnitAuditGameSeconds = -1.f;
	float M_LastSummaryGameSeconds = -1.f;
	bool bM_IsActive = false;

	// Missions seen active, in the order they first appeared, and when.
	TArray<FString> M_MissionsSeen;
	TArray<FString> M_Timeline;
	TMap<TWeakObjectPtr<UMissionBase>, float> M_RunningSinceGameSeconds;
	int32 M_TriggersFired = 0;
	int32 M_MissionsCompleted = 0;

	// Units seen, by owner slot (0..MaxPlayers, last slot for every other value).
	TSet<TWeakObjectPtr<AActor>> M_UnitsSeen;
	TMap<TWeakObjectPtr<AActor>, float> M_FirstSeenGameSeconds;
	int32 M_UnitsByOwner[RTSPlayers::PlayerArraySize + 1] = {};
	TArray<FString> M_OwnerProblems;
	int32 M_WeaponOwnerMismatches = 0;
	TSet<TWeakObjectPtr<AActor>> M_UnitsWithWeaponsChecked;

	// Enemy controller: formation units of player 2 over time, and any that were not owned by player 2.
	int32 M_EnemyControllerCount = 0;
	int32 M_PeakEnemyFormationUnits = 0;
	TSet<TWeakObjectPtr<AActor>> M_EnemyFormationUnitsSeen;
	int32 M_EnemyFormationOwnerMismatches = 0;

	// [attacking player][victim's owner]; attacker slot 0 is damage no source claimed.
	float M_DamageMatrix[RTSPlayers::PlayerArraySize][RTSPlayers::PlayerArraySize] = {};
	int32 M_HitMatrix[RTSPlayers::PlayerArraySize][RTSPlayers::PlayerArraySize] = {};

#endif
};
