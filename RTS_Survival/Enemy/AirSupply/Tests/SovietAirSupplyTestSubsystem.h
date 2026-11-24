// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "SovietAirSupplyTestSubsystem.generated.h"

class AAircraftMaster;
class ACameraActor;
class ASovietAirSupplyPad;
class UEnemyStrategicAIComponent;

/**
 * @brief -SovietAirSupplyValidate on a map with an enemy controller (UT_EnemyAI): spawns BP_SovietAirSupplyPad near the
 * camera with short timings and checks (1) a full delivery: the plane is player 2's, lands on the pad, the workers
 * carry every item, the AI gets the training and building points, the plane lifts off and leaves; (2) a plane shot
 * down while unloading gives nothing and clears the pad; (3) a plane shot down on its approach gives nothing.
 * Screenshots and results in Saved/SovietAirSupplyTest; quits when done. Only built with RTS_WITH_MAP_TESTS.
 */
UCLASS()
class RTS_SURVIVAL_API USovietAirSupplyTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(const float DeltaTime) override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	enum class EStep : uint8
	{
		WaitingForStart,
		FirstDelivery,
		FirstPlaneLeaves,
		ShootDownWhileUnloading,
		ShootDownOnApproach,
		Finished
	};

	EStep M_Step = EStep::WaitingForStart;
	double M_TestStartSeconds = -1.0;
	double M_StepStartSeconds = 0.0;
	double M_UnloadingSinceSeconds = -1.0;
	double M_ApproachSinceSeconds = -1.0;
	int32 M_Failed = 0;
	TArray<FString> M_Lines;
	int32 M_ShotCount = 0;

	// Seen during the first delivery.
	bool bM_SawCarry = false;
	bool bM_SawLanded = false;
	bool bM_TookUnloadShot = false;
	bool bM_TookTakeOffShot = false;
	// The close camera is used for a couple of frames around each close-up screenshot.
	bool bM_UseCloseCamera = false;
	int32 M_CloseShotDelayFrames = 0;
	int32 M_CloseShotCount = 0;
	float M_LandedOffset2D = -1.f;
	float M_MaxPlaneZAfterDelivery = -1.f;
	float M_WorkerMaxTravel = 0.f;
	FVector M_WorkerStart = FVector::ZeroVector;
	int32 M_TrainingBefore = 0;
	int32 M_BuildingBefore = 0;

	UPROPERTY()
	TObjectPtr<ASovietAirSupplyPad> M_Pad;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	UPROPERTY()
	TWeakObjectPtr<UEnemyStrategicAIComponent> M_StrategicAI;

	void StartTest();
	void TickSteps();
	void TickFirstDelivery();
	void OnFirstPlaneLanded(AAircraftMaster& Plane);
	void TickUnloadScreenshots();
	void CheckFirstDelivery();
	void TickFirstPlaneLeaves();
	void TickShootDownWhileUnloading();
	void TickShootDownOnApproach();
	void AdvanceStep(EStep NextStep);
	double GetSeconds() const;
	void UpdateCamera();
	void Screenshot(const FString& Label);
	int32 GetBuildingPoints() const;
	void Check(bool bCondition, const FString& Name, const FString& Detail);
	void Evaluate();
};
