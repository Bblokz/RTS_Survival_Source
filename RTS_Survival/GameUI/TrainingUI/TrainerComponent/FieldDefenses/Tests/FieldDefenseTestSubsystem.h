// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"

#include "FieldDefenseTestSubsystem.generated.h"

class ABuildingExpansion;
class ACPPController;
class ANomadicVehicle;
class AStaticPreviewMesh;
class URTSRequirement;
class UTrainerComponent;
struct FTrainingQueueItem;
enum class ERTSResourceType : uint8;

UENUM()
enum class EFieldDefenseTestPhase : uint8
{
	Inactive,
	WaitingForStart,
	ExpandingHQ,
	// A unit trains first; the first pit is planned behind it and its preview must wait.
	PlanningFirstPit,
	WaitingBehindUnit,
	// The first pit is built; its bar follows the queue item, also through a pause.
	Constructing,
	WaitingForFinish,
	// A planned pit is cancelled (nothing to refund) and so is one under construction (its paid part is refunded).
	CancellingPlanned,
	CancellingConstructing,
	// A pit destroyed while it is built leaves the queue and the next pit starts.
	LosingConstruction,
	// The leFH and sFH pits unlock with a Communication Center; a planned leFH pit goes when it is lost.
	CheckingTierTwo,
	// The player's own way: a click on the training item, the cursor over the map, a click there, and a right click
	// on the item (pause, then cancel). Sent as viewport mouse events; the real cursor of the machine stays put.
	PlacingWithMouse,
	// Eight pits planned behind another unit; the ninth is refused.
	PlanningAhead,
	WaitingForNextConstruction,
	CheckingOwnerDeath
};

