// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Buildings/BuildingExpansion/B4HowitzerPit/Tests/B4HowitzerCaptureRig.h"

#include "BreakthroughHQTestSubsystem.generated.h"

class ACPPController;
class ANomadicAttachmentSkeletal;
class ANomadicVehicle;
class UStaticMeshComponent;
enum class ERTSResourceType : uint8;

UENUM()
enum class EBreakthroughHQTestPhase : uint8
{
	Inactive,
	WaitingForStart,
	Expanding,
	// The three stores are filled and emptied through the drop-off; each step checks the mesh on every store socket.
	StockLevels,
	// Both cranes run their working loop as they do while the HQ trains; their hooks must never meet.
	CranesWorking,
	// Training stopped: each crane finishes its lift, then drives home to the pose it started from.
	CranesGoingHome
};

/** Where a crane stood before it started working, to check it comes back there. */
struct FBreakthroughHQCraneRest
{
	TWeakObjectPtr<ANomadicAttachmentSkeletal> Crane;
	FName TrackedBone;
	FVector RestLocation = FVector::ZeroVector;
	float MaxDistanceFromRestCm = 0.f;
};

/**
 * @brief Map test of the Breakthrough HQ, started with -BreakthroughHQValidate on UT_BreakthroughHQ (UT_Nomadics with the
 * player's first HQ truck swapped for Ger_HQ_Truck_Breakthrough). Expands the HQ and checks its mesh, sockets,
 * attachments, drop-off and unit exit; fills the radixite, metal and vehicle parts stores step by step and
 * checks the fill mesh each level shows; runs both cranes' working loop and sends them home.
 * -BreakthroughHQCapture=<folder> saves pictures. Logs RTS_BF_HQ_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UBreakthroughHQTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	void TickWaitingForStart();
	void TickExpanding();
	void TickStockLevels();
	void TickCranesWorking();
	void TickCranesGoingHome();
	void TickExitPending();

	bool FindHQ();
	void StartGameAndUnpause() const;
	void CheckBuilding();
	void CheckAttachments();
	void CheckDropOffAndExit();
	void FillStoresTo(int32 Percentage) const;
	void CheckFillMeshes(int32 Percentage);
	const UStaticMeshComponent* FindFillComponent(FName SocketName) const;
	void StartCranes();
	void SetCranesWorking(bool bWorking) const;
	void LogCranes();
	void TrackCranesAwayFromRest();
	bool GetAreCraneMontagesPlaying() const;
	void KeepAttachmentsAnimating() const;
	void CaptureViews(const FString& Label, const TArray<FB4HowitzerCameraView>& Views) const;

	void EnterPhase(EBreakthroughHQTestPhase NewPhase);
	float GetPhaseSeconds() const;
	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	void Finish();
	ACPPController* GetController() const;
	const UStaticMeshComponent* GetBuildingMesh() const;

	UPROPERTY()
	TWeakObjectPtr<ANomadicVehicle> M_HQ;

	TArray<FBreakthroughHQCraneRest> M_Cranes;
	FB4HowitzerCaptureRig M_CaptureRig;

	EBreakthroughHQTestPhase M_Phase = EBreakthroughHQTestPhase::WaitingForStart;
	float M_PhaseStartSeconds = -1.f;
	// When the HQ first stood as a building; its attachments get a moment to begin play.
	float M_ExpandedSeconds = -1.f;
	int32 M_LevelIndex = 0;
	bool bM_IsLevelFilled = false;
	float M_NextCraneLogSeconds = 0.f;
	int32 M_NextCraneCapture = 0;
	float M_MinHookDistanceCm = TNumericLimits<float>::Max();
	int32 M_FailedCheckCount = 0;
	bool bM_IsExitPending = false;
	int32 M_ExitFramesLeft = 0;
};
