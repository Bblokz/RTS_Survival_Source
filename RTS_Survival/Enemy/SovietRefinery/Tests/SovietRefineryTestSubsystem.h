// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Buildings/BuildingExpansion/B4HowitzerPit/Tests/B4HowitzerCaptureRig.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "SovietRefineryTestSubsystem.generated.h"

class ASkeletalCraneUnloadStation;
class USovietRefineryComponent;
class UHarvester;

UENUM()
enum class ESovietRefineryTestPhase : uint8
{
	Inactive,
	WaitingForStart,
	WaitingForActive,
	WaitingForHarvesters,
	// Each refinery's first harvester comes home with radixite, the second with metal: one docks under the crane,
	// the other unloads at the overflow point.
	ForcedUnload,
	// No more help: the harvesters must find fields, harvest and deliver on their own.
	NaturalHarvest
};

/** One refinery under test. */
struct FSovietRefineryUnderTest
{
	FString Design;
	TWeakObjectPtr<AActor> Building;
	TWeakObjectPtr<USovietRefineryComponent> Refinery;
	TWeakObjectPtr<ASkeletalCraneUnloadStation> Station;
	int32 DeliveredBaselineRadixite = 0;
	int32 DeliveredBaselineMetal = 0;
	int32 ForcedRadixite = 0;
	int32 ForcedMetal = 0;
	int32 CraneReleases = 0;
	uint8 LastPhase = 0;
	bool bForcedDone = false;
	bool bNaturalDone = false;
	bool bCapturedLowering = false;
	bool bCapturedCarry = false;
	bool bCapturedRelease = false;
	float MaxJawOffPick2DCm = 0.f;
	// Last logged harvester AI status per harvester name.
	TMap<FString, uint8> HarvesterStatus;
};

/**
 * @brief Map test of the Soviet refineries (USovietRefineryComponent + ASkeletalCraneUnloadStation), started with
 * -SovietRefineryValidate on a unit-test map (default RTS_ToCopySmall). Spawns Rus_Building_SovietRefinery_A1 and _G2
 * for player 2 (-SovietRefineryDesign=A1|G2|Both, -SovietRefineryAt=X,Y), checks: refinery active with its crane,
 * N free T-34 Breakers trained, a forced radixite + metal trip is unloaded (crane trips, exact amounts in the stock),
 * the harvesters then harvest and deliver on their own, the stock converts, and the human player's resources never
 * change. -SovietRefineryCapture=<folder> saves crane pictures. Logs RTS_SOVREF_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API USovietRefineryTestSubsystem : public UTickableWorldSubsystem
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
	void TickWaitingForActive();
	void TickWaitingForHarvesters();
	void TickForcedUnload();
	void TickNaturalHarvest();
	void TickExitPending();

	void SpawnRefinery(const FString& Design, const FVector2D& Anchor);
	int32 GetDelivered(const FSovietRefineryUnderTest& Test, ERTSResourceType Resource) const;
	void TrackCrane(FSovietRefineryUnderTest& Test);
	void TrackHarvesters(FSovietRefineryUnderTest& Test);
	void CaptureCrane(const FSovietRefineryUnderTest& Test, const FString& Label) const;
	int32 GetPlayerResource(ERTSResourceType Resource) const;
	void StartGameAndUnpause() const;

	void EnterPhase(ESovietRefineryTestPhase NewPhase);
	float GetPhaseSeconds() const;
	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	void Finish();

	ESovietRefineryTestPhase M_Phase = ESovietRefineryTestPhase::WaitingForStart;
	float M_PhaseStartSeconds = -1.f;
	int32 M_FailedCheckCount = 0;
	bool bM_IsExitPending = false;
	int32 M_ExitFramesLeft = 0;

	TArray<FSovietRefineryUnderTest> M_Refineries;
	int32 M_PlayerRadixiteBefore = 0;
	int32 M_PlayerMetalBefore = 0;
	FB4HowitzerCaptureRig M_CaptureRig;
};
