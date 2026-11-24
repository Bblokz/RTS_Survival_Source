// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/StaticArray.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "SmallArmsMuzzleVfxBatcher.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

// One group of emitters in the small arms muzzle Niagara system; emitters are named <Group>_<Layer>.
enum class ESmallArmsMuzzleVfxType : uint8
{
	// The flash of every round.
	Flash,
	// The puff of powder smoke; automatic weapons only puff on some rounds.
	Smoke,
	Count
};

// One emitter of the muzzle system and how many particles it spawns for one shot of its type.
struct FSmallArmsMuzzleEmitterSpawn
{
	int32 TypeIndex = 0;
	FName SpawnCountParameter;
	int32 ParticlesPerShot = 1;
	int32 PublishedSpawnCount = 0;
};

/**
 * @brief Lives on the projectile manager; infantry weapons hand it every shot and it draws the muzzle flashes and
 * powder smoke of all small arms with one Niagara system, instead of one Niagara component per weapon socket that
 * restarts (and wipes its own smoke) every round. Weapons reach it through GetSmallArmsMuzzleVfxBatcher.
 * Only ticks on frames that have shots to publish or to clear; see Docs/VFX/SmallArmsMuzzleVfx.md.
 */
UCLASS()
class RTS_SURVIVAL_API USmallArmsMuzzleVfxBatcher : public UActorComponent
{
	GENERATED_BODY()

public:
	USmallArmsMuzzleVfxBatcher();

	// False when the Niagara system is missing or the batched drawing is turned off; weapons then use their own.
	bool GetIsReadyToDraw() const { return bM_IsReadyToDraw; }

	/**
	 * @brief Queues one shot; its flash and smoke are spawned by the Niagara system on this frame's tick.
	 * @param MuzzleLocation World location of the barrel end.
	 * @param FireDirection Direction the barrel points; the flash and smoke are blown out along it.
	 * @param FlashScale Size of the flash, 1 for a rifle.
	 * @param SmokeScale Size of the smoke puff, 1 for a rifle; 0 for a round that only flashes.
	 * @param SmokeLifetimeMlt How long the smoke hangs in the air, 1 for a rifle.
	 */
	void AddShot(
		const FVector& MuzzleLocation,
		const FVector& FireDirection,
		const float FlashScale,
		const float SmokeScale,
		const float SmokeLifetimeMlt);

	/** @return The batcher of the world's projectile manager when it is ready to draw, else null. */
	static USmallArmsMuzzleVfxBatcher* FindReadyBatcher(const UWorld* World);

#if RTS_WITH_MAP_TESTS
	// Per emitter: particles alive now and spawned in total; shows a measuring run that every emitter works.
	FString DescribeEmitterParticleCounts() const;
	int64 GetTotalPublishedShots() const { return M_TotalPublishedShots; }
	// Position, age, colour and size of the first particle of every emitter; for checking the HLSL in a test run.
	FString DescribeFirstParticles() const;
#endif

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	static constexpr int32 MuzzleTypeCount = static_cast<int32>(ESmallArmsMuzzleVfxType::Count);

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_NiagaraComponent;

	// Shots gathered since the last tick, per type; xyz is the muzzle location and w the scale.
	TStaticArray<TArray<FVector4f>, MuzzleTypeCount> M_PendingPositionAndScale;
	// Same order as M_PendingPositionAndScale; xyz is the fire direction, w the lifetime multiplier.
	TStaticArray<TArray<FVector4f>, MuzzleTypeCount> M_PendingDirection;

	// What the Niagara system reads this frame: the pending shots of all types behind each other.
	TArray<FVector4f> M_PublishedPositionAndScale;
	TArray<FVector4f> M_PublishedDirection;

	// Where each type starts in the published arrays and how many shots it has, as last given to Niagara.
	TStaticArray<int32, MuzzleTypeCount> M_PublishedOffset;
	TStaticArray<int32, MuzzleTypeCount> M_PublishedCount;

	// Read from the system's user parameters: every <Group>_<Layer>_PerShot parameter is one emitter.
	TArray<FSmallArmsMuzzleEmitterSpawn> M_EmitterSpawns;

#if RTS_WITH_MAP_TESTS
	int64 M_TotalPublishedShots = 0;
#endif

	int32 M_PendingShotCount = 0;
	int32 M_MaxShotsPerFrame = 0;
	bool bM_IsReadyToDraw = false;

	bool GetIsValidNiagaraComponent() const;

	void BeginPlay_CreateNiagaraComponent(UNiagaraSystem* MuzzleSystem);
	void BeginPlay_ReadEmitterSpawns(const UNiagaraSystem& MuzzleSystem);

	void AddPendingEntry(
		const ESmallArmsMuzzleVfxType Type,
		const FVector& MuzzleLocation,
		const FVector3f& Direction,
		const float Scale,
		const float LifetimeMlt);

	// Hands this frame's shots to Niagara; every emitter spawns the particles of its own type from them.
	void PublishPendingShots();

	// Sets the counts back to zero so the shots of the previous frame are not spawned a second time.
	void ClearPublishedShots();

	void PublishTypeRange(const int32 TypeIndex, const int32 Offset, const int32 Count);

	// Tells every emitter how many particles to spawn this frame for the shots of its type.
	void PublishEmitterSpawnCounts();
};
