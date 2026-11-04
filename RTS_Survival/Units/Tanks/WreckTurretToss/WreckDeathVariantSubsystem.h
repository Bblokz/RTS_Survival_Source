#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckRegularDeathSettings.h"

#include "WreckDeathVariantSubsystem.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture;
class UNiagaraComponent;

/** A running debris burst and when its pieces sink away. */
struct FWreckDebrisBurstSinkState
{
	TWeakObjectPtr<UNiagaraComponent> Component;
	float SinkStartSeconds = 0.f;
	float SinkSeconds = 1.f;
};

/**
 * @brief Hands out regular death variants from a shuffle bag so consecutive deaths rarely look alike, caps how many
 * Niagara debris bursts run at once and keeps one hull-coloured debris material per hull material.
 * Used by ATankMaster when it spawns a regular-death wreck; nothing to set up.
 */
UCLASS()
class RTS_SURVIVAL_API UWreckDeathVariantSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** @return The next variant of the shuffle bag. */
	ERegularDeathVariant DrawVariant();

	/** @return True when another debris burst may start; finished bursts free their slot. */
	bool GetCanStartDebrisBurst();

	/**
	 * @brief Tracks a started burst and sinks its pieces away later by shrinking User.DebrisScale to zero.
	 * @param DebrisComponent The burst; deactivated once its pieces have sunk.
	 * @param LieSeconds Seconds before the pieces start to sink.
	 * @param SinkSeconds Seconds the sinking takes.
	 */
	void RegisterDebrisBurst(UNiagaraComponent* DebrisComponent, float LieSeconds, float SinkSeconds);

	/**
	 * @brief Chunk material tinted like a hull: a copy of the chunk material with the hull's colour texture in it.
	 * @param HullMaterial Material of the wreck hull; its first colour texture is used.
	 * @param ChunkMaterial Debris material with a HullBaseColor texture parameter.
	 * @return The cached copy for this hull material; the plain chunk material when no colour texture was found.
	 */
	UMaterialInterface* GetDebrisMaterialForHull(UMaterialInterface* HullMaterial, UMaterialInterface* ChunkMaterial);

private:
	void RefillVariantBag();
	void TickDebrisSinking();
	void PruneFinishedDebrisBursts();

	// Remaining draws of the current bag; refilled (shuffled) when empty.
	TArray<ERegularDeathVariant> M_VariantBag;
	ERegularDeathVariant M_LastVariant = ERegularDeathVariant::Standard;
	bool bM_HasDrawnVariant = false;

	TArray<FWreckDebrisBurstSinkState> M_ActiveDebrisBursts;
	FTimerHandle M_DebrisSinkTimerHandle;

	// Hull colour texture -> chunk material showing it; the textures are assets that outlive the world.
	UPROPERTY()
	TMap<TObjectPtr<UTexture>, TObjectPtr<UMaterialInstanceDynamic>> M_DebrisMaterialByTexture;
};
