#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/B4HowitzerPit/Tests/B4HowitzerCaptureRig.h"
#include "GunPitFireReactionWatch.h"
#include "GunPitSectorArcWatch.h"

class AActor;
class ACPPTurretsMaster;
class UGunPitCrewComponent;
class URTSCoverFinderWorldSubsystem;
class USceneComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UWorld;

enum class EGunPitTestPhase : uint8
{
	Inactive,
	WaitingForStart,
	WaitingForTurrets,
	WaitingForFirstRounds,
	// Sector pits only: the gun is ordered onto a target outside its arc and must hold at the edge without firing.
	HoldingOutsideSector,
	Observing
};

/** One German gun pit under test and what was seen of its drill. */
struct FGunPitUnderTest
{
	FString Label;
	FString ClassPath;
	FName PitMeshName;
	// Where the loader stands in the gun's PitOrigin space; he must ride along when the gun turns.
	FVector LoaderSpot = FVector::ZeroVector;
	// The camera views are sized for the howitzer pit; the Pak pit is smaller.
	float ViewScale = 1.f;
	// Owner as placed (1 player, 2 enemy) and what the pit carries.
	int32 ExpectedOwner = 1;
	// Gun socket the crew, crate and rounds ride on.
	FName PitOriginSocket = TEXT("PitOrigin");
	int32 ExpectedCrewCount = 4;
	bool bCarriesRounds = true;
	bool bHasReadyCrate = true;
	// A sector pit's gun only traverses this far either side of the pit's forward; zero turns a full circle.
	float SectorHalfDegrees = 0.f;

	TWeakObjectPtr<AActor> Pit;
	TWeakObjectPtr<ACPPTurretsMaster> Turret;
	TWeakObjectPtr<UGunPitCrewComponent> Crew;
	TWeakObjectPtr<USkeletalMeshComponent> GunMesh;
	TArray<TWeakObjectPtr<USkeletalMeshComponent>> CrewMeshes;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> Rounds;

	FVector SpawnLocation = FVector::ZeroVector;
	FVector GroundTarget = FVector::ZeroVector;
	TArray<float> ShotSeconds;
	int32 LastMagazine = INDEX_NONE;

	// Drill events seen on the round in flight, in world seconds; negative until seen.
	float DrillStartSeconds = -1.f;
	float DrillEndSeconds = -1.f;
	float HandoverSeconds = -1.f;
	float RamSeconds = -1.f;
	float PickSeconds = -1.f;
	// How far the round sits from the hand that took it; the drill bake aims for the palms.
	float HandoverHandDistance = -1.f;
	float PickHandDistance = -1.f;
	TWeakObjectPtr<UStaticMeshComponent> RoundAtDrillStart;
	TWeakObjectPtr<USceneComponent> HandlerAtDrillStart;
	TWeakObjectPtr<USceneComponent> LoaderAtHandover;
	TWeakObjectPtr<USceneComponent> HolderOfNextRound;
	float NextCaptureSeconds = 0.f;
	// Box-swap drill (machine-gun pit): the loader moves the gun's AmmoBox and SpareTop bones by hand.
	float ClosestHandToAmmoBox = TNumericLimits<float>::Max();
	float ClosestHandToSpareBox = TNumericLimits<float>::Max();
	float AmmoBoxTravel = 0.f;
	FVector AmmoBoxStart = FVector::ZeroVector;
	bool bGunReloadMontageSeen = false;
	bool bGunnerFireMontageSeen = false;
	bool bWasLoading = false;
	bool bDrillChecked = false;
	bool bTraverseChecked = false;
	FGunPitFireReactionWatch FireReaction;
	FGunPitSectorArcWatch SectorArc;
};

/**
 * @brief Spawns the four German gun pits (Pak 40, leFH 18, sFH 18, Flak 37) as the player's, orders each to attack the
 * ground off its forward and checks the pit: crew, crate and rounds ride on the gun's PitOrigin at 1:1 scale, the
 * whole gun and its crew turn to the target, the gun fires, and one loading drill moves the rounds from the
 * handler's hands to the loader's and into the breech while the handler picks up the next round.
 * Started with -GerPitValidate (on TestCover); -DShKPitValidate runs the Soviet DShK pit instead (enemy owned, two
 * crew, a box-swap drill instead of rounds); -At53KPitValidate the Soviet 53-K pit (the Pak 40 pit's drill); -BS3PitValidate
 * the Soviet BS-3 pit (the Pak drill turned to fit between the trails); -At53KSectorPitValidate the 53-K sector pit, whose
 * gun must first hold at its 45 degree limit without firing on a target outside its arc. Pictures with
 * -GerPitCapture=<folder>. Not in shipping.
 */
struct FGunPitTestScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem);

private:
	EGunPitTestPhase M_Phase = EGunPitTestPhase::Inactive;
	float M_PhaseStartSeconds = 0.f;
	float M_PhaseDeadlineSeconds = 0.f;
	float M_NextStatusSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	TArray<FGunPitUnderTest> M_Pits;
	FString M_CaptureDirectory;
	FB4HowitzerCaptureRig M_CaptureRig;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	void EnterPhase(const UWorld& World, EGunPitTestPhase NewPhase, float TimeoutSeconds);
	void Finish(const UWorld& World);

	void TickWaitingForStart(UWorld& World);
	void TickWaitingForTurrets(UWorld& World);
	void TickWaitingForFirstRounds(UWorld& World);
	void TickHoldingOutsideSector(UWorld& World);
	void TickObserving(UWorld& World);

	bool SpawnPits(UWorld& World);
	bool FindTurret(FGunPitUnderTest& Pit) const;
	void CheckAttachments(FGunPitUnderTest& Pit);
	void OrderAttackGround(UWorld& World, FGunPitUnderTest& Pit, float YawOffsetDegrees);
	// Checks the hold at the arc's edge and the target search's arc filter, then orders a target inside the arc.
	void CheckSectorArc(UWorld& World, FGunPitUnderTest& Pit);
	float GetGunYawFromPitForward(const FGunPitUnderTest& Pit) const;
	void CountShots(const UWorld& World, FGunPitUnderTest& Pit) const;
	void WatchDrill(UWorld& World, FGunPitUnderTest& Pit);
	void CheckDrill(FGunPitUnderTest& Pit);
	void WatchBoxDrill(UWorld& World, FGunPitUnderTest& Pit);
	void SampleBoxDrill(FGunPitUnderTest& Pit) const;
	void CheckBoxDrill(FGunPitUnderTest& Pit);
	// Enemy pits sit under the fog of war; the pictures must show them.
	void RevealPit(const FGunPitUnderTest& Pit) const;
	void LogHandover(const FGunPitUnderTest& Pit, const UStaticMeshComponent& Round,
	                 const USkeletalMeshComponent& Loader) const;
	void CheckTraverse(FGunPitUnderTest& Pit);
	// The gun bucks and the crew brace after the first shot (gun pit anim and crew anim instances).
	void WatchFireReaction(UWorld& World, FGunPitUnderTest& Pit);
	bool GetIsPitDone(const FGunPitUnderTest& Pit) const;

	// Unattended runs never draw the pits; their meshes would skip the pose and never play montages.
	void KeepMeshesAnimating() const;
	void ReportStatus(const UWorld& World);
	void CapturePit(UWorld& World, const FGunPitUnderTest& Pit, const FString& Moment);
};
