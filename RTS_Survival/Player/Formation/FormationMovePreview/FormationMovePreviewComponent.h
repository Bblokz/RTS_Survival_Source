#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Player/Formation/FormationDragLineLayout.h"
#include "RTS_Survival/Player/Formation/FormationDragPath.h"
#include "FormationMovePreviewComponent.generated.h"

class ASquadController;
class ASelectablePawnMaster;
class ASelectableActorObjectsMaster;
class UFormationController;
class ULineBatchComponent;
struct FBatchedLine;

/** What the player controller knows this frame that decides whether and where a formation move is previewed. */
struct FFormationMovePreviewInput
{
	// The current selection; only read during the call.
	const TArray<ASquadController*>* SelectedSquads = nullptr;
	const TArray<ASelectablePawnMaster*>* SelectedPawns = nullptr;
	const TArray<ASelectableActorObjectsMaster*>* SelectedActorMasters = nullptr;

	// True when the selection moves through the formation controller and no other cursor mode is active.
	bool bFormationMoveContext = false;

	bool bCursorHit = false;

	// False when the cursor is over an actor that a click would target instead of move to.
	bool bCursorOnMoveGround = false;
	FVector CursorLocation = FVector::ZeroVector;

	// True once the rotation arrow of the held secondary button appeared; from then on it shapes the formation.
	bool bArrowFacingChosen = false;
	FRotator ArrowRotation = FRotator::ZeroRotator;
};

/** The held secondary button that went down on move ground, from press to release. */
struct FFormationMovePreviewDragState
{
	bool bIsActive = false;

	// Set once the rotation arrow appeared; a release before that is a quick click with the regular rectangle.
	bool bIsArrowStarted = false;
	FFormationArrowDrag ArrowDrag;

	// Front the slots on screen line up behind, drawn as a bar so the player sees how wide the arrow made it.
	FFormationDragPath FrontLine;
};

/** Remembers what the slots on screen were built from, so they are only rebuilt when something relevant changed. */
struct FFormationMovePreviewRefreshState
{
	FVector CursorLocation = FVector::ZeroVector;
	double BuiltAtWorldSeconds = 0.0;
	uint32 SelectionHash = 0;
	bool bIsVisible = false;

	// The arrow the slots on screen were built for.
	float ArrowYaw = 0.0f;
	float ArrowLength = 0.0f;
};

/**
 * @brief Shows where a formation move would put every selected unit for the current cursor position, and
 * follows the held secondary button: once its rotation arrow appears, the arrow turns and widens the formation.
 * The player controller calls UpdatePreview every tick and BeginDrag / EndDrag around the secondary click.
 * @note InitFormationMovePreview: call from the owning controller before the first UpdatePreview.
 */
UCLASS()
class RTS_SURVIVAL_API UFormationMovePreviewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFormationMovePreviewComponent();

	void InitFormationMovePreview(UFormationController* FormationController);

	/**
	 * @brief Follows the rotation arrow of an active drag, rebuilds the slots when needed and redraws them.
	 * @param Input This frame's selection and cursor state from the player controller.
	 */
	void UpdatePreview(const FFormationMovePreviewInput& Input);

	/** Starts following the held secondary button that went down on this ground location. */
	void BeginDrag(const FVector& StartLocation);

	/** Forgets the held button; the next order is a regular formation move again. */
	void EndDrag();

	bool GetIsDragActive() const { return M_DragState.bIsActive; }

	/** @return True once the rotation arrow appeared, so releasing lines the units up behind its front. */
	bool GetIsArrowFormationReady() const { return M_DragState.bIsActive && M_DragState.bIsArrowStarted; }

	const FFormationArrowDrag& GetArrowDrag() const { return M_DragState.ArrowDrag; }

	/** @return Front line of the slots on screen; empty unless the arrow shapes the formation. */
	const FFormationDragPath& GetFrontLine() const { return M_DragState.FrontLine; }

	/** @return True when the renderer exists and the engine will actually draw it in the game view. */
	bool GetCanPreviewRender() const;

	/** @return Number of line segments of the preview that is on screen right now. */
	int32 GetPreviewLineCount() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TWeakObjectPtr<UFormationController> M_FormationController;

	bool GetIsValidFormationController() const;

	// Line renderer; its lines stay until flushed, so the preview is only redrawn when it changed.
	// Registered with the world instead of the owning controller: controllers are hidden actors and the
	// engine draws nothing that belongs to a hidden actor.
	UPROPERTY()
	TObjectPtr<ULineBatchComponent> M_PreviewLineBatch;

	bool GetIsValidPreviewLineBatch() const;

	FFormationMovePreviewDragState M_DragState;
	FFormationMovePreviewRefreshState M_RefreshState;

	// Slots of the preview on screen, parallel arrays.
	TArray<FVector> M_SlotLocations;
	TArray<FRotator> M_SlotRotations;

	bool GetShouldRebuild(
		const FFormationMovePreviewInput& Input,
		uint32 SelectionHash,
		double WorldSeconds) const;
	bool GetHasArrowChangedSinceBuild() const;
	void UpdateArrowDrag(const FFormationMovePreviewInput& Input);
	void RebuildSlots(const FFormationMovePreviewInput& Input);
	uint32 BuildSelectionHash(const FFormationMovePreviewInput& Input) const;

	void HidePreview();
	void DrawPreview();
	void AddFrontLineLines(TArray<FBatchedLine>& OutLines) const;
	void AddSlotArrowLines(const FVector& SlotLocation, const FRotator& SlotRotation,
	                       TArray<FBatchedLine>& OutLines) const;
};
