#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Player/SquadMovePreview/SquadMovePlanner.h"
#include "RTS_Survival/Player/SquadMovePreview/Tests/SquadMovePreviewTestScenario.h"
#include "SquadMovePreviewComponent.generated.h"

class ASquadController;
class ASquadUnit;
class URTSCoverFinderWorldSubsystem;

/** What the player controller knows this frame that decides whether and where a squad move is previewed. */
struct FSquadMovePreviewInput
{
	// Squads currently selected; only read during the call.
	const TArray<ASquadController*>* SelectedSquads = nullptr;

	// True when squads are the only thing selected and no other cursor mode (building, ability) is active.
	bool bSquadsOnlyMoveContext = false;

	// False when the cursor is over the UI, the sky, or an actor that a click would target instead of move to.
	bool bCursorOnMoveGround = false;
	FVector CursorLocation = FVector::ZeroVector;

	// Set while the player drags the rotation arrow: the squads then stay where the drag began and turn with it.
	bool bFacingChosenByPlayer = false;
	FVector ChosenAnchorLocation = FVector::ZeroVector;
	FRotator ChosenRotation = FRotator::ZeroRotator;
};

/** One soldier of the previewed selection, parallel to the positions of the current plan. */
struct FSquadMovePreviewUnit
{
	TWeakObjectPtr<ASquadUnit> SquadUnit;
	int32 SquadIndex = 0;
};

/** Remembers what the plan on screen was built from, so it is only rebuilt when something relevant changed. */
struct FSquadMovePreviewRefreshState
{
	FVector Anchor = FVector::ZeroVector;
	FVector Facing = FVector::ForwardVector;
	double PlannedAtWorldSeconds = 0.0;
	uint32 SelectionHash = 0;
	bool bFacingChosenByPlayer = false;
	bool bHasPlan = false;
};

/**
 * @brief Shows where every soldier of the selected squads would go for the current cursor position, and issues
 * that exact plan when the player gives the move order. Only active while squads are the whole selection.
 * The player controller calls UpdatePreview every tick and TryIssuePlannedMove from its move order.
 */
UCLASS()
class RTS_SURVIVAL_API USquadMovePreviewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USquadMovePreviewComponent();

	/**
	 * @brief Replans when the cursor, facing or selection changed enough, and draws the current plan.
	 * @param Input This frame's selection, cursor and rotation-arrow state from the player controller.
	 */
	void UpdatePreview(const FSquadMovePreviewInput& Input);

	/**
	 * @brief Sends the selected squads to freshly planned per-soldier positions instead of formation slots.
	 * @param SelectedSquads Squads to move; must be the whole selection.
	 * @param MoveLocation Ground location of the order.
	 * @param bFacingChosenByPlayer Whether the rotation arrow gave a facing for this order.
	 * @param ChosenRotation The arrow's rotation when it was used.
	 * @param bQueueCommand True to append to the squads' command queues instead of replacing them.
	 * @param OutIssuedCommandCount Number of squads that accepted the move.
	 * @return False when these squads cannot be planned, so the caller must use the regular formation move.
	 */
	bool TryIssuePlannedMove(
		const TArray<ASquadController*>& SelectedSquads,
		const FVector& MoveLocation,
		bool bFacingChosenByPlayer,
		const FRotator& ChosenRotation,
		bool bQueueCommand,
		uint32& OutIssuedCommandCount);

	/**
	 * @brief Plans positions for squads without touching the plan that is on screen.
	 * @param Squads Squads to plan for.
	 * @param Anchor Ground location the squads are sent to.
	 * @param bFacingChosenByPlayer Whether Facing is an explicit choice that cover must protect against.
	 * @param Facing Wanted facing; ignored unless chosen by the player, then derived from the walking direction.
	 * @param OutUnits The soldiers that were planned for, parallel to OutPlan.UnitPositions.
	 * @param OutPlan Resulting positions.
	 * @return False when the squads cannot be planned (not all plain infantry squads, or no soldiers).
	 */
	bool BuildPlanForSquads(
		const TArray<ASquadController*>& Squads,
		const FVector& Anchor,
		bool bFacingChosenByPlayer,
		const FVector& Facing,
		TArray<FSquadMovePreviewUnit>& OutUnits,
		FSquadMovePlan& OutPlan);

	/** The shape picked in the formation picker; squads-only moves place their squads in the same shape. */
	void SetFormationShape(EFormation FormationShape);

	/** @return True when every squad is plain infantry outside cargo, which is what the planner supports. */
	static bool GetCanPlanForSquads(const TArray<ASquadController*>& Squads);

	const FSquadMovePlan& GetCurrentPlan() const { return M_CurrentPlan; }
	bool GetIsPreviewVisible() const { return M_RefreshState.bHasPlan; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	FSquadMovePlan M_CurrentPlan;
	TArray<FSquadMovePreviewUnit> M_CurrentPlanUnits;
	FSquadMovePreviewRefreshState M_RefreshState;
	FSquadMovePlannerSettings M_PlannerSettings;
	EFormation M_FormationShape = EFormation::RectangleFormation;

	// Facing used while the cursor sits on top of the squads, where the walking direction is undefined.
	FVector M_LastAutomaticFacing = FVector::ForwardVector;

	FSquadMovePreviewTestScenario M_TestScenario;

	bool GetShouldReplan(
		const FSquadMovePreviewInput& Input,
		const FVector& Anchor,
		const FVector& Facing,
		uint32 SelectionHash,
		double WorldSeconds) const;
	void HidePreview();
	void DrawCurrentPlan() const;

	/**
	 * @brief Copies the soldiers of the squads into planner units.
	 * @param Squads Squads to read.
	 * @param OutUnits Soldiers in planner order.
	 * @param OutRequest Receives the planner units and squad count.
	 * @return Average location of all gathered soldiers.
	 */
	FVector GatherPlannerUnits(
		const TArray<ASquadController*>& Squads,
		TArray<FSquadMovePreviewUnit>& OutUnits,
		FSquadMovePlanRequest& OutRequest) const;

	// Only cover that is free or already held by one of the planned soldiers may be offered to the planner.
	void GatherAvailableCoverPoints(
		const TArray<FSquadMovePreviewUnit>& Units,
		FSquadMovePlanRequest& InOutRequest) const;
	FVector ResolveFacing(
		bool bFacingChosenByPlayer,
		const FVector& Facing,
		const FVector& UnitsCentre,
		const FVector& Anchor);
	void ProjectRegularPositionsToNavigation(FSquadMovePlan& InOutPlan) const;
	FVector ProjectToNavigation(const FVector& Location) const;
	uint32 BuildSelectionHash(const TArray<ASquadController*>& Squads) const;
};
