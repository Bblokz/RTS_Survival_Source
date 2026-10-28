#pragma once

#include "CoreMinimal.h"

class ACameraActor;
class ASquadController;
class ASquadUnit;
class URTSCoverFinderWorldSubsystem;
class UWorld;
enum class EAbilityID : uint8;

enum class ESquadMarchTestPhase : uint8
{
	Inactive,
	WaitingForCoverScan,
	FindingSquad,
	MarchWaitingForStride,
	PatrolWaitingForStride
};

/**
 * @brief Orders a player squad to march and checks the slower walk and the looping march montage, then hurts
 * one soldier and checks the whole squad drops the march to idle with its speed and locomotion back. Does the
 * same hurt-to-idle check for a patrol. Started with -SquadMarchValidate; logs RTS_MARCH_TEST lines and saves
 * pictures of the marching squad to Saved/SquadMarchTest. Not part of a shipping build.
 */
struct FSquadMarchTestScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem);

private:
	TWeakObjectPtr<ASquadController> M_Squad;
	TWeakObjectPtr<ACameraActor> M_Camera;
	// Walk speed of each soldier before the march, to check the march slows them and gives the speed back.
	TMap<TWeakObjectPtr<ASquadUnit>, float> M_BaseWalkSpeeds;
	FVector M_SquadStartLocation = FVector::ZeroVector;
	FVector M_MarchDestination = FVector::ZeroVector;
	float M_PhaseStartWorldSeconds = 0.0f;
	float M_PhaseDeadlineWorldSeconds = 0.0f;
	int32 M_FailedCheckCount = 0;
	int32 M_PicturesTaken = 0;
	int32 M_FramesUntilPicture = 0;
	ESquadMarchTestPhase M_Phase = ESquadMarchTestPhase::Inactive;

	void Check(bool bCondition, const TCHAR* Description, const FString& Details = FString());
	void EnterPhase(const UWorld& World, ESquadMarchTestPhase NewPhase);
	void Finish(const UWorld& World);
	bool FinishWhenTimedOut(const UWorld& World, const TCHAR* ExpectedEvent);

	void TickFindingSquad(UWorld& World);
	ASquadController* FindSquadFarthestFromEnemies(UWorld& World, FVector& OutAwayFromEnemies) const;
	bool FindWalkableDestination(UWorld& World, const FVector& PreferredDirection);
	void StartMarch(UWorld& World);
	void TickMarchWaitingForStride(UWorld& World);
	void StartPatrol(UWorld& World);
	void TickPatrolWaitingForStride(UWorld& World);

	/** @return True once the squad's soldiers walked far enough from where the order started. */
	bool GetHasSquadWalkedOff() const;
	void CheckMarchingSoldiers();
	void HurtOneSoldierAndCheckIdle(const TCHAR* OrderName);
	void CheckSoldiersBackToNormal(const TCHAR* OrderName);

	// Soldiers nobody sees move in coarse steps in an unattended run; the test's squad is moved every frame.
	void KeepSquadMovingSmoothly() const;
	void TickPictures(UWorld& World);
	void AimCameraAtSquad(UWorld& World);
};
