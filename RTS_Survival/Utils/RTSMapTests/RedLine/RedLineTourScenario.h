#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/B4HowitzerPit/Tests/B4HowitzerCaptureRig.h"

class AActor;
class ACPPTurretsMaster;
class APawn;
class ASquadUnit;
class URTSCoverFinderWorldSubsystem;
class UWorld;

enum class ERedLineTourPhase : uint8
{
	Inactive,
	WaitingForStart,
	// The garrison walks into the trenches; nothing else happens.
	Settling,
	// -RedLineAssault: a German tank platoon drives through the western lane.
	Assault,
	// One pillbox is destroyed and must leave its wreck behind.
	WaitingForPillboxWreck
};

/** One pillbox turret and the rounds it fired, counted from its weapons' magazines. */
struct FRedLineTurretWatch
{
	TWeakObjectPtr<AActor> Pillbox;
	TWeakObjectPtr<ACPPTurretsMaster> Turret;
	TArray<int32> LastMagazineCounts;
	int32 ShotCount = 0;
};

/**
 * @brief Plays RedLine unattended to check the line comes up whole (pillbox turrets, trench cover, garrison in the
 * trenches); -RedLineAssault then drives a German tank platoon at the western lane so the line has to fight, and
 * last one pillbox is destroyed and must leave its wreck.
 * Started with -RedLineTour; pictures with -RedLineCapture=<folder>. Logs RTS_REDLINE lines. Not in shipping.
 */
struct FRedLineTourScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem);

private:
	ERedLineTourPhase M_Phase = ERedLineTourPhase::Inactive;
	float M_PhaseStartSeconds = 0.f;
	float M_PhaseDeadlineSeconds = 0.f;
	float M_NextStatusSeconds = 0.f;
	float M_NextCaptureSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	bool bM_RunAssault = false;
	FString M_CaptureDirectory;
	FB4HowitzerCaptureRig M_CaptureRig;

	TArray<FRedLineTurretWatch> M_TurretWatches;
	TArray<TWeakObjectPtr<ASquadUnit>> M_GarrisonSoldiers;
	TArray<TWeakObjectPtr<APawn>> M_AssaultTanks;
	TArray<TWeakObjectPtr<AActor>> M_TrenchActors;
	TArray<TWeakObjectPtr<AActor>> M_GunBayActors;
	TWeakObjectPtr<AActor> M_B4Pit;
	FVector M_PillboxDeathLocation = FVector::ZeroVector;
	float M_WreckFoundSeconds = -1.f;

	void Check(bool bCondition, const TCHAR* Description, const FString& Details = FString());
	void EnterPhase(const UWorld& World, ERedLineTourPhase NewPhase, float TimeoutSeconds);
	void Finish(const UWorld& World);

	void TickWaitingForStart(UWorld& World);
	void TickSettling(UWorld& World, const URTSCoverFinderWorldSubsystem& CoverSubsystem);
	void TickAssault(UWorld& World);
	void KillOnePillbox(UWorld& World);
	void TickWaitingForPillboxWreck(UWorld& World);
	bool GetHasPillboxWreck(UWorld& World) const;

	// Counts the placed pieces of the line and checks they belong to the enemy.
	void InventoryLine(UWorld& World);
	void WatchPillboxTurrets(UWorld& World, int32& OutMountedTurrets);
	void CollectGarrisonSoldiers(UWorld& World);
	void CheckTrenchCover(const URTSCoverFinderWorldSubsystem& CoverSubsystem);
	void CheckGarrisonInTrenches();
	// Nobody watches an unattended run, so unseen soldiers would animate and move in coarse steps.
	void KeepGarrisonUpdating() const;
	void CountTurretShots();
	int32 GetTotalTurretShots(int32& OutTurretsThatFired) const;

	void SpawnAssault(UWorld& World);
	int32 GetLivingAssaultTanks() const;
	void ReportAssaultStatus(const UWorld& World);
	// Why a pillbox near the tanks does or does not fire: its target, range and aim.
	void ReportTurretStates() const;

	// Pictures of the whole line and of every kind of emplacement once the garrison has settled.
	void CaptureTour(UWorld& World);
	void CaptureAssault(UWorld& World);
	// The fog of war hides enemy units from the player; the pictures must show them.
	void RevealLine(UWorld& World) const;
};
