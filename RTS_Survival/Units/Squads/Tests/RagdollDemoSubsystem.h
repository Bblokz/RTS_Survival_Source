// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RagdollDemoSubsystem.generated.h"

class ACPPController;
class ARTSPooledRagdoll;
class ASceneCapture2D;
class AStaticMeshActor;
class UStaticMesh;
class ASkeletalMeshActor;
class USkeletalMesh;
class ASquadUnit;
class UTextureRenderTarget2D;

enum class ERagdollDemoStep : uint8
{
	StartGame,
	WaitForSquad,
	Settle,
	Record,
	Bench,
	CrewSpawn,
	CrewFilm,
	CrewStress,
	CrewLook,
	PoseSettle,
	PoseShoot,
	Done
};

/** Bodies thrown for the battlefield corpse pose library, their baked meshes and the pictures taken of them. */
USTRUCT()
struct FRagdollPoseBakeState
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<ARTSPooledRagdoll>> Bodies;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> BakedMeshes;

	// One actor shows each baked mesh in turn, at the spot where its ragdoll settled.
	UPROPERTY()
	TWeakObjectPtr<AStaticMeshActor> PreviewActor;

	TArray<FString> PoseNames;
	TArray<FVector> Pivots;
	FVector PlatformTop = FVector::ZeroVector;
	int32 ShotIndex = 0;
	int32 ShotFrame = 0;
};

/** One soldier of the film and how he was killed. */
struct FRagdollDemoSubject
{
	TWeakObjectPtr<ASquadUnit> Unit;
	FString KillName;
	FVector DeathLocation = FVector::ZeroVector;
	float DeathHeadHeightCm = 0.f;
	float MaxHeightCm = 0.f;
	float MaxDistanceCm = 0.f;
};

/**
 * @brief Headless film of infantry deaths: launch with -game -RagdollDemo -RagdollDemoDir=<dir> -RagdollDemoAt=X,Y
 * [-RagdollDemoSquad=<ESquadSubtype name>]. Kills soldiers one by one with a direct hit, a blast, fire and an
 * attacker-less hit, writes one PNG per frame and logs how each body moved (RTS_RGD lines), then quits.
 * With -RagdollCrew it films one player and one enemy tank dying, then stresses the pool (RTS_RGC lines).
 * With -RagdollBench it times spawning against two kinds of pool instead and logs simulation cost (RTS_RGB lines).
 * With -RagdollPoseBake it throws crew bodies onto a platform, bakes each settled pose into a static mesh for the
 * corpse library and photographs ragdoll and baked mesh side by side (RTS_RGP lines).
 */
UCLASS()
class RTS_SURVIVAL_API URagdollDemoSubsystem : public UTickableWorldSubsystem
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
	bool TryStartGame();
	void KeepGameRunning() const;
	bool TrySpawnSquad();
	bool TryCollectSubjects();
	void TickSettle();
	void TickRecord();
	void KillSubject(int32 SubjectIndex);
	void LogSubjects();
	void ReportSubjects() const;
	bool CreateCaptureRig();
	void AimCapture() const;
	bool TryGetGroundZ(const FVector& Location, float& OutGroundZ) const;
	float GetGameSeconds() const;
	void Finish(const TCHAR* Reason);

	// ---- Crew bodies out of dying tanks ----
	bool TrySpawnCrewTanks();
	void TickCrewSpawn();
	void TickCrewFilm();
	void TickCrewStress(double FrameMs);
	void TickCrewLook();
	void LogWokenCrewBodies() const;

	// ---- Spawn against pool benchmark ----
	void TickBench();
	bool SampleFrames(double FrameMs, const TCHAR* Label, int32 Count);
	ASkeletalMeshActor* SpawnBenchBody(bool bKeepBodiesReady) const;
	void ParkBenchBody(ASkeletalMeshActor* Body, bool bKeepBodiesReady) const;
	void WakeBenchBody(ASkeletalMeshActor* Body, const FVector& Location) const;
	void PrewarmBenchPool(TArray<TObjectPtr<ASkeletalMeshActor>>& OutPool, bool bKeepBodiesReady, const TCHAR* Label);
	void TickBenchBatches();
	void TickBenchSimulation(double FrameMs);
	FVector GetBenchSlot(int32 SlotIndex) const;

	// ---- Corpse pose library bake (RagdollDemoPoseBake.cpp) ----
	bool TryStartPoseBake();
	bool SpawnPosePlatform();
	void ThrowPoseBodies();
	static void LogPosePhysicsBodies(const USkeletalMesh* CrewMesh);
	void TickPoseSettle();
	void BakePoses();
	void TickPoseShoot();
	void SetUpPoseShot(int32 PoseIndex, int32 ShotKind) const;

	UPROPERTY()
	FRagdollPoseBakeState M_PoseBake;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TWeakObjectPtr<ASceneCapture2D> M_Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> M_RenderTarget;

	UPROPERTY()
	TObjectPtr<USkeletalMesh> M_BenchMesh;

	// Dormant without bodies: they are built when the body wakes.
	UPROPERTY()
	TArray<TObjectPtr<ASkeletalMeshActor>> M_PoolWithoutBodies;

	// Dormant with bodies already built and parked out of sight.
	UPROPERTY()
	TArray<TObjectPtr<ASkeletalMeshActor>> M_PoolWithBodies;

	UPROPERTY()
	TArray<TObjectPtr<ASkeletalMeshActor>> M_BenchActive;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_CrewTanks;

	TArray<FRagdollDemoSubject> M_Subjects;
	ERagdollDemoStep M_Step = ERagdollDemoStep::StartGame;
	FString M_OutputDirectory;
	FVector M_DemoLocation = FVector::ZeroVector;
	FVector M_FilmCentre = FVector::ZeroVector;
	double M_StartRealSeconds = 0.0;
	float M_StepStartGameSeconds = 0.f;
	float M_NextLogGameSeconds = 0.f;
	float M_NextKillGameSeconds = 0.f;
	int32 M_NextKillIndex = 0;
	int32 M_FrameIndex = 0;
	bool bM_IsActive = false;
	bool bM_IsBench = false;
	bool bM_IsCrew = false;
	bool bM_IsPoseBake = false;
	int32 M_CrewKills = 0;
	int32 M_CrewLookRequests = 0;
	float M_FilmDistanceScale = 1.f;
	int32 M_CrewRequests = 0;
	int32 M_MaxSimulating = 0;
	int32 M_BenchPhase = 0;
	int32 M_BenchFrame = 0;
	int32 M_BenchMode = 0;
	int32 M_BenchBatch = 0;
	int32 M_BenchSizeIndex = 0;
	double M_LastBenchTickSeconds = 0.0;
	double M_SampleTotalMs = 0.0;
	double M_SampleWorstMs = 0.0;
	int32 M_SampleCount = 0;
	double M_ActivateTotalMs = 0.0;
	double M_ActivateWorstMs = 0.0;
	double M_ReleaseTotalMs = 0.0;
};
