// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/StaticArray.h"
#include "TankShellTrailBatcher.generated.h"

class AProjectile;
class ASmallArmsProjectileManager;
class UNiagaraComponent;
class UNiagaraSystem;

// One group of emitters in a tank shell trail Niagara system; emitters are named <Group>_<Layer>.
enum class ETankShellTrailGroup : uint8
{
	// Dense samples along the path flown this frame: the hot streak.
	Tracer,
	// One sample per shell per frame at the shell itself: the halo and the sparks.
	Glow,
	// Sparser samples along the path flown this frame: the smoke that stays behind.
	Smoke,
	Count
};

// One emitter of the trail system and how many particles it spawns for one sample of its group.
struct FTankShellTrailEmitterSpawn
{
	int32 GroupIndex = 0;
	FName SpawnCountParameter;
	int32 ParticlesPerSample = 1;
	int32 PublishedSpawnCount = 0;
};

// What the batcher remembers of one pooled projectile between frames.
struct FTankShellTrailSlot
{
	FVector PreviousLocation = FVector::ZeroVector;
	// Identity of the launch being drawn; a pooled projectile that was reused starts a new trail.
	uint32 LaunchSerial = 0;
	// Distance already flown past the last sample, per sample spacing, so samples stay evenly spaced over frames.
	float TracerCarry = 0.0f;
	float SmokeCarry = 0.0f;
	bool bIsTracked = false;
};

/**
 * @brief Lives on the projectile manager and draws every pooled tank shell in flight with one world-space Niagara
 * system: a tracer streak, a halo and a smoke trail that stays in the air after the shell returned to the pool.
 * Set up in Project Settings, RTS Weapon VFX; shells fall back to their own effect when it is not ready.
 */
UCLASS()
class RTS_SURVIVAL_API UTankShellTrailBatcher : public UActorComponent
{
	GENERATED_BODY()

public:
	UTankShellTrailBatcher();

	// True when the trail system loaded: tank shells then leave their own Niagara component off.
	bool GetIsDrawingTankShells() const { return bM_IsReadyToDraw; }

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	static constexpr int32 GroupCount = static_cast<int32>(ETankShellTrailGroup::Count);

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_NiagaraComponent;

	// Samples gathered this tick, per group; xyz is the sample location, w packs shell kind and calibre scale.
	TStaticArray<TArray<FVector4f>, GroupCount> M_PendingPositionAndScale;
	// Same order; xyz is the flight direction, w how many seconds before now the shell passed the sample.
	TStaticArray<TArray<FVector4f>, GroupCount> M_PendingDirection;

	// What the Niagara system reads this frame: the samples of all groups behind each other.
	TArray<FVector4f> M_PublishedPositionAndScale;
	TArray<FVector4f> M_PublishedDirection;

	TStaticArray<int32, GroupCount> M_PublishedOffset;
	TStaticArray<int32, GroupCount> M_PublishedCount;

	TArray<FTankShellTrailEmitterSpawn> M_EmitterSpawns;

	// One entry per slot of the manager's tank projectile pool.
	TArray<FTankShellTrailSlot> M_Slots;

	int32 M_MaxSamplesPerFrame = 6000;
	bool bM_IsReadyToDraw = false;
	bool bM_HasPublishedSamples = false;

	bool GetIsValidNiagaraComponent() const;

	void BeginPlay_ReadEmitterSpawns(const UNiagaraSystem& TrailSystem);
	void BeginPlay_CreateNiagaraComponent(UNiagaraSystem* TrailSystem);

	/** @return How many samples were gathered from all shells in flight. */
	int32 GatherSamples(const ASmallArmsProjectileManager& Manager, const float DeltaTime);

	/**
	 * @brief Adds the samples of the path one shell flew since the last tick.
	 * @param Projectile The shell in flight.
	 * @param Slot What was remembered of this pool slot.
	 * @param DeltaTime Frame time, to tell each sample how long ago the shell passed it.
	 * @return How many samples were added.
	 */
	int32 GatherShellSamples(const AProjectile& Projectile, FTankShellTrailSlot& Slot, const float DeltaTime);

	/**
	 * @brief Adds evenly spaced samples of one group along a flown segment.
	 * @param Group Group the samples belong to.
	 * @param Start Where the shell was last tick.
	 * @param Direction Normalised flight direction.
	 * @param Length Length of the segment.
	 * @param Spacing Distance between samples.
	 * @param PackedKindAndScale Shell kind and calibre scale, as the HLSL unpacks them.
	 * @param DeltaTime Frame time.
	 * @param InOutCarry Distance flown past the last sample of earlier frames.
	 * @return How many samples were added.
	 */
	int32 AddSegmentSamples(
		const ETankShellTrailGroup Group,
		const FVector& Start,
		const FVector& Direction,
		const float Length,
		const float Spacing,
		const float PackedKindAndScale,
		const float DeltaTime,
		float& InOutCarry);

	void AddSample(
		const ETankShellTrailGroup Group,
		const FVector& Location,
		const FVector& Direction,
		const float PackedKindAndScale,
		const float AgeOffsetSeconds);

	void PublishPendingSamples();
	void ClearPublishedSamples();
	void PublishGroupRange(const int32 GroupIndex, const int32 Offset, const int32 Count);
	void PublishEmitterSpawnCounts();
};