/**
 * @brief Map test of the HQ's field defenses, started with -FieldDefenseValidate on UT_Nomadics, which holds a German HQ
 * truck on a navmesh. It expands the HQ and drives the player controller the way the training menu does. It checks that a planned
 * pit waits behind a unit in training, blocks its footprint, refuses a slope, is built at its spot with a progress bar
 * that follows its queue item through a pause; cancels (with refunds), a pit lost while building, the Communication
 * Center requirement both ways, that only eight pits can be planned ahead, and that the HQ's death destroys the
 * previews and the unfinished pit but not the finished one. Logs RTS_FIELD_DEFENSE_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UFieldDefenseTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EFieldDefenseTestPhase M_Phase = EFieldDefenseTestPhase::WaitingForStart;
	float M_PhaseStartSeconds = -1.f;
	float M_PhaseTimeoutSeconds = 0.f;
	// Steps of the current phase; one step per tick so that new previews are in the physics scene for the next one.
	int32 M_PhaseStep = 0;
	int32 M_FailedCheckCount = 0;
	float M_NextStatusSeconds = 0.f;

	UPROPERTY()
	TWeakObjectPtr<ANomadicVehicle> M_HQ;

	UPROPERTY()
	TWeakObjectPtr<UTrainerComponent> M_Trainer;

	// The first pit: planned behind a unit, then built; it must outlive the HQ.
	FVector M_FirstPitSpot = FVector::ZeroVector;

	UPROPERTY()
	TWeakObjectPtr<ABuildingExpansion> M_FirstPit;

	float M_ConstructionStartSeconds = 0.f;
	float M_PausedTrainedSeconds = 0.f;
	float M_PausedBarElapsedSeconds = 0.f;
	bool bM_SawPitBeforeItsTurn = false;

	// The pits planned ahead: their spots and previews; the HQ's death must destroy the previews.
	TArray<FVector> M_PlannedSpots;

	UPROPERTY()
	TArray<TWeakObjectPtr<AStaticPreviewMesh>> M_PlannedPreviews;

	UPROPERTY()
	TWeakObjectPtr<ABuildingExpansion> M_PitUnderConstructionAtDeath;

	// Spots and the pit the cancel, loss and requirement phases work with.
	FVector M_PhaseSpot = FVector::ZeroVector;
	FVector M_SecondPhaseSpot = FVector::ZeroVector;
	float M_PhaseMarkSeconds = 0.f;

	UPROPERTY()
	TWeakObjectPtr<ABuildingExpansion> M_PhasePit;

	// The Communication Center spawned for the requirement checks.
	UPROPERTY()
	TWeakObjectPtr<AActor> M_TierTwoBuilding;

	// Where the mouse phase put the viewport cursor, in Slate's absolute coordinates.
	FVector2D M_CursorAbsolutePosition = FVector2D::ZeroVector;
	int32 M_QueuedBeforeMouse = 0;

	void Check(const bool bCondition, const FString& Description, const FString& Details = FString());

	// -FieldDefenseCapture=<folder>: screenshots (with UI) of the key moments, for a run in a visible window.
	FString M_CaptureDirectory;
	TArray<TPair<float, FString>> M_PendingCaptures;
	// After the result the run waits for its last screenshots before it exits.
	bool bM_IsExitPending = false;
	int32 M_ExitFramesLeft = 0;

	/**
	 * @brief Queues a screenshot of a moment when captures are on.
	 * @param MomentName The file name of the picture.
	 * @param FocusLocation Where the camera looks; ignored when bFocusCamera is false.
	 * @param DelaySeconds How long the moment plays before the picture.
	 * @param bFocusCamera False when the camera must stay, e.g. while the cursor aims at the screen.
	 */
	void ScheduleCapture(const FString& MomentName, const FVector& FocusLocation, const float DelaySeconds,
	                     const bool bFocusCamera = true);
	void HoldCameraStill() const;
	void TakeDueCapture();
	void TickExitPending();
	void EnterPhase(const EFieldDefenseTestPhase NewPhase, const float TimeoutSeconds);
	float GetPhaseSeconds() const;
	void Finish();

	void TickWaitingForStart();
	void TickExpandingHQ();
	void TickPlanningFirstPit();
	void TickWaitingBehindUnit();
	void TickConstructing();
	void TickWaitingForFinish();
	void TickCancellingPlanned();
	void TickCancellingConstructing();
	void TickLosingConstruction();
	void TickCheckingTierTwo();
	void TickPlacingWithMouse();
	class UW_TrainingItem* FindTrainingItemWidget(const FTrainingOption& Option) const;
	bool ClickTrainingItemWidget(const FTrainingOption& Option, const bool bIsLeftClick) const;
	// True in a visible window, where the machine's cursor drives the viewport and must not be taken over.
	static bool GetIsCursorOwnedByVisibleWindow();
	// The visible-window stand-in for the cursor click: plans the pit at the phase spot through the placement path.
	void PlaceMousePhasePitWithoutCursor(ACPPController& Controller, const float Now);
	/** @return Whether the world location is on screen; the viewport then has its cursor there. */
	bool MoveViewportCursorTo(const FVector& WorldLocation);
	void SendViewportLeftButton(const bool bIsPressed) const;
	static void SetSyntheticCursorMovesEnabled(const bool bEnabled);
	void SpawnTierTwoBuilding();
	/** @brief Cancels the last queued item of the option the way a right click on a paused queue does. */
	bool CancelLastQueuedPit(const FTrainingOption& Option) const;
	int32 GetResourceAmount(const ERTSResourceType Resource) const;
	void TickPlanningAhead();
	void TickWaitingForNextConstruction();
	void TickCheckingOwnerDeath();

	bool FindHQ();
	void LogHQStatus(const ANomadicVehicle& HQ, const UTrainerComponent* Trainer);
	void GivePlayerResources() const;
	void KeepHQAnimating() const;
	void CheckLayoutAndRequirements();
	void CheckInclineOnSlab(const FVector& Spot);

	ACPPController* GetController() const;
	URTSRequirement* GetRequirementTemplate(const FTrainingOption& Option) const;
	FTrainingQueueItem MakeQueueItem(const FTrainingOption& Option) const;
	bool StartPlacing(const FTrainingOption& Option);
	bool FindValidSpot(FVector& OutSpot) const;
	bool GetGroundAt(const FVector2D& Location2D, FVector& OutGround) const;
	bool PlacePitAtValidSpot(FVector& OutSpot, const FTrainingOption& Option);
	int32 GetQueuedCount(const FTrainingOption& Option) const;
	ABuildingExpansion* FindPitNear(const FVector& Spot) const;
	AStaticPreviewMesh* FindPreviewNear(const FVector& Spot) const;
};
