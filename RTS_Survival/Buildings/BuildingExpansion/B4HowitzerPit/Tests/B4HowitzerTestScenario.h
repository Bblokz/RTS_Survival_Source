#pragma once

#include "CoreMinimal.h"
#include "B4HowitzerCaptureRig.h"
#include "RTS_Survival/Buildings/BuildingExpansion/GunPit/Tests/GunPitFireReactionWatch.h"
#include "RTS_Survival/Buildings/BuildingExpansion/GunPit/Tests/GunPitRunnerWatch.h"

class AActor;
class ABuildingExpansion;
class ANomadicVehicle;
class ASquadController;
class ATeamWeapon;
class UB4HowitzerPitCrewComponent;
class URTSCoverFinderWorldSubsystem;
class USceneComponent;
class USkeletalMeshComponent;
class UWorld;

enum class EB4HowitzerTestPhase : uint8
{
	Inactive,
	WaitingForStart,
	WaitingForSquadDeployed,
	WaitingForMoveDone,
	WaitingForSquadShots,
	WaitingForPitTurret,
	WaitingForPitShots,
	WaitingForPitDestroyed,
	// -B4HowitzerObserve: the pit and the squad were placed in the map (UT_B4Howitzer); nothing is spawned.
	ObservingPlaced,
	// -B4HowitzerPlayerBuild: the German player builds the captured pit from the HQ, as with a click on the option.
	PlayerWaitingForHq,
	PlayerWaitingForBxpLoaded,
	PlayerWaitingForConstruction,
	PlayerWaitingForBuiltPitDrill
};

/** The montage a mesh played last frame, so the scenario can log every montage that starts. */
struct FB4HowitzerMontageWatch
{
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	FString Label;
	FName LastMontageName = NAME_None;
	// When the current montage started and how long it should take at its play rate.
	float MontageStartSeconds = 0.f;
	float ExpectedMontageSeconds = 0.f;
	bool bIsPlayingReversed = false;
};

/** Closest a crew member's hands came to the shell during one loading drill. */
struct FB4HowitzerCrewReach
{
	float ClosestHandToShell = TNumericLimits<float>::Max();
	float SecondsAtClosest = 0.f;
};

/**
 * @brief Which pit the scenario spawns: the B-4 pit, or a pit built from it (-B4HowitzerPit=ML20 for the ML-20 pit;
 * -B4HowitzerConcretePit for either one's concrete-apron Blueprint),
 * so one run checks another gun on the same crane drill with its own turret, reload time and wreck.
 */
struct FB4HowitzerPitVariant
{
	FString PitClassPath;
	// Part of the class name of the turret the pit spawns as its child actor.
	FString TurretClassMarker;
	// Part of the class name of the gun wreck the pit leaves.
	FString GunWreckMarker;
	// Part of the name of the gun's reload montage; every shot starts it.
	FString ReloadMontageMarker;
	float ReloadSeconds = 0.f;
	// The building mesh the pit must show: the earth emplacement, or the concrete apron of the concrete pits.
	FName EmplacementMeshName = TEXT("SM_B4Pit_Emplacement");
	// The concrete pits' ammo runner, who rides PitOrigin and stocks the shell rack; no RunnerTag on the earth pits.
	// His shell never hides, so only its pick-up and lay-down places are checked.
	FGunPitRunnerWatch Runner;
};

/**
 * @brief Spawns the B-4 team weapon squad and the B-4 howitzer pit next to the player's camera and follows them:
 * the squad loads, deploys, packs, moves, deploys again and fires on an attack-ground order; the pit attaches its
 * crew and props to the gun, fires and plays the crane loading drill stretched to its reload time.
 * Started with -B4HowitzerValidate (on TestCover); logs RTS_B4_TEST lines and quits when done. Not in shipping.
 */
struct FB4HowitzerTestScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem);

