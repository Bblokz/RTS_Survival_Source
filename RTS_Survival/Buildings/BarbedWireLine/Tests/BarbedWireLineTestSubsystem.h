// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "BarbedWireLineTestSubsystem.generated.h"

class ACPPController;
class AActor;
class AFieldConstruction;
class ANomadicVehicle;
class ARTSBarbedWireLine;
class UBarbedWirePlacementComponent;
class UTrainerComponent;
struct FBarbedWirePieceRuntime;

UENUM()
enum class EBarbedWireTestStep : uint8
{
	WaitingForStart,
	ExpandingHQ,
	OpeningBarriersTab,
	PickingWire,
	Drawing,
	Confirming,
	WaitingForBuild,
	CheckingNavBlocked,
	CheckingDamagedPiece,
	CheckingNavOpen,
	StartingSnappedLine,
	ConfirmingSnappedLine,
	StartingPatch,
	ConfirmingPatch,
	WaitingForPatch,
	SpawningVehicle,
	DrivingOntoWire,
	CheckingCrush,
	// A scavenger barbed wire placed on its own, as a squad builds it, and an enemy T-34 driven onto it.
	SpawningScavengerWire,
	DrivingEnemyOntoScavengerWire,
	CheckingScavengerWireCrush,
	StartingCancel,
	CheckingCancel,
	Done
};

