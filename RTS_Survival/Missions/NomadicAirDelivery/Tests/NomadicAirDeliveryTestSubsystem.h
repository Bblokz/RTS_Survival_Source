// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Missions/NomadicAirDelivery/NomadicAirDeliveryTypes.h"
#include "NomadicAirDeliveryTestSubsystem.generated.h"

class ACameraActor;
class AGliderTankDelivery;
class ANomadicVehicle;
class UMissionBase;

UENUM()
enum class ENomadicAirDeliveryTestStage : uint8
{
	WaitingForStart,
	RunningBatch,
	ClearingBatch,
	WatchingMission,
	Finished
};

/** @brief One delivery the test orders and follows to the finished building. */
USTRUCT()
struct FNomadicAirDeliveryTestCase
{
	GENERATED_BODY()

	FNomadicAirDeliveryOrder Order;
	int32 Batch = 0;
	bool bStarted = false;
	bool bLandedCallback = false;
	bool bDone = false;
	bool bTookSlungShot = false;
	bool bTookLowShot = false;
	float StartSeconds = 0.f;
	float LandedSeconds = 0.f;
	FVector LandedLocation = FVector::ZeroVector;

	UPROPERTY()
	TWeakObjectPtr<ANomadicVehicle> Nomadic;
};

/**
 * @brief -NomadicAirDeliveryValidate: flies every German nomadic in by rope-slung cargo plane and the HQ by Mammut
 * glider (UNomadicAirDeliverySubsystem), a few at a time around the anchor (-NomadicAirDeliveryAnchorX= and
 * -NomadicAirDeliveryAnchorY=, else where the camera looks), and checks each lands on its spot, is the right
 * nomadic of the player and becomes a building there without an HQ build radius. -NomadicAirDeliveryTrainsMission
 * instead completes the Ger_Trains start mission and checks its four deliveries and that the next mission waits for
 * the HQ. Screenshots in Saved/NomadicAirDeliveryTest, log lines RTS_NOMADIC_AIR_TEST; not in shipping builds.
 */
UCLASS()
class RTS_SURVIVAL_API UNomadicAirDeliveryTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	ENomadicAirDeliveryTestStage M_Stage = ENomadicAirDeliveryTestStage::WaitingForStart;
	float M_BootSeconds = -1.f;
	float M_StageSeconds = 0.f;
	int32 M_CurrentBatch = 0;
	int32 M_BatchCount = 0;
	int32 M_PassedCheckCount = 0;
	int32 M_FailedCheckCount = 0;
	int32 M_ScreenshotCount = 0;
	bool bM_IsTrainsMissionMode = false;
	bool bM_NextMissionStartedBeforeHQ = false;
	bool bM_TookBatchShot = false;
	bool bM_TookSlungShot = false;
	FVector M_Anchor = FVector::ZeroVector;

	UPROPERTY()
	TArray<FNomadicAirDeliveryTestCase> M_Cases;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	UPROPERTY()
	TWeakObjectPtr<UMissionBase> M_TrainsStartMission;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	void EnterStage(ENomadicAirDeliveryTestStage NewStage);
	void Finish();

	void TickWaitingForStart();
	bool StartGameAndGetIsReady() const;
	bool FindAnchor(FVector& OutAnchor) const;
	void BuildRosterCases();
	void AddCase(ENomadicAirDeliveryMethod Method, ENomadicSubtype Subtype, const FVector& Site, int32 Batch);

	void StartBatch();
	void StartCase(int32 CaseIndex);
	void OnCaseLanded(int32 CaseIndex, ANomadicVehicle* Nomadic);
	void TickBatch();
	void TickCase(FNomadicAirDeliveryTestCase& Case);
	void TickCaseScreenshots(FNomadicAirDeliveryTestCase& Case);
	void CheckBuiltCase(const FNomadicAirDeliveryTestCase& Case);
	bool GetIsBatchDone() const;
	void TickClearingBatch();

	UMissionBase* FindStartedTrainsStartMission() const;
	bool StartTrainsMission();
	void OnTrainsMissionDeliveryFinished(ANomadicVehicle* Nomadic, const FNomadicAirDeliveryOrder& Order);
	void TickWatchingMission();
	void FinishTrainsMission();

	ANomadicVehicle* FindSlungNomadic() const;
	const ANomadicVehicle* FindSlungNomadicNear(const FVector& Location, float Radius) const;
	const AGliderTankDelivery* FindActiveGlider() const;
	bool GetIsInsideAnyHQBuildRadius(const ANomadicVehicle& Nomadic) const;
	void UpdateCamera();
	void RequestScreenshot(const FString& Label);
};
