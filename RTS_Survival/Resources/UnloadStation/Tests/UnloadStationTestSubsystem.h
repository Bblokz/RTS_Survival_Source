// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Buildings/BuildingExpansion/B4HowitzerPit/Tests/B4HowitzerCaptureRig.h"

#include "UnloadStationTestSubsystem.generated.h"

class AResourceUnloadStation;
class ACPPController;
class ANomadicVehicle;
class UHarvester;
class UResourceDropOff;
enum class ERTSResourceType : uint8;

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
	void CaptureViews(const FString& Label) const;

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

	// Refinery run: one bay, both harvesters carry radixite, the bay-less one uses the overflow point.
	bool bM_IsRefinery = false;
	int32 M_RadixiteCargoEach = 0;
	bool bM_SawCraneBusy = false;
	int32 M_PackUpHarvesterIndex = 0;
	// The pack-up baseline (cargo on board, stored) is taken when the harvester docks, so harvesting on the way in
	// does not count.
	bool bM_HasPackUpBaseline = false;

	void TickRefineryUnloadResults(AResourceUnloadStation& Station);
};
