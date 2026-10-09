#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnitPlannedPosition.h"

class ASquadController;
class ASquadUnit;
class URTSCoverFinderWorldSubsystem;
class USquadMovePreviewComponent;
class UWorld;
struct FRTSCoverPoint;
struct FSquadMovePlan;

enum class ESquadMovePreviewTestPhase : uint8
{
	Inactive,
	WaitingForCoverScan,
	SettlingAfterCoverScan,
	WaitingForPlannedMove
};

/** What one soldier was promised by the plan the test issued, to compare against where it ends up. */
struct FSquadMovePreviewTestExpectation
{
	TWeakObjectPtr<ASquadUnit> SquadUnit;
	FSquadUnitPlannedPosition Position;
};

/**
 * @brief Map test for the squads-only move preview, started with -SquadMovePreviewValidate on the cover test map.
 * It plans for cursor positions on crouch cover, on standing cover and on open ground, checks the plans, then
 * issues one planned move and verifies every soldier ends up on the position it was shown.
 * Results are logged as RTS_SQUAD_PREVIEW_TEST lines ending in RESULT PASS or RESULT FAIL.
 */
struct FSquadMovePreviewTestScenario
{
	void Start();
	void Tick(USquadMovePreviewComponent& PreviewComponent, float DeltaTime);
	bool GetIsRunning() const { return M_Phase != ESquadMovePreviewTestPhase::Inactive; }

	// True while the scenario shows a preview of its own that the player controller's updates must not replace.
	bool GetOwnsPreview() const { return M_StanceCaptureFramesLeft > 0; }

private:
	TArray<FSquadMovePreviewTestExpectation> M_Expectations;
	ESquadMovePreviewTestPhase M_Phase = ESquadMovePreviewTestPhase::Inactive;
	float M_PhaseElapsedSeconds = 0.0f;
	int32 M_FailedCheckCount = 0;

	// -SquadMovePreviewCaptureStances: keeps the preview on screen for a moment at two places and saves a
	// picture of each, so the stance meshes can be looked at after a rendered run.
	TArray<TWeakObjectPtr<ASquadController>> M_StanceCaptureSquads;
	FVector M_StanceCaptureLocations[2] = {FVector::ZeroVector, FVector::ZeroVector};
	int32 M_StanceCaptureFramesLeft = 0;
	void TickStanceCapture(USquadMovePreviewComponent& PreviewComponent, UWorld& World);

	void ResumeWorldIfPaused(UWorld& World) const;
	void RunPlanChecksAndIssueMove(
		USquadMovePreviewComponent& PreviewComponent,
		UWorld& World,
		const URTSCoverFinderWorldSubsystem& CoverSubsystem);
	void FinishPlannedMoveCheck(UWorld& World);

	/** Records and logs one named check; a failed check fails the whole scenario. */
	void Check(bool bPassed, const TCHAR* CheckName, const FString& Details = FString());

	/**
	 * @brief Shows the preview the way the player controller does and checks the stance meshes against the plan.
	 * @param PreviewComponent Component under test.
	 * @param TestSquad Squads to preview.
	 * @param CursorLocation Where the cursor is pretended to be.
	 */
	void CheckStanceMeshes(
		USquadMovePreviewComponent& PreviewComponent,
		const TArray<ASquadController*>& TestSquad,
		const FVector& CursorLocation);

	// Plans onto the nearest free prone point, when the map has one, and expects a prone position there.
	void CheckPronePlan(
		USquadMovePreviewComponent& PreviewComponent,
		const URTSCoverFinderWorldSubsystem& CoverSubsystem,
		const TArray<ASquadController*>& TestSquad);

	void CheckCoverPlans(
		USquadMovePreviewComponent& PreviewComponent,
		const TArray<ASquadController*>& TestSquad,
		const FRTSCoverPoint& CrouchPoint,
		const FRTSCoverPoint& StandingPoint);
	void CheckOpenGroundPlan(
		USquadMovePreviewComponent& PreviewComponent,
		const TArray<ASquadController*>& TestSquad,
		const FVector& OpenGroundLocation);
	void CheckChosenFacing(
		USquadMovePreviewComponent& PreviewComponent,
		const TArray<ASquadController*>& TestSquad,
		const FRTSCoverPoint& CoverPoint);
	void CheckSeveralSquads(
		USquadMovePreviewComponent& PreviewComponent,
		const TArray<ASquadController*>& PlayerSquads,
		const FVector& Anchor);
	void CheckPlanningCost(
		USquadMovePreviewComponent& PreviewComponent,
		const TArray<ASquadController*>& PlayerSquads,
		const FVector& Anchor);
	void IssuePlannedMove(
		USquadMovePreviewComponent& PreviewComponent,
		const TArray<ASquadController*>& TestSquad,
		const FVector& Anchor);
	bool GetIsExpectationMet(const FSquadMovePreviewTestExpectation& Expectation, FString& OutDetails) const;
	void EnterPhase(ESquadMovePreviewTestPhase NewPhase);
};
