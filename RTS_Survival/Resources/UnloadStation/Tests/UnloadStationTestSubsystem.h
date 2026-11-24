// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Buildings/BuildingExpansion/B4HowitzerPit/Tests/B4HowitzerCaptureRig.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "UnloadStationTestSubsystem.generated.h"

class AResourceUnloadStation;
class ACPPController;
class ANomadicVehicle;
class UHarvester;
class UResourceDropOff;
enum EHarvesterAIAction : uint8;

UENUM()
enum class EUnloadStationTestPhase : uint8
{
	Inactive,
	WaitingForStart,
	WaitingForStation,
	WaitingForHarvesters,
	// Two harvesters (radixite, metal) unload at the same time, one per bay.
	BothBaysUnloading,
	// A third trip; the HQ packs up while a load hangs in the air: nothing may be lost or doubled.
	PackUpMidUnload,
	WaitingForPackUpResult
};

/** One test harvester and what it carried into the station. */
struct FUnloadTestHarvester
{
	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<UHarvester> Harvester;
	ERTSResourceType Resource;
	int32 StartCargo = 0;
	bool bWasDocked = false;
	bool bFinished = false;
	float DockErrorCm = -1.f;
	float DockYawErrorDeg = -1.f;
	FVector DockedLocation = FVector::ZeroVector;
	// Where it stood when its cargo was gone (the overflow point for a harvester that found no free bay).
	FVector FinishedLocation = FVector::ZeroVector;

	// Unload lock check: right after docking the harvester gets a move (or a move then a stop); it must stay docked
	// until its cargo is gone and only then run the held order.
	bool bLockOrderGiven = false;
	bool bLockOrderIsStop = false;
	bool bLockOrderHeld = false;
	bool bLockKeptUnloading = false;
	float MaxDockDriftCm = 0.f;
	bool bHeldMoveRanAfterUnload = false;
	// Last harvester AI status written to the log (EHarvesterAIAction as uint8).
	uint8 LastLoggedStatus = MAX_uint8;
};

/**
 * @brief Map test of the HQ's loop crane (AResourceUnloadStation), started with -UnloadStationValidate on a unit-test
 * map (default RTS_ToCopySmall, HQ at -UnloadStationAt=X,Y). Spawns an unpacked Ger_HQ_Truck and two Pz IV harvesters
 * with full cargo; both reverse into their bays at once and are unloaded chunk by chunk; checks docking accuracy,
 * that both trolleys work at the same time, and that exactly the cargo arrived. Then packs the HQ up while a load is
 * in the air and checks that cargo on board + credited stays the same. -UnloadStationCapture=<folder> saves pictures.
 * -UnloadStationTruck=Refinery runs the same on Ger_Refinery_Truck (AKnuckleCraneUnloadStation, one bay): two radixite
 * harvesters, one docks and is unloaded by the knuckle boom, the other unloads the old way at the overflow point
 * without driving into the docked one.
 * Logs RTS_UNLOAD_TEST; not in shipping.
 */
/** Which truck's station the run tests (-UnloadStationTruck=HQ|Refinery|MetalDepot). */
enum class EUnloadTestStation : uint8
{
	HQ,
	Refinery,
	MetalDepot
};

