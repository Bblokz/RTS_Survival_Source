// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LandscapeAmbientFxSubsystem.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class URTSAmbientFxSettings;

/** One dust devil drifting over the sand; lives in a fixed slot so its particles keep reading the same entry. */
struct FAmbientDustDevil
{
	// Ground point under the column.
	FVector Location = FVector::ZeroVector;
	// Ground height the column eases towards; refreshed by a trace every few frames.
	float TargetGroundZ = 0.f;
	float HeadingRadians = 0.f;
	float DriftSpeed = 0.f;
	float WanderPhase = 0.f;
	float Age = 0.f;
	float Lifetime = 0.f;
	float Scale = 1.f;
	// 0 to 1, handed to the particles so the shape varies per devil.
	float Seed = 0.f;
	// +1 counter-clockwise, -1 clockwise.
	float Spin = 1.f;
	// 0 to 1: fades in after spawning and out at the end of its life or when it leaves the sand.
	float Strength = 0.f;
	bool bIsAlive = false;
	bool bIsFadingOut = false;
};

/** Which parts of the map have sand ground, sampled once on a grid when the mission starts. */
struct FAmbientFxSandGrid
{
	FVector2D Origin = FVector2D::ZeroVector;
	FIntPoint CellCount = FIntPoint::ZeroValue;
	float CellSize = 0.f;
	// Per cell: 1 when the landscape there returned a sand surface.
	TArray<uint8> bIsSandCell;
	// Indices of the sand cells, to pick spawn points from.
	TArray<int32> SandCellIndices;
	// Centre of each sand cell's landscape hit, same order as SandCellIndices.
	TArray<FVector> SandCellGroundPoints;
};

/**
 * @brief Draws every ambient landscape effect of a map (dust devils over sand) with one Niagara system, so many
 * devils cost one component, one GPU simulation per emitter and one draw per renderer (Docs/Environment/AmbientFx.md).
 * Devils live near the camera only; the count follows the graphics setting through rts.AmbientFx (0 to 3).
 */
UCLASS()
class RTS_SURVIVAL_API ULandscapeAmbientFxSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// Slots the Niagara system reads; no setting may ask for more devils than this.
	static constexpr int32 MaxDustDevilSlots = 16;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	int32 GetAliveDustDevilCount() const;

private:
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_NiagaraComponent = nullptr;

	TStaticArray<FAmbientDustDevil, MaxDustDevilSlots> M_DustDevils;
	FAmbientFxSandGrid M_SandGrid;

	// What the Niagara system reads; rebuilt from M_DustDevils on every tick that has devils.
	TArray<FVector4f> M_PublishedPositionAndScale;
	TArray<FVector4f> M_PublishedMotion;
	TArray<int32> M_PublishedAliveSlots;

	// Spawn rate parameter and particles per second per devil of each emitter, read from the system.
	TArray<TPair<FName, float>> M_EmitterSpawnRates;

	FVector M_CameraFocus = FVector::ZeroVector;
	FRandomStream M_Random;
	float M_SecondsUntilFocusUpdate = 0.f;
	float M_SecondsUntilSpawn = 0.f;
	float M_SecondsUntilStatusLog = 0.f;
	int32 M_NextGroundTraceSlot = 0;
	int32 M_MaxDustDevils = 0;
	// Seconds between new devils at the current graphics setting; read with the count from rts.AmbientFx.
	FFloatInterval M_SpawnInterval = FFloatInterval(20.f, 35.f);
	bool bM_HasCameraFocus = false;
	bool bM_IsReady = false;
	bool bM_WasPublishingLastTick = false;
	// The map has a baked SandMask_<MapName>; otherwise sand comes from the traced physical surfaces.
	bool bM_HasBakedSandMask = false;

	bool GetIsValidNiagaraComponent() const;

	/**
	 * @brief Samples the landscape on a grid and keeps the flat sand cells (baked mask, else physical surface);
	 * dust devils only form there.
	 * @param InWorld World that begins play.
	 * @param Settings Cell size and the surface types that count as sand.
	 * @return False when the map has no landscape or no sand.
	 */
	bool BeginPlay_BuildSandGrid(UWorld& InWorld, const URTSAmbientFxSettings& Settings);
	// Loads the map's baked sand mask into the grid; false when the map has none.
	bool BeginPlay_ReadBakedSandMask(const UWorld& InWorld, const URTSAmbientFxSettings& Settings);
	void BeginPlay_SetupTracedGrid(const FBox& LandscapeBounds, const URTSAmbientFxSettings& Settings);
	bool BeginPlay_CreateNiagaraComponent(UWorld& InWorld, const URTSAmbientFxSettings& Settings);
	void BeginPlay_ReadEmitterSpawnRates(const UNiagaraSystem& System);
	static bool GetIsMapExcluded(const UWorld& InWorld, const URTSAmbientFxSettings& Settings);

	// Reads rts.AmbientFx and turns it into the number of devils that may be alive and how often a new one starts.
	void UpdateMaxDustDevils(const URTSAmbientFxSettings& Settings);
	void UpdateCameraFocus(float DeltaTime);
	void UpdateDustDevils(float DeltaTime, const URTSAmbientFxSettings& Settings);
	void UpdateDustDevil(FAmbientDustDevil& DustDevil, float DeltaTime, const URTSAmbientFxSettings& Settings) const;
	void TraceGroundForNextDustDevil();
	void TrySpawnDustDevil(float DeltaTime, const URTSAmbientFxSettings& Settings);

	/**
	 * @brief Picks a sand spot near the camera focus that is not too close to another devil.
	 * @param Settings Spawn radius.
	 * @param OutGroundPoint The landscape point the new devil starts on.
	 * @return False when no free sand spot is near the camera.
	 */
	bool FindSpawnPoint(const URTSAmbientFxSettings& Settings, FVector& OutGroundPoint);
	void StartDustDevil(FAmbientDustDevil& DustDevil, const FVector& GroundPoint, const URTSAmbientFxSettings& Settings);

	// Hands the devils to Niagara and scales each emitter's spawn rate with the number alive.
	void PublishDustDevils();
	void SetComponentActive(bool bShouldBeActive) const;

	// With rts.AmbientFx.Log 1: one status line every few seconds (devils, focus, particles per emitter).
	void LogStatusWhenAsked(float DeltaTime);
	// Particles alive and spawned so far per emitter; GPU counts arrive a few frames late.
	FString DescribeEmitterParticleCounts() const;

	/**
	 * @brief Traces down onto the landscape.
	 * @param Location Point to trace under; only X and Y matter.
	 * @param OutHit The landscape hit, with its physical material.
	 * @return False when there is no landscape under the point.
	 */
	bool TraceLandscape(const FVector& Location, FHitResult& OutHit) const;
	// Sand by the baked mask when the map has one, else by the hit's physical surface.
	bool GetIsSandAt(const FHitResult& Hit) const;
	bool GetIsSandHit(const FHitResult& Hit) const;
	bool GetIsSandCellAt(const FVector& Location) const;
	float GetDistanceToNearestDustDevil(const FVector& Location) const;
};
