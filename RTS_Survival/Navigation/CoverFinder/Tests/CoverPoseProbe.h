#pragma once

#include "CoreMinimal.h"

class ACameraActor;
class ASquadUnit;
class UWorld;

/**
 * @brief Records how soldiers in cover really look during a TestCover run: bone heights, mesh visibility and
 * close-up screenshots. Owned by the test scenario and switched on with -CoverFinderCloseUps.
 */
struct FCoverPoseProbe
{
	void Start();
	void Tick(UWorld& World, float PhaseSeconds);

private:
	TWeakObjectPtr<ACameraActor> M_Camera;
	TWeakObjectPtr<ASquadUnit> M_PhotographedUnit;
	float M_NextPoseLogSeconds = 0.0f;
	float M_NextCloseUpSeconds = 0.0f;
	int32 M_FramesUntilScreenshot = 0;
	int32 M_CloseUpCount = 0;
	// Alternates between the pose families so one busy crouch point cannot use up every picture.
	int32 M_SubjectRotation = 0;
	// 0 frames the soldier from its own side of the cover, 1 looks down on it like the game camera does.
	int32 M_PendingCameraAngle = 0;
	bool bM_IsEnabled = false;

	void LogCoverPoses(UWorld& World, float PhaseSeconds) const;
	void LogCoverPose(const ASquadUnit& SquadUnit, float PhaseSeconds, const TCHAR* Occasion) const;
	ASquadUnit* FindCloseUpSubject(UWorld& World);
	void StartCloseUp(UWorld& World, ASquadUnit& SquadUnit, float PhaseSeconds);
	void AimCameraAtUnit(UWorld& World, const ASquadUnit& SquadUnit);
	void TickPendingScreenshot(UWorld& World, float PhaseSeconds);
};
