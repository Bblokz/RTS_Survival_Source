// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TurretTossDemoSubsystem.generated.h"

class ACPPController;
class ASceneCapture2D;
class ATankMaster;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

enum class ETurretTossDemoStep : uint8
{
	StartGame,
	WaitForTank,
	Settle,
	Record,
	Stills,
	AuditSpawnBatch,
	AuditSettle,
	AuditObserve,
	Done
};

/** One vehicle of the -TurretTossAudit run: spawned, killed during a cook-off and judged by what its wreck did. */
struct FTurretTossAuditEntry
{
	FString SubtypeName;
	uint8 SubtypeValue = 0;
	FVector Location = FVector::ZeroVector;
	TWeakObjectPtr<ATankMaster> Tank;
	TWeakObjectPtr<AActor> Wreck;
	bool bSpawnRequested = false;
	bool bKilled = false;
	float MaxTurretHeightCm = 0.f;
};

/**
 * @brief Headless film of the ammo cook-off death: launch with -game -TurretTossDemo -TurretTossDemoDir=<dir>
 * -TurretTossDemoAt=X,Y [-TurretTossDemoTank=<ETankSubtype name>]. Spawns the tank, kills it during a cook-off,
 * writes one PNG per frame plus a few stills and logs the thrown turret (RTS_TTD lines), then quits.
 * With -TurretTossAudit instead every ETankSubtype is spawned in batches around -TurretTossDemoAt and killed the same
 * way; one RTS_TTA line per vehicle says whether its turret was thrown, only the hull burned, or nothing happened.
 */
UCLASS()
class RTS_SURVIVAL_API UTurretTossDemoSubsystem : public UTickableWorldSubsystem
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
	bool TrySpawnTank();
	void TickSettle();
	void TickRecord();
	void TickStills();
	bool CreateCaptureRig();
	void AimCapture(float OrbitYawDegrees, float DistanceCm, float HeightCm, const FVector& LookAt) const;
	void ExportFrame(const FString& FileName) const;
	void LogTossedTurrets() const;

	// ---- Audit of every tank subtype ----
	void BuildAuditEntries();
	void TickAuditSpawnBatch();
	void TickAuditSettle();
	void TickAuditObserve();
	void SampleAuditTurretHeights();
	void StartAuditBatch();
	void ReportAuditEntry(const FTurretTossAuditEntry& Entry);
	void ClearAuditBatch();
	AActor* FindWreckNear(const FVector& Location) const;
	static const UStaticMeshComponent* FindWreckTurret(const AActor* Wreck);
	bool TryGetGroundZ(const FVector& Location, float& OutGroundZ) const;
	float GetGameSeconds() const;
	void Finish(const TCHAR* Reason);

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_Tank;

	UPROPERTY()
	TWeakObjectPtr<ASceneCapture2D> M_Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> M_RenderTarget;

	ETurretTossDemoStep M_Step = ETurretTossDemoStep::StartGame;
	FString M_OutputDirectory;
	FVector M_DemoLocation = FVector::ZeroVector;
	// Where the tank stood when it died; the camera keeps looking here after the tank is replaced by its wreck.
	FVector M_DeathLocation = FVector::ZeroVector;
	float M_DeathYaw = 0.f;
	double M_StartRealSeconds = 0.0;
	float M_StepStartGameSeconds = 0.f;
	float M_NextTurretLogGameSeconds = 0.f;
	int32 M_FrameIndex = 0;
	int32 M_StillIndex = 0;
	TArray<FTurretTossAuditEntry> M_AuditEntries;
	int32 M_AuditBatchStart = 0;
	int32 M_AuditBatchEnd = 0;
	int32 M_AuditCountsTossed = 0;
	int32 M_AuditCountsHullOnly = 0;
	int32 M_AuditCountsFailed = 0;
	bool bM_IsAudit = false;
	bool bM_IsActive = false;
	bool bM_HasKilledTank = false;
};