/**
 * @brief Map test of the HQ's barbed wire line, started with -BarbedWireValidate on UT_Nomadics (a German HQ truck on a
 * navmesh). It expands the HQ, picks the wire in the Barriers tab, drags a bent line and checks: one payment per piece,
 * every piece a scavenger barbed wire actor that blocks infantry on the character navmesh, one destroyed piece opening
 * a gap only infantry fits through, a new line snapping onto an open end, a patch closing the gap end to end, a tank
 * crushing exactly the piece it stands on, the scavengers' own wire crushed by an enemy T-34, and that cancelling removes
 * only the line not yet confirmed. -BarbedWireCapture=<folder> (with -RenderOffscreen, not -nullrhi) saves screenshots
 * of the planned, stringing, built and crushed wire.
 * Logs RTS_BARBED_WIRE_TEST; the last line holds RESULT PASS or FAIL. Not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UBarbedWireLineTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EBarbedWireTestStep M_Step = EBarbedWireTestStep::WaitingForStart;
	float M_StepStartSeconds = -1.0f;
	float M_TestStartSeconds = -1.0f;
	int32 M_FailedCheckCount = 0;
	bool bM_IsFinished = false;

	TWeakObjectPtr<ANomadicVehicle> M_HQ;
	TWeakObjectPtr<UTrainerComponent> M_Trainer;

	// The bent path the first line is drawn along, on the ground.
	TArray<FVector> M_Path;
	float M_CursorDistance = 0.0f;
	// The way from the HQ the test lays its lines (ChooseClearTestYaw); the HQ's forward until chosen.
	float M_TestYaw = 0.0f;
	int32 M_RadixiteBefore = 0;
	int32 M_ExpectedRadixite = 0;
	// Length of the infantry path across the first line's gap piece while the line stood whole.
	float M_BlockedPathLength = 0.0f;
	FVector M_NavProbeCentre = FVector::ZeroVector;
	FVector M_NavProbeSide = FVector::ZeroVector;
	FVector M_SnapCursor = FVector::ZeroVector;
	int32 M_LineCountBeforeCancel = 0;
	// Step time at which the patch and the snapped line were done; the patched-gap nav check waits from there.
	float M_PatchDoneSeconds = -1.0f;
	// The unconfirmed line of the cancel step was checked and the placement stopped; next tick checks it is gone.
	bool bM_HasStoppedCancelLine = false;

	TWeakObjectPtr<ARTSBarbedWireLine> M_FirstLine;
	TWeakObjectPtr<ARTSBarbedWireLine> M_SnappedLine;
	TWeakObjectPtr<ARTSBarbedWireLine> M_PatchLine;
	TWeakObjectPtr<AActor> M_Vehicle;
	TWeakObjectPtr<AActor> M_EnemyVehicle;
	TWeakObjectPtr<AActor> M_ScavengerWire;
	bool bM_ScavengerWireDied = false;
	bool bM_TookStringingCapture = false;

	// -BarbedWireCapture=<folder>: screenshots of the key moments; the run waits for them before it exits.
	FString M_CaptureDirectory;
	TArray<TPair<float, FString>> M_PendingCaptures;
	bool bM_IsExitPending = false;
	int32 M_ExitFramesLeft = 0;

	void Check(const bool bCondition, const FString& Description, const FString& Details = FString());
	void EnterStep(const EBarbedWireTestStep NewStep);
	float GetStepSeconds() const;
	void Finish();
	/** @brief Aims the camera at the spot and queues a screenshot after the delay, when captures are on. */
	void ScheduleCapture(const FString& MomentName, const FVector& FocusLocation, const float DelaySeconds);
	void TakeDueCapture();
	void TickExitPending();
	/** @brief Checks that shots on the AI's weapon channel, which hostile shells use, hit the piece's wire actor. */
	void CheckWeaponTraceHitsPiece(const FBarbedWirePieceRuntime& Piece);

	void TickStep();
	void TickWaitingForStart();
	void TickExpandingHQ();
	void TickOpeningBarriersTab();
	void TickPickingWire();
	void TickDrawing();
	void TickConfirming();
	void TickWaitingForBuild();
	void CheckBuiltPieces(const ARTSBarbedWireLine& Line);
	void TickCheckingNavBlocked();
	void TickCheckingDamagedPiece();
	void TickCheckingNavOpen();
	void TickStartingSnappedLine();
	void TickConfirmingSnappedLine();
	void TickStartingPatch();
	void TickConfirmingPatch();
	void TickWaitingForPatch();
	void TickSpawningVehicle();
	void TickDrivingOntoWire();
	void TickCheckingCrush();
	void TickSpawningScavengerWire();
	void TickDrivingEnemyOntoScavengerWire();
	void TickCheckingScavengerWireCrush();
	FVector GetScavengerWireSpot() const;
	/** @return The collision of every primitive of the wire, for the log. */
	static FString DescribeCrushSetup(const AFieldConstruction* WireActor);
	void TickStartingCancel();
	void TickCheckingCancel();
	/** @brief Checks that a line drawn into the HQ is cut before the first piece that would stand on it. */
	void CheckLineStopsBeforeHQ(const UBarbedWirePlacementComponent& Placement);
	/** @return Whether an infantry path of this length (-1 for none) goes round the wire instead of through it. */
	static bool GetIsInfantryPathBlocked(const float PathLength);

	ACPPController* GetController() const;
	UBarbedWirePlacementComponent* GetPlacement() const;
	bool FindHQ();
	void GivePlayerResources() const;
	void KeepHQAnimating() const;
	TMap<ERTSResourceType, int32> GetCostPerMetre() const;
	int32 GetRadixite() const;
	bool GetGroundAt(const FVector2D& Location2D, FVector& OutGround) const;
	FVector GetHQForward() const;
	FVector GetHQRight() const;
	/** @brief Picks the way from the HQ the test lays its lines: the first whose area holds no building. */
	void ChooseClearTestYaw();
	/** @brief Builds the first line's path: away from the HQ, then a 45 degree turn to the right. */
	void MakeFirstPath();
	bool ClickWireMenuItem() const;
	int32 CountLines() const;
	/** @return Whether the line has pieces and none of them is planned or being strung any more. */
	static bool GetIsLineDone(const ARTSBarbedWireLine* Line);

	/**
	 * @brief Measures the infantry path from one side of the probe to the other on the character navmesh.
	 * @return Its length; -1 when there is no full path.
	 */
	float MeasureInfantryPathAcrossProbe(const bool bLogPathPoints = false) const;
	void SetNavProbeOnPiece(const ARTSBarbedWireLine& Line, const int32 PieceIndex);
};
