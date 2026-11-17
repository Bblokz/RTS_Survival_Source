// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "AircraftStrafeImpactBatcher.generated.h"

class UDecalComponent;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
enum class ERTSSurfaceType : uint8;

// Which emitter group of NS_AircraftStrafeImpacts draws an impact; emitters are named <Group>_<Layer>.
enum class EAircraftStrafeImpactGroup : uint8
{
	// Dirt spurt, debris and lingering dust where a round strikes the ground.
	Ground,
	// Sparks and a small dust puff where a round strikes a unit.
	Unit,
	Count
};

// Surface tint the strafe impact emitters read from the impact (normal.w); keeps the system to one set of emitters.
enum class EAircraftStrafeImpactSurface : uint8
{
	Dirt = 0,
	Sand = 1,
	Stone = 2,
	Metal = 3
};

// One emitter of the strafe impact system and how many particles it spawns per impact of its group.
struct FAircraftStrafeImpactEmitterSpawn
{
	int32 GroupIndex = 0;
	FName SpawnCountParameter;
	int32 ParticlesPerImpact = 1;
	int32 PublishedSpawnCount = 0;
};

/**
 * @brief Lives on the small arms projectile manager; every aircraft strafe gun hands it its impacts (real rounds and
 * the extra spurts between them) and it draws all of a frame with the one NS_AircraftStrafeImpacts system.
 * Only ticks on frames that have impacts to publish or to clear. Built by RTS.StrafeImpactVfx.BuildNiagaraSystem.
 */
UCLASS()
class RTS_SURVIVAL_API UAircraftStrafeImpactBatcher : public UActorComponent
{
	GENERATED_BODY()

public:
	UAircraftStrafeImpactBatcher();

	// False when the strafe impact system is missing; strafe guns then draw their ground hits as regular trace impacts.
	bool GetIsReadyToDraw() const { return bM_IsReadyToDraw; }

	/**
	 * @brief Queues an impact; it is drawn by the Niagara system on this frame's tick.
	 * @param Group Ground (spurt, debris, dust) or Unit (sparks).
	 * @param Location World location of the impact.
	 * @param Normal Surface normal the spurt rises along.
	 * @param CalibreMm Calibre of the round; the effect's size and lifetime scale with it.
	 * @param Surface Tints the dirt and dust.
	 */
	void AddImpact(const EAircraftStrafeImpactGroup Group, const FVector& Location, const FVector& Normal,
	               const float CalibreMm, const EAircraftStrafeImpactSurface Surface);

	static EAircraftStrafeImpactSurface GetSurfaceForPhysicalSurface(const ERTSSurfaceType SurfaceType);

	/**
	 * @brief Leaves a small dark bullet pock (a deferred decal) that stays a while, then fades. Its own ring of decals,
	 * so a strafe never evicts the shell craters of ULandscapeImpactDecalSubsystem; the oldest pock moves first.
	 * @param Location Where the round struck the ground (on the drawn surface).
	 * @param Normal Ground normal; the decal projects along it.
	 * @param CalibreMm Scales the pock.
	 */
	void AddBulletPock(const FVector& Location, const FVector& Normal, const float CalibreMm);

#if RTS_WITH_MAP_TESTS
	// Impacts handed to Niagara since the game started, per group; the strafe QA test logs them.
	int64 GetTotalPublishedImpacts(const EAircraftStrafeImpactGroup Group) const;
	// Bullet pock decals placed since the game started.
	int64 GetTotalBulletPocks() const { return M_TotalBulletPocks; }
	// Per emitter: particles alive now and spawned in total; shows a measuring run that every emitter works.
	FString DescribeEmitterParticleCounts() const;
	// Emitters of the system that have not spawned a single particle yet (-1 without a system instance).
	int32 GetEmitterCountWithoutSpawns() const;
	// Particles alive over all emitters right now.
	int32 GetLiveParticleCount() const;
	// Per emitter: the first live particles' positions, sizes and alpha; shows where (and whether) they draw.
	FString DescribeParticleSamples() const;
#endif

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	static constexpr int32 GroupCount = static_cast<int32>(EAircraftStrafeImpactGroup::Count);

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_NiagaraComponent;

	// Impacts gathered since the last tick, per group; xyz is the location and w the calibre in mm.
	TStaticArray<TArray<FVector4f>, GroupCount> M_PendingPositionAndCalibre;
	// Same order; xyz is the surface normal and w the EAircraftStrafeImpactSurface tint.
	TStaticArray<TArray<FVector4f>, GroupCount> M_PendingNormalAndSurface;

	// What the Niagara system reads this frame: the pending impacts of all groups behind each other.
	TArray<FVector4f> M_PublishedPositionAndCalibre;
	TArray<FVector4f> M_PublishedNormalAndSurface;

	TStaticArray<int32, GroupCount> M_PublishedOffset;
	TStaticArray<int32, GroupCount> M_PublishedCount;

	// Read from the system's user parameters: every <Group>_<Layer>_PerImpact parameter is one emitter.
	TArray<FAircraftStrafeImpactEmitterSpawn> M_EmitterSpawns;

#if RTS_WITH_MAP_TESTS
	TStaticArray<int64, GroupCount> M_TotalPublishedByGroup;
#endif
	int64 M_TotalBulletPocks = 0;

	// Ring of bullet pock decals, made as needed up to the ring size, then recycled oldest first.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> M_PockDecals;
	// World time each ring slot's pock was placed; drives its fade (the crater material has no lifetime opacity
	// node, and UDecalComponent::SetFadeOut would destroy the pooled component when done).
	TArray<float> M_PockPlacedTimes;
	int32 M_NextPockSlot = 0;
	FTimerHandle M_PockFadeTimerHandle;

	// The crater decal material of URTSImpactDecalSettings (its smallest band's art, drawn at bullet-hole size).
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> M_PockMaterial;
	bool bM_HasTriedLoadingPockMaterial = false;

	// Returns the ring slot for the next pock (a fresh component while the ring grows or a slot was destroyed).
	int32 GetNextPockSlot();
	UDecalComponent* CreatePockDecal(AActor& OwningActor);
	void StartPockFadeTimerIfNeeded();
	// Lowers the opacity (DecalColor.A) of pocks past their hold time and hides the faded ones.
	void TickPockFades();

	int32 M_PendingImpactCount = 0;
	int32 M_MaxImpactsPerFrame = 0;
	bool bM_IsReadyToDraw = false;

	bool GetIsValidNiagaraComponent() const;

	void BeginPlay_ReadEmitterSpawns(const UNiagaraSystem& StrafeImpactSystem);
	void BeginPlay_CreateNiagaraComponent(UNiagaraSystem* StrafeImpactSystem);

	void PublishPendingImpacts();
	void ClearPublishedImpacts();
	void PublishGroupRange(const int32 GroupIndex, const int32 Offset, const int32 Count);
	void PublishEmitterSpawnCounts();
};
