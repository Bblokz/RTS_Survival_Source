// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/StaticArray.h"
#include "RTS_Survival/Physics/RTSSurfaceSubtypes.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "TraceImpactVfxBatcher.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

// One group of emitters in the trace impact Niagara system; the first five follow ERTSSurfaceType.
enum class ETraceImpactVfxType : uint8
{
	Sand,
	Metal,
	Flesh,
	Air,
	Stone,
	Bounce,
	Count
};

// One emitter of the trace impact Niagara system and how many particles it spawns for an impact of its type.
struct FTraceImpactVfxEmitterSpawn
{
	int32 TypeIndex = 0;
	FName SpawnCountParameter;
	int32 ParticlesPerImpact = 1;
	int32 PublishedSpawnCount = 0;
};

/**
 * @brief Lives on the projectile manager; trace weapons hand it every impact and it draws all impacts of a
 * frame with one Niagara system instead of one Niagara component per impact.
 * Only ticks on frames that have impacts to publish or to clear.
 */
UCLASS()
class RTS_SURVIVAL_API UTraceImpactVfxBatcher : public UActorComponent
{
	GENERATED_BODY()

public:
	UTraceImpactVfxBatcher();

	// False when the Niagara system is missing or the centralised drawing is turned off; weapons then draw their own.
	bool GetIsReadyToDraw() const { return bM_IsReadyToDraw; }

	/**
	 * @brief Queues an impact; it is drawn by the Niagara system on this frame's tick.
	 * @param ImpactType Which group of emitters draws it.
	 * @param Location World location of the hit.
	 * @param Normal Surface normal at the hit, the effect is oriented along it.
	 * @param Scale Scales how far the effect spreads, as the weapon's impact scale did.
	 */
	void AddImpact(
		const ETraceImpactVfxType ImpactType,
		const FVector& Location,
		const FVector& Normal,
		const float Scale);

	static ETraceImpactVfxType GetImpactTypeForSurface(const ERTSSurfaceType SurfaceType);

#if RTS_WITH_MAP_TESTS
	// Per emitter: particles alive now and spawned in total; shows a measuring run that every emitter works.
	FString DescribeEmitterParticleCounts() const;
#endif

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	static constexpr int32 ImpactTypeCount = static_cast<int32>(ETraceImpactVfxType::Count);

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_NiagaraComponent;

	// Impacts gathered since the last tick, per impact type; xyz is the location and w the scale.
	TStaticArray<TArray<FVector4f>, ImpactTypeCount> M_PendingPositionAndScale;
	// Same order as M_PendingPositionAndScale; xyz is the surface normal.
	TStaticArray<TArray<FVector4f>, ImpactTypeCount> M_PendingNormal;

	// What the Niagara system reads this frame: the pending impacts of all types behind each other.
	TArray<FVector4f> M_PublishedPositionAndScale;
	TArray<FVector4f> M_PublishedNormal;

	// Where each type starts in the published arrays and how many impacts it has, as last given to Niagara.
	TStaticArray<int32, ImpactTypeCount> M_PublishedOffset;
	TStaticArray<int32, ImpactTypeCount> M_PublishedCount;

	// Read from the system's user parameters: every <Type>_<Layer>_PerImpact parameter is one emitter.
	TArray<FTraceImpactVfxEmitterSpawn> M_EmitterSpawns;

#if RTS_WITH_MAP_TESTS
	// Impacts handed to Niagara since the game started, per type; written to the log on request.
	TStaticArray<int64, ImpactTypeCount> M_TotalPublishedByType;
	double M_LastTotalsLogSeconds = 0.0;

	// With the console variable RTS.TraceImpactVfx.LogTotals on, logs the totals every few seconds.
	void LogPublishedTotalsWhenAsked();
#endif

	int32 M_PendingImpactCount = 0;
	int32 M_MaxImpactsPerFrame = 0;
	bool bM_IsReadyToDraw = false;

	bool GetIsValidNiagaraComponent() const;

	void BeginPlay_CreateNiagaraComponent(UNiagaraSystem* TraceImpactSystem);
	void BeginPlay_ReadEmitterSpawns(const UNiagaraSystem& TraceImpactSystem);

	// Hands this frame's impacts to Niagara; every emitter spawns the particles of its own type from them.
	void PublishPendingImpacts();

	// Sets the counts back to zero so the impacts of the previous frame are not spawned a second time.
	void ClearPublishedImpacts();

	void PublishTypeRange(const int32 TypeIndex, const int32 Offset, const int32 Count);

	// Tells every emitter how many particles to spawn this frame for the impacts of its type.
	void PublishEmitterSpawnCounts();
};
