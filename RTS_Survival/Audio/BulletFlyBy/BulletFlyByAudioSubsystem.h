// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "BulletFlyByAudioSubsystem.generated.h"

class ARTSPooledAudio;
class UAudioComponent;
class UBulletFlyByDataAsset;

/**
 * @brief Plays a bullet whiz when a small-arms tracer passes close to the point the camera looks at.
 * The listener is the camera, high above the fight, so a bullet never really passes it; the camera pawn (the ground
 * point the camera looks at) stands in for the ear. ASmallArmsProjectileManager reports every tracer once, when fired:
 * the pass is found with a little math on the tracer's line, timed to when the tracer gets there, and kept only within
 * the budget of UBulletFlyByDataAsset. Nothing is tracked per tracer, the subsystem ticks only while a fly-by waits to
 * play, and the sounds play on a fixed pool of audio components (no objects created per shot).
 * Sounds and tuning: the data asset in Project Settings > Game > RTS Audio Settings > Bullet Fly By Data.
 */
UCLASS()
class RTS_SURVIVAL_API UBulletFlyByAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	/**
	 * @brief Schedules a fly-by if this tracer passes close to the point the camera looks at.
	 * @param LaunchLocation Where the tracer starts.
	 * @param EndLocation Where the tracer ends (impact or end of range).
	 * @param LaunchTime World time the shot was fired; the trace result arrives a frame or so later.
	 */
	void OnTracerFired(const FVector& LaunchLocation, const FVector& EndLocation, const float LaunchTime);

private:
	struct FPendingFlyBy
	{
		FVector Location = FVector::ZeroVector;
		double PlayTime = 0.0;
		float Volume = 0.f;
	};

	UPROPERTY()
	TObjectPtr<UBulletFlyByDataAsset> M_Data;

	UPROPERTY()
	TObjectPtr<ARTSPooledAudio> M_PoolOwner;

	UPROPERTY()
	TArray<TObjectPtr<UAudioComponent>> M_PoolComponents;

	// Play order of each pool entry's current sound, indexed like M_PoolComponents; the lowest started first.
	TArray<uint64> M_PoolPlayOrders;

	uint64 M_NextPlayOrder = 1;

	// Fly-bys waiting for their tracer to arrive; a handful at most (BulletFlyByAudio::MaxPendingFlyBys).
	TArray<FPendingFlyBy> M_PendingFlyBys;

	// Rate limit: fly-bys that may still start now; refills at MaxFlyBysPerSecond up to MaxSimultaneousFlyBys.
	float M_FlyByTokens = 0.f;
	double M_LastTokenRefillTime = 0.0;

	// World time of the last fly-by that started, to keep MinSecondsBetweenFlyBys towards new ones.
	double M_LastFlyByStartTime = -1.0e9;

	int32 M_LastSoundIndex = INDEX_NONE;

	// The ear (camera pawn) and the camera distance, refreshed once per frame on the first tracer of that frame.
	uint64 M_EarFrame = TNumericLimits<uint64>::Max();
	bool bM_HasEar = false;
	FVector M_EarLocation = FVector::ZeroVector;
	float M_CameraDistance = 0.f;

	void LoadData();
	void SpawnPool(UWorld& World);
	bool GetIsValidPoolOwner() const;

	/** @return Whether there is an ear this frame (a player camera pawn that is the view target). */
	bool UpdateEar();

	/** @return 0..1 fade of fly-bys by how far the camera is zoomed out. */
	float GetZoomVolumeFactor() const;

	void RefillTokens(const double Now);

	/** Adds the fly-by, or drops it for a louder one that starts within MinSecondsBetweenFlyBys of it. */
	void SchedulePendingFlyBy(const FPendingFlyBy& FlyBy);

	void PlayFlyBy(const FPendingFlyBy& FlyBy);
	int32 PickSoundIndex();
	int32 AcquirePoolIndex() const;
};