private:
	EB4HowitzerTestPhase M_Phase = EB4HowitzerTestPhase::Inactive;
	float M_PhaseStartSeconds = 0.f;
	float M_PhaseDeadlineSeconds = 0.f;
	int32 M_FailedCheckCount = 0;

	FVector M_SquadSpawnLocation = FVector::ZeroVector;
	FVector M_PitSpawnLocation = FVector::ZeroVector;
	FVector M_MoveDestination = FVector::ZeroVector;
	FVector M_GroundTarget = FVector::ZeroVector;
	FVector M_PitGroundTarget = FVector::ZeroVector;

	TWeakObjectPtr<ASquadController> M_Squad;
	TWeakObjectPtr<ATeamWeapon> M_TeamWeapon;
	TWeakObjectPtr<AActor> M_PitBxp;
	TWeakObjectPtr<AActor> M_PitTurret;
	TWeakObjectPtr<UB4HowitzerPitCrewComponent> M_PitCrew;
	TWeakObjectPtr<USkeletalMeshComponent> M_PitGunMesh;
	TWeakObjectPtr<USceneComponent> M_PitShell;
	TArray<TWeakObjectPtr<USkeletalMeshComponent>> M_PitCrewMeshes;

	TArray<FB4HowitzerMontageWatch> M_MontageWatches;
	TArray<float> M_SquadShotSeconds;
	TArray<float> M_PitShotSeconds;
	FGunPitFireReactionWatch M_PitFireReaction;
	TArray<FB4HowitzerCrewReach> M_CrewReach;
	bool bM_HasLeftDeployedForMove = false;
	bool bM_WasPitLoading = false;
	float M_PitLoadingStartSeconds = 0.f;
	float M_NextStatusSeconds = 0.f;
	float M_NextCaptureSeconds = 0.f;
	FVector M_PitDeathLocation = FVector::ZeroVector;
	bool bM_IsKillingPit = false;
	bool bM_ObservePlaced = false;
	bool bM_PlayerBuild = false;
	FB4HowitzerPitVariant M_PitVariant;
	// -B4HowitzerSquadReload=<s> when -B4HowitzerSquad runs another team weapon than the B-4.
	float M_SquadReloadSeconds = 0.f;
	TWeakObjectPtr<ANomadicVehicle> M_Hq;
	TWeakObjectPtr<ABuildingExpansion> M_PlayerPit;
	int32 M_HqSlotIndex = INDEX_NONE;
	float M_PlayerPitConstructionStartSeconds = 0.f;
	FB4HowitzerCaptureRig M_CaptureRig;
	FString M_CaptureDirectory;
	bool bM_WasSquadReloading = false;

	void Check(bool bCondition, const TCHAR* Description, const FString& Details = FString());
	void EnterPhase(const UWorld& World, EB4HowitzerTestPhase NewPhase, float TimeoutSeconds);
	bool FinishWhenTimedOut(const UWorld& World, const TCHAR* ExpectedEvent);
	void Finish(const UWorld& World);

	bool ChooseSpawnLocations(UWorld& World);
	void SpawnSquad(UWorld& World);
	void SpawnPit(UWorld& World);
	void FindTeamWeapon(UWorld& World);
	bool FindPitTurret(UWorld& World);
	void CheckPitAttachments();

	// Nobody sees an unattended run; meshes skip their pose and never advance montages unless forced.
	void KeepMeshesAnimating() const;
	// Stray shells from far fights must not end the test by killing its crew; unseen soldiers must still reach
	// their crew positions.
	void KeepTestUnitsAliveAndMoving() const;
	void WatchMontages(const UWorld& World);
	void SampleCrewReach(const UWorld& World);
	void ReportCrewReach();
	void ReportSquadStatus(const UWorld& World);
	void ReportPitStatus(const UWorld& World);

	void TickWaitingForStart(UWorld& World);
	void TickWaitingForSquadDeployed(UWorld& World);
	void TickWaitingForMoveDone(UWorld& World);
	void TickWaitingForSquadShots(UWorld& World);
	void TickWaitingForPitTurret(UWorld& World);
	void TickWaitingForPitShots(UWorld& World);
	void TickWaitingForPitDestroyed(UWorld& World);
	void StartObservingPlaced(UWorld& World);
	void TickPlayerWaitingForHq(UWorld& World);
	void TickPlayerWaitingForBxpLoaded(UWorld& World);
	void TickPlayerWaitingForConstruction(UWorld& World);
	void TickPlayerWaitingForBuiltPitDrill(UWorld& World);
	// Unattended runs never draw the truck, so its unfold montage never ends by itself.
	void KeepHqUnfolding() const;
	// Clicks the captured pit's build option on the HQ; false (with a failed check) when it is not offered.
	bool ClickCapturedPitOption(UWorld& World);
	bool FindBuildSpotNearHq(UWorld& World, FVector& OutLocation) const;
	void TickObservingPlaced(UWorld& World);
	// Times the loading drill and checks it against the reload; shared by the spawned and the placed pit.
	void TrackPitLoading(const UWorld& World);
	// The gun bucks after the first pit shot. The whole crew starts the crane drill at the shot, so their brace is
	// only logged.
	void WatchPitFireReaction(const UWorld& World);
	void WatchPitRunner(const UWorld& World);
	void KillPit(const UWorld& World);
	bool GetHasWreckNear(UWorld& World, const TCHAR* ClassMarker) const;
	void CheckFinishedMontage(const UWorld& World, const FB4HowitzerMontageWatch& Watch);

	// Pictures every capture interval of the phase that is running; nothing happens without -B4HowitzerCapture.
	void TickCaptures(UWorld& World);
	void RevealTestActors() const;
};
