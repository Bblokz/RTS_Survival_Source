// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Missions/MissionProps/FuelDelivery/FuelDeliveryDepotComponent.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "FuelDepotTestSubsystem.generated.h"

class AMissionFuelCrane;
class ASceneCapture2D;
class UFuelDeliveryDepotComponent;
class UTextureRenderTarget2D;

enum class EFuelDepotTestStep : uint8
{
	StartGame,
	Spawn,
	FirstTruckSpawned,
	FirstTruckParked,
	FirstTruckUnloaded,
	FirstTruckDespawned,
	SecondTruckSpawned,
	SecondTruckDestroyed,
	ThirdTruckSpawned,
	DepotDestroyed,
	NoMoreTrucks,
	Done
};

/**
 * @brief -FuelDepotValidate: spawns the fuel depot Blueprint for the first AI player on RTS_ToCopySmall and follows its
 * loop: the crane spawns at the crane node, truck 1 drives to the park node, the crane empties it, the AI gets the
 * training points, the truck drives home and is removed (not killed). Truck 2 comes after the (shortened) refill and
 * is destroyed on its way; truck 3 replaces it after the respawn delay. Then the depot building is destroyed: truck 3
 * must turn back and be removed, and no truck may come after it.
 * Switches: -FuelDepotAt=X,Y (default 16796,-15808), -FuelDepotClass=/Game/.../BP.BP_C, -FuelDepotCapture (pictures
 * while parked and unloading), -FuelDepotYaw=deg, -FuelDepotSpawnAtLoad (spawn before any actor begins play, like a
 * depot placed on the map). Logs RTS_FUELDEPOT lines, writes Saved/FuelDepotTest/<time>/summary.md.
 */
UCLASS()
class RTS_SURVIVAL_API UFuelDepotTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	UFUNCTION()
	void HandleTruckSpawned(AActor* Truck);
	UFUNCTION()
	void HandleTruckUnloaded(AActor* Truck, int32 TrainingPointsGranted);
	UFUNCTION()
	void HandleTruckDespawned(AActor* Truck);
	UFUNCTION()
	void HandleTruckDestroyed(AActor* Truck);

	UPROPERTY()
	TObjectPtr<ASceneCapture2D> M_CaptureActor = nullptr;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> M_CaptureTarget = nullptr;

#if RTS_WITH_MAP_TESTS
	EFuelDepotTestStep M_Step = EFuelDepotTestStep::StartGame;
	bool bM_IsActive = false;
	bool bM_Capture = false;
	bool bM_SpawnAtLoad = false;
	bool bM_HasStartedGame = false;
	bool bM_HasSuspendedAITraining = false;
	double M_StartRealSeconds = 0.0;
	float M_StepStartGameSeconds = 0.f;
	FVector M_Location = FVector::ZeroVector;
	float M_Yaw = 0.f;
	FString M_ClassPath;
	FString M_OutputDirectory;
	TArray<FString> M_Lines;
	TArray<FString> M_Failures;

	TWeakObjectPtr<AActor> M_Depot;
	TWeakObjectPtr<UFuelDeliveryDepotComponent> M_DepotComponent;
	TWeakObjectPtr<AActor> M_CurrentTruck;
	// Held by the test itself: the crane must outlive the depot building.
	TWeakObjectPtr<AMissionFuelCrane> M_Crane;
	float M_CurrentTruckSpawnSeconds = 0.f;
	int32 M_LastTickAIPoints = 0;
	int32 M_AIPointsDeltaAtGrant = 0;
	int32 M_SpawnedTrucks = 0;
	int32 M_UnloadedTrucks = 0;
	int32 M_DespawnedTrucks = 0;
	int32 M_DestroyedTrucks = 0;
	int32 M_PointsGrantedByEvent = 0;
	int32 M_AIPointsBefore = 0;
	int32 M_CraneCountBefore = 0;
	float M_ParkedAtSeconds = -1.f;
	float M_UnloadedAtSeconds = -1.f;
	float M_SecondTruckDestroyedAt = -1.f;
	float M_NextCaptureSeconds = 0.f;
	int32 M_CaptureIndex = 0;

	void ReadCommandLine();
	void SetStep(EFuelDepotTestStep Step);
	float GetGameSeconds() const;
	float GetStepSeconds() const;
	bool TryStartGame();
	void KeepGameRunning() const;
	void SpawnDepot();
	void TickStep();
	void SuspendAITrainingOnce();
	EFuelDeliveryState GetDepotState() const;
	void CheckCraneAtNode();
	void CheckFirstUnload();
	void TickFirstTruckSpawned();
	void TickFirstTruckParked();
	void TickFirstTruckUnloaded();
	void TickFirstTruckDespawned();
	void TickSecondTruckSpawned();
	void TickSecondTruckDestroyed();
	void TickThirdTruckSpawned();
	void TickDepotDestroyed();
	void TickNoMoreTrucks();
	void Fail(const FString& Reason);
	void Log(const FString& Line);
	void Finish();
	int32 GetAIPoints() const;
	int32 CountVisibleCargo(const AActor* Truck) const;
	void LogTruckPose(const TCHAR* Label) const;
	bool EnsureCaptureActor();
	void Capture(const FString& Suffix);
#endif
};