UCLASS()
class RTS_SURVIVAL_API UUnloadStationTestSubsystem : public UTickableWorldSubsystem
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
	void TickWaitingForStation();
	void TickWaitingForHarvesters();
	void TickBothBaysUnloading();
	void TickPackUpMidUnload();
	void TickWaitingForPackUpResult();
	void TickExitPending();

	void SpawnHQ();
	AResourceUnloadStation* FindStation() const;
	AActor* SpawnHarvester(const FVector& Location) const;
	void OrderReturnCargo(FUnloadTestHarvester& TestHarvester, ERTSResourceType Resource, int32 Amount);
	void TrackHarvester(FUnloadTestHarvester& TestHarvester, AResourceUnloadStation& Station);
	void EmptyStores() const;
	int32 GetStored(ERTSResourceType Resource) const;
	int32 GetCargo(const FUnloadTestHarvester& TestHarvester) const;
	void StartGameAndUnpause() const;
	/** @param MaxViews Only the first views when > 0 (sequence frames); all views otherwise. */
	void CaptureViews(const FString& Label, int32 MaxViews = 0) const;

	void EnterPhase(EUnloadStationTestPhase NewPhase);
	float GetPhaseSeconds() const;
	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	void Finish();
	ACPPController* GetController() const;

	EUnloadStationTestPhase M_Phase = EUnloadStationTestPhase::WaitingForStart;
	float M_PhaseStartSeconds = -1.f;
	int32 M_FailedCheckCount = 0;
	bool bM_IsExitPending = false;
	int32 M_ExitFramesLeft = 0;

	TWeakObjectPtr<ANomadicVehicle> M_HQ;
	TWeakObjectPtr<AResourceUnloadStation> M_Station;
	FVector M_Anchor = FVector::ZeroVector;
	TArray<FUnloadTestHarvester> M_Harvesters;
	int32 M_StoredRadixiteBefore = 0;
	int32 M_StoredMetalBefore = 0;
	int32 M_RadixiteStoreChanges = 0;
	int32 M_LastStoredRadixite = 0;
	bool bM_SawBothDocked = false;
	bool bM_SawBothTrolleysBusy = false;
	bool bM_CapturedBusy = false;
	bool bM_PackUpOrdered = false;
	int32 M_PackUpCargoBefore = 0;
	int32 M_PackUpStoredBefore = 0;

	FB4HowitzerCaptureRig M_CaptureRig;

	// Refinery / metal depot run: one bay, both harvesters carry the station's resource, the bay-less one uses the
	// overflow point.
	EUnloadTestStation M_StationKind = EUnloadTestStation::HQ;
	// Counted in chunks and used for the pack-up trip: radixite on the HQ and the refinery, metal on the depot.
	ERTSResourceType M_TrackedResource = ERTSResourceType::Resource_Radixite;
	int32 M_SingleBayCargoEach = 0;
	bool bM_SawCraneBusy = false;
	// Single-bay capture moments: the rig lowering (apron swing), then frames a set time into the busy phase, when
	// the elevator's bars ride the buckets and land in the bin.
	bool bM_CapturedLowering = false;
	float M_BusyStartWorldSeconds = -1.f;
	int32 M_MidCapturesTaken = 0;
	int32 M_PackUpHarvesterIndex = 0;
	// The pack-up baseline (cargo on board, stored) is taken when the harvester docks, so harvesting on the way in
	// does not count.
	bool bM_HasPackUpBaseline = false;

	void TickSingleBayUnloadResults(AResourceUnloadStation& Station);
	void TickSingleBayCaptures(const AResourceUnloadStation& Station);
	bool GetIsSingleBay() const { return M_StationKind != EUnloadTestStation::HQ; }

	/**
	 * @brief Orders a just-docked harvester away (a move; on the second docked harvester a move then a stop) to prove
	 * the unload lock holds the order instead of abandoning the unload.
	 */
	void GiveLockOrder(FUnloadTestHarvester& TestHarvester);
	void TrackLockedHarvester(FUnloadTestHarvester& TestHarvester, EHarvesterAIAction Status) const;
	void OnLockedHarvesterFinished(FUnloadTestHarvester& TestHarvester) const;
	void CheckLockResults();
	// How many docked harvesters got a lock order in the first unload; the second one gets the move + stop variant.
	int32 M_LockOrdersGiven = 0;

	/**
	 * @brief Animation geometry: when a bay's grab starts grabbing it must hang over the pick point (the hopper), and
	 * when it starts releasing over the bay's bin. Trolley and boom stations only (not the depot's elevator).
	 */
	void TickRigGeometry(const AResourceUnloadStation& Station);
	void CheckRigGeometryResults();
	// Last seen rig phase per bay (EUnloadTrolleyPhase as uint8), to act once on entering a phase.
	TArray<uint8> M_LastRigPhases;
	int32 M_GrabMoments = 0;
	int32 M_ReleaseMoments = 0;
	float M_MaxGrabOffPick2DCm = 0.f;
	float M_MaxReleaseOffBin2DCm = 0.f;

	/** @brief -UnloadStationCaptureEvery=<s>: the first two views every few seconds while a harvester is docked. */
	void TickCaptureSequence();
	float M_CaptureEverySeconds = 0.f;
	float M_LastSequenceCaptureWorldSeconds = -1.f;
	int32 M_SequenceFrameIndex = 0;
};
