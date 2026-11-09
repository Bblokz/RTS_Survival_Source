// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"

#include "RTS_Survival/Buildings/FieldTrench/Anchors/FieldTrenchAnchorComponent.h"
#include "FieldDefenseTestSubsystem.generated.h"

class ABuildingExpansion;
class ARTSFieldTrench;
class UFieldTrenchPlacementComponent;
struct FFieldTrenchPiecePlacement;
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
	CheckingOwnerDeath,
	// -FieldDefenseValidateTrench: the field trench of the Barriers tab is laid out with two clicks along a bent
	// path, dug piece by piece and paid per metre; it waits while the player cannot pay, and a trench that was not
	// confirmed goes when the placement is cancelled.
	LayingTrench,
	// -FieldDefenseValidateTrenchAnchor: a Pak 40 pit is built; a trench started near its anchor joins it and opens
	// its neck once the first piece is dug; a trench drawn towards the free anchor joins it at its end; and when
	// the pit goes, the trench caps the end that was joined to it.
	LayingAnchoredTrench,
	// -FieldDefenseValidateEnemyTrenches: six Soviet pits of the enemy AI (player 2) stand as in a level, away from
	// the HQ. The AI's base trench component, started with its strategic AI, digs a Soviet trench out of every
	// pit's anchor (the necks show) and fire trench sections on an arc facing the HQ, its plans hidden; the
	// player's HQ trench stays German beside it.
	DiggingEnemyBaseTrenches
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
	void TickLayingTrench();
	void TickLayingAnchoredTrench();
	/** @brief Checks that a planned trench starts on the anchor's port, without a start cap, and ends with one. */
	void CheckPlanStartsOnAnchor(const FFieldTrenchAnchorHandle& Anchor, const TArray<FFieldTrenchPiecePlacement>& Plan,
	                             const FString& Description);
	/** @brief Places the Breakthrough bunkers beside the pit's spot, facing the HQ, as a level designer would. */
	void SpawnTestBunkers();
	/** @brief Checks each placed bunker's anchors against its socket table and lists them for the trenches. */
	void CollectBunkerAnchors();
	// The Breakthrough bunkers placed by the anchor test, their anchors, and the trench laid from each anchor.
	TArray<TWeakObjectPtr<AActor>> M_TestBunkers;
	TArray<FFieldTrenchAnchorHandle> M_BunkerAnchors;
	TArray<TWeakObjectPtr<ARTSFieldTrench>> M_BunkerTrenches;
	int32 M_BunkerAnchorIndex = 0;
	bool GetIsNeckShown() const;
	/** @return Whether a TrenchNeck mesh of the actor is visible, i.e. a dug trench is joined to its anchor. */
	static bool GetIsNeckShownOn(const AActor* AnchorOwner);

	void TickDiggingEnemyBaseTrenches();
	/** @return Whether flat ground for the test base was found away from the HQ; sets M_EnemyBaseCentre. */
	bool FindEnemyTestBaseCentre();
	/** @brief Places each Soviet pit on a ring round the test base as the AI's, with its anchor facing the centre. */
	void SpawnSovietTestPits();
	/** @brief Checks each Soviet pit's anchor (owned by the AI, on its socket) and lists it. */
	void CollectSovietPitAnchors();
	/** @brief Fast settings, the test base, then the strategic AI's start, which starts the digging. */
	void StartEnemyBaseTrenches();
	/** @return Whether every piece of the trench shows a mesh of the Soviet kit. */
	static bool GetAreAllPiecesSovietKit(const ARTSFieldTrench& Trench);
	/** @return Whether every piece of the trench shows a mesh of the German kit (SM_FieldTrench_*). */
	static bool GetAreAllPiecesGermanKit(const ARTSFieldTrench& Trench);
	/** @brief Counts the pieces the AI planned but has not dug, and how many of them were shown. */
	void WatchHiddenPlannedPieces();
	/** @return Whether every pit anchor is claimed, the arc has been tried and every AI trench is dug. */
	bool GetAreEnemyTrenchesDug() const;
	void CheckEnemyBaseTrenches();
	/** @brief A trench of the HQ's Barriers tab stays German while the AI digs Soviet ones. */
	void CheckDefaultKitIsGerman(ACPPController& Controller, UTrainerComponent& Trainer,
	                             UFieldTrenchPlacementComponent& Placement);
	// The Soviet pits placed by the enemy trench test and their anchors.
	TArray<TWeakObjectPtr<AActor>> M_SovietPits;
	TArray<FFieldTrenchAnchorHandle> M_SovietPitAnchors;
	int32 M_SovietPitIndex = 0;
	FVector M_EnemyBaseCentre = FVector::ZeroVector;
	int32 M_EnemyPlannedPiecesSeen = 0;
	int32 M_EnemyPlannedPiecesShown = 0;
	TWeakObjectPtr<UFieldTrenchAnchorComponent> M_TestAnchor;
	int32 M_TrenchPieceCountBeforePitLoss = 0;
	/** @return Where along the path the given distance from its start lies. */
	static FVector GetPointAlongPath(const TArray<FVector>& Path, const float Distance);
	static float GetPathLength(const TArray<FVector>& Path);
	int32 CountFieldTrenches() const;

	TArray<FVector> M_TrenchPath;
	float M_TrenchCursorDistance = 0.f;
	int32 M_RadixiteBeforeTrench = 0;
	int32 M_ExpectedTrenchRadixite = 0;
	int32 M_TrenchCountBeforeCancel = 0;
	UPROPERTY()
	TWeakObjectPtr<ARTSFieldTrench> M_TestTrench;

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
